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



#include "lib/errno.h"
#include "lib/memory.h"
#include "lib/misc.h"	/* M0_IN */
#define M0_TRACE_SUBSYSTEM M0_TRACE_SUBSYS_POOL
#include "lib/trace.h"
#include "fop/fop.h"
#include "fop/fop_item_type.h"
#include "fop/fom_generic.h"
#include "rpc/item.h"
#include "rpc/rpc_opcodes.h"
#include "rpc/rpc.h"
#include "pool/pool_fops_xc.h"

struct m0_fop_type m0_fop_poolmach_query_fopt;
struct m0_fop_type m0_fop_poolmach_query_rep_fopt;
struct m0_fop_type m0_fop_poolmach_set_fopt;
struct m0_fop_type m0_fop_poolmach_set_rep_fopt;

M0_INTERNAL void m0_poolmach_fop_fini(void)
{
	m0_fop_type_fini(&m0_fop_poolmach_query_fopt);
	m0_fop_type_fini(&m0_fop_poolmach_query_rep_fopt);
	m0_fop_type_fini(&m0_fop_poolmach_set_fopt);
	m0_fop_type_fini(&m0_fop_poolmach_set_rep_fopt);
}

extern struct m0_reqh_service_type m0_ios_type;
extern const struct m0_fom_type_ops poolmach_fom_type_ops;

extern const struct m0_sm_conf poolmach_conf;
extern struct m0_sm_state_descr poolmach_phases[];

M0_INTERNAL int m0_poolmach_fop_init(void)
{
	m0_sm_conf_extend(m0_generic_conf.scf_state, poolmach_phases,
			  m0_generic_conf.scf_nr_states);
	M0_FOP_TYPE_INIT(&m0_fop_poolmach_query_fopt,
			 .name      = "Pool Machine query request",
			 .opcode    = M0_POOLMACHINE_QUERY_OPCODE,
			 .xt        = m0_fop_poolmach_query_xc,
			 .rpc_flags = M0_RPC_ITEM_TYPE_REQUEST,
			 .fom_ops   = &poolmach_fom_type_ops,
			 .svc_type  = &m0_ios_type,
			 .sm        = &poolmach_conf);
	M0_FOP_TYPE_INIT(&m0_fop_poolmach_query_rep_fopt,
			 .name      = "Pool Machine query reply",
			 .opcode    = M0_POOLMACHINE_QUERY_REP_OPCODE,
			 .xt        = m0_fop_poolmach_query_rep_xc,
			 .rpc_flags = M0_RPC_ITEM_TYPE_REPLY);
	M0_FOP_TYPE_INIT(&m0_fop_poolmach_set_fopt,
			 .name      = "Pool Machine set request",
			 .opcode    = M0_POOLMACHINE_SET_OPCODE,
			 .xt        = m0_fop_poolmach_set_xc,
			 .rpc_flags = M0_RPC_MUTABO_REQ,
			 .fom_ops   = &poolmach_fom_type_ops,
			 .sm        = &poolmach_conf,
			 .svc_type  = &m0_ios_type);
	M0_FOP_TYPE_INIT(&m0_fop_poolmach_set_rep_fopt,
			 .name      = "Pool Machine set reply",
			 .opcode    = M0_POOLMACHINE_SET_REP_OPCODE,
			 .xt        = m0_fop_poolmach_set_rep_xc,
			 .rpc_flags = M0_RPC_ITEM_TYPE_REPLY);
	return 0;
}

#undef M0_TRACE_SUBSYSTEM

/*
 *  Local variables:
 *  c-indentation-style: "K&R"
 *  c-basic-offset: 8
 *  tab-width: 8
 *  fill-column: 80
 *  scroll-step: 1
 *  End:
 */
