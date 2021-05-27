/* -*- C -*- */
/*
 * Copyright (c) 2021 Seagate Technology LLC and/or its Affiliates
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
#include "be/dtm0_log.h"
#include "dtm0/tx_desc.h"
#include "dtm0/tx_desc_xc.h"


extern struct m0_fop_type dtm0_req_fop_fopt;
extern struct m0_fop_type dtm0_rep_fop_fopt;

extern const struct m0_rpc_item_ops dtm0_req_fop_rpc_item_ops;


M0_INTERNAL int m0_dtm0_fop_init(void);
M0_INTERNAL void m0_dtm0_fop_fini(void);

enum m0_dtm0s_msg {
	DMT_EXECUTE,
	DMT_EXECUTED,
	/* XXX: Is it a typo? why do we have DMT everywhere except here? */
	DTM_PERSISTENT,
	DMT_REDO
} M0_XCA_ENUM;

struct dtm0_req_fop {
	uint32_t		dtr_msg M0_XCA_FENUM(
	m0_dtm0s_msg);
	struct m0_dtm0_tx_desc dtr_txr;
} M0_XCA_RECORD M0_XCA_DOMAIN(rpc);

struct dtm0_rep_fop {
	/** Status code of dtm operation. */
	int32_t			dr_rc;
	/** operation results. */
	struct m0_dtm0_tx_desc	dr_txr;
} M0_XCA_RECORD M0_XCA_DOMAIN(rpc);

/** Structure that describes dtm0 FOM. */
struct dtm0_fom {
	/** Caller FOM. */
	struct m0_fom dtf_fom;
};

M0_INTERNAL int m0_dtm0_on_committed(struct m0_fom                *fom,
				     const struct m0_dtm0_tx_desc *txd);

M0_INTERNAL int m0_dtm0_logrec_update(struct m0_be_dtm0_log  *log,
				      struct m0_be_tx        *tx,
				      struct m0_dtm0_tx_desc *txd,
				      struct m0_buf          *pyld);
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
