/* -*- C -*- */
/*
 * Copyright (c) 2015-2020 Seagate Technology LLC and/or its Affiliates
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


#include "lib/finject.h" /* M0_FI_ENABLED */
#include "lib/memory.h"
#include "lib/misc.h"
#include "lib/types.h"
#include "lib/refs.h"
#include "sm/sm.h"
#include "fop/fop.h"
#include "fop/fom.h"
#include "fop/fop_item_type.h"
#include "fop/fom_generic.h"
#include "rpc/rpc_opcodes.h"
#include "rpc/item.h"
#include "sss/process_fops.h"
#include "sss/process_fops_xc.h"
#include "sss/ss_svc.h"
#ifndef __KERNEL__
#include <unistd.h>
#endif

struct m0_fop_type m0_fop_process_fopt;
struct m0_fop_type m0_fop_process_rep_fopt;
struct m0_fop_type m0_fop_process_svc_list_rep_fopt;

extern struct m0_sm_state_descr     ss_process_fom_phases[];
extern struct m0_sm_conf            ss_process_fom_conf;
extern const struct m0_fom_type_ops ss_process_fom_type_ops;
const struct m0_fop_type_ops        ss_process_fop_type_ops;
const struct m0_fop_type_ops        ss_process_svc_list_fop_type_ops;

static const struct m0_rpc_item_type_ops ss_process_item_type_ops = {
	M0_FOP_DEFAULT_ITEM_TYPE_OPS,
};

M0_INTERNAL int m0_ss_process_fops_init(void)
{
#ifndef __KERNEL__
	m0_sm_conf_extend(m0_generic_conf.scf_state,
			  ss_process_fom_phases,
			  m0_generic_conf.scf_nr_states);
#endif
	m0_fop_process_fopt.ft_magix = 0;
	m0_fop_process_rep_fopt.ft_magix = 0;
	m0_fop_process_svc_list_rep_fopt.ft_magix = 0;

	M0_FOP_TYPE_INIT(&m0_fop_process_fopt,
			 .name      = "Process fop",
			 .opcode    = M0_SSS_PROCESS_REQ_OPCODE,
			 .xt        = m0_ss_process_req_xc,
			 .rpc_flags = M0_RPC_ITEM_TYPE_REQUEST,
			 .fop_ops   = &ss_process_fop_type_ops,
			 .fom_ops   = &ss_process_fom_type_ops,
			 .sm        = &ss_process_fom_conf,
			 .svc_type  = &m0_ss_svc_type,
			 .rpc_ops   = &ss_process_item_type_ops);

	M0_FOP_TYPE_INIT(&m0_fop_process_rep_fopt,
			 .name      = "Process reply fop",
			 .opcode    = M0_SSS_PROCESS_REP_OPCODE,
			 .xt        = m0_ss_process_rep_xc,
			 .rpc_flags = M0_RPC_ITEM_TYPE_REPLY,
			 .fop_ops   = &ss_process_fop_type_ops,
			 .fom_ops   = &ss_process_fom_type_ops,
			 .sm        = &ss_process_fom_conf,
			 .svc_type  = &m0_ss_svc_type,
			 .rpc_ops   = &ss_process_item_type_ops);

	M0_FOP_TYPE_INIT(&m0_fop_process_svc_list_rep_fopt,
			 .name      = "Process services list reply fop",
			 .opcode    = M0_SSS_PROCESS_SVC_LIST_REP_OPCODE,
			 .xt        = m0_ss_process_svc_list_rep_xc,
			 .rpc_flags = M0_RPC_ITEM_TYPE_REPLY,
			 .fop_ops   = &ss_process_svc_list_fop_type_ops,
			 .fom_ops   = &ss_process_fom_type_ops,
			 .sm        = &ss_process_fom_conf,
			 .svc_type  = &m0_ss_svc_type,
			 .rpc_ops   = &ss_process_item_type_ops);
	return 0;
}

M0_INTERNAL void m0_ss_process_fops_fini(void)
{
	m0_fop_type_fini(&m0_fop_process_fopt);
	m0_fop_type_fini(&m0_fop_process_rep_fopt);
	m0_fop_type_fini(&m0_fop_process_svc_list_rep_fopt);
}

static bool ss_fop_is_process_rep(const struct m0_fop *fop)
{
	M0_PRE(fop != NULL);
	return fop->f_type == &m0_fop_process_rep_fopt;
}

static bool ss_fop_is_process_svc_list_rep(const struct m0_fop *fop)
{
	M0_PRE(fop != NULL);
	return fop->f_type == &m0_fop_process_svc_list_rep_fopt;
}

M0_INTERNAL void m0_ss_process_rep_fop_release(struct m0_ref *ref)
{
	struct m0_ss_process_rep *rep;
	struct m0_fop            *fop = container_of(ref, struct m0_fop, f_ref);

	M0_PRE(fop != NULL);
	M0_PRE(ss_fop_is_process_rep(fop));

	rep = m0_fop_data(fop);
	m0_buf_free(&rep->sspr_bckey);
	m0_buf_free(&rep->sspr_bcrec);
	m0_fop_fini(fop);
	m0_free(fop);
}


M0_INTERNAL void m0_ss_process_stop_fop_release(struct m0_ref *ref)
{
#ifndef __KERNEL__
	int            pid = getpid();
#endif
	struct m0_fop *fop = container_of(ref, struct m0_fop, f_ref);

	M0_PRE(fop != NULL);
	M0_PRE(ss_fop_is_process_rep(fop));

	m0_fop_fini(fop);
	m0_free(fop);

#ifndef __KERNEL__
	if (!M0_FI_ENABLED("no_kill"))
		kill(pid, SIGQUIT);
#endif
}

M0_INTERNAL struct m0_fop *m0_ss_process_fop_create(struct m0_rpc_machine *mach,
						    uint32_t               cmd,
						    const struct m0_fid   *fid)
{
	struct m0_fop            *fop;
	struct m0_ss_process_req *req;

	M0_PRE(mach != NULL);
	M0_PRE(cmd < M0_PROCESS_NR);

	fop = m0_fop_alloc(&m0_fop_process_fopt, NULL, mach);
	if (fop == NULL)
		return NULL;

	req = m0_ss_fop_process_req(fop);
	req->ssp_cmd = cmd;
	req->ssp_id = *fid;

	return fop;
}

M0_INTERNAL bool m0_ss_fop_is_process_req(const struct m0_fop *fop)
{
	M0_PRE(fop != NULL);
	return fop->f_type == &m0_fop_process_fopt;
}

M0_INTERNAL struct m0_ss_process_req *m0_ss_fop_process_req(struct m0_fop *fop)
{
	M0_PRE(fop != NULL);
	M0_PRE(m0_ss_fop_is_process_req(fop));

	return m0_fop_data(fop);
}

M0_INTERNAL struct m0_ss_process_rep *m0_ss_fop_process_rep(struct m0_fop *fop)
{
	M0_PRE(fop != NULL);
	M0_PRE(ss_fop_is_process_rep(fop));

	return m0_fop_data(fop);
}

M0_INTERNAL struct m0_ss_process_svc_list_rep *
			m0_ss_fop_process_svc_list_rep(struct m0_fop *fop)
{
	M0_PRE(fop != NULL);
	M0_PRE(ss_fop_is_process_svc_list_rep(fop));
	return m0_fop_data(fop);
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
