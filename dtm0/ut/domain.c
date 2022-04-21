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

#include "dtm0/domain.h"
#include "lib/memory.h"       /* M0_ALLOC_PTR */
#include "dtm0/cfg_default.h" /* m0_dtm0_domain_cfg_default_dup */
#include "ut/ut.h"            /* M0_UT_ASSERT */
#include "be/ut/helper.h"     /* m0_be_ut_backend_init */
#include "reqh/reqh.h"        /* m0_reqh */

enum {
	M0_DTM0_UT_DOMAIN_SEG_SIZE  = 0x2000000,
};

struct dtm0_ut_domain_ctx {
	struct m0_be_ut_backend   ut_be;
	struct m0_be_ut_seg       ut_seg;
	struct m0_reqh            reqh;
	struct m0_dtm0_domain_cfg dod_cfg;
	struct m0_dtm0_domain     dod;
};

static void dtm0_ut_domain_init(struct dtm0_ut_domain_ctx *dctx)
{
	int rc;

	rc = m0_dtm0_domain_cfg_default_dup(&dctx->dod_cfg, true);
	M0_UT_ASSERT(rc == 0);

	m0_be_ut_backend_init(&dctx->ut_be);
	m0_be_ut_seg_init(&dctx->ut_seg, &dctx->ut_be,
			  M0_DTM0_UT_DOMAIN_SEG_SIZE);

	/* TODO: Move it to log_init ? */
	dctx->dod_cfg.dodc_log.dlc_be_domain = &dctx->ut_be.but_dom;
	dctx->dod_cfg.dodc_log.dlc_seg =
		m0_be_domain_seg_first(dctx->dod_cfg.dodc_log.dlc_be_domain);

	rc = M0_REQH_INIT(&dctx->reqh,
			  .rhia_dtm     = (void*)1,
			  .rhia_mdstore = (void*)1,
			  .rhia_db      = dctx->dod_cfg.dodc_log.dlc_seg,
			  .rhia_fid     = &g_process_fid);
	M0_UT_ASSERT(rc == 0);
	m0_reqh_start(&dctx->reqh);

	rc = m0_dtm0_domain_init(&dctx->dod, &dctx->dod_cfg);
	M0_UT_ASSERT(rc == 0);
}

static void dtm0_ut_domain_fini(struct dtm0_ut_domain_ctx *dctx)
{
	m0_dtm0_domain_fini(&dctx->dod);
	m0_reqh_services_terminate(&dctx->reqh);
	m0_reqh_fini(&dctx->reqh);
	m0_be_ut_seg_fini(&dctx->ut_seg);
	m0_be_ut_backend_fini(&dctx->ut_be);
	m0_dtm0_domain_cfg_free(&dctx->dod_cfg);
}

void m0_dtm0_ut_domain_init_fini(void)
{
	struct m0_dtm0_domain     *dod;
	struct m0_dtm0_domain_cfg *dod_cfg;
	int                        rc;

	M0_ALLOC_PTR(dod);
	M0_UT_ASSERT(dod != NULL);
	M0_ALLOC_PTR(dod_cfg);
	M0_UT_ASSERT(dod_cfg != NULL);

	rc = m0_dtm0_domain_cfg_default_dup(dod_cfg, true);
	M0_UT_ASSERT(rc == 0);

	rc = m0_dtm0_domain_init(dod, dod_cfg);
	M0_UT_ASSERT(rc == 0);

	m0_dtm0_domain_fini(dod);

	m0_dtm0_domain_cfg_free(dod_cfg);

	m0_free(dod_cfg);
	m0_free(dod);
}

void m0_dtm0_ut_domain_full_init_fini(void)
{
	struct dtm0_ut_domain_ctx dctx = {};

	dtm0_ut_domain_init(&dctx);
	dtm0_ut_domain_fini(&dctx);
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
