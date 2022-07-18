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

#include "lib/memory.h"
#include "lib/errno.h"
#include "lib/misc.h"
#include "lib/arith.h"

#include "fid/fid.h"
#include "sns/parity_repair.h"

#include "cm/proxy.h"

#include "sns/cm/cm_utils.h"
#include "sns/cm/dtrebalance/ag.h"
#include "sns/cm/cp.h"
#include "sns/cm/file.h"
#include "sns/cm/cm.h"
#include "ioservice/fid_convert.h" /* m0_fid_cob_device_id */

/**
   @addtogroup SNSCMAG

   @{
 */

M0_INTERNAL void m0_sns_cm_acc_cp_init(struct m0_sns_cm_cp *scp,
				       struct m0_sns_cm_ag *ag);

M0_INTERNAL int m0_sns_cm_acc_cp_setup(struct m0_sns_cm_cp *scp,
				       struct m0_fid *tgt_cobfid,
				       uint64_t tgt_cob_index,
				       uint64_t failed_unit_idx,
				       uint64_t data_seg_nr);

M0_INTERNAL struct m0_sns_cm_dtrebalance_ag *
sag2dtrebalanceag(const struct m0_sns_cm_ag *sag)
{
	return container_of(sag, struct m0_sns_cm_dtrebalance_ag, rag_base);
}

M0_INTERNAL int64_t m0_sns_cm_dtrebalance_ag_inbufs(struct m0_sns_cm *scm,
					       struct m0_sns_cm_file_ctx *fctx,
					       const struct m0_cm_ag_id *id)
{
	uint64_t                  nr_cp_bufs;
	uint64_t                  cp_data_seg_nr;
	uint64_t                  nr_acc_bufs;
	int64_t                   nr_in_bufs;
	struct m0_pdclust_layout *pl = m0_layout_to_pdl(fctx->sf_layout);

	cp_data_seg_nr = m0_sns_cm_data_seg_nr(scm, pl);
	/*
	 * Calculate number of buffers required for a copy packet.
	 * This depends on the unit size and the max buffer size.
	 */
	nr_cp_bufs = m0_sns_cm_cp_buf_nr(&scm->sc_ibp.sb_bp, cp_data_seg_nr);
	/* Calculate number of buffers required for incoming copy packets. */
	nr_in_bufs = m0_sns_cm_incoming_reserve_bufs(scm, id);
	if (nr_in_bufs < 0)
		return nr_in_bufs;
	/* Calculate number of buffers required for accumulator copy packets. */
	nr_acc_bufs = nr_cp_bufs * m0_sns_cm_ag_unrepaired_units(scm, fctx,
							id->ai_lo.u_lo, NULL);
	return nr_in_bufs + nr_acc_bufs;
}


static void dtrebalance_ag_fini(struct m0_cm_aggr_group *ag)
{
	struct m0_sns_cm_ag        *sag;
	struct m0_sns_cm_dtrebalance_ag *rag;

	M0_ENTRY();
	M0_PRE(ag != NULL);

	M0_ASSERT(ag->cag_fini_ast.sa_next == NULL);
	sag = ag2snsag(ag);
	rag = sag2dtrebalanceag(sag);

	m0_sns_cm_ag_fini(sag);
	m0_free(rag->rag_fc);
	m0_free(rag);

	M0_LEAVE();
}

static uint32_t dtrebalance_ag_inactive_acc_nr(struct m0_cm_aggr_group *ag)
{
	struct m0_sns_cm_ag        *sag = ag2snsag(ag);
	struct m0_sns_cm_dtrebalance_ag *rag = sag2dtrebalanceag(sag);
	uint32_t                    inactive_acc_nr = 0;
	int                         i;

	for (i = 0; i < sag->sag_fnr; ++i) {
		if (rag->rag_fc[i].fc_is_inuse && !rag->rag_fc[i].fc_is_active)
			M0_CNT_INC(inactive_acc_nr);
	}

	if (inactive_acc_nr >= rag->rag_acc_freed)
		return inactive_acc_nr - rag->rag_acc_freed;
	else
		return rag->rag_acc_freed - inactive_acc_nr;
}

static bool dtrebalance_ag_can_fini(const struct m0_cm_aggr_group *ag)
{
	struct m0_sns_cm_ag        *sag = ag2snsag(ag);
	struct m0_sns_cm_dtrebalance_ag *rag = sag2dtrebalanceag(sag);

	if (ag->cag_is_frozen || ag->cag_rc != 0) {
		return ag->cag_ref == dtrebalance_ag_inactive_acc_nr(&sag->sag_base);
	}

	return (rag->rag_acc_inuse_nr == rag->rag_acc_freed) &&
	       (ag->cag_transformed_cp_nr ==
		(ag->cag_freed_cp_nr - rag->rag_acc_freed));
}

static const struct m0_cm_aggr_group_ops sns_cm_dtrebalance_ag_ops = {
	.cago_ag_can_fini       = dtrebalance_ag_can_fini,
	.cago_fini              = dtrebalance_ag_fini,
	.cago_local_cp_nr       = m0_sns_cm_ag_local_cp_nr,
	.cago_has_incoming_from = m0_sns_cm_ag_has_incoming_from,
	.cago_is_frozen_on      = m0_sns_cm_ag_is_frozen_on
};

