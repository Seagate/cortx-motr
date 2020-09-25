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

#if defined(M0_LINUX)
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
	M0_CLOCK_SOURCE_GTOD = CLOCK_REALTIME + CLOCK_MONOTONIC +
				CLOCK_MONOTONIC_RAW,
	/** CLOCK_REALTIME + CLOCK_MONOTONIC combination.
	 *  @see m0_utime_init() */
	M0_CLOCK_SOURCE_REALTIME_MONOTONIC
};

#elif defined(M0_DARWIN)
/**
 * Darwin does not have clock_gettime(3). Alternatives are:
 *
 *     - gettimeofday(3): portable
 *
 *     - host_get_clock_service() + clock_get_time() (see
 *       https://dshil.github.io/blog/missed-os-x-clock-guide/).
 *
 * http://web.archive.org/web/20100501115556/http://le-depotoir.googlecode.com:80/svn/trunk/misc/clock_gettime_stub.c
 *
 * http://web.archive.org/web/20100517095152/http://www.wand.net.nz/~smr26/wordpress/2009/01/19/monotonic-time-in-mac-os-x/comment-page-1/
 */
enum CLOCK_SOURCES {
	M0_CLOCK_SOURCE_REALTIME,
	M0_CLOCK_SOURCE_MONOTONIC,
	M0_CLOCK_SOURCE_MONOTONIC_RAW,
	M0_CLOCK_SOURCE_GTOD,
	M0_CLOCK_SOURCE_REALTIME_MONOTONIC
};
#endif

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
