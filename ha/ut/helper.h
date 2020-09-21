/* -*- C -*- */
/*
 * Copyright (c) 2016-2020 Seagate Technology LLC and/or its Affiliates
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

#ifndef __MOTR_HA_UT_HELPER_H__
#define __MOTR_HA_UT_HELPER_H__

/**
 * @defgroup ha
 *
 * @{
 */

#include "net/net.h"            /* m0_net_domain */
#include "net/buffer_pool.h"    /* m0_net_buffer_pool */
#include "reqh/reqh.h"          /* m0_reqh */
#include "rpc/rpc.h"            /* m0_rpc_machine_init */
#include "rpc/conn.h"           /* m0_rpc_conn */
#include "rpc/session.h"        /* m0_rpc_session */

enum {
	M0_HA_UT_MAX_RPCS_IN_FLIGHT = 2,
};

struct m0_ha_ut_rpc_ctx {
	struct m0_net_domain      hurc_net_domain;
	struct m0_net_buffer_pool hurc_buffer_pool;
	struct m0_reqh            hurc_reqh;
	struct m0_rpc_machine     hurc_rpc_machine;
};

struct m0_ha_ut_rpc_session_ctx {
	struct m0_rpc_conn    husc_conn;
	struct m0_rpc_session husc_session;
};

M0_INTERNAL void m0_ha_ut_rpc_ctx_init(struct m0_ha_ut_rpc_ctx *ctx);
M0_INTERNAL void m0_ha_ut_rpc_ctx_fini(struct m0_ha_ut_rpc_ctx *ctx);

M0_INTERNAL void
m0_ha_ut_rpc_session_ctx_init(struct m0_ha_ut_rpc_session_ctx *sctx,
                              struct m0_ha_ut_rpc_ctx         *ctx);
M0_INTERNAL void
m0_ha_ut_rpc_session_ctx_fini(struct m0_ha_ut_rpc_session_ctx *sctx);

/** @} end of ha group */
#endif /* __MOTR_HA_UT_HELPER_H__ */

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
