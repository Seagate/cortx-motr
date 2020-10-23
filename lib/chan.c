/* -*- C -*- */
/*
 * Copyright (c) 2011-2020 Seagate Technology LLC and/or its Affiliates
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


#include "lib/thread.h"
#include "lib/errno.h"
#include "lib/chan.h"
#include "lib/assert.h"
#include "addb2/addb2.h"
#include "addb2/counter.h"
#include "motr/magic.h"
#include "lib/arith.h"      /* M0_CNT_DEC */

/**
   @addtogroup chan

   A simplistic user space implementation of m0_chan and m0_clink interfaces
   based on POSIX semaphores.

   A list of registered clinks is maintained for each channel. Each clink has a
   semaphore, used to wait for pending events. When an event is declared on the
   channel, a number (depending on whether event is signalled or broadcast) of
   clinks on the channel list is scanned and for each of them either call-back
   is called or semaphore is upped.

   To wait for an event, a user downs clink semaphore.

   Semaphore is initialized every time when the clink is registered with a
   channel (m0_clink_add()) and destroyed every time the clink is deleted from a
   channel (m0_clink_del()). This guarantees that semaphore counter is exactly
   equal to the number of pending events declared on the channel.

   @note that a version of m0_chan_wait() with a timeout would induce some
   changes to the design, because in this case it is waiter who has to unlink a
   clink from a channel.

   @{
 */

M0_TL_DESCR_DEFINE(clink, "chan clinks", static, struct m0_clink, cl_linkage,
		   cl_magic, M0_LIB_CHAN_MAGIC, M0_LIB_CHAN_HEAD_MAGIC);

M0_TL_DEFINE(clink, static, struct m0_clink);

static void clink_del(struct m0_clink *link);
static void semaphore_fini(struct m0_clink *link);
static void down_tail(struct m0_clink *link, bool got);

static bool clink_is_head(const struct m0_clink *clink)
{
	return clink->cl_group == clink;
}

M0_INTERNAL void m0_chan_lock(struct m0_chan *ch)
{
	m0_mutex_lock(ch->ch_guard);
}

M0_INTERNAL void m0_chan_unlock(struct m0_chan *ch)
{
	m0_mutex_unlock(ch->ch_guard);
}

M0_INTERNAL bool m0_chan_is_locked(const struct m0_chan *ch)
{
	return m0_mutex_is_locked(ch->ch_guard);
}

/**
   Channel invariant: all clinks on the list are clinks for this channel and
   number of waiters matches list length.
 */
static bool m0_chan_invariant(struct m0_chan *chan)
{
	return chan->ch_waiters == clink_tlist_length(&chan->ch_links) &&
		m0_tl_forall(clink, scan, &chan->ch_links,
			     scan->cl_chan == chan &&
			     scan->cl_group != NULL &&
			     clink_is_head(scan->cl_group));
}

M0_INTERNAL void m0_chan_init(struct m0_chan *chan, struct m0_mutex *ch_guard)
{
	M0_PRE(ch_guard != NULL);
	*chan = (struct m0_chan){ .ch_guard = ch_guard };
	clink_tlist_init(&chan->ch_links);
}
M0_EXPORTED(m0_chan_init);

M0_INTERNAL void m0_chan_fini(struct m0_chan *chan)
{
	M0_PRE(m0_chan_is_locked(chan));
	M0_ASSERT(chan->ch_waiters == 0);
	clink_tlist_fini(&chan->ch_links);
}
M0_EXPORTED(m0_chan_fini);

M0_INTERNAL void m0_chan_fini_lock(struct m0_chan *chan)
{
	/*
	 * This seemingly useless lock-unlock pair is to synchronize with
	 * m0_chan_{signal,broadcast}() that might be still using chan.
	 */
	m0_chan_lock(chan);
	m0_chan_fini(chan);
	m0_chan_unlock(chan);
}
M0_EXPORTED(m0_chan_fini_lock);

static void clink_signal(struct m0_clink *clink)
{
	/*
	 * This function must satisfy a number of subtle conditions:
	 *
	 *     - clink can be manipulated from its call-back;
	 *
	 *     - clink can freed by the waiter immediately after the semaphore
	 *       is increased;
	 *
	 *     - semaphore must be finalised after it was increased.
	 */
	struct m0_clink      *grp      = clink->cl_group;
	struct m0_chan_addb2 *ca       = clink->cl_chan->ch_addb2;
	bool                  consumed = false;

	if (clink->cl_flags & M0_CF_ONESHOT) {
		clink_del(clink);
		if (clink->cl_cb == NULL) {
			clink->cl_flags |= M0_CF_HEARD_BANSHEE;
			m0_semaphore_up(&grp->cl_wait);
			return;
		}
	}
	if (clink->cl_cb != NULL) {
		m0_enter_awkward();
		if (ca == NULL)
			consumed = clink->cl_cb(clink);
		else
			M0_ADDB2_HIST(ca->ca_cb, &ca->ca_cb_hist,
				      m0_ptr_wrap(clink->cl_cb),
				      consumed = clink->cl_cb(clink));
		m0_exit_awkward();
		M0_ASSERT(ergo(clink->cl_flags & M0_CF_ONESHOT, consumed));
	}
	if (!consumed)
		m0_semaphore_up(&grp->cl_wait);
}

