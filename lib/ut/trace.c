/* -*- C -*- */
/*
 * Copyright (c) 2011-2020 Seagate Technology LLC and/or its Affiliates
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


#include "lib/misc.h"   /* M0_SET0 */
#include "lib/ub.h"
#include "ut/ut.h"
#include "lib/thread.h"
#include "lib/assert.h"
#define M0_TRACE_SUBSYSTEM M0_TRACE_SUBSYS_UT
#include "lib/trace.h"

enum {
	NR       = 16,
	NR_INNER = 100000
};

static struct m0_thread t[NR];

void trace_thread_func(int d)
{
	int j;

	for (j = 0; j < NR_INNER; ++j)
		M0_LOG(M0_DEBUG, "d: %i, d*j: %i", d, d * j);
}

void test_trace(void)
{
	int i;
	int result;
	uint64_t u64;

	M0_LOG(M0_DEBUG, "forty two: %i", 42);
	M0_LOG(M0_DEBUG, "forty three and tree: %i %llu", 43,
			(unsigned long long)(u64 = 3));
	for (i = 0; i < NR_INNER; ++i)
		M0_LOG(M0_DEBUG, "c: %i, d: %i", i, i*i);

	M0_SET_ARR0(t);
	for (i = 0; i < NR; ++i) {
		result = M0_THREAD_INIT(&t[i], int, NULL, &trace_thread_func,
					i, "test_trace_%i", i);
		M0_ASSERT(result == 0);
	}
	for (i = 0; i < NR; ++i) {
		m0_thread_join(&t[i]);
		m0_thread_fini(&t[i]);
	}
	M0_LOG(M0_DEBUG, "X: %i and Y: %i", 43, result + 1);
	M0_LOG(M0_DEBUG, "%llx char: %c %llx string: %s",
		0x1234567887654321ULL,
		'c',
		0xfefefefefefefefeULL,
		(char *)"foobar");
}

enum {
	UB_ITER = 5000000
};

static void ub_empty(int i)
{
	M0_LOG(M0_DEBUG, "msg");
}

static void ub_8(int i)
{
	M0_LOG(M0_DEBUG, "%i", i);
}

static void ub_64(int i)
{
	M0_LOG(M0_DEBUG, "%i %i %i %i %i %i %i %i",
		i, i + 1, i + 2, i + 3, i + 4, i + 5,
		i + 6, i + 7);
}

struct m0_ub_set m0_trace_ub = {
	.us_name = "trace-ub",
	.us_run  = {
		{ .ub_name = "empty",
		  .ub_iter = UB_ITER,
		  .ub_round = ub_empty },

		{ .ub_name = "8",
		  .ub_iter = UB_ITER,
		  .ub_round = ub_8 },

		{ .ub_name = "64",
		  .ub_iter = UB_ITER,
		  .ub_round = ub_64 },

		{ .ub_name = NULL }
	}
};

/*
 *  Local variables:
 *  c-indentation-style: "K&R"
 *  c-basic-offset: 8
 *  tab-width: 8
 *  fill-column: 80
 *  scroll-step: 1
 *  End:
 */
