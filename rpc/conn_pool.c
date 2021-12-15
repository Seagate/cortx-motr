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


#define M0_TRACE_SUBSYSTEM    M0_TRACE_SUBSYS_RPC
#include "lib/trace.h"

#include "lib/memory.h"
#include "lib/string.h"
#include "lib/errno.h"
#include "lib/finject.h"              /* M0_FI_ENABLED */
#include "rpc/session.h"
#include "rpc/link.h"
#include "rpc/conn_pool.h"
#include "rpc/conn_pool_internal.h"
#include "rpc/rpc_machine_internal.h" /* m0_rpc_chan */

M0_TL_DESCR_DEFINE(rpc_conn_pool_items,
		   "rpc cpi list",
		   M0_INTERNAL,
		   struct m0_rpc_conn_pool_item,
		   cpi_linkage, cpi_magic,
		   M0_RPC_CONN_POOL_ITEMS_MAGIC,
		   M0_RPC_CONN_POOL_ITEMS_HEAD_MAGIC);

M0_TL_DEFINE(rpc_conn_pool_items, M0_INTERNAL, struct m0_rpc_conn_pool_item);

static struct m0_rpc_conn_pool_item *find_item_by_ep(
		struct m0_rpc_conn_pool *pool,
		const char              *remote_ep)
{
	struct m0_rpc_conn_pool_item *ret = NULL;
	struct m0_rpc_conn_pool_item *pool_item;

	M0_ENTRY();
	M0_PRE(m0_mutex_is_locked(&pool->cp_mutex));
	m0_tl_for(rpc_conn_pool_items, &pool->cp_items, pool_item) {
		if (!strcmp(m0_rpc_conn_addr(&pool_item->cpi_rpc_link.rlk_conn),
			    remote_ep))
		{
			ret = pool_item;
			break;
		}
	} m0_tl_endfor;

	M0_LEAVE("ret %p", ret);

	return ret;
}

static struct m0_rpc_conn_pool_item *find_pool_item(
		struct m0_rpc_session *session)
{
	struct m0_rpc_conn_pool_item *ret;
	struct m0_rpc_link           *rpc_link;

	M0_ENTRY();

	rpc_link = container_of(session, struct m0_rpc_link, rlk_sess);
	ret      = container_of(rpc_link,
				struct m0_rpc_conn_pool_item, cpi_rpc_link);

	M0_LEAVE("ret %p", ret);

	return ret;
}

static bool pool_item_clink_cb(struct m0_clink *link)
{
	struct m0_rpc_conn_pool_item *pool_item;
	struct m0_rpc_conn_pool      *pool;

	M0_ENTRY("link %p", link);

	pool_item = container_of(link,
			struct m0_rpc_conn_pool_item, cpi_clink);
	m0_chan_broadcast_lock(&pool_item->cpi_chan);
	pool = pool_item->cpi_pool;
	m0_mutex_lock(&pool->cp_mutex);
	pool_item->cpi_connecting = false;
	m0_mutex_unlock(&pool->cp_mutex);

	M0_LEAVE();
	return true;
}

static int conn_pool_item_init(
		struct m0_rpc_conn_pool      *pool,
		struct m0_rpc_conn_pool_item *item,
		const char                   *remote_ep)
{
	int rc;

	M0_PRE(m0_mutex_is_locked(&pool->cp_mutex));

	rc = m0_rpc_link_init(&item->cpi_rpc_link, pool->cp_rpc_mach,
			      NULL, remote_ep, pool->cp_max_rpcs_in_flight);

	if (rc == 0) {
		m0_chan_init(&item->cpi_chan, &pool->cp_ch_mutex);

		m0_clink_init(&item->cpi_clink, pool_item_clink_cb);
		item->cpi_clink.cl_flags = M0_CF_ONESHOT;

		rpc_conn_pool_items_tlink_init_at_tail(item, &pool->cp_items);
		item->cpi_pool = pool;
	}

	return rc;
}

