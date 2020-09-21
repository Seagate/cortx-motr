/* -*- C -*- */
/*
 * Copyright (c) 2017-2020 Seagate Technology LLC and/or its Affiliates
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

#ifndef __MOTR_DIX_CM_CP_ONWIRE_H__
#define __MOTR_DIX_CM_CP_ONWIRE_H__

#include "rpc/rpc_opcodes.h"
#include "fid/fid.h"
#include "fid/fid_xc.h"
#include "rpc/at.h"
#include "rpc/at_xc.h"
#include "cm/cp_onwire.h"
#include "cm/cp_onwire_xc.h"

struct m0_cm_type;
struct m0_fop_type;
struct m0_xcode_type;

/** DIX specific onwire copy packet structure. */
struct m0_dix_cpx {
        /** Base copy packet fields. */
        struct m0_cpx        dcx_cp;

        /** Destination catalog fid. */
        struct m0_fid        dcx_ctg_fid;

	uint32_t             dcx_ctg_op_flags;

	uint64_t             dcx_failed_idx;

	/** Copy packet fom phase before sending it onwire. */
	uint32_t             dcx_phase;

	struct m0_rpc_at_buf dcx_ab_key;
	struct m0_rpc_at_buf dcx_ab_val;
} M0_XCA_RECORD M0_XCA_DOMAIN(rpc);


/** Initialises copy packet FOP type. */
M0_INTERNAL void m0_dix_cpx_init(struct m0_fop_type *ft,
				 const struct m0_fom_type_ops *fomt_ops,
				 enum M0_RPC_OPCODES op,
				 const char *name,
				 const struct m0_xcode_type *xt,
				 uint64_t rpc_flags, struct m0_cm_type *cmt);

/** Finalises copy packet FOP type. */
M0_INTERNAL void m0_dix_cpx_fini(struct m0_fop_type *ft);

#endif /* __MOTR_DIX_CM_CP_ONWIRE_H__ */
/*
 *  Local variables:
 *  c-indentation-style: "K&R"
 *  c-basic-offset: 8
 *  tab-width: 8
 *  fill-column: 80
 *  scroll-step: 1
 *  End:
 */
