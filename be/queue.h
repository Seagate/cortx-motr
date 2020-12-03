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
 * Fair bounded FIFO MPMC queue.
 *
 * Highlights
 *
 * - m0_be_queue_put()/m0_be_queue_end()/m0_be_queue_get()/m0_be_queue_peek()
 *   expect the queue lock to be taken using m0_be_queue_lock();
 * - memory allocation happens only in m0_be_queue_init(). All other queue
 *   functions don't allocate memory from heap and just use pre-allocated
 *   memory, that had been allocated in m0_be_queue_init();
 * - the interface is non-blocking, i.e. each function always returns without
 *   waiting on a synchronisation primitive or something similar. If an
 *   operation, provided by a function, requires waiting for something (free
 *   space available, an item etc.), m0_be_op is used to tell that the operation
 *   is complete;
 * - op callbacks are called from under m0_be_queue lock, so they MUST NOT call
 *   m0_be_queue functions for the same queue;
 * - queues use lists in this way: items are added at tail and are removed from
 *   the head;
 *
 * Design decisions
 *
 * - as the queue is bounded m0_be_queue_put() could be called when the queue is
 *   full. In this case op wouldn't become done until there is more space in the
 *   queue. The implementation actually adds the item to the queue in such
 *   cases, because it doesn't change externally visible behaviour and it makes
 *   the implementation simpler by avoiding having another "almost-put-it-there"
 *   queues;
 * - m0_be_queue_get() could fail iff m0_be_queue_end() has been called for the
 *   queue. This may happen after m0_be_queue_get() is already finished and the
 *   user waits for the operation completion on op. This makes it impossible to
 *   pass the "if-the-get-is-successful" flag as the return value of the
 *   function, so it's returned as a pointer to a boolean flag;
 * - m0_be_queue_peek() doesn't need m0_be_op because it always returns whatever
 *   is at the top of the queue. So it could return "something has been
 *   returned" as the return value of the function.
 *
 * Internal queues diagram
 *
 * - the queue is neither full nor empty
 *
 * @verbatim
 *                 m0_be_queue_put()
 *                        |
 *                        v
 *          +---+---+---+---+         +---+---+---+---+---+---+
 *    head  |   |   |   |   | bq_q    |   |   |   |   |   |   | bq_q_unused
 *          +---+---+---+---+         +---+---+---+---+---+---+
 *            |
 *            v
 *     m0_be_queue_get()
 *
 * @endverbatim
 *
 * - the queue is full
 *
 * @verbatim
 *                                                  m0_be_queue_put()
 *                                                           |
 *                                                           v
 *                                                         +---+  +---+
 *                                                         |   |<<|   |
 *          +---+---+---+---+---+---+---+---+---+---+      +---+  +---+
 *          |   |   |   |   |   |   |   |   |   |   | bq_q |   |<<|   |
 *          +---+---+---+---+---+---+---+---+---+---+      +---+  +---+
 *            |                                              ^      ^
 *            v                                         this one    |
 *     m0_be_queue_get()                                is still    |
*                                                       in bq_q     |
 *                                                                  |
 *                                                      bq_op_put list
 *                                                      items point to the
 *                                                      items that are in
 *                                                      the put wait list
 * @endverbatim
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

/*
 * Put a buffer to the queue.
 *
 * @param data  the data from this buffer is copied to the queue internal data
 *              structures
 *
 * @pre data->b_nob == bq->bq_cfg.bqc_item_length
 *
 * - if the queue is full, then the operation would block until a space for at
 *   least one item becomes available in the queue;
 * - different producers waiting on m0_be_queue_put() would be awaken in the
 *   same order they were put to sleep.
 */
M0_INTERNAL void m0_be_queue_put(struct m0_be_queue  *bq,
                                 struct m0_be_op     *op,
                                 const struct m0_buf *data);

/*
 * Nothing is going to be put to the queue after this call.
 * This function could be called only once.
 */
M0_INTERNAL void m0_be_queue_end(struct m0_be_queue *bq);

/*
 * Get a buffer from the queue.
 *
 * @param data          The data is copied from queue internal data structures
 *                      to the buffer this parameter points to.
 * @param successful    true if the data is returned, false if the queue is
 *                      empty and m0_be_queue_end() has been called before this
 *                      call.
 *
 * @pre data->b_nob == bq->bq_cfg.bqc_item_length
 *
 * - if the queue is empty, then the operation would block until at least one
 *   item becomes available in the queue;
 * - different consumers waiting on m0_be_queue_get() would be awaken in the
 *   same order they were put to sleep.
 */
M0_INTERNAL void m0_be_queue_get(struct m0_be_queue *bq,
                                 struct m0_be_op    *op,
                                 struct m0_buf      *data,
                                 bool               *successful);

/*
 * Peek at the queue.
 *
 * @param data  the buffer that is in the queue internal data structures is
 *              copied to the memory data parameter points to.
 * @return      true if the data was copied, false if the queue is empty.
 *
 * @pre data->b_nob == bq->bq_cfg.bqc_item_length
 */
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
