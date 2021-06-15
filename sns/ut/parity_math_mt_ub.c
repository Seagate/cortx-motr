/*
 * Copyright (c) 2011-2021 Seagate Technology LLC and/or its Affiliates
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


#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "lib/types.h"
#include "lib/assert.h"
#include "lib/memory.h"
#include "lib/thread.h"
#include "lib/arith.h"	     /* m0_rnd64 */

#include "lib/ub.h"
#include "ut/ut.h"

#include "sns/matvec.h"
#include "sns/ls_solve.h"
#include "sns/parity_math.h"

#define MAX_NUM_THREADS 30
#define KB(x)		((x) * 1024)
#define MB(x)		(KB(x) * 1024)

static uint64_t seed = 42;

struct tb_cfg {
	uint32_t  tc_data_count;
	uint32_t  tc_parity_count;
	uint32_t  tc_fail_count;

	uint32_t  tc_block_size;

	uint8_t **tc_data;
	uint8_t **tc_backup;
	uint8_t **tc_parity;
	uint8_t  *tc_fail;
};

static int ub_init(const char *opts M0_UNUSED)
{
	return 0;
}

void tb_cfg_init(struct tb_cfg *cfg, uint32_t data_count, uint32_t parity_count,
		 uint32_t block_size, uint32_t th_num)
{
	uint32_t i;
	uint32_t fail_count;

	cfg->tc_data_count	= data_count;
	cfg->tc_parity_count	= parity_count;
	cfg->tc_fail_count	= data_count + parity_count;
	cfg->tc_block_size      = block_size;


	/* allocate data */
	M0_ALLOC_ARR(cfg->tc_data, data_count);
	M0_UT_ASSERT(cfg->tc_data != NULL);

	for (i = 0; i < data_count; ++i) {
		M0_ALLOC_ARR(cfg->tc_data[i], block_size);
		M0_UT_ASSERT(cfg->tc_data[i] != NULL);
	}

	/* allocate parity */
	M0_ALLOC_ARR(cfg->tc_parity, parity_count);
	M0_UT_ASSERT(cfg->tc_parity != NULL);

	/* allocate memory for backup */
	M0_ALLOC_ARR(cfg->tc_backup, parity_count);
	M0_UT_ASSERT(cfg->tc_backup != NULL);

	for (i = 0; i < parity_count; ++i) {
		M0_ALLOC_ARR(cfg->tc_parity[i], block_size);
		M0_UT_ASSERT(cfg->tc_parity[i] != NULL);

		M0_ALLOC_ARR(cfg->tc_backup[i], block_size);
		M0_UT_ASSERT(cfg->tc_backup[i] != NULL);
	}

	/* allocate and set fail info */
	M0_ALLOC_ARR(cfg->tc_fail, cfg->tc_fail_count);
	M0_UT_ASSERT(cfg->tc_fail != NULL);

	fail_count = (th_num % parity_count) + 1;
	for (i = 0; i < fail_count;) {
		uint32_t idx = m0_rnd64(&seed) % cfg->tc_fail_count;
		if (cfg->tc_fail[idx] == 0) {
			cfg->tc_fail[idx] = 1;
			i++;
		}
	}
}

void tb_cfg_fini(struct tb_cfg *cfg)
{
	uint32_t i;

	for (i = 0; i < cfg->tc_data_count; ++i)
		m0_free(cfg->tc_data[i]);
	m0_free(cfg->tc_data);

	for (i = 0; i < cfg->tc_parity_count; ++i){
		m0_free(cfg->tc_parity[i]);
		m0_free(cfg->tc_backup[i]);
	}
	m0_free(cfg->tc_parity);
	m0_free(cfg->tc_backup);

	m0_free(cfg->tc_fail);
}

static void unit_spoil(struct tb_cfg *cfg)
{
	uint32_t	i;
	uint32_t	j;
	uint8_t	       *addr;

	for (i = 0, j = 0; i < cfg->tc_fail_count; i++) {
		if (cfg->tc_fail[i]) {
			if (i < cfg->tc_data_count)
				addr = cfg->tc_data[i];
			else
				addr = cfg->tc_parity[i - cfg->tc_data_count];

			memcpy(cfg->tc_backup[j++], addr, cfg->tc_block_size);
			memset(addr, 0xFF, cfg->tc_block_size);
		}
	}
}

