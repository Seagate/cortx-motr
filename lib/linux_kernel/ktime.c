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


#include "lib/time.h"           /* m0_time_t */
#include "lib/time_internal.h"  /* m0_clock_gettime_wrapper */
#include "lib/misc.h"           /* M0_EXPORTED */

#include <linux/module.h>
#include <linux/time.h>
#include <linux/jiffies.h>
#include <linux/sched.h>

/**
   @addtogroup time

   <b>Implementation of m0_time_t on top of kernel struct timespec

   @{
*/

M0_INTERNAL m0_time_t m0_clock_gettime_wrapper(enum CLOCK_SOURCES clock_id)
{
	struct timespec ts;
	m0_time_t       ret;

	switch (clock_id) {
	case M0_CLOCK_SOURCE_MONOTONIC:
		getrawmonotonic(&ts);
		ret = M0_MKTIME(ts.tv_sec, ts.tv_nsec);
		break;
	case M0_CLOCK_SOURCE_REALTIME:
		/* ts = current_kernel_time(); */
		getnstimeofday(&ts);
		ret = M0_MKTIME(ts.tv_sec, ts.tv_nsec);
		break;
	default:
		M0_IMPOSSIBLE("Unknown clock source");
		ret = M0_MKTIME(0, 0);
	}
	return ret;
}

M0_INTERNAL m0_time_t m0_clock_gettimeofday_wrapper(void)
{
	struct timespec ts;

	getnstimeofday(&ts);
	return M0_MKTIME(ts.tv_sec, ts.tv_nsec);
}

/**
   Sleep for requested time
*/
int m0_nanosleep(const m0_time_t req, m0_time_t *rem)
{
	struct timespec ts = {
		.tv_sec  = m0_time_seconds(req),
		.tv_nsec = m0_time_nanoseconds(req)
	};
	unsigned long	tj = timespec_to_jiffies(&ts);
	unsigned long	remtj;
	struct timespec remts;

	/* this may use schedule_timeout_interruptible() to capture signals */
	remtj = schedule_timeout_uninterruptible(tj);
	if (rem != NULL) {
		jiffies_to_timespec(remtj, &remts);
		*rem = m0_time(remts.tv_sec, remts.tv_nsec);
	}
	return remtj == 0 ? 0 : -1;
}
M0_EXPORTED(m0_nanosleep);

/** @} end of time group */

/*
 *  Local variables:
 *  c-indentation-style: "K&R"
 *  c-basic-offset: 8
 *  tab-width: 8
 *  fill-column: 80
 *  scroll-step: 1
 *  End:
 */
