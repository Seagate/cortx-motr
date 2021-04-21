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

#define M0_TRACE_SUBSYSTEM M0_TRACE_SUBSYS_DTM
#include "lib/trace.h"

#include "ut/ut.h"
#include "dtm0/clk_src.h"
#include "lib/errno.h"  /* EINVAL */
#include "lib/string.h" /* m0_asprintf, m0_streq */
#include "lib/memory.h" /* m0_free */

/* test if a timestamp can be converted into a string */
static void ts_format(void)
{
	char             *str;
	struct m0_dtm0_ts ts = M0_DTM0_TS_MIN;
	const char       *expected = "'@1'";

	m0_asprintf(&str, "'" DTS0_F "'", DTS0_P(&ts));
	M0_UT_ASSERT(str);

	M0_UT_ASSERT(m0_streq(expected, str));
	m0_free(str);
}

static void init_fini(void)
{
	struct m0_dtm0_clk_src dcs;

	m0_dtm0_clk_src_init(&dcs, M0_DTM0_CS_PHYS);
	m0_dtm0_clk_src_fini(&dcs);
}

/* test if it is possible to get now() value */
static void get_now(void)
{
	struct m0_dtm0_clk_src dcs;
	struct m0_dtm0_ts      now;

	m0_dtm0_clk_src_init(&dcs, M0_DTM0_CS_PHYS);
	m0_dtm0_clk_src_now(&dcs, &now);
	m0_dtm0_clk_src_fini(&dcs);
}

/* test relation between now() and the limits */
static void now_min_max(void)
{
	struct m0_dtm0_clk_src dcs;
	struct m0_dtm0_ts      now;
	struct m0_dtm0_ts      past = M0_DTM0_TS_MIN;
	struct m0_dtm0_ts      future = M0_DTM0_TS_MAX;
	int                    rc;

	m0_dtm0_clk_src_init(&dcs, M0_DTM0_CS_PHYS);
	m0_dtm0_clk_src_now(&dcs, &now);

	rc = m0_dtm0_ts_cmp(&dcs, &past, &now);
	M0_UT_ASSERT(rc == M0_DTS_LT);
	rc = m0_dtm0_ts_cmp(&dcs, &now, &past);
	M0_UT_ASSERT(rc == M0_DTS_GT);
	rc = m0_dtm0_ts_cmp(&dcs, &now, &now);
	M0_UT_ASSERT(rc == M0_DTS_EQ);

	rc = m0_dtm0_ts_cmp(&dcs, &now, &future);
	M0_UT_ASSERT(rc == M0_DTS_LT);
	rc = m0_dtm0_ts_cmp(&dcs, &future, &now);
	M0_UT_ASSERT(rc == M0_DTS_GT);

	rc = m0_dtm0_ts_cmp(&dcs, &past, &future);
	M0_UT_ASSERT(rc == M0_DTS_LT);

	rc = m0_dtm0_ts_cmp(&dcs, &future, &past);
	M0_UT_ASSERT(rc == M0_DTS_GT);

	m0_dtm0_clk_src_fini(&dcs);
}

/* test if clock is always advancing forward */
static void now_and_then(void)
{
	struct m0_dtm0_clk_src dcs;
	struct m0_dtm0_ts      first;
	struct m0_dtm0_ts      second;
	struct m0_dtm0_ts      third;
	int                    rc;

	m0_dtm0_clk_src_init(&dcs, M0_DTM0_CS_PHYS);

	m0_dtm0_clk_src_now(&dcs, &first);
	m0_dtm0_clk_src_now(&dcs, &second);
	m0_dtm0_clk_src_now(&dcs, &third);

	rc = m0_dtm0_ts_cmp(&dcs, &first, &second);
	M0_UT_ASSERT(rc == M0_DTS_LT);
	rc = m0_dtm0_ts_cmp(&dcs, &second, &third);
	M0_UT_ASSERT(rc == M0_DTS_LT);
	rc = m0_dtm0_ts_cmp(&dcs, &first, &third);
	M0_UT_ASSERT(rc == M0_DTS_LT);

	m0_dtm0_clk_src_fini(&dcs);
}

struct m0_ut_suite dtm0_clk_src_ut = {
	.ts_name   = "dtm0-clk-src-ut",
	.ts_init   = NULL,
	.ts_fini   = NULL,
	.ts_tests  = {
		{ "ts-format",             ts_format        },
		{ "phys-init-fini",        init_fini        },
		{ "phys-now",              get_now          },
		{ "phys-now-min-max",      now_min_max      },
		{ "phys-now-and-then",     now_and_then     },
		{ NULL, NULL }
	}
};

#undef M0_TRACE_SUBSYSTEM
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