/* static uint64_t dtrebalance_ag_target_unit(struct m0_sns_cm_ag *sag,
				      struct m0_pdclust_layout *pl,
				      struct m0_pdclust_instance *pi,
				      uint64_t fdev, uint64_t funit)
{
        struct m0_poolmach *pm;
        struct m0_fid       gfid;
        uint64_t            group;
        uint32_t            tgt_unit;
        uint32_t            tgt_unit_prev;
        int                 rc;

	pm = sag->sag_fctx->sf_pm;

        agid2fid(&sag->sag_base.cag_id, &gfid);
        group = agid2group(&sag->sag_base.cag_id);
	m0_sns_cm_fctx_lock(sag->sag_fctx);
        rc = m0_sns_repair_spare_map(pm, &gfid, pl, pi,
			group, funit, &tgt_unit, &tgt_unit_prev);
	m0_sns_cm_fctx_unlock(sag->sag_fctx);
        if (rc != 0)
                tgt_unit = ~0;

        return tgt_unit;
} */

static int dtrebalance_ag_failure_ctxs_setup(struct m0_sns_cm_dtrebalance_ag *rag,
					const struct m0_bitmap *fmap,
					struct m0_pdclust_layout *pl)
{
	struct m0_sns_cm_ag                    *sag = &rag->rag_base;
	struct m0_fid                           fid;
	uint64_t                                fidx = 0;
	int                                     rc = 0;

	M0_PRE(fmap != NULL);
	M0_PRE(fmap->b_nr == m0_sns_cm_ag_size(pl));

	agid2fid(&sag->sag_base.cag_id, &fid);

	M0_POST(fidx <= sag->sag_fnr);
	return M0_RC(rc);
}

M0_INTERNAL int m0_sns_cm_dtrebalance_ag_alloc(struct m0_cm *cm,
					  const struct m0_cm_ag_id *id,
					  bool has_incoming,
					  struct m0_cm_aggr_group **out)
{
	struct m0_sns_cm_dtrebalance_ag             *rag;
	struct m0_sns_cm_dtrebalance_ag_failure_ctx *rag_fc;
	struct m0_sns_cm_ag                    *sag = NULL;
	struct m0_pdclust_layout               *pl = NULL;
	uint64_t                                f_nr;
	int                                     i;
	int                                     rc = 0;

	M0_ENTRY("scm: %p, ag id:%p", cm, id);
	M0_PRE(cm != NULL && id != NULL && out != NULL);
	M0_PRE(m0_cm_is_locked(cm));

	/* Allocate new aggregation group. */
	M0_ALLOC_PTR(rag);
	if (rag == NULL)
		return M0_ERR(-ENOMEM);
	rc = m0_sns_cm_ag_init(&rag->rag_base, cm, id, &sns_cm_dtrebalance_ag_ops,
			       has_incoming);
	if (rc != 0) {
		m0_free(rag);
		return M0_RC(rc);
	}
	f_nr = rag->rag_base.sag_fnr;
	M0_ALLOC_ARR(rag->rag_fc, f_nr);
	if (rag->rag_fc == NULL) {
		rc =  M0_ERR(-ENOMEM);
		goto cleanup_ag;
	}
	sag = &rag->rag_base;
	pl = m0_layout_to_pdl(sag->sag_fctx->sf_layout);
	/* Set the target cob fid of accumulators for this aggregation group. */
	rc = dtrebalance_ag_failure_ctxs_setup(rag, &sag->sag_fmap, pl);
	if (rc != 0)
		goto cleanup_ag;

	/* Initialise the accumulators. */
	for (i = 0; i < sag->sag_fnr; ++i) {
		rag_fc = &rag->rag_fc[i];
		if (rag_fc->fc_is_inuse)
			m0_sns_cm_acc_cp_init(&rag_fc->fc_tgt_acc_cp, sag);
	}

	/* Acquire buffers and setup the accumulators. */
	rc = m0_sns_cm_dtrebalance_ag_setup(sag, pl);
	if (rc != 0 && rc != -ENOBUFS)
		goto cleanup_acc;
	*out = &sag->sag_base;
	goto done;

cleanup_acc:
	for (i = 0; i < sag->sag_fnr; ++i) {
		rag_fc = &rag->rag_fc[i];
		if (rag_fc->fc_is_inuse)
			m0_sns_cm_cp_buf_release(&rag_fc->fc_tgt_acc_cp.sc_base);
	}
cleanup_ag:
	M0_LOG(M0_ERROR, "cleaning up group rc: %d", rc);
	m0_cm_aggr_group_fini(&sag->sag_base);
	m0_bitmap_fini(&sag->sag_fmap);
	m0_free(rag->rag_fc);
	m0_free(rag);
done:
	M0_LEAVE("ag: %p", &sag->sag_base);
	M0_ASSERT(rc <= 0);
	return M0_RC(rc);
}

M0_INTERNAL int m0_sns_cm_dtrebalance_ag_setup(struct m0_sns_cm_ag *sag,
					  struct m0_pdclust_layout *pl)
{
	int rc = 0;

	M0_PRE(sag != NULL);

	return rc;
}

/** @} SNSCMAG */
#undef M0_TRACE_SUBSYSTEM

/*
 *  Local variables:
 *  c-indentation-style: "K&R"
 *  c-basic-offset: 8
 *  tab-width: 8
 *  fill-column: 79
 *  scroll-step: 1
 *  End:
 */
