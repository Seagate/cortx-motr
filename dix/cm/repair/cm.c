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



/**
 * @addtogroup DIXCM
 *
 * @{
 */

#define M0_TRACE_SUBSYSTEM M0_TRACE_SUBSYS_DIXCM
#include "dix/cm/cp.h"
#include "dix/cm/cm.h"
#include "lib/trace.h"
#include "cm/repreb/cm.h"
#include "layout/pdclust.h"

M0_INTERNAL
int m0_dix_repair_sw_onwire_fop_setup(struct m0_cm *cm, struct m0_fop *fop,
				      void (*fop_release)(struct m0_ref *),
				      uint64_t proxy_id, const char *local_ep,
				      const struct m0_cm_sw *sw,
				      const struct m0_cm_sw *out_interval);

M0_INTERNAL int m0_dix_cm_ag_alloc(struct m0_cm *cm,
				   const struct m0_cm_ag_id *id,
				   bool has_incoming,
				   struct m0_cm_aggr_group **out);

static int dix_repair_cm_prepare(struct m0_cm *cm)
{
	struct m0_dix_cm *dcm = cm2dix(cm);

	M0_ENTRY("cm: %p", cm);
	M0_PRE(M0_IN(dcm->dcm_op, (CM_OP_REPAIR, CM_OP_REPAIR_RESUME)));
	return 0;
}

static struct m0_cm_cp *dix_repair_cm_cp_alloc(struct m0_cm *cm)
{
	struct m0_cm_cp *cp;

	cp = m0_dix_cm_cp_alloc(cm);
	if (cp != NULL)
		cp->c_ops = &m0_dix_cm_repair_cp_ops;
	return cp;
}

static void dix_repair_cm_stop(struct m0_cm *cm)
{
	struct m0_dix_cm *dcm = cm2dix(cm);

	M0_ENTRY();
	M0_PRE(M0_IN(dcm->dcm_op, (CM_OP_REPAIR, CM_OP_REPAIR_RESUME)));
	m0_dix_cm_stop(cm);
	M0_LEAVE();
}

/** Copy machine operations. */
const struct m0_cm_ops dix_repair_ops = {
	.cmo_setup               = m0_dix_cm_setup,
	.cmo_prepare             = dix_repair_cm_prepare,
	.cmo_start               = m0_dix_cm_start,
	.cmo_ag_alloc            = m0_dix_cm_ag_alloc,
	.cmo_cp_alloc            = dix_repair_cm_cp_alloc,
	.cmo_data_next           = m0_dix_cm_data_next,
	.cmo_ag_next             = m0_dix_cm_ag_next,
	.cmo_get_space_for       = m0_dix_get_space_for,
	.cmo_sw_onwire_fop_setup = m0_dix_repair_sw_onwire_fop_setup,
	.cmo_is_peer             = m0_dix_is_peer,
	.cmo_stop                = dix_repair_cm_stop,
	.cmo_fini                = m0_dix_cm_fini
};


#undef M0_TRACE_SUBSYSTEM

/** @} end of DIXCM group */

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
