/* -*- C -*- */
/*
 * Copyright (c) 2016-2020 Seagate Technology LLC and/or its Affiliates
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


#include "lib/misc.h" /* ARRAY_SIZE */
#include "lib/combinations.h"
#include "ut/ut.h"

void test_combinations(void)
{
	enum { NR_COMBINATIONS = 35 };
	int A[] = { 0, 1, 2, 3, 4, 5, 6 };
	int X[] = { 2, 4, 6 };
	int comb[] = { 0, 0, 0 };
	int N = ARRAY_SIZE(A);
	int K = ARRAY_SIZE(X);
	int cid;
	int i;

	M0_UT_ASSERT(m0_fact(K) == 6);
	M0_UT_ASSERT(m0_fact(N) / (m0_fact(K) * m0_fact(N - K)) ==
		     m0_ncr(N, K));

	cid = m0_combination_index(N, K, X);
	M0_UT_ASSERT(cid == 29);
	m0_combination_inverse(cid, N, K, comb);
	M0_UT_ASSERT(m0_forall(j, K, X[j] == comb[j]));

	M0_UT_ASSERT(m0_ncr(N, K) == NR_COMBINATIONS);
	for (i = 0; i < NR_COMBINATIONS; i++) {
		m0_combination_inverse(i, N, K, comb);
		cid = m0_combination_index(N, K, comb);
		M0_UT_ASSERT(cid == i);
	}
}
M0_EXPORTED(test_combinations);

/*
 *  Local variables:
 *  c-indentation-style: "K&R"
 *  c-basic-offset: 8
 *  tab-width: 8
 *  fill-column: 80
 *  scroll-step: 1
 *  End:
 */

