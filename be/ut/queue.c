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

#include "lib/memory.h"         /* M0_ALLOC_PTR */
#include "lib/semaphore.h"      /* m0_semaphore */
#include "lib/atomic.h"         /* m0_atomic64 */
#include "lib/arith.h"          /* m0_rnd64 */
#include "lib/misc.h"           /* m0_reduce */
#include "lib/buf.h"            /* m0_buf_eq */
#include "lib/atomic.h"         /* m0_atomic64 */
#include "lib/time.h"           /* m0_time_now */

#include "ut/threads.h"         /* m0_ut_theads_start */
#include "ut/ut.h"              /* M0_UT_ASSERT */
#include "ut/misc.h"            /* m0_ut_random_shuffle */

#include "be/tx_credit.h"       /* M0_BE_TX_CREDIT */
#include "be/op.h"              /* M0_BE_OP_SYNC */


enum be_ut_queue_test {
	BE_UT_QUEUE_1_1_1,
	BE_UT_QUEUE_2_1_1,
	BE_UT_QUEUE_100_1_1,
	BE_UT_QUEUE_100_1_10,
	BE_UT_QUEUE_100_10_1,
	BE_UT_QUEUE_100_10_10,
	BE_UT_QUEUE_10_100_1,
	BE_UT_QUEUE_10_100_5,
	BE_UT_QUEUE_10_1_100,
	BE_UT_QUEUE_10_5_100,
	BE_UT_QUEUE_10_100_100,
	BE_UT_QUEUE_NR,
};

struct be_ut_queue_cfg {
	uint64_t butc_q_size_max;
	uint64_t butc_producers;
	uint64_t butc_consumers;
	uint64_t butc_items_nr;
	uint64_t butc_seed;
};

struct be_ut_queue_result {
	uint64_t butr_put_before;
	uint64_t butr_put_after;
	uint64_t butr_get_before;
	uint64_t butr_get_after;
	bool     butr_checked;
};

struct be_ut_queue_data {
	void                   *buqd_user;
	struct m0_be_tx_credit  buqd_credit;
	m0_bcount_t             buqd_payload_size;
};

struct be_ut_queue_ctx {
	struct be_ut_queue_cfg    *butx_cfg;
	struct m0_be_queue        *butx_bq;
	/* producer increments and takes butx_data[] with the index returned */
	struct m0_atomic64         butx_pos;
	/* logical clock to check queue operations orderding */
	struct m0_atomic64         butx_clock;
	struct be_ut_queue_data   *butx_data;
	struct be_ut_queue_result *butx_result;
};

struct be_ut_queue_thread_param {
	struct be_ut_queue_ctx *butqp_ctx;
	/*
	 * Start barrier to launch all threads as close to each other as
	 * possible.
	 */
	struct m0_semaphore     butqp_sem_start;
	bool                    butqp_is_producer;
	/*
	 * Thread index, starts from 0 for producers and starts from 0 for
	 * consumers.
	 */
	uint64_t                butqp_index;
	/* Number of items to put/get to/from the queue. */
	uint64_t                butqp_items_nr;
	/* just for debugging purposes */
	uint64_t                butqp_peeks_successful;
	uint64_t                butqp_peeks_unsuccessful;
	uint64_t                butqp_gets_successful;
	uint64_t                butqp_gets_unsuccessful;
};

#define BE_UT_QUEUE_TEST(q_size_max, producers, consumers, items_nr)    \
{                                                                       \
	.butc_q_size_max = (q_size_max),                                \
	.butc_producers  = (producers),                                 \
	.butc_consumers  = (consumers),                                 \
	.butc_items_nr   = (items_nr)                                   \
}

static struct be_ut_queue_cfg be_ut_queue_tests_cfg[BE_UT_QUEUE_NR] = {
	[BE_UT_QUEUE_1_1_1]      = BE_UT_QUEUE_TEST(  1,   1,   1,     1),
	[BE_UT_QUEUE_2_1_1]      = BE_UT_QUEUE_TEST(  2,   1,   1, 10000),
	[BE_UT_QUEUE_100_1_1]    = BE_UT_QUEUE_TEST(100,   1,   1, 10000),
	[BE_UT_QUEUE_100_1_10]   = BE_UT_QUEUE_TEST(100,   1,  10, 10000),
	[BE_UT_QUEUE_100_10_1]   = BE_UT_QUEUE_TEST(100,  10,   1, 10000),
	[BE_UT_QUEUE_100_10_10]  = BE_UT_QUEUE_TEST(100,  10,  10, 10000),
	[BE_UT_QUEUE_10_100_1]   = BE_UT_QUEUE_TEST( 10, 100,   1, 10000),
	[BE_UT_QUEUE_10_100_5]   = BE_UT_QUEUE_TEST( 10, 100,   5, 10000),
	[BE_UT_QUEUE_10_1_100]   = BE_UT_QUEUE_TEST( 10,   1, 100, 10000),
	[BE_UT_QUEUE_10_5_100]   = BE_UT_QUEUE_TEST( 10,   5, 100, 10000),
	[BE_UT_QUEUE_10_100_100] = BE_UT_QUEUE_TEST( 10, 100, 100, 10000),
};

#undef BE_UT_QUEUE_TEST

static uint64_t be_ut_queue_data_index(struct be_ut_queue_ctx  *ctx,
                                       struct be_ut_queue_data *data)
{
	return (struct be_ut_queue_data *)data->buqd_user - ctx->butx_data;
}

static void be_ut_queue_try_peek(struct be_ut_queue_thread_param *param,
                                 struct be_ut_queue_ctx          *ctx)
{
	struct be_ut_queue_data data;
	struct m0_buf         buf;
	bool                  result;

	result = M0_BE_QUEUE_PEEK(ctx->butx_bq, &data);
	if (result) {
		++param->butqp_peeks_successful;
		buf = M0_BUF_INIT_PTR(&ctx->butx_data[
		                      be_ut_queue_data_index(ctx, &data)]);
		M0_UT_ASSERT(m0_buf_eq(&M0_BUF_INIT_PTR(&data), &buf));
	} else {
		++param->butqp_peeks_unsuccessful;
	}
}

