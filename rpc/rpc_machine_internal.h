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

#ifndef __MOTR_RPC_MACHINE_INT_H__
#define __MOTR_RPC_MACHINE_INT_H__

#include "lib/tlist.h"
#include "lib/refs.h"
#include "rpc/formation2_internal.h"

/* Imports */
struct m0_net_end_point;
struct m0_rpc_machine;
struct m0_rpc_machine_watch;
struct m0_rpc_conn;


/**
   @addtogroup rpc

   @{
 */

/**
   Struct m0_rpc_chan provides information about a target network endpoint.
   An rpc machine (struct m0_rpc_machine) contains list of m0_rpc_chan
   structures targeting different net endpoints.
   Rationale A physical node can have multiple endpoints associated with it.
   And multiple services can share endpoints for transport.
   The rule of thumb is to use one transfer machine per endpoint.
   So to make sure that services using same endpoint,
   use the same transfer machine, this structure has been introduced.
   Struct m0_rpc_conn is used for a particular service and now it
   points to a struct m0_rpc_chan to identify the transfer machine
   it is working with.
 */
struct m0_rpc_chan {
	/** Link in m0_rpc_machine::rm_chans list.
	    List descriptor: rpc_chan
	 */
	struct m0_tlink			  rc_linkage;
	/** Number of m0_rpc_conn structures using this transfer machine.*/
	struct m0_ref			  rc_ref;
	/** Formation state machine associated with chan. */
	struct m0_rpc_frm                 rc_frm;
	/** Destination end point to which rpcs will be sent. */
	struct m0_net_end_point		 *rc_destep;
	/** The rpc_machine, this chan structure is associated with.*/
	struct m0_rpc_machine		 *rc_rpc_machine;
	/** M0_RPC_CHAN_MAGIC */
	uint64_t			  rc_magic;
};

M0_INTERNAL void m0_rpc_machine_add_conn(struct m0_rpc_machine *rmach,
					 struct m0_rpc_conn    *conn);

M0_INTERNAL struct m0_rpc_conn *
m0_rpc_machine_find_conn(const struct m0_rpc_machine *machine,
			 const struct m0_rpc_item    *item);

M0_TL_DESCR_DECLARE(rpc_conn, M0_EXTERN);
M0_TL_DECLARE(rpc_conn, M0_INTERNAL, struct m0_rpc_conn);

M0_TL_DESCR_DECLARE(rmach_watch, M0_EXTERN);
M0_TL_DECLARE(rmach_watch, M0_INTERNAL, struct m0_rpc_machine_watch);

/**
  * Terminates all active incoming sessions and connections.
  *
  * Such cleanup is required to handle case where receiver is terminated
  * while one or more senders are still connected to it.
  *
  * For more information on this issue visit
  * <a href="http://goo.gl/5vXUS"> here </a>
  */
M0_INTERNAL void
m0_rpc_machine_cleanup_incoming_connections(struct m0_rpc_machine *machine);

/** @} */
#endif /* __MOTR_RPC_MACHINE_INT_H__ */
