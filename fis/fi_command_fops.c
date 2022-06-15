/* -*- C -*- */
/*
 * Copyright (c) 2017-2020 Seagate Technology LLC and/or its Affiliates
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

#define M0_TRACE_SUBSYSTEM M0_TRACE_SUBSYS_OTHER

#include "lib/trace.h"

#include "fis/fi_command_fom.h"    /* m0_fi_command_fom_type_ops */
#include "fis/fi_command_fops.h"
#include "fis/fi_command_xc.h"     /* m0_fi_command_req_xc, m0_fi_command_rep_xc */
#include "fop/fom_generic.h"       /* m0_generic_conf */
#include "fop/fop.h"               /* M0_FOP_TYPE_INIT */
#include "rpc/rpc.h"               /* m0_rpc_service_type */
#include "rpc/rpc_opcodes.h"

/**
 * @page fis-lspec-command-fops Fault Injection Command FOPs.
 *
 * Fault Injection Command FOPs initialised at FIS start and finalised at
 * service stop. FI command is accepted only when the FOPs are initialised.
 */

/**
 * @addtogroup fis-dlspec
 *
 * @{
 */
struct m0_fop_type m0_fi_command_req_fopt;
struct m0_fop_type m0_fi_command_rep_fopt;

M0_INTERNAL void m0_fi_command_fop_init(void)
{
	extern struct m0_reqh_service_type m0_rpc_service_type;

	M0_LOG(M0_ALWAYS, "FOP init");
	M0_FOP_TYPE_INIT(&m0_fi_command_req_fopt,
			 .name      = "Fault Injection Command",
			 .opcode    = M0_FI_COMMAND_OPCODE,
			 .xt        = m0_fi_command_req_xc,
			 .sm        = &m0_generic_conf,
			 .fom_ops   = &m0_fi_command_fom_type_ops,
			 .svc_type  = &m0_rpc_service_type,
			 .rpc_flags = M0_RPC_ITEM_TYPE_REQUEST);
	M0_FOP_TYPE_INIT(&m0_fi_command_rep_fopt,
			 .name      = "Fault Injection Command reply",
			 .opcode    = M0_FI_COMMAND_REP_OPCODE,
			 .xt        = m0_fi_command_rep_xc,
			 .rpc_flags = M0_RPC_ITEM_TYPE_REPLY,
			 .svc_type  = &m0_rpc_service_type);
}

M0_INTERNAL void m0_fi_command_fop_fini(void)
{
	M0_LOG(M0_ALWAYS, "FOP fini");
	m0_fop_type_fini(&m0_fi_command_req_fopt);
	m0_fop_type_fini(&m0_fi_command_rep_fopt);
}

/** @} end fis-dlspec */
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