static void be_ut_queue_thread(void *_param)
{
	struct be_ut_queue_data          data;
	struct be_ut_queue_thread_param *param = _param;
	struct be_ut_queue_ctx          *ctx = param->butqp_ctx;
	struct m0_be_queue              *bq = ctx->butx_bq;
	struct m0_be_op                 *op;
	uint64_t                         i;
	uint64_t                         index;
	uint64_t                         before;
	bool                             successful;

	M0_ALLOC_PTR(op);
	M0_UT_ASSERT(op != NULL);
	m0_be_op_init(op);
	m0_semaphore_down(&param->butqp_sem_start);
	for (i = 0; i < param->butqp_items_nr; ++i) {
		m0_be_op_reset(op);
		if (param->butqp_is_producer) {
			before = m0_atomic64_add_return(&ctx->butx_clock, 1);
			m0_be_queue_lock(bq);
			index = m0_atomic64_add_return(&ctx->butx_pos, 1) - 1;
			be_ut_queue_try_peek(param, ctx);
			M0_BE_QUEUE_PUT(bq, op, &ctx->butx_data[index]);
			m0_be_queue_unlock(bq);
			m0_be_op_wait(op);
			ctx->butx_result[index].butr_put_before = before;
			ctx->butx_result[index].butr_put_after =
				m0_atomic64_add_return(&ctx->butx_clock, 1);
			if (index == ctx->butx_cfg->butc_items_nr - 1) {
				m0_be_queue_lock(bq);
				m0_be_queue_end(bq);
				m0_be_queue_unlock(bq);
			}
		} else {
			M0_SET0(&data);
			successful = param->butqp_index % 2 == 0;
			before = m0_atomic64_add_return(&ctx->butx_clock, 1);
			m0_be_queue_lock(bq);
			M0_BE_QUEUE_GET(bq, op, &data, &successful);
			be_ut_queue_try_peek(param, ctx);
			m0_be_queue_unlock(bq);
			m0_be_op_wait(op);
			if (!successful) {
				++param->butqp_gets_unsuccessful;
				continue;
			}
			M0_ASSERT(param->butqp_gets_unsuccessful == 0);
			++param->butqp_gets_successful;
			index = be_ut_queue_data_index(ctx, &data);
			M0_UT_ASSERT(!ctx->butx_result[index].butr_checked);
			ctx->butx_result[index].butr_checked = true;
			ctx->butx_result[index].butr_get_before = before;
			ctx->butx_result[index].butr_get_after =
				m0_atomic64_add_return(&ctx->butx_clock, 1);
		}
	}
	m0_be_op_fini(op);
	m0_free(op);
}

/**
 * The test launches be_ut_queue_cfg::butc_producers +
 * be_ut_queue_cfg::butc_consumers threads for producers and consumers
 * respectively. Each producer/consumer thread does
 * M0_BE_QUEUE_PUT()/M0_BE_QUEUE_GET() in a loop, and it also tries to
 * M0_BE_QUEUE_PEEK() before each put()/get().
 */
static void be_ut_queue_with_cfg(struct be_ut_queue_cfg *test_cfg)
{
	struct m0_ut_threads_descr      *td;
	struct be_ut_queue_thread_param *params;
	struct m0_be_queue_cfg           bq_cfg = {
		.bqc_q_size_max       = test_cfg->butc_q_size_max,
		.bqc_producers_nr_max = test_cfg->butc_producers,
		.bqc_consumers_nr_max = test_cfg->butc_consumers,
		.bqc_item_length      = sizeof(struct be_ut_queue_data),
	};
	struct be_ut_queue_ctx          *ctx;
	struct m0_be_queue              *bq;
	uint64_t                         threads_nr;
	uint64_t                         items_nr = test_cfg->butc_items_nr;
	uint64_t                         i;
	uint64_t                         seed = test_cfg->butc_seed;
	uint64_t                         divisor;
	uint64_t                         remainder;
	struct be_ut_queue_result       *r;
	int                              rc;

	M0_ALLOC_PTR(bq);
	M0_ASSERT(bq != NULL);
	rc = m0_be_queue_init(bq, &bq_cfg);
	M0_ASSERT_INFO(rc == 0, "rc=%d", rc);
	M0_ALLOC_PTR(ctx);
	M0_UT_ASSERT(ctx != NULL);
	ctx->butx_cfg = test_cfg;
	ctx->butx_bq = bq;
	m0_atomic64_set(&ctx->butx_pos, 0);
	m0_atomic64_set(&ctx->butx_clock, 0);
	M0_ALLOC_ARR(ctx->butx_data, items_nr);
	M0_UT_ASSERT(ctx->butx_data != NULL);
	M0_ALLOC_ARR(ctx->butx_result, items_nr);
	M0_UT_ASSERT(ctx->butx_result != NULL);
	for (i = 0; i < items_nr; ++i) {
		ctx->butx_data[i] = (struct be_ut_queue_data){
			.buqd_user = &ctx->butx_data[i],
			.buqd_credit =
				M0_BE_TX_CREDIT(m0_rnd64(&seed) % 0x100 + 1,
				                m0_rnd64(&seed) % 0x100 + 1),
			.buqd_payload_size = m0_rnd64(&seed) % 0x1000 + 1,
		};
	}
	threads_nr = test_cfg->butc_producers + test_cfg->butc_consumers;
	M0_ALLOC_ARR(params, threads_nr);
	for (i = 0; i < threads_nr; ++i) {
		params[i].butqp_ctx = ctx;
		m0_semaphore_init(&params[i].butqp_sem_start, 0);
		params[i].butqp_is_producer = i < test_cfg->butc_producers;
		if (params[i].butqp_is_producer) {
			params[i].butqp_index = i;
			divisor = test_cfg->butc_producers;
		} else {
			params[i].butqp_index = i - test_cfg->butc_producers;
			divisor = test_cfg->butc_consumers;
		}
		remainder = items_nr % divisor;
		params[i].butqp_items_nr = items_nr / divisor +
			(remainder == 0 ?
			 0 : params[i].butqp_index < remainder) +
			!params[i].butqp_is_producer;
	}
	/* all producer threads would put items_nr items to the queue */
	M0_UT_ASSERT(m0_reduce(j, test_cfg->butc_producers,
			       0, + params[j].butqp_items_nr) == items_nr);
	/*
	 * All consumer threads would try to get items_nr + butc_consumers items
	 * from the queue. Only items_nr M0_BE_QUEUE_GET()s would be
	 * successful, the rest would be unsuccessful.
	 */
	M0_UT_ASSERT(m0_reduce(j, test_cfg->butc_consumers,
			       0, + params[test_cfg->butc_producers +
					   j].butqp_items_nr) ==
		     items_nr + test_cfg->butc_consumers);

	M0_ALLOC_PTR(td);
	M0_UT_ASSERT(td != NULL);
	td->utd_thread_func = &be_ut_queue_thread;
	m0_ut_threads_start(td, threads_nr, params, sizeof(params[0]));
	for (i = 0; i < threads_nr; ++i)
		m0_semaphore_up(&params[i].butqp_sem_start);
	/* work is done sometime around here */
	m0_ut_threads_stop(td);
	m0_free(td);

	for (i = 0; i < threads_nr; ++i)
		m0_semaphore_fini(&params[i].butqp_sem_start);

	r = ctx->butx_result;
	/* M0_BE_QUEUE_GET() has been called successfully for items_nr items */
	M0_UT_ASSERT(m0_reduce(j, test_cfg->butc_consumers,
			       0, + params[test_cfg->butc_producers +
					   j].butqp_gets_successful) ==
		     items_nr);
	/*
	 * There has been exactly butc_consumers unsuccessful M0_BE_QUEUE_GET()
	 * calls.
	 */
	M0_UT_ASSERT(m0_reduce(j, test_cfg->butc_consumers,
			       0, + params[test_cfg->butc_producers +
					   j].butqp_gets_unsuccessful) ==
		     test_cfg->butc_consumers);
	/* that each item is returned by m0_be_queue_get() exactly once */
	M0_UT_ASSERT(m0_forall(j, items_nr, r[j].butr_checked));
	/* at least one m0_be_queue_peek() is supposed to fail in each thread */
	M0_UT_ASSERT(m0_exists(j,
			       test_cfg->butc_producers +
			       test_cfg->butc_consumers,
			       params[j].butqp_peeks_unsuccessful > 0));
	/* happened-before relations */
	M0_UT_ASSERT(m0_forall(j, items_nr,
			       r[j].butr_put_before < r[j].butr_get_after));
	M0_UT_ASSERT(m0_forall(j, items_nr - 1,
			       r[j].butr_put_before < r[j + 1].butr_put_after));
	M0_UT_ASSERT(m0_forall(j, items_nr - 1,
			       r[j].butr_get_before < r[j + 1].butr_get_after));

	m0_free(params);
	m0_free(ctx->butx_result);
	m0_free(ctx->butx_data);
	m0_free(ctx);

	m0_be_queue_fini(bq);
	m0_free(bq);
}

