/* -*- C -*- */
/*
 * Copyright (c) 2017-2020 Seagate Technology LLC and/or its Affiliates
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


#define M0_TRACE_SUBSYSTEM M0_TRACE_SUBSYS_UT
#include "lib/trace.h"

#include "lib/finject.h"
#include "conf/ut/common.h"   /* CLIENT_ENDPOINT_ADDR, SERVER_ENDPOINT_ADDR */
#include "fis/fi_command.h"
#include "rpc/rpclib.h"       /* m0_rpc_server_ctx, m0_rpc_client_ctx */
#include "ut/misc.h"          /* M0_UT_CONF_PROCESS */
#include "ut/ut.h"

enum {
	MAX_RPCS_IN_FLIGHT = 1,
};

static struct m0_net_domain     client_net_dom;
static struct m0_rpc_client_ctx cctx = {
        .rcx_net_dom            = &client_net_dom,
        .rcx_local_addr         = CLIENT_ENDPOINT_ADDR,
        .rcx_remote_addr        = SERVER_ENDPOINT_ADDR,
        .rcx_max_rpcs_in_flight = MAX_RPCS_IN_FLIGHT,
        .rcx_fid                = &g_process_fid,
};

static void fis_ut_motr_start(struct m0_rpc_server_ctx *rctx)
{
	int rc;
#define NAME(ext) "fis-ut" ext
	char *argv[] = {
		NAME(""), "-T", "AD", "-D", NAME(".db"), "-j" /* fis enabled */,
		"-S", NAME(".stob"), "-A", "linuxstob:"NAME("-addb.stob"),
		"-w", "10", "-e", SERVER_ENDPOINT, "-H", SERVER_ENDPOINT_ADDR,
		"-f", M0_UT_CONF_PROCESS,
		"-c", M0_SRC_PATH("fis/ut/fis.xc")
	};
	*rctx = (struct m0_rpc_server_ctx) {
		.rsx_xprts         = m0_net_all_xprt_get(),
		.rsx_xprts_nr      = m0_net_xprt_nr(),
		.rsx_argv          = argv,
		.rsx_argc          = ARRAY_SIZE(argv),
		.rsx_log_file_name = NAME(".log")
	};
#undef NAME
	rc = m0_rpc_server_start(rctx);
	M0_UT_ASSERT(rc == 0);
}

static void fis_ut_motr_stop(struct m0_rpc_server_ctx *rctx)
{
	m0_rpc_server_stop(rctx);
}

static void fis_ut_client_start(void)
{
	int rc;

	rc = m0_net_domain_init(&client_net_dom, m0_net_xprt_default_get());
	M0_UT_ASSERT(rc == 0);
	rc = m0_rpc_client_start(&cctx);
	M0_UT_ASSERT(rc == 0);
}

static void fis_ut_client_stop(void)
{
	int rc = m0_rpc_client_stop(&cctx);
	M0_UT_ASSERT(rc == 0);
	m0_net_domain_fini(&client_net_dom);
}

static bool fault_is_injected(void)
{
	return M0_FI_ENABLED("test");
}

extern struct m0_fop_type m0_fic_req_fopt;

static void test_fi_command_post(void)
{
	struct m0_rpc_server_ctx rctx;
	const char              *func = "fault_is_injected";
	const char              *tag  = "test";
	int                      rc;

	/* test enabled state */
	m0_fi_enable(func, tag);
	M0_UT_ASSERT(fault_is_injected());

	m0_fi_disable(func, tag);
	/* test disabled state */
	M0_UT_ASSERT(!fault_is_injected());

	/* Now do control FI via Fault Injection Service */
	fis_ut_motr_start(&rctx);
	fis_ut_client_start();

	rc = m0_fi_command_post_sync(&cctx.rcx_session, func, tag,
				     M0_FI_DISP_ENABLE, 0, 0);
	M0_UT_ASSERT(rc == 0);
	/* test enabled state */
	M0_UT_ASSERT(fault_is_injected());
	/* make sure of being still enabled */
	M0_UT_ASSERT(fault_is_injected());

	rc = m0_fi_command_post_sync(&cctx.rcx_session, func, tag,
				     M0_FI_DISP_DISABLE, 0, 0);
	M0_UT_ASSERT(rc == 0);
	/* test disabled state */
	M0_UT_ASSERT(!fault_is_injected());

	rc = m0_fi_command_post_sync(&cctx.rcx_session, func, tag,
				     M0_FI_DISP_ENABLE_ONCE, 0, 0);
	M0_UT_ASSERT(rc == 0);
	/* make sure of exactly one shot */
	M0_UT_ASSERT(fault_is_injected());
	M0_UT_ASSERT(!fault_is_injected());

	rc = m0_fi_command_post_sync(&cctx.rcx_session, func, tag,
				     M0_FI_DISP_RANDOMIZE, 0, 0);
	M0_UT_ASSERT(rc == 0);
	rc = m0_fi_command_post_sync(&cctx.rcx_session, func, tag,
				     M0_FI_DISP_DISABLE, 0, 0);
	M0_UT_ASSERT(rc == 0);

	rc = m0_fi_command_post_sync(&cctx.rcx_session, func, tag,
				     M0_FI_DISP_DO_OFF_N_ON_M, 2, 1);
	M0_UT_ASSERT(rc == 0);
	M0_UT_ASSERT(!fault_is_injected());
	M0_UT_ASSERT(!fault_is_injected());
	M0_UT_ASSERT(fault_is_injected());

	rc = m0_fi_command_post_sync(&cctx.rcx_session, func, tag,
				     M0_FI_DISP_DISABLE, 0, 0);
	M0_UT_ASSERT(rc == 0);
	M0_UT_ASSERT(!fault_is_injected());

	/* make sure unsupported disposition value not welcomed */
	rc = m0_fi_command_post_sync(&cctx.rcx_session, func, tag, 255, 0, 0);
	M0_UT_ASSERT(rc == -EINVAL);

	fis_ut_client_stop();
	fis_ut_motr_stop(&rctx);
}

static void test_fi_command_post_fail(void)
{
	struct m0_rpc_server_ctx rctx;
	const char              *func = "fault_is_injected";
	const char              *tag  = "test";
	int                      rc;

	fis_ut_motr_start(&rctx);
	fis_ut_client_start();

	/*
	 * Inject 'no_mem' fault to prevent FOM creation. Rpc post is to result
	 * in standard timeout error. Please be prepared to wait for a solid
	 * minute.
	 */
	m0_fi_enable_once("fi_command_fom_create", "no_mem");
	rc= m0_fi_command_post_sync(&cctx.rcx_session, func, tag,
				    M0_FI_DISP_ENABLE, 0, 0);
	M0_UT_ASSERT(rc == -ETIMEDOUT);
	M0_UT_ASSERT(!fault_is_injected());

	fis_ut_client_stop();
	fis_ut_motr_stop(&rctx);
}

struct m0_ut_suite fis_ut = {
	.ts_name  = "fis-ut",
	.ts_tests = {
		{ "post",      test_fi_command_post },
		{ "post-fail", test_fi_command_post_fail },
		{ NULL, NULL }
	},
	.ts_owners = "IV",
};

#undef M0_TRACE_SUBSYSTEM

/*
 *  Local variables:
 *  c-indentation-style: "K&R"
 *  c-basic-offset: 8
 *  tab-width: 8
 *  fill-column: 80
 *  scroll-step: 1
 *  End:
 */
