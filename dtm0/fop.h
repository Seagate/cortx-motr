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


#pragma once

#ifndef __MOTR_DTM0_FOP_FOMS_H__
#define __MOTR_DTM0_FOP_FOMS_H__

#include "lib/types.h"
#include "xcode/xcode_attr.h"
#include "fop/fop.h"
#include "fop/fom_generic.h"
#include "fop/fom_long_lock.h"
#include "be/op.h"
#include "be/tx_credit.h"
#include "lib/buf.h"
#include "lib/buf_xc.h"
#include "lib/types.h"
#include "rpc/at.h"
#include "xcode/xcode_attr.h"
#include "format/format.h"
#include "be/dtm0_log.h"


extern struct m0_fop_type dtm0_req_fop_fopt;
extern struct m0_fop_type dtm0_rep_fop_fopt;

extern const struct m0_rpc_item_ops dtm0_req_fop_rpc_item_ops;


int m0_dtm0_fop_init(void);
void m0_dtm0_fop_fini(void);

/**
 * DTM0S operation flags.
 */
enum m0_dtm0s_op_flags {
	/**
	 * Define any operation flags
	 */
	DOF_NONE = 0,
	/**
	 * Delay reply until transaction is persisted.
	 */
	DOF_SYNC_WAIT = 1 << 0,
} M0_XCA_ENUM;

/**
* DTM0S operation codes.
*/

enum m0_dtm0s_opcode{
	DT_REQ,
	DT_REDO,
	DT_NR,
} M0_XCA_ENUM;

enum m0_dtm0s_msg {
	DMT_EXECUTE_DTX,
	DMT_EXECUTED,
	DMT_PERSISTENT,
	DMT_NR,
} M0_XCA_ENUM;

struct dtm0_req_fop {
	uint32_t		 dto_opcode M0_XCA_FENUM(
	m0_dtm0s_opcode);
	uint32_t		 dto_opmsg M0_XCA_FENUM(
	m0_dtm0s_msg);
	uint32_t		 dto_opflags M0_XCA_FENUM(
	m0_dtm0s_op_flags);
	struct m0_dtm0_txr	*dto_txr;
} M0_XCA_RECORD M0_XCA_DOMAIN(rpc);

struct dtm0_rep_fop {
	/** Status code of dtm operation. */
	int32_t			 dr_rc;
	/** operation results. */
	struct m0_dtm0_txr	*dr_txr;
} M0_XCA_RECORD M0_XCA_DOMAIN(rpc);

/* __MOTR_DTM0_FOP_FOMS_H__ */
#endif

/*
 *  Local variables:
 *  c-indentation-style: "K&R"
 *  c-basic-offset: 8
 *  tab-width: 8
 *  fill-column: 80
 *  scroll-step: 1
 *  End:
 */
