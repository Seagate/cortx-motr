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

#ifndef __MOTR_SNS_CM_DTREBALANCE_AG_H__
#define __MOTR_SNS_CM_DTREBALANCE_AG_H__


#include "sns/cm/ag.h"
#include "pool/pool_machine.h"

/**
   @defgroup SNSCMAG SNS copy machine aggregation group
   @ingroup SNSCM

   @{
 */

/**
 * Represents a failure context corresponding to an aggregation group.
 * This is populated on creation of the aggregation group.
 */
struct m0_sns_cm_dtrebalance_ag_failure_ctx {
	/** Accumulator copy packet for this failure context. */
	struct m0_sns_cm_cp          fc_tgt_acc_cp;

	/** Index of the failed unit in aggregation group. */
	uint32_t                     fc_failed_idx;

	/**
	 * Index of target unit corresponding to the failed unit in aggregation
	 * group.
	 */
	uint32_t                     fc_tgt_idx;

	/*
	 * cob fid containing the target unit for the aggregation
	 * group.
	 */
	struct m0_fid                fc_tgt_cobfid;

	/** Target unit offset within the cob identified by tgt_cobfid. */
	uint64_t                     fc_tgt_cob_index;

	/**
	 * True, if the copy packet fom corresponding to this accumulator is
	 * in-progress.
	 */
	bool                         fc_is_active;

	/** True, if this accumulator is in use. */
	bool                         fc_is_inuse;
};

struct m0_sns_cm_dtrebalance_ag {
	/** Base aggregation group. */
	struct m0_sns_cm_ag                     rag_base;

	/**
	 * Number of accumulator copy packets finalised.
	 * This should be equal to sag_fnr.
	 */
	uint32_t                                rag_acc_freed;

	/** Number of accumulators actually in use. */
	uint32_t                                rag_acc_inuse_nr;

	/**
	 * Aggregation group failure context.
	 * Number of failure contexts are equivalent to number of failures in
	 * the aggregation group, i.e. m0_sns_cm_ag::sag_fnr.
	 */
	struct m0_sns_cm_dtrebalance_ag_failure_ctx *rag_fc;

	/** Parity math context required for incremental recovery algorithm. */
	struct m0_parity_math                   rag_math;

	/** Incremental recovery context. */
	struct m0_sns_ir                        rag_ir;
};


/**
 * Allocates and initializes aggregation group for the given m0_cm_ag_id.
 * Every sns copy machine aggregation group maintains accumulator copy packets,
 * equivalent to the number of failed units in the aggregation group. During
 * initialisation, the buffers are acquired for the accumulator copy packets
 * from the copy machine buffer pool.
 * Caller is responsible to lock the copy machine before calling this function.
 * @pre m0_cm_is_locked(cm) == true
 */
M0_INTERNAL int m0_sns_cm_dtrebalance_ag_alloc(struct m0_cm *cm,
					  const struct m0_cm_ag_id *id,
					  bool has_incoming,
					  struct m0_cm_aggr_group **out);

/*
 * Configures accumulator copy packet, acquires buffer for accumulator copy
 * packet.
 * Increments struct m0_cm_aggr_group::cag_cp_local_nr for newly created
 * accumulator copy packets, so that aggregation group is not finalised before
 * the finalisation of accumulator copy packets.
 *
 * @see m0_sns_cm_acc_cp_setup()
 */
M0_INTERNAL int m0_sns_cm_dtrebalance_ag_setup(struct m0_sns_cm_ag *ag,
					  struct m0_pdclust_layout *pl);

/**
 * Calculates number of buffers required for all the incoming copy packets.
 */
M0_INTERNAL int64_t m0_sns_cm_dtrebalance_ag_inbufs(struct m0_sns_cm *scm,
					       struct m0_sns_cm_file_ctx *fctx,
					       const struct m0_cm_ag_id *id);

M0_INTERNAL struct m0_sns_cm_dtrebalance_ag *
sag2dtrebalanceag(const struct m0_sns_cm_ag *sag);

/** @} SNSCMAG */

#endif /* __MOTR_SNS_CM_DTREBALANCE_AG_H__ */

/*
 *  Local variables:
 *  c-indentation-style: "K&R"
 *  c-basic-offset: 8
 *  tab-width: 8
 *  fill-column: 80
 *  scroll-step: 1
 *  End:
 */
