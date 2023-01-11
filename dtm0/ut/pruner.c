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
 * @addtogroup dtm0
 *
 * @{
 */

#define M0_TRACE_SUBSYSTEM M0_TRACE_SUBSYS_UT
#include "lib/trace.h"

#include "dtm0/pruner.h"
#include "ut/ut.h"
#include "lib/memory.h" /* M0_ALLOC_PTR */
#include "dtm0/domain.h" /* m0_dtm0_domain_cfg */
#include "dtm0/cfg_default.h" /* m0_dtm0_domain_cfg_default_dup */
#include "reqh/reqh.h" /* M0_REQH_INIT */
#include "dtm0/ut/helper.h"     /* dtm0_ut_log_ctx */
#include "dtm0/dtm0.h" /* m0_dtm0_redo */

void m0_dtm0_ut_reqh_init(struct m0_reqh **preqh, struct m0_be_seg *be_seg)
{
	struct m0_reqh *reqh;
	int             rc;

	M0_ALLOC_PTR(reqh);
	M0_UT_ASSERT(reqh != NULL);

	rc = M0_REQH_INIT(reqh,
			  .rhia_dtm     = (void*)1,
			  .rhia_mdstore = (void*)1,
			  .rhia_db      = be_seg,
			  .rhia_fid     = &g_process_fid);
	M0_UT_ASSERT(rc == 0);
	m0_reqh_start(reqh);

	*preqh = reqh;
}

void m0_dtm0_ut_reqh_fini(struct m0_reqh **preqh)
{
	struct m0_reqh *reqh = *preqh;

	m0_reqh_services_terminate(reqh);
	reqh->rh_beseg = NULL;
	m0_reqh_fini(reqh);
	m0_free(reqh);
	*preqh = NULL;
}

void m0_dtm0_ut_cfs_init(struct m0_co_fom_service **pcfs,
			 struct m0_reqh *reqh)
{
	struct m0_co_fom_service *cfs;
	int                       rc;
	M0_ALLOC_PTR(cfs);
	M0_UT_ASSERT(cfs != NULL);
	rc = m0_co_fom_service_init(cfs, reqh);
	M0_UT_ASSERT(rc == 0);
	*pcfs = cfs;
}

void m0_dtm0_ut_cfs_fini(struct m0_co_fom_service **pcfs)
{
	struct m0_co_fom_service *cfs = *pcfs;
	m0_co_fom_service_fini(cfs);
	m0_free(cfs);
	*pcfs = NULL;
}

struct dtm0_ut_pruner_ctx {
	struct m0_dtm0_pruner    *pruner;
	struct m0_reqh           *reqh;
	struct m0_co_fom_service *cfs;
	struct dtm0_ut_log_ctx   *lctx;
};

static void dtm0_ut_pruner_init(struct dtm0_ut_pruner_ctx *pctx)
{
	struct m0_dtm0_domain_cfg *cfg;
	int                        rc;

	pctx->lctx = dtm0_ut_log_init();
	cfg = &pctx->lctx->dod_cfg;
	rc = m0_dtm0_log_open(&pctx->lctx->dol, &cfg->dodc_log);
	M0_UT_ASSERT(rc == 0);

	m0_dtm0_ut_reqh_init(&pctx->reqh, cfg->dodc_log.dlc_seg);
	m0_dtm0_ut_cfs_init(&pctx->cfs, pctx->reqh);

	M0_ALLOC_PTR(pctx->pruner);
	M0_UT_ASSERT(pctx->pruner != NULL);

	cfg->dodc_pruner.dpc_cfs = pctx->cfs;
	cfg->dodc_pruner.dpc_dol = &pctx->lctx->dol;

	rc = m0_dtm0_pruner_init(pctx->pruner, &cfg->dodc_pruner);
	M0_UT_ASSERT(rc == 0);
}

static void dtm0_ut_pruner_start(struct dtm0_ut_pruner_ctx *ctx)
{
	m0_dtm0_pruner_start(ctx->pruner);
}

static void dtm0_ut_pruner_stop(struct dtm0_ut_pruner_ctx *pctx)
{
	m0_dtm0_pruner_stop(pctx->pruner);
}

static void dtm0_ut_pruner_fini(struct dtm0_ut_pruner_ctx *pctx)
{
	m0_dtm0_pruner_fini(pctx->pruner);
	m0_free(pctx->pruner);
	pctx->pruner = NULL;

	m0_dtm0_ut_cfs_fini(&pctx->cfs);
	m0_dtm0_ut_reqh_fini(&pctx->reqh);

	m0_dtm0_log_close(&pctx->lctx->dol);
	dtm0_ut_log_fini(pctx->lctx);
}

void m0_dtm0_ut_pruner_init_fini(void)
{
	struct dtm0_ut_pruner_ctx ctx = {};
	dtm0_ut_pruner_init(&ctx);
	dtm0_ut_pruner_fini(&ctx);
}

void m0_dtm0_ut_pruner_start_stop(void)
{
	struct dtm0_ut_pruner_ctx ctx = {};

	dtm0_ut_pruner_init(&ctx);
	dtm0_ut_pruner_start(&ctx);
	m0_dtm0_log_end(&ctx.lctx->dol);
	dtm0_ut_pruner_stop(&ctx);
	dtm0_ut_pruner_fini(&ctx);
}

enum {
	DTM0_UT_REDO_TS_BASE = 1,
};

static void dtm0_ut_log_redo_add(struct dtm0_ut_log_ctx *lctx, int timestamp)
{
	struct m0_dtm0_redo *redo;
	int                  rc = 0;
	struct m0_fid       *p_sdev_fid;


	redo = dtm0_ut_redo_get(DTM0_UT_REDO_TS_BASE);
	M0_UT_ASSERT(redo != NULL);
	p_sdev_fid = &redo->dtr_descriptor.dtd_id.dti_originator_sdev_fid;
	redo->dtr_descriptor.dtd_id.dti_timestamp = timestamp;
	M0_BE_UT_TRANSACT(&lctx->ut_be, tx, cred,
			  m0_dtm0_log_redo_add_credit(&lctx->dol,
						      redo,
						      &cred),
			  rc = m0_dtm0_log_redo_add(&lctx->dol, tx, redo,
						    p_sdev_fid));
	M0_UT_ASSERT(rc == 0);
	dtm0_ut_redo_put(redo);
}

void m0_dtm0_ut_pruner_one(void)
{
	struct dtm0_ut_pruner_ctx ctx = {};

	dtm0_ut_pruner_init(&ctx);
	dtm0_ut_pruner_start(&ctx);
	dtm0_ut_log_redo_add(ctx.lctx, DTM0_UT_REDO_TS_BASE);
	m0_dtm0_log_end(&ctx.lctx->dol);
	dtm0_ut_pruner_stop(&ctx);
	M0_UT_ASSERT(m0_dtm0_log_is_empty(&ctx.lctx->dol));
	dtm0_ut_pruner_fini(&ctx);
}

void m0_dtm0_ut_pruner_two(void)
{
	struct dtm0_ut_pruner_ctx ctx = {};

	dtm0_ut_pruner_init(&ctx);
	dtm0_ut_pruner_start(&ctx);
	dtm0_ut_log_redo_add(ctx.lctx, DTM0_UT_REDO_TS_BASE);
	dtm0_ut_log_redo_add(ctx.lctx, DTM0_UT_REDO_TS_BASE + 1);
	m0_dtm0_log_end(&ctx.lctx->dol);
	dtm0_ut_pruner_stop(&ctx);
	M0_UT_ASSERT(m0_dtm0_log_is_empty(&ctx.lctx->dol));
	dtm0_ut_pruner_fini(&ctx);
}

void m0_dtm0_ut_pruner_many_left(void)
{
	struct dtm0_ut_pruner_ctx ctx    = {};
	const int                 dtx_nr = 0x100;
	int                       i;

	dtm0_ut_pruner_init(&ctx);
	for (i = 0; i < dtx_nr; ++i)
		dtm0_ut_log_redo_add(ctx.lctx, DTM0_UT_REDO_TS_BASE + i);
	dtm0_ut_pruner_start(&ctx);
	m0_dtm0_log_end(&ctx.lctx->dol);
	dtm0_ut_pruner_stop(&ctx);
	M0_UT_ASSERT(!m0_dtm0_log_is_empty(&ctx.lctx->dol));
	dtm0_ut_pruner_fini(&ctx);
}

void m0_dtm0_ut_pruner_mpsc(void)
{
	struct dtm0_ut_pruner_ctx ctx[1] = {};
	struct dtm0_ut_log_mp_ctx lmp_ctx;

	dtm0_ut_pruner_init(ctx);
	dtm0_ut_pruner_start(ctx);

	dtm0_ut_log_mp_init(&lmp_ctx, ctx->lctx);
	dtm0_ut_log_mp_run(&lmp_ctx);
	m0_be_op_wait(lmp_ctx.op);
	dtm0_ut_log_mp_fini(&lmp_ctx);

	M0_UT_ASSERT(!m0_dtm0_log_is_empty(&ctx->lctx->dol));
	m0_nanosleep(M0_MKTIME(0, 100), NULL);
	M0_UT_ASSERT(!m0_dtm0_log_is_empty(&ctx->lctx->dol));

	m0_dtm0_log_end(&ctx->lctx->dol);
	dtm0_ut_pruner_stop(ctx);
	dtm0_ut_pruner_fini(ctx);
}

void m0_dtm0_ut_pruner_mpsc_many(void)
{
	struct dtm0_ut_pruner_ctx ctx[1] = {};
	struct dtm0_ut_log_mp_ctx lmp_ctx;
	int                       run;
	const int                 nr_runs = 4;

	dtm0_ut_pruner_init(ctx);
	dtm0_ut_pruner_start(ctx);

	for (run = 0; run < nr_runs; ++run) {
		dtm0_ut_log_mp_init(&lmp_ctx, ctx->lctx);
		dtm0_ut_log_mp_run(&lmp_ctx);
		m0_be_op_wait(lmp_ctx.op);
		dtm0_ut_log_mp_fini(&lmp_ctx);

		while (!m0_dtm0_log_is_empty(&ctx->lctx->dol))
			m0_nanosleep(M0_MKTIME(0, 100), NULL);
	}

	m0_dtm0_log_end(&ctx->lctx->dol);
	dtm0_ut_pruner_stop(ctx);
	dtm0_ut_pruner_fini(ctx);
}
#undef M0_TRACE_SUBSYSTEM

/** @} end of dtm0 group */

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
