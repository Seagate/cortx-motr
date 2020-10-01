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

#ifndef __MOTR_LIB_SEMAPHORE_H__
#define __MOTR_LIB_SEMAPHORE_H__

#include "lib/types.h"
#include "lib/time.h"  /* m0_time_t */

/**
   @defgroup semaphore Dijkstra semaphore

   @see http://en.wikipedia.org/wiki/Semaphore_(programming)

   @{
 */

#ifdef __KERNEL__
#  include "lib/linux_kernel/semaphore.h"
#else
#  include "lib/user_space/semaphore.h"
#endif

M0_INTERNAL int m0_semaphore_init(struct m0_semaphore *semaphore,
				  unsigned value);
M0_INTERNAL void m0_semaphore_fini(struct m0_semaphore *semaphore);

/** Downs the semaphore (P-operation). */
M0_INTERNAL void m0_semaphore_down(struct m0_semaphore *semaphore);

/** Ups the semaphore (V-operation). */
M0_INTERNAL void m0_semaphore_up(struct m0_semaphore *semaphore);

/**
   Tries to down a semaphore without blocking.

   Returns true iff the P-operation succeeded without blocking.
 */
M0_INTERNAL bool m0_semaphore_trydown(struct m0_semaphore *semaphore);

/**
 * Brings down the semaphore to 0.
 */
M0_INTERNAL void m0_semaphore_drain(struct m0_semaphore *semaphore);

/**
   Downs the semaphore, blocking for not longer than the (absolute) timeout
   given.

   @note this call with cause the thread to wait on semaphore in
         non-interruptable state: signals won't preempt it.
         Use it to wait for events that are expected to arrive in a
         "short time".

   @param abs_timeout absolute time since Epoch (00:00:00, 1 January 1970)
   @return true if P-operation succeed immediately or before timeout;
   @return false otherwise.

 */
M0_INTERNAL bool m0_semaphore_timeddown(struct m0_semaphore *semaphore,
					const m0_time_t abs_timeout);

/** @} end of semaphore group */
#endif /* __MOTR_LIB_SEMAPHORE_H__ */

/*
 *  Local variables:
 *  c-indentation-style: "K&R"
 *  c-basic-offset: 8
 *  tab-width: 8
 *  fill-column: 80
 *  scroll-step: 1
 *  End:
 */
