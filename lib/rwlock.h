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

#ifndef __MOTR_LIB_RWLOCK_H__
#define __MOTR_LIB_RWLOCK_H__


/**
   @defgroup rwlock Read-write lock
   @{
 */

#ifndef __KERNEL__
#include "lib/user_space/rwlock.h"
#else
#include "lib/linux_kernel/rwlock.h"
#endif

/**
   read-write lock constructor
 */
M0_INTERNAL void m0_rwlock_init(struct m0_rwlock *lock);

/**
   read-write lock destructor
 */
M0_INTERNAL void m0_rwlock_fini(struct m0_rwlock *lock);

/**
   take exclusive lock
 */
M0_INTERNAL void m0_rwlock_write_lock(struct m0_rwlock *lock);
/**
   release exclusive lock
 */
M0_INTERNAL void m0_rwlock_write_unlock(struct m0_rwlock *lock);

/**
   take shared lock
 */
void m0_rwlock_read_lock(struct m0_rwlock *lock);
/**
   release shared lock
 */
void m0_rwlock_read_unlock(struct m0_rwlock *lock);


/* bool m0_rwlock_write_trylock(struct m0_rwlock *lock); */
/* bool m0_rwlock_read_trylock(struct m0_rwlock *lock); */

/** @} end of rwlock group */

/* __MOTR_LIB_RWLOCK_H__ */
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
