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


#define M0_TRACE_SUBSYSTEM M0_TRACE_SUBSYS_LIB
#include "lib/trace.h"

/**
   @addtogroup comb Combinations
   @{
*/

M0_INTERNAL uint64_t m0_fact(uint64_t n)
{
	M0_PRE(n > 0);

	return n == 1 || n == 0 ? 1 : n * m0_fact(n - 1);
}

M0_INTERNAL uint32_t m0_ncr(uint64_t n, uint64_t r)
{
	uint64_t i;
	uint64_t m = n;

	M0_PRE(n >= r);

	if (r == 0)
		return 1;
	for (i = 1; i < r; i++)
		m *= n - i;
	return m / m0_fact(r);
}

M0_INTERNAL int m0_combination_index(int N, int K, int *x)
{
	int m;
	int q;
	int n;
	int r;
	int idx = 0;

	M0_ENTRY("N:%d K=%d", N, K);
	M0_PRE(0 < K && K <= N);
	M0_PRE(m0_forall(i, K, x[i] < N));

	for (q = 0; q < x[0]; q++) {
		n = N - (q + 1);
		r = K - 1;
		idx += m0_ncr(n, r);
	}
	for (m = 1; m < K; m++) {
		for (q = 0; q < (x[m] - x[m - 1] - 1); q++) {
			n = N - (x[m - 1] + 1) - (q + 1);
			r = K - m - 1;
			idx += m0_ncr(n, r);
		}
	}
	return M0_RC(idx);
}

M0_INTERNAL void m0_combination_inverse(int cid, int N, int K, int *x)
{
	int m;
	int q;
	int n;
	int r;
	int idx = 0;
	int old_idx = 0;
	int i = 0;
	int j;

	M0_ENTRY("N:%d K=%d cid:%d \n", N, K, cid);
	M0_PRE(0 < K && K <= N);

	for (q = 0; idx < cid + 1; q++) {
		old_idx = idx;
		n = N - (q + 1);
		r = K - 1;
		idx += m0_ncr(n, r);
	}
	idx = old_idx;
	x[i++] = q - 1;

	for (m = 1; m < K; m++) {
		for (q = 0; idx < cid + 1; q++) {
			old_idx = idx;
			n = N - (x[i - 1] + 1) - (q + 1);
			r = K - m - 1;
			idx += m0_ncr(n, r);
		}
		if (idx >= cid + 1)
			idx = old_idx;
		x[i] = x[i - 1] + q;
		i++;
	}

	M0_LOG(M0_DEBUG, "Combinations");
	for (j = 0; j < i; j++)
		M0_LOG(M0_DEBUG, "%d \t", x[j]);
	M0_LEAVE();
}

#undef M0_TRACE_SUBSYSTEM

/** @} end of comb group */

/*
 *  Local variables:
 *  c-indentation-style: "K&R"
 *  c-basic-offset: 8
 *  tab-width: 8
 *  fill-column: 80
 *  scroll-step: 1
 *  End:
 */
