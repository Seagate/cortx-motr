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


#include "lib/cond.h"
#include "lib/mutex.h"
#include "lib/assert.h"

/**
   @addtogroup cond

   Very simple implementation of condition variables on top of waiting
   channels.

   Self-explanatory.

   @see m0_chan

   @{
 */

M0_INTERNAL void m0_cond_init(struct m0_cond *cond, struct m0_mutex *mutex)
{
	m0_chan_init(&cond->c_chan, mutex);
}
M0_EXPORTED(m0_cond_init);

M0_INTERNAL void m0_cond_fini(struct m0_cond *cond)
{
	m0_chan_fini_lock(&cond->c_chan);
}
M0_EXPORTED(m0_cond_fini);

M0_INTERNAL void m0_cond_wait(struct m0_cond *cond)
{
	struct m0_clink clink;

	/*
	 * First, register the clink with the channel, *then* unlock the
	 * mutex. This guarantees that signals to the condition variable are not
	 * missed, because they are done under the mutex.
	 */

	M0_PRE(m0_chan_is_locked(&cond->c_chan));

	m0_clink_init(&clink, NULL);
	m0_clink_add(&cond->c_chan, &clink);
	m0_chan_unlock(&cond->c_chan);
	m0_chan_wait(&clink);
	m0_chan_lock(&cond->c_chan);
	m0_clink_del(&clink);
	m0_clink_fini(&clink);
}
M0_EXPORTED(m0_cond_wait);

M0_INTERNAL bool m0_cond_timedwait(struct m0_cond *cond,
				   const m0_time_t abs_timeout)
{
	struct m0_clink clink;
	bool            retval;

	M0_PRE(m0_chan_is_locked(&cond->c_chan));

	m0_clink_init(&clink, NULL);
	m0_clink_add(&cond->c_chan, &clink);
	m0_chan_unlock(&cond->c_chan);
	retval = m0_chan_timedwait(&clink, abs_timeout);
	m0_chan_lock(&cond->c_chan);
	m0_clink_del(&clink);
	m0_clink_fini(&clink);

	return retval;
}
M0_EXPORTED(m0_cond_timedwait);

M0_INTERNAL void m0_cond_signal(struct m0_cond *cond)
{
	m0_chan_signal(&cond->c_chan);
}
M0_EXPORTED(m0_cond_signal);

M0_INTERNAL void m0_cond_broadcast(struct m0_cond *cond)
{
	m0_chan_broadcast(&cond->c_chan);
}

/** @} end of cond group */

/*
 *  Local variables:
 *  c-indentation-style: "K&R"
 *  c-basic-offset: 8
 *  tab-width: 8
 *  fill-column: 80
 *  scroll-step: 1
 *  End:
 */
