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
#include "lib/semaphore.h"
#include "lib/assert.h"
#include "lib/errno.h"

#if USE_GCD_SEMAPHORE

#include <dispatch/dispatch.h>
#include <stdlib.h>                           /* lldiv, lldiv_t */

/**
 *  @addtogroup semaphore
 *
 * Implementation of m0_semaphore on top of Grand Central Dispatch semaphores.
 *
 * @{
 */

M0_INTERNAL int m0_semaphore_init(struct m0_semaphore *semaphore,
				  unsigned value)
{
	semaphore->s_dsem = dispatch_semaphore_create(value);
	return semaphore->s_dsem != NULL ? 0 : M0_ERR(-ENOMEM);
}

M0_INTERNAL void m0_semaphore_fini(struct m0_semaphore *semaphore)
{
	dispatch_release(semaphore->s_dsem);
}

M0_INTERNAL void m0_semaphore_down(struct m0_semaphore *semaphore)
{
	int result = dispatch_semaphore_wait(semaphore->s_dsem,
					     DISPATCH_TIME_FOREVER);
	M0_ASSERT_INFO(result == 0, "result: %d errno: %d", result, errno);
}

M0_INTERNAL void m0_semaphore_up(struct m0_semaphore *semaphore)
{
	dispatch_semaphore_signal(semaphore->s_dsem);
}

M0_INTERNAL bool m0_semaphore_trydown(struct m0_semaphore *semaphore)
{
	int result = dispatch_semaphore_wait(semaphore->s_dsem,
					     DISPATCH_TIME_NOW);
	return result == 0;
}

M0_INTERNAL bool m0_semaphore_timeddown(struct m0_semaphore *semaphore,
					const m0_time_t timeout)
{
	dispatch_time_t time;

	if (timeout == M0_TIME_IMMEDIATELY)
		time = DISPATCH_TIME_NOW;
	else if (timeout == M0_TIME_NEVER)
		time = DISPATCH_TIME_FOREVER;
	else {
		lldiv_t qr = lldiv(timeout, M0_TIME_ONE_SECOND);
		struct timespec ts = {
			.tv_sec  = qr.quot,
			.tv_nsec = qr.rem
		};
		time = dispatch_walltime(&ts, 0);
	}
	return dispatch_semaphore_wait(semaphore->s_dsem, time) == 0;
}

/* USE_GCD_SEMAPHORE */
#endif

/** @} end of semaphore group */

/*
 *  Local variables:
 *  c-indentation-style: "K&R"
 *  c-basic-offset: 8
 *  tab-width: 8
 *  fill-column: 80
 *  scroll-step: 1
 *  End:
 */
