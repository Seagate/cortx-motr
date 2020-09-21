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

#ifndef __MOTR_SNS_CM_ST_REPAIR_CLI_H__
#define __MOTR_SNS_CM_ST_REPAIR_CLI_H__

#include "rpc/rpc.h"

enum {
	MAX_RPCS_IN_FLIGHT = 10,
	MAX_FILES_NR       = 10,
	MAX_SERVERS        = 1024
};

struct rpc_ctx {
	struct m0_rpc_conn    ctx_conn;
	struct m0_rpc_session ctx_session;
	int                   ctx_rc;
};

M0_INTERNAL int  repair_client_init(void);
M0_INTERNAL void repair_client_fini(void);
M0_INTERNAL int repair_rpc_ctx_init(struct rpc_ctx *ctx, const char *sep);
M0_INTERNAL void repair_rpc_ctx_fini(struct rpc_ctx *ctx);
M0_INTERNAL int repair_rpc_post(struct m0_fop *fop,
				struct m0_rpc_session *session,
				const struct m0_rpc_item_ops *ri_ops,
				m0_time_t  deadline);

#endif
/*
 *  Local variables:
 *  c-indentation-style: "K&R"
 *  c-basic-offset: 8
 *  tab-width: 8
 *  fill-column: 80
 *  scroll-step: 1
 *  End:
 */
