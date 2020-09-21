/* -*- C -*- */
/*
 * Copyright (c) 2018-2020 Seagate Technology LLC and/or its Affiliates
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


#define M0_TRACE_SUBSYSTEM M0_TRACE_SUBSYS_ISCS
#include "lib/trace.h"

#include "iscservice/ut/common.h"
#include "lib/memory.h"

M0_INTERNAL void cc_block_init(struct cnc_cntrl_block *cc_block, size_t size,
			       void (*t_data_init)(void *, int))
{
	int rc;
	int i;

	M0_SET0(cc_block);
	rc = m0_semaphore_init(&cc_block->ccb_barrier, THR_NR);
	M0_UT_ASSERT(rc == 0);
	for (i = 0; i < THR_NR; ++i) {
		m0_semaphore_down(&cc_block->ccb_barrier);
		cc_block->ccb_args[i].ta_data = m0_alloc(size);
		M0_UT_ASSERT(cc_block->ccb_args[i].ta_data != NULL);
		t_data_init(cc_block->ccb_args[i].ta_data, i);
		cc_block->ccb_args[i].ta_barrier = &cc_block->ccb_barrier;
	}
}

M0_INTERNAL void cc_block_launch(struct cnc_cntrl_block *cc_block,
				 void (*t_op)(void *))
{
	int i;
	int rc;

	for (i = 0; i < THR_NR; ++i) {
		rc = M0_THREAD_INIT(&cc_block->ccb_threads[i],
				    void *, NULL, t_op,
				    (void *)&cc_block->ccb_args[i], "isc_thrd");
		M0_UT_ASSERT(rc == 0);
	}
	for (i = 0; i < THR_NR; ++i) {
		m0_thread_join(&cc_block->ccb_threads[i]);
		M0_UT_ASSERT(cc_block->ccb_args[i].ta_rc == 0);
		m0_free(cc_block->ccb_args[i].ta_data);
	}
}

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
/*
 * vim: tabstop=8 shiftwidth=8 noexpandtab textwidth=80 nowrap
 */
