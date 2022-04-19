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

#define M0_TRACE_SUBSYSTEM M0_TRACE_SUBSYS_DTM0
#include "lib/trace.h"

#include "dtm0/ut/helper.h"

#include "ut/ut.h"              /* M0_UT_ASSERT */

#include "lib/misc.h"           /* M0_IS0 */
#include "net/net.h"            /* m0_net_all_xprt_get */
#include "dtm0/service.h"       /* m0_dtm0_service */
#include "dtm0/cfg_default.h"   /* m0_dtm0_domain_cfg_default_dup */


struct m0_reqh_service;

enum {
	M0_DTM0_UT_LOG_SIMPLE_SEG_SIZE  = 0x2000000,
	MAX_RPCS_IN_FLIGHT = 10,
};

#define SERVER_ENDPOINT_ADDR   "0@lo:12345:34:1"
#define SERVER_ENDPOINT        M0_NET_XPRT_PREFIX_DEFAULT":"SERVER_ENDPOINT_ADDR
#define DTM0_UT_CONF_PROCESS   "<0x7200000000000001:5>"

char *ut_dtm0_helper_argv[] = {
	"m0d", "-T", "linux",
	"-D", "dtm0_sdb", "-S", "dtm0_stob",
	"-A", "linuxstob:dtm0_addb_stob",
	"-e", SERVER_ENDPOINT,
	"-H", SERVER_ENDPOINT_ADDR,
	"-w", "10",
	"-f", DTM0_UT_CONF_PROCESS,
	"-c", M0_SRC_PATH("dtm0/conf.xc")
};
static const char *ut_dtm0_client_endpoint = "0@lo:12345:34:2";
const char        *ut_dtm0_helper_log      = "dtm0_ut_server.log";


M0_INTERNAL void m0_ut_dtm0_helper_init(struct m0_ut_dtm0_helper *udh)
{
	struct m0_reqh_service *svc;
	int                     rc;

	M0_PRE(M0_IS0(udh));

	*udh = (struct m0_ut_dtm0_helper){
		.udh_sctx = {
			.rsx_xprts         = m0_net_all_xprt_get(),
			.rsx_xprts_nr      = m0_net_xprt_nr(),
			.rsx_argv          = ut_dtm0_helper_argv,
			.rsx_argc          = ARRAY_SIZE(ut_dtm0_helper_argv),
			.rsx_log_file_name = ut_dtm0_helper_log,
		},
		.udh_cctx = {
			.rcx_net_dom            = &udh->udh_client_net_domain,
			.rcx_local_addr         = ut_dtm0_client_endpoint,
			.rcx_remote_addr        = SERVER_ENDPOINT_ADDR,
			.rcx_max_rpcs_in_flight = MAX_RPCS_IN_FLIGHT,
			.rcx_fid                = &g_process_fid,
		},
		.udh_server_reqh =
			&udh->udh_sctx.rsx_motr_ctx.cc_reqh_ctx.rc_reqh,
		.udh_client_reqh = &udh->udh_cctx.rcx_reqh,
		.udh_server_dtm0_fid = M0_FID_INIT(0x7300000000000001, 0x1c),
		.udh_client_dtm0_fid = M0_FID_INIT(0x7300000000000001, 0x1a),
	};
	rc = m0_rpc_server_start(&udh->udh_sctx);
	M0_UT_ASSERT(rc == 0);
	rc = m0_net_domain_init(&udh->udh_client_net_domain,
				m0_net_xprt_default_get());
	M0_UT_ASSERT(rc == 0);
	rc = m0_rpc_client_start(&udh->udh_cctx);
	M0_UT_ASSERT(rc == 0);
	rc = m0_dtm_client_service_start(udh->udh_client_reqh,
					 &udh->udh_client_dtm0_fid,
					 &svc);
	M0_UT_ASSERT(rc == 0);

	udh->udh_client_dtm0_service = container_of(svc, struct m0_dtm0_service,
						    dos_generic);

	svc = m0_reqh_service_lookup(udh->udh_server_reqh,
	                             &udh->udh_server_dtm0_fid);
	/* TODO export the function which does bob_of() */
	udh->udh_server_dtm0_service = container_of(svc, struct m0_dtm0_service,
	                                            dos_generic);
}

M0_INTERNAL void m0_ut_dtm0_helper_fini(struct m0_ut_dtm0_helper *udh)
{
	int rc;

	m0_dtm_client_service_stop(&udh->udh_client_dtm0_service->dos_generic);
	rc = m0_rpc_client_stop(&udh->udh_cctx);
	M0_UT_ASSERT(rc == 0);
	m0_net_domain_fini(&udh->udh_client_net_domain);
	m0_rpc_server_stop(&udh->udh_sctx);
}

M0_INTERNAL struct dtm0_ut_log_ctx *dtm0_ut_log_init(void)
{
	struct dtm0_ut_log_ctx *lctx;
	int                     rc;

	M0_ALLOC_PTR(lctx);
	M0_UT_ASSERT(lctx != NULL);

	m0_be_ut_backend_init(&lctx->ut_be);
	m0_be_ut_seg_init(&lctx->ut_seg, &lctx->ut_be,
			  M0_DTM0_UT_LOG_SIMPLE_SEG_SIZE);
	rc = m0_dtm0_domain_cfg_default_dup(&lctx->dod_cfg, true);
	M0_UT_ASSERT(rc == 0);
	lctx->dod_cfg.dodc_log.dlc_be_domain = &lctx->ut_be.but_dom;
	lctx->dod_cfg.dodc_log.dlc_seg =
		m0_be_domain_seg_first(lctx->dod_cfg.dodc_log.dlc_be_domain);

	rc = m0_dtm0_log_create(&lctx->dol, &lctx->dod_cfg.dodc_log);
	M0_UT_ASSERT(rc == 0);

	return lctx;
}

M0_INTERNAL void dtm0_ut_log_fini(struct dtm0_ut_log_ctx *lctx)
{
	m0_dtm0_log_destroy(&lctx->dol);
	m0_dtm0_domain_cfg_free(&lctx->dod_cfg);
	m0_be_ut_seg_fini(&lctx->ut_seg);
	m0_be_ut_backend_fini(&lctx->ut_be);
	m0_free(lctx);

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
