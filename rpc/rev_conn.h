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

#ifndef __MOTR_RPC_REV_CONN_H__
#define __MOTR_RPC_REV_CONN_H__

#include "lib/chan.h"
#include "lib/tlist.h"
#include "rpc/link.h"

/**
   @defgroup rev_conn Reverse connection

   @{
 */

enum {
	M0_REV_CONN_TIMEOUT            = 5,
	M0_REV_CONN_MAX_RPCS_IN_FLIGHT = 1,
};

struct m0_reverse_connection {
	struct m0_rpc_link  rcf_rlink;
	struct m0_tlink     rcf_linkage;
	/* signalled when connection is terminated */
	struct m0_clink     rcf_disc_wait;
	struct m0_sm_ast    rcf_free_ast;
	uint64_t            rcf_magic;
};

/** @} end of rev_conn group */

#endif /* __MOTR_RPC_REV_CONN_H__ */


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