static void be_ut_queue(enum be_ut_queue_test test)
{
	be_ut_queue_tests_cfg[test].butc_seed = test;
	be_ut_queue_with_cfg(&be_ut_queue_tests_cfg[test]);
}

void m0_be_ut_queue_1_1_1(void)      { be_ut_queue(BE_UT_QUEUE_1_1_1);      }
void m0_be_ut_queue_2_1_1(void)      { be_ut_queue(BE_UT_QUEUE_2_1_1);      }
void m0_be_ut_queue_100_1_1(void)    { be_ut_queue(BE_UT_QUEUE_100_1_1);    }
void m0_be_ut_queue_100_1_10(void)   { be_ut_queue(BE_UT_QUEUE_100_1_10);   }
void m0_be_ut_queue_100_10_1(void)   { be_ut_queue(BE_UT_QUEUE_100_10_1);   }
void m0_be_ut_queue_100_10_10(void)  { be_ut_queue(BE_UT_QUEUE_100_10_10);  }
void m0_be_ut_queue_10_100_1(void)   { be_ut_queue(BE_UT_QUEUE_10_100_1);   }
void m0_be_ut_queue_10_100_5(void)   { be_ut_queue(BE_UT_QUEUE_10_100_5);   }
void m0_be_ut_queue_10_1_100(void)   { be_ut_queue(BE_UT_QUEUE_10_1_100);   }
void m0_be_ut_queue_10_5_100(void)   { be_ut_queue(BE_UT_QUEUE_10_5_100);   }
void m0_be_ut_queue_10_100_100(void) { be_ut_queue(BE_UT_QUEUE_10_100_100); }

void m0_be_ut_queue_from_1_to_10(void)
{
	const int             MAX = 10;
	struct be_ut_queue_cfg *test_cfg;
	int                   i;
	int                   j;
	int                   k;

	M0_ALLOC_PTR(test_cfg);
	for (i = 1; i <= MAX; ++i)
		for (j = 1; j <= MAX; ++j)
			for (k = 1; k <= MAX; ++k) {
				*test_cfg = (struct be_ut_queue_cfg){
					.butc_q_size_max = i,
					.butc_producers  = j,
					.butc_consumers  = k,
					.butc_items_nr   = 100,
					.butc_seed       = i * 100 + j * 10 + k,
				};
				be_ut_queue_with_cfg(test_cfg);
			}
	m0_free(test_cfg);
}


enum {
	BE_UT_QUEUE_LAYERS_MAX = 0x10,
};


enum be_ut_queue_layers_type {
	BE_UT_QUEUE_LAYERS_TYPE_AND,
	BE_UT_QUEUE_LAYERS_TYPE_OR,
	BE_UT_QUEUE_LAYERS_TYPE_SYNC,
	BE_UT_QUEUE_LAYERS_TYPE_REC,
};

struct be_ut_queue_layers_cfg {
	uint64_t buqlc_recs_nr;
	uint64_t buqlc_layers_nr;
	struct {
		uint64_t                     buqlct_threads_nr;
		uint64_t                     buqlct_get_queues_per_thread;
		uint64_t                     buqlct_put_queues_per_thread;
		uint64_t                     buqlct_iter_nr;
		enum be_ut_queue_layers_type buqlct_type_get;
		enum be_ut_queue_layers_type buqlct_type_put;
	} buqlc_threads[BE_UT_QUEUE_LAYERS_MAX];
	struct {
		uint64_t buqlcq_queues_nr;
		uint64_t buqlcq_q_size_max;
	} buqlc_queues[BE_UT_QUEUE_LAYERS_MAX - 1];
};

struct be_ut_queue_layers_rec {
	uint64_t                       buqlr_value;
	m0_time_t                      buqlr_consumed;
	uint64_t                       buqlr_layers;
};

/** 0 <= consumed <= current <= nr_max */
struct be_ut_queue_layers_qcredit {
	struct m0_mutex    buqlqc_lock;
	struct m0_atomic64 buqlqc_consumed;
	uint64_t           buqlqc_current;
	uint64_t           buqlqc_nr_max;
};

struct be_ut_queue_layers_queue {
	struct m0_be_queue                buqlq_queue;
	struct be_ut_queue_layers_qcredit buqlq_credit_get;
	struct be_ut_queue_layers_qcredit buqlq_credit_put;
};

