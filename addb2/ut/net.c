/*
 * Copyright (c) 2015-2020 Seagate Technology LLC and/or its Affiliates
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


#define M0_TRACE_SUBSYSTEM M0_TRACE_SUBSYS_UT

#include "ut/ut.h"
#include "addb2/net.h"

static struct m0_addb2_net *net;

static bool stopped;
static void stop_callback(struct m0_addb2_net *n, void *datum)
{
	M0_UT_ASSERT(!stopped);
	M0_UT_ASSERT(n == net);
	M0_UT_ASSERT(datum == &stopped);
	stopped = true;
}

/**
 * "net-init-fini" test: initialise and finalise network machine.
 */
static void net_init_fini(void)
{
	stopped = false;
	net = m0_addb2_net_init();
	M0_UT_ASSERT(net != NULL);
	m0_addb2_net_tick(net);
	m0_addb2_net_stop(net, &stop_callback, &stopped);
	M0_UT_ASSERT(stopped);
	m0_addb2_net_fini(net);
}

struct m0_ut_suite addb2_net_ut = {
	.ts_name = "addb2-net",
	.ts_init = NULL,
	.ts_fini = NULL,
	.ts_tests = {
		{ "net-init-fini",               &net_init_fini },
		{ NULL, NULL }
	}
};

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