static void conn_pool_item_fini(struct m0_rpc_conn_pool_item *item)
{
	M0_PRE(item->cpi_pool != NULL);
	M0_PRE(m0_mutex_is_locked(&item->cpi_pool->cp_mutex));
	m0_clink_fini(&item->cpi_clink);
	m0_chan_fini_lock(&item->cpi_chan);
	m0_rpc_link_fini(&item->cpi_rpc_link);
	if (rpc_conn_pool_items_tlink_is_in(item))
		rpc_conn_pool_items_tlink_del_fini(item);
	m0_free(item);
}

static struct m0_rpc_conn_pool_item *conn_pool_item_get(
		struct m0_rpc_conn_pool *pool,
		const char              *remote_ep)
{
	struct m0_rpc_conn_pool_item *item;
	int                           rc;

	M0_PRE(m0_mutex_is_locked(&pool->cp_mutex));

	if (M0_FI_ENABLED("fail_conn_get"))
		return NULL;

	/*
	 * @todo Implement invariant function for pool and call
	 * here. (phase 2)
	 */
	M0_PRE(pool != NULL && pool->cp_rpc_mach != NULL);
	M0_ASSERT(remote_ep != NULL);

	M0_ENTRY("pool %p, remote_ep %s", pool, remote_ep);

	item = find_item_by_ep(pool, remote_ep);

	if (item == NULL) {
		/* Add next item */
		M0_ALLOC_PTR(item);

		if (item == NULL) {
			M0_LOG(M0_ERROR,
			       "Could not allocate new connection pool item");
			goto conn_pool_item_get_leave;
		}

		rc = conn_pool_item_init(pool, item, remote_ep);

		if (rc != 0) {
			m0_free(item);
			item = NULL;
		}
	}

	if (item != NULL) {
		/*
		 * @todo Is it necessary to do something
		 * when item->cpi_users_nr becomes zero?
		 */
		M0_CNT_INC(item->cpi_users_nr);
	}

conn_pool_item_get_leave:
	M0_LEAVE("item %p", item);

	return item;
}

M0_INTERNAL int m0_rpc_conn_pool_get_sync(
		struct m0_rpc_conn_pool  *pool,
		const char               *remote_ep,
		struct m0_rpc_session   **session)
{
	struct m0_clink                clink;
	int                            rc;

	rc = m0_rpc_conn_pool_get_async(
			pool,
			remote_ep,
			session);

	if (rc == -EBUSY) {
		m0_clink_init(&clink, NULL);
		clink.cl_flags = M0_CF_ONESHOT;
		m0_clink_add_lock(m0_rpc_conn_pool_session_chan(*session),
				  &clink);
		m0_chan_wait(&clink);
		m0_clink_fini(&clink);
		rc = find_pool_item(*session)->cpi_rpc_link.rlk_rc;
	}

	if (rc != 0 && *session) {
		M0_LOG(M0_ERROR,
			"conn error %s", remote_ep);
		m0_rpc_conn_pool_put(
				pool, *session);
	}

	return M0_RC(rc);
}

M0_INTERNAL int m0_rpc_conn_pool_get_async(
		struct m0_rpc_conn_pool  *pool,
		const char               *remote_ep,
		struct m0_rpc_session   **session)
{
	struct m0_rpc_conn_pool_item *item;
	int                           rc = 0;
	struct m0_rpc_link	     *rpc_link;

	if (M0_FI_ENABLED("fail_conn_get")) {
		*session = NULL;
		return -ENOMEM;
	}

	m0_mutex_lock(&pool->cp_mutex);
	M0_LOG(M0_DEBUG, "con pool mutex locked");

	item = conn_pool_item_get(pool, remote_ep);
	*session = (item != NULL) ? &item->cpi_rpc_link.rlk_sess : NULL;

	if (item != NULL) {
		if (item->cpi_connecting) {
			rc = -EBUSY;
		} else if (!m0_rpc_conn_pool_session_established(*session)) {
			/**
			 * @todo Looks like rpc link connect could not be
			 * called twice, even in case first attempt fails
			 * (phase 2).
			 */
			rpc_link = &item->cpi_rpc_link;
			if (rpc_link->rlk_connected) {
				m0_rpc_link_disconnect_sync(rpc_link,
					pool->cp_timeout == M0_TIME_NEVER ?
					    M0_TIME_NEVER :
					    m0_time_now() + pool->cp_timeout);
			}
			item->cpi_connecting = true;
			rpc_link->rlk_rc = 0;
			M0_LOG(M0_DEBUG, "ASYNC CONN to %s", remote_ep);

			m0_rpc_link_connect_async(
				rpc_link,
				pool->cp_timeout == M0_TIME_NEVER ?
					M0_TIME_NEVER :
					m0_time_now() + pool->cp_timeout,
				&item->cpi_clink);
			rc = -EBUSY;
		}
	} else {
		rc = -ENOMEM;
	}

