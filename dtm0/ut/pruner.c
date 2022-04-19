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
#include "lib/memory.h" /* M0_ALLOC_PTR */
#include "dtm0/domain.h" /* m0_dtm0_domain_cfg */
#include "dtm0/cfg_default.h" /* m0_dtm0_domain_cfg_default_dup */
#include "ut/ut.h"
#include "reqh/reqh.h" /* M0_REQH_INIT */
#include "dtm0/ut/helper.h"     /* dtm0_ut_log_ctx */

void m0_dtm0_ut_reqh_init(struct m0_reqh **preqh)
{
	struct m0_reqh *reqh;
	int             rc;

	M0_ALLOC_PTR(reqh);
	M0_UT_ASSERT(reqh != NULL);

	rc = M0_REQH_INIT(reqh,
			  .rhia_dtm     = (void*)1,
			  .rhia_mdstore = (void*)1,
			  .rhia_fid     = &g_process_fid);
	M0_UT_ASSERT(rc == 0);
	m0_reqh_start(reqh);

	*preqh = reqh;
}

void m0_dtm0_ut_reqh_fini(struct m0_reqh **preqh)
{
	struct m0_reqh *reqh = *preqh;

	m0_reqh_services_terminate(reqh);
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
	struct m0_dtm0_domain_cfg cfg;
	int                       rc;

	pctx->lctx = dtm0_ut_log_init();
	rc = m0_dtm0_log_open(&pctx->lctx->dol, &pctx->lctx->dod_cfg.dodc_log);
	M0_UT_ASSERT(rc == 0);

	m0_dtm0_ut_reqh_init(&pctx->reqh);
	m0_dtm0_ut_cfs_init(&pctx->cfs, pctx->reqh);

	M0_ALLOC_PTR(pctx->pruner);
	M0_UT_ASSERT(pctx->pruner != NULL);

	rc = m0_dtm0_domain_cfg_default_dup(&cfg, false);
	M0_UT_ASSERT(rc == 0);

	cfg.dodc_pruner.dpc_cfs = pctx->cfs;
	cfg.dodc_pruner.dpc_dol = &pctx->lctx->dol;

	rc = m0_dtm0_pruner_init(pctx->pruner, &cfg.dodc_pruner);
	M0_UT_ASSERT(rc == 0);

	m0_dtm0_domain_cfg_free(&cfg);
}

static void dtm0_ut_pruner_start(struct dtm0_ut_pruner_ctx *ctx)
{
	m0_dtm0_pruner_start(ctx->pruner);
}

static void dtm0_ut_pruner_stop(struct dtm0_ut_pruner_ctx *pctx)
{
	m0_dtm0_log_end(&pctx->lctx->dol);
	m0_dtm0_pruner_stop(pctx->pruner);
}

static void dtm0_ut_pruner_fini(struct dtm0_ut_pruner_ctx *pctx)
{
	m0_dtm0_log_close(&pctx->lctx->dol);
	m0_dtm0_pruner_fini(pctx->pruner);
	m0_free(pctx->pruner);
	pctx->pruner = NULL;

	m0_dtm0_ut_cfs_fini(&pctx->cfs);
	m0_dtm0_ut_reqh_fini(&pctx->reqh);

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
	dtm0_ut_pruner_stop(&ctx);
	dtm0_ut_pruner_fini(&ctx);
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
