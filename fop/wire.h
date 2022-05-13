/* -*- C -*- */
/*
 * Copyright (c) 2012-2022 Seagate Technology LLC and/or its Affiliates
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

#ifndef __MOTR_FOP_WIRE_H__
#define __MOTR_FOP_WIRE_H__

#include "lib/types.h"
#include "xcode/xcode_attr.h"

/**
   @defgroup fop File operation packet

   On-wire structures for fops.
   @{
*/

/**
 * fol record fragment for a fop.
 */
struct m0_fop_fol_frag {
	/** m0_fop_type::ft_rpc_item_type::rit_opcode of fop. */
	uint32_t  ffrp_fop_code;
	/** m0_fop_type::ft_rpc_item_type::rit_opcode of fop. */
	uint32_t  ffrp_rep_code;
	void	 *ffrp_fop M0_XCA_OPAQUE("m0_fop_xc_type");
	void	 *ffrp_rep M0_XCA_OPAQUE("m0_fop_rep_xc_type");
} M0_XCA_RECORD;

struct m0_fop_str {
	uint32_t s_len;
	uint8_t *s_buf;
} M0_XCA_SEQUENCE M0_XCA_DOMAIN(rpc);

/**
   Generic reply.

   RPC operations that return nothing but error code to sender can use
   this generic reply fop. Request handler uses this type of fop to
   report operation failure in generic fom phases.
 */
struct m0_fop_generic_reply {
	int32_t           gr_rc;
	struct m0_fop_str gr_msg;
} M0_XCA_RECORD M0_XCA_DOMAIN(rpc);

/**
 * Transaction identifier for remote nodes.
 */
struct m0_be_tx_remid {
	uint64_t tri_txid;
	uint64_t tri_locality;
} M0_XCA_RECORD M0_XCA_DOMAIN(be|rpc);

/**
 * m0_fop_mod_rep contains common reply values for an UPDATE fop.
 */
struct m0_fop_mod_rep {
	/** Remote ID assigned to this UPDATE operation */
	struct m0_be_tx_remid fmr_remid;
} M0_XCA_RECORD M0_XCA_DOMAIN(rpc);

/** @} end of fop group */
#endif /* __MOTR_FOP_WIRE_H__ */

/*
 *  Local variables:
 *  c-indentation-style: "K&R"
 *  c-basic-offset: 8
 *  tab-width: 8
 *  fill-column: 80
 *  scroll-step: 1
 *  End:
 */
