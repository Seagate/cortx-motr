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

#ifndef __MOTR_LIB_USER_SPACE_TIMER_H__
#define __MOTR_LIB_USER_SPACE_TIMER_H__

#include "lib/time.h"      /* m0_time_t */
#include "lib/thread.h"    /* m0_thread */
#include "lib/semaphore.h" /* m0_semaphore */

#if defined(M0_DARWIN)
/* There are no POSIX timers on Darwin. */
typedef struct {} timer_t;
#endif

/**
   @addtogroup timer

   <b>User space timer.</b>
   @{
 */

struct m0_timer {
	/** Timer type: M0_TIMER_SOFT or M0_TIMER_HARD. */
	enum m0_timer_type  t_type;
	/** Timer triggers this callback. */
	m0_timer_callback_t t_callback;
	/** User data. It is passed to m0_timer::t_callback(). */
	unsigned long	    t_data;
	/** Expire time in future of this timer. */
	m0_time_t	    t_expire;
	/** Timer state.  Used in state changes checking. */
	enum m0_timer_state t_state;

	/** Semaphore for m0_timer_stop() and user callback synchronisation. */
	struct m0_semaphore t_cb_sync_sem;

	/** Soft timer working thread. */
	struct m0_thread    t_thread;
	/** Soft timer working thread sleeping semaphore. */
	struct m0_semaphore t_sleep_sem;
	/** Thread is stopped by m0_timer_fini(). */
	bool		    t_thread_stop;

	/** POSIX timer ID, returned by timer_create(). */
	timer_t		    t_ptimer;
	/**
	   Target thread ID for hard timer callback.
	   If it is 0 then signal will be sent to the process
	   but not to any specific thread.
	 */
	pid_t		    t_tid;

};

/**
   Item of threads ID list in locality.
   Used in the implementation of userspace hard timer.
 */
struct m0_timer_tid {
	pid_t		tt_tid;
	struct m0_tlink tt_linkage;
	uint64_t	tt_magic;
};

/**
   Timer locality.

   The signal for M0_TIMER_HARD timers will be delivered to a thread
   from the locality.
 */
struct m0_timer_locality {
	/** Lock for tlo_tids */
	struct m0_mutex tlo_lock;
	/** List of thread ID's, associated with this locality */
	struct m0_tl tlo_tids;
	/** ThreadID of next thread for round-robin timer thread selection */
	struct m0_timer_tid *tlo_rrtid;
};

M0_EXTERN const struct m0_timer_operations m0_timer_ops[];

M0_INTERNAL int m0_timers_init(void);
M0_INTERNAL void m0_timers_fini(void);

/** @} end of timer group */

/* __MOTR_LIB_USER_SPACE_TIMER_H__ */
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
