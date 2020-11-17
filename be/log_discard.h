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

#ifndef __MOTR_BE_LOG_DISCARD_H__
#define __MOTR_BE_LOG_DISCARD_H__

#include "lib/mutex.h"          /* m0_mutex */
#include "lib/tlist.h"          /* m0_tl */
#include "lib/time.h"           /* m0_time_t */
#include "lib/semaphore.h"      /* m0_semaphore */

#include "be/op.h"              /* m0_be_op */
#include "be/pool.h"            /* m0_be_pool */

/**
 * @defgroup be
 *
 * * Overview
 *
 * m0_be_log_discard is an abstraction to keep seg I/O accounting and to control
 * BE log records discard.
 * m0_be_log_discard_item represents group's m0_be_log_record + seg I/O for
 * the group.
 *
 * * m0_be_log_discard_item lifecycle and meaning in BE
 *
 * - ---> m0_be_log_discard_item_get()
 *              - it's the only way to get m0_be_log_discard_item
 * - ---> m0_be_log_discard_item_starting()
 *              - seg I/O is about to start
 * - ---> m0_be_log_discard_item_finished()
 *              - seg I/O just finished
 * - <--- m0_be_log_discard_cfg::ldsc_sync() (optional)
 *              - fdatasync() should be called for all segment backing stobs
 * - <--- m0_be_log_discard_cfg::ldsc_discard()
 *              - m0_be_log_record can be discarded from log
 * - ---> m0_be_log_discard_item_put()
 *              - m0_be_log_discard_item is no longer needed
 *
 * Legend
 * - ---> - action from the user
 * - <--- - request from m0_be_log_discard
 *
 * Highlights
 * - order of items is determined by m0_be_log_discard_item_starting() call.
 *
 * Future directions
 * - add ordering based on m0_ext. Assign m0_ext to each m0_be_log_discard_item
 *   and use the m0_ext to sort items by the range.
 * @{
 */

struct m0_locality;
struct m0_ext;
struct m0_be_op;
struct m0_be_log_discard;
struct m0_be_log_discard_item;

struct m0_be_log_discard_cfg {
	void               (*ldsc_sync)(struct m0_be_log_discard      *ld,
	                                struct m0_be_op               *op,
	                                struct m0_be_log_discard_item *ldi);
	void               (*ldsc_discard)(struct m0_be_log_discard      *ld,
	                                   struct m0_be_log_discard_item *ldi);
	uint32_t             ldsc_items_max;
	/**
	 * m0_be_log_discard starts sync on all finished items when
	 * ldsc_items_threshold items are taken by user using
	 * m0_be_log_discard_item_get().
	 */
	uint32_t             ldsc_items_threshold;
	uint32_t             ldsc_items_pending_max;
	struct m0_locality  *ldsc_loc;
	m0_time_t            ldsc_sync_timeout;
};

struct m0_be_log_discard {
	struct m0_be_log_discard_cfg         lds_cfg;
	struct m0_be_log_discard_item       *lds_item;
	struct m0_mutex                      lds_lock;
	struct m0_be_pool                    lds_item_pool;
	struct m0_tl                         lds_start_q;
	bool                                 lds_need_sync;
	bool                                 lds_sync_in_progress;
	struct m0_be_log_discard_item       *lds_sync_item;
	m0_time_t                            lds_sync_deadline;
	struct m0_be_op                      lds_sync_op;
	struct m0_be_op                     *lds_flush_op;
	struct m0_sm_timer                   lds_sync_timer;
	bool                                 lds_stopping;
	/**
	 * m0_be_log_discard_fini() will wait on this semaphore
	 * until discard callbacks are executed.
	 * XXX Temporary solution.
	 */
	struct m0_semaphore                  lds_discard_wait_sem;
	bool                                 lds_discard_waiting;
	/**
	 * Ast and flags to discard multiple log records at once.
	 */
	struct m0_sm_ast                     lds_discard_ast;
	bool                                 lds_discard_ast_posted;
};

M0_INTERNAL int m0_be_log_discard_init(struct m0_be_log_discard     *ld,
                                       struct m0_be_log_discard_cfg *ld_cfg);
M0_INTERNAL void m0_be_log_discard_fini(struct m0_be_log_discard *ld);

M0_INTERNAL void m0_be_log_discard_sync(struct m0_be_log_discard *ld);
M0_INTERNAL void m0_be_log_discard_flush(struct m0_be_log_discard *ld,
                                         struct m0_be_op          *op);

M0_INTERNAL void
m0_be_log_discard_item_starting(struct m0_be_log_discard      *ld,
                                struct m0_be_log_discard_item *ldi);
M0_INTERNAL void
m0_be_log_discard_item_finished(struct m0_be_log_discard      *ld,
                                struct m0_be_log_discard_item *ldi);

M0_INTERNAL void
m0_be_log_discard_item_get(struct m0_be_log_discard       *ld,
                           struct m0_be_op                *op,
                           struct m0_be_log_discard_item **ldi);
M0_INTERNAL void m0_be_log_discard_item_put(struct m0_be_log_discard      *ld,
                                            struct m0_be_log_discard_item *ldi);

M0_INTERNAL void
m0_be_log_discard_item_user_data_set(struct m0_be_log_discard_item *ldi,
                                     void                          *data);
M0_INTERNAL void *
m0_be_log_discard_item_user_data(struct m0_be_log_discard_item *ldi);

M0_INTERNAL void
m0_be_log_discard_item_ext_set(struct m0_be_log_discard_item *ldi,
                               struct m0_ext                 *ext);
M0_INTERNAL struct m0_ext *
m0_be_log_discard_item_ext(struct m0_be_log_discard_item *ldi);


/** @} end of be group */
#endif /* __MOTR_BE_LOG_DISCARD_H__ */

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
