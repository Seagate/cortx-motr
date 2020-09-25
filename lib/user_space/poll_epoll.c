/* -*- C -*- */
/*
 * Copyright (c) 2020 Seagate Technology LLC and/or its Affiliates
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

#define M0_TRACE_SUBSYSTEM M0_TRACE_SUBSYS_LIB
#include "lib/trace.h"
#include "lib/poll.h"
#include "lib/assert.h"
#include "lib/errno.h"
#include "lib/types.h"
#include "lib/misc.h"           /* M0_IS0 */

/**
   @addtogroup poll

   Implementation of m0_poll on top of epoll(2).

   @{
*/

#if M0_POLL_USE_EPOLL

M0_INTERNAL int  m0_poll_init(struct m0_poll *poll)
{
	M0_PRE(M0_IS0(poll));
	poll->p_fd = epoll_create(1);
	return poll->p_fd > 0 ? poll->p_fd : M0_ERR(-errno);
}

M0_INTERNAL void m0_poll_fini(struct m0_poll  *poll)
{
	if (poll->p_fd > 0) {
		close(poll->p_fd);
		poll->p_fd = -1;
	}
}

M0_INTERNAL int m0_poll_ctl(struct m0_poll *poll, enum m0_poll_cmd cmd, int fd,
			    uint64_t flags, void *datum)
{
	static const int xlate[] = {
		[M0_PC_ADD] = EPOLL_CTL_ADD,
		[M0_PC_DEL] = EPOLL_CTL_DEL,
		[M0_PC_MOD] = EPOLL_CTL_MOD
	};
	M0_PRE(IS_IN_ARRAY(cmd, xlate));
	return epoll_ctl(poll->p_fd, xlate[cmd], fd, &(struct epoll_event){
				.events = flags,
				.data   = { .ptr = datum }});
}

M0_INTERNAL int m0_poll(struct m0_poll_data *pd, int msec)
{
	int result = epoll_wait(pd->pd_poll->p_fd, pd->pd_ev, pd->pd_nr, msec);
	return result >= 0 ? result : M0_ERR(-errno);
}

M0_INTERNAL void *m0_poll_ev_datum(struct m0_poll_data *pd, int i)
{
	M0_PRE(0 <= i && i < data->pd_nr);
	return pd->pd_ev[i].data.ptr;
}

M0_INTERNAL uint64_t m0_poll_ev_flags(struct m0_poll_data *pd, int i)
{
	M0_PRE(0 <= i && i < data->pd_nr);
	return pd->pd_ev[i].events;
}

/* M0_POLL_USE_EPOLL */
#endif

/** @} end of poll group */

/*
 *  Local variables:
 *  c-indentation-style: "K&R"
 *  c-basic-offset: 8
 *  tab-width: 8
 *  fill-column: 80
 *  scroll-step: 1
 *  End:
 */
