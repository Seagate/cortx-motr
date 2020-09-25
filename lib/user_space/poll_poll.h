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


#pragma once

#ifndef __MOTR_LIB_USER_SPACE_POLL_POLL_H__
#define __MOTR_LIB_USER_SPACE_POLL_POLL_H__

/**
 * @addtogroup poll
 *
 * @{
 */

#include <poll.h>

enum {
	EPOLLIN  = POLLIN,
	EPOLLOUT = POLLOUT,
	EPOLLPRI = POLLPRI,
	EPOLLERR = POLLERR,
	EPOLLHUP = POLLHUP,
	EPOLLRDHUP,
	EPOLLET
};

struct m0_poll {
	int            p_nr;
	struct pollfd *p_fd;
	void         **p_dt;
};

struct m0_poll_data {
	struct m0_poll *pd_poll;
	int             pd_nr;
};

#define M0_POLL_PREP(pd, poll, nr)		\
({						\
	struct m0_poll_data *__pd = (pd);	\
						\
	__pd->pd_poll = (poll);		\
	__pd->pd_nr   = (nr);			\
	(void)0;				\
})

#define M0_POLL_DONE(pd) ((void)0)

/** @} end of poll group */

/* __MOTR_LIB_USER_SPACE_POLL_POLL_H__ */
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
