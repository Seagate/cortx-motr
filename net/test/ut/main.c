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


#include "ut/ut.h"		/* m0_ut_suite */

#include "net/test/initfini.h"	/* m0_net_test_init */

extern void m0_net_test_ringbuf_ut(void);

extern void m0_net_test_serialize_ut(void);

extern void m0_net_test_str_ut(void);

extern void m0_net_test_slist_ut(void);

extern void m0_net_test_stats_ut(void);
extern void m0_net_test_timestamp_ut(void);

extern void m0_net_test_service_ut(void);

extern void m0_net_test_network_ut_buf_desc(void);
extern void m0_net_test_network_ut_ping(void);
extern void m0_net_test_network_ut_bulk(void);

extern void m0_net_test_cmd_ut_single(void);
extern void m0_net_test_cmd_ut_multiple(void);
extern void m0_net_test_cmd_ut_multiple2(void);

extern void m0_net_test_client_server_stub_ut(void);
extern void m0_net_test_client_server_ping_ut(void);
extern void m0_net_test_client_server_bulk_ut(void);
extern void m0_net_test_xprt_dymanic_reg_dereg_ut(void);

static int net_test_fini(void)
{
	m0_net_test_fini();
	return 0;
}

struct m0_ut_suite m0_net_test_ut = {
	.ts_name = "net-test",
	.ts_init = m0_net_test_init,
	.ts_fini = net_test_fini,
	.ts_tests = {
		{ "ringbuf",		m0_net_test_ringbuf_ut		  },
		{ "serialize",		m0_net_test_serialize_ut	  },
		{ "str",		m0_net_test_str_ut		  },
		{ "slist",		m0_net_test_slist_ut		  },
		{ "stats",		m0_net_test_stats_ut		  },
		{ "timestamp",		m0_net_test_timestamp_ut	  },
		{ "service",		m0_net_test_service_ut		  },
		{ "network-buf-desc",	m0_net_test_network_ut_buf_desc	  },
		{ "network-ping",	m0_net_test_network_ut_ping	  },
		{ "network-bulk",	m0_net_test_network_ut_bulk	  },
		{ "cmd-single",		m0_net_test_cmd_ut_single	  },
		{ "cmd-multiple",	m0_net_test_cmd_ut_multiple	  },
		{ "cmd-multiple2",	m0_net_test_cmd_ut_multiple2	  },
		{ "client-server-stub",	m0_net_test_client_server_stub_ut },
		/* XXX temporarily disabled. See MOTR-2267 */
#if 0
		{ "client-server-ping",	m0_net_test_client_server_ping_ut },
#endif
		{ "client-server-bulk",	m0_net_test_client_server_bulk_ut },
		{ "xprt-dymanic-reg-dereg",	m0_net_test_xprt_dymanic_reg_dereg_ut },
		{ NULL,			NULL				  }
	}
};
M0_EXPORTED(m0_net_test_ut);

/*
 *  Local variables:
 *  c-indentation-style: "K&R"
 *  c-basic-offset: 8
 *  tab-width: 8
 *  fill-column: 79
 *  scroll-step: 1
 *  End:
 */
