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

#include "lib/finject.h"
#include "ut/ut.h"

#include "stats/ut/stats_ut_svc.c"

static int stats_ut_init(void)
{
	return 0;
}

static int stats_ut_fini(void)
{
	return 0;
}

struct m0_ut_suite stats_ut = {
	.ts_name  = "stats-ut",
	.ts_init  = stats_ut_init,
	.ts_fini  = stats_ut_fini,
	.ts_tests = {
		{ "stats-svc-start-stop", stats_ut_svc_start_stop },
		{ "stats-svc-update-fom", stats_ut_svc_update_fom },
		{ "stats-svc-query-fom",  stats_ut_svc_query_fom },
		{ "stats-svc-query-api",  stats_svc_query_api },
		{ NULL,	NULL}
	}
};
M0_EXPORTED(stats_ut);

/*
 *  Local variables:
 *  c-indentation-style: "K&R"
 *  c-basic-offset: 8
 *  tab-width: 8
 *  fill-column: 80
 *  scroll-step: 1
 *  End:
 */
