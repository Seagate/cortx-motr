/* -*- C -*- */
/*
 * Copyright (c) 2012-2020 Seagate Technology LLC and/or its Affiliates
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

#ifndef __MOTR_RPC_SESSION_INT_H__
#define __MOTR_RPC_SESSION_INT_H__

#include "rpc/session.h"

/**
   @addtogroup rpc_session

   @{
 */

/* Imports */
struct m0_rpc_item;

enum {
	/** [conn|session]_[create|terminate] items go on session 0 */
	SESSION_ID_0             = 0,
	SESSION_ID_INVALID       = UINT64_MAX,
	/** Range of valid session ids */
	SESSION_ID_MIN           = SESSION_ID_0 + 1,
	SESSION_ID_MAX           = SESSION_ID_INVALID - 1,
};

/**
   checks internal consistency of session
 */
M0_INTERNAL bool m0_rpc_session_invariant(const struct m0_rpc_session *session);

/**
   Holds a session in BUSY state.
   Every call to m0_rpc_session_hold_busy() must accompany
   call to m0_rpc_session_release()

   @pre M0_IN(session_state(session), (M0_RPC_SESSION_IDLE,
				       M0_RPC_SESSION_BUSY))
   @pre m0_rpc_machine_is_locked(session_machine(session))
   @post session_state(session) == M0_RPC_SESSION_BUSY
 */
M0_INTERNAL void m0_rpc_session_hold_busy(struct m0_rpc_session *session);

/**
   Decrements hold count. Moves session to IDLE state if it becomes idle.

   @pre session_state(session) == M0_RPC_SESSION_BUSY
   @pre session->s_hold_cnt > 0
   @pre m0_rpc_machine_is_locked(session_machine(session))
   @post ergo(m0_rpc_session_is_idle(session),
	      session_state(session) == M0_RPC_SESSION_IDLE)
 */
M0_INTERNAL void m0_rpc_session_release(struct m0_rpc_session *session);

M0_INTERNAL void session_state_set(struct m0_rpc_session *session, int state);
M0_INTERNAL int session_state(const struct m0_rpc_session *session);

M0_INTERNAL int m0_rpc_session_init_locked(struct m0_rpc_session *session,
					   struct m0_rpc_conn *conn);
M0_INTERNAL void m0_rpc_session_fini_locked(struct m0_rpc_session *session);

/**
   Terminates receiver end of session.

   @pre session->s_state == M0_RPC_SESSION_IDLE
   @post ergo(result == 0, session->s_state == M0_RPC_SESSION_TERMINATED)
   @post ergo(result != 0 && session->s_rc != 0, session->s_state ==
	      M0_RPC_SESSION_FAILED)
 */
M0_INTERNAL int m0_rpc_rcv_session_terminate(struct m0_rpc_session *session);

/**
   Callback routine called through item->ri_ops->rio_replied().

   The routine is executed when reply to session create fop is received
 */
M0_INTERNAL void m0_rpc_session_establish_reply_received(struct m0_rpc_item
							 *req);

/**
   Callback routine called through item->ri_ops->rio_replied().

   The routine is executed when reply to session terminate fop is received
 */
M0_INTERNAL void m0_rpc_session_terminate_reply_received(struct m0_rpc_item
							 *req);

M0_INTERNAL bool m0_rpc_session_is_idle(const struct m0_rpc_session *session);

M0_INTERNAL void m0_rpc_session_item_failed(struct m0_rpc_item *item);

M0_INTERNAL struct m0_rpc_machine *session_machine(const struct m0_rpc_session
						   *s);

M0_TL_DESCR_DECLARE(rpc_session, M0_EXTERN);
M0_TL_DECLARE(rpc_session, M0_INTERNAL, struct m0_rpc_session);

/** @}  End of rpc_session group */
#endif

/*
 *  Local variables:
 *  c-indentation-style: "K&R"
 *  c-basic-offset: 8
 *  tab-width: 8
 *  fill-column: 80
 *  scroll-step: 1
 *  End:
 */
