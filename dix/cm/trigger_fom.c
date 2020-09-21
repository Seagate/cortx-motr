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
 * @addtogroup DIXCM
 *
 * @{
 */

#define M0_TRACE_SUBSYSTEM M0_TRACE_SUBSYS_DIXCM
#include "fop/fop.h"
#include "cm/repreb/trigger_fom.h"
#include "cm/repreb/trigger_fop.h"
#include "cm/repreb/trigger_fop_xc.h"
#include "dix/cm/cm.h"
#include "dix/cm/trigger_fop.h"
#include "lib/memory.h"
#include "lib/errno.h"
#include "lib/trace.h"


static int dix_trigger_fom_create(struct m0_fop *fop, struct m0_fom **out,
				  struct m0_reqh *reqh);
static struct m0_fop_type *dix_fop_type(uint32_t op);
static uint64_t dix_progress(struct m0_fom *fom, bool reinit_counter);
static void dix_prepare(struct m0_fom *fom);


static const struct m0_fom_trigger_ops dix_trigger_ops = {
	.fto_type     = dix_fop_type,
	.fto_progress = dix_progress,
	.fto_prepare  = dix_prepare
};

const struct m0_fom_type_ops m0_dix_trigger_fom_type_ops = {
	.fto_create = dix_trigger_fom_create,
};

static int dix_trigger_fom_create(struct m0_fop *fop, struct m0_fom **out,
				  struct m0_reqh *reqh)
{
	struct m0_trigger_fom *tfom;
	int                    rc;

	M0_ALLOC_PTR(tfom);
	if (tfom == NULL)
		return M0_ERR(-ENOMEM);
	tfom->tf_ops = &dix_trigger_ops;
	rc = m0_trigger_fom_create(tfom, fop, reqh);
	if (rc != 0) {
		m0_free(tfom);
		return M0_ERR(rc);
	}
	*out = &tfom->tf_fom;
	return 0;
}

static struct m0_fop_type *dix_fop_type(uint32_t op)
{
	struct m0_fop_type *dix_fop_type[] = {
		[M0_DIX_REPAIR_TRIGGER_OPCODE] =
			&m0_dix_repair_trigger_rep_fopt,
		[M0_DIX_REPAIR_QUIESCE_OPCODE] =
			&m0_dix_repair_quiesce_rep_fopt,
		[M0_DIX_REPAIR_STATUS_OPCODE] =
			&m0_dix_repair_status_rep_fopt,
		[M0_DIX_REBALANCE_TRIGGER_OPCODE] =
			&m0_dix_rebalance_trigger_rep_fopt,
		[M0_DIX_REBALANCE_QUIESCE_OPCODE] =
			&m0_dix_rebalance_quiesce_rep_fopt,
		[M0_DIX_REBALANCE_STATUS_OPCODE] =
			&m0_dix_rebalance_status_rep_fopt,
		[M0_DIX_REPAIR_ABORT_OPCODE] =
			&m0_dix_repair_abort_rep_fopt,
		[M0_DIX_REBALANCE_ABORT_OPCODE] =
			&m0_dix_rebalance_abort_rep_fopt,
	};
	M0_ASSERT(IS_IN_ARRAY(op, dix_fop_type));
	return dix_fop_type[op];
}

static uint64_t dix_progress(struct m0_fom *fom, bool reinit_counter)
{
	struct m0_cm     *cm   = container_of(fom->fo_service,
						struct m0_cm, cm_service);
	struct m0_dix_cm *dcm  = cm2dix(cm);
	int               progress;

	if (reinit_counter)
		dcm->dcm_recs_nr = m0_ctg_rec_nr();
	progress = dcm->dcm_recs_nr == 0 ? 100 :
			dcm->dcm_processed_nr * 100 / dcm->dcm_recs_nr;
	return progress;
}

static void dix_prepare(struct m0_fom *fom)
{
	struct m0_cm       *cm   = container_of(fom->fo_service,
						struct m0_cm, cm_service);
	struct m0_dix_cm   *dcm  = cm2dix(cm);
	struct trigger_fop *treq = m0_fop_data(fom->fo_fop);

	M0_PRE(dcm != NULL);
	M0_PRE(treq != NULL);
	dcm->dcm_op = treq->op;
}


#undef M0_TRACE_SUBSYSTEM

/** @} end of DIXCM group */

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
