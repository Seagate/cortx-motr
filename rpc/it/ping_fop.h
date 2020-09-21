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

#ifndef __MOTR_RPC_IT_PING_FOP_H__
#define __MOTR_RPC_IT_PING_FOP_H__

#include "fop/fop.h"
#include "rpc/rpc_opcodes.h"
#include "lib/types.h"
#include "xcode/xcode_attr.h"

M0_INTERNAL void m0_ping_fop_init(void);
M0_INTERNAL void m0_ping_fop_fini(void);

/**
 * FOP definitions and corresponding fop type formats
 */
extern struct m0_fop_type m0_fop_ping_fopt;
extern struct m0_fop_type m0_fop_ping_rep_fopt;

extern const struct m0_fop_type_ops m0_fop_ping_ops;
extern const struct m0_fop_type_ops m0_fop_ping_rep_ops;

extern const struct m0_rpc_item_type m0_rpc_item_type_ping;
extern const struct m0_rpc_item_type m0_rpc_item_type_ping_rep;

struct m0_fop_ping_arr {
	uint32_t  f_count;
	uint64_t *f_data;
} M0_XCA_SEQUENCE M0_XCA_DOMAIN(rpc);

struct m0_fop_ping {
	struct m0_fop_ping_arr fp_arr;
} M0_XCA_RECORD M0_XCA_DOMAIN(rpc);

struct m0_fop_ping_rep {
	int32_t fpr_rc;
} M0_XCA_RECORD M0_XCA_DOMAIN(rpc);

/* __MOTR_RPC_IT_PING_FOP_H__ */
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
