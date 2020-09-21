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

#ifndef __MOTR_DTM_OPERATION_H__
#define __MOTR_DTM_OPERATION_H__


/**
 * @addtogroup dtm
 *
 * @{
 */

/* import */
#include "dtm/nucleus.h"
#include "dtm/update.h"
#include "dtm/update_xc.h"
struct m0_dtm_remote;
struct m0_dtm;
struct m0_tl;

/* export */
struct m0_dtm_oper;

struct m0_dtm_oper {
	struct m0_dtm_op oprt_op;
	struct m0_tl     oprt_uu;
	uint64_t         oprt_flags;
};
M0_INTERNAL bool m0_dtm_oper_invariant(const struct m0_dtm_oper *oper);

enum m0_dtm_oper_flags {
	M0_DOF_CLOSED = 1 << 0,
	M0_DOF_LAST   = 1 << 1,
	M0_DOF_SENT   = 1 << 2
};

struct m0_dtm_oper_updates {
	uint32_t                    ou_nr;
	struct m0_dtm_update_descr *ou_update;
} M0_XCA_SEQUENCE M0_XCA_DOMAIN(rpc);

struct m0_dtm_oper_descr {
	struct m0_dtm_oper_updates od_updates;
} M0_XCA_RECORD M0_XCA_DOMAIN(rpc);

M0_INTERNAL void m0_dtm_oper_init(struct m0_dtm_oper *oper, struct m0_dtm *dtm,
				  struct m0_tl *uu);
M0_INTERNAL void m0_dtm_oper_fini(struct m0_dtm_oper *oper);
M0_INTERNAL void m0_dtm_oper_close(struct m0_dtm_oper *oper);
M0_INTERNAL void m0_dtm_oper_prepared(const struct m0_dtm_oper *oper,
				      const struct m0_dtm_remote *rem);
M0_INTERNAL void m0_dtm_oper_done(const struct m0_dtm_oper *oper,
				  const struct m0_dtm_remote *rem);
M0_INTERNAL void m0_dtm_oper_pack(struct m0_dtm_oper *oper,
				  const struct m0_dtm_remote *rem,
				  struct m0_dtm_oper_descr *ode);
M0_INTERNAL void m0_dtm_oper_unpack(struct m0_dtm_oper *oper,
				    const struct m0_dtm_oper_descr *ode);
M0_INTERNAL int  m0_dtm_oper_build(struct m0_dtm_oper *oper, struct m0_tl *uu,
				   const struct m0_dtm_oper_descr *ode);
M0_INTERNAL void m0_dtm_reply_pack(const struct m0_dtm_oper *oper,
				   const struct m0_dtm_oper_descr *request,
				   struct m0_dtm_oper_descr *reply);
M0_INTERNAL void m0_dtm_reply_unpack(struct m0_dtm_oper *oper,
				     const struct m0_dtm_oper_descr *reply);

M0_INTERNAL struct m0_dtm_update *m0_dtm_oper_get(const struct m0_dtm_oper *oper,
						  uint32_t label);

/** @} end of dtm group */

#endif /* __MOTR_DTM_OPERATION_H__ */


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
