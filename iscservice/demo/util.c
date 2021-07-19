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
#include <unistd.h>           /* gethostname */

#include "lib/semaphore.h"    /* m0_semaphore */
#include "lib/trace.h"        /* m0_trace_set_mmapped_buffer */
#include "rpc/rpclib.h"       /* m0_rpc_post_sync */
#include "motr/client.h"
#include "motr/client_internal.h" /* m0_reqh */
#include "conf/helpers.h"     /* m0_confc_root_open */
#include "conf/diter.h"       /* m0_conf_diter_next_sync */
#include "conf/obj_ops.h"     /* M0_CONF_DIRNEXT */
#include "spiel/spiel.h"      /* m0_spiel_process_lib_load */
#include "reqh/reqh.h"        /* m0_reqh */
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
static struct m0_client          *m0_instance = NULL;
static struct m0_container container;
static struct m0_config    m0_conf;
static struct m0_idx_dix_config   dix_conf;
static struct m0_spiel            spiel_inst;

static char c0rcfile[MAX_RCFILE_NAME_LEN] = "./.m0utilrc";

/*
 ******************************************************************************
 * GLOBAL VARIABLES
 ******************************************************************************
 */
struct m0_realm uber_realm;
unsigned unit_size = 0;
int trace_level=0;
bool m0trace_on = false;

struct param {
	char name[MAX_CONF_STR_LEN];
	char value[MAX_CONF_STR_LEN];
};

static struct m0_fid pools[MAX_POOLS] = {};

/**
 * Read parameters into array p[] from file.
 */
static int read_params(FILE *in, struct param *p, int max_params)
{
	int ln, n=0;
	char s[MAX_CONF_STR_LEN];

	for (ln=1; max_params > 0 && fgets(s, MAX_CONF_STR_LEN, in); ln++) {
		if (sscanf(s, " %[#\n\r]", p->name))
			continue; /* skip emty line or comment */
		if (sscanf(s, " %[a-z_A-Z0-9] = %[^#\n\r]",
		           p->name, p->value) < 2) {
			ERR("error at line %d: %s\n", ln, s);
			return -1;
		}
		DBG("%d: name='%s' value='%s'\n", ln, p->name, p->value);
		p++, max_params--, n++;
	}

	return n;
}

static const char* param_get(const char *name, const struct param p[], int n)
{
	while (n-- > 0)
		if (strcmp(p[n].name, name) == 0)
			return p[n].value;
	return NULL;
}

/**
 * Set pools fids at pools[] from parameters array p[].
 */
static int pools_fids_set(struct param p[], int n)
{
	int i;
	char pname[32];
	const char *pval;

	for (i = 0; i < MAX_POOLS; i++) {
		sprintf(pname, "M0_POOL_TIER%d", i + 1);
		if ((pval = param_get(pname, p, n)) == NULL)
			break;
		if (m0_fid_sscanf(pval, pools + i) != 0) {
			ERR("failed to parse FID of %s\n", pname);
			return -1;
		}
	}

	return i;
}

/**
 * Return pool fid from pools[] at tier_idx.
 */
static struct m0_fid *tier2pool(uint8_t tier_idx)
{
	if (tier_idx < 1 || tier_idx > MAX_POOLS)
		return NULL;
	return pools + tier_idx - 1;
}

/* Warning: non-reentrant. */
static char* fid2str(const struct m0_fid *fid)
{
	static char buf[256];

	if (fid)
		sprintf(buf, FID_F, FID_P(fid));
	else
		sprintf(buf, "%p", fid);

	return buf;
}

/**
 * Return parity group size for object.
 */
