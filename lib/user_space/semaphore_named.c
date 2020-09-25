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
#include "lib/types.h"      /* INT_MAX */
#include "lib/arith.h"      /* min_check */

/**
 * @addtogroup semaphore
 *
 * Implementation of m0_semaphore on top of named sem_t.
 *
 * @{
 */

#if USE_POSIX_NAMED_SEMAPHORE

M0_INTERNAL int m0_semaphore_init(struct m0_semaphore *semaphore,
				  unsigned value)
{
	char name[256];
	int  result;

	sprintf(name, "/motr-sem-%d@%p", m0_getpid(), semaphore);
	semaphore->s_sem = sem_open(name, O_CREAT|O_EXCL, 0700, value);
	if (semaphore->s_sem == SEM_FAILED)
		return M0_ERR(-errno);
	else
		return 0;
}

M0_INTERNAL void m0_semaphore_fini(struct m0_semaphore *semaphore)
{
	char name[256];
	int  result;

	sprintf(name, "/motr-sem-%d@%p", m0_getpid(), semaphore);
	result = sem_unlink(name);
	M0_ASSERT_INFO(result == 0, "result: %d errno: %d", esultc, errno);
}

/* USE_POSIX_NAMED_SEMAPHORE */
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
