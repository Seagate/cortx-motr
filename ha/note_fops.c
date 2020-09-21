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


#include "fop/fom_generic.h"
#include "rpc/rpc.h"
#include "rpc/rpc_opcodes.h"

#include "ha/note_fops.h"
#include "ha/note_fops_xc.h"
#include "ha/note_xc.h"

extern struct m0_reqh_service_type m0_rpc_service_type;

struct m0_fop_type m0_ha_state_get_fopt;
struct m0_fop_type m0_ha_state_get_rep_fopt;
struct m0_fop_type m0_ha_state_set_fopt;

M0_INTERNAL void m0_ha_state_fop_fini(void)
{
	m0_fop_type_fini(&m0_ha_state_get_fopt);
	m0_fop_type_fini(&m0_ha_state_get_rep_fopt);
	m0_fop_type_fini(&m0_ha_state_set_fopt);
}

M0_INTERNAL int m0_ha_state_fop_init(void)
{
	M0_FOP_TYPE_INIT(&m0_ha_state_get_fopt,
			 .name      = "HA State Get",
			 .opcode    = M0_HA_NOTE_GET_OPCODE,
			 .xt        = m0_ha_nvec_xc,
			 .fom_ops   = m0_ha_state_get_fom_type_ops,
			 .sm        = &m0_generic_conf,
			 .svc_type  = &m0_rpc_service_type,
			 .rpc_flags = M0_RPC_ITEM_TYPE_REQUEST);
	M0_FOP_TYPE_INIT(&m0_ha_state_get_rep_fopt,
			 .name      = "HA State Get Reply",
			 .opcode    = M0_HA_NOTE_GET_REP_OPCODE,
			 .xt        = m0_ha_state_fop_xc,
			 .svc_type  = &m0_rpc_service_type,
			 .rpc_flags = M0_RPC_ITEM_TYPE_REPLY);
	M0_FOP_TYPE_INIT(&m0_ha_state_set_fopt,
			 .name      = "HA State Set",
			 .opcode    = M0_HA_NOTE_SET_OPCODE,
			 .xt        = m0_ha_nvec_xc,
			 .fom_ops   = m0_ha_state_set_fom_type_ops,
			 .sm        = &m0_generic_conf,
			 .svc_type  = &m0_rpc_service_type,
			 .rpc_flags = M0_RPC_ITEM_TYPE_REQUEST);
	return 0;
}


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
