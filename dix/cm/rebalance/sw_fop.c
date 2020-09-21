/* -*- C -*- */
/*
 * Copyright (c) 2016-2020 Seagate Technology LLC and/or its Affiliates
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



#define M0_TRACE_SUBSYSTEM M0_TRACE_SUBSYS_DIX
#include "lib/trace.h"

#include "fop/fop.h"

#include "cm/cm.h"

#include "cm/repreb/sw_onwire_fop.h"
#include "cm/repreb/sw_onwire_fom.h"

#include "cm/repreb/sw_onwire_fop_xc.h"

/**
 * @addtogroup dix
 *
 * @{
 */

struct m0_fop_type dix_rebalance_sw_onwire_fopt;
struct m0_fop_type dix_rebalance_sw_onwire_rep_fopt;
extern struct m0_cm_type dix_rebalance_cmt;

static int dix_rebalance_sw_fom_create(struct m0_fop *fop, struct m0_fom **m,
				       struct m0_reqh *reqh)
{
	struct m0_fop *r_fop;
	int            rc;

	r_fop = m0_fop_reply_alloc(fop, &dix_rebalance_sw_onwire_rep_fopt);
	if (r_fop == NULL)
		return M0_ERR(-ENOMEM);
	rc = m0_cm_repreb_sw_onwire_fom_create(fop, r_fop, m, reqh);

	return M0_RC(rc);
}

const struct m0_fom_type_ops dix_rebalance_sw_fom_type_ops = {
	.fto_create = dix_rebalance_sw_fom_create
};

/** @see m0_cm_repreb_sw_onwire_fop_init() */
M0_INTERNAL void m0_dix_rebalance_sw_onwire_fop_init(void)
{
        m0_cm_repreb_sw_onwire_fop_init(&dix_rebalance_sw_onwire_fopt,
					&dix_rebalance_sw_fom_type_ops,
					M0_DIX_CM_REBALANCE_SW_FOP_OPCODE,
					"dix cm sw update fop",
					m0_cm_sw_onwire_xc,
					M0_RPC_ITEM_TYPE_REQUEST,
					&dix_rebalance_cmt);
        m0_cm_repreb_sw_onwire_fop_init(&dix_rebalance_sw_onwire_rep_fopt,
					&dix_rebalance_sw_fom_type_ops,
					M0_DIX_CM_REBALANCE_SW_REP_FOP_OPCODE,
					"dix cm sw update rep fop",
					m0_cm_sw_onwire_rep_xc,
					M0_RPC_ITEM_TYPE_REPLY,
					&dix_rebalance_cmt);
}

/** @see m0_cm_repreb_sw_onwire_fop_fini() */
M0_INTERNAL void m0_dix_rebalance_sw_onwire_fop_fini(void)
{
	m0_cm_repreb_sw_onwire_fop_fini(&dix_rebalance_sw_onwire_fopt);
	m0_cm_repreb_sw_onwire_fop_fini(&dix_rebalance_sw_onwire_rep_fopt);
}

/** @see m0_cm_repreb_sw_onwire_fop_setup() */
M0_INTERNAL int
m0_dix_rebalance_sw_onwire_fop_setup(struct m0_cm *cm, struct m0_fop *fop,
				     void (*fop_release)(struct m0_ref *),
				     uint64_t proxy_id, const char *local_ep,
				     const struct m0_cm_sw *sw,
				     const struct m0_cm_sw *out_interval)
{
	return m0_cm_repreb_sw_onwire_fop_setup(cm, &dix_rebalance_sw_onwire_fopt,
						fop, fop_release, proxy_id,
						local_ep, sw, out_interval);
}

#undef M0_TRACE_SUBSYSTEM

/** @} end of dix group */

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
