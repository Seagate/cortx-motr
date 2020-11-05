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

#pragma once
#ifndef __MOTR_NET_MODULE_H__
#define __MOTR_NET_MODULE_H__

#include "module/module.h"  /* m0_module */
#include "net/net.h"        /* m0_net_domain */

/**
 * @addtogroup net
 *
 * @{
 */

/** Identifiers of network transports. */
enum m0_net_xprt_id {
	M0_NET_XPRT_LNET,
	M0_NET_XPRT_BULKMEM,
	M0_NET_XPRT_SOCK,
	M0_NET_XPRT_NR
};

/** Network transport module. */
struct m0_net_xprt_module {
	struct m0_module     nx_module;
	struct m0_net_xprt  *nx_xprt;
	struct m0_net_domain nx_domain;
};

/** Network module. */
struct m0_net_module {
	struct m0_module          n_module;
	struct m0_net_xprt_module n_xprts[M0_NET_XPRT_NR];
};

/** Levels of m0_net_module::n_module. */
enum {
	M0_LEVEL_NET
};

/** Levels of m0_net_xprt_module::nx_module. */
enum {
	/** m0_net_xprt_module::nx_domain has been initialised. */
	M0_LEVEL_NET_DOMAIN
};

/*
 *  m0_net_module         m0_net_xprt_module
 * +--------------+      +---------------------+
 * | M0_LEVEL_NET |<-----| M0_LEVEL_NET_DOMAIN |
 * +--------------+      +---------------------+
 */
extern const struct m0_module_type m0_net_module_type;

/** @} net */
#endif /* __MOTR_NET_MODULE_H__ */
