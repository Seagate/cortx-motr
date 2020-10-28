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

#ifndef __MOTR_BE_QUEUE_H__
#define __MOTR_BE_QUEUE_H__

/**
 * @defgroup be
 *
 * Highlights
 *
 * - put()/get()/peek()/end() expect queue lock to be taken;
 * - op callbacks are called from under m0_be_queue lock, so they MUST NOT call
 *   m0_be_queue functions for the same bq;
 * - queues use lists in this way: items are added at tail and are removed from
 *   the head;
 *
 * Further directions
 *
 * - put() multiple items at once
 *
 * @{
 */

#include "lib/types.h"          /* m0_bcount_t */
#include "lib/tlist.h"          /* m0_tl */
#include "lib/mutex.h"          /* m0_mutex */

#include "be/tx_credit.h"       /* m0_be_tx_credit */


struct m0_be_op;
struct be_queue_wait_op;
struct m0_buf;

struct m0_be_queue_cfg {
	uint64_t    bqc_q_size_max;
	uint64_t    bqc_producers_nr_max;
	uint64_t    bqc_consumers_nr_max;
	m0_bcount_t bqc_item_length;
};

struct m0_be_queue {
	struct m0_be_queue_cfg   bq_cfg;
	struct m0_mutex          bq_lock;

	/** bqq_tl, be_queue_item::bqi_link */
	struct m0_tl             bq_q;
	struct m0_tl             bq_q_unused;

	/**
	 * Pre-allocated array of be_queue_item.
	 *
	 * - initially all items are in m0_be_queue::bq_q_unused;
	 * - when m0_be_queue_put() is called an item is moved to
	 *   m0_be_queue::bq_q;
	 * - after m0_be_get() is done the item is moved back;
	 * - array of pointers to be_queue_item couldn't be used because the
	 *   structure has a flexible array member at the end.
	 */
	char                    *bq_qitems;

	/** bqop_tl, be_queue_item::bqi_link */
	struct m0_tl             bq_op_put;
	struct m0_tl             bq_op_put_unused;
	struct m0_tl             bq_op_get;
	struct m0_tl             bq_op_get_unused;

	/** Is used to wait in m0_be_queue_get() */
	struct be_queue_wait_op *bq_ops_get;
	/** Is used to wait in m0_be_queue_put() */
	struct be_queue_wait_op *bq_ops_put;

	bool                     bq_the_end;

	uint64_t                 bq_enqueued;
	uint64_t                 bq_dequeued;
};

#define BEQ_F "(queue=%p bq_enqueued=%"PRIu64" bq_dequeued=%"PRIu64" " \
	"bq_the_end=%d)"
#define BEQ_P(bq) (bq), (bq)->bq_enqueued, (bq)->bq_dequeued, \
	!!((bq)->bq_the_end)

M0_INTERNAL int m0_be_queue_init(struct m0_be_queue     *bq,
                                 struct m0_be_queue_cfg *cfg);
M0_INTERNAL void m0_be_queue_fini(struct m0_be_queue *bq);

M0_INTERNAL void m0_be_queue_lock(struct m0_be_queue *bq);
M0_INTERNAL void m0_be_queue_unlock(struct m0_be_queue *bq);

M0_INTERNAL void m0_be_queue_put(struct m0_be_queue *bq,
                                 struct m0_be_op    *op,
                                 struct m0_buf      *data);
/* nothing is going to be added to the queue after this call */
M0_INTERNAL void m0_be_queue_end(struct m0_be_queue *bq);
M0_INTERNAL void m0_be_queue_get(struct m0_be_queue *bq,
                                 struct m0_be_op    *op,
                                 struct m0_buf      *data,
                                 bool               *successful);
M0_INTERNAL bool m0_be_queue_peek(struct m0_be_queue *bq,
                                  struct m0_buf      *data);

#define M0_BE_QUEUE_PUT(bq, op, ptr)                                           \
		m0_be_queue_put(bq, op, &M0_BUF_INIT_PTR(ptr))
#define M0_BE_QUEUE_GET(bq, op, ptr, successful)                               \
		m0_be_queue_get(bq, op, &M0_BUF_INIT_PTR(ptr), successful)
#define M0_BE_QUEUE_PEEK(bq, ptr)                                              \
		m0_be_queue_peek(bq, &M0_BUF_INIT_PTR(ptr))

/** @} end of be group */
#endif /* __MOTR_BE_QUEUE_H__ */

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
