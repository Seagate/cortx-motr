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


#include "lib/errno.h"
#include "lib/memory.h"
#include "fop/fom.h"
#include "fop/fop.h"
#include "rpc/it/ping_fop.h"
#include "rpc/it/ping_fop_xc.h"
#include "rpc/it/ping_fom.h"
#include "lib/errno.h"
#include "rpc/rpc.h"
#include "fop/fop_item_type.h"
#include "fop/fom_generic.h"

struct m0_fop_type m0_fop_ping_fopt;
struct m0_fop_type m0_fop_ping_rep_fopt;

M0_INTERNAL void m0_ping_fop_fini(void)
{
	m0_fop_type_fini(&m0_fop_ping_rep_fopt);
	m0_fop_type_fini(&m0_fop_ping_fopt);
	m0_xc_rpc_it_ping_fop_fini();
}

extern const struct m0_fom_type_ops m0_fom_ping_type_ops;
extern struct m0_reqh_service_type m0_rpc_service_type;

M0_INTERNAL void m0_ping_fop_init(void)
{
	m0_xc_rpc_it_ping_fop_init();
	M0_FOP_TYPE_INIT(&m0_fop_ping_fopt,
			 .name      = "Ping fop",
			 .opcode    = M0_RPC_PING_OPCODE,
			 .xt        = m0_fop_ping_xc,
			 .rpc_flags = M0_RPC_ITEM_TYPE_REQUEST,
			 .fom_ops   = &m0_fom_ping_type_ops,
			 .sm        = &m0_generic_conf,
			 .svc_type  = &m0_rpc_service_type);
	M0_FOP_TYPE_INIT(&m0_fop_ping_rep_fopt,
			 .name      = "Ping fop reply",
			 .opcode    = M0_RPC_PING_REPLY_OPCODE,
			 .xt        = m0_fop_ping_rep_xc,
			 .rpc_flags = M0_RPC_ITEM_TYPE_REPLY,
			 .svc_type  = &m0_rpc_service_type);
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
