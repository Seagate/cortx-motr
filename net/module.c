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
#include "module/instance.h"
#include "net/lnet/lnet.h"    /* m0_net_lnet_xprt */
#include "net/sock/sock.h"
#include "net/net.h"
#include "net/bulk_mem.h"     /* m0_net_bulk_mem_xprt */
#include "lib/memory.h"       /* M0_ALLOC_PTR */

static struct m0_module *net_module_create(struct m0 *instance);
static int  level_net_enter(struct m0_module *module);
static void level_net_leave(struct m0_module *module);
static int  level_net_xprt_enter(struct m0_module *module);
static void level_net_xprt_leave(struct m0_module *module);

const struct m0_module_type m0_net_module_type = {
	.mt_name   = "m0_net_module",
	.mt_create = net_module_create
};

static const struct m0_modlev levels_net[] = {
	[M0_LEVEL_NET] = {
		.ml_name  = "M0_LEVEL_NET",
		.ml_enter = level_net_enter,
		.ml_leave = level_net_leave
	}
};

static const struct m0_modlev levels_net_xprt[] = {
	[M0_LEVEL_NET_DOMAIN] = {
		.ml_name  = "M0_LEVEL_NET_DOMAIN",
		.ml_enter = level_net_xprt_enter,
		.ml_leave = level_net_xprt_leave
	}
};

static struct {
	const char         *name;
	struct m0_net_xprt *xprt;
} net_xprt_mods[] = {
	[M0_NET_XPRT_LNET] = {
		.name = "\"lnet\" m0_net_xprt_module",
		.xprt = &m0_net_lnet_xprt
	},
	[M0_NET_XPRT_BULKMEM] = {
		.name = "\"bulk-mem\" m0_net_xprt_module",
		.xprt = &m0_net_bulk_mem_xprt
	},
	[M0_NET_XPRT_SOCK] = {
		.name = "\"sock\" m0_net_xprt_module",
		.xprt = &m0_net_sock_xprt
	}
};
M0_BASSERT(ARRAY_SIZE(net_xprt_mods) ==
	   ARRAY_SIZE(M0_FIELD_VALUE(struct m0_net_module, n_xprts)));

static struct m0_module *net_module_create(struct m0 *instance)
{
	struct m0_net_module *net;
	struct m0_module     *m;
	unsigned              i;

	M0_ALLOC_PTR(net);
	if (net == NULL)
		return NULL;
	m0_module_setup(&net->n_module, m0_net_module_type.mt_name,
			levels_net, ARRAY_SIZE(levels_net), instance);
	for (i = 0; i < ARRAY_SIZE(net->n_xprts); ++i) {
		m = &net->n_xprts[i].nx_module;
		m0_module_setup(m, net_xprt_mods[i].name, levels_net_xprt,
				ARRAY_SIZE(levels_net_xprt), instance);
		m0_module_dep_add(m, M0_LEVEL_NET_DOMAIN,
				  &net->n_module, M0_LEVEL_NET);
	}
	instance->i_moddata[M0_MODULE_NET] = net;
	return &net->n_module;
}

static int level_net_enter(struct m0_module *module)
{
	struct m0_net_module *m = M0_AMB(m, module, n_module);
	int                   i;

	M0_PRE(module->m_cur + 1 == M0_LEVEL_NET);
	/*
	 * We could have introduced a dedicated level for assigning
	 * m0_net_xprt_module::nx_xprt pointers, but assigning them
	 * this way is good enough.
	 */
	for (i = 0; i < ARRAY_SIZE(net_xprt_mods); ++i)
		m->n_xprts[i].nx_xprt = net_xprt_mods[i].xprt;
#if 0 /* XXX TODO
       * Rename current m0_net_init() to m0_net__init(), exclude it
       * from subsystem[] of motr/init.c, and ENABLEME.
       */
	return m0_net__init();
#else
	return 0;
#endif
}

static void level_net_leave(struct m0_module *module)
{
	M0_PRE(module->m_cur == M0_LEVEL_NET);
#if 0 /* XXX TODO
       * Rename current m0_net_fini() to m0_net__fini(), exclude it
       * from subsystem[] of motr/init.c, and ENABLEME.
       */
	m0_net__fini();
#endif
}

static int level_net_xprt_enter(struct m0_module *module)
{
	struct m0_net_xprt_module *m = M0_AMB(m, module, nx_module);

	M0_PRE(module->m_cur + 1 == M0_LEVEL_NET_DOMAIN);
	return m0_net_domain_init(&m->nx_domain, m->nx_xprt);
}

static void level_net_xprt_leave(struct m0_module *module)
{
	M0_PRE(module->m_cur == M0_LEVEL_NET_DOMAIN);
	m0_net_domain_fini(&container_of(module, struct m0_net_xprt_module,
					 nx_module)->nx_domain);
}
