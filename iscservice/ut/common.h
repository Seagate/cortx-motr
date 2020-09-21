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

#ifndef __MOTR_ISCSERVICE_UT_COMMON_H__
#define __MOTR_ISCSERVICE_UT_COMMON_H__

#include "lib/thread.h"
#include "ut/ut.h"
#include "ut/misc.h"
enum {
	THR_NR = 5,
};

struct thr_args {
	void                *ta_data;
	struct m0_semaphore *ta_barrier;
	int                  ta_rc;
};

struct cnc_cntrl_block {
	struct m0_thread    ccb_threads[THR_NR];
	struct m0_semaphore ccb_barrier;
	struct thr_args     ccb_args[THR_NR];
};

M0_INTERNAL void cc_block_init(struct cnc_cntrl_block *cc_block, size_t size,
			       void (*t_data_init)(void *, int));

M0_INTERNAL void cc_block_launch(struct cnc_cntrl_block *cc_block,
				 void (*t_op)(void *));

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
