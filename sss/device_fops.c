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


#include "lib/memory.h"
#include "lib/misc.h"
#include "lib/types.h"
#include "conf/obj.h"           /* M0_CONF_DRIVE_TYPE */
#include "sm/sm.h"
#include "fop/fop.h"
#include "fop/fom.h"
#include "fop/fop_item_type.h"
#include "fop/fom_generic.h"
#include "rpc/rpc_opcodes.h"
#include "rpc/item.h"
#include "sss/device_foms.h"
#include "sss/device_fops.h"
#include "sss/device_fops_xc.h"
#include "sss/ss_svc.h"

struct m0_fop_type m0_sss_fop_device_fopt;
struct m0_fop_type m0_sss_fop_device_rep_fopt;

extern struct m0_sm_state_descr     sss_device_fom_phases_desc[];
extern const struct m0_sm_conf      sss_device_fom_conf;
extern const struct m0_fom_type_ops sss_device_fom_type_ops;

M0_INTERNAL int m0_sss_device_fops_init(void)
{
	m0_sm_conf_extend(m0_generic_conf.scf_state, sss_device_fom_phases_desc,
			  m0_generic_conf.scf_nr_states);

	/*
	 * Extend phases this FOM needs to open separate BE transaction.
	 * Because extend phases execute before M0_FOPH_TXN_INIT phase.
	 * FOM handler catch M0_FOPH_TXN_INIT phase. If it first time then
	 * move to extended phase else simple way.
	 */
	sss_device_fom_phases_desc[M0_FOPH_TXN_INIT].sd_allowed |=
				M0_BITS(SSS_DFOM_DISK_OPENING);

	m0_sss_fop_device_fopt.ft_magix = 0;
	m0_sss_fop_device_rep_fopt.ft_magix = 0;

	M0_FOP_TYPE_INIT(&m0_sss_fop_device_fopt,
			 .name      = "Device-fop",
			 .opcode    = M0_SSS_DEVICE_REQ_OPCODE,
			 .xt        = m0_sss_device_fop_xc,
			 .rpc_flags = M0_RPC_ITEM_TYPE_REQUEST,
			 .fom_ops   = &sss_device_fom_type_ops,
			 .svc_type  = &m0_ss_svc_type,
			 .sm        = &sss_device_fom_conf);

	M0_FOP_TYPE_INIT(&m0_sss_fop_device_rep_fopt,
			 .name      = "Device-reply-fop",
			 .opcode    = M0_SSS_DEVICE_REP_OPCODE,
			 .xt        = m0_sss_device_fop_rep_xc,
			 .rpc_flags = M0_RPC_ITEM_TYPE_REPLY);
	return 0;
}

M0_INTERNAL void m0_sss_device_fops_fini(void)
{
	m0_fop_type_fini(&m0_sss_fop_device_fopt);
	m0_fop_type_fini(&m0_sss_fop_device_rep_fopt);
}

M0_INTERNAL struct m0_fop *m0_sss_device_fop_create(struct m0_rpc_machine *mach,
						    uint32_t               cmd,
						    const struct m0_fid   *fid)
{
	struct m0_fop            *fop;
	struct m0_sss_device_fop *req;

	M0_PRE(mach != NULL);
	M0_PRE(cmd < M0_DEVICE_CMDS_NR);
	M0_PRE(m0_conf_fid_type(fid) == &M0_CONF_DRIVE_TYPE);

	fop = m0_fop_alloc(&m0_sss_fop_device_fopt, NULL, mach);
	if (fop == NULL)
		return NULL;

	req = m0_sss_fop_to_dev_req(fop);
	req->ssd_cmd = cmd;
	req->ssd_fid = *fid;

	return fop;
}

M0_INTERNAL bool m0_sss_fop_is_dev_req(const struct m0_fop *fop)
{
	M0_PRE(fop != NULL);
	return fop->f_type == &m0_sss_fop_device_fopt;
}

M0_INTERNAL struct m0_sss_device_fop *m0_sss_fop_to_dev_req(struct m0_fop *fop)
{
	M0_PRE(fop != NULL);
	M0_PRE(m0_sss_fop_is_dev_req(fop));

	return m0_fop_data(fop);
}

M0_INTERNAL bool m0_sss_fop_is_dev_rep(const struct m0_fop *fop)
{
	M0_PRE(fop != NULL);
	return fop->f_type == &m0_sss_fop_device_rep_fopt;
}

M0_INTERNAL
struct m0_sss_device_fop_rep *m0_sss_fop_to_dev_rep(struct m0_fop *fop)
{
	M0_PRE(fop != NULL);
	M0_PRE(m0_sss_fop_is_dev_rep(fop));

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
