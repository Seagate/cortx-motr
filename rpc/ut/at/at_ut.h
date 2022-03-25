/* -*- C -*- */
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


#pragma once

#ifndef __MOTR_RPC_UT_AT_AT_UT_H__
#define __MOTR_RPC_UT_AT_AT_UT_H__

#include "xcode/xcode_attr.h"
#include "lib/types.h"
#include "rpc/at.h"
#include "rpc/at_xc.h"

/* import */
struct m0_rpc_machine;

enum {
	DATA_PATTERN     = 0x0a,
	INLINE_LEN       = 70,
};

enum {
	/* Tests oriented on sending data to server. */
	AT_TEST_INLINE_SEND,
	AT_TEST_INBULK_SEND,

	/*
	 * Tests oriented on receiving data from server.
	 * Note, AT_TEST_INLINE_RECV should be the first.
	 */
	AT_TEST_INLINE_RECV,
	AT_TEST_INLINE_RECV_UNK,
	AT_TEST_INBULK_RECV_UNK,
	AT_TEST_INBULK_RECV,
};

M0_INTERNAL void atut__bufdata_alloc(struct m0_buf *buf, size_t size,
				     struct m0_rpc_machine *rmach);

struct atut__req {
	uint32_t             arq_test_id;
	struct m0_rpc_at_buf arq_buf;
} M0_XCA_RECORD M0_XCA_DOMAIN(rpc);

struct atut__rep {
	uint32_t             arp_rc;
	struct m0_rpc_at_buf arp_buf;
} M0_XCA_RECORD M0_XCA_DOMAIN(rpc);

#endif /* __MOTR_RPC_UT_AT_AT_UT_H__ */

/*
 *  Local variables:
 *  c-indentation-style: "K&R"
 *  c-basic-offset: 8
 *  tab-width: 8
 *  fill-column: 80
 *  scroll-step: 1
 *  End:
 */
/*
 * vim: tabstop=8 shiftwidth=8 noexpandtab textwidth=80 nowrap
 */
