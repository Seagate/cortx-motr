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

	m0_dtm0_domain_init(dod, dod_cfg);
	m0_dtm0_domain_fini(dod);

	m0_dtm0_domain_cfg_free(dod_cfg);

	m0_free(dod_cfg);
	m0_free(dod);
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
