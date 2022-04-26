/* -*- C -*- */
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


#include "ut/ut.h"
#include "lib/arith.h"                  /* max64, min64 */
#include "lib/misc.h"
#include "lib/tlist.h"

struct foo {
	uint64_t        f_val;
	struct m0_tlink f_linkage;
};

M0_TL_DESCR_DEFINE(foo, "fold-foo", static, struct foo, f_linkage, f_val, 0, 0);
M0_TL_DEFINE(foo, static, struct foo);

void test_fold(void)
{
	const int a[] = { 1, 2, 3, 4, 5, 6, 7, 8, 9, 0 };
	struct m0_tl head;
	struct foo foos[ARRAY_SIZE(a)] = {};
	int x = 0;

	/* Test summation. */
	M0_UT_ASSERT(m0_reduce(i, 4, 0, + a[i]) == a[0] + a[1] + a[2] + a[3]);
	/* Test empty range. */
	M0_UT_ASSERT(m0_reduce(i, 0, 8, + 1/x) == 8);
	/* Gauss' childhood problem, as in popular sources. */
	M0_UT_ASSERT(m0_reduce(i, 100, 0, + i + 1) == 5050);
	/* Maximum. */
	M0_UT_ASSERT(m0_fold(i, s, ARRAY_SIZE(a), -1, max64(s, a[i])) == 9);
	/* Minimum. */
	M0_UT_ASSERT(m0_fold(i, s, ARRAY_SIZE(a), 99, min64(s, a[i])) == 0);
	/* Now, find the *index* of the maximum. */
	M0_UT_ASSERT(m0_fold(i, s, ARRAY_SIZE(a), 0, a[i] > a[s] ? i : s) == 8);
	M0_UT_ASSERT(m0_fold(i, s, ARRAY_SIZE(a), 0, a[i] < a[s] ? i : s) == 9);
	foo_tlist_init(&head);
	/* Check empty list. */
	M0_UT_ASSERT(m0_tl_reduce(foo, f, &head, 8, + 1/x) == 8);
	for (x = 0; x < ARRAY_SIZE(a); ++x) {
		foo_tlink_init_at(&foos[x], &head);
		foos[x].f_val = a[x];
	}
	/* Sums of squares are the same. */
	M0_UT_ASSERT(m0_tl_reduce(foo, f, &head, 0, + f->f_val * f->f_val) ==
		     m0_reduce(i, ARRAY_SIZE(a), 0, + a[i] * a[i]));
	/* Maximal element in the list has maximal value. */
	M0_UT_ASSERT(m0_tl_fold(foo, f, s, &head, foo_tlist_head(&head),
				f->f_val > s->f_val ? f : s)->f_val ==
		     m0_fold(i, s, ARRAY_SIZE(a), -1, max64(s, a[i])));
}

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
