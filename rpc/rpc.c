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


#define M0_TRACE_SUBSYSTEM M0_TRACE_SUBSYS_RPC
#include "lib/trace.h"

#include "lib/memory.h"
#include "lib/errno.h"
#include "lib/misc.h"     /* M0_IN */
#include "lib/types.h"
#include "lib/finject.h"

#include "rpc/link.h"     /* m0_rpc_link_module_init */
#include "rpc/rpc.h"
#include "rpc/rpc_internal.h"
#include "rpc/service.h"
#include "net/lnet/lnet.h"
/**
 * @addtogroup rpc
 * @{
 */

M0_INTERNAL int m0_rpc_init(void)
{
	M0_ENTRY();
	return M0_RC(m0_rpc_item_module_init() ?:
		     m0_rpc_service_register() ?:
		     m0_rpc_session_module_init() ?:
		     m0_rpc_link_module_init());
}

M0_INTERNAL void m0_rpc_fini(void)
{
	M0_ENTRY();

	m0_rpc_link_module_fini();
	m0_rpc_session_module_fini();
	m0_rpc_service_unregister();
	m0_rpc_item_module_fini();

	M0_LEAVE();
}

M0_INTERNAL int m0_rpc_post(struct m0_rpc_item *item)
{
	int                    rc;
	struct m0_rpc_machine *machine;

	M0_ENTRY("%p[%s/%u]", item, item_kind(item),
		 item->ri_type->rit_opcode);
	M0_PRE(item->ri_session != NULL);
	M0_PRE(m0_rpc_conn_is_snd(item2conn(item)));

	machine = session_machine(item->ri_session);
	M0_ASSERT(m0_rpc_item_size(item) <= machine->rm_min_recv_size);

	m0_rpc_machine_lock(machine);
	rc = m0_rpc__post_locked(item);
	m0_rpc_machine_unlock(machine);
	return M0_RC(rc);
}
M0_EXPORTED(m0_rpc_post);

M0_INTERNAL int m0_rpc__post_locked(struct m0_rpc_item *item)
{
	struct m0_rpc_session *session;
	int                    error;
	M0_PRE(item != NULL && item->ri_type != NULL);

	M0_ENTRY("%p[%s/%u]",
	       item, item_kind(item), item->ri_type->rit_opcode);
	M0_PRE(m0_rpc_item_is_request(item));

	session = item->ri_session;
	M0_ASSERT_EX(m0_rpc_session_invariant(session));
	M0_ASSERT(m0_rpc_item_size(item) <=
			m0_rpc_session_get_max_item_size(session));
	M0_ASSERT(m0_rpc_machine_is_locked(session_machine(session)));
	/*
	 * For requests an additional reference is taken, this reference is
	 * released after ->rio_replied() is called.
	 */
	m0_rpc_item_get(item);
	item->ri_rmachine = session_machine(session);
	item->ri_rpc_time = m0_time_now();
	m0_rpc_item_sm_init(item, M0_RPC_ITEM_OUTGOING);

	m0_cookie_new(&item->ri_cookid);
	m0_cookie_init(&item->ri_header.osr_cookie, &item->ri_cookid);
	item->ri_header.osr_uuid       = session->s_conn->c_uuid;
	item->ri_header.osr_sender_id  = session->s_conn->c_sender_id;
	item->ri_header.osr_session_id = session->s_session_id;

	if (!M0_IN(session_state(session), (M0_RPC_SESSION_IDLE,
					    M0_RPC_SESSION_BUSY))) {
		M0_LOG(M0_DEBUG, "%p[%s/%u], fop %p, session %p, "
		       "Session isn't established. Hence, not posting the item",
		       item, item_kind(item), item->ri_type->rit_opcode,
		       m0_rpc_item_to_fop(item), session);
		error = M0_ERR(-ENOTCONN);
	} else if (session->s_session_id != SESSION_ID_0 &&
		   m0_rpc_session_is_cancelled(session)) {
		M0_LOG(M0_DEBUG, "%p[%s/%u], fop %p, session %p, "
		       "Session is cancelled. Hence, not posting the item",
		       item, item_kind(item), item->ri_type->rit_opcode,
		       m0_rpc_item_to_fop(item), session);
		error = M0_ERR(-ECANCELED);
	} else {
		m0_rpc_item_send(item);
		return M0_RC(item->ri_error);
	}
	m0_rpc_item_failed(item, error);
	return error;
}

