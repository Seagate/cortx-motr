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
 * @addtogroup ha
 *
 * @{
 */

#define M0_TRACE_SUBSYSTEM M0_TRACE_SUBSYS_HA
#include "lib/trace.h"

#include "ha/link_fops.h"
#include "ha/link_fops_xc.h"

#include "rpc/rpc_opcodes.h"    /* M0_HA_LINK_INCOMING_REQ */
#include "fop/fom_generic.h"    /* XXX m0_generic_conf */
#include "ha/link_service.h"    /* m0_ha_link_service_type */
#include "ha/link.h"            /* m0_ha_link_incoming_fom_type_ops */

struct m0_fop_type m0_ha_link_msg_fopt;
struct m0_fop_type m0_ha_link_msg_rep_fopt;

M0_INTERNAL void m0_ha_link_tags_initial(struct m0_ha_link_tags *tags,
                                         bool                    tag_even)
{
	uint64_t tag = tag_even ? 2 : 1;

	*tags = (struct m0_ha_link_tags){
		.hlt_delivered = tag,
		.hlt_confirmed = tag,
		.hlt_next      = tag,
		.hlt_assign    = tag,
	};
}

M0_INTERNAL bool m0_ha_link_tags_eq(const struct m0_ha_link_tags *tags1,
                                    const struct m0_ha_link_tags *tags2)
{
	return tags1->hlt_delivered == tags2->hlt_delivered &&
	       tags1->hlt_confirmed == tags2->hlt_confirmed &&
	       tags1->hlt_next      == tags2->hlt_next &&
	       tags1->hlt_assign    == tags2->hlt_assign;

}

M0_INTERNAL void m0_ha_link_params_invert(struct m0_ha_link_params       *dst,
                                          const struct m0_ha_link_params *src)
{
	*dst = (struct m0_ha_link_params){
		.hlp_id_local      = src->hlp_id_remote,
		.hlp_id_remote     = src->hlp_id_local,
		.hlp_id_connection = src->hlp_id_connection,
		.hlp_tags_local    = src->hlp_tags_remote,
		.hlp_tags_remote   = src->hlp_tags_local,
	};
}

M0_INTERNAL int m0_ha_link_fops_init(void)
{
	M0_FOP_TYPE_INIT(&m0_ha_link_msg_fopt,
			 .name      = "HA link msg fop",
			 .opcode    = M0_HA_LINK_MSG_REQ,
			 .xt        = m0_ha_link_msg_fop_xc,
			 .fom_ops   = &m0_ha_link_incoming_fom_type_ops,
			 .sm        = &m0_generic_conf, /* XXX make specific conf */
			 .svc_type  = &m0_ha_link_service_type,
			 .rpc_flags = M0_RPC_ITEM_TYPE_REQUEST);
	M0_FOP_TYPE_INIT(&m0_ha_link_msg_rep_fopt,
			 .name      = "HA link msg fop reply",
			 .opcode    = M0_HA_LINK_MSG_REP,
			 .xt        = m0_ha_link_msg_rep_fop_xc,
			 .svc_type  = &m0_ha_link_service_type,
			 .rpc_flags = M0_RPC_ITEM_TYPE_REPLY);
	return 0;
}

M0_INTERNAL void m0_ha_link_fops_fini(void)
{
	m0_fop_type_fini(&m0_ha_link_msg_rep_fopt);
	m0_fop_type_fini(&m0_ha_link_msg_fopt);
}

#undef M0_TRACE_SUBSYSTEM

/** @} end of ha group */

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
