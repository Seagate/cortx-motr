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

#ifndef __MOTR_LIB_USER_SPACE_SEMAPHORE_H__
#define __MOTR_LIB_USER_SPACE_SEMAPHORE_H__

#if defined(M0_LINUX)
#define USE_SYSV_SEMAPHORE        (0)
#define USE_POSIX_SEMAPHORE       (1)
#define USE_POSIX_NAMED_SEMAPHORE (0)
#define USE_GCD_SEMAPHORE         (0)
#elif defined(M0_DARWIN)
#define USE_SYSV_SEMAPHORE        (0)
#define USE_POSIX_SEMAPHORE       (0)
#define USE_POSIX_NAMED_SEMAPHORE (0)
#define USE_GCD_SEMAPHORE         (1)
#else
#error Wrong platform.
#endif

#if USE_SYSV_SEMAPHORE
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/sem.h>
#elif USE_GCD_SEMAPHORE
#include <dispatch/dispatch.h>
#else
#include <semaphore.h>
#endif

/**
   @addtogroup semaphore

   User space semaphore.
   @{
*/

struct m0_semaphore {
#if USE_SYSV_SEMAPHORE
	/** SysV semaphore set identifier, see semget(2). */
	int s_semid;
#elif USE_GCD_SEMAPHORE
	dispatch_semaphore_t s_dsem;
#else
	/** POSIX semaphore. */
	sem_t s_sem;
#endif
};

/** @} end of semaphore group */

/* __MOTR_LIB_USER_SPACE_SEMAPHORE_H__ */
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