static void unit_compare(struct tb_cfg *cfg)
{
	uint32_t i, j;
	uint8_t *addr;

	for (i = 0, j = 0; i < cfg->tc_fail_count; i++) {
		if (cfg->tc_fail[i]) {
			if (i < cfg->tc_data_count)
				addr = cfg->tc_data[i];
			else
				addr = cfg->tc_parity[i - cfg->tc_data_count];

			M0_UT_ASSERT(memcmp(cfg->tc_backup[j++], addr, cfg->tc_block_size) == 0);
		}
	}
}

void tb_thread(struct tb_cfg *cfg)
{
	int ret = 0;
	uint32_t i = 0;

	uint32_t data_count	= cfg->tc_data_count;
	uint32_t parity_count	= cfg->tc_parity_count;
	uint32_t buff_size	= cfg->tc_block_size;
	uint32_t fail_count	= data_count + parity_count;

	struct m0_parity_math math;
	struct m0_buf *data_buf = 0;
	struct m0_buf *parity_buf = 0;
	struct m0_buf fail_buf;

	data_buf = m0_alloc(data_count * sizeof(struct m0_buf));
	M0_UT_ASSERT(data_buf);

	parity_buf = m0_alloc(parity_count * sizeof(struct m0_buf));
	M0_UT_ASSERT(parity_buf);


	ret = m0_parity_math_init(&math, data_count, parity_count);
	M0_UT_ASSERT(ret == 0);

	for (i = 0; i < data_count; ++i)
		m0_buf_init(&data_buf  [i], cfg->tc_data  [i], buff_size);

	for (i = 0; i < parity_count; ++i)
		m0_buf_init(&parity_buf[i], cfg->tc_parity[i], buff_size);


	ret = m0_parity_math_calculate(&math, data_buf, parity_buf);
	M0_UT_ASSERT(ret == 0);

	m0_buf_init(&fail_buf, cfg->tc_fail, fail_count);
	unit_spoil(cfg);

	ret = m0_parity_math_recover(&math, data_buf, parity_buf, &fail_buf, 0);
	M0_UT_ASSERT(ret == 0);

	unit_compare(cfg);

	m0_parity_math_fini(&math);
	m0_free(data_buf);
	m0_free(parity_buf);
}

static void ub_mt_test(uint32_t data_count,
		       uint32_t parity_count,
		       uint32_t block_size)
{
	uint32_t num_threads = MAX_NUM_THREADS;
	uint32_t i;
	int result = 0;

	struct tb_cfg *cfg;
	struct m0_thread *threads;

	threads = m0_alloc(num_threads * sizeof(struct m0_thread));
	M0_UT_ASSERT(threads != NULL);

	cfg = m0_alloc(num_threads * sizeof(struct tb_cfg));
	M0_UT_ASSERT(cfg != NULL);

	for (i = 0; i < num_threads; i++) {
		tb_cfg_init(&cfg[i], data_count, parity_count, block_size, i);
		result = M0_THREAD_INIT(&threads[i], struct tb_cfg*, NULL,
					&tb_thread, &cfg[i],
					"tb_thread%d", i);
		M0_UT_ASSERT(result == 0);
	}

	for (i = 0; i < num_threads; i++) {
		result = m0_thread_join(&threads[i]);
		M0_UT_ASSERT(result == 0);
		tb_cfg_fini(&cfg[i]);
	}

	m0_free(cfg);
	m0_free(threads);
}

void ub_small_4K() {
	ub_mt_test(10, 3, KB(4));
}

void ub_medium_4K() {
	ub_mt_test(20, 4, KB(4));
}

void ub_large_4K() {
	ub_mt_test(30, 4, KB(4));
}

void ub_small_1M() {
	ub_mt_test(10, 3, MB(1));
}

void ub_medium_1M() {
	ub_mt_test(20, 4, MB(1));
}

void ub_large_1M() {
	ub_mt_test(30, 4, MB(1));
}

void ub_small_32K() {
	ub_mt_test(10, 3, KB(32));
}

