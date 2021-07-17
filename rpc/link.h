/* -*- C -*- */
/*
 * Copyright (c) 2014-2021 Seagate Technology LLC and/or its Affiliates
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

#ifndef __MOTR_RPC_LINK_H__
#define __MOTR_RPC_LINK_H__

#include "fop/fom.h"
#include "lib/chan.h"
#include "lib/mutex.h"
#include "lib/time.h"     /* m0_time_t */
#include "lib/tlist.h"
#include "rpc/conn.h"
#include "rpc/session.h"

struct m0_rpc_machine;
struct m0_co_op;

/**
   @defgroup rpc_link RPC link

   @{
 */

struct m0_rpc_link {
	struct m0_rpc_conn     rlk_conn;
	struct m0_rpc_session  rlk_sess;
	int                    rlk_rc;
	/* private */
	struct m0_fom          rlk_fom;
	struct m0_fom_callback rlk_fomcb;
	struct m0_chan         rlk_wait;
	struct m0_mutex        rlk_wait_mutex;
	/**< `future'-alike object, provided by the user */
	struct m0_co_op       *rlk_op;
	m0_time_t              rlk_timeout;
	bool                   rlk_connected;
};

enum m0_rpc_link_states {
	/* Common */
	M0_RLS_INIT = M0_FOM_PHASE_INIT,
	M0_RLS_FINI = M0_FOM_PHASE_FINISH,
	M0_RLS_CONN_FAILURE,
	M0_RLS_SESS_FAILURE,
	/* Connect */
	M0_RLS_CONN_CONNECTING,
	M0_RLS_SESS_ESTABLISHING,
	/* Disconnect */
	M0_RLS_SESS_WAIT_IDLE,
	M0_RLS_SESS_TERMINATING,
	M0_RLS_CONN_TERMINATING,
};

M0_INTERNAL int  m0_rpc_link_module_init(void);
M0_INTERNAL void m0_rpc_link_module_fini(void);

/**
 * Initialises an rpc_link object.
 *
 * @param rlink      Rpc link object that encapsulates rpc_conn and rpc_session.
 * @param ep         End point.
 * @param timeout    Timeout for connection/session establishment/termination.
 */
M0_INTERNAL int m0_rpc_link_init(struct m0_rpc_link *rlink,
				 struct m0_rpc_machine *mach,
				 struct m0_fid *svc_fid,
				 const char *ep,
				 uint64_t max_rpcs_in_flight);
M0_INTERNAL void m0_rpc_link_fini(struct m0_rpc_link *rlink);
M0_INTERNAL void m0_rpc_link_reset(struct m0_rpc_link *rlink);

/**
 * Makes asynchronous rpc_conn and rpc_session establishing.
 *
 * @param wait_clink If not NULL, signalled when session is established. Must
 *		     be 'oneshot'.
 *
 * @param wait_op If not NULL, transited with m0_co_done() to DONE when link
 * gets connected.
 */
M0_INTERNAL void m0_rpc_link_connect_async(struct m0_rpc_link *rlink,
					   m0_time_t abs_timeout,
					   struct m0_clink *wait_clink,
					   struct m0_co_op *wait_op);
M0_INTERNAL int m0_rpc_link_connect_sync(struct m0_rpc_link *rlink,
					 m0_time_t abs_timeout);
/**
 * Makes asynchronous rpc_session and rpc_conn termination.
 *
 * @param wait_clink If not NULL, signalled when connection is terminated. Must
 *		     be 'oneshot'.
 *
 * @param wait_op If not NULL, transited with m0_co_done() to DONE when link
 * gets connected.
 */
M0_INTERNAL void m0_rpc_link_disconnect_async(struct m0_rpc_link *rlink,
					      m0_time_t abs_timeout,
					      struct m0_clink *wait_clink,
					      struct m0_co_op *wait_op);
M0_INTERNAL int m0_rpc_link_disconnect_sync(struct m0_rpc_link *rlink,
					    m0_time_t abs_timeout);

M0_INTERNAL bool m0_rpc_link_is_connected(const struct m0_rpc_link *rlink);
M0_INTERNAL const char *m0_rpc_link_end_point(const struct m0_rpc_link *rlink);

/** @} end of rpc_link group */

#endif /* __MOTR_RPC_LINK_H__ */

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
