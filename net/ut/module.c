/*
 * Copyright (c) 2014-2020 Seagate Technology LLC and/or its Affiliates
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


#include "net/module.h"
#include "module/instance.h"  /* m0_get */
#include "lib/memory.h"       /* m0_free0 */
#include "ut/ut.h"

static void test_net_modules(void)
{
	struct m0        *inst = m0_get();
	struct m0_module *net;
	struct m0_module *xprt;
	int               rc;
	M0_UT_ASSERT(inst->i_moddata[M0_MODULE_NET] == NULL);
	net = m0_net_module_type.mt_create(inst);
	M0_UT_ASSERT(net != NULL);
	M0_UT_ASSERT(inst->i_moddata[M0_MODULE_NET] ==
		     container_of(net, struct m0_net_module, n_module));

	xprt = &((struct m0_net_module *)inst->i_moddata[M0_MODULE_NET])
		->n_xprts[M0_NET_XPRT_BULKMEM].nx_module;
	M0_UT_ASSERT(xprt->m_cur == M0_MODLEV_NONE);
	M0_UT_ASSERT(net->m_cur == M0_MODLEV_NONE);

	rc = m0_module_init(xprt, M0_LEVEL_NET_DOMAIN);
	M0_UT_ASSERT(rc == 0);
	M0_UT_ASSERT(xprt->m_cur == M0_LEVEL_NET_DOMAIN);
	M0_UT_ASSERT(net->m_cur == M0_LEVEL_NET);

	m0_module_fini(xprt, M0_MODLEV_NONE);
	M0_UT_ASSERT(xprt->m_cur == M0_MODLEV_NONE);
	M0_UT_ASSERT(net->m_cur == M0_MODLEV_NONE);

	m0_free0(&inst->i_moddata[M0_MODULE_NET]);
}
struct m0_ut_suite m0_net_module_ut = {
	.ts_name  = "net-module",
	.ts_tests = {
		{ "test", test_net_modules },
		{ NULL, NULL }
	}
};
