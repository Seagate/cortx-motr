/* -*- C -*- */
/*
 * Copyright (c) 2021 Seagate Technology LLC and/or its Affiliates
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 * For any questions about this software or licensing,
 * please email opensource@seagate.com or cortx-questions@seagate.com.
 */

#include <stdio.h>

#include "lib/trace.h"        /* m0_trace_set_mmapped_buffer */
#include "rpc/rpclib.h"       /* M0_RPCLIB_MAX_RETRIES */
#include "motr/client_internal.h" /* m0_client */
#include "layout/plan.h"      /* m0_layout_io_plop */

#include "iscservice/isc.h"
#include "util.h"

#ifndef DEBUG
#define DEBUG 0
#endif

enum {
	MAX_M0_BUFSZ = 128*1024*1024, /* max bs for object store I/O  */
	MAX_POOLS = 16,
	MAX_RCFILE_NAME_LEN = 512,
	MAX_CONF_STR_LEN = 128,
	MAX_CONF_PARAMS = 32,
};

/* static variables */
static struct m0_container      container;
static struct m0_idx_dix_config dix_conf = {};

/* global variables */
struct m0_realm     uber_realm;
int                 trace_level = 0;
bool                m0trace_on = false;
struct m0_semaphore isc_sem;
struct m0_list      isc_reqs;

/**
 * Return parity group size for object.
 */
uint64_t isc_m0gs(struct m0_obj *obj, struct m0_client *cinst)
{
	unsigned long           usz; /* unit size */
	struct m0_pool_version *pver;

	pver = m0_pool_version_find(&cinst->m0c_pools_common,
				    &obj->ob_attr.oa_pver);
	if (pver == NULL) {
		ERR("invalid object pool version: "FID_F"\n",
		    FID_P(&obj->ob_attr.oa_pver));
		return 0;
	}

	usz = m0_obj_layout_id_to_unit_size(obj->ob_attr.oa_layout_id);

	return usz * pver->pv_attr.pa_N;
}

void free_segs(struct m0_bufvec *data, struct m0_indexvec *ext,
	       struct m0_bufvec *attr)
{
	m0_indexvec_free(ext);
	m0_bufvec_free(data);
	m0_bufvec_free(attr);
}

int alloc_segs(struct m0_bufvec *data, struct m0_indexvec *ext,
	       struct m0_bufvec *attr, uint64_t bsz, uint32_t cnt)
{
	int i, rc;

	rc = m0_bufvec_alloc(data, cnt, bsz) ?:
	     m0_bufvec_alloc(attr, cnt, 1) ?:
	     m0_indexvec_alloc(ext, cnt);
	if (rc != 0)
		goto err;

	for (i = 0; i < cnt; i++)
		attr->ov_vec.v_count[i] = 0; /* no attrs */

	return 0;
 err:
	free_segs(data, ext, attr);
	return rc;
}

uint64_t set_exts(struct m0_indexvec *ext, uint64_t off, uint64_t bsz)
{
	uint32_t i;

	for (i = 0; i < ext->iv_vec.v_nr; i++) {
		ext->iv_index[i] = off;
		ext->iv_vec.v_count[i] = bsz;
		off += bsz;
	}

	return i * bsz;
}

int isc_req_prepare(struct isc_req *req, struct m0_buf *args,
		    const struct m0_fid *comp_fid,
		    struct m0_layout_io_plop *iop, uint32_t reply_len)
{
	int                    rc;
	struct m0_rpc_session *sess = iop->iop_session;
	struct m0_fop_isc     *fop_isc = &req->cir_isc_fop;
	struct m0_fop         *arg_fop = &req->cir_fop;

	req->cir_plop = &iop->iop_base;
	fop_isc->fi_comp_id = *comp_fid;
	m0_rpc_at_init(&fop_isc->fi_args);
	rc = m0_rpc_at_add(&fop_isc->fi_args, args, sess->s_conn);
	if (rc != 0) {
		m0_rpc_at_fini(&fop_isc->fi_args);
		fprintf(stderr, "error! m0_rpc_at_add() failed with %d\n", rc);
		return rc;
	}
	/* Initialise the reply RPC AT buffer to be received.*/
	m0_rpc_at_init(&fop_isc->fi_ret);
	rc = m0_rpc_at_recv(&fop_isc->fi_ret, sess->s_conn, reply_len, false);
	if (rc != 0) {
		m0_rpc_at_fini(&fop_isc->fi_args);
		m0_rpc_at_fini(&fop_isc->fi_ret);
		fprintf(stderr, "error! m0_rpc_at_recv() failed with %d\n", rc);
		return rc;
	}
	m0_fop_init(arg_fop, &m0_fop_isc_fopt, fop_isc, m0_fop_release);
	req->cir_rpc_sess = sess;

	return rc;
}

