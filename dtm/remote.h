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

#ifndef __MOTR_DTM_REMOTE_H__
#define __MOTR_DTM_REMOTE_H__


/**
 * @addtogroup dtm
 *
 * @{
 */

#include "lib/types.h"               /* m0_uint128 */
#include "xcode/xcode_attr.h"
#include "dtm/update.h"              /* m0_dtm_history_id */
#include "dtm/update_xc.h"           /* m0_dtm_history_id_xc */

/* import */
#include "dtm/fol.h"
struct m0_dtm_oper;
struct m0_dtm_update;
struct m0_rpc_conn;
struct m0_dtm_remote;
struct m0_reqh;

/* export */
struct m0_dtm_remote;
struct m0_dtm_rpc_remote;
struct m0_dtm_remote_ops;

struct m0_dtm_remote {
	struct m0_uint128               re_id;
	uint64_t                        re_instance;
	const struct m0_dtm_remote_ops *re_ops;
	struct m0_dtm_fol_remote        re_fol;
};

struct m0_dtm_remote_ops {
	void (*reo_persistent)(struct m0_dtm_remote *rem,
			       struct m0_dtm_history *history);
	void (*reo_fixed)(struct m0_dtm_remote *rem,
			  struct m0_dtm_history *history);
	void (*reo_reset)(struct m0_dtm_remote *rem,
			  struct m0_dtm_history *history);
	void (*reo_undo)(struct m0_dtm_remote *rem,
			 struct m0_dtm_history *history, m0_dtm_ver_t upto);
	void (*reo_send)(struct m0_dtm_remote *rem,
			 struct m0_dtm_update *update);
	void (*reo_resend)(struct m0_dtm_remote *rem,
			   struct m0_dtm_update *update);
};

M0_INTERNAL void m0_dtm_remote_init(struct m0_dtm_remote *remote,
				    struct m0_uint128 *id,
				    struct m0_dtm *local);
M0_INTERNAL void m0_dtm_remote_fini(struct m0_dtm_remote *remote);

M0_INTERNAL void m0_dtm_remote_add(struct m0_dtm_remote *rem,
				   struct m0_dtm_oper *oper,
				   struct m0_dtm_history *history,
				   struct m0_dtm_update *update);

struct m0_dtm_rpc_remote {
	struct m0_dtm_remote  rpr_rem;
	struct m0_rpc_conn   *rpr_conn;
};

M0_INTERNAL void m0_dtm_rpc_remote_init(struct m0_dtm_rpc_remote *remote,
					struct m0_uint128 *id,
					struct m0_dtm *local,
					struct m0_rpc_conn *conn);
M0_INTERNAL void m0_dtm_rpc_remote_fini(struct m0_dtm_rpc_remote *remote);

struct m0_dtm_notice {
	struct m0_dtm_history_id dno_id;
	uint64_t                 dno_ver;
	uint8_t                  dno_opcode;
} M0_XCA_RECORD M0_XCA_DOMAIN(rpc);

struct m0_dtm_local_remote {
	struct m0_dtm_remote  lre_rem;
	struct m0_reqh       *lre_reqh;
};

M0_INTERNAL void m0_dtm_local_remote_init(struct m0_dtm_local_remote *lre,
					  struct m0_uint128 *id,
					  struct m0_dtm *local,
					  struct m0_reqh *reqh);
M0_INTERNAL void m0_dtm_local_remote_fini(struct m0_dtm_local_remote *remote);

/** @} end of dtm group */

#endif /* __MOTR_DTM_REMOTE_H__ */


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
