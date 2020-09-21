/*
 * Copyright (c) 2015-2020 Seagate Technology LLC and/or its Affiliates
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


#define M0_TRACE_SUBSYSTEM M0_TRACE_SUBSYS_UT

#include "lib/trace.h"
#include "ut/ut.h"
#include "addb2/histogram.h"

#include "addb2/ut/common.h"

static void init_fini(void)
{
	struct m0_addb2_hist h = {};

	m0_addb2_hist_add(&h, 0, 1, 6, -1);
	m0_addb2_hist_del(&h);
	M0_SET0(&h);
	m0_addb2_hist_add_auto(&h, 3, 6, -1);
	m0_addb2_hist_del(&h);
}

static const int64_t minmax[][2] = {
	{ 0, 1 },
	{ 1, 2 },
	{ -1, 0 },
	{ 0, M0_BITS(58) },
	{ -M0_BITS(58), 2 },
	{ -M0_BITS(58), M0_BITS(58) },
	{ 0, 0 }
};

static const int64_t random_val[] = {
	3, 5, 7, 9, 10, 11, 13, 15, 17, 1024, 1500, 60, 7776, 0xdeadbeef, 0
};

static void test_one(struct m0_addb2_hist *h, int64_t val)
{
	uint32_t  orig[M0_ADDB2_HIST_BUCKETS];
	uint32_t *bucket = h->hi_data.hd_bucket;
	int       idx    = m0_addb2_hist_bucket(h, val);

	memcpy(orig, bucket, sizeof orig);
	m0_addb2_hist_mod(h, val);
	M0_UT_ASSERT(m0_forall(i, ARRAY_SIZE(orig),
			       bucket[i] == orig[i] + (i == idx)));
	M0_UT_ASSERT((idx == 0) == (val < h->hi_data.hd_min));
	M0_UT_ASSERT(ergo(val == h->hi_data.hd_min, idx == 1));
	M0_UT_ASSERT(ergo(val >= h->hi_data.hd_max,
			  idx == M0_ADDB2_HIST_BUCKETS - 1));
}
static void test_around(struct m0_addb2_hist *h, int64_t val)
{
	test_one(h, val - 1);
	test_one(h, val);
	test_one(h, val + 1);
}

static void test_hist(struct m0_addb2_hist *h)
{
	int i;

	for (i = 0; i < 60; ++i) {
		test_around(h, M0_BITS(i));
		test_around(h, -M0_BITS(i));
	}
	test_around(h, 0);
	test_around(h, h->hi_data.hd_min);
	test_around(h, h->hi_data.hd_max);
	for (i = 0; i < ARRAY_SIZE(random_val); ++i)
		test_around(h, random_val[i]);
}

static void test_bucket(void)
{
	int i;

	for (i = 0; minmax[i][0] != minmax[i][1]; ++i) {
		struct m0_addb2_hist h = {};

		m0_addb2_hist_add(&h, minmax[i][0],minmax[i][1], 68 + i, -1);
		test_hist(&h);
		m0_addb2_hist_del(&h);
	}
}

struct m0_ut_suite addb2_hist_ut = {
	.ts_name = "addb2-histogram",
	.ts_init = NULL,
	.ts_fini = NULL,
	.ts_tests = {
		{ "init-fini",      &init_fini },
		{ "history-bucket", &test_bucket },
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
