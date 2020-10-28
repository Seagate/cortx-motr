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
 * Further directions
 *
 * - make test_work_put() functions multithreaded to test MPMC queues;
 * @{
 */

#define M0_TRACE_SUBSYSTEM M0_TRACE_SUBSYS_UT
#include "lib/trace.h"

#include "be/tx_bulk.h"

#include "lib/mutex.h"          /* m0_mutex */
#include "lib/memory.h"         /* M0_ALLOC_PTR */
#include "lib/errno.h"          /* ENOENT */

#include "be/ut/helper.h"       /* m0_be_ut_backend_init */
#include "be/op.h"              /* m0_be_op */

#include "ut/ut.h"              /* M0_UT_ASSERT */

enum {
	BE_UT_TX_BULK_Q_SIZE_MAX     = 0x100,
	BE_UT_TX_BULK_SEG_SIZE       = 1UL << 26,
	BE_UT_TX_BULK_TX_SIZE_MAX_BP = 2000,
};

struct be_ut_tx_bulk_be_cfg {
	size_t tbbc_tx_group_nr;
};

static void be_ut_tx_bulk_test_run(struct m0_be_tx_bulk_cfg    *tb_cfg,
				   struct be_ut_tx_bulk_be_cfg *be_cfg,
                                   void                       (*test_prepare)
					(struct m0_be_ut_backend *ut_be,
					 struct m0_be_ut_seg     *ut_seg,
					 void                    *ptr),
				   void                       (*test_work_put)
				        (struct m0_be_tx_bulk *tb,
                                         bool                  success,
					 void                 *ptr),
				   void                        *ptr,
				   bool                         success)
{
	struct m0_be_ut_backend *ut_be;
	struct m0_be_domain_cfg  cfg = {};
	struct m0_be_tx_bulk    *tb;
	struct m0_be_ut_seg     *ut_seg;
	struct m0_be_op         *op;
	int                      rc;

	M0_ALLOC_PTR(ut_be);
	M0_UT_ASSERT(ut_be != NULL);
	M0_ALLOC_PTR(tb);
	M0_UT_ASSERT(tb != NULL);
	M0_ALLOC_PTR(ut_seg);
	M0_UT_ASSERT(ut_seg != NULL);

	/*
	 * Decrease max group and tx size to reduce seg and log I/O size needed
	 * for tx_bulk UTs.
	 */
	m0_be_ut_backend_cfg_default(&cfg);
	if (be_cfg != NULL && be_cfg->tbbc_tx_group_nr != 0) {
		cfg.bc_engine.bec_group_nr = be_cfg->tbbc_tx_group_nr;
		cfg.bc_pd_cfg.bpdc_seg_io_nr =
			max64u(cfg.bc_pd_cfg.bpdc_seg_io_nr,
			       be_cfg->tbbc_tx_group_nr);
	}
	m0_be_tx_credit_mul_bp(&cfg.bc_engine.bec_tx_size_max,
	                       BE_UT_TX_BULK_TX_SIZE_MAX_BP);
	m0_be_tx_credit_mul_bp(&cfg.bc_engine.bec_group_cfg.tgc_size_max,
	                       BE_UT_TX_BULK_TX_SIZE_MAX_BP);
	rc = m0_be_ut_backend_init_cfg(ut_be, &cfg, true);
	M0_UT_ASSERT(rc == 0);
	m0_be_ut_seg_init(ut_seg, ut_be, BE_UT_TX_BULK_SEG_SIZE);

	test_prepare(ut_be, ut_seg, ptr);

	tb_cfg->tbc_dom = &ut_be->but_dom;
	rc = m0_be_tx_bulk_init(tb, tb_cfg);

	M0_UT_ASSERT(rc == 0);
	M0_ALLOC_PTR(op);
	M0_UT_ASSERT(op != NULL);
	m0_be_op_init(op);

	m0_be_tx_bulk_run(tb, op);
	test_work_put(tb, success, ptr);
	m0_be_op_wait(op);

	m0_be_op_fini(op);
	m0_free(op);

	rc = m0_be_tx_bulk_status(tb);
	M0_UT_ASSERT(equi(rc == 0, success));
	m0_be_tx_bulk_fini(tb);

	m0_be_ut_seg_fini(ut_seg);
	m0_be_ut_backend_fini(ut_be);

	m0_free(ut_seg);
	m0_free(tb);
	m0_free(ut_be);
}


enum {
	BE_UT_TX_BULK_USECASE_ALLOC    = 1UL << 16,
};

struct be_ut_tx_bulk_usecase {
	struct m0_be_seg *tbu_seg;
	void             *tbu_pos;
	void             *tbu_end;
};

static void be_ut_tx_bulk_usecase_work_put(struct m0_be_tx_bulk *tb,
                                           bool                  success,
                                           void                 *ptr)
{
	struct be_ut_tx_bulk_usecase *bu = ptr;

	M0_UT_ASSERT(success);
	while (bu->tbu_pos + sizeof(uint64_t) < bu->tbu_end) {
		M0_BE_OP_SYNC(op,
			      m0_be_tx_bulk_put(tb, &op,
			                        &M0_BE_TX_CREDIT_TYPE(uint64_t),
			                        0, 0, bu->tbu_pos));
		bu->tbu_pos += sizeof(uint64_t);
	}
	m0_be_tx_bulk_end(tb);
}

static void be_ut_tx_bulk_usecase_do(struct m0_be_tx_bulk   *tb,
                                     struct m0_be_tx        *tx,
                                     struct m0_be_op        *op,
                                     void                   *datum,
                                     void                   *user)
{
	struct be_ut_tx_bulk_usecase *bu = datum;
	uint64_t                     *value = user;

	m0_be_op_active(op);
	*value = (uint64_t)value;
	M0_BE_TX_CAPTURE_PTR(bu->tbu_seg, tx, value);
	m0_be_op_done(op);
}

static void be_ut_tx_bulk_usecase_test_prepare(struct m0_be_ut_backend *ut_be,
                                               struct m0_be_ut_seg     *ut_seg,
                                               void                    *ptr)
{
	struct be_ut_tx_bulk_usecase *bu = ptr;

	bu->tbu_seg = ut_seg->bus_seg;
	m0_be_ut_alloc(ut_be, ut_seg, &bu->tbu_pos,
		       BE_UT_TX_BULK_USECASE_ALLOC);
	M0_UT_ASSERT(bu->tbu_pos != NULL);
	bu->tbu_end = bu->tbu_pos + BE_UT_TX_BULK_USECASE_ALLOC;
}

void m0_be_ut_tx_bulk_usecase(void)
{
	struct be_ut_tx_bulk_usecase *bu;
	struct m0_be_tx_bulk_cfg      tb_cfg = {
		.tbc_q_cfg                 = {
			.bqc_q_size_max       = BE_UT_TX_BULK_Q_SIZE_MAX,
			.bqc_producers_nr_max = 1,
			.bqc_consumers_nr_max = 0x100,  /* XXX */
		},
		.tbc_workers_nr            = 0x40, /* XXX */
		.tbc_partitions_nr         = 1,    /* XXX */
		.tbc_work_items_per_tx_max = 3,
		.tbc_do                    = &be_ut_tx_bulk_usecase_do,
	};
	M0_ALLOC_PTR(bu);
	M0_UT_ASSERT(bu != NULL);
	tb_cfg.tbc_datum = bu;
	be_ut_tx_bulk_test_run(&tb_cfg, NULL,
			       &be_ut_tx_bulk_usecase_test_prepare,
			       &be_ut_tx_bulk_usecase_work_put,
			       bu, true);
	m0_free(bu);
}

enum {
	BE_UT_TX_BULK_TX_NR_EMPTY         = 0x1000,
	BE_UT_TX_BULK_TX_NR_ERROR         = 0x10000,
	BE_UT_TX_BULK_TX_NR_LARGE_TX      = 0x40,
	BE_UT_TX_BULK_TX_NR_LARGE_PAYLOAD = 0x100,
	BE_UT_TX_BULK_TX_NR_LARGE_ALL     = 0x40,
	BE_UT_TX_BULK_TX_NR_SMALL_TX      = 0x1000,
	BE_UT_TX_BULK_TX_NR_MEDIUM_TX     = 0x1000,
	BE_UT_TX_BULK_TX_NR_LARGE_CRED    = 0x1000,
	BE_UT_TX_BULK_TX_NR_MEDIUM_CRED   = 0x1000,
	BE_UT_TX_BULK_BUF_NR              = 0x30,
	BE_UT_TX_BULK_BUF_SIZE            = 0x100000,
};

/**
 * bbs_<someting>     - plain value
 * bbs_<something>_bp - basis point from max value from engine cfg
 * Total value is calculated as
 * bbs_<someting> + (bbs_<something>_bp * <max value from engine cfg) / 10000
 */
struct be_ut_tx_bulk_state {
	size_t                   bbs_tx_group_nr;

	uint64_t                 bbs_nr_max;
	struct m0_be_tx_credit   bbs_cred;
	unsigned                 bbs_cred_bp;
	m0_bcount_t              bbs_payload_cred;
	unsigned                 bbs_payload_cred_bp;
	struct m0_be_tx_credit   bbs_use;
	unsigned                 bbs_use_bp;
	m0_bcount_t              bbs_payload_use;
	unsigned                 bbs_payload_use_bp;

	struct m0_be_tx_credit   bbs_cred_max;
	m0_bcount_t              bbs_payload_max;
	struct m0_be_seg        *bbs_seg;
	uint32_t                 bbs_buf_nr;
	m0_bcount_t              bbs_buf_size;
	void                   **bbs_buf;
};

static void be_ut_tx_bulk_state_calc(struct be_ut_tx_bulk_state *tbs,
                                     bool                        calc_cred,
                                     struct m0_be_tx_credit     *cred,
                                     m0_bcount_t                *cred_payload)
{
	struct m0_be_tx_credit cred_v;
	m0_bcount_t            payload_v;
	unsigned               cred_bp;
	unsigned               payload_bp;

	cred_v     = calc_cred ? tbs->bbs_cred           : tbs->bbs_use;
	cred_bp    = calc_cred ? tbs->bbs_cred_bp        : tbs->bbs_use_bp;
	payload_v  = calc_cred ? tbs->bbs_payload_cred   : tbs->bbs_payload_use;
	payload_bp = calc_cred ? tbs->bbs_payload_cred_bp :
							tbs->bbs_payload_use_bp;

	*cred         = tbs->bbs_cred_max;
	*cred_payload = tbs->bbs_payload_max;
	m0_be_tx_credit_mul_bp(cred, cred_bp);
	*cred_payload = (*cred_payload * payload_bp) / 10000;
	m0_be_tx_credit_add(cred, &cred_v);
	*cred_payload += payload_v;
}

static void be_ut_tx_bulk_state_work_put(struct m0_be_tx_bulk *tb,
                                         bool                  success,
                                         void                 *ptr)
{
	struct be_ut_tx_bulk_state *tbs = ptr;
	struct m0_be_tx_credit      cred;
	m0_bcount_t                 cred_payload;
	uint64_t                    i;
	bool                        put_successful = true;

	for (i = 0; i < tbs->bbs_nr_max; ++i) {
		be_ut_tx_bulk_state_calc(tbs, true, &cred, &cred_payload);
		M0_BE_OP_SYNC(op, put_successful =
		              m0_be_tx_bulk_put(tb, &op, &cred, cred_payload, 0,
		                                (void *)i));
		if (!put_successful)
			break;
	}
	M0_UT_ASSERT(equi(success, put_successful));
	m0_be_tx_bulk_end(tb);
}

static void be_ut_tx_bulk_state_do(struct m0_be_tx_bulk   *tb,
                                   struct m0_be_tx        *tx,
                                   struct m0_be_op        *op,
                                   void                   *datum,
                                   void                   *user)
{
	struct be_ut_tx_bulk_state *tbs = datum;
	struct m0_be_tx_credit      use;
	struct m0_buf               buf;
	m0_bcount_t                 left;
	m0_bcount_t                 use_payload;
	uint64_t                    start = (uint64_t)user;
	uint64_t                    i;

	m0_be_op_active(op);
	be_ut_tx_bulk_state_calc(tbs, false, &use, &use_payload);
	left = use.tc_reg_size;
	for (i = start; i < start + tbs->bbs_buf_nr; ++i) {
		if (left == 0)
			break;
		buf.b_nob  = min_check(left, tbs->bbs_buf_size);
		buf.b_addr = tbs->bbs_buf[i % tbs->bbs_buf_nr];
		left -= buf.b_nob;
		M0_BE_TX_CAPTURE_BUF(tbs->bbs_seg, tx, &buf);
	}
	M0_UT_ASSERT(left == 0);
	tx->t_payload.b_nob = use_payload;
	memset(tx->t_payload.b_addr, 0, tx->t_payload.b_nob);
	m0_be_op_done(op);
}

static void be_ut_tx_bulk_test_prepare(struct m0_be_ut_backend *ut_be,
                                       struct m0_be_ut_seg     *ut_seg,
                                       void                    *ptr)
{
	struct be_ut_tx_bulk_state *tbs = ptr;
	uint32_t                    i;

	tbs->bbs_seg         = ut_seg->bus_seg;
	m0_be_domain_tx_size_max(&ut_be->but_dom, &tbs->bbs_cred_max,
				 &tbs->bbs_payload_max);
	for (i = 0; i < tbs->bbs_buf_nr; ++i) {
		m0_be_ut_alloc(ut_be, ut_seg, &tbs->bbs_buf[i],
		               tbs->bbs_buf_size);
		M0_UT_ASSERT(tbs->bbs_buf[i] != NULL);
	}
}

enum {
	BE_UT_TX_BULK_FILL_NOTHING  = 0,
	BE_UT_TX_BULK_FILL_TX       = 1 << 1,
	BE_UT_TX_BULK_FILL_PAYLOAD  = 1 << 2,
	BE_UT_TX_BULK_ERROR_REG     = 1 << 3,
	BE_UT_TX_BULK_ERROR_PAYLOAD = 1 << 4,
};

static void be_ut_tx_bulk_state_test_run(struct be_ut_tx_bulk_state  *tbs,
					 struct be_ut_tx_bulk_be_cfg *be_cfg,
                                         bool                         success)
{
	struct m0_be_tx_bulk_cfg tb_cfg = {
		.tbc_q_cfg                 = {
			.bqc_q_size_max       = BE_UT_TX_BULK_Q_SIZE_MAX,
			.bqc_producers_nr_max = 1,
			.bqc_consumers_nr_max = 0x100,  /* XXX */
		},
		.tbc_workers_nr            = 0x40, /* XXX */
		.tbc_partitions_nr         = 1,    /* XXX */
		.tbc_work_items_per_tx_max = 1,
		.tbc_do                    = &be_ut_tx_bulk_state_do,
	};

	tb_cfg.tbc_datum = tbs;
	tbs->bbs_buf_nr   = BE_UT_TX_BULK_BUF_NR;
	tbs->bbs_buf_size = BE_UT_TX_BULK_BUF_SIZE;
	M0_ALLOC_ARR(tbs->bbs_buf, tbs->bbs_buf_nr);
	M0_UT_ASSERT(tbs->bbs_buf != NULL);

	be_ut_tx_bulk_test_run(&tb_cfg, be_cfg, &be_ut_tx_bulk_test_prepare,
	                       &be_ut_tx_bulk_state_work_put,
			       tbs, success);

	m0_free(tbs->bbs_buf);
}

void m0_be_ut_tx_bulk_empty(void)
{
	be_ut_tx_bulk_state_test_run(&((struct be_ut_tx_bulk_state){
		.bbs_nr_max          = BE_UT_TX_BULK_TX_NR_EMPTY,
		.bbs_cred            = M0_BE_TX_CREDIT(0, 0),
		.bbs_cred_bp         = 0,
		.bbs_payload_cred    = 0,
		.bbs_payload_cred_bp = 0,
		.bbs_use             = M0_BE_TX_CREDIT(0, 0),
		.bbs_use_bp          = 0,
		.bbs_payload_use     = 0,
		.bbs_payload_use_bp  = 0,
	}), NULL, true);
}

void m0_be_ut_tx_bulk_error_reg(void)
{
	be_ut_tx_bulk_state_test_run(&((struct be_ut_tx_bulk_state){
		.bbs_nr_max          = BE_UT_TX_BULK_TX_NR_ERROR,
		.bbs_cred            = M0_BE_TX_CREDIT(0, 1),
		.bbs_cred_bp         = 10000,
		.bbs_payload_cred    = 0,
		.bbs_payload_cred_bp = 0,
		.bbs_use             = M0_BE_TX_CREDIT(0, 0),
		.bbs_use_bp          = 0,
		.bbs_payload_use     = 0,
		.bbs_payload_use_bp  = 0,
	}), NULL, false);
}

void m0_be_ut_tx_bulk_error_payload(void)
{
	be_ut_tx_bulk_state_test_run(&((struct be_ut_tx_bulk_state){
		.bbs_nr_max          = BE_UT_TX_BULK_TX_NR_ERROR,
		.bbs_cred            = M0_BE_TX_CREDIT(0, 0),
		.bbs_cred_bp         = 0,
		.bbs_payload_cred    = 1,
		.bbs_payload_cred_bp = 10000,
		.bbs_use             = M0_BE_TX_CREDIT(0, 0),
		.bbs_use_bp          = 0,
		.bbs_payload_use     = 0,
		.bbs_payload_use_bp  = 0,
	}), NULL, false);
}

void m0_be_ut_tx_bulk_large_tx(void)
{
	be_ut_tx_bulk_state_test_run(&((struct be_ut_tx_bulk_state){
		.bbs_nr_max          = BE_UT_TX_BULK_TX_NR_LARGE_TX,
		.bbs_cred            = M0_BE_TX_CREDIT(0, 0),
		.bbs_cred_bp         = 10000,
		.bbs_payload_cred    = 0,
		.bbs_payload_cred_bp = 0,
		.bbs_use             = M0_BE_TX_CREDIT(0, 0),
		.bbs_use_bp          = 10000,
		.bbs_payload_use     = 0,
		.bbs_payload_use_bp  = 0,
	}), NULL, true);
}

void m0_be_ut_tx_bulk_large_payload(void)
{
	be_ut_tx_bulk_state_test_run(&((struct be_ut_tx_bulk_state){
		.bbs_nr_max          = BE_UT_TX_BULK_TX_NR_LARGE_PAYLOAD,
		.bbs_cred            = M0_BE_TX_CREDIT(0, 0),
		.bbs_cred_bp         = 0,
		.bbs_payload_cred    = 0,
		.bbs_payload_cred_bp = 10000,
		.bbs_use             = M0_BE_TX_CREDIT(0, 0),
		.bbs_use_bp          = 0,
		.bbs_payload_use     = 0,
		.bbs_payload_use_bp  = 10000,
	}), NULL, true);
}

void m0_be_ut_tx_bulk_large_all(void)
{
	be_ut_tx_bulk_state_test_run(&((struct be_ut_tx_bulk_state){
		.bbs_nr_max          = BE_UT_TX_BULK_TX_NR_LARGE_ALL,
		.bbs_cred            = M0_BE_TX_CREDIT(0, 0),
		.bbs_cred_bp         = 10000,
		.bbs_payload_cred    = 0,
		.bbs_payload_cred_bp = 10000,
		.bbs_use             = M0_BE_TX_CREDIT(0, 0),
		.bbs_use_bp          = 10000,
		.bbs_payload_use     = 0,
		.bbs_payload_use_bp  = 10000,
	}), NULL, true);
}

void m0_be_ut_tx_bulk_small_tx(void)
{
	be_ut_tx_bulk_state_test_run(&((struct be_ut_tx_bulk_state){
		.bbs_nr_max          = BE_UT_TX_BULK_TX_NR_SMALL_TX,
		.bbs_cred            = M0_BE_TX_CREDIT(0x8, 0x8),
		.bbs_cred_bp         = 0,
		.bbs_payload_cred    = 0x8,
		.bbs_payload_cred_bp = 0,
		.bbs_use             = M0_BE_TX_CREDIT(0x8, 0x8),
		.bbs_use_bp          = 0,
		.bbs_payload_use     = 0x8,
		.bbs_payload_use_bp  = 0,
	}), NULL, true);
}

void m0_be_ut_tx_bulk_medium_tx(void)
{
	be_ut_tx_bulk_state_test_run(&((struct be_ut_tx_bulk_state){
		.bbs_nr_max          = BE_UT_TX_BULK_TX_NR_MEDIUM_TX,
		.bbs_cred            = M0_BE_TX_CREDIT(1, 1),
		.bbs_cred_bp         = 10,
		.bbs_payload_cred    = 1,
		.bbs_payload_cred_bp = 5,
		.bbs_use             = M0_BE_TX_CREDIT(1, 1),
		.bbs_use_bp          = 10,
		.bbs_payload_use     = 1,
		.bbs_payload_use_bp  = 5,
	}), NULL, true);
}

/* m0_be_ut_tx_bulk_medium_tx with 8 tx_groups */
void m0_be_ut_tx_bulk_medium_tx_multi(void)
{
	be_ut_tx_bulk_state_test_run(&((struct be_ut_tx_bulk_state){
		.bbs_nr_max          = BE_UT_TX_BULK_TX_NR_MEDIUM_TX,
		.bbs_cred            = M0_BE_TX_CREDIT(1, 1),
		.bbs_cred_bp         = 10,
		.bbs_payload_cred    = 1,
		.bbs_payload_cred_bp = 5,
		.bbs_use             = M0_BE_TX_CREDIT(1, 1),
		.bbs_use_bp          = 10,
		.bbs_payload_use     = 1,
		.bbs_payload_use_bp  = 5,
	}),
	&((struct be_ut_tx_bulk_be_cfg){
		.tbbc_tx_group_nr = 8,
	}), true);
}

void m0_be_ut_tx_bulk_medium_cred(void)
{
	be_ut_tx_bulk_state_test_run(&((struct be_ut_tx_bulk_state){
		.bbs_nr_max          = BE_UT_TX_BULK_TX_NR_MEDIUM_CRED,
		.bbs_cred            = M0_BE_TX_CREDIT(0, 0),
		.bbs_cred_bp         = 100,
		.bbs_payload_cred    = 0,
		.bbs_payload_cred_bp = 80,
		.bbs_use             = M0_BE_TX_CREDIT(0, 0),
		.bbs_use_bp          = 10,
		.bbs_payload_use     = 0,
		.bbs_payload_use_bp  = 2,
	}), NULL, true);
}

void m0_be_ut_tx_bulk_large_cred(void)
{
	be_ut_tx_bulk_state_test_run(&((struct be_ut_tx_bulk_state){
		.bbs_nr_max          = BE_UT_TX_BULK_TX_NR_LARGE_CRED,
		.bbs_cred            = M0_BE_TX_CREDIT(0, 0),
		.bbs_cred_bp         = 1000,
		.bbs_payload_cred    = 0,
		.bbs_payload_cred_bp = 500,
		.bbs_use             = M0_BE_TX_CREDIT(0, 0),
		.bbs_use_bp          = 10,
		.bbs_payload_use     = 0,
		.bbs_payload_use_bp  = 2,
	}), NULL, true);
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
