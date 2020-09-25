/* -*- C -*- */
/*
 * Copyright (c) 2020 Seagate Technology LLC and/or its Affiliates
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

#if USE_SYSV_SEMAPHORE

#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/sem.h>

/**
 *  @addtogroup semaphore
 *
 * Implementation of m0_semaphore on top of SysV semaphores.
 *
 * @{
 */

M0_INTERNAL int m0_semaphore_init(struct m0_semaphore *semaphore,
				  unsigned value)
{
	int result;

	result = semget(IPC_PRIVATE, 1, 0777);
	if (result != -1) {
		semaphore->s_semid = result;
		result = semctl(semaphore->s_semid, 0, SETVAL,
				(union semun) { .val = value });
	} else
		result = M0_ERR(-errno);
	return result;
}

M0_INTERNAL void m0_semaphore_fini(struct m0_semaphore *semaphore)
{
	int result;

	result = semctl(semaphore->s_semid, 0, IPC_RMID);
	M0_ASSERT_INFO(result == 0, "result: %d errno: %d", result, errno);
}

M0_INTERNAL void m0_semaphore_down(struct m0_semaphore *semaphore)
{
	int result = semop(semaphore->s_semid, &(struct sembuf) {
			.sem_num = 0, .sem_op = -1 }, 1);
	M0_ASSERT_INFO(result == 0, "result: %d errno: %d", result, errno);
}

M0_INTERNAL void m0_semaphore_up(struct m0_semaphore *semaphore)
{
	int result = semop(semaphore->s_semid, &(struct sembuf) {
			.sem_num = 0, .sem_op = +1 }, 1);
	M0_ASSERT_INFO(result == 0, "result: %d errno: %d", result, errno);
}

M0_INTERNAL bool m0_semaphore_trydown(struct m0_semaphore *semaphore)
{
	int result = semop(semaphore->s_semid, &(struct sembuf) {
			.sem_num = 0, .sem_op = -1, .sem_flg = IPC_NOWAIT }, 1);
	M0_ASSERT_INFO(result == 0 || (result == -1 && errno == EAGAIN),
		       "result: %d errno: %d", result, errno);
	return result == 0;
}

M0_INTERNAL unsigned m0_semaphore_value(struct m0_semaphore *semaphore)
{
	int result = semctl(semaphore->s_semid, 0, GETVAL);
	M0_ASSERT_INFO(result >= 0, "result: %d errno: %d", result, errno);
	return result;
}

M0_INTERNAL bool m0_semaphore_timeddown(struct m0_semaphore *semaphore,
					const m0_time_t abs_timeout)
{
#if defined(M0_LINUX)
	m0_time_t abs_timeout_realtime = m0_time_to_realtime(abs_timeout);
	struct timespec ts = {
		.tv_sec  = m0_time_seconds(abs_timeout_realtime),
		.tv_nsec = m0_time_nanoseconds(abs_timeout_realtime)
	};
	int result = semtimedop(semaphore->s_semid, &(struct sembuf) {
			.sem_num = 0, .sem_op = -1 }, 1, &ts);
	M0_ASSERT_INFO(result == 0 || (result == -1 && errno == EAGAIN),
		       "result: %d errno: %d", result, errno);
	return result == 0;
#elif defined(M0_DARWIN)
	/*
	 * @todo XXX No semtimedop() on Darwin.
	 */
	m0_semaphore_down(semaphore);
	return true;
#endif
}

/* USE_SYSV_SEMAPHORE */
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
