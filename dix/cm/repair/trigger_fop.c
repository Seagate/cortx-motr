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
#include "cm/repreb/trigger_fop.h"
#include "cm/repreb/trigger_fop_xc.h"
#include "cm/repreb/trigger_fom.h"
#include "dix/cm/cm.h"
#include "dix/cm/trigger_fom.h"
#include "dix/cm/trigger_fop.h"
#include "rpc/item.h"

/**
 * Finalises start, quiesce, status, abort repair trigger FOP and
 * corresponding reply FOP types.
 *
 * @see m0_cm_trigger_fop_fini()
 */
M0_INTERNAL void m0_dix_cm_repair_trigger_fop_fini(void)
{
	m0_cm_trigger_fop_fini(&m0_dix_repair_trigger_fopt);
	m0_cm_trigger_fop_fini(&m0_dix_repair_trigger_rep_fopt);
	m0_cm_trigger_fop_fini(&m0_dix_repair_quiesce_fopt);
	m0_cm_trigger_fop_fini(&m0_dix_repair_quiesce_rep_fopt);
	m0_cm_trigger_fop_fini(&m0_dix_repair_status_fopt);
	m0_cm_trigger_fop_fini(&m0_dix_repair_status_rep_fopt);
	m0_cm_trigger_fop_fini(&m0_dix_repair_abort_fopt);
	m0_cm_trigger_fop_fini(&m0_dix_repair_abort_rep_fopt);
}

/**
 * Initialises start, quiesce, status, abort repair trigger FOP and
 * corresponding reply FOP types.
 *
 * @see m0_cm_trigger_fop_init()
 */
M0_INTERNAL void m0_dix_cm_repair_trigger_fop_init(void)
{
	m0_cm_trigger_fop_init(&m0_dix_repair_trigger_fopt,
			       M0_DIX_REPAIR_TRIGGER_OPCODE,
			       "dix repair trigger",
			       trigger_fop_xc,
			       M0_RPC_MUTABO_REQ,
			       &dix_repair_cmt,
			       &m0_dix_trigger_fom_type_ops);
	m0_cm_trigger_fop_init(&m0_dix_repair_trigger_rep_fopt,
			       M0_DIX_REPAIR_TRIGGER_REP_OPCODE,
			       "dix repair trigger reply",
			       trigger_rep_fop_xc,
			       M0_RPC_ITEM_TYPE_REPLY,
			       &dix_repair_cmt,
			       &m0_dix_trigger_fom_type_ops);

	m0_cm_trigger_fop_init(&m0_dix_repair_quiesce_fopt,
			       M0_DIX_REPAIR_QUIESCE_OPCODE,
			       "dix repair quiesce trigger",
			       trigger_fop_xc,
			       M0_RPC_MUTABO_REQ,
			       &dix_repair_cmt,
			       &m0_dix_trigger_fom_type_ops);
	m0_cm_trigger_fop_init(&m0_dix_repair_quiesce_rep_fopt,
			       M0_DIX_REPAIR_QUIESCE_REP_OPCODE,
			       "dix repair quiesce trigger reply",
			       trigger_rep_fop_xc,
			       M0_RPC_ITEM_TYPE_REPLY,
			       &dix_repair_cmt,
			       &m0_dix_trigger_fom_type_ops);

	m0_cm_trigger_fop_init(&m0_dix_repair_status_fopt,
			       M0_DIX_REPAIR_STATUS_OPCODE,
			       "dix repair status",
			       trigger_fop_xc,
			       M0_RPC_MUTABO_REQ,
			       &dix_repair_cmt,
			       &m0_dix_trigger_fom_type_ops);
	m0_cm_trigger_fop_init(&m0_dix_repair_status_rep_fopt,
			       M0_DIX_REPAIR_STATUS_REP_OPCODE,
			       "dix repair status reply",
			       m0_status_rep_fop_xc,
			       M0_RPC_ITEM_TYPE_REPLY,
			       &dix_repair_cmt,
			       &m0_dix_trigger_fom_type_ops);
	m0_cm_trigger_fop_init(&m0_dix_repair_abort_fopt,
			       M0_DIX_REPAIR_ABORT_OPCODE,
			       "dix repair abort",
			       trigger_fop_xc,
			       M0_RPC_MUTABO_REQ,
			       &dix_repair_cmt,
			       &m0_dix_trigger_fom_type_ops);
	m0_cm_trigger_fop_init(&m0_dix_repair_abort_rep_fopt,
			       M0_DIX_REPAIR_ABORT_REP_OPCODE,
			       "dix repair abort reply",
			       trigger_rep_fop_xc,
			       M0_RPC_ITEM_TYPE_REPLY,
			       &dix_repair_cmt,
			       &m0_dix_trigger_fom_type_ops);
}



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
