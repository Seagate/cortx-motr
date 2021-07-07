/* -*- C -*- */
/*
 * Copyright (c) 2021 Seagate Technology LLC and/or its Affiliates
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
		   M0_INTERNAL, struct m0_layout_plop, pl_linkage, pl_magix,
		   M0_LAYOUT_PLAN_PLOP_MAGIC, M0_LAYOUT_PPLOPS_HMAGIC);
M0_TL_DEFINE(pplops, M0_INTERNAL, struct m0_layout_plop);

M0_TL_DESCR_DEFINE(pldeps, "plop deps", M0_INTERNAL,
		   struct m0_layout_plop_rel, plr_dep_linkage, plr_magix,
		   M0_LAYOUT_PLAN_PLOPR_MAGIC, M0_LAYOUT_PPLD_HMAGIC);
M0_TL_DEFINE(pldeps, M0_INTERNAL, struct m0_layout_plop_rel);

M0_TL_DESCR_DEFINE(plrdeps, "plop rdeps", M0_INTERNAL,
		   struct m0_layout_plop_rel, plr_rdep_linkage, plr_magix,
		   M0_LAYOUT_PLAN_PLOPR_MAGIC, M0_LAYOUT_PPLRD_HMAGIC);
M0_TL_DEFINE(plrdeps, M0_INTERNAL, struct m0_layout_plop_rel);

/**
 * Layout access plan structure.
 * Links all the plan plops and tracks the dependecies between them.
 * Created by m0_layout_plan_build() and destroyed by m0_layout_plan_fini().
 */
struct m0_layout_plan {
	/** Layout instance the plan belongs to. */
	struct m0_layout_instance *lp_layout;
	/** Plan plops linked via ::pl_linkage. */
	struct m0_tl               lp_plops;
	/** Last returned plop via m0_layout_plan_get(). */
	struct m0_layout_plop     *lp_last_plop;
	/** Operation the plan describes. */
	struct m0_op              *lp_op;
	/** Lock for protecting concurrent plan_get() calls. */
	struct m0_mutex            lp_lock;
};

static struct m0_layout_plop *
plop_alloc_init(struct m0_layout_plan *plan, enum m0_layout_plop_type type,
		struct target_ioreq *ti)
{
	struct m0_layout_plop    *plop;
	struct m0_layout_io_plop *iopl;

	M0_PRE(m0_mutex_is_locked(&plan->lp_lock));

	if (M0_IN(type, (M0_LAT_READ, M0_LAT_WRITE))) {
		M0_ALLOC_PTR(iopl);
		plop = iopl == NULL ? NULL : &iopl->iop_base;
	} else
		M0_ALLOC_PTR(plop);
	if (plop == NULL)
		return NULL;

	pplops_tlink_init_at(plop, &plan->lp_plops);
	plop->pl_ti = ti;
	plop->pl_type = type;
	plop->pl_plan = plan;
	plop->pl_state = M0_LPS_INIT;
	pldeps_tlist_init(&plop->pl_deps);
	plrdeps_tlist_init(&plop->pl_rdeps);

	return plop;
}

static int add_plops_relation(struct m0_layout_plop *rdep,
			      struct m0_layout_plop *dep)
{
	struct m0_layout_plop_rel *plrel;

	M0_ALLOC_PTR(plrel);
	if (plrel == NULL)
		return M0_ERR(-ENOMEM);

	plrel->plr_dep = dep;
	plrel->plr_rdep = rdep;
	pldeps_tlink_init_at_tail(plrel, &rdep->pl_deps);
	plrdeps_tlink_init_at_tail(plrel, &dep->pl_rdeps);

	return 0;
}

static void del_plop_relations(struct m0_layout_plop *plop)
{
	struct m0_layout_plop_rel *rel;

	m0_tl_teardown(pldeps, &plop->pl_deps, rel) {
		plrdeps_tlink_del_fini(rel);
		m0_free(rel);
	}
	m0_tl_teardown(plrdeps, &plop->pl_rdeps, rel) {
		pldeps_tlink_del_fini(rel);
		m0_free(rel);
	}
	pldeps_tlist_fini(&plop->pl_deps);
	plrdeps_tlist_fini(&plop->pl_rdeps);
}

