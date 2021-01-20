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



/**
 * @addtogroup be
 *
 * @{
 */

#define M0_TRACE_SUBSYSTEM M0_TRACE_SUBSYS_BE
#include "lib/trace.h"

#include "be/queue.h"

#include "lib/errno.h"          /* ENOMEM */
#include "lib/memory.h"         /* M0_ALLOC_ARR */
#include "lib/buf.h"            /* m0_buf_memcpy */
#include "lib/misc.h"           /* ergo */

#include "be/op.h"              /* m0_be_op_active */


struct be_queue_item {
	uint64_t         bqi_magic;
	/** bqq_tl, m0_be_queue::bq_q */
	struct m0_tlink  bqi_link;
	char            *bqi_data[];
};

#define BE_QUEUE_ITEM2BUF(bq, bqi) \
	M0_BUF_INIT((bq)->bq_cfg.bqc_item_length, &(bqi)->bqi_data)

struct be_queue_wait_op {
	struct m0_be_op      *bbo_op;
	struct be_queue_item *bbo_bqi;
	struct m0_buf         bbo_data;
	bool                 *bbo_successful;
	uint64_t              bbo_magic;
	struct m0_tlink       bbo_link;
};

M0_TL_DESCR_DEFINE(bqq, "m0_be_queue::bq_q*[]", static,
		   struct be_queue_item, bqi_link, bqi_magic,
		   M0_BE_QUEUE_Q_MAGIC, M0_BE_QUEUE_Q_HEAD_MAGIC);
M0_TL_DEFINE(bqq, static, struct be_queue_item);

M0_TL_DESCR_DEFINE(bqop, "m0_be_queue::bq_op_*[]", static,
		   struct be_queue_wait_op, bbo_link, bbo_magic,
		   M0_BE_QUEUE_OP_MAGIC, M0_BE_QUEUE_OP_HEAD_MAGIC);
M0_TL_DEFINE(bqop, static, struct be_queue_wait_op);


static uint64_t bq_queue_items_max(struct m0_be_queue *bq)
{
	return bq->bq_cfg.bqc_q_size_max + bq->bq_cfg.bqc_producers_nr_max;
}

static struct be_queue_item *be_queue_qitem(struct m0_be_queue *bq,
					    uint64_t            index)
{
	M0_PRE(index < bq_queue_items_max(bq));
	return (struct be_queue_item *)
		(bq->bq_qitems + index *
		 (sizeof(struct be_queue_item) + bq->bq_cfg.bqc_item_length));
}

static bool be_queue_invariant(struct m0_be_queue *bq)
{
	M0_PRE(m0_mutex_is_locked(&bq->bq_lock));

	return bq->bq_enqueued >= bq->bq_dequeued;
}

M0_INTERNAL int m0_be_queue_init(struct m0_be_queue     *bq,
                                 struct m0_be_queue_cfg *cfg)
{
	uint64_t i;

	M0_ENTRY("bq=%p bqc_q_size_max=%"PRIu64" "
		 "bqc_producers_nr_max=%"PRIu64" bqc_consumers_nr_max=%"PRIu64,
		 bq, cfg->bqc_q_size_max,
		 cfg->bqc_producers_nr_max, cfg->bqc_consumers_nr_max);
	M0_PRE(M0_IS0(bq));
	M0_PRE(cfg->bqc_q_size_max > 0);
	M0_PRE(cfg->bqc_producers_nr_max > 0);
	M0_PRE(cfg->bqc_consumers_nr_max > 0);
	M0_PRE(cfg->bqc_item_length > 0);
	M0_PRE(M0_IS_8ALIGNED(cfg->bqc_item_length));