	m0_mutex_unlock(&pool->cp_mutex);
	M0_LOG(M0_DEBUG, "con pool mutex unlocked");

	return M0_RC(rc);
}

M0_INTERNAL void m0_rpc_conn_pool_put(
		struct m0_rpc_conn_pool *pool,
	       	struct m0_rpc_session *session)
{
	M0_LOG(M0_DEBUG, "m0_rpc_conn_pool_put");

	m0_mutex_lock(&pool->cp_mutex);
	M0_LOG(M0_DEBUG, "con pool mutex locked");

	/* @todo Do we do anything when counter is zero? */
	M0_CNT_DEC(find_pool_item(session)->cpi_users_nr);

	m0_mutex_unlock(&pool->cp_mutex);
	M0_LOG(M0_DEBUG, "con pool mutex unlocked");
}

M0_INTERNAL struct m0_chan *m0_rpc_conn_pool_session_chan(
		struct m0_rpc_session    *session)
{
	return &find_pool_item(session)->cpi_chan;
}

M0_INTERNAL bool m0_rpc_conn_pool_session_established(
		struct m0_rpc_session    *session)
{
	struct m0_rpc_link            *rpc_link;
	uint32_t                       sess_state;

	rpc_link  = container_of(session, struct m0_rpc_link, rlk_sess);

	/* @todo Unprotected access to ->sm_state. */
	sess_state = rpc_link->rlk_sess.s_sm.sm_state;
	M0_LOG(M0_DEBUG, "session state %d", sess_state);

	return M0_IN(sess_state, (M0_RPC_SESSION_IDLE, M0_RPC_SESSION_BUSY));
}

M0_INTERNAL int m0_rpc_conn_pool_init(
		struct m0_rpc_conn_pool *pool,
		struct m0_rpc_machine   *rpc_mach,
		m0_time_t                conn_timeout,
		uint64_t                 max_rpcs_in_flight)
{
	M0_ENTRY("pool %p", pool);

	M0_ASSERT(rpc_mach != NULL);
	M0_PRE(M0_IS0(pool));

	pool->cp_rpc_mach            = rpc_mach;
	pool->cp_timeout             = conn_timeout;
	pool->cp_max_rpcs_in_flight  = max_rpcs_in_flight;
	m0_mutex_init(&pool->cp_mutex);
	m0_mutex_init(&pool->cp_ch_mutex);
	rpc_conn_pool_items_tlist_init(&pool->cp_items);
	return M0_RC(0);
}

M0_INTERNAL void m0_rpc_conn_pool_fini(struct m0_rpc_conn_pool *pool)
{
	struct m0_rpc_conn_pool_item  *item;

	M0_ENTRY();

	m0_mutex_lock(&pool->cp_mutex);
	M0_LOG(M0_DEBUG, "con pool mutex locked");

	m0_tl_for(rpc_conn_pool_items, &pool->cp_items, item) {
		if (m0_rpc_conn_pool_session_established(
				&item->cpi_rpc_link.rlk_sess)) {
			m0_rpc_link_disconnect_sync(
					&item->cpi_rpc_link,
					pool->cp_timeout == M0_TIME_NEVER ?
					    M0_TIME_NEVER :
					    m0_time_now() + pool->cp_timeout);
		}

		conn_pool_item_fini(item);
	} m0_tl_endfor;
	rpc_conn_pool_items_tlist_fini(&pool->cp_items);
	m0_mutex_unlock(&pool->cp_mutex);
	M0_LOG(M0_DEBUG, "con pool mutex unlocked");

	m0_mutex_fini(&pool->cp_mutex);
	m0_mutex_fini(&pool->cp_ch_mutex);

	M0_LEAVE();
}

