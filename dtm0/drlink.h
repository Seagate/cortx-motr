/* -*- C -*- */
/*
 * Copyright (c) 2021 Seagate Technology LLC and/or its Affiliates
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

#ifndef __MOTR_DTM0_DRLINK_H__
#define __MOTR_DTM0_DRLINK_H__

/*
 * DTM0 RPC Link is a lazy ("on-demand") RPC link.
 *
 * Whenever ::m0_dtm0_req_post is called, it launches a FOM that is trying
 * to connect to the target process (if there is no connection) and then
 * it sends the DTM0 message (for example, PERSISTENT).
 *
 */

/* import */
#include "lib/types.h"
struct dtm0_req_fop;
struct m0_dtm0_service;
struct m0_fid;
struct m0_fom;
struct m0_be_op;

M0_INTERNAL int  m0_dtm0_rpc_link_mod_init(void);
M0_INTERNAL void m0_dtm0_rpc_link_mod_fini(void);

/**
 * Asynchronously send a DTM0 message to a remote DTM0 service.
 * @param svc local DTM0 service.
 * @param op BE op to wait for completion. Could be NULL.
 * @param req DTM0 message to be sent.
 * @param tgt FID of the remote DTM0 service.
 * @param parent_fom FOM that caused this DTM0 message (used by ADDB).
 * @param wait_for_ack Determines whether the local service should wait
 *        for a reply from the counterpart. NOTE: it has nothing to do with
 *        blocking API. It just lets the FOM to revolve one more time.
 *        TODO: This flag will likely be removed after testing with HA.
 * TODO: Add a co_op or clink to support blocking mode.
 * @return An error is returned if there is not enough resources (-ENOMEM) or
 *         if the remote service does not exist in the conf cache (-ENOENT).
 */
M0_INTERNAL int m0_dtm0_req_post(struct m0_dtm0_service    *svc,
                                 struct m0_be_op           *op,
				 const struct dtm0_req_fop *req,
				 const struct m0_fid       *tgt,
				 const struct m0_fom       *parent_fom,
				 bool                       wait_for_ack);

#endif /* __MOTR_DTM0_DRLINK_H__ */

/*
 *  Local variables:
 *  c-indentation-style: "K&R"
 *  c-basic-offset: 8
 *  tab-width: 8
 *  fill-column: 80
 *  scroll-step: 1
 *  End:
 */
/*
 * vim: tabstop=8 shiftwidth=8 noexpandtab textwidth=80 nowrap
 */

