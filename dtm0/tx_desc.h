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

#ifndef __MOTR_DTM0_TX_DESC_H__
#define __MOTR_DTM0_TX_DESC_H__

/* Transaction descriptor
 * ----------------------
 *
 *   Transaction descriptor is a on-wire/on-disk used to attach the following
 * information to RPC messages and log records:
 *     * unique TX id;
 *     * list of participants and their states;
 *
 * Use-cases
 * ---------
 *
 * 1. Tx descriptor as a part of a log record:
 * @verbatim
 *	struct log_record {
 *		tx_descr td;
 *		request  req;
 *		tlink    vlog_linkage;
 *	};
 *
 *	...
 *	log_record *rec = alloc();
 *	tx_desc_copy((fop.data as kvs_op).td, rec.td);
 *	rec->req = fop.data.copy();
 *	log_add(rec);
 *	...
 *
 * @endverbatim
 *
 * 2. Tx descriptor as a part of a data/metadata message:
 * @verbatim
 *	struct kvs_op {
 *		tx_descr td;
 *		kv_pairs kv;
 *	};
 *
 *	...
 *	kvs_op op;
 *	nr_devices = layout.nr_devices();
 *	tx_desc_init(&op->td, nr_devices);
 *	...
 *
 * @endverbatim
 *
 * 3. Tx descriptor as a part of a notice:
 * @verbatim
 *	struct persistent_notice_op {
 *		tx_descr td;
 *	};
 *
 *	...
 *	log_record = fom.log_record;
 *	persistent_notice_op item;
 *	tx_desc_copy(&item->td, log_record->td);
 *	post_item(persistent);
 *	....
 *
 * @endverbatim
 */

#include "fid/fid.h" /* m0_fid */
#include "fid/fid_xc.h"
#include "dtm0/clk_src.h" /* ts and cs */
#include "dtm0/clk_src_xc.h"
#include "xcode/xcode.h" /* XCA */

struct m0_dtm0_tid {
	struct m0_dtm0_ts dti_ts;
	struct m0_fid     dti_fid;
} M0_XCA_RECORD M0_XCA_DOMAIN(rpc|be);

#define DTID0_F "{" DTS0_F "," FID_F "}"
#define DTID0_P(__tid) DTS0_P(&(__tid)->dti_ts), FID_P(&(__tid)->dti_fid)

enum m0_dtm0_tx_pa_state {
	M0_DTPS_INIT,
	M0_DTPS_INPROGRESS,
	M0_DTPS_EXECUTED,
	M0_DTPS_PERSISTENT,
	M0_DTPS_NR,
} M0_XCA_ENUM M0_XCA_DOMAIN(rpc|be);

struct m0_dtm0_tx_pa {
	struct m0_fid p_fid;
	uint32_t      p_state M0_XCA_FENUM(m0_dtm0_tx_pa_state);
} M0_XCA_RECORD M0_XCA_DOMAIN(rpc|be);

/** A list of participants (and their states) of a transaction. */
struct m0_dtm0_tx_participants {
	uint32_t              dtp_nr;
	struct m0_dtm0_tx_pa *dtp_pa;
} M0_XCA_SEQUENCE M0_XCA_DOMAIN(rpc|be);

struct m0_dtm0_tx_desc {
	struct m0_dtm0_tid              dtd_id;
	struct m0_dtm0_tx_participants  dtd_ps;
} M0_XCA_RECORD M0_XCA_DOMAIN(rpc|be);

/** Writes a deep copy of "src" into "dst". */
M0_INTERNAL int m0_dtm0_tx_desc_copy(const struct m0_dtm0_tx_desc *src,
				     struct m0_dtm0_tx_desc       *dst);

/** Creates a new tx descriptor with the given number of participants. */
M0_INTERNAL int m0_dtm0_tx_desc_init(struct m0_dtm0_tx_desc *td,
				     uint32_t                nr_pa);

M0_INTERNAL void m0_dtm0_tx_desc_fini(struct m0_dtm0_tx_desc *td);

M0_INTERNAL int m0_dtm0_tid_cmp(struct m0_dtm0_clk_src   *cs,
				const struct m0_dtm0_tid *left,
				const struct m0_dtm0_tid *right);

M0_INTERNAL void m0_dtm0_tx_desc_init_none(struct m0_dtm0_tx_desc *td);
M0_INTERNAL bool m0_dtm0_tx_desc_is_none(const struct m0_dtm0_tx_desc *td);

M0_INTERNAL bool m0_dtm0_tid__invariant(const struct m0_dtm0_tid *tid);
M0_INTERNAL bool m0_dtm0_tx_desc__invariant(const struct m0_dtm0_tx_desc *td);

/** Returns "true" iif the state of each participant equals to the given
 * "state" value.
 */
M0_INTERNAL bool m0_dtm0_tx_desc_state_eq(const struct m0_dtm0_tx_desc *txd,
					  enum m0_dtm0_tx_pa_state      state);

/** Applies an update onto the given target value.
 * The state of the participants of the target modifed as per the following
 * statement:
 * @verbatim
 *	target = max_state(target, update);
 * @endverbatim
 */
M0_INTERNAL void m0_dtm0_tx_desc_apply(struct m0_dtm0_tx_desc *tgt,
				       const struct m0_dtm0_tx_desc *upd);

/* TODO: move them to a dtm0 internal header? */

#define dtds_forall(__txd, __exp)                                    \
	m0_forall(i, (__txd)->dtd_ps.dtp_nr,                        \
			 (__txd)->dtd_ps.dtp_pa[i].p_state __exp)

#define dtds_exists(__txd, __exp) !dtds_forall(__txd, __exp)



#endif /* __MOTR_DTM0_TX_DESC_H__ */

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
