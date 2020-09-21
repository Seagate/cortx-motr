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

#ifndef __MOTR_LIB_USER_SPACE_TIME_H__
#define __MOTR_LIB_USER_SPACE_TIME_H__

#include <time.h>

/**
 * Clock sources for m0_time_now(). @see m0_time_now()
 * @note Be sure to change m0_semaphore and m0_timer implementations
 * after changing CLOCK_SOURCES list.
 * @see man 3p clock_gettime
 * @see timer_posix_set(), m0_semaphore_timeddown(), m0_time_now(),
 *	m0_time_to_realtime().
 */
enum CLOCK_SOURCES {
	M0_CLOCK_SOURCE_REALTIME = CLOCK_REALTIME,
	M0_CLOCK_SOURCE_MONOTONIC = CLOCK_MONOTONIC,
	/** @note POSIX timers on Linux don't support this clock source */
	M0_CLOCK_SOURCE_MONOTONIC_RAW = CLOCK_MONOTONIC_RAW,
	/** gettimeofday(). All others clock sources use clock_gettime() */
	M0_CLOCK_SOURCE_GTOD,
	/** CLOCK_REALTIME + CLOCK_MONOTONIC combination.
	 *  @see m0_utime_init() */
	M0_CLOCK_SOURCE_REALTIME_MONOTONIC,
};

/* __MOTR_LIB_USER_SPACE_TIME_H__ */
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
