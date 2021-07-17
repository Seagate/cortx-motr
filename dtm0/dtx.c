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
 */

/**
 * @addtogroup dtm
 *
 * @{
 */

#define M0_TRACE_SUBSYSTEM M0_TRACE_SUBSYS_DTM0
#include "dtm0/dtx.h"
#include "lib/assert.h"   /* M0_PRE */
#include "lib/memory.h"   /* M0_ALLOC */
#include "lib/errno.h"    /* ENOMEM */
#include "lib/trace.h"    /* M0_ERR */
#include "be/dtm0_log.h"  /* dtm0_log API */
#include "conf/helpers.h" /* proc2srv */
#include "reqh/reqh.h"    /* reqh2confc */
#include "rpc/rpc_machine.h" /* rpc_machine */
#include "dtm0/fop.h"     /* dtm0 req fop */
#include "dtm0/service.h" /* m0_dtm0_service */

static struct m0_sm_state_descr dtx_states[] = {
	[M0_DDS_INIT] = {
		.sd_flags     = M0_SDF_INITIAL | M0_SDF_FINAL,
		.sd_name      = "init",
		.sd_allowed   = M0_BITS(M0_DDS_INPROGRESS),
	},
	[M0_DDS_INPROGRESS] = {
		.sd_name      = "inprogress",
		.sd_allowed   = M0_BITS(M0_DDS_EXECUTED, M0_DDS_FAILED),
	},
	[M0_DDS_EXECUTED] = {
		.sd_name      = "executed",
		.sd_allowed   = M0_BITS(M0_DDS_EXECUTED_ALL),
	},
	[M0_DDS_EXECUTED_ALL] = {
		.sd_name      = "executed-all",
		.sd_allowed   = M0_BITS(M0_DDS_STABLE),
	},
	[M0_DDS_STABLE] = {
		.sd_name      = "stable",
		.sd_allowed   = M0_BITS(M0_DDS_DONE),
	},
	[M0_DDS_DONE] = {
		.sd_name      = "done",
		.sd_flags     = M0_SDF_TERMINAL,
	},
	[M0_DDS_FAILED] = {
		.sd_name      = "failed",
		.sd_flags     = M0_SDF_TERMINAL | M0_SDF_FAILURE
	}
};

static struct m0_sm_trans_descr dtx_trans[] = {
	{ "populated",  M0_DDS_INIT,         M0_DDS_INPROGRESS   },
	{ "executed",   M0_DDS_INPROGRESS,   M0_DDS_EXECUTED     },
	{ "exec-all",   M0_DDS_EXECUTED,     M0_DDS_EXECUTED_ALL },
	{ "exec-fail",  M0_DDS_INPROGRESS,   M0_DDS_FAILED       },
	{ "stable",     M0_DDS_EXECUTED_ALL, M0_DDS_STABLE       },
	{ "prune",      M0_DDS_STABLE,       M0_DDS_DONE         }
};

struct m0_sm_conf m0_dtx_sm_conf = {
	.scf_name      = "dtm0dtx",
	.scf_nr_states = ARRAY_SIZE(dtx_states),
	.scf_state     = dtx_states,
	.scf_trans_nr  = ARRAY_SIZE(dtx_trans),
	.scf_trans     = dtx_trans,
};

M0_INTERNAL void m0_dtm0_dtx_domain_init(void)
{
	M0_ENTRY();
	if (!m0_sm_conf_is_initialized(&m0_dtx_sm_conf))
		m0_sm_conf_init(&m0_dtx_sm_conf);
	M0_LEAVE();
}

M0_INTERNAL void m0_dtm0_dtx_domain_fini(void)
{
	M0_ENTRY();
	if (m0_sm_conf_is_initialized(&m0_dtx_sm_conf))
		m0_sm_conf_fini(&m0_dtx_sm_conf);
	M0_LEAVE();
}

static int dtx_log_insert(struct m0_dtm0_dtx *dtx)
{
	struct m0_be_dtm0_log  *log;
	struct m0_dtm0_log_rec *record = M0_AMB(record, dtx, dlr_dtx);
	int                     rc;

	M0_PRE(m0_dtm0_tx_desc_state_eq(&dtx->dd_txd, M0_DTPS_INPROGRESS));
	M0_PRE(dtx->dd_dtms != NULL);
	log = dtx->dd_dtms->dos_log;
	M0_PRE(log != NULL);

	m0_mutex_lock(&log->dl_lock);
	rc = m0_be_dtm0_volatile_log_insert(log, record);
	m0_mutex_unlock(&log->dl_lock);
	return rc;
}