struct be_ut_queue_layers_thread {
	uint64_t                          buqlt_index;
	uint64_t                          buqlt_rng_seed;
	struct m0_thread                  buqlt_thread;
	struct m0_be_op                   buqlt_op_start;
	struct m0_be_op                   buqlt_op_finish;
	enum be_ut_queue_layers_type      buqlt_type_get;
	enum be_ut_queue_layers_type      buqlt_type_put;
	uint64_t                          buqlt_get_nr;
	uint64_t                          buqlt_put_nr;
	uint64_t                          buqlt_iter_nr;
	struct be_ut_queue_layers_rec    *buqlt_recs;
	struct m0_atomic64               *buqlt_recs_pos;
	uint64_t                          buqlt_recs_nr;
	uint64_t                          buqlt_value;
	struct be_ut_queue_layers_queue **buqlt_q_get;
	struct be_ut_queue_layers_queue **buqlt_q_put;
	struct be_ut_queue_layers_queue **buqlt_q_put_all;
	uint64_t                          buqlt_q_put_all_nr;
};

struct be_ut_queue_layers_qitem {
	uint64_t buqli_pos;
};

static void
be_ut_queue_layers_qcredits_initfini(struct be_ut_queue_layers_queue *q,
                                     uint64_t                         nr,
                                     uint64_t                         rec_nr,
                                     uint64_t                        *seed,
                                     bool                             is_init)
{
	struct be_ut_queue_layers_qcredit **qc;
	uint64_t                           *random_rec_nr;
	uint64_t                            even;
	int                                 i;
	int                                 k;

	M0_ALLOC_ARR(qc, nr);
	M0_UT_ASSERT(qc != NULL);
	M0_ALLOC_ARR(random_rec_nr, nr);
	M0_UT_ASSERT(random_rec_nr != NULL);
	for (k = 0; k < 2; ++k) {
		for (i = 0; i < nr; ++i) {
			qc[i] = k == 0 ? &q[i].buqlq_credit_get :
					 &q[i].buqlq_credit_put;
		}
		for (i = 0; i < nr; ++i) {
			if (is_init) {
				m0_mutex_init(&qc[i]->buqlqc_lock);
				m0_atomic64_set(&qc[i]->buqlqc_consumed, 0);
				qc[i]->buqlqc_current  = 0;
			} else {
				M0_UT_ASSERT(m0_atomic64_get(
				                &qc[i]->buqlqc_consumed) ==
				             qc[i]->buqlqc_nr_max);
				M0_UT_ASSERT(qc[i]->buqlqc_current ==
				             qc[i]->buqlqc_nr_max);
				m0_mutex_fini(&qc[i]->buqlqc_lock);
			}
		}
		if (!is_init)
			continue;
		if (k == 1) {
			for (i = 0; i < nr; ++i) {
				q[i].buqlq_credit_put.buqlqc_nr_max =
					q[i].buqlq_credit_get.buqlqc_nr_max;
			}
			break;
		}
		even = rec_nr * 3 / 4 / nr;
		M0_UT_ASSERT(even * nr < rec_nr);
		m0_ut_random_arr_with_sum(random_rec_nr, nr, rec_nr - even * nr,
		                          seed);
		for (i = 0; i < nr; ++i)
			qc[i]->buqlqc_nr_max = even + random_rec_nr[i];
		M0_UT_ASSERT(m0_reduce(j, nr, 0, + qc[j]->buqlqc_nr_max) ==
			     rec_nr);
	}
	m0_free(random_rec_nr);
	m0_free(qc);
}

static void
be_ut_queue_layers_qcredits_init(struct be_ut_queue_layers_queue *q,
                                 uint64_t                         nr,
                                 uint64_t                         rec_nr,
                                 uint64_t                        *seed)
{
	be_ut_queue_layers_qcredits_initfini(q, nr, rec_nr, seed, true);
}

static void
be_ut_queue_layers_qcredits_fini(struct be_ut_queue_layers_queue *q,
                                 uint64_t                         nr,
                                 uint64_t                         rec_nr)
{
	be_ut_queue_layers_qcredits_initfini(q, nr, rec_nr, NULL, false);
}

static bool
be_ut_queue_layers_qcredit_invariant(struct be_ut_queue_layers_qcredit *qc)
{
	return m0_atomic64_get(&qc->buqlqc_consumed) <= qc->buqlqc_current &&
	       qc->buqlqc_current <= qc->buqlqc_nr_max;
}

static int be_ut_queue_layers_borrow(struct be_ut_queue_layers_queue  **qin,
                                     uint64_t                           nr,
                                     bool                               is_get,
                                     struct be_ut_queue_layers_queue ***out)
{
	struct be_ut_queue_layers_qcredit  *qc;
	struct be_ut_queue_layers_queue    *q;
	struct be_ut_queue_layers_queue   **qout;
	bool                                the_end = true;
	int                                 i;
	int                                 got;

	M0_ALLOC_ARR(qout, nr);
	M0_UT_ASSERT(qout != NULL);
	got = 0;
	for (i = 0; i < nr; ++i) {
		q = qin[i];
		qc = is_get ? &q->buqlq_credit_get : &q->buqlq_credit_put;

		m0_mutex_lock(&qc->buqlqc_lock);
		M0_UT_ASSERT(be_ut_queue_layers_qcredit_invariant(qc));
		if (qc->buqlqc_current < qc->buqlqc_nr_max) {
			++qc->buqlqc_current;
			qout[got++] = q;
		}
		if (m0_atomic64_get(&qc->buqlqc_consumed) < qc->buqlqc_nr_max)
			the_end = false;
		m0_mutex_unlock(&qc->buqlqc_lock);
	}
	M0_UT_ASSERT(ergo(the_end, got == 0));
	if (got > 0) {
		*out = qout;
	} else {
		m0_free(qout);
	}
	/* TODO m0_be_queue_end() at the end */
	return the_end ? -1 : got;
}

static void be_ut_queue_layers_return(struct be_ut_queue_layers_queue **qin,
                                      bool                              is_get,
                                      uint64_t                          nr)
{
	struct be_ut_queue_layers_qcredit  *qc;
	struct be_ut_queue_layers_queue    *q;
	int                                 i;

	for (i = 0; i < nr; ++i) {
		q = qin[i];
		qc = is_get ? &q->buqlq_credit_get : &q->buqlq_credit_put;
		m0_atomic64_inc(&qc->buqlqc_consumed);
	}
	m0_free(qin);
}

