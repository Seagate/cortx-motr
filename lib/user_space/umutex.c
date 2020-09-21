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


#include "lib/misc.h"   /* M0_SET0 */
#include "lib/mutex.h"
#include "lib/assert.h"
#include "lib/errno.h"  /* EBUSY */

/**
   @addtogroup mutex

   Implementation of m0_arch_mutex on top of pthread_mutex_t.

   @{
*/

M0_INTERNAL void m0_arch_mutex_init(struct m0_arch_mutex *mutex)
{
	int rc;

	rc = pthread_mutex_init(&mutex->m_impl, NULL);
	M0_ASSERT_INFO(rc == 0, "rc=%d", rc);
}

M0_INTERNAL void m0_arch_mutex_fini(struct m0_arch_mutex *mutex)
{
	int rc;

	rc = pthread_mutex_destroy(&mutex->m_impl);
	M0_ASSERT_INFO(rc == 0, "rc=%d", rc);
}

M0_INTERNAL void m0_arch_mutex_lock(struct m0_arch_mutex *mutex)
{
	int rc;

	rc = pthread_mutex_lock(&mutex->m_impl);
	M0_ASSERT_INFO(rc == 0, "rc=%d", rc);
}

M0_INTERNAL void m0_arch_mutex_unlock(struct m0_arch_mutex *mutex)
{
	int rc;

	rc = pthread_mutex_unlock(&mutex->m_impl);
	M0_ASSERT_INFO(rc == 0, "rc=%d", rc);
}

M0_INTERNAL int m0_arch_mutex_trylock(struct m0_arch_mutex *mutex)
{
	int rc;

	rc = pthread_mutex_trylock(&mutex->m_impl);
	M0_ASSERT_INFO(M0_IN(rc, (0, EBUSY)), "rc=%d", rc);
	return rc;
}

/** @} end of mutex group */

/*
 *  Local variables:
 *  c-indentation-style: "K&R"
 *  c-basic-offset: 8
 *  tab-width: 8
 *  fill-column: 80
 *  scroll-step: 1
 *  End:
 */
