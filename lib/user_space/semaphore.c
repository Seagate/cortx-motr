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

#define M0_TRACE_SUBSYSTEM M0_TRACE_SUBSYS_LIB
#include "lib/trace.h"
#include "lib/semaphore.h"
#include "lib/assert.h"
#include "lib/errno.h"
#include "lib/types.h"      /* INT_MAX */
#include "lib/arith.h"      /* min_check */

/**
   @addtogroup semaphore

   Implementation of m0_semaphore on top of sem_t.

   @{
*/

#if USE_POSIX_SEMAPHORE

M0_INTERNAL int m0_semaphore_init(struct m0_semaphore *semaphore,
				  unsigned value)
{
	int rc;
	int err;

	rc = sem_init(&semaphore->s_sem, 0, value);
	err = rc == 0 ? 0 : errno;
	M0_ASSERT_INFO(rc == 0, "rc=%d errno=%d", rc, err);
	return -err;
}

M0_INTERNAL void m0_semaphore_fini(struct m0_semaphore *semaphore)
{
	int rc;

	rc = sem_destroy(&semaphore->s_sem);
	M0_ASSERT_INFO(rc == 0, "rc=%d errno=%d", rc, errno);
}

/* USE_POSIX_SEMAPHORE */
#endif

#if USE_POSIX_SEMAPHORE || USE_POSIX_NAMED_SEMAPHORE

M0_INTERNAL void m0_semaphore_down(struct m0_semaphore *semaphore)
{
	int rc;

	do
		rc = sem_wait(&semaphore->s_sem);
	while (rc == -1 && errno == EINTR);
	M0_ASSERT_INFO(rc == 0, "rc=%d errno=%d", rc, errno);
}

M0_INTERNAL void m0_semaphore_up(struct m0_semaphore *semaphore)
{
	int rc;

	rc = sem_post(&semaphore->s_sem);
	M0_ASSERT_INFO(rc == 0, "rc=%d errno=%d", rc, errno);
}

M0_INTERNAL bool m0_semaphore_trydown(struct m0_semaphore *semaphore)
{
	int rc;

	do
		rc = sem_trywait(&semaphore->s_sem);
	while (rc == -1 && errno == EINTR);
	M0_ASSERT_INFO(rc == 0 || (rc == -1 && errno == EAGAIN),
	               "rc=%d errno=%d", rc, errno);
	errno = 0;
	return rc == 0;
}

M0_INTERNAL unsigned m0_semaphore_value(struct m0_semaphore *semaphore)
{
	int rc;
	int result;

	rc = sem_getvalue(&semaphore->s_sem, &result);
	M0_ASSERT_INFO(rc == 0, "rc=%d errno=%d", rc, errno);
	M0_POST(result >= 0);
	return result;
}

M0_INTERNAL bool m0_semaphore_timeddown(struct m0_semaphore *semaphore,
					const m0_time_t abs_timeout)
{
	m0_time_t	abs_timeout_realtime = m0_time_to_realtime(abs_timeout);
	struct timespec ts = {
			.tv_sec  = m0_time_seconds(abs_timeout_realtime),
			.tv_nsec = m0_time_nanoseconds(abs_timeout_realtime)
	};
	int		rc;

	/*
	 * Workaround for sem_timedwait(3) on Centos >= 7.2, which returns
	 * -ETIMEDOUT immediately if tv_sec is greater than
	 * gettimeofday(2) + INT_MAX.
	 *
	 *  For more information refer to:
	 *    https://bugzilla.redhat.com/show_bug.cgi?id=1412082
	 *    https://jts.seagate.com/browse/CASTOR-1990
	 *    `git blame` these lines and read commit message
	 *    doc/workarounds.md
	 *
	 *  It should be reverted when glibc is fixed in future RedHat releases.
	 */
	ts.tv_sec = min_check(ts.tv_sec, (time_t)(INT_MAX - 1));
	/* ----- end of workaround ----- */

	do
		rc = sem_timedwait(&semaphore->s_sem, &ts);
	while (rc == -1 && errno == EINTR);
	M0_ASSERT_INFO(rc == 0 || (rc == -1 && errno == ETIMEDOUT),
	               "rc=%d errno=%d", rc, errno);
	if (rc == -1 && errno == ETIMEDOUT)
		errno = 0;
	return rc == 0;
}

/* USE_POSIX_SEMAPHORE || USE_POSIX_NAMED_SEMAPHORE */
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