static int be_ut_queue_layers_get_rec(struct be_ut_queue_layers_thread *t,
                                      uint64_t                         *pos)
{
	struct be_ut_queue_layers_rec *rec;
	bool                           end_of_recs = false;
	int                            nr;
	int                            i;

	for (nr = 0; nr < t->buqlt_get_nr; ++nr) {
		pos[nr] = m0_atomic64_add_return( t->buqlt_recs_pos, 1) - 1;
		if (pos[nr] >= t->buqlt_recs_nr) {
			m0_atomic64_dec(t->buqlt_recs_pos);
			--nr;
			end_of_recs = true;
			break;
		}
	}
	if (nr == -1)
		return -1;
	if (end_of_recs)
		++nr;
	for (i = 0; i < nr; ++i) {
		rec = &t->buqlt_recs[pos[i]];
		M0_UT_ASSERT(rec->buqlr_consumed == 0);
		rec->buqlr_consumed = m0_time_now();
		rec->buqlr_value = m0_rnd64(&t->buqlt_rng_seed);
	}
	return nr;
}

static void be_ut_queue_layers_put_rec(struct be_ut_queue_layers_thread *t,
                                       uint64_t                         *pos,
                                       uint64_t                          nr)
{
	struct be_ut_queue_layers_rec *rec;
	int                            i;

	for (i = 0; i < nr; ++i) {
		rec = &t->buqlt_recs[pos[i]];
		M0_UT_ASSERT(rec->buqlr_consumed != 0);
		M0_UT_ASSERT(rec->buqlr_consumed <= m0_time_now());
	}
}

// XXX static
void be_ut_queue_layers_qput(struct be_ut_queue_layers_queue *q,
                                    struct m0_be_op                 *op,
                                    struct be_ut_queue_layers_qitem *qi)
{
}

static void be_ut_queue_layers_qget(struct be_ut_queue_layers_queue *q,
                                    struct m0_be_op                 *op,
                                    struct be_ut_queue_layers_qitem *qi,
                                    bool                            *successful)
{
}

static int be_ut_queue_layers_get_and(struct be_ut_queue_layers_thread *t,
                                      uint64_t                         *pos)
{
	return 0;
}

static void be_ut_queue_layers_put_and(struct be_ut_queue_layers_thread *t,
                                       uint64_t                         *pos,
                                       uint64_t                          nr)
{
}

static int be_ut_queue_layers_get_or(struct be_ut_queue_layers_thread *t,
                                     uint64_t                         *pos)
{
	struct be_ut_queue_layers_queue **q;
	struct be_ut_queue_layers_qitem  *qi;
	struct m0_be_op                  *ops;
	struct m0_be_op                  *op;
	int                               nr_total;
	int                               nr_max = t->buqlt_get_nr;
	bool                              successful[nr_max];
	int                               nr;
	int                               i;

	M0_ALLOC_ARR(ops, nr_max);
	M0_UT_ASSERT(ops != NULL);
	M0_ALLOC_PTR(op);
	M0_UT_ASSERT(op != NULL);
	M0_ALLOC_ARR(qi, nr_max);
	M0_UT_ASSERT(qi != NULL);
	for (i = 0; i < nr_max; ++i)
		m0_be_op_init(&ops[i]);
	m0_be_op_init(op);
	nr_total = 0;
	/* XXX doesn't work */
	while (nr_total < nr_max) {
		nr = be_ut_queue_layers_borrow(t->buqlt_q_get,
					       nr_max - nr_total, true, &q);
		if (M0_IN(nr, (0, -1))) {
			if (nr_total == 0)
				nr_total = nr;
			break;
		}
		m0_be_op_reset(op);
		m0_be_op_make_set_or(op);
		for (i = 0; i < nr; ++i)
			m0_be_op_reset(&ops[i]);
		for (i = 0; i < nr; ++i) {
			be_ut_queue_layers_qget(q[i], &op[i], &qi[i],
			                        &successful[i]);
			m0_be_op_set_add(op, &ops[i]);
		}
		m0_be_op_set_add_finish(op);
		be_ut_queue_layers_return(q, true, nr);
		nr_total += nr;
	}
	m0_be_op_fini(op);
	for (i = 0; i < nr_max; ++i)
		m0_be_op_fini(&ops[i]);
	m0_free(qi);
	m0_free(op);
	m0_free(ops);
	return nr_total;
}

static void be_ut_queue_layers_put_or(struct be_ut_queue_layers_thread *t,
                                      uint64_t                         *pos,
                                      uint64_t                          nr)
{
}

static int be_ut_queue_layers_get_sync(struct be_ut_queue_layers_thread *t,
                                       uint64_t                         *pos)
{
	struct be_ut_queue_layers_queue **q;
	struct be_ut_queue_layers_qitem  *qi;
	struct m0_be_queue               *bq;
	struct m0_be_op                  *op;
	bool                              successful;
	int                               nr;
	int                               i;

	nr = be_ut_queue_layers_borrow(t->buqlt_q_get,
				       t->buqlt_get_nr, true, &q);
	if (M0_IN(nr, (0, -1)))
		return nr;
	M0_ALLOC_PTR(op);
	M0_UT_ASSERT(op != NULL);
	m0_be_op_init(op);
	M0_ALLOC_PTR(qi);
	M0_UT_ASSERT(qi != NULL);
	for (i = 0; i < t->buqlt_get_nr; ++i) {
		bq = &q[i]->buqlq_queue;
		m0_be_queue_lock(bq);
		M0_BE_QUEUE_GET(bq, op, qi, &successful);
		m0_be_queue_unlock(bq);
		m0_be_op_wait(op);
		M0_UT_ASSERT(successful);
		pos[i] = qi->buqli_pos;
		m0_be_op_reset(op);
	}
	m0_free(qi);
	m0_be_op_fini(op);
	m0_free(op);
	be_ut_queue_layers_return(q, true, nr);
	return nr;
}

static void
be_ut_queue_layers_put_sync(struct be_ut_queue_layers_thread *t,
                            uint64_t                         *pos,
                            uint64_t                          nr_to_put)
{
	struct be_ut_queue_layers_queue **q;
	struct be_ut_queue_layers_qitem  *qi;
	struct m0_be_queue               *bq;
	struct m0_be_op                  *op;
	int                               nr;
	int                               nr_done = 0;
	int                               i;

	M0_ALLOC_PTR(op);
	M0_UT_ASSERT(op != NULL);
	m0_be_op_init(op);
	M0_ALLOC_PTR(qi);
	M0_UT_ASSERT(qi != NULL);
	while (nr_done < nr_to_put) {
		nr = be_ut_queue_layers_borrow(t->buqlt_q_put,
					       nr_to_put - nr_done, false, &q);
		if (M0_IN(nr, (0, -1))) {
			nr = be_ut_queue_layers_borrow(t->buqlt_q_put_all,
			                               nr_to_put - nr_done,
						       false, &q);
			M0_UT_ASSERT(nr == nr_to_put - nr_done);
		}
		for (i = 0; i < nr; ++i) {
			qi->buqli_pos = *pos;
			++pos;
			bq = &q[i]->buqlq_queue;
			m0_be_queue_lock(bq);
			M0_BE_QUEUE_PUT(bq, op, qi);
			m0_be_queue_unlock(bq);
			m0_be_op_wait(op);
			m0_be_op_reset(op);
		}
		be_ut_queue_layers_return(q, false, nr);
		nr_done += nr;
	}
	M0_UT_ASSERT(nr_done == nr_to_put);
	m0_free(qi);
	m0_be_op_fini(op);
	m0_free(op);
}

