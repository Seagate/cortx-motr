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

#ifndef __MOTR_DTM0_DTX_H__
#define __MOTR_DTM0_DTX_H__

#include "dtm0/tx_desc.h" /* m0_dtm0_tx_desc */
#include "sm/sm.h"        /* m0_sm */
#include "dtm/dtm.h"      /* m0_dtx */

enum m0_dtm0_dtx_state {
	/** An empty dtx. */
	M0_DDS_INIT,
	/* dtx has a valid tx record. */
	M0_DDS_INPROGRESS,
	/* dtx got one reply. */
	M0_DDS_EXECUTED,
	/* dtx got all replies. */
	M0_DDS_EXECUTED_ALL,
	/* dtx got enough PERSISTENT notices. */
	M0_DDS_STABLE,
	/* dtx can be released when this state reached. */
	M0_DDS_DONE,
	/* dtx can fail. */
	M0_DDS_FAILED,
	M0_DDS_NR,
};

struct m0_dtm0_dtx {
	/** An imprint of the ancient version of dtx.
	 * This DTX was created at the beginning of the time, and it was
	 * propagated all over the codebase. Since it is really hard to
	 * remove it, we instead venerate it by providing the very first
	 * place in this new dtm0 structure. It helps to have a simple
	 * typecast (without branded object) and no extra allocations for
	 * this ancient but not really useful at this moment structure.
	 */
	struct m0_dtx           dd_ancient_dtx;
	/* see m0_dtm0_dtx_state */
	struct m0_sm            dd_sm;
	struct m0_dtm0_tx_desc  dd_txd;
	struct m0_dtm0_service *dd_dtms;
	uint32_t                dd_nr_executed;
	struct m0_sm_ast        dd_exec_all_ast;

	/* XXX: The implementation is very simple and it relies on the idea
	 * of fully replicated requests. Therefore, all FOP submitted
	 * as a part of this DTX should to be identical (at least
	 * from the REDO mechanism standpoint). So that we do not use
	 * any lists here. But we will have to some time later.
	 */
	const struct m0_fop    *dd_fop;
};

M0_INTERNAL void m0_dtm0_dtx_domain_init(void);
M0_INTERNAL void m0_dtm0_dtx_domain_fini(void);

/* The API below extends the existing m0_dtx API. Since it operates
 * on m0_dtx structure in the "DTM0"-way, the naming convention here is
 * a bit different (m0_dtx + dtm0 => m0_dtx0).
 */

M0_INTERNAL struct m0_dtx* m0_dtx0_alloc(struct m0_dtm0_service *svc,
					 struct m0_sm_group     *group);
M0_INTERNAL void m0_dtx0_free(struct m0_dtx *dtx);

/** Assignsa a TID to the transaction. */
M0_INTERNAL int m0_dtx0_prepare(struct m0_dtx *dtx);

M0_INTERNAL int m0_dtx0_open(struct m0_dtx  *dtx, uint32_t nr);

/** Fills in the FID of the given participant. */
M0_INTERNAL int m0_dtx0_assign_fid(struct m0_dtx       *dtx,
				   uint32_t             pa_idx,
				   const struct m0_fid *pa_fid);

/** Fills in the FOP associated with the given participant. */
M0_INTERNAL void m0_dtx0_assign_fop(struct m0_dtx       *dtx,
				    uint32_t             pa_idx,
				    const struct m0_fop *pa_fop);
M0_INTERNAL int m0_dtx0_close(struct m0_dtx *dtx);

M0_INTERNAL void m0_dtx0_executed(struct m0_dtx *dtx, uint32_t pa_idx);

/** Puts a copy of dtx's transaction descriptor into "dst".
 * User is responsible for m0_dtm0_tx_desc_fini()lasing of 'dst'.
 * If dtx is NULL then dst will be filled with the empty tx_desc.
 */
M0_INTERNAL int m0_dtx0_copy_txd(const struct m0_dtx    *dtx,
				 struct m0_dtm0_tx_desc *dst);

M0_INTERNAL enum m0_dtm0_dtx_state m0_dtx0_sm_state(const struct m0_dtx *dtx);

#endif /* __MOTR_DTM0_DTX_H__ */

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
