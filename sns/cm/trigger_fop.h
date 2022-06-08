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

#ifndef __MOTR_SNS_CM_TRIGGER_FOP_H__
#define __MOTR_SNS_CM_TRIGGER_FOP_H__

#include "lib/types.h"
#include "xcode/xcode_attr.h"
#include "rpc/rpc_opcodes.h"

#include "cm/cm.h"

extern struct m0_fop_type m0_sns_repair_trigger_fopt;
extern struct m0_fop_type m0_sns_direct_rebalance_trigger_fopt;
extern struct m0_fop_type m0_sns_repair_quiesce_fopt;
extern struct m0_fop_type m0_sns_repair_status_fopt;
extern struct m0_fop_type m0_sns_rebalance_trigger_fopt;
extern struct m0_fop_type m0_sns_rebalance_quiesce_fopt;
extern struct m0_fop_type m0_sns_rebalance_status_fopt;
extern struct m0_fop_type m0_sns_repair_abort_fopt;
extern struct m0_fop_type m0_sns_rebalance_abort_fopt;

extern struct m0_fop_type m0_sns_repair_trigger_rep_fopt;
extern struct m0_fop_type m0_sns_direct_rebalance_trigger_rep_fopt;
extern struct m0_fop_type m0_sns_repair_quiesce_rep_fopt;
extern struct m0_fop_type m0_sns_repair_status_rep_fopt;
extern struct m0_fop_type m0_sns_rebalance_trigger_rep_fopt;
extern struct m0_fop_type m0_sns_rebalance_quiesce_rep_fopt;
extern struct m0_fop_type m0_sns_rebalance_status_rep_fopt;
extern struct m0_fop_type m0_sns_repair_abort_rep_fopt;
extern struct m0_fop_type m0_sns_rebalance_abort_rep_fopt;


M0_INTERNAL int m0_sns_cm_trigger_fop_alloc(struct m0_rpc_machine  *mach,
					    uint32_t                op,
					    struct m0_fop         **fop);
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
