/* -*- C -*- */
/*
 * Copyright (c) 2020 Seagate Technology LLC and/or its Affiliates
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

#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <signal.h>                 /* struct stack */
#include <assert.h>                 /* assert */

#include "lib/getopts.h"
#include "lib/thread.h"
#include "motr/client.h"
#include "motr/idx.h"
#include "motr/st/st.h"
#include "motr/st/st_misc.h"
#include "motr/st/st_assert.h"

enum idx_service {
	IDX_MOTR = 1,
	IDX_CASS,
};

/* Client parameters */
static char            *local_addr;
static char            *ha_addr;
static char            *prof;
static char            *proc_fid;
static char            *tests;
enum   idx_service      index_service;
static struct m0_config conf;

#include "motr/client_internal.h"
#include "sm/sm.h"

static struct m0_idx_dix_config  dix_conf;
static struct m0_idx_cass_config cass_conf;

static int st_init_instance(void)
{
	int               rc;
	struct m0_client *instance = NULL;

	conf.mc_is_oostore            = true;
	conf.mc_is_read_verify        = false;
	conf.mc_local_addr            = local_addr;
	conf.mc_ha_addr               = ha_addr;
	conf.mc_profile               = prof;
	conf.mc_process_fid           = proc_fid;
	conf.mc_tm_recv_queue_min_len = M0_NET_TM_RECV_QUEUE_DEF_LEN;
	conf.mc_max_rpc_msg_size      = M0_RPC_DEF_MAX_RPC_MSG_SIZE;

	if (index_service == IDX_MOTR) {
		conf.mc_idx_service_id   = M0_IDX_DIX;
		/* DIX- Metadata is created by m0dixinit() in ST script. */
		dix_conf.kc_create_meta = false;
		conf.mc_idx_service_conf = &dix_conf;
	} else if (index_service == IDX_CASS) {
		/* Cassandra index driver */
		cass_conf.cc_cluster_ep              = "127.0.0.1";
		cass_conf.cc_keyspace                = "index_keyspace";
		cass_conf.cc_max_column_family_num   = 1;
		conf.mc_idx_service_id        = M0_IDX_CASS;
		conf.mc_idx_service_conf      = &cass_conf;
	} else {
		rc = -EINVAL;
		console_printf("Invalid index service configuration.");
		goto exit;
	}

	rc = m0_client_init(&instance, &conf, true);
	if (rc != 0)
		goto exit;

	st_set_instance(instance);

exit:
	return rc;
}

static void st_fini_instance(void)
{
	m0_client_fini(st_get_instance(), true);
}

static int st_wait_workers(void)
{
	return st_stop_workers();
}

void st_usage()
{
	console_printf(
		"Client System Test Framework: m0st\n"
		"    -- Note: if -l|-u is used, no Client details are needed\n"
		"       otherwise, -m, -h and -p have to be provided\n"
		"Usage: m0st "
		"[-l|-u|-r] [-m local] [-h ha] [-p prof_opt] "
		"[-t tests]\n"
		"    -l                List all tests\n"
		"    -m local          Local(my) end point address\n"
		"    -h ha             HA address \n"
		"    -p prof_opt       Profile options for Motr\n"
		"    -f proc_fid       Process FID for rmservice@Motr\n"
		"    -t tests          Only run the specified tests\n"
		"    -I index service  Index service(Cassandra, mock, Motr-KVS)\n"
		"    -r                Run tests in a suite in random order\n"
		"    -u                Print usage\n"
		);
}

void st_get_opts(int argc, char **argv)
{
	int rc;

	if (argc < 2) {
		st_usage();
		exit(-1);
	}

	local_addr = NULL;
	ha_addr    = NULL;
	prof       = NULL;
	proc_fid   = NULL;
	tests      = NULL;

	rc = M0_GETOPTS("st", argc, argv,
			M0_HELPARG('?'),
			M0_VOIDARG('i', "more verbose help",
					LAMBDA(void, (void) {
						st_usage();
						exit(0);
					})),
			M0_VOIDARG('l', "Lists all client tests",
					LAMBDA(void, (void) {
						st_list(true);
						exit(0);
					})),
			M0_STRINGARG('m', "Local endpoint address",
					  LAMBDA(void, (const char *string) {
					       local_addr = (char*)string;
					  })),
			M0_STRINGARG('h', "HA address",
					  LAMBDA(void, (const char *str) {
					       ha_addr = (char*)str;
					  })),
			M0_STRINGARG('f', "Process FID",
					  LAMBDA(void, (const char *str) {
					       proc_fid = (char*)str;
					  })),
			M0_STRINGARG('p', "Profile options for Motr",
					  LAMBDA(void, (const char *str) {
					       prof = (char*)str;
					  })),
			M0_NUMBERARG('I', "Index service id",
					  LAMBDA(void, (int64_t service_idx) {
					       index_service = service_idx;
				          })),
			M0_VOIDARG('r', "Ramdom test mode",
				        LAMBDA(void, (void) {
					       st_set_test_mode(
							ST_RAND_MODE);
				        })),
			M0_STRINGARG('t', "Lists client tests",
					LAMBDA(void, (const char *str) {
					       tests = (char*)str;
					})));
	/* some checks */
	if (rc != 0 || local_addr == NULL || ha_addr == NULL ||
	    prof == NULL || proc_fid == NULL)
	{
		st_usage();
		exit(0);
	}

	/*
	 * Set tests to be run. If tests == NULL, all ST will
	 * be executed.
	 */
	st_set_tests(tests);
}


int main(int argc, char **argv)
{
	int           rc;
	static struct m0 instance;

	m0_instance_setup(&instance);
	rc = m0_module_init(&instance.i_self, M0_LEVEL_INST_ONCE);
	if (rc != 0) {
		fprintf(stderr, "Cannot init module %i\n", rc);
		return rc;
	}

	/* initialise Client ST */
	st_init();
	st_add_suites();

	/* Get input parameters */
	st_get_opts(argc, argv);

	/* currently, all threads share the same instance */
	if (st_init_instance() < 0) {
		fprintf(stderr, "init failed!\n");
		return -1;
	}

	/* start worker threads */
	st_set_nr_workers(1);
	st_cleaner_init();
	st_start_workers();

	/* wait till all workers complete */
	rc = st_wait_workers();
	st_cleaner_fini();

	/* clean-up */
	st_fini_instance();
	st_fini();

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
