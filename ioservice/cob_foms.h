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

#ifndef __MOTR_IOSERVICE_COB_FOMS_H__
#define __MOTR_IOSERVICE_COB_FOMS_H__

#include "cob/cob.h"

/* import */
struct m0_storage_dev;

/**
 * Phases of cob create/delete state machine.
 */
enum m0_fom_cob_operations_phases {
	M0_FOPH_COB_OPS_PREPARE = M0_FOPH_NR + 1,
	/**
	 * Internally creates/deletes a stob, a cob and adds/removes a record to
	 * or from auxiliary database.
	 * Or this is a getattr/setattr.
	 */
	M0_FOPH_COB_OPS_EXECUTE
};

/**
 * Fom context object for "cob create" and "cob delete" fops.
 * Same structure is used for both type of fops.
 */
struct m0_fom_cob_op {
	/** Stob identifier. */
	struct m0_stob_id        fco_stob_id;
	/** Generic fom object. */
	struct m0_fom		 fco_fom;
        /** Pool version for this request. */
        struct m0_pool_version  *fco_pver;
	/** Fid of global file. */
	struct m0_fid		 fco_gfid;
	/** Fid of component object. */
	struct m0_fid		 fco_cfid;
	/** Unique cob index in pool. */
	uint32_t                 fco_cob_idx;
	/** Cob type. */
	enum m0_cob_type         fco_cob_type;
	struct m0_stob          *fco_stob;
	bool                     fco_is_done;
	enum m0_cob_op           fco_fop_type;
	bool                     fco_recreate;
	/**
         * Range of segments to be truncated or
	 * deleted. The number of segments to be
	 * truncated or deleted is restricted by
	 * the available BE transaction credits.
	 */
	struct m0_indexvec       fco_range;
	/**
	 * fco_range contains packed segments which
	 * can be large for truncation, so truncation
	 * is done by storing a segment in fco_want.
         */
	struct m0_indexvec       fco_want;
	/**
	 * Current index of the segemnt of fco_range
         * for which truncation will be done.
	 */
	uint32_t                 fco_range_idx;
	/**
         * Range of segments to be truncated for
	 * which BE credits are available.
	 */
	struct m0_indexvec       fco_got;
	/** FOL rec fragment for create and delete operations. */
	struct m0_fol_frag       fco_fol_frag;
	/** The flags from m0_fop_cob_common::c_flags. */
	uint64_t                 fco_flags;
};

M0_INTERNAL int m0_cob_fom_create(struct m0_fop *fop, struct m0_fom **out,
				  struct m0_reqh *reqh);

/**
 * Create the cob for the cob domain.
 */
M0_INTERNAL int m0_cc_cob_setup(struct m0_fom_cob_op     *cc,
				struct m0_cob_domain     *cdom,
				const struct m0_cob_attr *attr,
				struct m0_be_tx	         *ctx);

M0_INTERNAL int m0_cc_stob_cr_credit(struct m0_stob_id *sid,
				     struct m0_be_tx_credit *accum);

M0_INTERNAL int m0_cc_stob_create(struct m0_fom *fom, struct m0_stob_id *sid);

M0_INTERNAL int m0_cc_cob_nskey_make(struct m0_cob_nskey **nskey,
				     const struct m0_fid *gfid,
				     uint32_t cob_idx);

M0_INTERNAL size_t m0_cob_io_fom_locality(const struct m0_fid *fid);

#endif    /* __MOTR_IOSERVICE_COB_FOMS_H__ */

/*
 *  Local variables:
 *  c-indentation-style: "K&R"
 *  c-basic-offset: 8
 *  tab-width: 8
 *  fill-column: 80
 *  scroll-step: 1
 *  End:
 */
