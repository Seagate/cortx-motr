/* -*- C -*- */
/*
 * Copyright (c) 2011-2020 Seagate Technology LLC and/or its Affiliates
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


#include "lib/rwlock.h"
#include "lib/assert.h"

/**
   @addtogroup rwlock Read-write lock

   @{
 */

M0_INTERNAL void m0_rwlock_init(struct m0_rwlock *lock)
{
	init_rwsem(&lock->rw_sem);
}

M0_INTERNAL void m0_rwlock_fini(struct m0_rwlock *lock)
{
	M0_ASSERT(!rwsem_is_locked(&lock->rw_sem));
}

M0_INTERNAL void m0_rwlock_write_lock(struct m0_rwlock *lock)
{
	down_write(&lock->rw_sem);
}

M0_INTERNAL void m0_rwlock_write_unlock(struct m0_rwlock *lock)
{
	up_write(&lock->rw_sem);
}

M0_INTERNAL void m0_rwlock_read_lock(struct m0_rwlock *lock)
{
	down_read(&lock->rw_sem);
}

M0_INTERNAL void m0_rwlock_read_unlock(struct m0_rwlock *lock)
{
	up_read(&lock->rw_sem);
}

/** @} end of rwlock group */

/*
 *  Local variables:
 *  c-indentation-style: "K&R"
 *  c-basic-offset: 8
 *  tab-width: 8
 *  fill-column: 80
 *  scroll-step: 1
 *  End:
 */
