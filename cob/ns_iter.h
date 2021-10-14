/* -*- C -*- */
/*
 * Copyright (c) 2012-2021 Seagate Technology LLC and/or its Affiliates
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

#ifndef __MOTR_COB_NS_ITER_H__
#define __MOTR_COB_NS_ITER_H__

#include "cob/cob.h"

/**
 * @defgroup cob_fid_ns_iter Cob-fid namespace iterator
 *
 * The cob on data server has cob nskey = <gob_fid, cob_index>,
 * where,
 * gob_fid   : global file identifier corresponding to which the cob is
 *             being created.
 * cob_index : unique index of the cob in the pool.
 *
 * @see m0_cob_nskey
 *
 * The cob-fid iterator uniquely iterates over gob_fids, thus skipping entries
 * with same gob_fids but different cob_index.
 *
 * This iterator is used in SNS repair iterator. @see m0_sns_repair_iter
 *
 * @{
 */

struct m0_cob_fid_ns_iter {
	/** Cob domain. */
	struct m0_cob_domain *cni_cdom;

	/** Last fid value returned. */
	struct m0_fid         cni_last_fid;
};

/**
 * Initialises the namespace iterator.
 * @param iter - Cob fid namespace iterator that is to be initialised.
 * @param gfid - Initial gob-fid with which iterator is initialised.
 * @param dbenv - DB environment from which the records should be extracted.
 * @param cdom - Cob domain.
 */
M0_INTERNAL int m0_cob_ns_iter_init(struct m0_cob_fid_ns_iter *iter,
				    struct m0_fid *gfid,
				    struct m0_cob_domain *cdom);

/**
 * Iterates over namespace to point to unique gob fid in the namespace.
 * @param iter - Pointer to the namespace iterator.
 * @param tx - Database transaction used for DB operations by iterator.
 * @param gfid - Next unique gob-fid in the iterator. This is output variable.
 */
M0_INTERNAL int m0_cob_ns_iter_next(struct m0_cob_fid_ns_iter *iter,
				    struct m0_fid *gfid,
				    struct m0_cob_nsrec **nsrec);

M0_INTERNAL int m0_cob_ns_rec_of(struct m0_btree *cob_namespace,
				 const struct m0_fid *key_gfid,
				 struct m0_fid *next_gfid,
				 struct m0_cob_nsrec **nsrec);

/**
 * Finalises the namespace iterator.
 * @param iter - Namespace iterator that is to be finalised.
 */
M0_INTERNAL void m0_cob_ns_iter_fini(struct m0_cob_fid_ns_iter *iter);

/** @} end group cob_fid_ns_iter */

#endif    /* __MOTR_COB_NS_ITER_H__ */
/*
 *  Local variables:
 *  c-indentation-style: "K&R"
 *  c-basic-offset: 8
 *  tab-width: 8
 *  fill-column: 80
 *  scroll-step: 1
 *  End:
 */