	bq->bq_cfg = *cfg;
	bq->bq_the_end = false;
	bq->bq_enqueued = 0;
	bq->bq_dequeued = 0;
	M0_ALLOC_ARR(bq->bq_qitems, bq_queue_items_max(bq) *
		     (sizeof(struct be_queue_item) + cfg->bqc_item_length));
	M0_ALLOC_ARR(bq->bq_ops_put, bq->bq_cfg.bqc_producers_nr_max);
	M0_ALLOC_ARR(bq->bq_ops_get, bq->bq_cfg.bqc_consumers_nr_max);
	if (bq->bq_qitems == NULL ||
	    bq->bq_ops_put == NULL ||
	    bq->bq_ops_get == NULL) {
		m0_free(bq->bq_ops_get);
		m0_free(bq->bq_ops_put);
		m0_free(bq->bq_qitems);
		return M0_ERR(-ENOMEM);
	}
	m0_mutex_init(&bq->bq_lock);
	bqop_tlist_init(&bq->bq_op_put_unused);
	for (i = 0; i < bq->bq_cfg.bqc_producers_nr_max; ++i) {
		bqop_tlink_init_at_tail(&bq->bq_ops_put[i],
		                        &bq->bq_op_put_unused);
	}
	bqop_tlist_init(&bq->bq_op_put);
	bqop_tlist_init(&bq->bq_op_get_unused);
	for (i = 0; i < bq->bq_cfg.bqc_consumers_nr_max; ++i) {
		bqop_tlink_init_at_tail(&bq->bq_ops_get[i],
		                        &bq->bq_op_get_unused);
	}
	bqop_tlist_init(&bq->bq_op_get);
	bqq_tlist_init(&bq->bq_q_unused);
	for (i = 0; i < bq_queue_items_max(bq); ++i) {
		bqq_tlink_init_at_tail(be_queue_qitem(bq, i),
		                       &bq->bq_q_unused);
	}
	bqq_tlist_init(&bq->bq_q);
	return M0_RC(0);
}

M0_INTERNAL void m0_be_queue_fini(struct m0_be_queue *bq)
{
	struct be_queue_wait_op *bwo;
	struct be_queue_item    *bqi;
	uint64_t                 i;

	M0_ENTRY("bq="BEQ_F, BEQ_P(bq));
	M0_ASSERT_INFO(bq->bq_enqueued == bq->bq_dequeued,
	               "bq="BEQ_F, BEQ_P(bq));

	m0_tl_for(bqq, &bq->bq_q, bqi) {
		/*
		 * M0_LOG() couldn't print the item buffer at once,
		 * unfortunately. So let's just at least show the number of
		 * items by printing every item.
		 */
		M0_LOG(M0_ERROR, "there is an item in the queue");
	} m0_tl_endfor;
	bqq_tlist_fini(&bq->bq_q);
	m0_tl_for(bqop, &bq->bq_op_get, bwo) {
		M0_LOG(M0_ERROR, "bq=%p bbo_data="BUF_F,
		       bq, BUF_P(&bwo->bbo_data));
	} m0_tl_endfor;
	bqop_tlist_fini(&bq->bq_op_get);
	for (i = 0; i < bq->bq_cfg.bqc_consumers_nr_max; ++i)
		bqop_tlink_del_fini(&bq->bq_ops_get[i]);
	bqop_tlist_fini(&bq->bq_op_get_unused);
	/* if there was nothing in bq_q then the following list is empty */
	bqop_tlist_fini(&bq->bq_op_put);
	for (i = 0; i < bq->bq_cfg.bqc_producers_nr_max; ++i)
		bqop_tlink_del_fini(&bq->bq_ops_put[i]);
	bqop_tlist_fini(&bq->bq_op_put_unused);
	for (i = 0; i < bq_queue_items_max(bq); ++i)
		bqq_tlink_del_fini(be_queue_qitem(bq, i));
	bqq_tlist_fini(&bq->bq_q_unused);
	m0_mutex_fini(&bq->bq_lock);
	m0_free(bq->bq_ops_put);
	m0_free(bq->bq_ops_get);
	m0_free(bq->bq_qitems);
	M0_LEAVE();
}

M0_INTERNAL void m0_be_queue_lock(struct m0_be_queue *bq)
{
	m0_mutex_lock(&bq->bq_lock);
}

M0_INTERNAL void m0_be_queue_unlock(struct m0_be_queue *bq)
{
	m0_mutex_unlock(&bq->bq_lock);
}

static uint64_t be_queue_items_nr(struct m0_be_queue *bq)
{
	M0_PRE(m0_mutex_is_locked(&bq->bq_lock));
	M0_ASSERT_INFO(bq->bq_enqueued >= bq->bq_dequeued,
	               "bq="BEQ_F, BEQ_P(bq));
	return bq->bq_enqueued - bq->bq_dequeued;
}

static bool be_queue_is_empty(struct m0_be_queue *bq)
{
	return be_queue_items_nr(bq) == 0;
}

static bool be_queue_is_full(struct m0_be_queue *bq)
{
	return be_queue_items_nr(bq) >= bq->bq_cfg.bqc_q_size_max;
}

static struct be_queue_item *be_queue_q_put(struct m0_be_queue  *bq,
                                            const struct m0_buf *data)
{
	struct be_queue_item *bqi;

	M0_PRE(m0_mutex_is_locked(&bq->bq_lock));

	bqi = bqq_tlist_head(&bq->bq_q_unused);
	m0_buf_memcpy(&BE_QUEUE_ITEM2BUF(bq, bqi), data);
	bqq_tlist_move_tail(&bq->bq_q, bqi);
	++bq->bq_enqueued;
	M0_LOG(M0_DEBUG, "bq="BEQ_F, BEQ_P(bq));
	return bqi;
}

