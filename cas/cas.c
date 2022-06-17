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


#define M0_TRACE_SUBSYSTEM M0_TRACE_SUBSYS_CAS

#include "lib/trace.h"
#include "lib/finject.h"     /* M0_FI_ENABLED */
#include "fid/fid.h"         /* m0_fid_type_register */
#include "fop/fop.h"
#include "fop/wire_xc.h"
#include "rpc/rpc_opcodes.h"
#include "cas/cas.h"
#include "cas/cas_xc.h"
#include "mdservice/fsync_foms.h"       /* m0_fsync_fom_conf */
#include "mdservice/fsync_fops.h"       /* m0_fsync_fom_ops */
#include "mdservice/fsync_fops_xc.h"    /* m0_fop_fsync_xc */
#include "cas/client.h"                 /* m0_cas_sm_conf_init */

struct m0_fom_type_ops;
struct m0_sm_conf;
struct m0_reqh_service_type;

/**
 * @addtogroup cas
 *
 * @{
 */

M0_INTERNAL struct m0_fop_type cas_get_fopt;
M0_INTERNAL struct m0_fop_type cas_put_fopt;
M0_INTERNAL struct m0_fop_type cas_del_fopt;
M0_INTERNAL struct m0_fop_type cas_cur_fopt;
M0_INTERNAL struct m0_fop_type cas_rep_fopt;
M0_INTERNAL struct m0_fop_type cas_gc_fopt;
struct m0_fop_type m0_fop_fsync_cas_fopt;

static int cas_fops_init(const struct m0_sm_conf           *sm_conf,
			 const struct m0_fom_type_ops      *fom_ops,
			 const struct m0_reqh_service_type *svctype)
{
	M0_FOP_TYPE_INIT(&cas_get_fopt,
			 .name      = "cas-get",
			 .opcode    = M0_CAS_GET_FOP_OPCODE,
			 .rpc_flags = M0_RPC_ITEM_TYPE_REQUEST,
			 .xt        = m0_cas_op_xc,
			 .fom_ops   = fom_ops,
			 .sm        = sm_conf,
			 .svc_type  = svctype);
	M0_FOP_TYPE_INIT(&cas_put_fopt,
			 .name      = "cas-put",
			 .opcode    = M0_CAS_PUT_FOP_OPCODE,
			 .rpc_flags = M0_RPC_ITEM_TYPE_REQUEST |
				      M0_RPC_ITEM_TYPE_MUTABO,
			 .xt        = m0_cas_op_xc,
			 .fom_ops   = fom_ops,
			 .sm        = sm_conf,
			 .svc_type  = svctype);
	M0_FOP_TYPE_INIT(&cas_del_fopt,
			 .name      = "cas-del",
			 .opcode    = M0_CAS_DEL_FOP_OPCODE,
			 .rpc_flags = M0_RPC_ITEM_TYPE_REQUEST |
				      M0_RPC_ITEM_TYPE_MUTABO,
			 .xt        = m0_cas_op_xc,
			 .fom_ops   = fom_ops,
			 .sm        = sm_conf,
			 .svc_type  = svctype);
	M0_FOP_TYPE_INIT(&cas_cur_fopt,
			 .name      = "cas-cur",
			 .opcode    = M0_CAS_CUR_FOP_OPCODE,
			 .rpc_flags = M0_RPC_ITEM_TYPE_REQUEST,
			 .xt        = m0_cas_op_xc,
			 .fom_ops   = fom_ops,
			 .sm        = sm_conf,
			 .svc_type  = svctype);
	M0_FOP_TYPE_INIT(&cas_rep_fopt,
			 .name      = "cas-rep",
			 .opcode    = M0_CAS_REP_FOP_OPCODE,
			 .rpc_flags = M0_RPC_ITEM_TYPE_REPLY,
			 .xt        = m0_cas_rep_xc,
			 .svc_type  = svctype);
	M0_FOP_TYPE_INIT(&cas_gc_fopt,
			 .name      = "cas-gc-wait",
			 .opcode    = M0_CAS_GCW_FOP_OPCODE,
			 .rpc_flags = M0_RPC_ITEM_TYPE_REQUEST,
			 .xt        = m0_cas_op_xc,
			 .fom_ops   = fom_ops,
			 .sm        = sm_conf,
			 .svc_type  = svctype);
	M0_FOP_TYPE_INIT(&m0_fop_fsync_cas_fopt,
			 .name      = "fsync-cas",
			 .opcode    = M0_FSYNC_CAS_OPCODE,
			 .xt        = m0_fop_fsync_xc,
#ifndef __KERNEL__
			 .svc_type  = svctype,
			 .sm        = &m0_fsync_fom_conf,
			 .fom_ops   = &m0_fsync_fom_ops,
#endif
			 .rpc_flags = M0_RPC_ITEM_TYPE_REQUEST);
	return  m0_fop_type_addb2_instrument(&cas_get_fopt) ?:
		m0_fop_type_addb2_instrument(&cas_put_fopt) ?:
		m0_fop_type_addb2_instrument(&cas_del_fopt) ?:
		m0_fop_type_addb2_instrument(&cas_cur_fopt) ?:
		m0_fop_type_addb2_instrument(&cas_gc_fopt)?:
		m0_fop_type_addb2_instrument(&m0_fop_fsync_cas_fopt);
}

