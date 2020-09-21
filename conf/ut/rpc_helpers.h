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

#pragma once
#ifndef __MOTR_CONF_UT_RPC_HELPERS_H__
#define __MOTR_CONF_UT_RPC_HELPERS_H__

struct m0_net_xprt;
struct m0_rpc_machine;

/** Initializes and start reqh service of passed type. */
M0_INTERNAL int m0_ut_rpc_service_start(struct m0_reqh_service **service,
				const struct m0_reqh_service_type *type);

/** Initialises net and rpc layers, performs m0_rpc_machine_init(). */
M0_INTERNAL int m0_ut_rpc_machine_start(struct m0_rpc_machine *mach,
					struct m0_net_xprt *xprt,
					const char *ep_addr);

/** Performs m0_rpc_machine_fini(), finalises rpc and net layers. */
M0_INTERNAL void m0_ut_rpc_machine_stop(struct m0_rpc_machine *mach);

#endif /* __MOTR_CONF_UT_RPC_HELPERS_H__ */
