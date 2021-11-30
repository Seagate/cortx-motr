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


#pragma once

#ifndef __MOTR_LIB_TIME_H__
#define __MOTR_LIB_TIME_H__

#include "lib/types.h"

/**
   @defgroup time Generic time manipulation

   M0 time delivers resolution in nanoseconds. It is an unsigned 64-bit integer.
   @{
 */

typedef uint64_t m0_time_t;

enum {
	M0_TIME_ONE_SECOND = 1000000000ULL,
	M0_TIME_ONE_MSEC    = M0_TIME_ONE_SECOND / 1000,
};

#define TIME_F "[%" PRIu64 ":%09" PRIu64 "]"
#define TIME_P(t) m0_time_seconds(t), m0_time_nanoseconds(t)

/**
   Special value of abs_timeout indicates that action should be performed
   immediately
 */
extern const m0_time_t M0_TIME_IMMEDIATELY;
/** The largest time that is never reached in system life. */
extern const m0_time_t M0_TIME_NEVER;

#ifdef __KERNEL__
#  include "lib/linux_kernel/time.h"
#else
#  include "lib/user_space/time.h"
#endif


/** Clock source for m0_time_now() */
extern const enum CLOCK_SOURCES M0_CLOCK_SOURCE;

/**
 * Offset for M0_CLOCK_SOURCE_REALTIME_MONOTONIC clock source.
 *  @see m0_utime_init()
 */
extern m0_time_t                m0_time_monotonic_offset;

/**
 * Useful for mutex/semaphore implementation. This function will translate
 * time from value obtained from m0_time_now() to value that can be used
 * with CLOCK_REALTIME-only functions such as sem_timedwait() and
 * pthread_mutex_timedlock().
 * @param time Time obtained from m0_time_now() and adjusted somehow if needed.
 * @return Converted time value.
 * @note In some cases this function will have 2 calls to clock_gettime().
 */
M0_INTERNAL m0_time_t m0_time_to_realtime(m0_time_t abs_time);

/** Create and return a m0_time_t from seconds and nanoseconds. */
m0_time_t m0_time(uint64_t secs, long ns);

/** Similar to m0_time(). To be used in initialisers. */
#define M0_MKTIME(secs, ns) \
	((m0_time_t)((uint64_t)(secs) * M0_TIME_ONE_SECOND + (uint64_t)(ns)))

/** Get the current time.  This may or may not relate to wall time. */
m0_time_t m0_time_now(void);

/**
   Create a m0_time_t initialised with seconds + nanosecond in the future.

   @param secs seconds from now
   @param ns nanoseconds from now

   @return The result time.
 */
m0_time_t m0_time_from_now(uint64_t secs, long ns);

/**
   Add t2 to t1 and return that result.

   @return The result time. If either t1 or t2 is M0_TIME_NEVER, the result
   is M0_TIME_NEVER.
 */
m0_time_t m0_time_add(const m0_time_t t1, const m0_time_t t2);

/**
   Subtract t2 from t1 and return that result.

   @return The result time. If t1 == M0_TIME_NEVER, M0_TIME_NEVER is returned.
   @pre t2 < M0_TIME_NEVER && t1 >= t2
 */
m0_time_t m0_time_sub(const m0_time_t t1, const m0_time_t t2);

/**
   Sleep for requested time. If interrupted, remaining time returned.

   @param req requested time to sleep
   @param rem [OUT] remaining time, NULL causes remaining time to be ignored.
   @return 0 means success. -1 means error. Remaining time is stored in rem.
 */
int m0_nanosleep(const m0_time_t req, m0_time_t *rem);

/** Get "second" part from the time. */
uint64_t m0_time_seconds(const m0_time_t time);

/** Get "nanosecond" part from the time. */
uint64_t m0_time_nanoseconds(const m0_time_t time);

bool m0_time_is_in_past(m0_time_t time);

/** @} end of time group */
#endif /* __MOTR_LIB_TIME_H__ */

/*
 *  Local variables:
 *  c-indentation-style: "K&R"
 *  c-basic-offset: 8
 *  tab-width: 8
 *  fill-column: 80
 *  scroll-step: 1
 *  End:
 */