M0_INTERNAL struct m0_layout_plan * m0_layout_plan_build(struct m0_op *op)
{
	int                         rc;
	struct m0_layout_plan      *plan;
	struct m0_layout_plop      *plop;
	struct m0_layout_plop      *plop_out;
	struct m0_layout_plop      *plop_done;
	struct m0_layout_io_plop   *iopl;
	struct m0_op_common        *oc;
	struct m0_op_obj           *oo;
	struct m0_op_io            *ioo;
	struct m0_layout_instance  *linst;
	struct target_ioreq        *ti;

	M0_ENTRY("op=%p", op);

	M0_PRE(op != NULL);
	/* XXX current limitations */
	M0_PRE(op->op_entity->en_type == M0_ET_OBJ);
	M0_PRE(M0_IN(op->op_code, (M0_OC_READ, M0_OC_WRITE)));

	oc = bob_of(op, struct m0_op_common, oc_op, &oc_bobtype);
	oo = bob_of(oc, struct m0_op_obj, oo_oc, &oo_bobtype);
	ioo = bob_of(oo, struct m0_op_io, ioo_oo, &ioo_bobtype);

	linst = oo->oo_layout_instance;
	M0_ASSERT_INFO(linst != NULL, "layout instance is not initialised, "
	                              "was object opened?");
	M0_ALLOC_PTR(plan);
	if (plan == NULL) {
		M0_LOG(M0_ERROR, "failed to allocate memory for the plan");
		return NULL;
	}

	m0_mutex_init(&plan->lp_lock);

	pplops_tlist_init(&plan->lp_plops);
	plan->lp_op = op;
	plan->lp_layout = linst;

	rc = ioo->ioo_ops->iro_iomaps_prepare(ioo) ?:
	     ioo->ioo_nwxfer.nxr_ops->nxo_distribute(&ioo->ioo_nwxfer);
	if (rc != 0)
		goto out;

	/*
	 * There is no concurrency at this stage yet, but we take
	 * the lock here for the same of check at plop_alloc_init().
	 */
	m0_mutex_lock(&plan->lp_lock);

	plop_done = plop_alloc_init(plan, M0_LAT_DONE, NULL);
	if (plop_done == NULL) {
		rc = M0_ERR(-ENOMEM);
		goto out;
	}

	m0_htable_for(tioreqht, ti, &ioo->ioo_nwxfer.nxr_tioreqs_hash) {
		/*
		 * ti reqs go in reverse order (by ti_goff) in this loop,
		 * so we need to add OUT_READ plop 1st to the list.
		 */
		plop_out = plop_alloc_init(plan, M0_LAT_OUT_READ, NULL);
		rc = add_plops_relation(plop_done, plop_out);
		if (rc != 0)
			break;

		plop = plop_alloc_init(plan, M0_LAT_READ, ti);
		if (plop == NULL || plop_out == NULL) {
			rc = M0_ERR(-ENOMEM);
			break;
		}
		plop->pl_ent = ti->ti_fid;
		iopl = container_of(plop, struct m0_layout_io_plop, iop_base);
		iopl->iop_ext  = ti->ti_ivec;
		iopl->iop_data = ti->ti_bufvec;
		iopl->iop_session = ti->ti_session;
		iopl->iop_goff = ti->ti_goff;
		rc = add_plops_relation(plop_out, plop);
		if (rc != 0)
			break;
	} m0_htable_endfor;

	m0_mutex_unlock(&plan->lp_lock);

 out:
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

	/*
	 * There should not be any concurrency by this stage already.
	 * But if there is any (which is a bug) - we want it to be
	 * caught properly with the assert below which checks pl_state.
	 * So we want this check to be atomic with any possible plop
	 * changes going in parallel (which is, again, a logical bug).
	 */
	m0_mutex_lock(&plan->lp_lock);

	m0_htable_for(tioreqht, ti, &ioo->ioo_nwxfer.nxr_tioreqs_hash) {
		tioreqht_htable_del(&ioo->ioo_nwxfer.nxr_tioreqs_hash, ti);
		target_ioreq_fini(ti);
	} m0_htable_endfor;

	if (ioo->ioo_iomaps != NULL)
		ioo->ioo_ops->iro_iomaps_destroy(ioo);

	m0_tl_teardown(pplops, &plan->lp_plops, plop) {
		if (plop->pl_ops && plop->pl_ops->po_fini)
			plop->pl_ops->po_fini(plop);
		/* For each plan_get(), plop_done() must be called. */
		M0_ASSERT(M0_IN(plop->pl_state, (M0_LPS_INIT, M0_LPS_DONE)));
		del_plop_relations(plop);
		m0_free(plop);
	}
	pplops_tlist_fini(&plan->lp_plops);

	m0_mutex_unlock(&plan->lp_lock);
	m0_mutex_fini(&plan->lp_lock);

	m0_free(plan);

	M0_LEAVE();
}

M0_INTERNAL int m0_layout_plan_get(struct m0_layout_plan *plan, uint64_t colour,
				   struct m0_layout_plop **plop)
{
	M0_PRE(plan != NULL);
	M0_PRE(plop != NULL);
	M0_PRE(plan->lp_op != NULL);

	m0_mutex_lock(&plan->lp_lock);

	if (plan->lp_last_plop == NULL)
		*plop = pplops_tlist_head(&plan->lp_plops);
	else
		*plop = pplops_tlist_next(&plan->lp_plops, plan->lp_last_plop);

	plan->lp_last_plop = *plop;

	m0_mutex_unlock(&plan->lp_lock);

	return M0_RC(0);
}

M0_INTERNAL int m0_layout_plop_start(struct m0_layout_plop *plop)
{
	M0_PRE(plop->pl_state == M0_LPS_INIT);
	/* All the dependencies for this plop must be done. */
	M0_PRE(m0_tl_forall(pldeps, rel, &plop->pl_deps,
			    rel->plr_dep->pl_state == M0_LPS_DONE));

	plop->pl_state = M0_LPS_STARTED;

	return M0_RC(0);
}

M0_INTERNAL void m0_layout_plop_done(struct m0_layout_plop *plop)
{
	M0_PRE(plop->pl_state == M0_LPS_STARTED);
	plop->pl_state = M0_LPS_DONE;
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