void ub_medium_32K() {
	ub_mt_test(20, 4, KB(32));
}

void ub_large_32K() {
	ub_mt_test(30, 4, KB(32));
}

static void ub_small_4_2_4K(int iter) {
	ub_mt_test(4, 2, KB(4));
}

static void ub_small_4_2_256K(int iter) {
	ub_mt_test(4, 2, KB(256));
}

static void ub_small_4_2_1M(int iter) {
	ub_mt_test(4, 2, MB(1));
}

enum { UB_ITER = 1 };

struct m0_ub_set m0_parity_math_mt_ub = {
	.us_name = "parity-math-mt-ub",
	.us_init = ub_init,
	.us_fini = NULL,
	.us_run  = {
		/*     parity_math-: */
		{ .ub_name  = "s 10/03/  4K",
		  .ub_iter  = UB_ITER,
		  .ub_round = ub_small_4K,
		  .ub_block_size = KB(4),
		  .ub_blocks_per_op = 13 * MAX_NUM_THREADS },

		{ .ub_name  = "m 20/04/  4K",
		  .ub_iter  = UB_ITER,
		  .ub_round = ub_medium_4K,
		  .ub_block_size = KB(4),
		  .ub_blocks_per_op = 24 * MAX_NUM_THREADS },

		{ .ub_name  = "l 30/04/  4K",
		  .ub_iter  = UB_ITER,
		  .ub_round = ub_large_4K,
		  .ub_block_size = KB(4),
		  .ub_blocks_per_op = 34 * MAX_NUM_THREADS },

		{ .ub_name  = "s 10/03/ 32K",
		  .ub_iter  = UB_ITER,
		  .ub_round = ub_small_32K,
		  .ub_block_size = KB(32),
		  .ub_blocks_per_op = 13 * MAX_NUM_THREADS },

		{ .ub_name  = "m 20/04/ 32K",
		  .ub_iter  = UB_ITER,
		  .ub_round = ub_medium_32K,
		  .ub_block_size = KB(32),
		  .ub_blocks_per_op = 24 * MAX_NUM_THREADS },

		{ .ub_name  = "l 30/04/ 32K",
		  .ub_iter  = UB_ITER,
		  .ub_round = ub_large_32K,
		  .ub_block_size = KB(32),
		  .ub_blocks_per_op = 34 * MAX_NUM_THREADS },

		{ .ub_name  = "s 10/05/  1M",
		  .ub_iter  = UB_ITER,
		  .ub_round = ub_small_1M,
		  .ub_block_size = MB(1),
		  .ub_blocks_per_op = 15 * MAX_NUM_THREADS },

		{ .ub_name  = "m 20/04/  1M",
		  .ub_iter  = UB_ITER,
		  .ub_round = ub_medium_1M,
		  .ub_block_size = MB(1),
		  .ub_blocks_per_op = 24 * MAX_NUM_THREADS },

		{ .ub_name  = "l 30/04/  1M",
		  .ub_iter  = UB_ITER,
		  .ub_round = ub_large_1M,
		  .ub_block_size = MB(1),
		  .ub_blocks_per_op = 34 * MAX_NUM_THREADS },

		{ .ub_name  = "s 04/02/  4K",
		  .ub_iter  = UB_ITER,
		  .ub_round = ub_small_4_2_4K,
		  .ub_block_size = 4096,
		  .ub_blocks_per_op = 6 * MAX_NUM_THREADS },

		{ .ub_name  = "m 04/02/256K",
		  .ub_iter  = UB_ITER,
		  .ub_round = ub_small_4_2_256K,
		  .ub_block_size = 262144,
		  .ub_blocks_per_op = 6 * MAX_NUM_THREADS },

		{ .ub_name  = "l 04/02/  1M",
		  .ub_iter  = UB_ITER,
		  .ub_round = ub_small_4_2_1M,
		  .ub_block_size = 1048576,
		  .ub_blocks_per_op = 6 * MAX_NUM_THREADS },

		{ .ub_name = NULL}
	}
};


/*
 *  Local variables:
 *  c-indentation-style: "K&R"
 *  c-basic-offset: 8
 *  tab-width: 8
 *  fill-column: 80
 *  scroll-step: 1
 *  End:
 */
