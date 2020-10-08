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


#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/sched.h>
#include <linux/mm.h>

#include "motr/client.h"
#include "motr/st/st.h"
#include "motr/st/st_assert.h"

/*
 * We are using Apache license for complete motr code but for MODULE_LICENSE
 * marker there is no provision to mention Apache for this marker. But as this
 * marker is necessary to remove the warnings, keeping this blank to make
 * compiler happy.
 */


enum idx_service {
	IDX_MOTR = 1,
	IDX_CASS,
};

/* Module parameters */
static char                    *local_addr;
static char                    *ha_addr;
static char                    *prof;
static char                    *proc_fid;
static char                    *tests;
static int                      index_service;
static struct m0_config         conf;
static struct m0_idx_dix_config dix_conf;

module_param(local_addr, charp, S_IRUGO);
MODULE_PARM_DESC(local_addr, "Local Address");

module_param(ha_addr, charp, S_IRUGO);
MODULE_PARM_DESC(ha_addr, "HA Address");

module_param(prof, charp, S_IRUGO);
MODULE_PARM_DESC(prof, "Profile Opt");

module_param(proc_fid, charp, S_IRUGO);
MODULE_PARM_DESC(proc_fid, "Process FID for rmservice");

module_param(index_service, int, S_IRUGO);
MODULE_PARM_DESC(index_service, "Index service");

module_param(tests, charp, S_IRUGO);
MODULE_PARM_DESC(tests, "ST tests");

static int st_init_instance(void)
{
	int		  rc;
	struct m0_client *instance = NULL;

	conf.mc_is_oostore            = true;
	conf.mc_is_read_verify        = false;
	conf.mc_local_addr            = local_addr;
	conf.mc_ha_addr               = ha_addr;
	conf.mc_profile               = prof;
	conf.mc_process_fid           = proc_fid;
	conf.mc_tm_recv_queue_min_len = M0_NET_TM_RECV_QUEUE_DEF_LEN;
	conf.mc_max_rpc_msg_size      = M0_RPC_DEF_MAX_RPC_MSG_SIZE;

	/* TODO: ST for index APIs are disabled in kernel mode.
	 * Motr KVS need to implement a new feature demanded by MOTR-2210
	 * System tests for Index will be enabled again after that feature
	 * is implemented in KVS backend.
	 */
	conf.mc_idx_service_id = M0_IDX_DIX;
	dix_conf.kc_create_meta = false;
	conf.mc_idx_service_conf = &dix_conf;

	rc = m0_client_init(&instance, &conf, true);
	if (rc != 0)
		goto exit;

	st_set_instance(instance);

exit:
	return rc;
}

static void st_fini_instance(void)
{
	struct m0_client *instance;

	instance = st_get_instance();
	m0_client_fini(instance, true);
}

static int __init st_module_init(void)
{
	int rc;

	M0_CLIENT_THREAD_ENTER;

	/* Initilises MOTR ST. */
	st_init();
	st_add_suites();

	/*
	 * Set tests to be run. If tests == NULL, all ST will
	 * be executed.
	 */
	st_set_tests(tests);

	/* Currently, all threads share the same instance. */
	rc = st_init_instance();
	if (rc < 0) {
		printk(KERN_INFO"init failed!\n");
		return rc;
	}

	/*
	 * Start worker threads.
	 */
	st_set_nr_workers(1);
	st_cleaner_init();
	return st_start_workers();
}

static void __exit st_module_fini(void)
{
	M0_CLIENT_THREAD_ENTER;
	st_stop_workers();
	st_cleaner_fini();
	st_fini_instance();
	st_fini();
}

module_init(st_module_init)
module_exit(st_module_fini)

MODULE_LICENSE("GPL");

/*
 *  Local variables:
 *  c-indentation-style: "K&R"
 *  c-basic-offset: 8
 *  tab-width: 8
 *  fill-column: 80
 *  scroll-step: 1
 *  End:
 */
