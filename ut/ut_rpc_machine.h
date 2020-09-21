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

#ifndef __MOTR_UT_UT_RPC_MACHINE_H__
#define __MOTR_UT_UT_RPC_MACHINE_H__

#include "cob/cob.h"
#include "net/lnet/lnet.h"
#include "net/buffer_pool.h"
#include "mdstore/mdstore.h"
#include "reqh/reqh.h"
#include "rpc/rpc.h"
#include "rpc/rpc_machine.h"
#include "be/ut/helper.h"

struct m0_ut_rpc_mach_ctx {
	const char                *rmc_ep_addr;
	struct m0_rpc_machine      rmc_rpc;
	struct m0_be_ut_backend    rmc_ut_be;
	struct m0_be_ut_seg        rmc_ut_seg;
	struct m0_cob_domain_id    rmc_cob_id;
	struct m0_mdstore          rmc_mdstore;
	struct m0_cob_domain       rmc_cob_dom;
	struct m0_net_domain       rmc_net_dom;
	struct m0_net_buffer_pool  rmc_bufpool;
	struct m0_net_xprt        *rmc_xprt;
	struct m0_reqh             rmc_reqh;
};

M0_INTERNAL void m0_ut_rpc_mach_init_and_add(struct m0_ut_rpc_mach_ctx *ctx);

M0_INTERNAL void m0_ut_rpc_mach_fini(struct m0_ut_rpc_mach_ctx *ctx);

#endif /* __MOTR_UT_UT_RPC_MACHINE_H__ */


/*
 *  Local variables:
 *  c-indentation-style: "K&R"
 *  c-basic-offset: 8
 *  tab-width: 8
 *  fill-column: 80
 *  scroll-step: 1
 *  End:
 */
/*
 * vim: tabstop=8 shiftwidth=8 noexpandtab textwidth=80 nowrap
 */
