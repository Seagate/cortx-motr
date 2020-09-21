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


#pragma once

#ifndef __MOTR_SNS_CM_CP_ONWIRE_H__
#define __MOTR_SNS_CM_CP_ONWIRE_H__

#include "rpc/rpc_opcodes.h"
#include "stob/stob.h"
#include "stob/stob_xc.h"
#include "cm/cp_onwire.h"
#include "cm/cp_onwire_xc.h"

struct m0_cm_type;

/** SNS specific onwire copy packet structure. */
struct m0_sns_cpx {
        /** Base copy packet fields. */
        struct m0_cpx             scx_cp;
        /**
         * Index vectors representing the extent information for the
         * data represented by the copy packet.
         */
        struct m0_io_indexvec_seq scx_ivecs;

        /** Destination stob id. */
        struct m0_stob_id         scx_stob_id;

	uint64_t                  scx_failed_idx;

	/** Copy packet fom phase before sending it onwire. */
	uint32_t                  scx_phase;
} M0_XCA_RECORD M0_XCA_DOMAIN(rpc);

/** SNS specific onwire copy packet reply structure. */
struct m0_sns_cpx_reply {
	int32_t             scr_rc;
        /** Base copy packet reply fields. */
        struct m0_cpx_reply scr_cp_rep;
} M0_XCA_RECORD M0_XCA_DOMAIN(rpc);

M0_INTERNAL void m0_sns_cpx_init(struct m0_fop_type *ft,
				 const struct m0_fom_type_ops *fomt_ops,
				 enum M0_RPC_OPCODES op,
				 const char *name,
				 const struct m0_xcode_type *xt,
				 uint64_t rpc_flags, struct m0_cm_type *cmt);

M0_INTERNAL void m0_sns_cpx_fini(struct m0_fop_type *ft);

#endif /* __MOTR_SNS_CM_CP_ONWIRE_H__ */
/*
 *  Local variables:
 *  c-indentation-style: "K&R"
 *  c-basic-offset: 8
 *  tab-width: 8
 *  fill-column: 80
 *  scroll-step: 1
 *  End:
 */