static void be_queue_q_peek(struct m0_be_queue *bq, struct m0_buf *data)
{
	struct be_queue_item *bqi;

	M0_PRE(m0_mutex_is_locked(&bq->bq_lock));
	M0_PRE(!be_queue_is_empty(bq));

	bqi = bqq_tlist_head(&bq->bq_q);
	m0_buf_memcpy(data, &BE_QUEUE_ITEM2BUF(bq, bqi));
	M0_LOG(M0_DEBUG, "bq="BEQ_F, BEQ_P(bq));
}

static void be_queue_q_get(struct m0_be_queue *bq,
			   struct m0_buf      *data,
                           bool               *successful)
{
	struct be_queue_item *bqi;

	M0_PRE(m0_mutex_is_locked(&bq->bq_lock));
	M0_PRE(!be_queue_is_empty(bq));

	bqi = bqq_tlist_head(&bq->bq_q);
	m0_buf_memcpy(data, &BE_QUEUE_ITEM2BUF(bq, bqi));
	*successful = true;
	bqq_tlist_move(&bq->bq_q_unused, bqi);
	++bq->bq_dequeued;
	M0_LOG(M0_DEBUG, "bq="BEQ_F, BEQ_P(bq));
}

static void be_queue_op_put(struct m0_be_queue   *bq,
                            struct m0_be_op      *op,
                            struct be_queue_item *bqi)
{
	struct be_queue_wait_op *bwo;

	M0_PRE(m0_mutex_is_locked(&bq->bq_lock));
	M0_PRE(!bqop_tlist_is_empty(&bq->bq_op_put_unused));

	bwo = bqop_tlist_head(&bq->bq_op_put_unused);
	M0_ASSERT_INFO(bwo != NULL,
	               "Too many producers: bqc_producers_nr_max=%"PRIu64,
	               bq->bq_cfg.bqc_producers_nr_max);
	bwo->bbo_bqi = bqi;
	bwo->bbo_op  = op;
	bqop_tlist_move_tail(&bq->bq_op_put, bwo);
	M0_LOG(M0_DEBUG, "bq="BEQ_F, BEQ_P(bq));
}

static void be_queue_op_put_done(struct m0_be_queue *bq)
{
	struct be_queue_wait_op *bwo;

	M0_PRE(m0_mutex_is_locked(&bq->bq_lock));
	M0_PRE(!bqop_tlist_is_empty(&bq->bq_op_put));

	bwo = bqop_tlist_head(&bq->bq_op_put);
	m0_be_op_done(bwo->bbo_op);
	bqop_tlist_move(&bq->bq_op_put_unused, bwo);
	M0_LOG(M0_DEBUG, "bq="BEQ_F, BEQ_P(bq));
}

static bool be_queue_op_put_is_waiting(struct m0_be_queue *bq)
{
	return !bqop_tlist_is_empty(&bq->bq_op_put);
}

static void be_queue_op_get(struct m0_be_queue *bq,
                            struct m0_be_op    *op,
                            struct m0_buf      *data,
                            bool               *successful)
{
	struct be_queue_wait_op *bwo;

	M0_PRE(m0_mutex_is_locked(&bq->bq_lock));
	M0_PRE(!bqop_tlist_is_empty(&bq->bq_op_get_unused));

	bwo = bqop_tlist_head(&bq->bq_op_get_unused);
	M0_ASSERT_INFO(bwo != NULL,
	               "Too many consumers: bqc_consumers_nr_max=%"PRIu64,
	               bq->bq_cfg.bqc_consumers_nr_max);
	bwo->bbo_data       = *data;
	bwo->bbo_successful = successful;
	bwo->bbo_op         = op;
	bqop_tlist_move_tail(&bq->bq_op_get, bwo);
	M0_LOG(M0_DEBUG, "bq="BEQ_F, BEQ_P(bq));
}

static void be_queue_op_get_done(struct m0_be_queue *bq, bool success)
{
	struct be_queue_wait_op *bwo;

	M0_ENTRY("bq="BEQ_F" success=%d", BEQ_P(bq), !!success);
	M0_PRE(m0_mutex_is_locked(&bq->bq_lock));
	M0_PRE(!bqop_tlist_is_empty(&bq->bq_op_get));

	bwo = bqop_tlist_head(&bq->bq_op_get);
	if (success) {
		be_queue_q_get(bq, &bwo->bbo_data, bwo->bbo_successful);
	} else {
		*bwo->bbo_successful = false;
	}
	m0_be_op_done(bwo->bbo_op);
	bqop_tlist_move(&bq->bq_op_get_unused, bwo);
	M0_LEAVE("bq="BEQ_F, BEQ_P(bq));
}

