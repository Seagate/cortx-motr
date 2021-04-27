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
 * Original author: Andriy Tkachuk <andriy.tkachuk@seagate.com>
 * Original creation date: 26-Apr-2021
 */


/**
 * @addtogroup layout
 * @{
 */

#define M0_TRACE_SUBSYSTEM M0_TRACE_SUBSYS_LAYOUT
#include "lib/trace.h"

#include "lib/tlist.h"
#include "lib/hash.h"
#include "motr/client.h"
#include "motr/client_internal.h"
#include "motr/io.h" /* m0_op_io */

#include "layout/layout.h"
#include "layout/plan.h"

M0_TL_DESCR_DEFINE(pplops, "plan plops",
		   M0_INTERNAL, struct m0_layout_plop, pl_all_link, pl_magix,
		   M0_LAYOUT_PLAN_PLOP_MAGIC, M0_LAYOUT_PPLOPS_HMAGIC);
M0_TL_DEFINE(pplops, M0_INTERNAL, struct m0_layout_plop);


/**
 * Layout access plan structure.
 * Links all the plan plops and tracks the dependecies between them.
 * Created by m0_layout_plan_build() and destroyed by m0_layout_plan_fini().
 */
struct m0_layout_plan {
	/** layout instance the plan belongs to */
	struct m0_layout_instance *lp_layout;
	/** plan plops linked via ::pl_all_link */
	struct m0_tl               lp_plops;
	/** operation the plan describes */
	struct m0_op              *lp_op;
};

M0_INTERNAL struct m0_layout_plan * m0_layout_plan_build(struct m0_op *op)
{
	int                         rc;
	struct m0_layout_plan      *plan;
	struct m0_op_common        *oc;
	struct m0_op_obj           *oo;
	struct m0_op_io            *ioo;
	struct m0_layout_instance  *linst;

	M0_ENTRY("op=%p", op);

	M0_PRE(op != NULL);
	/* XXX current limitations */
	M0_PRE(op->op_entity->en_type == M0_ET_OBJ);
	M0_PRE(M0_IN(op->op_code, (M0_OC_READ, M0_OC_WRITE)));

	oc = bob_of(op, struct m0_op_common, oc_op, &oc_bobtype);
	oo = bob_of(oc, struct m0_op_obj, oo_oc, &oo_bobtype);
	ioo = bob_of(oo, struct m0_op_io, ioo_oo, &ioo_bobtype);

	linst = oo->oo_layout_instance;
	if (linst == NULL) {
		M0_LOG(M0_ERROR, "layout instance is not initialised, "
		                 "was object opened?");
		return NULL;
	}

	M0_ALLOC_PTR(plan);
	if (plan == NULL) {
		M0_LOG(M0_ERROR, "failed to allocate memory for the plan");
		return NULL;
	}

	pplops_tlist_init(&plan->lp_plops);
	plan->lp_op = op;
	plan->lp_layout = linst;

	rc = ioo->ioo_ops->iro_iomaps_prepare(ioo) ?:
	     ioo->ioo_nwxfer.nxr_ops->nxo_distribute(&ioo->ioo_nwxfer);

	if (rc != 0) {
		m0_layout_plan_fini(plan);
		plan = NULL;
	}

	M0_LEAVE();
	return plan;
}

M0_INTERNAL void m0_layout_plan_fini(struct m0_layout_plan *plan)
{
	struct m0_layout_plop  *plop;
	struct m0_op_common    *oc;
	struct m0_op_obj       *oo;
	struct m0_op_io        *ioo;
	struct target_ioreq    *ti;

	M0_ENTRY("plan=%p", plan);

	oc = bob_of(plan->lp_op, struct m0_op_common, oc_op, &oc_bobtype);
	oo = bob_of(oc, struct m0_op_obj, oo_oc, &oo_bobtype);
	ioo = bob_of(oo, struct m0_op_io, ioo_oo, &ioo_bobtype);

	m0_htable_for(tioreqht, ti, &ioo->ioo_nwxfer.nxr_tioreqs_hash) {
		tioreqht_htable_del(&ioo->ioo_nwxfer.nxr_tioreqs_hash, ti);
		target_ioreq_fini(ti);
	} m0_htable_endfor;

	if (ioo->ioo_iomaps != NULL)
		ioo->ioo_ops->iro_iomaps_destroy(ioo);

	m0_tl_teardown(pplops, &plan->lp_plops, plop) {
		if (plop->pl_ops->po_fini)
			plop->pl_ops->po_fini(plop);
	}
	pplops_tlist_fini(&plan->lp_plops);
	plan->lp_op = NULL;
	m0_free(plan);

	M0_LEAVE();
}

M0_INTERNAL int m0_layout_plan_get(struct m0_layout_plan *plan, uint64_t colour,
				   struct m0_layout_plop **out)
{
	if (plan->lp_op == NULL)
		return M0_ERR_INFO(-EINVAL, "plan %p is not built", plan);

	return M0_RC(0);
}


#undef M0_TRACE_SUBSYSTEM

/** @} end group layout */

/*
 *  Local variables:
 *  c-indentation-style: "K&R"
 *  c-basic-offset: 8
 *  tab-width: 8
 *  fill-column: 80
 *  scroll-step: 1
 *  End:
 */