static void cas_fops_fini(void)
{
	m0_fop_type_addb2_deinstrument(&cas_gc_fopt);
	m0_fop_type_addb2_deinstrument(&cas_cur_fopt);
	m0_fop_type_addb2_deinstrument(&cas_del_fopt);
	m0_fop_type_addb2_deinstrument(&cas_put_fopt);
	m0_fop_type_addb2_deinstrument(&cas_get_fopt);
	m0_fop_type_addb2_deinstrument(&m0_fop_fsync_cas_fopt);
	m0_fop_type_fini(&cas_gc_fopt);
	m0_fop_type_fini(&cas_rep_fopt);
	m0_fop_type_fini(&cas_cur_fopt);
	m0_fop_type_fini(&cas_del_fopt);
	m0_fop_type_fini(&cas_put_fopt);
	m0_fop_type_fini(&cas_get_fopt);
	m0_fop_type_fini(&m0_fop_fsync_cas_fopt);
}

/**
 * FID of the meta-index. It has the smallest possible FID in order to be always
 * the first during iteration over existing indices.
 */
M0_INTERNAL const struct m0_fid m0_cas_meta_fid = M0_FID_TINIT('i', 0, 0);

/**
 * FID of the catalogue-index index.
 */
M0_INTERNAL const struct m0_fid m0_cas_ctidx_fid = M0_FID_TINIT('i', 0, 1);

/**
 * FID of the "dead index" catalogue (used to collect deleted indices).
 */
M0_INTERNAL const struct m0_fid m0_cas_dead_index_fid = M0_FID_TINIT('i', 0, 2);

M0_INTERNAL const struct m0_fid_type m0_cas_index_fid_type = {
	.ft_id   = 'i',
	.ft_name = "cas-index"
};

M0_INTERNAL const struct m0_fid_type m0_cctg_fid_type = {
	.ft_id   = 'T',
	.ft_name = "component-catalogue"
};

M0_INTERNAL const struct m0_fid_type m0_dix_fid_type = {
	.ft_id   = 'x',
	.ft_name = "distributed-index"
};

M0_INTERNAL int m0_cas_module_init(void)
{
	struct m0_sm_conf            *sm_conf;
	const struct m0_fom_type_ops *fom_ops;
	struct m0_reqh_service_type  *svctype;

	m0_fid_type_register(&m0_cas_index_fid_type);
	m0_fid_type_register(&m0_cctg_fid_type);
	m0_fid_type_register(&m0_dix_fid_type);
	m0_cas_svc_init();
	m0_cas_svc_fop_args(&sm_conf, &fom_ops, &svctype);
	return cas_fops_init(sm_conf, fom_ops, svctype) ?:
		m0_cas_sm_conf_init();
}

