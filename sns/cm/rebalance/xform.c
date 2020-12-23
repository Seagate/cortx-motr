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


#define M0_TRACE_SUBSYSTEM M0_TRACE_SUBSYS_SNSCM
#include "lib/trace.h"

#include "fid/fid.h"
#include "reqh/reqh.h"

#include "cm/cm.h"

#include "sns/cm/ag.h"
#include "sns/cm/cp.h"

#include "sns/cm/cm_utils.h"

/**
 * @addtogroup SNSCMCP
 * @{
 */

M0_INTERNAL int m0_sns_cm_rebalance_tgt_info(struct m0_sns_cm_ag *sag,
					     struct m0_sns_cm_cp *scp);

/**
 * Transformation function for sns rebalance.
 *
 * @pre cp != NULL && m0_fom_phase(&cp->c_fom) == M0_CCP_XFORM
 * @param cp Copy packet that has to be transformed.
 */
M0_INTERNAL int m0_sns_cm_rebalance_cp_xform(struct m0_cm_cp *cp)
{
	struct m0_sns_cm_ag     *sns_ag;
	struct m0_sns_cm_cp     *scp;
	struct m0_cm_aggr_group *ag;
	struct m0_cm_ag_id       id;
	struct m0_cm            *cm;
	struct m0_cm_cp         *tgt_cp;
	enum m0_cm_cp_phase      phase;
	int                      rc;

	M0_PRE(cp != NULL && m0_fom_phase(&cp->c_fom) == M0_CCP_XFORM);

	ag = cp->c_ag;
	id = ag->cag_id;
	sns_ag = ag2snsag(ag);
	scp = cp2snscp(cp);
	cm = ag->cag_cm;
	m0_cm_ag_lock(ag);

        M0_LOG(M0_DEBUG, "xform: id ["M0_AG_F"] local_cp_nr: [%"PRIu64"]"
			 "transformed_cp_nr: [%"PRIu64"] has_incoming: %d\n",
			 M0_AG_P(&id), ag->cag_cp_local_nr,
			 ag->cag_transformed_cp_nr, !!ag->cag_has_incoming);

	/* Increment number of transformed copy packets in the accumulator. */
	M0_CNT_INC(ag->cag_transformed_cp_nr);
	if (!ag->cag_has_incoming)
		M0_ASSERT(ag->cag_transformed_cp_nr <= ag->cag_cp_local_nr);

	M0_ASSERT(m0_fid_is_set(&scp->sc_cobfid));
	rc = m0_sns_cm_rebalance_tgt_info(sns_ag, scp);
	M0_ASSERT(m0_fid_is_set(&scp->sc_cobfid));
	if (rc == 0) {
		/*
		 * Handle concurrancy of multiple copy packet foms writing
		 * to the same stob through different reqh localities by
		 * starting a duplicate copy packet fom for writing and
		 * finalising the source copy packet @cp.
		 * Concurrancy is handled by running the copy packet foms
		 * doing i/o to a same stob in the same reqh locality.
		 */
		rc = m0_sns_cm_cp_dup(cp, &tgt_cp);
		if (rc == 0) {
			if (scp->sc_is_local) {
				M0_CNT_INC(ag->cag_cp_local_nr);
				M0_CNT_INC(sns_ag->sag_cp_created_nr);
			 } else
				M0_CNT_INC(sns_ag->sag_incoming_cp_nr);

			rc = m0_cm_cp_enqueue(cm, tgt_cp);
		}
	}

	if (rc == 0 || rc == -ENOENT) {
		phase = M0_CCP_FINI;
		rc = 0;
	} else
		phase = M0_CCP_FAIL;
	m0_fom_phase_move(&cp->c_fom, rc, phase);
	rc = phase == M0_CCP_FAIL ? M0_FSO_AGAIN : M0_FSO_WAIT;
	m0_cm_ag_unlock(ag);

	return M0_RC(rc);
}

#undef M0_TRACE_SUBSYSTEM
/** @} SNSCMCP */
/*
 *  Local variables:
 *  c-indentation-style: "K&R"
 *  c-basic-offset: 8
 *  tab-width: 8
 *  fill-column: 80
 *  scroll-step: 1
 *  End:
 */
