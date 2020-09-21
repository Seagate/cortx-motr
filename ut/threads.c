/* -*- C -*- */
/*
 * Copyright (c) 2014-2020 Seagate Technology LLC and/or its Affiliates
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


#include "ut/threads.h"

#include "lib/memory.h"	/* M0_ALLOC_ARR */
#include "lib/thread.h" /* m0_thread */

M0_INTERNAL void m0_ut_threads_start(struct m0_ut_threads_descr *descr,
				     int			 thread_nr,
				     void			*param_array,
				     size_t			 param_size)
{
	int rc;
	int i;

	M0_PRE(descr->utd_thread_nr == 0);
	descr->utd_thread_nr = thread_nr;

	M0_ALLOC_ARR(descr->utd_thread, descr->utd_thread_nr);
	M0_ASSERT(descr->utd_thread != NULL);

	for (i = 0; i < thread_nr; ++i) {
		rc = M0_THREAD_INIT(&descr->utd_thread[i],
				    void *, NULL,
				    descr->utd_thread_func,
				    param_array + i * param_size,
				    "ut_thread%d", i);
		M0_ASSERT(rc == 0);
	}
}

M0_INTERNAL void m0_ut_threads_stop(struct m0_ut_threads_descr *descr)
{
	int rc;
	int i;

	M0_PRE(descr->utd_thread_nr > 0);

	for (i = 0; i < descr->utd_thread_nr; ++i) {
		rc = m0_thread_join(&descr->utd_thread[i]);
		M0_ASSERT(rc == 0);
		m0_thread_fini(&descr->utd_thread[i]);
	}
	m0_free(descr->utd_thread);

	descr->utd_thread    = NULL;
	descr->utd_thread_nr = 0;
}


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