void m0_rpc_reply_post(struct m0_rpc_item *request, struct m0_rpc_item *reply)
{
	struct m0_rpc_machine *machine;

	M0_ENTRY("req_item: %p, rep_item: %p", request, reply);
	M0_PRE(request != NULL && reply != NULL);
	M0_PRE(request->ri_session != NULL);
	M0_PRE(reply->ri_type != NULL);
	M0_PRE(m0_rpc_item_size(reply) <=
			m0_rpc_session_get_max_item_size(request->ri_session));
	M0_PRE(m0_rpc_conn_is_rcv(item2conn(request)));

	if (M0_FI_ENABLED("delay_reply")) {
		M0_LOG(M0_DEBUG, "%p reply delayed", request);
		m0_nanosleep(m0_time(M0_RPC_ITEM_RESEND_INTERVAL,
				     200 * 1000 * 1000), NULL);
	}

	reply->ri_resend_interval = M0_TIME_NEVER;
	reply->ri_rpc_time = m0_time_now();
	reply->ri_session  = request->ri_session;
	machine = reply->ri_rmachine = request->ri_rmachine;

	reply->ri_prio     = request->ri_prio;
	reply->ri_deadline = 0;
	reply->ri_error    = 0;

	m0_rpc_machine_lock(machine);
	m0_rpc_item_sm_init(reply, M0_RPC_ITEM_OUTGOING);
	m0_rpc_item_send_reply(request, reply);
	m0_rpc_machine_unlock(machine);
}
M0_EXPORTED(m0_rpc_reply_post);

M0_INTERNAL void m0_rpc_oneway_item_post(const struct m0_rpc_conn *conn,
					 struct m0_rpc_item *item)
{
	struct m0_rpc_machine *machine;

	M0_ENTRY("conn: %p, item: %p", conn, item);
	M0_PRE(conn != NULL);
	M0_PRE(m0_rpc_machine_is_not_locked(conn->c_rpc_machine));

	machine = conn->c_rpc_machine;
	m0_rpc_machine_lock(machine);
	m0_rpc_oneway_item_post_locked(conn, item);
	m0_rpc_machine_unlock(machine);
}

M0_INTERNAL void m0_rpc_oneway_item_post_locked(const struct m0_rpc_conn *conn,
						struct m0_rpc_item *item)
{
	M0_PRE(conn != NULL &&
	       m0_rpc_machine_is_locked(conn->c_rpc_machine));
	M0_PRE(item != NULL && m0_rpc_item_is_oneway(item));

	/*
	 * Rpc always acquires an *internal* reference to "all" items (Here
	 * one-way items). This reference is released when the item is sent.
	 */
	m0_rpc_item_get(item);
	item->ri_resend_interval = M0_TIME_NEVER;
	item->ri_rpc_time        = m0_time_now();
	item->ri_rmachine        = conn->c_rpc_machine;

	m0_rpc_item_sm_init(item, M0_RPC_ITEM_OUTGOING);
	item->ri_nr_sent++;
	m0_rpc_frm_enq_item(&conn->c_rpcchan->rc_frm, item);
}

M0_INTERNAL int m0_rpc_reply_timedwait(struct m0_clink *clink,
				       const m0_time_t timeout)
{
	int rc;
	M0_ENTRY("timeout: "TIME_F, TIME_P(timeout));
	M0_PRE(clink != NULL);
	M0_PRE(m0_clink_is_armed(clink));

	rc = m0_chan_timedwait(clink, timeout) ? 0 : -ETIMEDOUT;
	return M0_RC(rc);
}
M0_EXPORTED(m0_rpc_reply_timedwait);


static void rpc_buffer_pool_low(struct m0_net_buffer_pool *bp)
{
	/* Buffer pool is below threshold.  */
}

static const struct m0_net_buffer_pool_ops b_ops = {
	.nbpo_not_empty	      = m0_net_domain_buffer_pool_not_empty,
	.nbpo_below_threshold = rpc_buffer_pool_low,
};

