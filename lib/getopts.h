/* -*- C -*- */
/*
 * Copyright (c) 2012-2020 Seagate Technology LLC and/or its Affiliates
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

#ifndef __MOTR_LIB_GETOPTS_H__
#define __MOTR_LIB_GETOPTS_H__

#include "lib/types.h"	/* m0_bcount_t */
#include "lib/time.h"	/* m0_time_t */

#ifndef __KERNEL__
#include "lib/user_space/getopts.h"
#endif

/**
   @addtogroup getopts
   @{
 */
extern const char M0_GETOPTS_DECIMAL_POINT;

/**
   Convert numerical argument, followed by a optional multiplier suffix, to an
   uint64_t value.  The numerical argument is expected in the format that
   strtoull(..., 0) can parse. The multiplier suffix should be a char
   from "bkmgKMG" string. The char matches factor which will be
   multiplied by numerical part of argument.

   Suffix char matches:
   - @b b = 512
   - @b k = 1024
   - @b m = 1024 * 1024
   - @b g = 1024 * 1024 * 1024
   - @b K = 1000
   - @b M = 1000 * 1000
   - @b G = 1000 * 1000 * 1000
 */
M0_INTERNAL int m0_bcount_get(const char *arg, m0_bcount_t *out);

/**
   Convert numerical argument, followed by a optional multiplier suffix, to an
   m0_time_t value.  The numerical argument is expected in the format
   "[integer].[integer]" or just "integer", where [integer] is optional integer
   value in format that strtoull(..., 10) can parse, and at least one integer
   should be present in the numerical argument. The multiplier suffix matches
   unit of time and should be a string from the following list.

   Suffix string matches:
   - empty string = a second
   - @b s = a second
   - @b ms = millisecond = 1/1000 of a second
   - @b us = microsecond = 1/1000'000 of a second
   - @b ns = nanosecond  = 1/1000'000'000 of a second

   @note M0_GETOPTS_DECIMAL_POINT is used as decimal point in numerical
   argument to this function.
 */
M0_INTERNAL int m0_time_get(const char *arg, m0_time_t * out);

/** @} end of getopts group */

/* __MOTR_LIB_GETOPTS_H__ */
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