void isc_req_replied(struct m0_rpc_item *item)
{
	int                    rc;
	struct m0_fop         *fop = M0_AMB(fop, item, f_item);
	struct m0_fop         *reply_fop;
	struct m0_fop_isc_rep *isc_reply;
	struct isc_req        *req = M0_AMB(req, fop, cir_fop);
	const char *addr = m0_rpc_conn_addr(req->cir_rpc_sess->s_conn);

	if (item->ri_error != 0) {
		req->cir_rc = item->ri_error;
		fprintf(stderr,
			"No reply from %s: rc=%d.\n", addr, item->ri_error);
		goto err;
	}
	reply_fop = m0_rpc_item_to_fop(req->cir_fop.f_item.ri_reply);
	isc_reply = (struct m0_fop_isc_rep *)m0_fop_data(reply_fop);
	rc = req->cir_rc = isc_reply->fir_rc;
	if (rc != 0) {
		fprintf(stderr,
			"Got error in reply from %s: rc=%d.\n", addr, rc);
		if (rc == -ENOENT)
			fprintf(stderr, "Was isc .so library is loaded?\n");
		goto err;
	}
	rc = m0_rpc_at_rep_get(&req->cir_isc_fop.fi_ret, &isc_reply->fir_ret,
			       &req->cir_result);
	if (rc != 0)
		fprintf(stderr,
			"rpc_at_rep_get() from %s failed: rc=%d\n", addr, rc);
 err:
	m0_fop_put(&req->cir_fop);
	m0_semaphore_up(&isc_sem);
}

static const struct m0_rpc_item_ops isc_item_ops = {
	.rio_replied = isc_req_replied,
};

static void ireqs_list_add_in_order(struct isc_req *req)
{
	struct isc_req *r;
	struct m0_layout_io_plop *pl1;
	struct m0_layout_io_plop *pl2 = M0_AMB(pl2, req->cir_plop, iop_base);

	m0_list_for_each_entry(&isc_reqs, r, struct isc_req, cir_link) {
		pl1 = M0_AMB(pl1, r->cir_plop, iop_base);
		if (pl1->iop_goff > pl2->iop_goff)
			break;
	}
	m0_list_add_before(&r->cir_link, &req->cir_link);
}

int isc_req_send(struct isc_req *req)
{
	int                    rc;
	struct m0_rpc_item    *item;

	item              = &req->cir_fop.f_item;
	item->ri_session  = req->cir_rpc_sess;
	item->ri_prio     = M0_RPC_ITEM_PRIO_MID;
	item->ri_deadline = M0_TIME_IMMEDIATELY;
	item->ri_nr_sent_max = M0_RPCLIB_MAX_RETRIES;
	item->ri_ops      = &isc_item_ops;

	m0_fop_get(&req->cir_fop);
	rc = m0_rpc_post(item);
	if (rc != 0) {
		fprintf(stderr, "Failed to send request to %s: rc=%d\n",
			m0_rpc_conn_addr(req->cir_rpc_sess->s_conn), rc);
		m0_fop_put(&req->cir_fop);
	}

	ireqs_list_add_in_order(req);

	return rc;
}

static void fop_fini_lock(struct m0_fop *fop)
{
	struct m0_rpc_machine *mach = m0_fop_rpc_machine(fop);

	m0_rpc_machine_lock(mach);
	m0_fop_fini(fop);
	m0_rpc_machine_unlock(mach);
}

void isc_req_fini(struct isc_req *req)
{
	struct m0_fop *reply_fop = NULL;

	if (req->cir_fop.f_item.ri_reply != NULL)
		reply_fop = m0_rpc_item_to_fop(req->cir_fop.f_item.ri_reply);
	if (reply_fop != NULL)
		m0_fop_put_lock(reply_fop);
	req->cir_fop.f_item.ri_reply = NULL;
	m0_rpc_at_fini(&req->cir_isc_fop.fi_args);
	m0_rpc_at_fini(&req->cir_isc_fop.fi_ret);
	req->cir_fop.f_data.fd_data = NULL;
	fop_fini_lock(&req->cir_fop);
}

/*
 * init client resources.
 */
int isc_init(struct m0_config *conf, struct m0_client **cinst)
{
	int   rc;

	conf->mc_is_oostore            = true;
	conf->mc_is_read_verify        = false;
#if 0
	/* set to default values */
	conf->mc_tm_recv_queue_min_len = M0_NET_TM_RECV_QUEUE_DEF_LEN;
	conf->mc_max_rpc_msg_size      = M0_RPC_DEF_MAX_RPC_MSG_SIZE;
#endif
	/* set to Sage cluster specific values */
	conf->mc_tm_recv_queue_min_len = 64;
	conf->mc_max_rpc_msg_size      = 65536;
	conf->mc_layout_id             = 0;

	conf->mc_idx_service_id   = M0_IDX_DIX;
	conf->mc_idx_service_conf = &dix_conf;

	if (!m0trace_on)
		m0_trace_set_mmapped_buffer(false);

	rc = m0_client_init(cinst, conf, true);
	if (rc != 0) {
		fprintf(stderr, "failed to initilise the Client API\n");
		return rc;
	}

	m0_container_init(&container, NULL, &M0_UBER_REALM, *cinst);
	rc = container.co_realm.re_entity.en_sm.sm_rc;
	if (rc != 0) {
		fprintf(stderr,"failed to open uber realm\n");
		return rc;
	}
	uber_realm = container.co_realm;

	m0_list_init(&isc_reqs);

	return 0;
}

void isc_fini(struct m0_client *cinst)
{
	m0_client_fini(cinst, true);
}

/*
 *  Local variables:
 *  c-indentation-style: "K&R"
 *  c-basic-offset: 8
 *  tab-width: 8
 *  fill-column: 80
 *  scroll-step: 1
 *  End:
 */
