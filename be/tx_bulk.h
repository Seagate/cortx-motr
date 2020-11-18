/* -*- C -*- */
/*
 * Copyright (c) 2015-2020 Seagate Technology LLC and/or its Affiliates
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

#ifndef __MOTR_BE_TX_BULK_H__
#define __MOTR_BE_TX_BULK_H__

#include "lib/types.h"          /* uint32_t */
#include "lib/mutex.h"          /* m0_mutex */

#include "be/queue.h"           /* m0_be_queue */
#include "be/op.h"              /* m0_be_op */

/**
 * @defgroup be
 *
 * tx_bulk is an abstraction that allows user to execute uniform transactions as
 * fast as possible.
 *
 * @see m0_be_ut_tx_bulk_usecase() for an example.
 *
 * Future directions
 * - use m0_fom instead of asts
 * - use m0_module for init()/fini()
 * - support multiple partitions per worker
 *
 * @{
 */

struct m0_be_op;
struct m0_be_domain;
struct m0_be_tx_credit;
struct m0_be_tx;
struct m0_be_tx_bulk;
struct be_tx_bulk_worker;

/**
 * User configuration with user-supplied callbacks for m0_be_tx_bulk.
 *
 * - tbc_do() should be thread-safe because it can be called from any locality.
 *   It could also be called from different localities at the same time;
 * - it's possible to have more than one tbc_do() call in a single transaction.
 */
struct m0_be_tx_bulk_cfg {
	struct m0_be_queue_cfg   tbc_q_cfg;
	uint64_t                 tbc_workers_nr;
	uint64_t                 tbc_partitions_nr;
	uint64_t                 tbc_work_items_per_tx_max;
	/** BE domain for transactions */
	struct m0_be_domain     *tbc_dom;
	/** it's passed as a parameter to m0_be_tx_bulk_cfg::tbc_do() */
	void                    *tbc_datum;
	/** do some work in the context of a BE transaction */
	void                   (*tbc_do)(struct m0_be_tx_bulk *tb,
	                                 struct m0_be_tx      *tx,
	                                 struct m0_be_op      *op,
	                                 void                 *datum,
	                                 void                 *user,
	                                 uint64_t              worker_index,
	                                 uint64_t              partition);
	/** tx with this operation had become persistent */
	void                   (*tbc_done)(struct m0_be_tx_bulk *tb,
	                                   void                 *datum,
	                                   void                 *user,
	                                   uint64_t              worker_index,
	                                   uint64_t              partition);
};

struct m0_be_tx_bulk {
	struct m0_be_tx_bulk_cfg  btb_cfg;
	struct m0_be_queue       *btb_q;
	uint32_t                  btb_worker_nr;
	struct be_tx_bulk_worker *btb_worker;
	/** @see m0_be_tx_bulk_status */
	int                       btb_rc;
	/** protects access to m0_be_tx_bulk fields */
	struct m0_mutex           btb_lock;
	uint32_t                  btb_done_nr;
	bool                      btb_tx_open_failed;
	bool                      btb_done;
	bool                      btb_termination_in_progress;
	struct m0_be_op          *btb_op;
	struct m0_be_op           btb_kill_put_op;
};

M0_INTERNAL int m0_be_tx_bulk_init(struct m0_be_tx_bulk     *tb,
                                   struct m0_be_tx_bulk_cfg *tb_cfg);
M0_INTERNAL void m0_be_tx_bulk_fini(struct m0_be_tx_bulk *tb);

/**
 * Runs the work.
 * op is signalled after all work is done or some of m0_be_tx_open() failed.
 */
M0_INTERNAL void m0_be_tx_bulk_run(struct m0_be_tx_bulk *tb,
                                   struct m0_be_op      *op);

/** Add more work.  */
M0_INTERNAL bool m0_be_tx_bulk_put(struct m0_be_tx_bulk   *tb,
                                   struct m0_be_op        *op,
                                   struct m0_be_tx_credit *credit,
                                   m0_bcount_t             payload_credit,
                                   uint64_t                partition,
                                   void                   *user);
/* No new work is expected after this function is called. */
M0_INTERNAL void m0_be_tx_bulk_end(struct m0_be_tx_bulk *tb);

/**
 * Gets m0_be_tx_bulk result.
 * Can be called only after op from m0_be_tx_bulk_run is signalled.
 */
M0_INTERNAL int m0_be_tx_bulk_status(struct m0_be_tx_bulk *tb);


/** @} end of be group */
#endif /* __MOTR_BE_TX_BULK_H__ */

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
