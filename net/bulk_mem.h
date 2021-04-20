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

#ifndef __MOTR_NET_BULK_MEM_H__
#define __MOTR_NET_BULK_MEM_H__

#include "net/net.h"

/**
   @defgroup bulkmem In-Memory Messaging and Bulk Transfer Emulation Transport
   @ingroup net

   This module provides a network transport with messaging and bulk
   data transfer across domains within a single process.

   An end point address is a string with two tuples separated by a colon (:).
   The first tuple is the dotted decimal representation of an IPv4 address,
   and the second is an IP port number.

   When used as a base for a derived transport, a third tuple representing
   an unsigned 32 bit service identifier is supported in the address.

   @{
**/

enum {
	M0_NET_BULK_MEM_XEP_ADDR_LEN = 36 /**< Max addr length, 3-tuple */
};

/**
   The bulk in-memory transport pointer to be used in m0_net_domain_init().
 */
extern const struct m0_net_xprt m0_net_bulk_mem_xprt;

/**
   Set the number of worker threads used by a bulk in-memory transfer machine.
   This can be changed before the the transfer machine has started.
   @param tm  Pointer to the transfer machine.
   @param num Number of threads.
   @pre tm->ntm_state == M0_NET_TM_INITIALZIED
 */
M0_INTERNAL void m0_net_bulk_mem_tm_set_num_threads(struct m0_net_transfer_mc
						    *tm, size_t num);

/**
   Return the number of threads used by a bulk in-memory transfer machine.
   @param tm  Pointer to the transfer machine.
   @retval Number-of-threads
 */
M0_INTERNAL size_t m0_net_bulk_mem_tm_get_num_threads(const struct
						      m0_net_transfer_mc *tm);


/**
   @}
*/

#endif /* __MOTR_NET_BULK_MEM_H__ */

/*
 *  Local variables:
 *  c-indentation-style: "K&R"
 *  c-basic-offset: 8
 *  tab-width: 8
 *  fill-column: 79
 *  scroll-step: 1
 *  End:
 */
