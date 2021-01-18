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


#include "lib/memory.h"
#include "lib/string.h"

#include "fop/fop.h"
#include "rpc/rpc.h"
#include "rpc/rpc_opcodes.h"

#include "cm/cm.h"
#include "cm/repreb/sw_onwire_fop.h"
#include "cm/repreb/sw_onwire_fop_xc.h"

#define M0_TRACE_SUBSYSTEM M0_TRACE_SUBSYS_CM
#include "lib/trace.h"

/**
   @addtogroup XXX

   @{
 */

extern const struct m0_sm_conf m0_cm_repreb_sw_onwire_conf;

M0_INTERNAL
void m0_cm_repreb_sw_onwire_fop_init(struct m0_fop_type *ft,
				     const struct m0_fom_type_ops *fomt_ops,
				     enum M0_RPC_OPCODES op,
				     const char *name,
				     const struct m0_xcode_type *xt,
				     uint64_t rpc_flags,
				     struct m0_cm_type *cmt)
{
	M0_FOP_TYPE_INIT(ft,
			 .name      = name,
			 .opcode    = op,
			 .xt        = xt,
			 .rpc_flags = rpc_flags,
			 .fom_ops   = fomt_ops,
			 .sm        = &m0_cm_repreb_sw_onwire_conf,
			 .svc_type  = &cmt->ct_stype);
}

M0_INTERNAL void m0_cm_repreb_sw_onwire_fop_fini(struct m0_fop_type *ft)
{
	m0_fop_type_fini(ft);
}

M0_INTERNAL int
m0_cm_repreb_sw_onwire_fop_setup(struct m0_cm *cm, struct m0_fop_type *ft,
				 struct m0_fop *fop,
				 void (*fop_release)(struct m0_ref *),
				 uint64_t proxy_id, const char *local_ep,
				 const struct m0_cm_sw *sw,
				 const struct m0_cm_sw *out_interval)
{
	struct m0_cm_sw_onwire *swo_fop;
	int                     rc = 0;
	M0_ENTRY("cm=%p proxy_id=%"PRIu64" local_ep=%s", cm, proxy_id, local_ep);

	M0_PRE(cm != NULL && sw != NULL && local_ep != NULL);

	m0_fop_init(fop, ft, NULL, fop_release);
	rc = m0_fop_data_alloc(fop);
	if (rc  != 0) {
		m0_fop_fini(fop);
		return M0_RC(rc);
	}
	swo_fop = m0_fop_data(fop);
	rc = m0_cm_sw_onwire_init(cm, swo_fop, proxy_id, local_ep, sw,
				  out_interval);

	return M0_RC(rc);
}

#undef M0_TRACE_SUBSYSTEM

/** @} XXX */
/*
 *  Local variables:
 *  c-indentation-style: "K&R"
 *  c-basic-offset: 8
 *  tab-width: 8
 *  fill-column: 80
 *  scroll-step: 1
 *  End:
 */
