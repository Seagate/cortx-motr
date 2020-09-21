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


/**
 * @addtogroup ut
 *
 * @{
 */

#define M0_TRACE_SUBSYSTEM M0_TRACE_SUBSYS_UT
#include "lib/trace.h"

#include "ut/misc.h"

#include "lib/arith.h"  /* M0_SWAP */
#include "lib/misc.h"   /* m0_reduce */

M0_INTERNAL void m0_ut_random_shuffle(uint64_t *arr,
				      uint64_t  nr,
				      uint64_t *seed)
{
	uint64_t i;

	for (i = nr - 1; i > 0; --i)
		M0_SWAP(arr[i], arr[m0_rnd64(seed) % (i + 1)]);
}

M0_INTERNAL void m0_ut_random_arr_with_sum(uint64_t *arr,
					   uint64_t  nr,
					   uint64_t  sum,
					   uint64_t *seed)
{
	uint64_t split;
	uint64_t sum_split;

	M0_PRE(nr > 0);
	if (nr == 1) {
		arr[0] = sum;
	} else {
		split = m0_rnd64(seed) % (nr - 1) + 1;
		sum_split = sum == 0 ? 0 : m0_rnd64(seed) % sum;
		m0_ut_random_arr_with_sum(&arr[0], split, sum_split, seed);
		m0_ut_random_arr_with_sum(&arr[split],
					  nr - split, sum - sum_split, seed);
	}
	M0_POST_EX(m0_reduce(i, nr, 0, + arr[i]) == sum);
}


#undef M0_TRACE_SUBSYSTEM

/** @} end of ut group */

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