static void pool_item_disconnected_ast(struct m0_sm_group *grp,
				       struct m0_sm_ast   *ast)
{
	struct m0_rpc_conn_pool_item *item = ast->sa_datum;

	M0_LOG(M0_DEBUG, "item=%p got freed", item);
	m0_rpc_link_fini(&item->cpi_rpc_link);
	m0_free(item);
}

static bool pool_item_disconn_cb(struct m0_clink *link)
{
	struct m0_rpc_conn_pool_item *item;
	struct m0_rpc_conn_pool      *pool;
	struct m0_rpc_session        *session;
	struct m0_sm_ast             *ast;
	struct m0_locality           *loc = m0_locality0_get();

	M0_ENTRY("link %p", link);

	item = container_of(link, struct m0_rpc_conn_pool_item, cpi_clink);
	pool = item->cpi_pool;
	session = &item->cpi_rpc_link.rlk_sess;

	m0_mutex_lock(&pool->cp_mutex);
	M0_LOG(M0_DEBUG, "session established=%d rpc connected=%d",
			 !!m0_rpc_conn_pool_session_established(session),
			 !!item->cpi_rpc_link.rlk_connected);

	m0_clink_fini(&item->cpi_clink);
	m0_chan_fini_lock(&item->cpi_chan);
	M0_LOG(M0_DEBUG, "Going to free item=%p in an AST", item);

	ast = &pool->cp_ast;
	ast->sa_cb = pool_item_disconnected_ast;
	ast->sa_datum = item;
	m0_sm_ast_post(loc->lo_grp, ast);

	m0_mutex_unlock(&pool->cp_mutex);

	M0_LEAVE();
	return true;
}

static void pool_item_disconn_ast(struct m0_sm_group *grp,
				  struct m0_sm_ast   *ast)
{
	struct m0_rpc_conn_pool_item *item;
	struct m0_rpc_session        *session;

	item = ast->sa_datum;
	session = &item->cpi_rpc_link.rlk_sess;
	/* disconnect the rpc link */
	M0_LOG(M0_DEBUG, "Async disconnect this rpc. item=%p", item);
	if (m0_rpc_conn_pool_session_established(session) &&
	    item->cpi_rpc_link.rlk_connected){
		m0_rpc_session_cancel(session);
		m0_clink_init(&item->cpi_clink, pool_item_disconn_cb);
		item->cpi_clink.cl_flags = M0_CF_ONESHOT;
		m0_rpc_link_disconnect_async(&item->cpi_rpc_link,
					     m0_time_from_now(10, 0),
					     &item->cpi_clink);
	}
}


/**
 * Destroy this rpc session from this pool.
 *
 * First, the connection pool item will be identified by iterating the
 * connection pool list and removed from the list.
 * Then, the rpc link will be asynchronously disconnect. In the disconnection
 * callback, we will destroy this rpc link.
 */
M0_INTERNAL void m0_rpc_conn_pool_destroy(struct m0_rpc_conn_pool *pool,
					  struct m0_rpc_session   *session)
{
	struct m0_rpc_conn_pool_item *item;
	struct m0_sm_ast             *ast;
	struct m0_locality           *loc = m0_locality0_get();

	M0_ENTRY();

	m0_mutex_lock(&pool->cp_mutex);

	m0_tl_for(rpc_conn_pool_items, &pool->cp_items, item) {
		if (&item->cpi_rpc_link.rlk_sess != session)
			continue;

		/* remove this item from pool. */
		rpc_conn_pool_items_tlink_del_fini(item);
		break;
	} m0_tl_endfor;

	if (item != NULL) {
		/* Posting an AST to dosconnect this rpc */
		ast = &pool->cp_ast;
		ast->sa_cb = pool_item_disconn_ast;
		ast->sa_datum = item;
		m0_sm_ast_post(loc->lo_grp, ast);
	}
	m0_mutex_unlock(&pool->cp_mutex);

	M0_LEAVE();
}


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
/*
 * vim: tabstop=8 shiftwidth=8 noexpandtab textwidth=80 nowrap
 */
