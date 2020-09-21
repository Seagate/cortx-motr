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


#pragma once

#ifndef __MOTR_UT_MISC_H__
#define __MOTR_UT_MISC_H__

#include "lib/types.h"  /* uint64_t */
#include "lib/misc.h"   /* M0_SRC_PATH */

/**
 * @defgroup ut
 *
 * @{
 */

/**
 * Returns absolute path to given file in ut/ directory.
 * M0_UT_DIR is defined in ut/Makefile.sub.
 */
#define M0_UT_PATH(name) M0_SRC_PATH("ut/" name)

#define M0_UT_CONF_PROFILE     "<0x7000000000000001:0>"
#define M0_UT_CONF_PROFILE_BAD "<0x7000000000000000:999>" /* non-existent */
#define M0_UT_CONF_PROCESS     "<0x7200000000000001:5>"

/**
 * Random shuffles an array.
 * Uses seed parameter as the seed for RNG.
 *
 * @note It uses an UT-grade RNG.
 */
M0_INTERNAL void m0_ut_random_shuffle(uint64_t *arr,
				      uint64_t  nr,
				      uint64_t *seed);

/**
 * Gives an array with random values with the given sum.
 *
 * @pre nr > 0
 * @post m0_reduce(i, nr, 0, + arr[i]) == sum
 *
 * @note It uses an UT-grade RNG.
 */
M0_INTERNAL void m0_ut_random_arr_with_sum(uint64_t *arr,
					   uint64_t  nr,
					   uint64_t  sum,
					   uint64_t *seed);

/** @} end of ut group */
#endif /* __MOTR_UT_MISC_H__ */

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