static void dtx_log_update(struct m0_dtm0_dtx *dtx)
{
	struct m0_be_dtm0_log  *log;
	struct m0_dtm0_log_rec *record = M0_AMB(record, dtx, dlr_dtx);

	M0_PRE(dtx->dd_dtms != NULL);
	log = dtx->dd_dtms->dos_log;
	M0_PRE(log != NULL);

	m0_mutex_lock(&log->dl_lock);
	m0_be_dtm0_volatile_log_update(log, record);
	m0_mutex_unlock(&log->dl_lock);
}

static void dtx_init(struct m0_dtm0_dtx     *dtx,
		     struct m0_dtm0_service *svc,
		     struct m0_sm_group     *grp)
{
	dtx->dd_dtms = svc;
	dtx->dd_ancient_dtx.tx_dtx = dtx;
	m0_sm_init(&dtx->dd_sm, &m0_dtx_sm_conf, M0_DDS_INIT, grp);
	m0_sm_addb2_counter_init(&dtx->dd_sm);
}

static struct m0_dtm0_dtx *dtx_alloc(struct m0_dtm0_service *svc,
				     struct m0_sm_group     *grp)
{
	struct m0_dtm0_log_rec *rec;

	M0_PRE(svc != NULL);
	M0_PRE(grp != NULL);

	M0_ALLOC_PTR(rec);
	if (rec == NULL)
		return NULL;

	dtx_init(&rec->dlr_dtx, svc, grp);
	return &rec->dlr_dtx;
}

static void dtx_fini(struct m0_dtm0_dtx *dtx)
{
	struct m0_dtm0_log_rec *rec;
	M0_PRE(dtx != NULL);
	rec = M0_AMB(rec, dtx, dlr_dtx);
	M0_ASSERT_INFO(ergo(dtx->dd_sm.sm_state >= M0_DDS_INPROGRESS,
			    rec->dlr_magic != 0),
		       "A DTX in INPROGRESS+ state must be a part of the log.");
	m0_sm_fini(&dtx->dd_sm);
	m0_dtm0_tx_desc_fini(&dtx->dd_txd);
	M0_SET0(dtx);
}

static void dtx_prepare(struct m0_dtm0_dtx *dtx)
{
	M0_ENTRY("dtx=%p", dtx);

	M0_PRE(dtx != NULL);
	m0_dtm0_clk_src_now(&dtx->dd_dtms->dos_clk_src,
			    &dtx->dd_txd.dtd_id.dti_ts);

	dtx->dd_txd.dtd_id.dti_fid = dtx->dd_dtms->dos_generic.rs_service_fid;
	M0_POST(m0_dtm0_tid__invariant(&dtx->dd_txd.dtd_id));
	M0_LEAVE("prepared dtx with tid "DTID0_F, DTID0_P(&dtx->dd_txd.dtd_id));
}

static int dtx_open(struct m0_dtm0_dtx  *dtx,
		    uint32_t             nr_pa)
{
	M0_PRE(dtx != NULL);
	M0_ENTRY("dtx=%p", dtx);
	return M0_RC(m0_dtm0_tx_desc_init(&dtx->dd_txd, nr_pa));
}

static void dtx_fop_assign(struct m0_dtm0_dtx  *dtx,
			   uint32_t             pa_idx,
			   const struct m0_fop *pa_fop)
{
	M0_PRE(dtx != NULL);
	M0_PRE(pa_idx < dtx->dd_txd.dtd_ps.dtp_nr);

	M0_ENTRY("dtx=%p, pa=%" PRIu32 ", fop=%p", dtx, pa_idx, pa_fop);

	(void) pa_idx;

	/*
	 * TODO:REDO: On the DTM side we should enforce the requirement
	 * described at m0_dtm0_dtx::dd_op.
	 * At this moment we silently ignore this as well as anything
	 * related directly to REDO use-cases.
	 */
	if (dtx->dd_fop == NULL) {
		dtx->dd_fop = pa_fop;
	}

	M0_LEAVE();
}


