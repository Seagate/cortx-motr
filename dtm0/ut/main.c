/* -*- C -*- */
/*
 * Copyright (c) 2012-2020 Seagate Technology LLC and/or its Affiliates
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


#include "dtm0/clk_src.h"
#include "dtm0/fop.h"
#include "dtm0/helper.h"
#include "dtm0/service.h"
#include "dtm0/tx_desc.h"
#include "net/net.h"
#include "rpc/rpclib.h"
#include "ut/ut.h"

#define M0_FID(c_, k_)  { .f_container = c_, .f_key = k_ }
#define SERVER_ENDPOINT_ADDR    "0@lo:12345:34:1"
#define SERVER_ENDPOINT         "lnet:" SERVER_ENDPOINT_ADDR
#define DTM0_UT_CONF_PROCESS    "<0x7200000000000001:5>"
#define DTM0_UT_LOG             "dtm0_ut_server.log"

enum { MAX_RPCS_IN_FLIGHT = 10 };

static struct m0_fid cli_srv_fid = M0_FID(0x7300000000000001, 0x1a);
static struct m0_fid srv_dtm0_fid = M0_FID(0x7300000000000001, 0x1c);
static const char *cl_ep_addr =  "0@lo:12345:34:2";
static const char *srv_ep_addr =  SERVER_ENDPOINT_ADDR;
static char *dtm0_ut_argv[] = { "m0d", "-T", "linux",
			       "-D", "dtm0_sdb", "-S", "dtm0_stob",
			       "-A", "linuxstob:dtm0_addb_stob",
			       "-e", SERVER_ENDPOINT,
			       "-H", SERVER_ENDPOINT_ADDR,
			       "-w", "10",
			       "-f", DTM0_UT_CONF_PROCESS,
			       "-c", M0_SRC_PATH("dtm0/conf.xc")};

struct cl_ctx {
	struct m0_net_domain     cl_ndom;
	struct m0_rpc_client_ctx cl_ctx;
};

static struct m0_net_xprt *dtm0_xprts[] = {
	&m0_net_lnet_xprt,
};

static struct dtm0_rep_fop *reply(struct m0_rpc_item *reply)
{
	return m0_fop_data(m0_rpc_item_to_fop(reply));
}

static void dtm0_ut_send_fops(struct m0_rpc_session *cl_rpc_session)
{
	int                    rc;
        struct m0_fop         *fop;
	struct dtm0_rep_fop   *rep;
	struct dtm0_req_fop   *req;

	struct m0_dtm0_tx_desc txr = {};
	struct m0_dtm0_tid      reply_data;

	struct m0_dtm0_clk_src dcs;
	struct m0_dtm0_ts      now;


	m0_dtm0_clk_src_init(&dcs, M0_DTM0_CS_PHYS);
	rc = m0_dtm0_clk_src_now(&dcs, &now);
	M0_UT_ASSERT(rc == 0);

	M0_PRE(cl_rpc_session != NULL);

	txr.dtd_id = (struct m0_dtm0_tid) { .dti_ts = now, .dti_fid = g_process_fid};
	fop = m0_fop_alloc_at(cl_rpc_session,
			      &dtm0_req_fop_fopt);
	req = m0_fop_data(fop);
	req->dtr_msg = DMT_EXECUTE;
	req->dtr_txr = txr;
	rc = m0_rpc_post_sync(fop, cl_rpc_session,
			      &dtm0_req_fop_rpc_item_ops,
			      M0_TIME_IMMEDIATELY);
	M0_UT_ASSERT(rc == 0);
	rep = reply(fop->f_item.ri_reply);
	reply_data = rep->dr_txr.dtd_id;

	M0_ASSERT(m0_dtm0_ts__invariant(&reply_data.dti_ts));

	M0_UT_ASSERT(m0_fid_cmp(&g_process_fid, &reply_data.dti_fid) == 0);
	M0_UT_ASSERT(m0_dtm0_ts_cmp(&dcs, &now, &reply_data.dti_ts) == M0_DTS_EQ);

	m0_fop_put_lock(fop);
}

static void dtm0_ut_client_init(struct cl_ctx *cctx, const char *cl_ep_addr,
			      const char *srv_ep_addr, struct m0_net_xprt *xprt)
{
	int                       rc;
	struct m0_rpc_client_ctx *cl_ctx;

	M0_PRE(cctx != NULL && cl_ep_addr != NULL &&
	       srv_ep_addr != NULL && xprt != NULL);

	rc = m0_net_domain_init(&cctx->cl_ndom, xprt);
	M0_UT_ASSERT(rc == 0);

	cl_ctx = &cctx->cl_ctx;

	cl_ctx->rcx_net_dom            = &cctx->cl_ndom;
	cl_ctx->rcx_local_addr         = cl_ep_addr;
	cl_ctx->rcx_remote_addr        = srv_ep_addr;
	cl_ctx->rcx_max_rpcs_in_flight = MAX_RPCS_IN_FLIGHT;
	cl_ctx->rcx_fid                = &g_process_fid;

	rc = m0_rpc_client_start(cl_ctx);
	M0_UT_ASSERT(rc == 0);
}

static void dtm0_ut_client_fini(struct cl_ctx *cctx)
{
	int rc;

	rc = m0_rpc_client_stop(&cctx->cl_ctx);
	M0_UT_ASSERT(rc == 0);

	m0_net_domain_fini(&cctx->cl_ndom);
}

static void dtm0_ut_service(void)
{
	int rc;
	struct cl_ctx            cctx = {};
	struct m0_rpc_server_ctx sctx = {
		.rsx_xprts         = dtm0_xprts,
		.rsx_xprts_nr      = ARRAY_SIZE(dtm0_xprts),
		.rsx_argv          = dtm0_ut_argv,
		.rsx_argc          = ARRAY_SIZE(dtm0_ut_argv),
		.rsx_log_file_name = DTM0_UT_LOG,
	};
	struct m0_reqh_service  *cli_srv;
	struct m0_reqh_service  *srv_srv;
	struct m0_reqh          *srv_reqh = &sctx.rsx_motr_ctx.cc_reqh_ctx.rc_reqh;

	rc = m0_rpc_server_start(&sctx);
	M0_UT_ASSERT(rc == 0);

	dtm0_ut_client_init(&cctx, cl_ep_addr, srv_ep_addr, dtm0_xprts[0]);
	cli_srv = m0_dtm__client_service_start(&cctx.cl_ctx.rcx_reqh, &cli_srv_fid);
	M0_UT_ASSERT(cli_srv != NULL);
	srv_srv = m0_reqh_service_lookup(srv_reqh, &srv_dtm0_fid);
	rc = m0_dtm0_service_process_connect(srv_srv, &cli_srv_fid, cl_ep_addr);
	M0_UT_ASSERT(rc == 0);

	dtm0_ut_send_fops(&cctx.cl_ctx.rcx_session);

	rc = m0_dtm0_service_process_disconnect(srv_srv, &cli_srv_fid);
	M0_UT_ASSERT(rc == 0);
	m0_dtm__client_service_stop(cli_srv);
	dtm0_ut_client_fini(&cctx);
	m0_rpc_server_stop(&sctx);
}

struct m0_ut_suite dtm0_ut = {
        .ts_name = "dtm0-ut",
        .ts_tests = {
                { "service", dtm0_ut_service},
		{ NULL, NULL },
	}
};

/*
 *  Local variables:
 *  c-indentation-style: "K&R"
 *  c-basic-offset: 8
 *  tab-width: 8
 *  fill-column: 80
 *  scroll-step: 1
 *  End:
 */
