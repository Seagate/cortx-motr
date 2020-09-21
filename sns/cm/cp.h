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

#ifndef __MOTR_SNS_CM_CP_H__
#define __MOTR_SNS_CM_CP_H__

#include "cm/cp.h"
#include "be/engine.h" /* struct m0_stob_io */
#include "be/extmap.h" /* struct m0_be_emap */

/**
   @defgroup SNSCMCP SNS copy machine Copy packet
   @ingroup SNSCM

 */

struct m0_sns_cm_cp {
	struct m0_cm_cp        sc_base;

	/** cob fid of the cob this copy packet is targeted for. */
	struct m0_fid          sc_cobfid;

	/** Read/write stob id. */
	struct m0_stob_id      sc_stob_id;

	/**
	 * This is true if for the local/outgoing copy packet and false
	 * for incoming copy packet.
	 * This flag is also used to select the buffer pool to assign buffers
	 * for a particular copy packet.
	 */
	bool                   sc_is_local;

	uint64_t               sc_failed_idx;

	bool                   sc_is_acc;

	/** Offset within the stob. */
	m0_bindex_t            sc_index;

	/** Stob IO context. */
	struct m0_stob_io      sc_stio;

	/** Stob context. */
	struct m0_stob        *sc_stob;

	/**
	 * Flag to decide if punch operation on spare extent should be called.
	 * The value of the flag is decided by the return value from
	 * m0_stob_punch_credit(), if it returns 0 then flag will be set to
	 * true and stob will be punched for the extents allocated for spare
	 * space in previous sns operation. If it reurns -ENOENT which means
	 * there are no extents allocated for the spare (very first write
	 * on spare space) the flag will be set to false, so punch operaion
	 * will not be called in this case.
	 */
	bool                   sc_spare_punch;

	bool                   sc_is_hole_eof;

	/** FOL record frag for storage objects. */
	struct m0_fol_frag     sc_fol_frag;
};

M0_INTERNAL struct m0_sns_cm_cp *cp2snscp(const struct m0_cm_cp *cp);

/**
 * Uses GOB fid key and parity group number to generate a scalar to
 * help select a request handler locality for copy packet FOM.
 */
M0_INTERNAL uint64_t cp_home_loc_helper(const struct m0_cm_cp *cp);
M0_INTERNAL struct m0_cm *cpfom2cm(struct m0_fom *fom);

M0_INTERNAL bool m0_sns_cm_cp_invariant(const struct m0_cm_cp *cp);

extern const struct m0_cm_cp_ops m0_sns_cm_repair_cp_ops;
extern const struct m0_cm_cp_ops m0_sns_cm_rebalance_cp_ops;
extern const struct m0_cm_cp_ops m0_sns_cm_acc_cp_ops;

M0_INTERNAL int m0_sns_cm_cp_init(struct m0_cm_cp *cp);
M0_INTERNAL int m0_sns_cm_cp_fail(struct m0_cm_cp *cp);

/** Copy packet read phase function. */
M0_INTERNAL int m0_sns_cm_cp_read(struct m0_cm_cp *cp);

M0_INTERNAL int m0_sns_cm_cp_write_pre(struct m0_cm_cp *cp);

/** Copy packet write phase function. */
M0_INTERNAL int m0_sns_cm_cp_write(struct m0_cm_cp *cp);

/** Copy packet IO wait phase function. */
M0_INTERNAL int m0_sns_cm_cp_io_wait(struct m0_cm_cp *cp);

M0_INTERNAL int m0_sns_cm_cp_sw_check(struct m0_cm_cp *cp);

M0_INTERNAL int m0_sns_cm_cp_send(struct m0_cm_cp *cp, struct m0_fop_type *ft);

M0_INTERNAL int m0_sns_cm_cp_send_wait(struct m0_cm_cp *cp);

M0_INTERNAL int m0_sns_cm_cp_buf_acquire(struct m0_cm_cp *cp);

M0_INTERNAL int m0_sns_cm_cp_recv_init(struct m0_cm_cp *cp);

M0_INTERNAL int m0_sns_cm_cp_recv_wait(struct m0_cm_cp *cp);

M0_INTERNAL void m0_sns_cm_cp_complete(struct m0_cm_cp *cp);

M0_INTERNAL int m0_sns_cm_cp_phase_next(struct m0_cm_cp *cp);

M0_INTERNAL void m0_sns_cm_cp_free(struct m0_cm_cp *cp);

M0_INTERNAL int m0_sns_cm_cp_fini(struct m0_cm_cp *cp);

M0_INTERNAL int m0_sns_cm_cp_next_phase_get(int phase, struct m0_cm_cp *cp);

M0_INTERNAL void m0_sns_cm_cp_tgt_info_fill(struct m0_sns_cm_cp *scp,
					    const struct m0_fid *cob_fid,
					    uint64_t stob_offset,
					    uint64_t ag_cp_idx);

M0_INTERNAL int m0_sns_cm_cp_setup(struct m0_sns_cm_cp *scp,
				    const struct m0_fid *cob_fid,
				    uint64_t stob_offset,
				    uint64_t data_seg_nr,
				    uint64_t failed_unit_index,
				    uint64_t ag_cp_idx);

M0_INTERNAL void m0_sns_cm_cp_buf_release(struct m0_cm_cp *cp);
M0_INTERNAL int m0_sns_cm_cp_dup(struct m0_cm_cp *src, struct m0_cm_cp **dest);
M0_INTERNAL int m0_sns_cm_cp_tx_open(struct m0_cm_cp *cp);
M0_INTERNAL int m0_sns_cm_cp_tx_close(struct m0_cm_cp *cp);
M0_INTERNAL struct m0_cob_domain *m0_sns_cm_cp2cdom(struct m0_cm_cp *cp);

/** @} SNSCMCP */
#endif /* __MOTR_SNS_CM_CP_H__ */
/*
 *  Local variables:
 *  c-indentation-style: "K&R"
 *  c-basic-offset: 8
 *  tab-width: 8
 *  fill-column: 80
 *  scroll-step: 1
 *  End:
 */
