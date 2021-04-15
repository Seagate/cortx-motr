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


#include "ut/ut.h"
#include "lib/memory.h"
#include "lib/processor.h"
#include "lib/trace.h"
#include "lib/finject.h"
#include "fop/fop.h"
#include "reqh/reqh.h"

#include "rpc/session.h"
#include "rpc/it/ping_fop.h"
#include "rpc/it/ping_fop_xc.h"
#include "rpc/rpclib.h"
#include "net/lnet/lnet.h"

#include "ut/cs_service.h"
#include "ut/cs_fop.h"
#include "ut/cs_fop_xc.h"

#include "rpc/ut/clnt_srv_ctx.c"   /* sctx, cctx, MAX_RETRIES */

#ifdef ENABLE_FAULT_INJECTION
static void test_m0_rpc_server_start(void)
{
	int rc;

	m0_fi_enable_once("m0_cs_init", "fake_error");
	sctx_reset();
	rc = m0_rpc_server_start(&sctx);
	M0_UT_ASSERT(rc != 0);

	m0_fi_enable_once("m0_cs_setup_env", "fake_error");
	sctx_reset();
	rc = m0_rpc_server_start(&sctx);
	M0_UT_ASSERT(rc != 0);
}

static void test_m0_rpc_client_start(void)
{
	int rc;

	sctx_reset();
	rc = m0_rpc_server_start(&sctx);
	M0_UT_ASSERT(rc == 0);
	if (rc != 0)
		return;

	m0_fi_enable_once("m0_rpc_machine_init", "fake_error");
	M0_UT_ASSERT(m0_rpc_client_start(&cctx) != 0);

	m0_fi_enable_once("m0_net_end_point_create", "fake_error");
	M0_UT_ASSERT(m0_rpc_client_start(&cctx) != 0);

	m0_fi_enable_once("m0_rpc_conn_create", "fake_error");
	M0_UT_ASSERT(m0_rpc_client_start(&cctx) != 0);

	m0_fi_enable_once("m0_rpc_conn_establish", "fake_error");
	M0_UT_ASSERT(m0_rpc_client_start(&cctx) != 0);

	m0_fi_enable_once("m0_rpc_session_establish", "fake_error");
	M0_UT_ASSERT(m0_rpc_client_start(&cctx) != 0);

	m0_rpc_server_stop(&sctx);
}

static void test_rpclib_error_paths(void)
{
	test_m0_rpc_server_start();
	test_m0_rpc_client_start();
}
#else
static void test_rpclib_error_paths(void)
{
}
#endif

static int send_fop(struct m0_rpc_session *session)
{
	int                   rc;
	struct m0_fop         *fop;
	struct cs_ds2_req_fop *cs_ds2_fop;

	fop = m0_fop_alloc_at(session, &cs_ds2_req_fop_fopt);
	M0_UT_ASSERT(fop != NULL);
	if (fop == NULL) {
		rc = -ENOMEM;
		goto out;
	}

	cs_ds2_fop = m0_fop_data(fop);
	cs_ds2_fop->csr_value = 0xaaf5;

	rc = m0_rpc_post_sync(fop, session, &cs_ds_req_fop_rpc_item_ops,
			      0 /* deadline */);
	M0_UT_ASSERT(rc == 0);
	M0_UT_ASSERT(fop->f_item.ri_error == 0);
	M0_UT_ASSERT(fop->f_item.ri_reply != 0);

	m0_fop_put_lock(fop);
out:
	return rc;
}

static void test_rpclib(void)
{
	int rc;

	/*
	 * There is no need to initialize xprt explicitly if client and server
	 * run within a single process, because in this case transport is
	 * initialized by m0_rpc_server_start().
	 */

	sctx_reset();
	rc = m0_rpc_server_start(&sctx);
	M0_UT_ASSERT(rc == 0);
	if (rc != 0)
		return;
	rc = m0_rpc_client_start(&cctx);
	M0_UT_ASSERT(rc == 0);
	if (rc != 0)
		goto server_fini;

	rc = send_fop(&cctx.rcx_session);
	M0_UT_ASSERT(rc == 0);

	rc = m0_rpc_client_stop(&cctx);
	M0_UT_ASSERT(rc == 0);

server_fini:
	m0_rpc_server_stop(&sctx);
	return;
}

static int test_rpclib_init(void)
{
	int rc;
	rc = m0_net_domain_init(&client_net_dom, m0_net_xprt_default_get());
	M0_ASSERT(rc == 0);
	return rc;
}

static int test_rpclib_fini(void)
{
	m0_net_domain_fini(&client_net_dom);
	return 0;
}

struct m0_ut_suite rpclib_ut = {
	.ts_name = "rpc-lib-ut",
	.ts_init = test_rpclib_init,
	.ts_fini = test_rpclib_fini,
	.ts_tests = {
		{ "rpclib",             test_rpclib },
		{ "rpclib_error_paths", test_rpclib_error_paths },
		{ NULL, NULL }
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
