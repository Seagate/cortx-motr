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

#ifndef __MOTR_CM_REPREB_TRIGGER_FOP_H__
#define __MOTR_CM_REPREB_TRIGGER_FOP_H__

/**
 * @defgroup CM
 *
 * @{
 */
#include "lib/types.h"
#include "xcode/xcode_attr.h"
#include "rpc/rpc_opcodes.h"  /* M0_RPC_OPCODES */

struct m0_fom_type_ops;
struct m0_cm_type;
struct m0_fop_type;
struct m0_xcode_type;

struct failure_data {
	uint32_t  fd_nr;
	uint64_t *fd_index;
} M0_XCA_SEQUENCE M0_XCA_DOMAIN(rpc);

/**
 * Simplistic implementation of repair trigger fop for testing purposes
 * only.
 */
struct trigger_fop {
	uint32_t op;
} M0_XCA_RECORD M0_XCA_DOMAIN(rpc);

struct trigger_rep_fop {
	int32_t rc;
} M0_XCA_RECORD M0_XCA_DOMAIN(rpc);

struct m0_status_rep_fop {
	int32_t  ssr_rc;
	uint32_t ssr_state;
	uint64_t ssr_progress;
} M0_XCA_RECORD M0_XCA_DOMAIN(rpc);


/** Initialises copy machine trigger FOP type. */
M0_INTERNAL void m0_cm_trigger_fop_init(struct m0_fop_type *ft,
					enum M0_RPC_OPCODES op,
					const char *name,
					const struct m0_xcode_type *xt,
					uint64_t rpc_flags,
					struct m0_cm_type *cmt,
					const struct m0_fom_type_ops *ops);

/** Finalises copy machine trigger FOP type. */
M0_INTERNAL void m0_cm_trigger_fop_fini(struct m0_fop_type *ft);

/** @} end of CM group */
#endif /* __MOTR_CM_REPREB_TRIGGER_FOP_H__ */

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
