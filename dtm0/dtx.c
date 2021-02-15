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



/**
 * @addtogroup dtm
 *
 * @{
 */

#define M0_TRACE_SUBSYSTEM M0_TRACE_SUBSYS_DTM
#include "dtm0/dtx.h"
#include "lib/assert.h" /* M0_PRE */
#include "lib/memory.h" /* M0_ALLOC */
#include "lib/errno.h"  /* ENOMEM */
#include "lib/trace.h"  /* M0_ERR */
#include "dtm0/service.h" /* m0_dtm0_service */
#include "reqh/reqh.h" /* reqh2confc */
#include "conf/helpers.h" /* proc2srv */

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

static struct m0_sm_conf dtx_sm_conf = {
	.scf_name      = "dtm0dtx",
	.scf_nr_states = ARRAY_SIZE(dtx_states),
	.scf_state     = dtx_states,
	.scf_trans_nr  = ARRAY_SIZE(dtx_trans),
	.scf_trans     = dtx_trans,
};

M0_INTERNAL void m0_dtm0_dtx_domain_init(void)
{
	if (!m0_sm_conf_is_initialized(&dtx_sm_conf))
		m0_sm_conf_init(&dtx_sm_conf);
}

M0_INTERNAL void m0_dtm0_dtx_domain_fini(void)
{
	if (m0_sm_conf_is_initialized(&dtx_sm_conf))
		m0_sm_conf_fini(&dtx_sm_conf);
}

static struct m0_dtm0_dtx *m0_dtm0_dtx_alloc(struct m0_dtm0_service *svc,
					     struct m0_sm_group     *group)
{
	struct m0_dtm0_dtx *dtx;

	M0_PRE(svc != NULL);
	M0_PRE(group != NULL);

	M0_ALLOC_PTR(dtx);
	if (dtx != NULL) {
		dtx->dd_dtms = svc;
		dtx->dd_ancient_dtx.tx_dtx = dtx;
		m0_sm_init(&dtx->dd_sm, &dtx_sm_conf, M0_DDS_INIT, group);
	}
	return dtx;
}

static void m0_dtm0_dtx_free(struct m0_dtm0_dtx *dtx)
{
	M0_PRE(dtx != NULL);
	m0_sm_fini(&dtx->dd_sm);
	m0_dtm0_tx_desc_fini(&dtx->dd_txd);
	m0_free(dtx);
}

static int m0_dtm0_dtx_prepare(struct m0_dtm0_dtx *dtx)
{
	int               rc;

	M0_PRE(dtx != NULL);
	rc = m0_dtm0_clk_src_now(&dtx->dd_dtms->dos_clk_src,
				 &dtx->dd_txd.dtd_id.dti_ts);
	if (rc != 0)
		return M0_RC(rc);

	dtx->dd_txd.dtd_id.dti_fid = dtx->dd_dtms->dos_generic.rs_service_fid;
	M0_POST(m0_dtm0_tid__invariant(&dtx->dd_txd.dtd_id));
	return 0;
}

static int m0_dtm0_dtx_open(struct m0_dtm0_dtx  *dtx,
			    uint32_t             nr)
{
	M0_PRE(dtx != NULL);
	return m0_dtm0_tx_desc_init(&dtx->dd_txd, nr);
}

