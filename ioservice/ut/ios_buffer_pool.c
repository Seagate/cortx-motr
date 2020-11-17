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


#include "ioservice/io_service.c"
#include "net/bulk_mem.h"         /* m0_net_bulk_mem_xprt */
#include "ut/misc.h"              /* M0_UT_PATH */
#include "ut/ut.h"
#include "net/sock/sock.h"

extern const struct m0_tl_descr bufferpools_tl;

/* Motr setup arguments. */
static char *ios_ut_bp_singledom_cmd[] = { "m0d", "-T", "AD",
				"-D", "cs_sdb", "-S", "cs_stob",
				"-A", "linuxstob:cs_addb_stob",
				"-e", "lnet:0@lo:12345:34:1",
				"-H", "0@lo:12345:34:1",
				"-w", "10",
				"-f", M0_UT_CONF_PROCESS,
				"-c", M0_UT_PATH("conf.xc")};

static char *ios_ut_bp_multidom_cmd[] = { "m0d", "-T", "AD",
				"-D", "cs_sdb", "-S", "cs_stob",
				"-A", "linuxstob:cs_addb_stob",
				"-w", "10",
				"-e", "lnet:0@lo:12345:34:1",
				"-e", "bulk-mem:127.0.0.1:35678",
				"-H", "0@lo:12345:34:1",
				"-f", M0_UT_CONF_PROCESS,
				"-c", M0_UT_PATH("conf.xc")};

static char *ios_ut_bp_repeatdom_cmd[] = { "m0d", "-T", "AD",
				"-D", "cs_sdb", "-S", "cs_stob",
				"-A", "linuxstob:cs_addb_stob",
				"-w", "10",
				"-e", "lnet:0@lo:12345:34:1",
				"-e", "bulk-mem:127.0.0.1:35678",
				"-e", "bulk-mem:127.0.0.1:35679",
				"-H", "0@lo:12345:34:1",
				"-f", M0_UT_CONF_PROCESS,
				"-c", M0_UT_PATH("conf.xc")};

static char *ios_ut_bp_onerepeatdom_cmd[] = { "m0d", "-T", "AD",
				"-D", "cs_sdb", "-S", "cs_stob",
				"-A", "linuxstob:cs_addb_stob",
				"-w", "10",
				"-e", "lnet:0@lo:12345:34:1",
				"-e", "bulk-mem:127.0.0.1:35678",
				"-e", "bulk-mem:127.0.0.1:35679",
				"-H", "0@lo:12345:34:1",
				"-f", M0_UT_CONF_PROCESS,
				"-c", M0_UT_PATH("conf.xc")};

/* Transports used in motr context. */
static struct m0_net_xprt *cs_xprts[] = {
	&m0_net_lnet_xprt,
	&m0_net_bulk_mem_xprt,
#ifndef __KERNEL__
	&m0_net_sock_xprt
	/*&m0_net_libfabric_xprt*/
#endif
};

#define SERVER_LOG_FILE_NAME "cs_ut.errlog"

static int get_ioservice_buffer_pool_count(struct m0_rpc_server_ctx *sctx)
{
	struct m0_reqh_io_service *serv_obj;
	struct m0_reqh_service    *reqh_ios;
	struct m0_reqh            *reqh;

	reqh     = m0_cs_reqh_get(&sctx->rsx_motr_ctx);
	reqh_ios = m0_reqh_service_find(&m0_ios_type, reqh);
	serv_obj = container_of(reqh_ios, struct m0_reqh_io_service, rios_gen);
	M0_UT_ASSERT(serv_obj != NULL);

	return bufferpools_tlist_length(&serv_obj->rios_buffer_pools);
}

static int check_buffer_pool_per_domain(char *cs_argv[], int cs_argc, int nbp)
{
	int rc;
	int bp_count;
	struct m0_rpc_server_ctx sctx = {
		.rsx_xprts            = cs_xprts,
		.rsx_xprts_nr         = ARRAY_SIZE(cs_xprts),
		.rsx_argv             = cs_argv,
		.rsx_argc             = cs_argc,
		.rsx_log_file_name    = SERVER_LOG_FILE_NAME
	};

	rc = m0_rpc_server_start(&sctx);
	M0_UT_ASSERT(rc == 0);

	bp_count = get_ioservice_buffer_pool_count(&sctx);
	M0_UT_ASSERT(bp_count == nbp);

	m0_rpc_server_stop(&sctx);
	return rc;
}

void test_ios_bp_single_dom()
{
	/* It will create single buffer pool (per domain)*/
	check_buffer_pool_per_domain(ios_ut_bp_singledom_cmd,
				     ARRAY_SIZE(ios_ut_bp_singledom_cmd), 1);
}

void test_ios_bp_multi_dom()
{
	/* It will create two buffer pool (per domain) */
	check_buffer_pool_per_domain(ios_ut_bp_multidom_cmd,
				     ARRAY_SIZE(ios_ut_bp_multidom_cmd), 2);
}

void test_ios_bp_repeat_dom()
{
	/* It will create single buffer pool (per domain) */
	check_buffer_pool_per_domain(ios_ut_bp_repeatdom_cmd,
				     ARRAY_SIZE(ios_ut_bp_repeatdom_cmd), 2);
}
void test_ios_bp_onerepeat_dom()
{
	/* It will create two buffer pool (per domain) */
	check_buffer_pool_per_domain(ios_ut_bp_onerepeatdom_cmd,
				     ARRAY_SIZE(ios_ut_bp_onerepeatdom_cmd), 2);
}

struct m0_ut_suite ios_bufferpool_ut = {
        .ts_name = "ios-bufferpool-ut",
        .ts_init = NULL,
        .ts_fini = NULL,
        .ts_tests = {
                { "ios-bufferpool-single-domain", test_ios_bp_single_dom},
                { "ios-bufferpool-multiple-domains", test_ios_bp_multi_dom},
                { "ios-bufferpool-repeat-domains", test_ios_bp_repeat_dom},
                { "ios-bufferpool-onerepeat-domain", test_ios_bp_onerepeat_dom},
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
