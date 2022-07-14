/* -*- C -*- */
/*
 * Copyright (c) 2013-2020 Seagate Technology LLC and/or its Affiliates
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
 *
 */

/**
 * @addtogroup dtm0
 *
 * @{
 */

#define M0_TRACE_SUBSYSTEM M0_TRACE_SUBSYS_UT
#include "lib/trace.h"

#include "dtm0/net.h"
#include "ut/ut.h"              /* M0_UT_ASSERT */
#include "fid/fid.h"            /* M0_FID_INIT */

#include "dtm0/service.h"       /* m0_dtm0_service */
#include "dtm0/ut/helper.h"     /* m0_ut_dtm0_helper_init */


enum {
	MSG_NR = 0x100,
};

#if 0
/*
 * TODO: implement it fully.
 */
struct ut_motr_net {
	void *domain;
	void *reqh;
	void *buf_pool;
	void *rpc_machine;
};
#endif

struct m0_ut_dtm0_net_helper {
	/* TODO: use ut_motr_net here */
	struct m0_ut_dtm0_helper base;
	struct m0_dtm0_net       dnet_client;
	struct m0_dtm0_net       dnet_server;
};
#if 0
static int ut_motr_net_init(struct ut_motr_net *net, const m0_fid *fid,
			    const char *ep_addr)
{
	enum { NR_TMS = 1 };
	int      rc;
	uint32_t bufs_nr;

	rc = m0_net_domain_init(&net->domain, m0_net_xprt_default_get());
	M0_ASSERT(rc == 0);
	bufs_nr = m0_rpc_bufs_nr(M0_NET_TM_RECV_QUEUE_DEF_LEN, NR_TMS);
	rc = m0_rpc_net_buffer_pool_setup(&ut_client_net_dom, &net->buf_pool,
					  bufs_nr, NR_TMS);
	M0_ASSERT(rc == 0);
	rc = M0_REQH_INIT(&net->reqh,
			  .rhia_dtm     = (void *)1,
			  .rhia_db      = NULL,
			  .rhia_mdstore = (void *)1,
			  .rhia_fid     = fid) ?:
		m0_rpc_machine_init(&net->rpc_machine, &net->domain,
				    ep_addr, &net->reqh, &net->buf_pool,
				    M0_BUFFER_ANY_COLOUR,
				    M0_RPC_DEF_MAX_RPC_MSG_SIZE,
				    M0_NET_TM_RECV_QUEUE_DEF_LEN);
	return rc;
}
#endif

#if 0
static void reqh_service_ctx_ut__remote_rmach_fini(void)
{
	m0_rpc_machine_fini(&ut_rmach);
	m0_reqh_fini(&ut_reqh);
	m0_rpc_net_buffer_pool_cleanup(&ut_buf_pool);
	m0_net_domain_fini(&ut_client_net_dom);
}
#endif

static void m0_ut_dtm0_net_helper_init(struct m0_ut_dtm0_net_helper *h)
{
	struct m0_ut_dtm0_helper *udh = &h->base;

	m0_ut_dtm0_helper_init(udh);
	m0_dtm0_net_init(&h->dnet_server,
			 &(struct m0_dtm0_net_cfg) {
				.dnc_reqh = udh->udh_server_reqh, });
	m0_dtm0_net_init(&h->dnet_client,
			 &(struct m0_dtm0_net_cfg) {
				.dnc_reqh = udh->udh_client_reqh, });
}

static void m0_ut_dtm0_net_helper_fini(struct m0_ut_dtm0_net_helper *h)
{
	m0_dtm0_net_fini(&h->dnet_client);
	m0_dtm0_net_fini(&h->dnet_server);
	m0_ut_dtm0_helper_fini(&h->base);
}

