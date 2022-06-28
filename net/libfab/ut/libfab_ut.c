/* -*- C -*- */
/*
 * Copyright (c) 2022 Seagate Technology LLC and/or its Affiliates
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


#include "ut/ut.h"             /* m0_ut_suite */
#include "net/net.h"
#include "net/libfab/libfab.h"
#include "net/test/initfini.h" /* m0_net_test_init */
#include "net/test/network.h"
#include "lib/finject.h"

enum {
	UT_BUF_NR     = 1,
	UT_BUF_SIZE   = 4096,
	UT_EP_NR      = 100,
	UT_MAX_EP_LEN = 150,
};

static void ut_tm_cb(const struct m0_net_tm_event *ev)
{
}

static void ut_buf_cb(struct m0_net_test_network_ctx *ctx,
		      const uint32_t buf_index,
		      enum m0_net_queue_type q,
		      const struct m0_net_buffer_event *ev)
{
}

static const struct m0_net_tm_callbacks libfab_ut_tm_cb = {
	.ntc_event_cb = ut_tm_cb
};

static struct m0_net_test_network_buffer_callbacks libfab_ut_buf_cb = {
	.ntnbc_cb = {
		[M0_NET_QT_MSG_RECV]          = ut_buf_cb,
		[M0_NET_QT_MSG_SEND]          = ut_buf_cb,
		[M0_NET_QT_PASSIVE_BULK_RECV] = ut_buf_cb,
		[M0_NET_QT_PASSIVE_BULK_SEND] = ut_buf_cb,
		[M0_NET_QT_ACTIVE_BULK_RECV]  = ut_buf_cb,
		[M0_NET_QT_ACTIVE_BULK_SEND]  = ut_buf_cb,
	}
};

static int libfab_ut_init(void)
{
	m0_net_test_init();
	return 0;
}

static int libfab_ut_fini(void)
{
	m0_net_test_fini();
	return 0;
}

/**
 * This UT verifies that concurrent access to the array of fids passed to
 * fi_trywait() is done under mutex lock.
 */
void test_libfab_fi_trywait(void)
{
	static struct m0_net_test_network_cfg cfg;
	static struct m0_net_test_network_ctx ut_ctx;
	int                                   rc;
	char                                  ep[UT_MAX_EP_LEN];
	int                                   i;

	M0_SET0(&cfg);
	cfg.ntncfg_tm_cb         = libfab_ut_tm_cb;
	cfg.ntncfg_buf_cb        = libfab_ut_buf_cb;
	cfg.ntncfg_buf_size_ping = UT_BUF_SIZE;
	cfg.ntncfg_buf_ping_nr   = UT_BUF_NR;
	cfg.ntncfg_ep_max        = UT_EP_NR;
	cfg.ntncfg_timeouts      = m0_net_test_network_timeouts_never();

	rc = m0_net_test_network_ctx_init(&ut_ctx, &cfg, "0@lo:12345:42:3000");
	M0_UT_ASSERT(rc == 0);

	m0_fi_enable("libfab_poller", "fail-trywait");
	m0_nanosleep(M0_MKTIME(2,0), NULL);

	for (i = 0; i < UT_EP_NR; i++) {
		memset(&ep, 0, sizeof(ep));
		sprintf(ep, "0@lo:12345:42:%d", i);
		rc = m0_net_test_network_ep_add(&ut_ctx, ep);
		M0_UT_ASSERT(rc == i);
	}

	m0_fi_disable("libfab_poller", "fail-trywait");

	m0_net_test_network_ctx_fini(&ut_ctx);
}

struct m0_ut_suite m0_net_libfab_ut = {
	.ts_name = "libfab-ut",
	.ts_init = libfab_ut_init,
	.ts_fini = libfab_ut_fini,
	.ts_tests = {
		{ "libfab-fi-trywait", test_libfab_fi_trywait },
		{ NULL, NULL }
	}
};

/*
 *  Local variables:
 *  c-indentation-style: "K&R"
 *  c-basic-offset: 8
 *  tab-width: 8
 *  fill-column: 80
 *  scroll-step: 1
 *  End:
 */
