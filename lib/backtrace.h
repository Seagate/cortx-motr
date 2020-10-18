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


#pragma once

#ifndef __MOTR_LIB_BACKTRACE_H__
#define __MOTR_LIB_BACKTRACE_H__

/**
 * @addtogroup assert
 *
 * @{
 */

enum { M0_BACKTRACE_DEPTH_MAX = 32 };

struct m0_backtrace {
	int   b_used;
	void *b_frame[M0_BACKTRACE_DEPTH_MAX];
};

M0_INTERNAL void m0_backtrace_fill(struct m0_backtrace *bt);
M0_INTERNAL void m0_bactrace_print(struct m0_backtrace *bt, int idx,
				   char *buf, int size);

/** @} end of assert group */

/* __MOTR_LIB_BACKTRACE_H__ */
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
