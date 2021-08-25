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


#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>

#include "net/test/node.h"

/*
 * We are using Apache license for complete motr code but for MODULE_LICENSE
 * marker there is no provision to mention Apache for this marker. But as this
 * marker is necessary to remove the warnings, keeping this blank to make
 * compiler happy.
 */
MODULE_LICENSE();

static char	    *addr = NULL;
static char	    *addr_console = NULL;
static unsigned long timeout = 3;

module_param(addr, charp, S_IRUGO);
MODULE_PARM_DESC(addr, "endpoint address for node commands");

module_param(addr_console, charp, S_IRUGO);
MODULE_PARM_DESC(addr_console, "endpoint address for console commands");

module_param(timeout, ulong, S_IRUGO);
MODULE_PARM_DESC(timeout, "command send timeout, seconds");

static int __init m0_net_test_module_init(void)
{
	struct m0_net_test_node_cfg cfg = {
		.ntnc_addr	   = addr,
		.ntnc_addr_console = addr_console,
		.ntnc_send_timeout = M0_MKTIME(timeout, 0),
	};
	M0_THREAD_ENTER;
	return m0_net_test_node_module_initfini(&cfg);
}

static void __exit m0_net_test_module_fini(void)
{
	M0_THREAD_ENTER;
	m0_net_test_node_module_initfini(NULL);
}

module_init(m0_net_test_module_init)
module_exit(m0_net_test_module_fini)

/*
 *  Local variables:
 *  c-indentation-style: "K&R"
 *  c-basic-offset: 8
 *  tab-width: 8
 *  fill-column: 79
 *  scroll-step: 1
 *  End:
 */
