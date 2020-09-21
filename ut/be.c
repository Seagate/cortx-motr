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


/**
 * @addtogroup ut
 *
 * @{
 */

#include "ut/be.h"

#define M0_TRACE_SUBSYSTEM M0_TRACE_SUBSYS_UT
#include "lib/trace.h"

#include "ut/ut.h"         /* M0_UT_ASSERT */
#include "be/ut/helper.h"  /* m0_be_ut_backend */
#include "be/op.h"         /* M0_BE_OP_SYNC */
#include "lib/misc.h"      /* M0_BITS */
#include "lib/memory.h"    /* M0_ALLOC_PTR */

#include "reqh/reqh.h"

M0_INTERNAL void
m0_ut_backend_init(struct m0_be_ut_backend *be, struct m0_be_ut_seg *seg)
{
	m0_be_ut_backend_init(be);
	m0_be_ut_seg_init(seg, be, 1 << 20 /* 1 MB */);
}

M0_INTERNAL void
m0_ut_backend_fini(struct m0_be_ut_backend *be, struct m0_be_ut_seg *seg)
{
	m0_be_ut_seg_fini(seg);
	m0_be_ut_backend_fini(be);
}

M0_INTERNAL void m0_ut_be_tx_begin(struct m0_be_tx *tx,
				   struct m0_be_ut_backend *ut_be,
				   struct m0_be_tx_credit *cred)
{
	int rc;

	m0_be_ut_tx_init(tx, ut_be);
	m0_be_tx_prep(tx, cred);
	rc = m0_be_tx_open_sync(tx);
	M0_ASSERT(rc == 0);
}

M0_INTERNAL void m0_ut_be_tx_begin2(struct m0_be_tx *tx,
				   struct m0_be_ut_backend *ut_be,
				   struct m0_be_tx_credit *cred,
				   m0_bcount_t payload_cred)
{
	int rc;

	m0_be_ut_tx_init(tx, ut_be);
	m0_be_tx_prep(tx, cred);
	m0_be_tx_payload_prep(tx, payload_cred);
	rc = m0_be_tx_open_sync(tx);
	M0_ASSERT(rc == 0);
}

M0_INTERNAL void m0_ut_be_tx_end(struct m0_be_tx *tx)
{
	m0_be_tx_close_sync(tx);
	m0_be_tx_fini(tx);
}

#undef M0_TRACE_SUBSYSTEM

/** @} end of be group */

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
