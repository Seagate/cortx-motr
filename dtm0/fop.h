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

#ifndef __MOTR_DTM0_FOP_H__
#define __MOTR_DTM0_FOP_H__

#include "fop/fop.h"
#include "fop/fom_generic.h"
#include "fop/fom_long_lock.h"
#include "be/op.h"
#include "be/btree.h"
#include "be/btree_xc.h"
#include "be/tx_credit.h"
/* #include "dtm0/fop_xc.h" */
#include "lib/buf.h"
#include "lib/buf_xc.h"
#include "lib/types.h"
#include "rpc/at.h"
#include "xcode/xcode_attr.h"
#include "format/format.h"

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

/* Dummy definitions for dtm0_log structures */
struct m0_dtm0_log_record {
	uint64_t               magic;
} M0_XCA_RECORD M0_XCA_DOMAIN(rpc);

struct m0_dtm0_txr {
	struct m0_buf                   dt_txr_payload;
} M0_XCA_RECORD M0_XCA_DOMAIN(rpc);

struct dtm0_op {
	uint32_t		 dto_opcode;
	uint32_t		 dto_opmsg;
	uint32_t		 dto_opflags;
	struct m0_dtm0_txr	*dto_txr;
} M0_XCA_RECORD M0_XCA_DOMAIN(rpc);

struct m0_dtm0_req {
	struct dtm0_op		    *op;
	/** DTM0 log record for storing the operation data. */
	struct m0_dtm0_log_record   *dtf_lgr;
	/** Manages calling of callback on completion of dtm0 operation. */
	struct m0_clink		     dtf_clink;
	/** Channel to communicate with caller FOM. */
	struct m0_chan		     dtf_channel;
	/** Channel guard. */
	struct m0_mutex		     dtf_channel_lock;
	/** Operation result code. */
	int32_t			     dtf_rc;
};

struct dtm0_rep_op {
	/** Status code of dtm operation. */
	int32_t			dr_rc;
	/** operation results. */
	struct m0_dtm0_txr	dr_txr;
} M0_XCA_RECORD M0_XCA_DOMAIN(rpc);

/** Structure that describes dtm0 FOM. */
struct dtm0_fom {
	/** Caller FOM. */
	struct m0_fom		     dtf_fom;
};

/* __MOTR_DTM0_FOP_H__ */
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
