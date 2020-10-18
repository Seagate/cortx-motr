/* -*- C -*- */
/*
 * Copyright (c) 2020 Seagate Technology LLC and/or its Affiliates
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

#include "lib/backtrace.h"

#ifdef HAVE_BACKTRACE
#include <execinfo.h>		/* backtrace */
#endif
#include <stdlib.h>             /* free */

/**
 * @addtogroup assert
 *
 * @{
 */

#if defined(__KERNEL__) || !defined(HAVE_BACKTRACE)
M0_INTERNAL void m0_backtrace_fill(struct m0_backtrace *bt)
{
	bt->b_frame[0] = __builtin_return_address(0);
	bt->b_frame[1] = __builtin_return_address(1);
	bt->b_frame[2] = __builtin_return_address(2);
	bt->b_frame[3] = __builtin_return_address(3);
	bt->b_frame[4] = __builtin_return_address(4);
	bt->b_used = 5;
}

M0_INTERNAL char *m0_bactrace_print(struct m0_backtrace *bt, int idx,
				    char *buf, int size)
{
	M0_PRE(idx < bt->b_used);
	snprintf(buf, size, "%p", bt->b_frame[idx]);
}

#else

M0_INTERNAL void m0_backtrace_fill(struct m0_backtrace *bt)
{
	bt->b_used = backtrace(bt->b_frame, ARRAY_SIZE(bt->b_frame));
}

M0_INTERNAL void m0_bactrace_print(struct m0_backtrace *bt, int idx,
				   char *buf, int size)
{
	char **symbols;

	M0_PRE(idx < bt->b_used);
	symbols = backtrace_symbols(bt->b_frame, bt->b_used);
	strncpy(buf, symbols != NULL ? symbols[idx] : "ENOMEM", size - 1);
	free(symbols);
}

#endif

/** @} end of assert group */
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
