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

#include "ut/threads.h"         /* m0_ut_theads_start */
#include "ut/ut.h"              /* M0_UT_ASSERT */

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