static int m0_dtm0_dtx_assign(struct m0_dtm0_dtx  *dtx,
			      uint32_t             pa_idx,
			      const struct m0_fid *pa_fid)
{
	struct m0_dtm0_tx_pa   *pa;
	struct m0_reqh         *reqh;
	struct m0_conf_cache   *cache;
	struct m0_conf_obj     *obj;
	struct m0_conf_process *proc;
	struct m0_fid           rdtms_fid;
	int                     rc;

	M0_PRE(dtx != NULL);
	M0_PRE(pa_idx < dtx->dd_txd.dtd_pg.dtpg_nr);
	M0_PRE(m0_fid_is_valid(pa_fid));

	/* TODO: Should we release any conf objects in the end? */

	reqh = dtx->dd_dtms->dos_generic.rs_reqh;
	cache = &m0_reqh2confc(reqh)->cc_cache;

	obj = m0_conf_cache_lookup(cache, pa_fid);
	M0_ASSERT_INFO(obj != NULL, "User service is not in the conf cache?");

	obj = m0_conf_obj_grandparent(obj);
	M0_ASSERT_INFO(obj != NULL, "Process the service belongs to "
		       "is not a part of the conf cache?");

	proc = M0_CONF_CAST(obj, m0_conf_process);
	M0_ASSERT_INFO(proc != NULL, "The grandparent is not a process?");

	rc = m0_conf_process2service_get(&reqh->rh_rconfc.rc_confc,
					 &proc->pc_obj.co_id, M0_CST_DTM0,
					 &rdtms_fid);

	M0_ASSERT_INFO(rc == 0, "Cannot find remote DTM service on the remote "
		       "process that runs this user service?");

	pa = &dtx->dd_txd.dtd_pg.dtpg_pa[pa_idx];
	M0_PRE(M0_IS0(pa));

	pa->pa_fid = rdtms_fid;
	M0_ASSERT(pa->pa_state == M0_DTPS_INIT);
	pa->pa_state = M0_DTPS_INPROGRESS;

	M0_LOG(DEBUG, "pa: " FID_F " (User) => " FID_F " (DTM) ",
	       FID_P(pa_fid), FID_P(&rdtms_fid));

	/* TODO: All these M0_ASSERTs will be converted into IFs eventually
	 * if we want to gracefully fail instead of m0_panic'ing in the case
	 * where the config is not correct.
	 */
	return M0_RC(0);
}

static int m0_dtm0_dtx_close(struct m0_dtm0_dtx *dtx)
{
	M0_PRE(dtx != NULL);
	M0_PRE(m0_sm_group_is_locked(dtx->dd_sm.sm_grp));

	m0_sm_state_set(&dtx->dd_sm, M0_DDS_INPROGRESS);

	/* TODO: add a log entry */

	return 0;
}

/* TODO: move into tx_desc module */
static bool m0_dtm0_tx_desc_state(const struct m0_dtm0_tx_desc *txd,
				  enum m0_dtm0_tx_pa_state state)
{
	return m0_forall(i, txd->dtd_pg.dtpg_nr,
			 txd->dtd_pg.dtpg_pa[i].pa_state == state);
}

static void m0_dtm0_dtx_persistent(struct m0_dtm0_dtx *dtx, uint32_t idx)
{
	struct m0_dtm0_tx_pa *pa;

	M0_PRE(dtx != NULL);
	M0_PRE(m0_sm_group_is_locked(dtx->dd_sm.sm_grp));

	pa = &dtx->dd_txd.dtd_pg.dtpg_pa[idx];

	/* Only I->P, E->P, P->P transitions are allowed here */
	M0_ASSERT(pa->pa_state >= M0_DTPS_INPROGRESS &&
		  pa->pa_state <= M0_DTPS_PERSISTENT);
	pa->pa_state = M0_DTPS_PERSISTENT;

	/* TODO: remove the log entry */
}

static void dtx_exec_all_ast_cb(struct m0_sm_group *grp, struct m0_sm_ast *ast)
{
	struct m0_dtm0_dtx *dtx = ast->sa_datum;
	int                 i;

	M0_ASSERT(dtx->dd_sm.sm_state == M0_DDS_EXECUTED_ALL);
	M0_ASSERT(dtx->dd_nr_executed == dtx->dd_txd.dtd_pg.dtpg_nr);

	/* XXX: This loop emulates synchronous arrival of DTM0 notices.
	 * It should be removed once DTM0 service is able to send the notice.
	 */
	for (i = 0; i < dtx->dd_txd.dtd_pg.dtpg_nr; ++i) {
		m0_dtm0_dtx_persistent(dtx, i);
	}

	if (m0_dtm0_tx_desc_state(&dtx->dd_txd, M0_DTPS_PERSISTENT)) {
		m0_sm_state_set(&dtx->dd_sm, M0_DDS_STABLE);
	}
}

