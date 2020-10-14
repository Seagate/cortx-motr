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

#ifndef __MOTR_RPC_INT_H__
#define __MOTR_RPC_INT_H__

#include "rpc/conn_internal.h"
#include "rpc/session_internal.h"
#include "rpc/item_internal.h"
#include "rpc/rpc_machine_internal.h"
#include "rpc/formation2_internal.h"
#include "rpc/packet_internal.h"
#include "rpc/session_fops_xc.h"
#include "rpc/session_fops.h"
#include "rpc/session_foms.h"
#include "rpc/onwire.h"
#include "rpc/onwire_xc.h"
#include "rpc/rpc.h"

/**
 * @addtogroup rpc
 * @{
 */

/**
   Initialises all the session related fop types
 */
M0_INTERNAL int m0_rpc_session_module_init(void);

/**
   Finalises all session realted fop types
 */
M0_INTERNAL void m0_rpc_session_module_fini(void);

/**
   Called for each received item.
 */
M0_INTERNAL void rpc_worker_thread_fn(struct m0_rpc_machine *machine);

/**
   Helper routine, internal to rpc module.
   Sets up and posts rpc-item representing @fop.
 */
M0_INTERNAL int m0_rpc__fop_post(struct m0_fop *fop,
				 struct m0_rpc_session *session,
				 const struct m0_rpc_item_ops *ops,
				 m0_time_t abs_timeout);

/**
   Posts rpc item while having rpc machine already locked.
 */
M0_INTERNAL int m0_rpc__post_locked(struct m0_rpc_item *item);

/**
   Temporary routine to place fop in a global queue, from where it can be
   selected for execution.
 */
M0_INTERNAL int m0_rpc_item_dispatch(struct m0_rpc_item *item);

M0_INTERNAL void m0_rpc_oneway_item_post_locked(const struct m0_rpc_conn *conn,
						struct m0_rpc_item *item);
/**
 * Takes a unique fid and hashes it with id generated from timestamp seed to
 * generate clusterwide unique RPC id.
 */
M0_INTERNAL uint64_t m0_rpc_id_generate(const struct m0_fid *uniq_fid);

M0_INTERNAL int m0_rpc_service_start(struct m0_reqh *reqh);
M0_INTERNAL void m0_rpc_service_stop(struct m0_reqh *reqh);

M0_TL_DESCR_DECLARE(item_source, M0_EXTERN);
M0_TL_DECLARE(item_source, M0_INTERNAL, struct m0_rpc_item_source);

static inline struct m0_rpc_conn *item2conn(const struct m0_rpc_item *item)
{
	return item->ri_session->s_conn;
}

/** @} */

#endif /* __MOTR_RPC_INT_H__ */