void m0_dtm0_ut_net_init_fini(void)
{
	struct m0_reqh     *reqh;
	struct m0_dtm0_net *dnet;
	int                 rc;

	M0_ALLOC_PTR(reqh);
	M0_UT_ASSERT(reqh != NULL);
	M0_ALLOC_PTR(dnet);
	M0_UT_ASSERT(dnet != NULL);

	rc = M0_REQH_INIT(reqh,
			  .rhia_dtm     = (void*)1,
			  .rhia_mdstore = (void*)1,
			  .rhia_fid     = &g_process_fid);
	M0_UT_ASSERT(rc == 0);
	m0_reqh_start(reqh);
	m0_dtm0_net_init(dnet,
			 &(struct m0_dtm0_net_cfg) { .dnc_reqh = reqh, });
	m0_dtm0_net_fini(dnet);
	m0_reqh_services_terminate(reqh);
	m0_reqh_fini(reqh);

	m0_free(dnet);
	m0_free(reqh);
}

void m0_dtm0_ut_net_tranceive(void)
{
	struct m0_ut_dtm0_net_helper *h;
	struct m0_dtm0_msg           *fop;
	struct m0_dtm0_msg            msg_out;
	struct m0_be_op              *op;
	struct m0_be_op               op_out = {};
	struct m0_fid                *fid;
	struct m0_fid                 fid_out;
	bool                          successful;
	int                           i;
	int                           j;
	struct m0_fid                *target;

	M0_ALLOC_PTR(h);
	M0_UT_ASSERT(h != NULL);

	m0_ut_dtm0_net_helper_init(h);
	target = &h->base.udh_client_dtm0_fid;

	M0_ALLOC_ARR(fid, MSG_NR);
	M0_UT_ASSERT(fid != NULL);
	for (i = 0; i < MSG_NR; ++i) {
		fid[i] = M0_FID_INIT(0, i+1);  /* TODO set fid type */
	}
	M0_ALLOC_ARR(fop, MSG_NR);
	M0_UT_ASSERT(fop != NULL);
	for (i = 0; i < MSG_NR; ++i) {
		fop[i] = M0_DTM0_MSG_EOL_INIT(&fid[i]);
	}
	M0_ALLOC_ARR(op, MSG_NR);
	M0_UT_ASSERT(op != NULL);
	for (i = 0; i < MSG_NR; ++i)
		m0_be_op_init(&op[i]);

	for (i = 0; i < MSG_NR; ++i)
		m0_dtm0_net_send(&h->dnet_server,
				 &op[i], target, &fop[i], NULL);

	m0_be_op_init(&op_out);
	for (i = 0; i < MSG_NR; ++i) {
		successful = false;
		msg_out = (struct m0_dtm0_msg) {};
		m0_dtm0_net_recv(&h->dnet_client,
				 &op_out, &successful, &msg_out, M0_DMT_EOL);
		m0_be_op_wait(&op_out);
		M0_UT_ASSERT(successful);
		M0_UT_ASSERT(memcmp(&msg_out, &((struct m0_dtm0_msg) {}),
				    sizeof(msg_out)) != 0);
		M0_UT_ASSERT(msg_out.dm_type == M0_DMT_EOL);
		fid_out = msg_out.dm_msg.eol.dme_initiator;
		m0_be_op_reset(&op_out);
		/* TODO: it would be nice to have m0_find() */
		j = m0_fold(m, a, MSG_NR, -1,
			    a  == -1 ? m0_fid_eq(&fid_out, &fid[m]) ? m :
			    a : a);
		M0_UT_ASSERT(j != -1);
		M0_UT_ASSERT(fid[j].f_key == j + 1);
		fid[j] = M0_FID0;
	}
	m0_be_op_fini(&op_out);
	for (i = 0; i < MSG_NR; ++i)
		M0_UT_ASSERT(m0_fid_eq(&fid[i], &M0_FID0));
	m0_free(fid);
	for (i = 0; i < MSG_NR; ++i) {
		m0_be_op_wait(&op[i]);
		m0_be_op_fini(&op[i]);
	}
	m0_free(op);
	m0_free(fop);

	m0_ut_dtm0_net_helper_fini(h);
	m0_free(h);
}


#undef M0_TRACE_SUBSYSTEM

/** @} end of dtm0 group */

/*
 *  Local variables:
 *  c-indentation-style: "K&R"
 *  c-basic-offset: 8
 *  tab-width: 8
 *  fill-column: 80
 *  scroll-step: 1
 *  End:
 */
/*
 * vim: tabstop=8 shiftwidth=8 noexpandtab textwidth=80 nowrap
 */
