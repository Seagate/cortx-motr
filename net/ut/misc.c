/*
 * Copyright (c) 2016-2020 Seagate Technology LLC and/or its Affiliates
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


#include "net/net.h"  /* m0_net_endpoint_is_valid */
#include "ut/ut.h"

#ifndef __KERNEL__
#include <unistd.h>
#endif

static void test_endpoint_is_valid(void)
{
	char hostname[M0_NET_IP_STRLEN_MAX] = {};
	char ep_str[M0_NET_IP_STRLEN_MAX]   = {};
	int  rc;

	const char *good[] = {
		"0@lo:12345:34:1",
		"172.18.1.1@tcp:12345:40:401",
		"255.0.0.0@tcp:12345:1:1",
		"172.18.50.40@o2ib:12345:34:1",
		"172.18.50.40@o2ib1:12345:34:1",
		"inet:tcp:127.0.0.1@3000",
		"inet:tcp:127.0.0.1@3001",
		"inet:tcp:localhost@3002",
		"inet:verbs:localhost@3003",
		ep_str,
	};
	const char *bad[] = {
		"",
		" 172.18.1.1@tcp:12345:40:401",
		"172.18.1.1@tcp:12345:40:401 ",
		"1@lo:12345:34:1",
		"172.16.64.1:12345:45:41",
		"256.0.0.0@tcp:12345:1:1",
		"172.18.50.40@o2ib:54321:34:1",
		"inet:abc:127.0.0.1@3000",
		"lnet:tcp:127.0.0.1@3001",
		"inet:tcp:127.0.0.1@67000",
	};

	rc = gethostname(hostname, sizeof(hostname)-1);
	M0_UT_ASSERT(rc == 0);
	sprintf(ep_str, "inet:tcp:%s@3004", hostname);

	M0_UT_ASSERT(m0_forall(i, ARRAY_SIZE(good),
			       m0_net_endpoint_is_valid(good[i])));
	M0_UT_ASSERT(m0_forall(i, ARRAY_SIZE(bad),
			       !m0_net_endpoint_is_valid(bad[i])));
}

struct m0_ut_suite m0_net_misc_ut = {
	.ts_name  = "net-misc-ut",
	.ts_tests = {
		{ "endpoint-is-valid", test_endpoint_is_valid },
		{ NULL, NULL }
	}
};