static int dtx_fid_assign(struct m0_dtm0_dtx  *dtx,
			  uint32_t             pa_idx,
			  const struct m0_fid *pa_sfid)
{
	struct m0_dtm0_tx_pa   *pa;
	struct m0_reqh         *reqh;
	struct m0_conf_cache   *cache;
	struct m0_conf_obj     *obj;
	struct m0_conf_process *proc;
	struct m0_fid           rdtms_fid;
	int                     rc;

	M0_ENTRY("dtx=%p", dtx);

	M0_PRE(dtx != NULL);
	M0_PRE(pa_idx < dtx->dd_txd.dtd_ps.dtp_nr);
	M0_PRE(m0_fid_is_valid(pa_sfid));

	/* XXX: Should we lock or release any conf objects or the cache? */

	reqh = dtx->dd_dtms->dos_generic.rs_reqh;
	cache = &m0_reqh2confc(reqh)->cc_cache;

	obj = m0_conf_cache_lookup(cache, pa_sfid);
	if (obj == NULL)
		return M0_ERR_INFO(-ENOENT,
				   "User service is not in the conf cache.");

	obj = m0_conf_obj_grandparent(obj);
	if (obj == NULL)
		return M0_ERR_INFO(-ENOENT, "Process the service belongs to "
		       "is not a part of the conf cache.");

	proc = M0_CONF_CAST(obj, m0_conf_process);
	M0_ASSERT_INFO(proc != NULL, "The grandparent is not a process?");

	rc = m0_conf_process2service_get(&reqh->rh_rconfc.rc_confc,
					 &proc->pc_obj.co_id, M0_CST_DTM0,
					 &rdtms_fid);
	if (rc != 0)
		return M0_ERR_INFO(rc, "Cannot find remote DTM service on"
				   " the remote process that runs"
				   " this user service.");

	pa = &dtx->dd_txd.dtd_ps.dtp_pa[pa_idx];
	M0_ASSERT_INFO(M0_IS0(pa), "FID cannot be re-assigned.");
	M0_ASSERT(m0_fid_is_valid(&rdtms_fid));

	pa->p_fid = rdtms_fid;
	M0_ASSERT(pa->p_state == M0_DTPS_INIT);
	pa->p_state = M0_DTPS_INPROGRESS;

	return M0_RC_INFO(0, "pa %" PRIu32 ": "
			  FID_F " (User svc) => " FID_F " (DTM svc) ",
			  pa_idx, FID_P(pa_sfid), FID_P(&rdtms_fid));
}

static int dtx_close(struct m0_dtm0_dtx *dtx)
{
	int rc;

	M0_ENTRY("dtx=%p", dtx);

	M0_PRE(dtx != NULL);
	M0_PRE(m0_sm_group_is_locked(dtx->dd_sm.sm_grp));

	/*
	 * TODO:REDO: We may want to capture the fop contents here.
	 * See ::fol_record_pack and ::m0_fop_encdec for the details.
	 * At this moment we do not do REDO, so that it is safe to
	 * avoid any actions on the fop here.
	 */

	rc = dtx_log_insert(dtx);
	M0_ASSERT(rc == 0);

	/*
	 * Once a dtx is closed, the FOP (or FOPs) has to be serialized
	 * into the log, so that we should no longer hold any references to it.
	 */
	dtx->dd_fop = NULL;
	m0_sm_state_set(&dtx->dd_sm, M0_DDS_INPROGRESS);
	return M0_RC(rc);
}

static void dtx_done(struct m0_dtm0_dtx *dtx)
{
	M0_ENTRY("dtx=%p", dtx);
	M0_PRE(dtx != NULL);
	M0_PRE(m0_sm_group_is_locked(dtx->dd_sm.sm_grp));
	M0_PRE(dtx->dd_sm.sm_state == M0_DDS_STABLE);
	m0_sm_state_set(&dtx->dd_sm, M0_DDS_DONE);
	dtx_fini(dtx);
	M0_LEAVE();
}

static void dtx_exec_all_ast_cb(struct m0_sm_group *grp, struct m0_sm_ast *ast)
{
	struct m0_dtm0_dtx *dtx = ast->sa_datum;

	M0_ENTRY("dtx=%p", dtx);

	M0_ASSERT(dtx->dd_sm.sm_state == M0_DDS_EXECUTED_ALL);
	M0_ASSERT(dtx->dd_nr_executed == dtx->dd_txd.dtd_ps.dtp_nr);

	if (m0_dtm0_tx_desc_state_eq(&dtx->dd_txd, M0_DTPS_PERSISTENT)) {
		m0_sm_state_set(&dtx->dd_sm, M0_DDS_STABLE);
		M0_LOG(M0_DEBUG, "dtx " DTID0_F "is stable (EXEC_ALL)",
		       DTID0_P(&dtx->dd_txd.dtd_id));
	}

	M0_LEAVE();
}

