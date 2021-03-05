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

#ifndef __MOTR_RPC_SERVICE_H__
#define __MOTR_RPC_SERVICE_H__

#include "rpc/session.h"
#include "rpc/item.h"
#include "reqh/reqh_service.h"

/**
   @defgroup rpc_service RPC service
   @{
 */

struct m0_rpc_service {
	/** Reqh service representation */
	struct m0_reqh_service rps_svc;
	/** List maintaining reverse connections to clients */
	struct m0_tl           rps_rev_conns;
	/** magic == M0_RPC_SERVICE_MAGIC */
	uint64_t               rps_magix;
};

M0_INTERNAL int m0_rpc_service_register(void);
M0_INTERNAL void m0_rpc_service_unregister(void);

/**
 * Return reverse session to given item.
 *
 * @pre svc != NULL
 * @pre item != NULL && session != NULL
 */
M0_INTERNAL int
m0_rpc_service_reverse_session_get(struct m0_reqh_service   *service,
				   const struct m0_rpc_item *item,
				   struct m0_clink          *clink,
				   struct m0_rpc_session   **session);

M0_INTERNAL void
m0_rpc_service_reverse_session_put(struct m0_rpc_session *session);

M0_INTERNAL void
m0_rpc_service_reverse_sessions_cleanup(struct m0_reqh_service *service);

M0_INTERNAL int m0_rpc_session_status(struct m0_rpc_session *session);

M0_INTERNAL struct m0_rpc_session *
m0_rpc_service_reverse_session_lookup(struct m0_reqh_service    *service,
				      const struct m0_rpc_item *item);

M0_INTERNAL struct m0_reqh_service *
m0_reqh_rpc_service_find(struct m0_reqh *reqh);

/**
   @} end of rpc_service group
 */
#endif /* __MOTR_RPC_SERVICE_H__ */

/*
 *  Local variables:
 *  c-indentation-style: "K&R"
 *  c-basic-offset: 8
 *  tab-width: 8
 *  fill-column: 80
 *  scroll-step: 1
 *  End:
 */
