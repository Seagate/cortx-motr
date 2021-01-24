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


#include "dtm0/fop.h"
#include "net/net.h"
#include "rpc/rpclib.h"
#include "ut/ut.h"

#define SERVER_ENDPOINT_ADDR    "0@lo:12345:34:1"
#define SERVER_ENDPOINT         "lnet:" SERVER_ENDPOINT_ADDR
#define DTM0_UT_CONF_PROCESS    "<0x7200000000000001:5>"
#define DTM0_UT_LOG             "dtm0_ut_server.log"
#define M0_FID(c_, k_)  { .f_container = c_, .f_key = k_ }

enum { MAX_RPCS_IN_FLIGHT = 10 };

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

extern void dtm0_ut_send_fops(struct m0_rpc_session *cl_rpc_session);

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

struct m0_reqh_service *client_service_start(struct m0_reqh *reqh)
{
	struct m0_reqh_service_type *svct;
	struct m0_reqh_service      *reqh_svc;
	struct m0_fid fid = M0_FID(0x7300000000000001, 0x1a);
	int rc;

	svct = m0_reqh_service_type_find("M0_CST_DTM0");
	M0_UT_ASSERT(svct != NULL);

	rc = m0_reqh_service_allocate(&reqh_svc, svct, NULL);
	M0_UT_ASSERT(rc == 0);

	m0_reqh_service_init(reqh_svc, reqh, &fid);

	rc = m0_reqh_service_start(reqh_svc);
	M0_UT_ASSERT(rc == 0);

	return reqh_svc;
}

void client_service_stop(struct m0_reqh_service *svc)
{
	m0_reqh_service_prepare_to_stop(svc);
	m0_reqh_service_stop(svc);
	m0_reqh_service_fini(svc);
}

extern struct m0_rpc_link g_xxx_dtm0_link;
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
	struct m0_rpc_link *rlink = &g_xxx_dtm0_link;
	struct m0_fid      rfid = M0_FID(0x7300000000000001, 0x1a);
	struct m0_reqh_service *cli_srv;

	rc = m0_rpc_server_start(&sctx);
	M0_UT_ASSERT(rc == 0);

	dtm0_ut_client_init(&cctx, cl_ep_addr, srv_ep_addr, dtm0_xprts[0]);


	cli_srv = client_service_start(&cctx.cl_ctx.rcx_reqh);

	rc = m0_rpc_link_init(rlink, m0_motr_to_rmach(&sctx.rsx_motr_ctx),
			      &rfid, cl_ep_addr, 10);
	M0_UT_ASSERT(rc == 0);
	m0_rpc_link_connect_sync(rlink, M0_TIME_NEVER);


	dtm0_ut_send_fops(&cctx.cl_ctx.rcx_session);


	client_service_stop(cli_srv);
	m0_rpc_link_disconnect_sync(rlink, M0_TIME_NEVER);
	m0_rpc_link_fini(rlink);
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