static void chan_signal_nr(struct m0_chan *chan, uint32_t nr)
{
	struct m0_clink *clink;

	M0_PRE_EX(m0_chan_is_locked(chan) && m0_chan_invariant(chan));
	while (nr-- > 0 &&
	       (clink = clink_tlist_head(&chan->ch_links)) != NULL) {
		clink_tlist_move_tail(&chan->ch_links, clink);
		clink_signal(clink);
	}
	M0_POST_EX(m0_chan_invariant(chan));
}

M0_INTERNAL void m0_chan_signal(struct m0_chan *chan)
{
	chan_signal_nr(chan, 1);
}
M0_EXPORTED(m0_chan_signal);

M0_INTERNAL void m0_chan_signal_lock(struct m0_chan *chan)
{
	m0_chan_lock(chan);
	m0_chan_signal(chan);
	m0_chan_unlock(chan);
}

M0_INTERNAL void m0_chan_broadcast(struct m0_chan *chan)
{
	chan_signal_nr(chan, chan->ch_waiters);
}
M0_EXPORTED(m0_chan_broadcast);

M0_INTERNAL void m0_chan_broadcast_lock(struct m0_chan *chan)
{
	m0_chan_lock(chan);
	m0_chan_broadcast(chan);
	m0_chan_unlock(chan);
}

M0_INTERNAL bool m0_chan_has_waiters(struct m0_chan *chan)
{
	return chan->ch_waiters > 0;
}

static void clink_init(struct m0_clink *link,
		       struct m0_clink *group, m0_chan_cb_t cb)
{
	link->cl_group      = group;
	link->cl_chan       = NULL;
	link->cl_cb         = cb;
	link->cl_flags      = 0;
	clink_tlink_init(link);
	M0_POST(clink_is_head(group));
}

M0_INTERNAL void m0_clink_init(struct m0_clink *link, m0_chan_cb_t cb)
{
	clink_init(link, link, cb);
	/* do NOT initialise the semaphore here */
}
M0_EXPORTED(m0_clink_init);

M0_INTERNAL void m0_clink_fini(struct m0_clink *link)
{
	/* do NOT finalise the semaphore here */
	clink_tlink_fini(link);
	if (link->cl_flags & M0_CF_HEARD_BANSHEE)
		semaphore_fini(link);
}
M0_EXPORTED(m0_clink_fini);

M0_INTERNAL void m0_clink_attach(struct m0_clink *link,
				 struct m0_clink *group, m0_chan_cb_t cb)
{
	M0_PRE(clink_is_head(group));

	clink_init(link, group, cb);
}
M0_EXPORTED(m0_clink_attach);

/**
   @pre  !m0_clink_is_armed(link)
   @post  m0_clink_is_armed(link)
 */
M0_INTERNAL void m0_clink_add(struct m0_chan *chan, struct m0_clink *link)
{
	int rc;

	M0_PRE(m0_chan_is_locked(chan));
	M0_PRE(!m0_clink_is_armed(link));
	/* head is registered first */
	M0_PRE(ergo(!clink_is_head(link), m0_clink_is_armed(link->cl_group)));

	link->cl_chan = chan;
	if (clink_is_head(link)) {
		rc = m0_semaphore_init(&link->cl_wait, 0);
		M0_ASSERT(rc == 0);
	}

	M0_ASSERT_EX(m0_chan_invariant(chan));
	M0_CNT_INC(chan->ch_waiters);
	clink_tlist_add_tail(&chan->ch_links, link);
	if (chan->ch_addb2 != NULL)
		m0_addb2_hist_mod(&chan->ch_addb2->ca_queue_hist,
				  chan->ch_waiters);
	M0_ASSERT_EX(m0_chan_invariant(chan));

	M0_POST(m0_clink_is_armed(link));
}
M0_EXPORTED(m0_clink_add);

void m0_clink_add_lock(struct m0_chan *chan, struct m0_clink *link)
{
	m0_chan_lock(chan);
	m0_clink_add(chan, link);
	m0_chan_unlock(chan);
}
M0_EXPORTED(m0_clink_add_lock);