static bool be_queue_op_get_is_waiting(struct m0_be_queue *bq)
{
	return !bqop_tlist_is_empty(&bq->bq_op_get);
}

M0_INTERNAL void m0_be_queue_put(struct m0_be_queue  *bq,
                                 struct m0_be_op     *op,
                                 const struct m0_buf *data)
{
	struct be_queue_item *bqi;
	bool                  was_full;

	M0_ENTRY("bq="BEQ_F, BEQ_P(bq));
	M0_PRE(m0_mutex_is_locked(&bq->bq_lock));
	M0_PRE(be_queue_invariant(bq));
	M0_PRE(!bq->bq_the_end);
	M0_PRE(data->b_nob == bq->bq_cfg.bqc_item_length);

	m0_be_op_active(op);
	was_full = be_queue_is_full(bq);
	bqi = be_queue_q_put(bq, data);
	if (was_full) {
		be_queue_op_put(bq, op, bqi);
	} else {
		m0_be_op_done(op);
	}
	/*
	 * Shortcut for this case hasn't been not done intentionally.
	 * It's much easier to look at the logs when all items are always added
	 * to the queue.
	 */
	if (be_queue_op_get_is_waiting(bq))
		be_queue_op_get_done(bq, true);

	M0_POST(be_queue_invariant(bq));
	M0_LEAVE("bq="BEQ_F, BEQ_P(bq));
}

M0_INTERNAL void m0_be_queue_end(struct m0_be_queue *bq)
{
	M0_ENTRY("bq="BEQ_F, BEQ_P(bq));
	M0_PRE(m0_mutex_is_locked(&bq->bq_lock));
	M0_PRE(be_queue_invariant(bq));
	M0_PRE(!bq->bq_the_end);
	M0_PRE(ergo(be_queue_op_get_is_waiting(bq), be_queue_is_empty(bq)));

	bq->bq_the_end = true;
	while (be_queue_op_get_is_waiting(bq))
		be_queue_op_get_done(bq, false);
	M0_POST(be_queue_invariant(bq));
	M0_LEAVE("bq="BEQ_F, BEQ_P(bq));
}

M0_INTERNAL void m0_be_queue_get(struct m0_be_queue *bq,
                                 struct m0_be_op    *op,
                                 struct m0_buf      *data,
                                 bool               *successful)
{
	M0_PRE(m0_mutex_is_locked(&bq->bq_lock));
	M0_PRE(be_queue_invariant(bq));
	M0_PRE(data->b_nob == bq->bq_cfg.bqc_item_length);

	M0_ENTRY("bq="BEQ_F, BEQ_P(bq));
	m0_be_op_active(op);
	if (be_queue_is_empty(bq) && bq->bq_the_end) {
		*successful = false;
		m0_be_op_done(op);
		M0_LEAVE();
		return;
	}
	if (be_queue_is_empty(bq) || be_queue_op_get_is_waiting(bq)) {
		be_queue_op_get(bq, op, data, successful);
		M0_LEAVE();
		return;
	}
	be_queue_q_get(bq, data, successful);
	m0_be_op_done(op);
	if (be_queue_op_put_is_waiting(bq))
		be_queue_op_put_done(bq);
	M0_POST(be_queue_invariant(bq));
	M0_LEAVE("bq="BEQ_F, BEQ_P(bq));
}

M0_INTERNAL bool m0_be_queue_peek(struct m0_be_queue *bq,
                                  struct m0_buf      *data)
{
	M0_PRE(m0_mutex_is_locked(&bq->bq_lock));
	M0_PRE(be_queue_invariant(bq));
	M0_PRE(data->b_nob == bq->bq_cfg.bqc_item_length);

	if (be_queue_is_empty(bq) ||
	    be_queue_op_get_is_waiting(bq)) {
		M0_LOG(M0_DEBUG, "bq=%p the queue is empty", bq);
		return false;
	}
	be_queue_q_peek(bq, data);
	M0_POST(be_queue_invariant(bq));
	M0_LEAVE("bq="BEQ_F, BEQ_P(bq));
	return true;
}

#undef BE_QUEUE_ITEM2BUF

#undef M0_TRACE_SUBSYSTEM

/** @} end of be group */

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
