/* -*- C -*- */
/*
 * Copyright (c) 2014-2020 Seagate Technology LLC and/or its Affiliates
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


//#include <stdio.h>		/* snprintf */

#include "be/ut/helper.h"
#include "lib/errno.h"
#include "lib/memory.h"		/* m0_free */
#include "ut/ut.h"		/* M0_UT_ASSERT */
#include "ut/stob.h"		/* m0_ut_stob_linux_get */

#include "stob/ad.h"		/* m0_stob_ad_cfg_make */
#include "stob/domain.h"
#include "stob/stob.h"
#include "stob/stob_internal.h"

static void stob_ut_stob_domain(const char *location, const char *cfg,
				const char *init_cfg)
{
	struct m0_stob_domain *dom;
	uint64_t               dom_key = 0xec0de;
	struct m0_fid          dom_id;
	int		       rc;

	rc = m0_stob_domain_init(location, init_cfg, &dom);
	M0_UT_ASSERT(rc == -ENOENT);
	rc = m0_stob_domain_destroy_location(location);
	M0_UT_ASSERT(rc == 0 || rc == -ENOENT);
	rc = m0_stob_domain_create(location, init_cfg, dom_key, cfg, &dom);
	M0_UT_ASSERT(rc == 0);
	M0_UT_ASSERT(dom != NULL);
	m0_stob_domain_fini(dom);

	rc = m0_stob_domain_create(location, init_cfg, dom_key, cfg, &dom);
	M0_UT_ASSERT(rc == -EEXIST);

	rc = m0_stob_domain_init(location, init_cfg, &dom);
	M0_UT_ASSERT(rc == 0);
	M0_UT_ASSERT(dom != NULL);
	M0_UT_ASSERT(m0_stob_domain__dom_key(m0_stob_domain_id_get(dom)) ==
		     dom_key);

	/* Find existent domain */
	dom_id = *m0_stob_domain_id_get(dom);
	M0_UT_ASSERT(m0_stob_domain_find(&dom_id) == dom);

	/* Find non-existent domain */
	dom_id.f_key  ^= (1ULL << 56) - 1;
	M0_UT_ASSERT(m0_stob_domain_find(&dom_id) == NULL);

	rc = m0_stob_domain_destroy(dom);
	M0_UT_ASSERT(rc == 0);

	rc = m0_stob_domain_create(location, init_cfg, dom_key, cfg, &dom);
	M0_UT_ASSERT(rc == 0);
	m0_stob_domain_fini(dom);
	rc = m0_stob_domain_destroy_location(location);
	M0_UT_ASSERT(rc == 0);
}

void m0_stob_ut_stob_domain_null(void)
{
	stob_ut_stob_domain("nullstob:./__s", NULL, NULL);
}

#ifndef __KERNEL__
void m0_stob_ut_stob_domain_linux(void)
{
	/* Use unique name to avoid conflicts with other UTs. */
	stob_ut_stob_domain("linuxstob:./__sdl", NULL, NULL);
}

void m0_stob_ut_stob_domain_perf(void)
{
	stob_ut_stob_domain("perfstob:./__s", NULL, NULL);
}

void m0_stob_ut_stob_domain_perf_null(void)
{
	stob_ut_stob_domain("perfstob:./__s", "null=true", NULL);
}

extern void m0_stob_ut_ad_init(struct m0_be_ut_backend *ut_be,
			       struct m0_be_ut_seg     *ut_seg);
extern void m0_stob_ut_ad_fini(struct m0_be_ut_backend *ut_be,
			       struct m0_be_ut_seg     *ut_seg);

void m0_stob_ut_stob_domain_ad(void)
{
	struct m0_be_ut_backend  ut_be;
	struct m0_be_ut_seg      ut_seg;
	struct m0_stob          *stob;
	char                    *cfg;
	char                    *init_cfg;

	m0_stob_ut_ad_init(&ut_be, &ut_seg);
	stob = m0_ut_stob_linux_get();
	M0_UT_ASSERT(stob != NULL);
	m0_stob_ad_cfg_make(&cfg, ut_seg.bus_seg, m0_stob_id_get(stob), 0);
	M0_UT_ASSERT(cfg != NULL);
	m0_stob_ad_init_cfg_make(&init_cfg, &ut_be.but_dom);
	M0_UT_ASSERT(init_cfg != NULL);

	stob_ut_stob_domain("adstob:some_suffix", cfg, init_cfg);

	m0_free(cfg);
	m0_ut_stob_put(stob, true);
	m0_stob_ut_ad_fini(&ut_be, &ut_seg);
}
#endif

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