static int (*be_ut_queue_layers_ops_get[])
(struct be_ut_queue_layers_thread *t,
 uint64_t                         *pos) = {
	[BE_UT_QUEUE_LAYERS_TYPE_AND]  = &be_ut_queue_layers_get_and,
	[BE_UT_QUEUE_LAYERS_TYPE_OR]   = &be_ut_queue_layers_get_or,
	[BE_UT_QUEUE_LAYERS_TYPE_SYNC] = &be_ut_queue_layers_get_sync,
	[BE_UT_QUEUE_LAYERS_TYPE_REC]  = &be_ut_queue_layers_get_rec,
};

static void (*be_ut_queue_layers_ops_put[])
(struct be_ut_queue_layers_thread *t,
 uint64_t                         *pos,
 uint64_t                          nr) = {
	[BE_UT_QUEUE_LAYERS_TYPE_AND]  = &be_ut_queue_layers_put_and,
	[BE_UT_QUEUE_LAYERS_TYPE_OR]   = &be_ut_queue_layers_put_or,
	[BE_UT_QUEUE_LAYERS_TYPE_SYNC] = &be_ut_queue_layers_put_sync,
	[BE_UT_QUEUE_LAYERS_TYPE_REC]  = &be_ut_queue_layers_put_rec,
};

static void be_ut_queue_layers_thread_func(struct be_ut_queue_layers_thread *t)
{
	struct be_ut_queue_layers_queue *queues_get;
	struct be_ut_queue_layers_queue *queues_put;
	struct be_ut_queue_layers_rec   *rec;
	uint64_t                        *pos;
	// void                            *state_get = NULL;
	// void                            *state_put = NULL;
	bool                            *queues_remove;
	bool                             end_loop;
	int                              nr;
	int                              i;

	M0_ALLOC_ARR(pos, t->buqlt_iter_nr);
	M0_UT_ASSERT(pos != NULL);
	M0_ALLOC_ARR(queues_remove, t->buqlt_iter_nr);
	M0_UT_ASSERT(queues_remove != NULL);
	M0_ALLOC_ARR(queues_get, t->buqlt_iter_nr);
	M0_UT_ASSERT(queues_get != NULL);
	M0_ALLOC_ARR(queues_put, t->buqlt_iter_nr);
	M0_UT_ASSERT(queues_put != NULL);

	m0_be_op_wait(&t->buqlt_op_start);
	end_loop = false;
	while (!end_loop) {
		nr = be_ut_queue_layers_ops_get[t->buqlt_type_get](t, pos);
		if (nr == -1)
			break;
		if (nr == 0) {
			m0_nanosleep(M0_TIME_ONE_MSEC, NULL);
			continue;
		}
		for (i = 0; i < nr; ++i) {
			rec = &t->buqlt_recs[pos[i]];
			M0_UT_ASSERT((rec->buqlr_layers &
				      1 << t->buqlt_index) == 0);
			rec->buqlr_layers |= 1 << t->buqlt_index;
			t->buqlt_value += rec->buqlr_value;
		}
		/* TODO put to other queues if couldn't put to the assigned */
		be_ut_queue_layers_ops_put[t->buqlt_type_put](t, pos, nr);
	}
	m0_free(queues_put);
	m0_free(queues_get);
	m0_free(queues_remove);
	m0_free(pos);
	m0_be_op_active(&t->buqlt_op_finish);
	m0_be_op_done(&t->buqlt_op_finish);
}

static void be_ut_queue_layers_assign_queues_to_threads(
		struct be_ut_queue_layers_thread *t,
		int                               t_nr,
		struct be_ut_queue_layers_queue  *q,
		int                               q_nr,
		int                               q_per_t,
		int                               k,
		uint64_t                         *seed)
{
	struct be_ut_queue_layers_queue ***tq;
	uint64_t                          *qindex;
	bool                               found;
	int                               *nr;
	int                                i;
	int                                j;
	int                                n;

	M0_ALLOC_ARR(nr, t_nr);
	M0_UT_ASSERT(nr != NULL);
	M0_ALLOC_ARR(tq, t_nr);
	M0_UT_ASSERT(tq != NULL);
	for (i = 0; i < t_nr; ++i) {
		if (k == 0) {
			tq[i] = t[i].buqlt_q_get;
		} else {
			tq[i] = t[i].buqlt_q_put;
		}
	}
	for (i = 0; i < q_nr; ++i) {
		j = i % t_nr;
		tq[j][nr[j]++] = &q[i];
		M0_UT_ASSERT(nr[j] <= q_per_t);
	}
	M0_ALLOC_ARR(qindex, q_nr);
	for (i = 0; i < t_nr; ++i) {
		for (j = 0; j < q_nr; ++j)
			qindex[j] = j;
		m0_ut_random_shuffle(qindex, q_nr, seed);
		j = -1;
		for (; nr[i] < q_per_t; ++nr[i]) {
			do {
				found = false;
				++j;
				M0_UT_ASSERT(j < q_nr);
				for (n = 0; n < nr[i]; ++n) {
					if (tq[i][n] == &q[qindex[j]]) {
						found = true;
						break;
					}
				}
			} while (found);
			tq[i][nr[i]] = &q[qindex[j]];
		}
	}
	m0_free(qindex);
	for (i = 0; i < t_nr; ++i) {
		for (j = 0; j < t->buqlt_q_put_all_nr; ++j)
			t[i].buqlt_q_put_all[j] = &q[j];
	}
	m0_free(tq);
	m0_free(nr);
}

