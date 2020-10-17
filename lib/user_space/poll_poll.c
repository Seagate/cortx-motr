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

#include "lib/mutex.h"
#define M0_TRACE_SUBSYSTEM M0_TRACE_SUBSYS_LIB
#include "lib/trace.h"
#include "lib/poll.h"
#include "lib/assert.h"
#include "lib/errno.h"
#include "lib/memory.h"
#include "lib/types.h"
#include "lib/misc.h"           /* M0_IS0 */

/**
   @addtogroup poll

   Implementation of m0_poll on top of poll(2).

   @{
*/

#if M0_POLL_USE_POLL

static int expand(struct m0_poll *poll, int nsize);
static int fd_get(struct m0_poll_data *pd, int idx);

M0_INTERNAL int m0_poll_init(struct m0_poll *poll)
{
	M0_PRE(M0_IS0(poll));
	m0_mutex_init(&poll->p_lock);
	return 0;
}

M0_INTERNAL void m0_poll_fini(struct m0_poll *poll)
{
	m0_mutex_fini(&poll->p_lock);
	m0_free(poll->p_dt);
	m0_free(poll->p_fd);
}

M0_INTERNAL int m0_poll_ctl(struct m0_poll *poll, enum m0_poll_cmd cmd, int fd,
			    uint64_t flags, void *datum)
{
	int result = 0;

	M0_PRE(fd > 0);
	m0_mutex_lock(&poll->p_lock);
	if (fd >= poll->p_nr) {
		result = expand(poll, max_check(fd + 1, poll->p_nr * 2));
		if (result != 0) {
			m0_mutex_unlock(&poll->p_lock);
			return M0_ERR(result);
		}
	}
	M0_ASSERT(fd < poll->p_nr);
	switch (cmd) {
	case M0_PC_ADD:
		if (poll->p_fd[fd].fd < 0) {
			poll->p_fd[fd].fd     = fd;
			poll->p_fd[fd].events = flags;
			poll->p_dt[fd]        = datum;
		} else
			result = M0_ERR(-EEXIST);
		break;
	case M0_PC_DEL:
		if (poll->p_fd[fd].fd > 0) {
			poll->p_fd[fd].fd = -1;
		} else
			result = M0_ERR(-ENOENT);
		break;
	case M0_PC_MOD:
		if (poll->p_fd[fd].fd > 0) {
			poll->p_fd[fd].events = flags;
			poll->p_dt[fd]        = datum;
		} else
			result = M0_ERR(-ENOENT);
		break;
	}
	m0_mutex_unlock(&poll->p_lock);
	return result;
}

M0_INTERNAL int m0_poll(struct m0_poll_data *pd, int msec)
{
	struct m0_poll *p      = pd->pd_poll;
	int             result = 0;
	int             nr;
	struct pollfd  *pfd;

	m0_mutex_lock(&p->p_lock);
	pd->pd_count = nr = p->p_nr;
	M0_ALLOC_ARR(pfd, nr);
	if (pfd == NULL)
		result = M0_ERR(-ENOMEM);
	else
		pd->pd_fd = memcpy(pfd, p->p_fd, sizeof pfd[0] * nr);
	m0_mutex_unlock(&p->p_lock);
	if (result == 0) {
		result = poll(pfd, nr, msec);
		M0_ASSERT(ergo(result >= 0,
			       result == m0_count(i, nr, pfd[i].revents != 0)));
		if (result < 0)
			result = M0_ERR(-errno);
	}
	return M0_RC(result);
}

M0_INTERNAL void *m0_poll_ev_datum(struct m0_poll_data *pd, int i)
{
	return pd->pd_poll->p_dt[fd_get(pd, i)];
}

M0_INTERNAL uint64_t m0_poll_ev_flags(struct m0_poll_data *pd, int i)
{
	return pd->pd_fd[fd_get(pd, i)].revents;
}

static int expand(struct m0_poll *poll, int nsize)
{
	struct pollfd  *nfd;
	void          **ndt;
	int             i;

	M0_PRE(nsize > poll->p_nr);
	M0_ALLOC_ARR(nfd, nsize);
	M0_ALLOC_ARR(ndt, nsize);
	if (nfd != NULL && ndt != NULL) {
		for (i = 0; i < poll->p_nr; ++i) {
			nfd[i] = poll->p_fd[i];
			ndt[i] = poll->p_dt[i];
		}
		for (; i < nsize; ++i)
			nfd[i].fd = -1;
		m0_free(poll->p_fd);
		m0_free(poll->p_dt);
		poll->p_fd = nfd;
		poll->p_dt = ndt;
		poll->p_nr = nsize;
		return 0;
	} else
		return M0_ERR(-ENOMEM);
}

static int fd_get(struct m0_poll_data *pd, int idx)
{
	int i;
	int idx0 = idx;

	for (i = 0; i < pd->pd_count; ++i) {
		if (pd->pd_fd[i].revents != 0 && idx-- == 0)
			return i;
	}
	M0_IMPOSSIBLE("poll(2) lied: %i %i.", idx0, pd->pd_count);
}

/* M0_POLL_USE_POLL */
#endif

#undef M0_TRACE_SUBSYSTEM

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
