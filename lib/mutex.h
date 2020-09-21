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

#ifndef __MOTR_LIB_MUTEX_H__
#define __MOTR_LIB_MUTEX_H__

#include "lib/types.h"
#include "addb2/histogram.h"

/**
   @defgroup mutex Mutual exclusion synchronisation object
   @{
*/

#ifndef __KERNEL__
#include "lib/user_space/mutex.h"
#else
#include "lib/linux_kernel/mutex.h"
#endif

/* struct m0_arch_mutex is defined by headers above. */

struct m0_mutex_addb2;
struct m0_thread;

struct m0_mutex {
	struct m0_arch_mutex   m_arch;
	struct m0_thread      *m_owner;
	struct m0_mutex_addb2 *m_addb2;
};

/**
 * Mutex static initialiser.
 *
 * @code
 * static struct m0_mutex lock = M0_MUTEX_SINIT(&lock);
 * @endcode
 *
 * This macro is useful only for global static mutexes, in other cases
 * m0_mutex_init() should be used.
 */
#define M0_MUTEX_SINIT(m) { .m_arch = M0_ARCH_MUTEX_SINIT((m)->m_arch) }

M0_INTERNAL void m0_mutex_init(struct m0_mutex *mutex);
M0_INTERNAL void m0_mutex_fini(struct m0_mutex *mutex);

/**
   Returns with the mutex locked.

   @pre  m0_mutex_is_not_locked(mutex)
   @post m0_mutex_is_locked(mutex)
*/
M0_INTERNAL void m0_mutex_lock(struct m0_mutex *mutex);

/**
   Unlocks the mutex.

   @pre  m0_mutex_is_locked(mutex)
   @post m0_mutex_is_not_locked(mutex)
*/
M0_INTERNAL void m0_mutex_unlock(struct m0_mutex *mutex);

/**
   Try to take a mutex lock.
   Returns 0 with the mutex locked,
   or non-zero if lock is already hold by others.
*/
M0_INTERNAL int m0_mutex_trylock(struct m0_mutex *mutex);


/**
   True iff mutex is locked by the calling thread.

   @note this function can be used only in assertions.
*/
M0_INTERNAL bool m0_mutex_is_locked(const struct m0_mutex *mutex);

/**
   True iff mutex is not locked by the calling thread.

   @note this function can be used only in assertions.
*/
M0_INTERNAL bool m0_mutex_is_not_locked(const struct m0_mutex *mutex);

struct m0_mutex_addb2 {
	m0_time_t            ma_taken;
	struct m0_addb2_hist ma_hold;
	struct m0_addb2_hist ma_wait;
	uint64_t             ma_id;
};

/*
 * Arch functions, implemented in lib/$ARCH/?mutex.c.
 */

M0_INTERNAL void m0_arch_mutex_init   (struct m0_arch_mutex *mutex);
M0_INTERNAL void m0_arch_mutex_fini   (struct m0_arch_mutex *mutex);
M0_INTERNAL void m0_arch_mutex_lock   (struct m0_arch_mutex *mutex);
M0_INTERNAL void m0_arch_mutex_unlock (struct m0_arch_mutex *mutex);
M0_INTERNAL int  m0_arch_mutex_trylock(struct m0_arch_mutex *mutex);

/** @} end of mutex group */

/* __MOTR_LIB_MUTEX_H__ */
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
