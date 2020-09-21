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

#ifndef __MOTR_NET_LNET_XO_H__
#define __MOTR_NET_LNET_XO_H__

#include "net/lnet/lnet_core.h"
#include "lib/thread.h"
#include "lib/bitmap.h" /* m0_bitmap */

/**
   @defgroup LNetXODFS LNet Transport XO Interface
   @ingroup LNetDFS
   @{
 */

struct nlx_xo_buffer;
struct nlx_xo_domain;
struct nlx_xo_ep;
struct nlx_xo_transfer_mc;

enum {
	M0_NET_LNET_EVT_SHORT_WAIT_SECS = 1, /**< Event thread short wait */
	M0_NET_LNET_EVT_LONG_WAIT_SECS = 10, /**< Event thread long wait */
	M0_NET_LNET_BUF_TIMEOUT_TICK_SECS = 15, /**< Min timeout granularity */
};

/**
   LNet transport's internal end point structure.
 */
struct nlx_xo_ep {
	/** Magic constant to validate end point */
	uint64_t                xe_magic;

	/** embedded network end point structure. */
	struct m0_net_end_point xe_ep;

	/** LNet transport address */
	struct nlx_core_ep_addr xe_core;

	/**
	   Memory for the string representation of the end point.
	   The @c xe_ep.nep_addr field points to @c xe_addr.
	 */
	char                    xe_addr[M0_NET_LNET_XEP_ADDR_LEN];
};

/**
   Private data pointed to by m0_net_domain::nd_xprt_private.
 */
struct nlx_xo_domain {
	/** Pointer back to the network dom */
	struct m0_net_domain   *xd_dom;

	/** LNet Core transfer domain data (shared memory) */
	struct nlx_core_domain  xd_core;

	unsigned                _debug_;
};

/**
   Private data pointed to by m0_net_transfer_mc::ntm_xprt_private.
 */
struct nlx_xo_transfer_mc {
	/** Pointer back to the network tm */
	struct m0_net_transfer_mc   *xtm_tm;

	/** Transfer machine thread processor affinity */
	struct m0_bitmap             xtm_processors;

	/** Event thread */
	struct m0_thread             xtm_ev_thread;

	/** Condition variable used by the event thread for synchronous buffer
	    event notification.
	 */
	struct m0_cond               xtm_ev_cond;

	/** Channel used for synchronous buffer event notification */
	struct m0_chan              *xtm_ev_chan;

	/** LNet Core transfer machine data (shared memory) */
	struct nlx_core_transfer_mc  xtm_core;

	unsigned                     _debug_;
};

/**
   Private data pointed to by m0_net_buffer::nb_xprt_private.
 */
struct nlx_xo_buffer {
	/** Pointer back to the network buffer */
	struct m0_net_buffer   *xb_nb;

	/** LNet Core buffer data (shared memory) */
	struct nlx_core_buffer  xb_core;
};

/**
   @}
 */

#endif /* __MOTR_NET_LNET_XO_H__ */

/*
 *  Local variables:
 *  c-indentation-style: "K&R"
 *  c-basic-offset: 8
 *  tab-width: 8
 *  fill-column: 79
 *  scroll-step: 1
 *  End:
 */