static void be_ut_queue_layers(struct be_ut_queue_layers_cfg *cfg)
{
	struct be_ut_queue_layers_thread **threads;
	struct be_ut_queue_layers_thread  *t;
	struct be_ut_queue_layers_queue  **queues;
	struct be_ut_queue_layers_queue   *q;
	struct be_ut_queue_layers_rec     *recs;
	struct m0_be_queue_cfg             bq_cfg;
	struct m0_atomic64                 recs_pos;
	struct m0_be_op                   *ops;
	struct m0_be_op                   *op;
	uint64_t                           seed;
	uint64_t                           rec_values;
	uint64_t                           thread_values;
	int                                producers;
	int                                consumers;
	int                                i;
	int                                j;
	int                                k;
	int                                n;
	int                                rc;

	M0_ALLOC_ARR(recs, cfg->buqlc_recs_nr);
	M0_UT_ASSERT(recs != NULL);
	M0_ALLOC_ARR(threads, cfg->buqlc_layers_nr);
	M0_UT_ASSERT(threads != NULL);
	seed = 0;
	for (i = 0; i < cfg->buqlc_layers_nr; ++i) {
		M0_ALLOC_ARR(threads[i],
			     cfg->buqlc_threads[i].buqlct_threads_nr);
		M0_UT_ASSERT(threads[i] != NULL);
		for (j = 0; j < cfg->buqlc_threads[i].buqlct_threads_nr; ++j) {
			++seed;
			t = &threads[i][j];
			*t = (struct be_ut_queue_layers_thread){
				.buqlt_index        = i,
				.buqlt_rng_seed     = seed,
				.buqlt_type_get     =
					cfg->buqlc_threads[i].buqlct_type_get,
				.buqlt_type_put     =
					cfg->buqlc_threads[i].buqlct_type_put,
				.buqlt_get_nr       =
		    cfg->buqlc_threads[i].buqlct_get_queues_per_thread / 2 + 1,
				.buqlt_put_nr       =
		    cfg->buqlc_threads[i].buqlct_put_queues_per_thread / 2 + 1,
				.buqlt_iter_nr      =
		    cfg->buqlc_threads[i].buqlct_iter_nr,
				.buqlt_recs         = recs,
				.buqlt_recs_pos     = &recs_pos,
				.buqlt_recs_nr      = cfg->buqlc_recs_nr,
				.buqlt_value        = 0,
				.buqlt_q_put_all_nr =
					i == cfg->buqlc_layers_nr - 1 ? 0 :
					cfg->buqlc_queues[i].buqlcq_queues_nr,
			};
			m0_be_op_init(&t->buqlt_op_start);
			m0_be_op_init(&t->buqlt_op_finish);
			M0_ALLOC_ARR(t->buqlt_q_get, t->buqlt_get_nr);
			M0_UT_ASSERT(t->buqlt_q_get != NULL);
			M0_ALLOC_ARR(t->buqlt_q_put, t->buqlt_put_nr);
			M0_UT_ASSERT(t->buqlt_q_put != NULL);
			M0_ALLOC_ARR(t->buqlt_q_put_all, t->buqlt_q_put_all_nr);
			M0_UT_ASSERT(t->buqlt_q_put_all != NULL);
		}
	}
	for (i = 0; i < cfg->buqlc_layers_nr; ++i) {
		for (j = 0; j < cfg->buqlc_threads[i].buqlct_threads_nr; ++j) {
			rc = M0_THREAD_INIT(&threads[i][j].buqlt_thread,
			        struct be_ut_queue_layers_thread *,
			        NULL, &be_ut_queue_layers_thread_func,
			        &threads[i][j], "t-%d-%d", i, j);
			M0_UT_ASSERT(rc == 0);
		}
	}
	M0_ALLOC_ARR(ops, cfg->buqlc_layers_nr);
	M0_UT_ASSERT(ops != NULL);
	M0_ALLOC_PTR(op);
	M0_UT_ASSERT(op != NULL);
	m0_be_op_init(op);
	m0_be_op_make_set_and(op);
	for (i = 0; i < cfg->buqlc_layers_nr; ++i) {
		m0_be_op_init(&ops[i]);
		m0_be_op_set_add(op, &ops[i]);
		m0_be_op_make_set_and(&ops[i]);
		for (j = 0; j < cfg->buqlc_threads[i].buqlct_threads_nr; ++j) {
			m0_be_op_set_add(&ops[i],
					 &threads[i][j].buqlt_op_finish);
		}
		m0_be_op_set_add_finish(&ops[i]);
	}
	m0_be_op_set_add_finish(op);
	M0_ALLOC_ARR(queues, cfg->buqlc_layers_nr - 1);
	M0_UT_ASSERT(queues != NULL);
	for (i = 0; i < cfg->buqlc_layers_nr - 1; ++i) {
		M0_ALLOC_ARR(queues[i], cfg->buqlc_queues[i].buqlcq_queues_nr);
		be_ut_queue_layers_qcredits_init(queues[i],
				 cfg->buqlc_queues[i].buqlcq_queues_nr,
				 cfg->buqlc_recs_nr, &seed);
	}
	for (i = 0; i < cfg->buqlc_layers_nr; ++i) {
		for (k = 0; k < 2; ++k) {
			if ((i == 0 && k == 0) ||
			    (i == cfg->buqlc_layers_nr - 1 && k == 1))
				continue;
			be_ut_queue_layers_assign_queues_to_threads(
				threads[i],
				cfg->buqlc_threads[i].buqlct_threads_nr,
				queues[k == 0 ? i-1 : i],
				cfg->buqlc_queues[k == 0 ? i-1 : i].
				buqlcq_queues_nr,
				k == 0 ?
				cfg->buqlc_threads[i].
				buqlct_get_queues_per_thread :
				cfg->buqlc_threads[i].
				buqlct_put_queues_per_thread,
				k, &seed);
		}
	}
	for (i = 0; i < cfg->buqlc_layers_nr - 1; ++i) {
		for (j = 0; j < cfg->buqlc_queues[i].buqlcq_queues_nr; ++j) {
			q = &queues[i][j];
			producers = 0;
			for (k = 0; k < cfg->buqlc_threads[i].buqlct_threads_nr;
			     ++k) {
				t = &threads[i][k];
				for (n = 0; n < t->buqlt_put_nr; ++n) {
					producers += t->buqlt_q_put[n] == q;
				}
			}
			consumers = 0;
			for (k = 0;
			     k < cfg->buqlc_threads[i+1].buqlct_threads_nr;
			     ++k) {
				t = &threads[i+1][k];
				for (n = 0; n < t->buqlt_get_nr; ++n) {
					consumers += t->buqlt_q_get[n] == q;
				}
			}
			M0_UT_ASSERT(producers > 0);
			M0_UT_ASSERT(consumers > 0);
			// XXX check that they are more or less evenly
			// XXX distributed
			bq_cfg = (struct m0_be_queue_cfg){
				.bqc_q_size_max =
					cfg->buqlc_queues[i].buqlcq_q_size_max,
				.bqc_producers_nr_max =
					cfg->buqlc_threads[i].buqlct_threads_nr,
				.bqc_consumers_nr_max =
				      cfg->buqlc_threads[i+1].buqlct_threads_nr,
				.bqc_item_length =
					sizeof(struct be_ut_queue_layers_qitem),
			};
			rc = m0_be_queue_init(&q->buqlq_queue, &bq_cfg);
			M0_UT_ASSERT(rc == 0);
		}
	}
	m0_atomic64_set(&recs_pos, 0);
	for (i = cfg->buqlc_layers_nr - 1; i >= 0; --i) {
		for (j = 0; j < cfg->buqlc_threads[i].buqlct_threads_nr; ++j) {
			m0_be_op_active(&threads[i][j].buqlt_op_start);
			m0_be_op_done(&threads[i][j].buqlt_op_start);
		}
	}
	m0_be_op_wait(op);
	m0_be_op_fini(op);
	m0_free(op);
	for (i = 0; i < cfg->buqlc_layers_nr; ++i)
		m0_be_op_fini(&ops[i]);
	m0_free(ops);
	for (i = 0; i < cfg->buqlc_layers_nr - 1; ++i) {
		be_ut_queue_layers_qcredits_fini(queues[i],
				 cfg->buqlc_queues[i].buqlcq_queues_nr,
				 cfg->buqlc_recs_nr);
		for (j = 0; j < cfg->buqlc_queues[i].buqlcq_queues_nr; ++j) {
			m0_be_queue_fini(&queues[i][j].buqlq_queue);
		}
		m0_free(queues[i]);
	}
	m0_free(queues);
	M0_UT_ASSERT(m0_atomic64_get(&recs_pos) == cfg->buqlc_recs_nr);
	rec_values = 0;
	for (i = 0; i < cfg->buqlc_recs_nr; ++i) {
		M0_UT_ASSERT(recs[i].buqlr_consumed != 0);
		M0_UT_ASSERT(recs[i].buqlr_layers ==
			     (1 << cfg->buqlc_layers_nr) - 1);
		rec_values += recs[i].buqlr_value;
	}
	M0_UT_ASSERT(rec_values != 0);
	m0_free(recs);
	for (i = 0; i < cfg->buqlc_layers_nr; ++i) {
		thread_values = 0;
		for (j = 0; j < cfg->buqlc_threads[i].buqlct_threads_nr; ++j) {
			t = &threads[i][j];
			thread_values += t->buqlt_value;
			rc = m0_thread_join(&t->buqlt_thread);
			M0_UT_ASSERT(rc == 0);
			m0_thread_fini(&t->buqlt_thread);
			m0_free(t->buqlt_q_put_all);
			m0_free(t->buqlt_q_put);
			m0_free(t->buqlt_q_get);
			m0_be_op_fini(&t->buqlt_op_finish);
			m0_be_op_fini(&t->buqlt_op_start);
		}
		M0_UT_ASSERT(rec_values == thread_values);
		m0_free(threads[i]);
	}
	m0_free(threads);
}