static void dtx_persistent_ast_cb(struct m0_sm_group *grp,
				  struct m0_sm_ast   *ast)
{
	struct m0_dtm0_pmsg_ast      *pma = ast->sa_datum;
	struct m0_dtm0_dtx           *dtx = pma->p_dtx;
	struct m0_fop                *fop = pma->p_fop;
	struct dtm0_req_fop          *req = m0_fop_data(fop);
	const struct m0_dtm0_tx_desc *txd = &req->dtr_txr;

	M0_ENTRY("dtx=%p, fop=%p", dtx, fop);

	if (M0_IS0(dtx)) {
		M0_LOG(M0_DEBUG, "Dtx has already reached DONE state, "
		       "ignoring P msg for " DTID0_F ".",
		       DTID0_P(&txd->dtd_id));
		goto out;
	}

	m0_dtm0_tx_desc_apply(&dtx->dd_txd, txd);
	dtx_log_update(dtx);

	if (dtx->dd_sm.sm_state == M0_DDS_EXECUTED_ALL &&
	    m0_dtm0_tx_desc_state_eq(&dtx->dd_txd, M0_DTPS_PERSISTENT)) {
		M0_ASSERT(dtx->dd_nr_executed == dtx->dd_txd.dtd_ps.dtp_nr);
		M0_LOG(M0_DEBUG, "dtx " DTID0_F "is stable (PMA)",
		       DTID0_P(&dtx->dd_txd.dtd_id));
		m0_sm_state_set(&dtx->dd_sm, M0_DDS_STABLE);
	}

out:
	m0_free(pma);
	m0_fop_put_lock(fop); /* it was taken in ::m0_dtm0_dtx_post_pmsg */
	M0_LEAVE();
}

M0_INTERNAL void m0_dtm0_dtx_pmsg_post(struct m0_dtm0_dtx *dtx,
				       struct m0_fop      *fop)
{
	struct m0_dtm0_pmsg_ast *pma;

	M0_PRE(fop != NULL);
	M0_PRE(fop->f_opaque != NULL);
	M0_PRE(m0_fop_data(fop) != NULL);
	M0_PRE(dtx->dd_sm.sm_grp != &fop->f_item.ri_rmachine->rm_sm_grp);

	M0_ENTRY("dtx=%p, fop=%p", dtx, fop);
	/*
	 * XXX: it is an uncommon case where f_opaque is used on the server
	 * side (rather than on the client side) to associate a FOP with
	 * a user-defined datum. It helps to connect the lifetime of the
	 * AST that handles the notice with the lifetime of the FOP that
	 * has delivered the notice.
	 */
	pma = fop->f_opaque;
	M0_ASSERT_INFO(M0_IS0(pma), "PMA cannot be re-used");
	*pma = (struct  m0_dtm0_pmsg_ast) {
		.p_dtx = dtx,
		.p_fop = fop,
		.p_ast = {
			.sa_cb = dtx_persistent_ast_cb,
			.sa_datum = pma,
		},
	};
	m0_fop_get(fop); /* it will be put back by ::dtx_persistent_ast_cb */
	m0_sm_ast_post(dtx->dd_sm.sm_grp, &pma->p_ast);
	M0_LEAVE();
}

