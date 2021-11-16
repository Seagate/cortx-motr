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

#ifndef __MOTR_RPC_ITEM_INT_H__
#define __MOTR_RPC_ITEM_INT_H__

#include "rpc/item.h"

struct m0_fid;

/**
   @addtogroup rpc

   @{
 */

/** Initialises global the rpc item state including types list and lock */
M0_INTERNAL int m0_rpc_item_module_init(void);

/**
  Finalizes and destroys the global rpc item state including type list by
  traversing the list and deleting and finalizing each element.
*/
M0_INTERNAL void m0_rpc_item_module_fini(void);

M0_INTERNAL bool m0_rpc_item_is_update(const struct m0_rpc_item *item);
M0_INTERNAL bool m0_rpc_item_is_oneway(const struct m0_rpc_item *item);
M0_INTERNAL bool m0_rpc_item_is_request(const struct m0_rpc_item *item);
M0_INTERNAL bool m0_rpc_item_is_reply(const struct m0_rpc_item *item);

M0_INTERNAL void m0_rpc_item_xid_assign(struct m0_rpc_item *item);
M0_INTERNAL void m0_rpc_item_xid_min_update(struct m0_rpc_item *item);

M0_INTERNAL void m0_rpc_item_sm_init(struct m0_rpc_item *item,
				     enum m0_rpc_item_dir dir);
M0_INTERNAL void m0_rpc_item_sm_fini(struct m0_rpc_item *item);

M0_INTERNAL void m0_rpc_item_change_state(struct m0_rpc_item *item,
					  enum m0_rpc_item_state state);
M0_INTERNAL void m0_rpc_item_failed(struct m0_rpc_item *item, int32_t rc);

M0_INTERNAL int m0_rpc_item_timer_start(struct m0_rpc_item *item);
M0_INTERNAL void m0_rpc_item_timer_stop(struct m0_rpc_item *item);

M0_INTERNAL void m0_rpc_item_ha_timer_stop(struct m0_rpc_item *item);

M0_INTERNAL void m0_rpc_item_send(struct m0_rpc_item *item);

M0_INTERNAL const char *item_kind(const struct m0_rpc_item *item);
M0_INTERNAL const char *item_state_name(const struct m0_rpc_item *item);

M0_INTERNAL int m0_rpc_item_received(struct m0_rpc_item *item,
				     struct m0_rpc_machine *machine);

M0_INTERNAL void m0_rpc_item_process_reply(struct m0_rpc_item *req,
					   struct m0_rpc_item *reply);

M0_INTERNAL void m0_rpc_item_send_reply(struct m0_rpc_item *req,
					struct m0_rpc_item *reply);

M0_INTERNAL void m0_rpc_item_replied_invoke(struct m0_rpc_item *item);

/** @} */
#endif /* __MOTR_RPC_ITEM_INT_H__ */
