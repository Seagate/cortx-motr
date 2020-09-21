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


#include "lib/timer.h"
#include "lib/thread.h"         /* M0_THREAD_ENTER */

#include <linux/jiffies.h>	/* timespec_to_jiffies */
#include <linux/version.h>
/**
   @addtogroup timer

   <b>Implementation of m0_timer on top of Linux struct timer_list.</b>

   @{
*/

#if LINUX_VERSION_CODE >= KERNEL_VERSION(4,14,0)
static void timer_kernel_trampoline_callback(struct timer_list *tl)
#else
static void timer_kernel_trampoline_callback(unsigned long tl)
#endif
{

	struct m0_timer *timer = container_of((struct timer_list *)tl,
					       struct m0_timer, t_timer);
	struct m0_thread th    = { 0, };
	m0_thread_enter(&th, false);
	m0_timer_callback_execute(timer);
	m0_thread_leave();
}

static int timer_kernel_init(struct m0_timer	      *timer,
			     struct m0_timer_locality *loc)
{
	struct timer_list *tl = &timer->t_timer;

#if LINUX_VERSION_CODE >= KERNEL_VERSION(4,14,0)
	timer_setup(tl, timer_kernel_trampoline_callback, tl->flags);
#else
	init_timer(tl);
	tl->data     = (unsigned long)tl;
	tl->function = timer_kernel_trampoline_callback;
#endif
	return 0;
}

static void timer_kernel_fini(struct m0_timer *timer)
{
}

static void timer_kernel_start(struct m0_timer *timer)
{
	struct timespec ts;
	m0_time_t	expire = timer->t_expire;
	m0_time_t       now    = m0_time_now();

	expire = expire > now ? m0_time_sub(expire, now) : 0;
	ts.tv_sec  = m0_time_seconds(expire);
	ts.tv_nsec = m0_time_nanoseconds(expire);
	timer->t_timer.expires = jiffies + timespec_to_jiffies(&ts);

	add_timer(&timer->t_timer);
}

static void timer_kernel_stop(struct m0_timer *timer)
{
	/*
	 * This function returns whether it has deactivated
	 * a pending timer or not. It always successful.
	 */
	del_timer_sync(&timer->t_timer);
}

M0_INTERNAL const struct m0_timer_operations m0_timer_ops[] = {
	[M0_TIMER_SOFT] = {
		.tmr_init  = timer_kernel_init,
		.tmr_fini  = timer_kernel_fini,
		.tmr_start = timer_kernel_start,
		.tmr_stop  = timer_kernel_stop,
	},
	[M0_TIMER_HARD] = {
		.tmr_init  = timer_kernel_init,
		.tmr_fini  = timer_kernel_fini,
		.tmr_start = timer_kernel_start,
		.tmr_stop  = timer_kernel_stop,
	},
};

/** @} end of timer group */

/*
 *  Local variables:
 *  c-indentation-style: "K&R"
 *  c-basic-offset: 8
 *  tab-width: 8
 *  fill-column: 80
 *  scroll-step: 1
 *  End:
 */
