/* -*- C -*- */
/*
 * Copyright (c) 2017-2020 Seagate Technology LLC and/or its Affiliates
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

#ifndef __MOTR_RPC_CONN_POOL_H__
#define __MOTR_RPC_CONN_POOL_H__

#include "rpc/conn.h"
#include "rpc/session.h"
#include "rpc/rpc_machine.h"
#include "rpc/link.h"

struct m0_rpc_conn_pool;

struct m0_rpc_conn_pool_item {
	struct m0_rpc_link       cpi_rpc_link;
	struct m0_chan           cpi_chan;
	struct m0_clink          cpi_clink;
	int                      cpi_users_nr;
	bool                     cpi_connecting; /**< connect is in progress */
	struct m0_tlink          cpi_linkage;    /**< linkage for cp_items */
	struct m0_rpc_conn_pool *cpi_pool;       /**< conn pool ref */
	uint64_t                 cpi_magic;
};

struct m0_rpc_conn_pool {
	struct m0_tl             cp_items;
	struct m0_rpc_machine   *cp_rpc_mach;
	struct m0_mutex          cp_mutex;
	struct m0_mutex          cp_ch_mutex;
	m0_time_t                cp_timeout;
	uint64_t                 cp_max_rpcs_in_flight;
	struct m0_sm_ast         cp_ast;
};

M0_INTERNAL int m0_rpc_conn_pool_init(
	struct m0_rpc_conn_pool *pool,
	struct m0_rpc_machine   *rpc_mach,
	m0_time_t                conn_timeout,
	uint64_t                 max_rpcs_in_flight);

M0_INTERNAL void m0_rpc_conn_pool_fini(struct m0_rpc_conn_pool *pool);

M0_INTERNAL int m0_rpc_conn_pool_get_sync(
		struct m0_rpc_conn_pool *pool,
		const char              *remote_ep,
		struct m0_rpc_session   **session);

/**
 * @todo Potential race if connection is established before
 * clink is added to session channel.
 */
M0_INTERNAL int m0_rpc_conn_pool_get_async(
		struct m0_rpc_conn_pool *pool,
		const char              *remote_ep,
		struct m0_rpc_session   **session);

M0_INTERNAL void m0_rpc_conn_pool_put(
		struct m0_rpc_conn_pool *pool,
		struct m0_rpc_session   *session);

M0_INTERNAL
struct m0_chan *m0_rpc_conn_pool_session_chan(struct m0_rpc_session *session);

/**
 * @todo Unprotected access to ->sm_state in this function.
 */
M0_INTERNAL bool m0_rpc_conn_pool_session_established(
		struct m0_rpc_session *session);

/**
 * Destroy this rpc session from this pool.
 */
M0_INTERNAL void m0_rpc_conn_pool_destroy(struct m0_rpc_conn_pool *pool,
					  struct m0_rpc_session   *session);
#endif /* __MOTR_RPC_CONN_POOL_H__ */

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
