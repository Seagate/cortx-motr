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

#ifndef __MOTR_LIB_POLL_H__
#define __MOTR_LIB_POLL_H__

/**
 * @defgroup poll
 *
 * @{
 */

#include "lib/types.h"

#if defined(M0_LINUX)
#define M0_POLL_USE_EPOLL   (1)
#define M0_POLL_USE_POLL    (0)
#define M0_POLL_USE_SELECT  (0)
#elif defined(M0_DARWIN)
#define M0_POLL_USE_EPOLL   (0)
#define M0_POLL_USE_POLL    (1)
#define M0_POLL_USE_SELECT  (0)
#endif

#if M0_POLL_USE_EPOLL
#include "lib/user_space/poll_epoll.h"
#endif

#if M0_POLL_USE_POLL
#include "lib/user_space/poll_poll.h"
#endif

#if M0_POLL_USE_SELECT
#include "lib/user_space/poll_select.h"
#endif

struct m0_poll;
struct m0_poll_data;
struct m0_poll_ev;

enum m0_poll_cmd {
	M0_PC_ADD,
	M0_PC_DEL,
	M0_PC_MOD
};

M0_INTERNAL int  m0_poll_init(struct m0_poll  *poll);
M0_INTERNAL void m0_poll_fini(struct m0_poll  *poll);

M0_INTERNAL int m0_poll_ctl(struct m0_poll *poll, enum m0_poll_cmd cmd, int fd,
			    uint64_t flags, void *datum);

/*
 * See M0_POLL_PREP() and M0_POLL_DONE() defined in lib/user_space/poll_*.h
 */

M0_INTERNAL int      m0_poll         (struct m0_poll_data *data, int msec);
M0_INTERNAL void    *m0_poll_ev_datum(struct m0_poll_data *data, int i);
M0_INTERNAL uint64_t m0_poll_ev_flags(struct m0_poll_data *data, int i);

/** @} end of poll group */

/* __MOTR_LIB_POLL_H__ */
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