static void clink_del(struct m0_clink *link)
{
	struct m0_chan *chan = link->cl_chan;

	M0_PRE(m0_clink_is_armed(link));
	M0_PRE(m0_chan_is_locked(chan));
	/* head is de-registered last */
	M0_PRE(ergo(!clink_is_head(link), m0_clink_is_armed(link->cl_group)));

	M0_ASSERT_EX(m0_chan_invariant(chan));
	M0_CNT_DEC(chan->ch_waiters);
	clink_tlist_del(link);
	if (chan->ch_addb2 != NULL)
		m0_addb2_hist_mod(&chan->ch_addb2->ca_queue_hist,
				  chan->ch_waiters);
	M0_ASSERT_EX(m0_chan_invariant(chan));
	/*
	 * Do not zero link->cl_chan: for one-shot clinks, the channel should be
	 * still valid at the time of m0_chan_wait() call.
	 */
	M0_POST(!m0_clink_is_armed(link));
}

static void semaphore_fini(struct m0_clink *link)
{
	if (clink_is_head(link))
		m0_semaphore_fini(&link->cl_wait);
}

/**
   @pre   m0_clink_is_armed(link)
   @post !m0_clink_is_armed(link)
 */
M0_INTERNAL void m0_clink_del(struct m0_clink *link)
{
	clink_del(link);
	semaphore_fini(link);
}
M0_EXPORTED(m0_clink_del);

M0_INTERNAL void m0_clink_del_lock(struct m0_clink *link)
{
	struct m0_chan *chan = link->cl_chan;

	m0_chan_lock(chan);
	m0_clink_del(link);
	m0_chan_unlock(chan);
}
M0_EXPORTED(m0_clink_del_lock);

M0_INTERNAL bool m0_clink_is_armed(const struct m0_clink *link)
{
	return link->cl_chan != NULL &&
	       link->cl_linkage.t_link.ll_next != NULL &&
	       m0_list_link_is_in(&link->cl_linkage.t_link);
}

M0_INTERNAL void m0_clink_cleanup(struct m0_clink *link)
{
	if (link->cl_chan != NULL) {
		m0_chan_lock(link->cl_chan);
		m0_clink_cleanup_locked(link);
		m0_chan_unlock(link->cl_chan);
	}
}

M0_INTERNAL void m0_clink_cleanup_locked(struct m0_clink *link)
{
	M0_PRE(m0_chan_is_locked(link->cl_chan));
	if (m0_clink_is_armed(link))
		m0_clink_del(link);
}

M0_INTERNAL void m0_clink_signal(struct m0_clink *clink)
{
	M0_PRE(!(clink->cl_flags & M0_CF_ONESHOT));
	clink_signal(clink);
}

M0_INTERNAL bool m0_chan_trywait(struct m0_clink *link)
{
	bool got = m0_semaphore_trydown(&link->cl_group->cl_wait);
	down_tail(link, got);
	return got;
}

M0_INTERNAL void m0_chan_wait(struct m0_clink *link)
{
	struct m0_chan_addb2 *ca = link->cl_chan->ch_addb2;

	if (ca == NULL)
		m0_semaphore_down(&link->cl_group->cl_wait);
	else
		M0_ADDB2_HIST(ca->ca_wait, &ca->ca_wait_hist,
			      m0_ptr_wrap(__builtin_return_address(0)),
			      m0_semaphore_down(&link->cl_group->cl_wait));
	down_tail(link, true);
}
M0_EXPORTED(m0_chan_wait);

M0_INTERNAL bool m0_chan_timedwait(struct m0_clink *link,
				   const m0_time_t abs_timeout)
{
	struct m0_chan_addb2 *ca = link->cl_chan->ch_addb2;
	bool                  got;

	if (ca == NULL)
		got = m0_semaphore_timeddown(&link->cl_group->cl_wait,
					     abs_timeout);
	else
		M0_ADDB2_HIST(ca->ca_wait, &ca->ca_wait_hist,
		       m0_ptr_wrap(__builtin_return_address(0)),
		       got = m0_semaphore_timeddown(&link->cl_group->cl_wait,
						    abs_timeout));
	down_tail(link, got);
	return got;
}
M0_EXPORTED(m0_chan_timedwait);

static void down_tail(struct m0_clink *link, bool got)
{
	if (got && (link->cl_flags & M0_CF_HEARD_BANSHEE)) {
		link->cl_flags &= ~M0_CF_HEARD_BANSHEE;
		semaphore_fini(link);
	}
}

/** @} end of chan group */

/*
 *  Local variables:
 *  c-indentation-style: "K&R"
 *  c-basic-offset: 8
 *  tab-width: 8
 *  fill-column: 80
 *  scroll-step: 1
 *  End:
 */