void m0_be_ut_queue_layers(void)
{
	/*
	struct be_ut_queue_layers_cfg cfg = {
		.buqlc_recs_nr = 0x1000,
		.buqlc_layers_nr = 0x4,
		.buqlc_threads = {
			{
				.buqlct_threads_nr = 16,
				.buqlct_get_queues_per_thread = 0,
				.buqlct_put_queues_per_thread = 16,
				.buqlct_type_get = BE_UT_QUEUE_LAYERS_TYPE_REC,
				.buqlct_type_put = BE_UT_QUEUE_LAYERS_TYPE_OR,
			},
			{
				.buqlct_threads_nr = 8,
				.buqlct_get_queues_per_thread = 32,
				.buqlct_put_queues_per_thread = 32,
				.buqlct_type_get = BE_UT_QUEUE_LAYERS_TYPE_AND,
				.buqlct_type_put = BE_UT_QUEUE_LAYERS_TYPE_AND,
			},
			{
				.buqlct_threads_nr = 12,
				.buqlct_get_queues_per_thread = 64,
				.buqlct_put_queues_per_thread = 8,
				.buqlct_type_get = BE_UT_QUEUE_LAYERS_TYPE_OR,
				.buqlct_type_put = BE_UT_QUEUE_LAYERS_TYPE_SYNC,
			},
			{
				.buqlct_threads_nr = 8,
				.buqlct_get_queues_per_thread = 16,
				.buqlct_put_queues_per_thread = 0,
				.buqlct_type_get = BE_UT_QUEUE_LAYERS_TYPE_OR,
				.buqlct_type_put = BE_UT_QUEUE_LAYERS_TYPE_REC,
			},
		},
		.buqlc_queues = {
			{ .buqlcq_queues_nr = 64,  .buqlcq_q_size_max = 2 },
			{ .buqlcq_queues_nr = 128, .buqlcq_q_size_max = 1 },
			{ .buqlcq_queues_nr = 32,  .buqlcq_q_size_max = 3 },
		},
	};
	*/
	struct be_ut_queue_layers_cfg cfg = {
		.buqlc_recs_nr = 0x1000,
		.buqlc_layers_nr = 0x3,
		.buqlc_threads = {
			{
				.buqlct_threads_nr            = 1,
				.buqlct_get_queues_per_thread = 1,
				.buqlct_put_queues_per_thread = 1,
				.buqlct_iter_nr               = 1,
				.buqlct_type_get = BE_UT_QUEUE_LAYERS_TYPE_REC,
				.buqlct_type_put = BE_UT_QUEUE_LAYERS_TYPE_SYNC,
			},
			{
				.buqlct_threads_nr            = 1,
				.buqlct_get_queues_per_thread = 1,
				.buqlct_put_queues_per_thread = 1,
				.buqlct_iter_nr               = 1,
				.buqlct_type_get = BE_UT_QUEUE_LAYERS_TYPE_SYNC,
				.buqlct_type_put = BE_UT_QUEUE_LAYERS_TYPE_SYNC,
			},
			{
				.buqlct_threads_nr            = 1,
				.buqlct_get_queues_per_thread = 1,
				.buqlct_put_queues_per_thread = 1,
				.buqlct_iter_nr               = 1,
				.buqlct_type_get = BE_UT_QUEUE_LAYERS_TYPE_SYNC,
				.buqlct_type_put = BE_UT_QUEUE_LAYERS_TYPE_REC,
			},
		},
		.buqlc_queues = {
			{ .buqlcq_queues_nr = 1,  .buqlcq_q_size_max = 1 },
			{ .buqlcq_queues_nr = 1,  .buqlcq_q_size_max = 1 },
		},
	};
	be_ut_queue_layers(&cfg);
}

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
