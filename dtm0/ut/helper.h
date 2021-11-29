/* -*- C -*- */
/*
 * Copyright (c) 2013-2020 Seagate Technology LLC and/or its Affiliates
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

#ifndef __MOTR_DTM0_UT_HELPER_H__
#define __MOTR_DTM0_UT_HELPER_H__

/**
 * @defgroup dtm0
 *
 * @{
 */

#include "net/net.h"            /* m0_net_domain */
#include "fid/fid.h"            /* m0_fid */
#include "rpc/rpclib.h"         /* m0_rpc_server_ctx */


struct m0_reqh;
struct m0_dtm0_service;


struct m0_ut_dtm0_helper {
	struct m0_rpc_server_ctx  udh_sctx;
	struct m0_rpc_client_ctx  udh_cctx;

	struct m0_net_domain      udh_client_net_domain;
	/**
	 * The following fields are available read-only for the users of this
	 * structure. They are populated in m0_ut_dtm0_helper_init().
	 */
	struct m0_reqh           *udh_server_reqh;
	struct m0_reqh           *udh_client_reqh;
	struct m0_fid             udh_server_dtm0_fid;
	struct m0_fid             udh_client_dtm0_fid;
	struct m0_dtm0_service   *udh_server_dtm0_service;
	struct m0_dtm0_service   *udh_client_dtm0_service;
};

M0_INTERNAL void m0_ut_dtm0_helper_init(struct m0_ut_dtm0_helper *udh);
M0_INTERNAL void m0_ut_dtm0_helper_fini(struct m0_ut_dtm0_helper *udh);


/** @} end of dtm0 group */
#endif /* __MOTR_DTM0_UT_HELPER_H__ */

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