M0_INTERNAL int m0_rpc_net_buffer_pool_setup(struct m0_net_domain *ndom,
					     struct m0_net_buffer_pool
					     *app_pool, uint32_t bufs_nr,
					     uint32_t tm_nr)
{
	int	    rc;
	uint32_t    segs_nr;
	m0_bcount_t seg_size;

	M0_ENTRY("net_dom: %p", ndom);
	M0_PRE(ndom != NULL);
	M0_PRE(app_pool != NULL);
	M0_PRE(bufs_nr != 0);

	seg_size = m0_rpc_max_seg_size(ndom);
	segs_nr  = m0_rpc_max_segs_nr(ndom);
	app_pool->nbp_ops = &b_ops;
	rc = m0_net_buffer_pool_init(app_pool, ndom,
				     M0_NET_BUFFER_POOL_THRESHOLD,
				     segs_nr, seg_size, tm_nr, M0_SEG_SHIFT,
				     false);
	if (rc != 0)
		return M0_ERR_INFO(rc, "net_buf_pool: Initialization");

	m0_net_buffer_pool_lock(app_pool);
	rc = m0_net_buffer_pool_provision(app_pool, bufs_nr);
	m0_net_buffer_pool_unlock(app_pool);

	if (rc == bufs_nr)
		return M0_RC(0);
	m0_net_buffer_pool_fini(app_pool);
	return M0_RC(-ENOMEM);
}
M0_EXPORTED(m0_rpc_net_buffer_pool_setup);

void m0_rpc_net_buffer_pool_cleanup(struct m0_net_buffer_pool *app_pool)
{
	M0_PRE(app_pool != NULL);
	m0_net_buffer_pool_fini(app_pool);
}
M0_EXPORTED(m0_rpc_net_buffer_pool_cleanup);

M0_INTERNAL uint32_t m0_rpc_bufs_nr(uint32_t len, uint32_t tms_nr)
{
	return len +
	       /* It is used so that more than one free buffer is present
		* for each TM when tms_nr > 8.
		*/
	       max32u(tms_nr / 4, 1) +
	       /* It is added so that frequent low_threshold callbacks of
		* buffer pool can be reduced.
		*/
	       M0_NET_BUFFER_POOL_THRESHOLD;
}

M0_INTERNAL m0_bcount_t m0_rpc_max_seg_size(struct m0_net_domain *ndom)
{
	M0_PRE(ndom != NULL);
	M0_PRE(ndom->nd_xprt != NULL);
	M0_PRE(ndom->nd_xprt->nx_ops->xo_rpc_max_seg_size != NULL);

	return ndom->nd_xprt->nx_ops->xo_rpc_max_seg_size(ndom);
}

M0_INTERNAL uint32_t m0_rpc_max_segs_nr(struct m0_net_domain *ndom)
{
	M0_PRE(ndom != NULL);
	M0_PRE(ndom->nd_xprt != NULL);
	M0_PRE(ndom->nd_xprt->nx_ops->xo_rpc_max_segs_nr != NULL);

	return ndom->nd_xprt->nx_ops->xo_rpc_max_segs_nr(ndom);
}

M0_INTERNAL m0_bcount_t m0_rpc_max_msg_size(struct m0_net_domain *ndom,
					    m0_bcount_t rpc_size)
{
	M0_PRE(ndom != NULL);
	M0_PRE(ndom->nd_xprt != NULL);
	M0_PRE(ndom->nd_xprt->nx_ops->xo_rpc_max_msg_size != NULL);

	return ndom->nd_xprt->nx_ops->xo_rpc_max_msg_size(ndom, rpc_size);
}

M0_INTERNAL uint32_t m0_rpc_max_recv_msgs(struct m0_net_domain *ndom,
					  m0_bcount_t rpc_size)
{
	M0_PRE(ndom != NULL);
	M0_PRE(ndom->nd_xprt != NULL);
	M0_PRE(ndom->nd_xprt->nx_ops->xo_rpc_max_recv_msgs != NULL);

	return ndom->nd_xprt->nx_ops->xo_rpc_max_recv_msgs(ndom, rpc_size);
}

M0_INTERNAL m0_time_t m0_rpc__down_timeout(void)
{
	return m0_time_from_now(M0_RPC_ITEM_RESEND_INTERVAL * 2 + 1, 0);
}

/** @} */
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
