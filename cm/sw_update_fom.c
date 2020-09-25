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


#define M0_TRACE_SUBSYSTEM M0_TRACE_SUBSYS_CM
#include "lib/trace.h"

#include "lib/bob.h"
#include "lib/misc.h"  /* M0_BITS */
#include "lib/errno.h" /* ENOENT EPERM */

#include "reqh/reqh.h"
#include "sm/sm.h"
#include "rpc/rpc_opcodes.h" /* M0_CM_SW_UPDATE_OPCODE */

#include "cm/sw.h"
#include "cm/cm.h"
#include "cm/proxy.h"

#include "be/op.h"           /* M0_BE_OP_SYNC */

/**
   @defgroup CMSWFOM sliding window update fom
   @ingroup CMSW

   Implementation of sliding window update FOM.
   Provides mechanism to handle blocking operations like local sliding
   update and updating the persistent store with new sliding window.
   Provides interfaces to start, wakeup (if idle) and stop the sliding
   window update FOM.

   @{
*/

enum cm_sw_update_fom_phase {
	SWU_UPDATE = M0_FOM_PHASE_INIT,
	SWU_FINI   = M0_FOM_PHASE_FINISH,
	SWU_NR
};

static const struct m0_fom_type_ops cm_sw_update_fom_type_ops = {
};

static struct m0_sm_state_descr cm_sw_update_sd[SWU_NR] = {
	[SWU_UPDATE] = {
		.sd_flags   = M0_SDF_INITIAL,
		.sd_name    = "Update",
		.sd_allowed = M0_BITS(SWU_FINI)
	},
	[SWU_FINI] = {
		.sd_flags   = M0_SDF_TERMINAL,
		.sd_name    = "Fini",
		.sd_allowed = 0
	},
};

struct m0_sm_conf cm_sw_update_conf = {
	.scf_name      = "sm: sw update conf",
	.scf_nr_states = ARRAY_SIZE(cm_sw_update_sd),
	.scf_state     = cm_sw_update_sd,
};

static struct m0_cm *cm_swu2cm(struct m0_cm_sw_update *swu)
{
	return container_of(swu, struct m0_cm, cm_sw_update);
}

static struct m0_cm_sw_update *cm_fom2swu(struct m0_fom *fom)
{
	return container_of(fom, struct m0_cm_sw_update, swu_fom);
}

static int swu_update(struct m0_cm_sw_update *swu)
{
	struct m0_cm *cm = cm_swu2cm(swu);
	int           rc;
	M0_ENTRY();

	rc = m0_cm_sw_local_update(cm);
	return M0_RC(rc);
}

static int (*swu_action[]) (struct m0_cm_sw_update *swu) = {
	[SWU_UPDATE] = swu_update,
};

static size_t cm_swu_fom_locality(const struct m0_fom *fom)
{
	return fom->fo_type->ft_id;
}
static int cm_swu_fom_tick(struct m0_fom *fom)
{
	struct m0_cm           *cm;
	struct m0_cm_sw_update *swu;
	int                     phase = m0_fom_phase(fom);
	int                     rc;
	M0_ENTRY();

	swu = cm_fom2swu(fom);
	cm = cm_swu2cm(swu);
	m0_cm_lock(cm);
	rc = swu_action[phase](swu);
	if (rc != 0) {
		if (!M0_IN(rc, (-ENOBUFS, -ENODATA)))
			M0_LOG(M0_ERROR, "SWU phase=%d rc=%d.", phase, rc);
		else
			M0_LOG(M0_DEBUG, "SWU phase=%d rc=%d.", phase, rc);

		m0_cm_sw_remote_update(cm);
	}
	if (rc < 0) {
		if (rc != -ENOBUFS) {
			M0_LOG(M0_DEBUG, "Sliding window update"
					 " fom complete with rc: %d", rc);
			m0_cm_sw_update_complete(cm);
			m0_cm_complete_notify(cm);
			if (!M0_IN(rc, (-ENOBUFS, -ENODATA)))
				m0_cm_abort(cm, rc);
			m0_fom_phase_move(fom, 0, SWU_FINI);
		}
		rc = M0_FSO_WAIT;
		m0_cm_sw_remote_update(cm);
	}
	m0_cm_unlock(cm);

	return M0_RC(rc);
}

static void cm_swu_fom_fini(struct m0_fom *fom)
{
	m0_fom_fini(fom);
}

static const struct m0_fom_ops cm_sw_update_fom_ops = {
	.fo_fini          = cm_swu_fom_fini,
	.fo_tick          = cm_swu_fom_tick,
	.fo_home_locality = cm_swu_fom_locality
};

M0_INTERNAL void m0_cm_sw_update_init(struct m0_cm_type *cmtype)
{
	m0_fom_type_init(&cmtype->ct_swu_fomt, cmtype->ct_fom_id + 1,
			 &cm_sw_update_fom_type_ops,
			 &cmtype->ct_stype, &cm_sw_update_conf);
}

M0_INTERNAL void m0_cm_sw_update_start(struct m0_cm *cm)
{
	struct m0_cm_sw_update *swu = &cm->cm_sw_update;
	struct m0_fom          *fom = &swu->swu_fom;

	M0_ENTRY();

	swu->swu_is_complete = false;
	m0_fom_init(&cm->cm_sw_update.swu_fom, &cm->cm_type->ct_swu_fomt,
		    &cm_sw_update_fom_ops, NULL, NULL, cm->cm_service.rs_reqh);
	m0_fom_queue(fom);

	M0_LEAVE();
}

M0_INTERNAL void m0_cm_sw_update_complete(struct m0_cm *cm)
{
	struct m0_cm_sw_update *swu = &cm->cm_sw_update;

	swu->swu_is_complete = true;
}

#undef M0_TRACE_SUBSYSTEM

/** @} endgroup CMSWFOM */

/*
 *  Local variables:
 *  c-indentation-style: "K&R"
 *  c-basic-offset: 8
 *  tab-width: 8
 *  fill-column: 80
 *  scroll-step: 1
 *  End:
 */
