/* -*- C -*- */
/*
 * Copyright (c) 2013-2020 Seagate Technology LLC and/or its Affiliates
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

#include "lib/misc.h"		/* M0_SET0 */
#include "lib/assert.h"		/* M0_PRE */
#include "lib/thread.h"		/* m0_enter_awkward */

#define M0_TRACE_SUBSYSTEM M0_TRACE_SUBSYS_LIB
#include "lib/trace.h"

/**
   @addtogroup timer

   Implementation of m0_timer.

 */

M0_INTERNAL int m0_timer_init(struct m0_timer	       *timer,
			      enum m0_timer_type	type,
			      struct m0_timer_locality *loc,
			      m0_timer_callback_t	callback,
			      unsigned long		data)
{
	int rc;

	M0_PRE(callback != NULL);
	M0_PRE(M0_IN(type, (M0_TIMER_SOFT, M0_TIMER_HARD)));

	M0_SET0(timer);
	timer->t_magix    = M0_LIB_TIMER_MAGIC;
	timer->t_type     = type;
	timer->t_expire   = 0;
	timer->t_callback = callback;
	timer->t_data     = data;

	rc = m0_timer_ops[timer->t_type].tmr_init(timer, loc);

	if (rc == 0)
		timer->t_state = M0_TIMER_INITED;

	M0_LEAVE("%p, rc=%d", timer, rc);
	return rc;
}

M0_INTERNAL void m0_timer_fini(struct m0_timer *timer)
{
	M0_ENTRY("%p", timer);
	M0_PRE(M0_IN(timer->t_state, (M0_TIMER_STOPPED, M0_TIMER_INITED)));

	timer->t_magix = M0_LIB_TIMER_MAGIC;
	m0_timer_ops[timer->t_type].tmr_fini(timer);
	timer->t_state = M0_TIMER_UNINIT;
	M0_LEAVE("%p", timer);
}

M0_INTERNAL void m0_timer_start(struct m0_timer *timer,
				m0_time_t	 expire)
{
	M0_PRE(M0_IN(timer->t_state, (M0_TIMER_STOPPED, M0_TIMER_INITED)));

	timer->t_expire = expire;

	timer->t_state = M0_TIMER_RUNNING;
	m0_timer_ops[timer->t_type].tmr_start(timer);
}

M0_INTERNAL void m0_timer_stop(struct m0_timer *timer)
{
	M0_PRE(timer->t_state == M0_TIMER_RUNNING);

	m0_timer_ops[timer->t_type].tmr_stop(timer);
	timer->t_state = M0_TIMER_STOPPED;
}

M0_INTERNAL bool m0_timer_is_started(const struct m0_timer *timer)
{
	return timer->t_state == M0_TIMER_RUNNING;
}

M0_INTERNAL void m0_timer_callback_execute(struct m0_timer *timer)
{
	m0_enter_awkward();
	timer->t_callback(timer->t_data);
	m0_exit_awkward();
}

#undef M0_TRACE_SUBSYSTEM

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
