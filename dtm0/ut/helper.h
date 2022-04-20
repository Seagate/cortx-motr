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

struct dtm0_ut_log_ctx {
	struct m0_be_ut_backend   ut_be;
	struct m0_be_ut_seg       ut_seg;
	struct m0_dtm0_domain_cfg dod_cfg;
	struct m0_dtm0_log        dol;
};

M0_INTERNAL struct dtm0_ut_log_ctx *dtm0_ut_log_init(void);
M0_INTERNAL void dtm0_ut_log_fini(struct dtm0_ut_log_ctx *lctx);

M0_INTERNAL struct m0_dtm0_redo *dtm0_ut_redo_get(int timestamp);
M0_INTERNAL void dtm0_ut_redo_put(struct m0_dtm0_redo *redo);

enum {
	MPSC_TS_BASE = 0,
};

enum {
	MPSC_NR_REC_TOTAL = 0x100,
};

struct dtm0_ut_log_mp_ctx {
	struct m0_be_tx_bulk_cfg *tb_cfg;
	struct m0_be_tx_bulk     *tb;
	struct m0_be_op          *op;
	struct dtm0_ut_log_ctx   *lctx;
};

M0_INTERNAL void dtm0_ut_log_mp_init(struct dtm0_ut_log_mp_ctx *lmp_ctx,
				     struct dtm0_ut_log_ctx    *lctx);

M0_INTERNAL void dtm0_ut_log_mp_run(struct dtm0_ut_log_mp_ctx *lmp_ctx);

M0_INTERNAL void dtm0_ut_log_mp_fini(struct dtm0_ut_log_mp_ctx *lmp_ctx);

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
