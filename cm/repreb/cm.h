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

#ifndef __MOTR_CM_REPREB_CM_H__
#define __MOTR_CM_REPREB_CM_H__

/**
 * @defgroup XXX
 *
 * @{
 */

/**
 * Operation that copy machine is carrying out.
 */
enum m0_cm_op {
	CM_OP_INVALID           = 0,
	CM_OP_REPAIR,
	CM_OP_REBALANCE,
	CM_OP_REPAIR_QUIESCE,
	CM_OP_REBALANCE_QUIESCE,
	CM_OP_REPAIR_RESUME,
	CM_OP_REBALANCE_RESUME,
	CM_OP_REPAIR_STATUS,
	CM_OP_REBALANCE_STATUS,
	CM_OP_REPAIR_ABORT,
	CM_OP_REBALANCE_ABORT,
	CM_OP_DIRECT_REBALANCE
};

/**
 * Repair/re-balance copy machine status
 */
enum m0_cm_status {
	CM_STATUS_INVALID = 0,
	CM_STATUS_IDLE    = 1,
	CM_STATUS_STARTED = 2,
	CM_STATUS_FAILED  = 3,
	CM_STATUS_PAUSED  = 4,
	CM_STATUS_NR,
};

/** @} end of XXX group */
#endif /* __MOTR_CM_REPREB_CM_H__ */

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