static void dtx_executed(struct m0_dtm0_dtx *dtx, uint32_t idx)
{
	struct m0_dtm0_tx_pa  *pa;

	M0_ENTRY("dtx=%p, idx=%"PRIu32, dtx, idx);

	M0_PRE(dtx != NULL);
	M0_PRE(m0_sm_group_is_locked(dtx->dd_sm.sm_grp));

	pa = &dtx->dd_txd.dtd_ps.dtp_pa[idx];

	M0_ASSERT(pa->p_state >= M0_DTPS_INPROGRESS);

	pa->p_state = max_check(pa->p_state, (uint32_t)M0_DTPS_EXECUTED);

	dtx->dd_nr_executed++;

	if (dtx->dd_sm.sm_state < M0_DDS_EXECUTED) {
		M0_ASSERT(dtx->dd_sm.sm_state == M0_DDS_INPROGRESS);
		m0_sm_state_set(&dtx->dd_sm, M0_DDS_EXECUTED);
	}

	if (dtx->dd_nr_executed == dtx->dd_txd.dtd_ps.dtp_nr) {
		M0_ASSERT(dtx->dd_sm.sm_state == M0_DDS_EXECUTED);
		M0_ASSERT_INFO(dtds_forall(&dtx->dd_txd, >= M0_DTPS_EXECUTED),
			       "Non-executed PAs should not exist "
			       "at this point.");
		m0_sm_state_set(&dtx->dd_sm, M0_DDS_EXECUTED_ALL);

		/*
		 * EXECUTED and STABLE should not be triggered within the
		 * same ast tick. This ast helps us to enforce it.
		 * XXX: there is a catch22-like problem with DIX states:
		 * DIX request should already be in "FINAL" state when
		 * the corresponding dtx reaches STABLE. However, the dtx
		 * cannot transit from EXECUTED to STABLE (through EXEC_ALL)
		 * if DIX reached FINAL already (the list of CAS rops has been
		 * destroyed). So that the EXEC_ALL ast cuts this knot by
		 * scheduling the transition EXEC_ALL -> STABLE in a separate
		 * tick where DIX request reached FINAL.
		 */
		dtx->dd_exec_all_ast = (struct m0_sm_ast) {
			.sa_cb = dtx_exec_all_ast_cb,
			.sa_datum = dtx,
		};
		m0_sm_ast_post(dtx->dd_sm.sm_grp, &dtx->dd_exec_all_ast);
	}

	dtx_log_update(dtx);
	M0_LEAVE();
}

M0_INTERNAL struct m0_dtx* m0_dtx0_alloc(struct m0_dtm0_service *svc,
					 struct m0_sm_group     *grp)
{
	struct m0_dtm0_dtx *dtx = dtx_alloc(svc, grp);
	return dtx ? &dtx->dd_ancient_dtx : NULL;
}

M0_INTERNAL void m0_dtx0_prepare(struct m0_dtx *dtx)
{
	M0_PRE(dtx != NULL);
	dtx_prepare(dtx->tx_dtx);
}

M0_INTERNAL int m0_dtx0_open(struct m0_dtx *dtx, uint32_t nr)
{
	M0_PRE(dtx != NULL);
	return dtx_open(dtx->tx_dtx, nr);
}

M0_INTERNAL int m0_dtx0_fid_assign(struct m0_dtx       *dtx,
				   uint32_t             pa_idx,
				   const struct m0_fid *pa_sfid)
{
	M0_PRE(dtx != NULL);
	return dtx_fid_assign(dtx->tx_dtx, pa_idx, pa_sfid);
}

M0_INTERNAL void m0_dtx0_fop_assign(struct m0_dtx       *dtx,
				    uint32_t             pa_idx,
				    const struct m0_fop *pa_fop)
{
	M0_PRE(dtx != NULL);
	dtx_fop_assign(dtx->tx_dtx, pa_idx, pa_fop);
}


M0_INTERNAL int m0_dtx0_close(struct m0_dtx *dtx)
{
	M0_PRE(dtx != NULL);
	return dtx_close(dtx->tx_dtx);
}

M0_INTERNAL void m0_dtx0_executed(struct m0_dtx *dtx, uint32_t pa_idx)
{
	M0_PRE(dtx != NULL);
	dtx_executed(dtx->tx_dtx, pa_idx);
}

M0_INTERNAL void m0_dtx0_done(struct m0_dtx *dtx)
{
	M0_PRE(dtx != NULL);
	dtx_done(dtx->tx_dtx);
}

M0_INTERNAL int m0_dtx0_txd_copy(const struct m0_dtx    *dtx,
				 struct m0_dtm0_tx_desc *dst)
{
	if (dtx == NULL) {
		/* No DTX => no txr */
		M0_SET0(dst);
		return 0;
	}

	return m0_dtm0_tx_desc_copy(&dtx->tx_dtx->dd_txd, dst);
}

M0_INTERNAL enum m0_dtm0_dtx_state m0_dtx0_sm_state(const struct m0_dtx *dtx)
{
	M0_PRE(dtx != NULL);
	return dtx->tx_dtx->dd_sm.sm_state;
}

#undef M0_TRACE_SUBSYSTEM

/** @} end of dtm group */

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