static void m0_dtm0_dtx_executed(struct m0_dtm0_dtx *dtx, uint32_t idx)
{
	struct m0_dtm0_tx_pa *pa;

	M0_PRE(dtx != NULL);
	M0_PRE(m0_sm_group_is_locked(dtx->dd_sm.sm_grp));

	pa = &dtx->dd_txd.dtd_pg.dtpg_pa[idx];

	M0_ASSERT(pa->pa_state >= M0_DTPS_INPROGRESS);

	pa->pa_state = max_check(pa->pa_state, (uint32_t) M0_DTPS_EXECUTED);

	dtx->dd_nr_executed++;

	if (dtx->dd_sm.sm_state < M0_DDS_EXECUTED) {
		M0_ASSERT(dtx->dd_sm.sm_state == M0_DDS_INPROGRESS);
		m0_sm_state_set(&dtx->dd_sm, M0_DDS_EXECUTED);
	}

	if (dtx->dd_nr_executed == dtx->dd_txd.dtd_pg.dtpg_nr) {
		M0_ASSERT(dtx->dd_sm.sm_state == M0_DDS_EXECUTED);
		m0_sm_state_set(&dtx->dd_sm, M0_DDS_EXECUTED_ALL);

		dtx->dd_exec_all_ast.sa_cb = dtx_exec_all_ast_cb;
		dtx->dd_exec_all_ast.sa_datum = dtx;
		/* TODO: This is a poor-man's async call to ensure that
		 * EXECUTED and STABLE are processed in "separate" ticks of
		 * this machine. Such situation might happen only if DTM0
		 * service notices are emulated (see the XXX comment inside
		 * ::dtx_exec_all_ast_cb). In real life, DTM0 service will
		 * always post such an update as an ast, therefore the
		 * state transition from the callback (::dtx_exec_all_ast_cb)
		 * can be moved right in here.
		 */
		m0_sm_ast_post(dtx->dd_sm.sm_grp, &dtx->dd_exec_all_ast);
	}

	/* TODO: update the log entry */
}

M0_INTERNAL struct m0_dtx* m0_dtx0_alloc(struct m0_dtm0_service *svc,
					 struct m0_sm_group     *group)
{
	struct m0_dtm0_dtx *dtx;

	dtx = m0_dtm0_dtx_alloc(svc, group);
	if (dtx == NULL)
		return NULL;

	return &dtx->dd_ancient_dtx;
}

M0_INTERNAL void m0_dtx0_free(struct m0_dtx *dtx)
{
	M0_PRE(dtx != NULL);
	m0_dtm0_dtx_free(dtx->tx_dtx);
}

M0_INTERNAL int m0_dtx0_prepare(struct m0_dtx *dtx)
{
	M0_PRE(dtx != NULL);
	return m0_dtm0_dtx_prepare(dtx->tx_dtx);
}

M0_INTERNAL int m0_dtx0_open(struct m0_dtx  *dtx, uint32_t nr)
{
	M0_PRE(dtx != NULL);
	return m0_dtm0_dtx_open(dtx->tx_dtx, nr);
}

M0_INTERNAL int m0_dtx0_assign(struct m0_dtx       *dtx,
			       uint32_t             pa_idx,
			       const struct m0_fid *pa_fid)
{
	M0_PRE(dtx != NULL);
	return m0_dtm0_dtx_assign(dtx->tx_dtx, pa_idx, pa_fid);
}

M0_INTERNAL int m0_dtx0_close(struct m0_dtx *dtx)
{
	M0_PRE(dtx != NULL);
	return m0_dtm0_dtx_close(dtx->tx_dtx);
}

M0_INTERNAL void m0_dtx0_executed(struct m0_dtx *dtx, uint32_t pa_idx)
{
	M0_PRE(dtx != NULL);
	m0_dtm0_dtx_executed(dtx->tx_dtx, pa_idx);
}

M0_INTERNAL int m0_dtx0_copy_txd(const struct m0_dtx    *dtx,
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
