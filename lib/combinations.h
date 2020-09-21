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


#pragma once

#ifndef __MOTR_LIB_COMBINATIONS_H__
#define __MOTR_LIB_COMBINATIONS_H__

/**
 * @addtogroup comb combinations
 *
 * For a given alphabet 'A', having length 'N' containing sorted and no
 * duplicate elements, combinations are build in lexoicographical order using
 * Lehmer code.
 * Then given a combination 'X' it's index can be returned.
 * Also given a index and length of combination, combination can be returned.
 *
 * For more information on this issue visit
 * <a href="https://drive.google.com/a/seagate.com/file/d/0BxJP-hCBgo5OcU9td2Fhc2xHek0/view"> here </a>
 */

/**
 * Returns combination index of 'x' in combinations of alphabet 'A'.
 *
 * @param 'N' length of alphabet 'A' ordered elements.
 * @param 'K' lenght of combination.
 *
 * @pre  0 < K <= N
 * @pre  m0_forall(i, K, x[i] < N)
 */
M0_INTERNAL int m0_combination_index(int N, int K, int *x);

/**
 * Returns the combination array 'x' for a given combination index.
 *
 * @param cid     Combination index.
 * @param N       Length of alphabet (total number of elements).
 * @param K       Length of combination (# of elements in the combination).
 * @param[out] x  Indices of elements, represented by the combination.
 *
 * @note  Output array 'x' is provided by user. Its capacity must not be
 *        less than K elements.
 *
 * @pre  0 < K <= N
 */
M0_INTERNAL void m0_combination_inverse(int cid, int N, int K, int *x);

/**
 * Factorial of n.
 */
M0_INTERNAL uint64_t m0_fact(uint64_t n);

/**
 * The number of possible combinations that can be obtained by taking
 * a sub-set of `r' items from a larger set of `n' elements.
 *
 * @pre  n >= r
 *
 * @see http://www.calculatorsoup.com/calculators/discretemathematics/combinations.php
 */
M0_INTERNAL uint32_t m0_ncr(uint64_t n, uint64_t r);

/** @} end of comb group */
#endif /* __MOTR_LIB_COMBINATIONS_H__ */

/*
 *  Local variables:
 *  c-indentation-style: "K&R"
 *  c-basic-offset: 8
 *  tab-width: 8
 *  fill-column: 80
 *  scroll-step: 1
 *  End:
 */
