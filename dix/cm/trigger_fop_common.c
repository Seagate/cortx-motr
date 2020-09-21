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


/**
 * @addtogroup dixcm
 *
 * @{
 */

#define M0_TRACE_SUBSYSTEM M0_TRACE_SUBSYS_DIXCM
#include "lib/trace.h"
#include "fop/fop.h"
#include "fop/fop_item_type.h"
#include "cm/repreb/cm.h"

struct m0_fop_type m0_dix_rebalance_trigger_fopt;
struct m0_fop_type m0_dix_rebalance_trigger_rep_fopt;
struct m0_fop_type m0_dix_rebalance_quiesce_fopt;
struct m0_fop_type m0_dix_rebalance_quiesce_rep_fopt;
struct m0_fop_type m0_dix_rebalance_status_fopt;
struct m0_fop_type m0_dix_rebalance_status_rep_fopt;
struct m0_fop_type m0_dix_rebalance_abort_fopt;
struct m0_fop_type m0_dix_rebalance_abort_rep_fopt;

struct m0_fop_type m0_dix_repair_trigger_fopt;
struct m0_fop_type m0_dix_repair_quiesce_fopt;
struct m0_fop_type m0_dix_repair_status_fopt;
struct m0_fop_type m0_dix_repair_abort_fopt;
struct m0_fop_type m0_dix_repair_trigger_rep_fopt;
struct m0_fop_type m0_dix_repair_quiesce_rep_fopt;
struct m0_fop_type m0_dix_repair_status_rep_fopt;
struct m0_fop_type m0_dix_repair_abort_rep_fopt;

M0_INTERNAL int m0_dix_cm_trigger_fop_alloc(struct m0_rpc_machine  *mach,
					    uint32_t                op,
					    struct m0_fop         **fop)
{
	static struct m0_fop_type *dix_fop_type[] = {
		[CM_OP_REPAIR]           = &m0_dix_repair_trigger_fopt,
		[CM_OP_REPAIR_QUIESCE]   = &m0_dix_repair_quiesce_fopt,
		[CM_OP_REBALANCE]        = &m0_dix_rebalance_trigger_fopt,
		[CM_OP_REBALANCE_QUIESCE]= &m0_dix_rebalance_quiesce_fopt,
		[CM_OP_REPAIR_RESUME]    = &m0_dix_repair_trigger_fopt,
		[CM_OP_REBALANCE_RESUME] = &m0_dix_rebalance_trigger_fopt,
		[CM_OP_REPAIR_STATUS]    = &m0_dix_repair_status_fopt,
		[CM_OP_REBALANCE_STATUS] = &m0_dix_rebalance_status_fopt,
		[CM_OP_REPAIR_ABORT]     = &m0_dix_repair_abort_fopt,
		[CM_OP_REBALANCE_ABORT]  = &m0_dix_rebalance_abort_fopt,
	};
	M0_ENTRY();
	M0_PRE(IS_IN_ARRAY(op, dix_fop_type));

	*fop = m0_fop_alloc(dix_fop_type[op], NULL, mach);
	return *fop == NULL ? M0_ERR(-ENOMEM) : M0_RC(0);
}


#undef M0_TRACE_SUBSYSTEM

/** @} end of dixcm group */

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
