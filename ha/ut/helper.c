/* -*- C -*- */
/*
 * Copyright (c) 2016-2020 Seagate Technology LLC and/or its Affiliates
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
 * @addtogroup ha
 *
 * @{
 */

#define M0_TRACE_SUBSYSTEM M0_TRACE_SUBSYS_UT
#include "lib/trace.h"

#include "ha/ut/helper.h"
#include "ut/ut.h"

#include "lib/types.h"          /* uint32_t */
#include "fid/fid.h"            /* m0_fid */
#include "net/lnet/lnet.h"      /* m0_net_lnet_xprt */
#include "rpc/rpclib.h"         /* m0_rpc_client_connect */


M0_INTERNAL void m0_ha_ut_rpc_ctx_init(struct m0_ha_ut_rpc_ctx *ctx)
{
	struct m0_fid       process_fid = M0_FID_TINIT('r', 0, 1);
	const uint32_t      tms_nr      = 1;
	const uint32_t      bufs_nr     =
		m0_rpc_bufs_nr(M0_NET_TM_RECV_QUEUE_DEF_LEN, tms_nr);
	const char         *ep          = "0@lo:12345:42:100";
	int                 rc;
	struct m0_net_xprt *xprt = m0_net_xprt_get();

	rc = m0_net_domain_init(&ctx->hurc_net_domain, xprt);
	M0_ASSERT(rc == 0);
	rc = m0_rpc_net_buffer_pool_setup(&ctx->hurc_net_domain,
					  &ctx->hurc_buffer_pool,
					  bufs_nr, tms_nr);
	M0_ASSERT(rc == 0);
	rc = M0_REQH_INIT(&ctx->hurc_reqh,
			  .rhia_dtm          = (void*)1,
			  .rhia_mdstore      = (void*)1,
			  .rhia_fid          = &process_fid);
	M0_ASSERT(rc == 0);
	m0_reqh_start(&ctx->hurc_reqh);
	rc = m0_rpc_machine_init(&ctx->hurc_rpc_machine,
				 &ctx->hurc_net_domain, ep,
				 &ctx->hurc_reqh,
				 &ctx->hurc_buffer_pool,
				 M0_BUFFER_ANY_COLOUR,
				 M0_RPC_DEF_MAX_RPC_MSG_SIZE,
				 M0_NET_TM_RECV_QUEUE_DEF_LEN);
	M0_ASSERT(rc == 0);
}

M0_INTERNAL void m0_ha_ut_rpc_ctx_fini(struct m0_ha_ut_rpc_ctx *ctx)
{
	m0_reqh_shutdown_wait(&ctx->hurc_reqh);
	m0_rpc_machine_fini(&ctx->hurc_rpc_machine);
	m0_reqh_services_terminate(&ctx->hurc_reqh);
	m0_reqh_fini(&ctx->hurc_reqh);
	m0_rpc_net_buffer_pool_cleanup(&ctx->hurc_buffer_pool);
	m0_net_domain_fini(&ctx->hurc_net_domain);
}

M0_INTERNAL void
m0_ha_ut_rpc_session_ctx_init(struct m0_ha_ut_rpc_session_ctx *sctx,
                              struct m0_ha_ut_rpc_ctx         *ctx)
{
	int rc;

	rc = m0_rpc_client_connect(&sctx->husc_conn, &sctx->husc_session,
	                           &ctx->hurc_rpc_machine,
	                           m0_rpc_machine_ep(&ctx->hurc_rpc_machine),
				   NULL, M0_HA_UT_MAX_RPCS_IN_FLIGHT,
				   M0_TIME_NEVER);
	M0_UT_ASSERT(rc == 0);
}

M0_INTERNAL void
m0_ha_ut_rpc_session_ctx_fini(struct m0_ha_ut_rpc_session_ctx *sctx)
{
	int rc;

	rc = m0_rpc_session_destroy(&sctx->husc_session, M0_TIME_NEVER);
	M0_UT_ASSERT(rc == 0);
	rc = m0_rpc_conn_destroy(&sctx->husc_conn, M0_TIME_NEVER);
	M0_UT_ASSERT(rc == 0);
}

#undef M0_TRACE_SUBSYSTEM

/** @} end of ha group */

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
