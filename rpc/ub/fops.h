/* -*- C -*- */
/*
 * Copyright (c) 2012-2020 Seagate Technology LLC and/or its Affiliates
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

#ifndef __MOTR_RPC_UB_FOPS_H__
#define __MOTR_RPC_UB_FOPS_H__

#include "xcode/xcode.h"
#include "lib/buf_xc.h"

/** RPC UB request. */
struct ub_req {
	uint64_t      uq_seqn; /**< Sequential number. */
	struct m0_buf uq_data; /**< Data buffer. */
} M0_XCA_RECORD M0_XCA_DOMAIN(rpc);

/** RPC UB response. */
struct ub_resp {
	int32_t       ur_rc;
	uint64_t      ur_seqn; /**< Sequential number. */
	struct m0_buf ur_data; /**< Data buffer. */
} M0_XCA_RECORD M0_XCA_DOMAIN(rpc);

extern struct m0_fop_type m0_rpc_ub_req_fopt;
extern struct m0_fop_type m0_rpc_ub_resp_fopt;

M0_INTERNAL void m0_rpc_ub_fops_init(void);
M0_INTERNAL void m0_rpc_ub_fops_fini(void);

#endif /* __MOTR_RPC_UB_FOPS_H__ */

/*
 *  Local variables:
 *  c-indentation-style: "K&R"
 *  c-basic-offset: 8
 *  tab-width: 8
 *  fill-column: 80
 *  scroll-step: 1
 *  End:
 */
