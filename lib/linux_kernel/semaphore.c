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


#include <linux/jiffies.h>  /* timespec_to_jiffies */
#include "lib/semaphore.h"
#include "lib/assert.h"

#define M0_TRACE_SUBSYSTEM M0_TRACE_SUBSYS_M0T1FS
#include "lib/trace.h"

/**
   @addtogroup semaphore

   <b>Implementation of m0_semaphore on top of Linux struct semaphore.</b>

   @{
 */

M0_INTERNAL int m0_semaphore_init(struct m0_semaphore *semaphore,
				  unsigned value)
{
	sema_init(&semaphore->s_sem, value);
	return 0;
}

M0_INTERNAL void m0_semaphore_fini(struct m0_semaphore *semaphore)
{
}

M0_INTERNAL void m0_semaphore_down(struct m0_semaphore *semaphore)
{
	int flag_was_set = 0;

	while (down_interruptible(&semaphore->s_sem) != 0)
		flag_was_set |= test_and_clear_thread_flag(TIF_SIGPENDING);

	if (flag_was_set)
		set_thread_flag(TIF_SIGPENDING);
}

M0_INTERNAL bool m0_semaphore_trydown(struct m0_semaphore *semaphore)
{
	return !down_trylock(&semaphore->s_sem);
}

M0_INTERNAL void m0_semaphore_up(struct m0_semaphore *semaphore)
{
	up(&semaphore->s_sem);
}

M0_INTERNAL bool m0_semaphore_timeddown(struct m0_semaphore *semaphore,
					const m0_time_t abs_timeout)
{
	m0_time_t       nowtime = m0_time_now();
	m0_time_t       abs_timeout_realtime = m0_time_to_realtime(abs_timeout);
	m0_time_t       reltime;
	unsigned long   reljiffies;
	struct timespec ts;

	/* same semantics as user_space semaphore: allow abs_time < now */
	if (abs_timeout_realtime > nowtime)
		reltime = m0_time_sub(abs_timeout_realtime, nowtime);
	else
		reltime = 0;
	ts.tv_sec  = m0_time_seconds(reltime);
	ts.tv_nsec = m0_time_nanoseconds(reltime);
	reljiffies = timespec_to_jiffies(&ts);
	return down_timeout(&semaphore->s_sem, reljiffies) == 0;
}

/** @} end of semaphore group */

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
