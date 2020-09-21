/*
 * Copyright (c) 2011-2020 Seagate Technology LLC and/or its Affiliates
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


#define M0_TRACE_SUBSYSTEM M0_TRACE_SUBSYS_CONF
#include "lib/trace.h"

#include "conf/fop.h"
#include "conf/onwire_xc.h"
#include "conf/confd_fom.h"   /* m0_confd_fom_create */
#include "conf/confd.h"       /* m0_confd_stype */
#include "rpc/rpc_opcodes.h"
#include "fop/fom_generic.h"  /* m0_generic_conf */
#include "conf/flip_fop.h"
#include "conf/flip_fop_xc.h" /* m0_xc_flip_fop_init, m0_xc_flip_fop_fini */
#include "conf/load_fop.h"
#include "conf/load_fop_xc.h" /* m0_xc_load_fop_init, m0_xc_load_fop_fini */

/**
 * @addtogroup conf_fop
 *
 * @{
 */

struct m0_fop_type m0_conf_fetch_fopt;
struct m0_fop_type m0_conf_fetch_resp_fopt;

struct m0_fop_type m0_conf_update_fopt;
struct m0_fop_type m0_conf_update_resp_fopt;

#ifndef __KERNEL__
static const struct m0_fom_type_ops confd_fom_ops = {
	.fto_create = m0_confd_fom_create
};
#endif


extern struct m0_sm_conf conf_load_conf;
extern struct m0_sm_state_descr conf_load_phases[];

struct m0_fop_type m0_fop_conf_load_fopt;
struct m0_fop_type m0_fop_conf_load_rep_fopt;

extern const struct m0_fom_type_ops conf_load_fom_type_ops;


extern struct m0_sm_conf conf_flip_conf;
extern struct m0_sm_state_descr conf_flip_phases[];

struct m0_fop_type m0_fop_conf_flip_fopt;
struct m0_fop_type m0_fop_conf_flip_rep_fopt;

extern const struct m0_fom_type_ops conf_flip_fom_type_ops;


M0_INTERNAL int m0_conf_fops_init(void)
{
        M0_FOP_TYPE_INIT(&m0_conf_fetch_fopt,
			 .name      = "Configuration fetch request",
			 .opcode    = M0_CONF_FETCH_OPCODE,
			 .xt        = m0_conf_fetch_xc,
#ifndef __KERNEL__
			 .fom_ops   = &confd_fom_ops,
			 .svc_type  = &m0_confd_stype,
			 .sm        = &m0_generic_conf,
#endif
			 .rpc_flags = M0_RPC_ITEM_TYPE_REQUEST);
	M0_FOP_TYPE_INIT(&m0_conf_fetch_resp_fopt,
			 .name      = "Configuration fetch response",
			 .opcode    = M0_CONF_FETCH_RESP_OPCODE,
			 .xt        = m0_conf_fetch_resp_xc,
			 .rpc_flags = M0_RPC_ITEM_TYPE_REPLY);
	/*
	 * XXX Argh! Why bother defining update _stubs_?  Do we win anything? Is
	 * it worth the cost of maintenance?
	 */
	M0_FOP_TYPE_INIT(&m0_conf_update_fopt,
			 .name      = "Configuration update request",
			 .opcode    = M0_CONF_UPDATE_OPCODE,
			 .xt        = m0_conf_update_xc,
#ifndef __KERNEL__
			 .fom_ops   = &confd_fom_ops,
			 .svc_type  = &m0_confd_stype,
			 .sm        = &m0_generic_conf,
#endif
			 .rpc_flags = M0_RPC_MUTABO_REQ);
	M0_FOP_TYPE_INIT(&m0_conf_update_resp_fopt,
			 .name      = "Configuration update response",
			 .opcode    = M0_CONF_UPDATE_RESP_OPCODE,
			 .xt        = m0_conf_update_resp_xc,
			 .rpc_flags = M0_RPC_ITEM_TYPE_REPLY);


	/*
	 * Provided by m0gccxml2xcode after parsing load_fop.h
	 */
	m0_xc_conf_load_fop_init();

#ifndef __KERNEL__
	m0_sm_conf_extend(m0_generic_conf.scf_state, conf_load_phases,
			  m0_generic_conf.scf_nr_states);
#endif

	M0_FOP_TYPE_INIT(&m0_fop_conf_load_fopt,
			 .name      = "Conf Load configure request",
			 .opcode    = M0_SPIEL_CONF_FILE_OPCODE,
			 .xt        = m0_fop_conf_load_xc,
#ifndef __KERNEL__
			 .fom_ops   = &conf_load_fom_type_ops,
			 .svc_type  = &m0_confd_stype,
			 .sm        = &conf_load_conf,
#endif
			 .rpc_flags = M0_RPC_ITEM_TYPE_REQUEST);

	M0_FOP_TYPE_INIT(&m0_fop_conf_load_rep_fopt,
			 .name      = "Conf Load configure reply",
			 .opcode    = M0_SPIEL_CONF_FILE_REP_OPCODE,
			 .xt        = m0_fop_conf_load_rep_xc,
			 .rpc_flags = M0_RPC_ITEM_TYPE_REPLY);

	/*
	 * Provided by m0gccxml2xcode after parsing flip_fop.h
	 */
	m0_xc_conf_flip_fop_init();

#ifndef __KERNEL__
	m0_sm_conf_extend(m0_generic_conf.scf_state, conf_flip_phases,
			  m0_generic_conf.scf_nr_states);
#endif

	M0_FOP_TYPE_INIT(&m0_fop_conf_flip_fopt,
			 .name      = "Conf Flip configure request",
			 .opcode    = M0_SPIEL_CONF_FLIP_OPCODE,
			 .xt        = m0_fop_conf_flip_xc,
#ifndef __KERNEL__
			 .fom_ops   = &conf_flip_fom_type_ops,
			 .svc_type  = &m0_confd_stype,
			 .sm        = &conf_flip_conf,
#endif
			 .rpc_flags = M0_RPC_ITEM_TYPE_REQUEST);

	M0_FOP_TYPE_INIT(&m0_fop_conf_flip_rep_fopt,
			 .name      = "Conf Flip configure reply",
			 .opcode    = M0_SPIEL_CONF_FLIP_REP_OPCODE,
			 .xt        = m0_fop_conf_flip_rep_xc,
			 .rpc_flags = M0_RPC_ITEM_TYPE_REPLY);

	return 0;
}

M0_INTERNAL void m0_conf_fops_fini(void)
{
	m0_fop_type_fini(&m0_conf_fetch_fopt);
	m0_fop_type_fini(&m0_conf_fetch_resp_fopt);

	m0_fop_type_fini(&m0_conf_update_fopt);
	m0_fop_type_fini(&m0_conf_update_resp_fopt);

	m0_fop_type_fini(&m0_fop_conf_load_rep_fopt);
	m0_fop_type_fini(&m0_fop_conf_load_fopt);

	m0_fop_type_fini(&m0_fop_conf_flip_rep_fopt);
	m0_fop_type_fini(&m0_fop_conf_flip_fopt);

	m0_xc_conf_load_fop_fini();
}

M0_INTERNAL int m0_confx_types_init(void)
{
	M0_ENTRY();
	return M0_RC(0);
}

M0_INTERNAL void m0_confx_types_fini(void)
{
	M0_ENTRY();
	M0_LEAVE();
}

/** @} conf_fop */
#undef M0_TRACE_SUBSYSTEM
