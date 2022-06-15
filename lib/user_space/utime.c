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


#include "lib/time.h"            /* m0_time_t */
#include "lib/time_internal.h"   /* m0_clock_gettime_wrapper */

#include "lib/assert.h"          /* M0_ASSERT */
#include "lib/misc.h"            /* M0_IN */
#include "lib/errno.h"           /* ENOSYS */

#include <sys/time.h>            /* gettimeofday */
#include <time.h>                /* clock_gettime */

#define M0_TRACE_SUBSYSTEM M0_TRACE_SUBSYS_LIB
#include "lib/trace.h"

M0_INTERNAL m0_time_t m0_clock_gettime_wrapper(enum CLOCK_SOURCES clock_id)
{
	struct timespec tp;
	int             rc;

	rc = clock_gettime((clockid_t)clock_id, &tp);
	/* clock_gettime() can fail iff clock_id is invalid */
	M0_ASSERT(rc == 0);
	return M0_MKTIME(tp.tv_sec, tp.tv_nsec);
}

M0_INTERNAL m0_time_t m0_clock_gettimeofday_wrapper(void)
{
	struct timeval tv;
	int            rc;

	rc = gettimeofday(&tv, NULL);
	M0_ASSERT(rc == 0);
	return M0_MKTIME(tv.tv_sec, tv.tv_usec * 1000);
}

/** Sleep for requested time */
int m0_nanosleep(const m0_time_t req, m0_time_t *rem)
{
	struct timespec	reqts = {
		.tv_sec  = m0_time_seconds(req),
		.tv_nsec = m0_time_nanoseconds(req)
	};
	struct timespec remts;
	int		rc;

	rc = nanosleep(&reqts, &remts);
	if (rem != NULL)
		*rem = rc != 0 ? m0_time(remts.tv_sec, remts.tv_nsec) : 0;
	return rc != 0 ? M0_ERR(rc) : rc;
}
M0_EXPORTED(m0_nanosleep);

#undef M0_TRACE_SUBSYSTEM

/*
 *  Local variables:
 *  c-indentation-style: "K&R"
 *  c-basic-offset: 8
 *  tab-width: 8
 *  fill-column: 80
 *  scroll-step: 1
 *  End:
 */