uint64_t m0util_m0gs(struct m0_obj *obj, int tier)
{
	int                     rc;
	unsigned long           usz; /* unit size */
	struct m0_reqh         *reqh = &m0_instance->m0c_reqh;
	struct m0_pool_version *pver;
	struct m0_fid          *pool = tier2pool(tier);

	rc = m0_pool_version_get(reqh->rh_pools, pool, &pver);
	if (rc != 0) {
		ERR("m0_pool_version_get(pool=%s) failed: rc=%d\n",
		    fid2str(pool), rc);
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

static int spiel_prepare(struct m0_spiel *spiel)
{
	struct m0_reqh *reqh = &m0_instance->m0c_reqh;
	int             rc;

	rc = m0_spiel_init(spiel, reqh);
	if (rc != 0) {
		fprintf(stderr, "error! spiel initialisation failed.\n");
		return rc;
	}

	rc = m0_spiel_cmd_profile_set(spiel, m0_conf.mc_profile);
	if (rc != 0) {
		fprintf(stderr, "error! spiel initialisation failed.\n");
		return rc;
	}
	rc = m0_spiel_rconfc_start(spiel, NULL);
	if (rc != 0) {
		fprintf(stderr, "error! starting of rconfc failed in spiel failed.\n");
		return rc;
	}

	return 0;
}

static void m0util_spiel_destroy(struct m0_spiel *spiel)
{
	m0_spiel_rconfc_stop(spiel);
	m0_spiel_fini(spiel);
}

static bool conf_obj_is_svc(const struct m0_conf_obj *obj)
{
	return m0_conf_obj_type(obj) == &M0_CONF_SERVICE_TYPE;
}

struct m0_rpc_link * m0util_svc_rpc_link_get(struct m0_fid *svc_fid)
{
	struct m0_reqh *reqh = &m0_instance->m0c_reqh;
	struct m0_reqh_service_ctx *ctx;
	struct m0_pools_common *pc = reqh->rh_pools;

	m0_tl_for(pools_common_svc_ctx, &pc->pc_svc_ctxs, ctx) {
		if (m0_fid_eq(&ctx->sc_fid, svc_fid))
			return &ctx->sc_rlink;
	} m0_tl_endfor;

	return NULL;
}

/*
 * Loads a library into m0d instances.
 */
int m0util_isc_api_register(const char *libpath)
{
	int                     rc;
	struct m0_reqh         *reqh = &m0_instance->m0c_reqh;
	struct m0_confc        *confc;
	struct m0_conf_root    *root;
	struct m0_conf_process *proc;
	struct m0_conf_service *svc;
	struct m0_conf_diter    it;

	rc = spiel_prepare(&spiel_inst);
	if (rc != 0) {
		fprintf(stderr, "error! spiel initialization failed");
		return rc;
	}

	confc = m0_reqh2confc(reqh);
	rc = m0_confc_root_open(confc, &root);
	if (rc != 0) {
		m0util_spiel_destroy(&spiel_inst);
		return rc;
	}

	rc = m0_conf_diter_init(&it, confc,
				&root->rt_obj,
				M0_CONF_ROOT_NODES_FID,
				M0_CONF_NODE_PROCESSES_FID,
				M0_CONF_PROCESS_SERVICES_FID);
	if (rc != 0) {
		m0_confc_close(&root->rt_obj);
		m0util_spiel_destroy(&spiel_inst);
		return rc;
	}

	while (M0_CONF_DIRNEXT ==
	       (rc = m0_conf_diter_next_sync(&it, conf_obj_is_svc))) {

		svc = M0_CONF_CAST(m0_conf_diter_result(&it), m0_conf_service);
		if (svc->cs_type != M0_CST_ISCS)
			continue;
		proc = M0_CONF_CAST(m0_conf_obj_grandparent(&svc->cs_obj),
				    m0_conf_process);
		rc = m0_spiel_process_lib_load(&spiel_inst, &proc->pc_obj.co_id,
					       libpath);
		if (rc != 0) {
			fprintf(stderr, "error! loading the library %s failed "
				        "for process "FID_F": rc=%d\n", libpath,
					FID_P(&proc->pc_obj.co_id), rc);
			m0_conf_diter_fini(&it);
			m0_confc_close(&root->rt_obj);
			m0util_spiel_destroy(&spiel_inst);
			return rc;
		}
	}
	m0_conf_diter_fini(&it);
	m0_confc_close(&root->rt_obj);
	m0util_spiel_destroy(&spiel_inst);

	return 0;
}

int m0util_isc_nxt_svc_get(struct m0_fid *svc_fid, struct m0_fid *nxt_fid,
			   enum m0_conf_service_type s_type)
{
	struct m0_reqh_service_ctx *ctx;
	struct m0_reqh             *reqh = &m0_instance->m0c_reqh;
	struct m0_pools_common     *pc = reqh->rh_pools;
	struct m0_fid               current_fid = *svc_fid;

	m0_tl_for(pools_common_svc_ctx, &pc->pc_svc_ctxs, ctx) {
		if (ctx->sc_type == s_type) {
			if (m0_fid_eq(&current_fid, &M0_FID0)) {
				*nxt_fid = ctx->sc_fid;
				return 0;
			} else if (m0_fid_eq(svc_fid, &ctx->sc_fid))
				current_fid = M0_FID0;
		}
	} m0_tl_endfor;

	*nxt_fid = M0_FID0;
	return -ENOENT;
}

int m0util_isc_req_prepare(struct m0util_isc_req *req, struct m0_buf *args,
			   const struct m0_fid *comp_fid,
			   struct m0_layout_io_plop *iop, uint32_t reply_len)
{
	struct m0_rpc_session *sess = iop->iop_session;
	struct m0_fop_isc  *fop_isc = &req->cir_isc_fop;
	struct m0_fop      *arg_fop = &req->cir_fop;
	int                 rc;

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
	struct m0util_isc_req *req = M0_AMB(req, fop, cir_fop);
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
	m0_semaphore_up(req->cir_sem);
}

struct m0_semaphore isc_sem;
struct m0_list      isc_reqs;

static const struct m0_rpc_item_ops isc_item_ops = {
	.rio_replied = isc_req_replied,
};

static void ireqs_list_add_in_order(struct m0util_isc_req *req)
{
	struct m0util_isc_req *r;
	struct m0_layout_io_plop *pl1;
	struct m0_layout_io_plop *pl2 = M0_AMB(pl2, req->cir_plop, iop_base);

	m0_list_for_each_entry(&isc_reqs, r, struct m0util_isc_req, cir_link) {
		pl1 = M0_AMB(pl1, r->cir_plop, iop_base);
		if (pl1->iop_goff > pl2->iop_goff)
			break;
	}
	m0_list_add_before(&r->cir_link, &req->cir_link);
}

int m0util_isc_req_send(struct m0util_isc_req *req)
{
	int                    rc;
	struct m0_rpc_item    *item;

	req->cir_sem = &isc_sem;

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

int m0util_isc_req_send_sync(struct m0util_isc_req *req)
{
	struct m0_fop         *reply_fop;
	struct m0_fop_isc_rep  isc_reply;
	int                    rc;

	rc = m0_rpc_post_sync(&req->cir_fop, req->cir_rpc_sess, NULL,
			      M0_TIME_IMMEDIATELY);
	if (rc != 0) {
		fprintf(stderr, "Failed to send request to %s: rc=%d\n",
			m0_rpc_conn_addr(req->cir_rpc_sess->s_conn), rc);
		return rc;
	}
	reply_fop = m0_rpc_item_to_fop(req->cir_fop.f_item.ri_reply);
	isc_reply = *(struct m0_fop_isc_rep *)m0_fop_data(reply_fop);
	rc = req->cir_rc = isc_reply.fir_rc;
	if (rc != 0)
		fprintf(stderr, "Got error from %s: rc=%d\n",
			m0_rpc_conn_addr(req->cir_rpc_sess->s_conn), rc);
	rc = m0_rpc_at_rep_get(&req->cir_isc_fop.fi_ret, &isc_reply.fir_ret,
			       &req->cir_result);
	if (rc != 0)
		fprintf(stderr, "rpc_at_rep_get() from %s failed: rc=%d\n",
			m0_rpc_conn_addr(req->cir_rpc_sess->s_conn), rc);

	return req->cir_rc == 0 ? rc : req->cir_rc;
}

static void fop_fini_lock(struct m0_fop *fop)
{
	struct m0_rpc_machine *mach = m0_fop_rpc_machine(fop);

	m0_rpc_machine_lock(mach);
	m0_fop_fini(fop);
	m0_rpc_machine_unlock(mach);
}

void m0util_isc_req_fini(struct m0util_isc_req *req)
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

static int read_conf_params(int idx, const struct param params[], int n)
{
	int i;
	struct conf_params_to_read {
		const char *name;
		const char **conf_ptr;
	} p[] = { { "HA_ENDPOINT_ADDR",      &m0_conf.mc_ha_addr },
		  { "PROFILE_FID",           &m0_conf.mc_profile },
		  { "LOCAL_ENDPOINT_ADDR%d", &m0_conf.mc_local_addr },
		  { "LOCAL_PROC_FID%d",      &m0_conf.mc_process_fid }, };
	char pname[256];

	for (i = 0; i < ARRAY_SIZE(p); i++) {
		sprintf(pname, p[i].name, idx);
		*(p[i].conf_ptr) = param_get(pname, params, n);
		if (*(p[i].conf_ptr) == NULL) {
			ERR("%s is not set at %s\n", pname, c0rcfile);
			return -EINVAL;
		}
	}

	return 0;
}

/*
 * m0util_init()
 * init client resources.
 */
int m0util_init(int idx)
{
	int   rc;
	int   lid;
	int   rc_params_nr;
	FILE *fp;
	struct param rc_params[MAX_CONF_PARAMS] = {};

	fp = fopen(c0rcfile, "r");
	if (fp == NULL) {
		ERR("failed to open resource file %s: %s\n", c0rcfile,
		    strerror(errno));
		return -errno;
	}

	rc = read_params(fp, rc_params, ARRAY_SIZE(rc_params));
	fclose(fp);
	if (rc < 0) {
		ERR("failed to read parameters from %s: rc=%d", c0rcfile, rc);
		return rc;
	}
	rc_params_nr = rc;

	rc = pools_fids_set(rc_params, rc_params_nr);
	if (rc <= 0) {
		if (rc == 0) {
			ERR("no pools configured at %s\n", c0rcfile);
		} else {
			ERR("failed to set pools from %s\n", c0rcfile);
		}
		return -EINVAL;
	}

	rc = read_conf_params(idx, rc_params, rc_params_nr);
	if (rc != 0) {
		ERR("failed to read conf parameters from %s\n", c0rcfile);
		return rc;
	}

	m0_conf.mc_is_oostore            = true;
	m0_conf.mc_is_read_verify        = false;
#if 0
	/* set to default values */
	m0_conf.mc_tm_recv_queue_min_len = M0_NET_TM_RECV_QUEUE_DEF_LEN;
	m0_conf.mc_max_rpc_msg_size      = M0_RPC_DEF_MAX_RPC_MSG_SIZE;
#endif
	/* set to Sage cluster specific values */
	m0_conf.mc_tm_recv_queue_min_len = 64;
	m0_conf.mc_max_rpc_msg_size      = 65536;
	m0_conf.mc_layout_id             = 0;
	if (unit_size) {
		lid = m0_obj_unit_size_to_layout_id(unit_size * 1024);
		if (lid == 0) {
			fprintf(stderr, "invalid unit size %u, it should be: "
				"power of 2, >= 4 and <= 4096\n", unit_size);
			return -EINVAL;
		}
		m0_conf.mc_layout_id = lid;
	}

	/* IDX_MOTR */
	m0_conf.mc_idx_service_id   = M0_IDX_DIX;
	dix_conf.kc_create_meta = false;
	m0_conf.mc_idx_service_conf = &dix_conf;

#if DEBUG
	fprintf(stderr,"\n---\n");
	fprintf(stderr,"%s,", (char *)m0_conf.mc_local_addr);
	fprintf(stderr,"%s,", (char *)m0_conf.mc_ha_addr);
	fprintf(stderr,"%s,", (char *)m0_conf.mc_profile);
	fprintf(stderr,"%s,", (char *)m0_conf.mc_process_fid);
	fprintf(stderr,"%s,", (char *)m0_conf.mc_idx_service_conf);
	fprintf(stderr,"\n---\n");
#endif

	if (!m0trace_on)
		m0_trace_set_mmapped_buffer(false);
	/* m0_instance */
	rc = m0_client_init(&m0_instance, &m0_conf, true);
	if (rc != 0) {
		fprintf(stderr, "failed to initilise the Client API\n");
		return rc;
	}

	/* And finally, client root realm */
	m0_container_init(&container, NULL, &M0_UBER_REALM,
				 m0_instance);
	rc = container.co_realm.re_entity.en_sm.sm_rc;
	if (rc != 0) {
		fprintf(stderr,"failed to open uber realm\n");
		return rc;
	}

	m0_list_init(&isc_reqs);

	/* success */
	uber_realm = container.co_realm;
	return 0;
}

/*
 * m0util_free()
 * free client resources.
 */
int m0util_free(void)
{
	m0_client_fini(m0_instance, true);
	memset(c0rcfile, 0, sizeof(c0rcfile));
	return 0;
}

/*
 * m0util_setrc()
 * set c0apps resource filename
 */
int m0util_setrc(char *prog)
{
	char hostname[MAX_RCFILE_NAME_LEN / 2] = {0};

	/* null */
	if (!prog) {
		fprintf(stderr, "error! null progname.\n");
		return -EINVAL;
	}

	gethostname(hostname, MAX_RCFILE_NAME_LEN / 2);
	snprintf(c0rcfile, MAX_RCFILE_NAME_LEN, "%s/.m0util/%src/%s",
		 getenv("HOME"), prog, hostname);

	/* success */
	return 0;
}

/*
 * open_entity()
 * open m0 entity.
 */
int open_entity(struct m0_entity *entity)
{
	int                  rc;
	struct m0_op *op = NULL;

	m0_entity_open(entity, &op);
	m0_op_launch(&op, 1);
	rc = m0_op_wait(op, M0_BITS(M0_OS_FAILED,
					       M0_OS_STABLE),
			       M0_TIME_NEVER) ?: m0_rc(op);
	m0_op_fini(op);
	m0_op_free(op);

	return rc;
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
