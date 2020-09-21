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


#include "fop/fop.h"             /* m0_fop_xcode_length */
#include "fop/fom_generic.h"     /* m0_generic_conf */
#include "rpc/rpc_opcodes.h"     /* M0_CONS_FOP_DEVICE_OPCODE */

#include "console/console_fom.h" /* FOMs defs */
#include "console/console_fop.h" /* FOPs defs */
#include "console/console_fop_xc.h" /* FOP memory layout */

/**
   @addtogroup console
   @{
*/

extern struct m0_reqh_service_type m0_rpc_service_type;

struct m0_fop_type m0_cons_fop_device_fopt;
struct m0_fop_type m0_cons_fop_reply_fopt;
struct m0_fop_type m0_cons_fop_test_fopt;

M0_INTERNAL void m0_console_fop_fini(void)
{
	m0_fop_type_fini(&m0_cons_fop_device_fopt);
	m0_fop_type_fini(&m0_cons_fop_reply_fopt);
	m0_fop_type_fini(&m0_cons_fop_test_fopt);
}

extern const struct m0_fom_type_ops m0_cons_fom_device_type_ops;

M0_INTERNAL int m0_console_fop_init(void)
{
	M0_FOP_TYPE_INIT(&m0_cons_fop_device_fopt,
			 .name      = "Device Failed",
			 .opcode    = M0_CONS_FOP_DEVICE_OPCODE,
			 .xt        = m0_cons_fop_device_xc,
			 .rpc_flags = M0_RPC_ITEM_TYPE_REQUEST,
			 .sm        = &m0_generic_conf,
			 .fom_ops   = &m0_console_fom_type_device_ops,
			 .svc_type  = &m0_rpc_service_type);
	M0_FOP_TYPE_INIT(&m0_cons_fop_reply_fopt,
			 .name      = "Console Reply",
			 .opcode    = M0_CONS_FOP_REPLY_OPCODE,
			 .xt        = m0_cons_fop_reply_xc,
			 .rpc_flags = M0_RPC_ITEM_TYPE_REPLY);
	M0_FOP_TYPE_INIT(&m0_cons_fop_test_fopt,
			 .name      = "Console Test",
			 .opcode    = M0_CONS_TEST,
			 .xt        = m0_cons_fop_test_xc,
			 .sm        = &m0_generic_conf,
			 .rpc_flags = M0_RPC_ITEM_TYPE_REQUEST,
			 .fom_ops   = &m0_console_fom_type_test_ops,
			 .svc_type  = &m0_rpc_service_type);
	return 0;
}

/** @} end of console */
/*
 *  Local variables:
 *  c-indentation-style: "K&R"
 *  c-basic-offset: 8
 *  tab-width: 8
 *  fill-column: 80
 *  scroll-step: 1
 *  End:
 */