M0_INTERNAL void m0_cas_module_fini(void)
{
	m0_cas_sm_conf_fini();
	cas_fops_fini();
	m0_cas_svc_fini();
	m0_fid_type_unregister(&m0_cas_index_fid_type);
	m0_fid_type_unregister(&m0_cctg_fid_type);
	m0_fid_type_unregister(&m0_dix_fid_type);
}

M0_INTERNAL void m0_cas_id_fini(struct m0_cas_id *cid)
{
	M0_PRE(cid != NULL);

	if (m0_fid_type_getfid(&cid->ci_fid) == &m0_cctg_fid_type)
		m0_dix_ldesc_fini(&cid->ci_layout.u.dl_desc);
	M0_SET0(cid);
}

M0_INTERNAL bool m0_cas_id_invariant(const struct m0_cas_id *cid)
{
	return _0C(cid != NULL) &&
	       _0C(M0_IN(m0_fid_type_getfid(&cid->ci_fid),
		      (&m0_cas_index_fid_type, &m0_cctg_fid_type,
		       &m0_dix_fid_type))) &&
	       _0C(M0_IN(cid->ci_layout.dl_type, (DIX_LTYPE_UNKNOWN,
				       DIX_LTYPE_ID, DIX_LTYPE_DESCR))) &&
	       _0C(ergo(m0_fid_type_getfid(&cid->ci_fid) ==
				       &m0_cas_index_fid_type,
			M0_IS0(&cid->ci_layout)));
}

M0_INTERNAL bool cas_in_ut(void)
{
	return M0_FI_ENABLED("ut");
}

M0_INTERNAL bool m0_crv_tbs(const struct m0_crv *crv)
{
	return crv->crv_encoded & M0_CRV_TBS;
}

M0_INTERNAL void m0_crv_tbs_set(struct m0_crv *crv, bool tbs)
{
	if (tbs)
		crv->crv_encoded |= M0_CRV_TBS;
	else
		crv->crv_encoded &= ~M0_CRV_TBS;
}

M0_INTERNAL struct m0_dtm0_ts m0_crv_ts(const struct m0_crv *crv)
{
	return (struct m0_dtm0_ts) {
		.dts_phys = crv->crv_encoded & ~M0_CRV_TBS
	};
}

M0_INTERNAL void m0_crv_ts_set(struct m0_crv           *crv,
			       const struct m0_dtm0_ts *ts)
{
	crv->crv_encoded = (crv->crv_encoded & M0_CRV_TBS) | ts->dts_phys;
}

M0_INTERNAL void m0_crv_init(struct m0_crv           *crv,
			     const struct m0_dtm0_ts *ts,
			     bool                     tbs)
{
	uint64_t version = ts->dts_phys;

	M0_PRE(version <= M0_CRV_VER_MAX);
	M0_PRE(version >= M0_CRV_VER_MIN);

	m0_crv_ts_set(crv, ts);
	m0_crv_tbs_set(crv, tbs);

	M0_POST(equi(m0_crv_tbs(crv), tbs));
	M0_POST(m0_crv_ts(crv).dts_phys == version);
}

/**
 * Compare two versions.
 *   Note, tombstones are checked at the end which means that if there are two
 * different operations with the same version (for example, PUT@10 and DEL@10)
 * then the operation that sets the tombstone (DEL@10) is always considered
 * to be "newer" than the other one. It helps to ensure operations have the
 * same order on any server despite the order of execution.
 */
M0_INTERNAL int m0_crv_cmp(const struct m0_crv *left,
			   const struct m0_crv *right)
{
	return M0_3WAY(m0_crv_ts(left).dts_phys, m0_crv_ts(right).dts_phys) ?:
		M0_3WAY(m0_crv_tbs(left), m0_crv_tbs(right));
}

M0_INTERNAL bool m0_crv_is_none(const struct m0_crv *crv)
{
	return memcmp(crv, &M0_CRV_INIT_NONE, sizeof(*crv)) == 0;
}

#undef M0_TRACE_SUBSYSTEM

/** @} end of cas group */

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
