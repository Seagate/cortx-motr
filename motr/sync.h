/* -*- C -*- */
/*
 * Copyright (c) 2017-2020 Seagate Technology LLC and/or its Affiliates
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


#include "mdservice/fsync_fops.h"       /* m0_fop_fsync_mds_fopt */
#include "cas/cas.h"                    /* m0_fop_fsync_cas_fopt */
#include "fop/fop.h"                    /* m0_fop */

#pragma once

#ifndef __MOTR_OSYNC_H__
#define __MOTR_OSYNC_H__

/**
 * Experimental: sync operation for objects (sync for short).
 * This is heavily based on the m0t1fs::fsync work, see fsync DLD
 * for details. Client re-uses the fsync fop defined in m0t1fs.
 */

/* import */
struct m0_obj;
struct m0_op_sync;
struct m0_reqh_service_ctx;

M0_TL_DESCR_DECLARE(spti, M0_EXTERN);
M0_TL_DECLARE(spti, M0_EXTERN, struct m0_reqh_service_txid);

enum sync_type {
	SYNC_ENTITY = 0,
	SYNC_OP,
	SYNC_INSTANCE
};

/**
 * The entity or op an SYNC request is going to sync.
 */
struct sync_target {
	uint32_t                         srt_type;
	union {
		struct m0_entity *srt_ent;
		struct m0_op     *srt_op;
	} u;

	/* Link to an SYNC request list. */
	struct m0_tlink                  srt_tlink;
	uint64_t                         srt_tlink_magic;
};

struct sync_request {
	/* Back pointer to sync_op. */
	struct m0_op_sync        *sr_op_sync;

	/** List of targets to sync. */
	struct m0_tl                     sr_targets;

	/** List of {service, txid} pairs constructed from all targets. */
	struct m0_mutex                  sr_stxs_lock;
	struct m0_tl                     sr_stxs;

	/**
	 * Records the number of FSYNC fops and fops(wrpper).
	 * sr_nr_fops seems redundant here but the purpose is to avoid
	 * scanning the list to get the length of sr_fops.
	 */
	struct m0_mutex                  sr_fops_lock;
	struct m0_tl                     sr_fops;
	int32_t                          sr_nr_fops;


	/* Post an AST when all fops of this SYNC request are done. */
	struct m0_sm_ast                 sr_ast;

	int32_t                          sr_rc;
};

/**
 * Wrapper for sync messages, used to list/group pending replies
 * and pair fop/reply with the struct m0_reqh_service_txid
 * that needs updating.
 */
struct sync_fop_wrapper {
	/** The fop for fsync messages */
	struct m0_fop                sfw_fop;

	/**
	 * The service transaction that needs updating
	 * gain the m0t1fs_inode::ci_pending_txid_lock lock
	 * for inodes or the m0_reqh_service_ctx::sc_max_pending_tx_lock
	 * for the super block before dereferencing
	 */
	struct m0_reqh_service_txid *sfw_stx;

	struct sync_request         *sfw_req;

	/* AST to handle when receiving reply fop. */
	struct m0_sm_ast             sfw_ast;

	/* Link to FSYNC fop list in a request. */
	struct m0_tlink              sfw_tlink;
	uint64_t                     sfw_tlink_magic;
};

/**
 * Ugly abstraction of sync interactions with wider motr code
 * - purely to facilitate unit testing.
 * - this is used in sync.c and its unit tests.
 */
struct sync_interactions {
	int (*si_post_rpc)(struct m0_rpc_item *item);
	int (*si_wait_for_reply)(struct m0_rpc_item *item, m0_time_t timeout);
	void (*si_fop_fini)(struct m0_fop *fop);
	void (*si_fop_put)(struct m0_fop *fop);
};

/**
 * Updates sync records in fop callbacks.
 * Service must be specified, one or both of csb/inode should be specified.
 * new_txid may be null.
 */
M0_INTERNAL void
sync_record_update(struct m0_reqh_service_ctx *service,
		   struct m0_entity *obj,
		   struct m0_op *op,
		   struct m0_be_tx_remid *btr);

/**
 * Return first entity from sync operation.
 * It is used as helper function to get client instance from
 * entity for sync operation.
 */
M0_INTERNAL struct m0_entity *
m0__op_sync_entity(const struct m0_op *op);

#endif /* __MOTR_OSYNC_H__ */

/*
 *  Local variables:
 *  c-indentation-style: "K&R"
 *  c-basic-offset: 8
 *  tab-width: 8
 *  fill-column: 80
 *  scroll-step: 1
 *  End:
 */
