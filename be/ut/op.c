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

#define M0_TRACE_SUBSYSTEM M0_TRACE_SUBSYS_UT
#include "lib/trace.h"

#include "be/op.h"

#include "lib/memory.h"    /* M0_ALLOC_PTR */
#include "lib/semaphore.h" /* m0_semaphore */
#include "ut/ut.h"         /* M0_UT_ASSERT */
#include "ut/threads.h"    /* M0_UT_THREADS_DEFINE */
#include "lib/time.h"      /* m0_time_now */


void m0_be_ut_op_usecase(void)
{
	struct m0_be_op op = {};

	m0_be_op_init(&op);
	M0_UT_ASSERT(!m0_be_op_is_done(&op));
	m0_be_op_active(&op);
	M0_UT_ASSERT(!m0_be_op_is_done(&op));
	m0_be_op_done(&op);
	M0_UT_ASSERT(m0_be_op_is_done(&op));
	m0_be_op_fini(&op);
}

enum be_ut_op_mt_cmd {
	BE_UT_OP_MT_INIT,
	BE_UT_OP_MT_WAIT1,
	BE_UT_OP_MT_ACTIVE,
	BE_UT_OP_MT_WAIT2,
	BE_UT_OP_MT_DONE,
	BE_UT_OP_MT_WAIT3,
	BE_UT_OP_MT_FINI,
	BE_UT_OP_MT_CMD_NR,
};

enum be_ut_op_mt_dep_type {
	BE_UT_OP_MT_WAIT_BEFORE,
	BE_UT_OP_MT_WAIT_AFTER,
};

struct be_ut_op_mt_dep {
	enum be_ut_op_mt_cmd      bod_src;
	enum be_ut_op_mt_dep_type bod_type;
	enum be_ut_op_mt_cmd      bod_dst;
};

struct be_ut_op_mt_thread_cfg {
	struct m0_semaphore   bom_barrier;
	struct m0_semaphore  *bom_signal_to[2];
	struct m0_semaphore  *bom_wait_before;
	struct m0_semaphore  *bom_wait_after;
	struct m0_semaphore  *bom_try_down;
	enum be_ut_op_mt_cmd  bom_cmd;
	struct m0_be_op      *bom_op;
};

static void be_ut_op_mt_thread_func(void *param)
{
	struct be_ut_op_mt_thread_cfg *cfg = param;
	bool                           success;
	bool                           done;
	int                            i;

	M0_ENTRY("enter %d, wait4 %p", cfg->bom_cmd, cfg->bom_wait_before);
	if (cfg->bom_wait_before != NULL)
		m0_semaphore_down(cfg->bom_wait_before);
	M0_LOG(M0_DEBUG, "waited %d", cfg->bom_cmd);
	switch (cfg->bom_cmd) {
	case BE_UT_OP_MT_INIT:
		m0_be_op_init(cfg->bom_op);
		break;
	case BE_UT_OP_MT_ACTIVE:
		done = m0_be_op_is_done(cfg->bom_op);
		M0_UT_ASSERT(!done);
		m0_be_op_active(cfg->bom_op);
		break;
	case BE_UT_OP_MT_DONE:
		done = m0_be_op_is_done(cfg->bom_op);
		M0_UT_ASSERT(!done);
		m0_be_op_done(cfg->bom_op);
		break;
	case BE_UT_OP_MT_FINI:
		done = m0_be_op_is_done(cfg->bom_op);
		M0_UT_ASSERT(done);
		m0_be_op_fini(cfg->bom_op);
		break;
	case BE_UT_OP_MT_WAIT1:
	case BE_UT_OP_MT_WAIT2:
	case BE_UT_OP_MT_WAIT3:
		m0_be_op_wait(cfg->bom_op);
		done = m0_be_op_is_done(cfg->bom_op);
		M0_UT_ASSERT(done);
		break;
	default:
		M0_IMPOSSIBLE("invalid command %d", cfg->bom_cmd);
	}
	if (cfg->bom_try_down != NULL) {
		success = m0_semaphore_trydown(cfg->bom_try_down);
		M0_UT_ASSERT(success);
	}
	if (cfg->bom_wait_after != NULL)
		m0_semaphore_down(cfg->bom_wait_after);
	for (i = 0; i < ARRAY_SIZE(cfg->bom_signal_to); ++i) {
		if (cfg->bom_signal_to[i] != NULL) {
			M0_LOG(M0_DEBUG, "signal to %p", cfg->bom_signal_to[i]);
			m0_semaphore_up(cfg->bom_signal_to[i]);
		}
	}
	M0_LEAVE();
}

M0_UT_THREADS_DEFINE(be_ut_op_mt, &be_ut_op_mt_thread_func);

/**
 *  +---------------+
 *  |               |
 *  |               V
 *  |   INIT ---> WAIT1
 *  |    |          |
 *  |    V          V
 *  |  ACTIVE --> WAIT2
 *  |    |          |
 *  |    V          V
 *  +-- DONE ---> WAIT3 --> FINI
 */
void m0_be_ut_op_mt(void)
{
	struct be_ut_op_mt_thread_cfg *cfg;
	struct be_ut_op_mt_thread_cfg *src;
	struct be_ut_op_mt_thread_cfg *dst;
	struct be_ut_op_mt_dep        *dep;
	struct m0_semaphore            trigger = {};
	struct m0_semaphore            completion = {};
#define DEP(src, type, dst) { .bod_src = src, .bod_type = type, .bod_dst = dst }
	struct be_ut_op_mt_dep         deps[] = {
	DEP(BE_UT_OP_MT_INIT,   BE_UT_OP_MT_WAIT_BEFORE, BE_UT_OP_MT_ACTIVE),
	DEP(BE_UT_OP_MT_ACTIVE, BE_UT_OP_MT_WAIT_BEFORE, BE_UT_OP_MT_DONE),
	DEP(BE_UT_OP_MT_DONE,   BE_UT_OP_MT_WAIT_AFTER,  BE_UT_OP_MT_WAIT1),
	DEP(BE_UT_OP_MT_INIT,   BE_UT_OP_MT_WAIT_BEFORE, BE_UT_OP_MT_WAIT1),
	DEP(BE_UT_OP_MT_ACTIVE, BE_UT_OP_MT_WAIT_BEFORE, BE_UT_OP_MT_WAIT2),
	DEP(BE_UT_OP_MT_DONE,   BE_UT_OP_MT_WAIT_BEFORE, BE_UT_OP_MT_WAIT3),
	DEP(BE_UT_OP_MT_WAIT1,  BE_UT_OP_MT_WAIT_AFTER,  BE_UT_OP_MT_WAIT2),
	DEP(BE_UT_OP_MT_WAIT2,  BE_UT_OP_MT_WAIT_AFTER,  BE_UT_OP_MT_WAIT3),
	DEP(BE_UT_OP_MT_WAIT3,  BE_UT_OP_MT_WAIT_BEFORE, BE_UT_OP_MT_FINI),
	};
#undef DEP
	struct m0_be_op                op = {};
	int                            i;
	int                            rc;

	M0_ALLOC_ARR(cfg, BE_UT_OP_MT_CMD_NR);
	M0_UT_ASSERT(cfg != NULL);
	for (i = 0; i < BE_UT_OP_MT_CMD_NR; ++i) {
		rc = m0_semaphore_init(&cfg[i].bom_barrier, 0);
		M0_UT_ASSERT(rc == 0);
		cfg[i].bom_cmd = i;
		cfg[i].bom_op = &op;
	}
	rc = m0_semaphore_init(&trigger, 0);
	M0_UT_ASSERT(rc == 0);
	rc = m0_semaphore_init(&completion, 0);
	M0_UT_ASSERT(rc == 0);
	cfg[BE_UT_OP_MT_INIT].bom_wait_before  = &trigger;
	cfg[BE_UT_OP_MT_FINI].bom_signal_to[0] = &completion;
	for (i = 0; i < ARRAY_SIZE(deps); ++i) {
		dep = &deps[i];
		src = &cfg[dep->bod_src];
		dst = &cfg[dep->bod_dst];
		if (src->bom_signal_to[0] == NULL) {
			src->bom_signal_to[0] = &dst->bom_barrier;
		} else if (src->bom_signal_to[1] == NULL) {
			src->bom_signal_to[1] = &dst->bom_barrier;
		} else {
			M0_IMPOSSIBLE("invalid deps");
		}
		switch (dep->bod_type) {
		case BE_UT_OP_MT_WAIT_BEFORE:
			M0_UT_ASSERT(dst->bom_wait_before == NULL);
			dst->bom_wait_before = &dst->bom_barrier;
			break;
		case BE_UT_OP_MT_WAIT_AFTER:
			M0_UT_ASSERT(dst->bom_wait_after == NULL);
			dst->bom_wait_after = &dst->bom_barrier;
			break;
		default:
			M0_IMPOSSIBLE("invalid dep type");
		}
	}
	M0_UT_THREADS_START(be_ut_op_mt, BE_UT_OP_MT_CMD_NR, cfg);
	m0_semaphore_up(&trigger);
	m0_semaphore_down(&completion);
	M0_UT_THREADS_STOP(be_ut_op_mt);

	m0_semaphore_fini(&completion);
	m0_semaphore_fini(&trigger);
	for (i = 0; i < BE_UT_OP_MT_CMD_NR; ++i)
		m0_semaphore_fini(&cfg[i].bom_barrier);
	m0_free(cfg);
}

enum {
	BE_UT_OP_SET_AND_USECASE_NR = 0x1000,
};

/*
 * op
 *  \__ set[0]
 *  \__ set[1]
 *  ...
 *  \__ set[BE_UT_OP_SET_AND_USECASE_NR - 1]
 */
void m0_be_ut_op_set_and_usecase(void)
{
	struct m0_be_op  op = {};
	struct m0_be_op *set;
	int              i;

	M0_ALLOC_ARR(set, BE_UT_OP_SET_AND_USECASE_NR);
	M0_UT_ASSERT(set != NULL);
	m0_be_op_init(&op);
	m0_be_op_make_set_and(&op);
	M0_UT_ASSERT(!m0_be_op_is_done(&op));
	for (i = 0; i < BE_UT_OP_SET_AND_USECASE_NR; ++i) {
		m0_be_op_init(&set[i]);
		M0_UT_ASSERT(!m0_be_op_is_done(&op));
		m0_be_op_set_add(&op, &set[i]);
		M0_UT_ASSERT(!m0_be_op_is_done(&op));
	}
	m0_be_op_set_add_finish(&op);
	M0_UT_ASSERT(!m0_be_op_is_done(&op));
	for (i = 0; i < BE_UT_OP_SET_AND_USECASE_NR / 2; ++i) {
		m0_be_op_active(&set[i]);
		M0_UT_ASSERT(!m0_be_op_is_done(&op));
	}
	for (i = 1; i < BE_UT_OP_SET_AND_USECASE_NR / 2; ++i) {
		m0_be_op_done(&set[i]);
		M0_UT_ASSERT(!m0_be_op_is_done(&op));
	}
	for (i = BE_UT_OP_SET_AND_USECASE_NR / 2;
	     i < BE_UT_OP_SET_AND_USECASE_NR; ++i) {
		m0_be_op_active(&set[i]);
		M0_UT_ASSERT(!m0_be_op_is_done(&op));
		m0_be_op_done(&set[i]);
		M0_UT_ASSERT(!m0_be_op_is_done(&op));
	}
	m0_be_op_done(&set[0]);
	M0_UT_ASSERT(m0_be_op_is_done(&op));

	for (i = 0; i < BE_UT_OP_SET_AND_USECASE_NR; ++i)
		m0_be_op_fini(&set[i]);
	m0_be_op_fini(&op);

	m0_free(set);
}

enum {
	BE_UT_OP_SET_OR_USECASE_NR = 0x80,
};

void m0_be_ut_op_set_or_usecase(void)
{
	struct m0_be_op *op;
	struct m0_be_op *c;  /* children */
	struct m0_be_op *trigger;
	unsigned        *done_map;  /* one value == one done */
	bool             trigger_found;
	int              i;
	int              j;
	int              test_index;

	M0_ALLOC_PTR(op);
	M0_ASSERT(op != NULL);
	M0_ALLOC_ARR(c, BE_UT_OP_SET_OR_USECASE_NR);
	M0_ASSERT(c != NULL);
	M0_ALLOC_ARR(done_map, BE_UT_OP_SET_OR_USECASE_NR);
	M0_ASSERT(done_map != NULL);

	M0_UT_ASSERT(!m0_be_op_is_done(op));
	for (j = 0; j < BE_UT_OP_SET_OR_USECASE_NR; ++j)
		m0_be_op_init(&c[j]);
	/*
	 * 1. Check that op becomes done when one of the children becomes done.
	 * 2. Check that op becomes done when one of the children is already
	 *    done before the addition.
	 */
	m0_be_op_init(op);
	for (test_index = 1; test_index <= 2; ++test_index) {
		for (i = 0; i < BE_UT_OP_SET_OR_USECASE_NR; ++i) {
			m0_be_op_make_set_or(op);
			M0_UT_ASSERT(!m0_be_op_is_done(op));
			if (test_index == 2) {
				m0_be_op_active(&c[i]);
				M0_UT_ASSERT(!m0_be_op_is_done(op));
				m0_be_op_done(&c[i]);
				M0_UT_ASSERT(!m0_be_op_is_done(op));
			}
			for (j = i; j < BE_UT_OP_SET_OR_USECASE_NR; ++j) {
				m0_be_op_set_add(op, &c[j]);
				M0_UT_ASSERT(!m0_be_op_is_done(op));
			}
			m0_be_op_set_add_finish(op);
			if (test_index == 1) {
				M0_UT_ASSERT(!m0_be_op_is_done(op));
				m0_be_op_active(&c[i]);
				M0_UT_ASSERT(!m0_be_op_is_done(op));
				m0_be_op_done(&c[i]);
			}
			M0_UT_ASSERT(m0_be_op_is_done(op));
			M0_UT_ASSERT(m0_be_op_set_triggered_by(op) == &c[i]);
			if (i != BE_UT_OP_SET_OR_USECASE_NR - 1)
				m0_be_op_reset(op);
		}
		m0_be_op_reset(op);
		for (j = 0; j < BE_UT_OP_SET_OR_USECASE_NR; ++j)
			m0_be_op_reset(&c[j]);
	}
	/*
	 * 3. All BE ops become done before the first addition, parent op is
	 *    DONE nr times after.
	 * 4. All BE ops become done after the first addition, parent op is DONE
	 *    nr times after.
	 */
	for (test_index = 3; test_index <= 4; ++test_index) {
		for (j = 0; j < BE_UT_OP_SET_OR_USECASE_NR; ++j)
			done_map[j] = 0;
		if (test_index == 3) {
			for (j = 0; j < BE_UT_OP_SET_OR_USECASE_NR; ++j) {
				M0_UT_ASSERT(!m0_be_op_is_done(op));
				m0_be_op_active(&c[j]);
				M0_UT_ASSERT(!m0_be_op_is_done(op));
				m0_be_op_done(&c[j]);
				M0_UT_ASSERT(m0_be_op_is_done(&c[j]));
			}
		}
		for (i = 0; i < BE_UT_OP_SET_OR_USECASE_NR; ++i) {
			m0_be_op_make_set_or(op);
			for (j = 0; j < BE_UT_OP_SET_OR_USECASE_NR; ++j) {
				m0_be_op_set_add(op, &c[j]);
				M0_UT_ASSERT(!m0_be_op_is_done(op));
			}
			m0_be_op_set_add_finish(op);
			if (test_index == 4 && i == 0) {
				for (j = 0; j < BE_UT_OP_SET_OR_USECASE_NR;
				     ++j) {
					M0_UT_ASSERT(equi(
					        j == 0, !m0_be_op_is_done(op)));
					m0_be_op_active(&c[j]);
					M0_UT_ASSERT(equi(
					        j == 0, !m0_be_op_is_done(op)));
					m0_be_op_done(&c[j]);
					M0_UT_ASSERT(m0_be_op_is_done(op));
				}
			}
			M0_UT_ASSERT(m0_be_op_is_done(op));
			trigger = m0_be_op_set_triggered_by(op);
			trigger_found = false;
			for (j = 0; j < BE_UT_OP_SET_OR_USECASE_NR; ++j) {
				if (&c[j] == trigger) {
					done_map[j] = 1;
					trigger_found = true;
					break;
				}
			}
			M0_UT_ASSERT(trigger_found);
			m0_be_op_reset(trigger);
			m0_be_op_reset(op);
		}
		for (j = 0; j < BE_UT_OP_SET_OR_USECASE_NR; ++j)
			M0_ASSERT(done_map[j] == 1);
		m0_be_op_reset(op);
		for (j = 0; j < BE_UT_OP_SET_OR_USECASE_NR; ++j)
			m0_be_op_reset(&c[j]);
	}
	for (j = 0; j < BE_UT_OP_SET_OR_USECASE_NR; ++j)
		m0_be_op_fini(&c[j]);
	m0_be_op_fini(op);

	m0_free(done_map);
	m0_free(c);
	m0_free(op);
}

enum {
	BE_UT_OP_SET_TREE_LEVEL_SIZE   = 5,
	BE_UT_OP_SET_TREE_LEVEL_NR     = 8,
	BE_UT_OP_SET_TREE_RNG_SEED     = 5,
	BE_UT_OP_SET_TREE_SHUFFLE_ITER = 0x100,
};

enum be_ut_op_set_tree_cmd {
	BE_UT_OP_SET_TREE_INIT,
	BE_UT_OP_SET_TREE_SET_CONVERT,
	BE_UT_OP_SET_TREE_SET_ADD,
	BE_UT_OP_SET_TREE_SET_ADD_FINISH,
	BE_UT_OP_SET_TREE_STATES,
	BE_UT_OP_SET_TREE_FINI,
	BE_UT_OP_SET_TREE_SET_ACTIVE,
	BE_UT_OP_SET_TREE_SET_DONE,
	BE_UT_OP_SET_TREE_ASSERT_DONE,
	BE_UT_OP_SET_TREE_ASSERT_NOT_DONE,
};

static void be_ut_op_set_tree_swap(unsigned *a, unsigned *b)
{
	unsigned t;

	 t = *a;
	*a = *b;
	*b =  t;
}

/*
 * XXX add random_shuffle function to ut/something.h and use it everywhere
 * TODO use m0_ut_random_shuffle().
 */
static void be_ut_op_set_tree_random_shuffle(unsigned *arr,
                                             unsigned  nr,
                                             bool      keep_half_dist_order,
                                             uint64_t *seed)
{
	unsigned half_dist = nr / 2 + 1;
	unsigned a;
	unsigned b;
	int      i;
	int      j;

	for (i = 0; i < BE_UT_OP_SET_TREE_SHUFFLE_ITER; ++i) {
		a = m0_rnd64(seed) % nr;
		b = m0_rnd64(seed) % nr;
		be_ut_op_set_tree_swap(&arr[a], &arr[b]);
	}
	if (keep_half_dist_order) {
		for (i = 0; i < nr; ++i) {
			for (j = i + 1; j < nr; ++j) {
				if (arr[i] > arr[j] &&
				    (arr[i] + half_dist == arr[j] ||
				     arr[j] + half_dist == arr[i])) {
					be_ut_op_set_tree_swap(&arr[i],
							       &arr[j]);
				}
			}
		}
	}
}

static void be_ut_op_set_tree_do(struct m0_be_op            *op,
                                 struct m0_be_op            *child,
                                 enum be_ut_op_set_tree_cmd  cmd)
{
	bool done;

	switch (cmd) {
	case BE_UT_OP_SET_TREE_INIT:
		m0_be_op_init(op);
		break;
	case BE_UT_OP_SET_TREE_SET_CONVERT:
		m0_be_op_make_set_and(op);
		break;
	case BE_UT_OP_SET_TREE_SET_ADD:
		m0_be_op_set_add(op, child);
		break;
	case BE_UT_OP_SET_TREE_SET_ADD_FINISH:
		m0_be_op_set_add_finish(op);
		break;
	case BE_UT_OP_SET_TREE_FINI:
		m0_be_op_fini(op);
		break;
	case BE_UT_OP_SET_TREE_SET_ACTIVE:
		m0_be_op_active(op);
		break;
	case BE_UT_OP_SET_TREE_SET_DONE:
		m0_be_op_done(op);
		break;
	case BE_UT_OP_SET_TREE_ASSERT_DONE:
		done = m0_be_op_is_done(op);
		M0_UT_ASSERT(done);
		break;
	case BE_UT_OP_SET_TREE_ASSERT_NOT_DONE:
		done = m0_be_op_is_done(op);
		M0_UT_ASSERT(!done);
		break;
	default:
		M0_IMPOSSIBLE("impossible branch");
	}
}

static void be_ut_op_set_tree_recursive(struct m0_be_op            *op,
                                        enum be_ut_op_set_tree_cmd  cmd,
                                        int                         level,
                                        int                         index,
                                        uint64_t                   *seed)
{
	enum be_ut_op_set_tree_cmd cmd2;
	const int                  level_size = BE_UT_OP_SET_TREE_LEVEL_SIZE;
	const int                  level_nr   = BE_UT_OP_SET_TREE_LEVEL_NR;
	unsigned                   order[BE_UT_OP_SET_TREE_LEVEL_SIZE * 2] = {};
	unsigned                   i;
	unsigned                   j;

	for (i = 0; i < ARRAY_SIZE(order); ++i)
		order[i] = i;
	if (cmd == BE_UT_OP_SET_TREE_STATES) {
		be_ut_op_set_tree_do(&op[index], NULL,
				     BE_UT_OP_SET_TREE_ASSERT_NOT_DONE);
	}
	if (cmd == BE_UT_OP_SET_TREE_STATES && level + 2 == level_nr) {
		be_ut_op_set_tree_random_shuffle(order, level_size * 2 - 1,
						 true, seed);
		for (i = 0; i < ARRAY_SIZE(order); ++i) {
			j = index * level_size + order[i] % level_size + 1;
			cmd2 = order[i] / level_size == 0 ?
			       BE_UT_OP_SET_TREE_SET_ACTIVE :
			       BE_UT_OP_SET_TREE_SET_DONE;
			be_ut_op_set_tree_do(&op[j], NULL, cmd2);
			cmd2 = i < ARRAY_SIZE(order) - 1 ?
			       BE_UT_OP_SET_TREE_ASSERT_NOT_DONE :
			       BE_UT_OP_SET_TREE_ASSERT_DONE;
			be_ut_op_set_tree_do(&op[index], NULL, cmd2);
		}
	} else {
		if (level < level_nr - 1) {
			/* only first half of order[] is used in this branch */
			be_ut_op_set_tree_random_shuffle(order, level_size,
							 false, seed);
			/*
			 * Order is interpreted as order of op processing
			 * on the current level.
			 */
			for (i = 0; i < level_size; ++i) {
				j = index * level_size + order[i] + 1;
				if (cmd == BE_UT_OP_SET_TREE_SET_ADD) {
					be_ut_op_set_tree_do(&op[index],
							     &op[j], cmd);
				}
				be_ut_op_set_tree_recursive(op, cmd, level + 1,
							    j, seed);
			}
			if (cmd == BE_UT_OP_SET_TREE_SET_ADD) {
				be_ut_op_set_tree_do(
				        &op[index], NULL,
				        BE_UT_OP_SET_TREE_SET_ADD_FINISH);
			}
		}
		if (!M0_IN(cmd, (BE_UT_OP_SET_TREE_SET_ADD,
		                 BE_UT_OP_SET_TREE_STATES)))
			be_ut_op_set_tree_do(&op[index], NULL, cmd);
		if (cmd == BE_UT_OP_SET_TREE_INIT && level < level_nr - 1)
			be_ut_op_set_tree_do(&op[index], NULL,
					     BE_UT_OP_SET_TREE_SET_CONVERT);
	}
	if (cmd == BE_UT_OP_SET_TREE_STATES) {
		be_ut_op_set_tree_do(&op[index], NULL,
				     BE_UT_OP_SET_TREE_ASSERT_DONE);
	}
}

/*
 * op[0]
 * \__ op[1]
 * |   \___ op[LEVEL_SIZE + 1]
 * |   |    \___ op[LEVEL_SIZE + LEVEL_SIZE * LEVEL_SIZE + 1]
 * |   ...........
 * |   \___ op[LEVEL_SIZE + 2]
 * |   ...
 * |   \___ op[LEVEL_SIZE + LEVEL_SIZE]
 * \__ op[2]
 * |   \___ op[LEVEL_SIZE + LEVEL_SIZE + 1]
 * |   \___ op[LEVEL_SIZE + LEVEL_SIZE + 2]
 * |   ...
 * |   \___ op[LEVEL_SIZE + LEVEL_SIZE + LEVEL_SIZE]
 * \__ op[3]
 * |   \___ op[LEVEL_SIZE + 2 * LEVEL_SIZE + 1]
 * |   \___ op[LEVEL_SIZE + 2 * LEVEL_SIZE + 2]
 * |   ...
 * |   \___ op[LEVEL_SIZE + 2 * LEVEL_SIZE + LEVEL_SIZE]
 * ...
 * \__ op[LEVEL_SIZE]
 *     \___ op[LEVEL_SIZE + (LEVEL_SIZE - 1) * LEVEL_SIZE + 1]
 *     \___ op[LEVEL_SIZE + (LEVEL_SIZE - 1) * LEVEL_SIZE + 2]
 *     ...
 *     \___ op[LEVEL_SIZE + (LEVEL_SIZE - 1) * LEVEL_SIZE + LEVEL_SIZE]
 */
void m0_be_ut_op_set_and_tree(void)
{
	struct m0_be_op *op;
	unsigned         op_nr;
	unsigned         op_per_lvl;
	int              level;
	uint64_t         seed = BE_UT_OP_SET_TREE_RNG_SEED;

	op_nr = 0;
	op_per_lvl = 1;
	for (level = 0; level < BE_UT_OP_SET_TREE_LEVEL_NR; ++level) {
		op_nr      += op_per_lvl;
		op_per_lvl *= BE_UT_OP_SET_TREE_LEVEL_SIZE;
	}
	M0_ALLOC_ARR(op, op_nr);
	M0_UT_ASSERT(op != NULL);

	be_ut_op_set_tree_recursive(op, BE_UT_OP_SET_TREE_INIT,    0, 0, &seed);
	be_ut_op_set_tree_recursive(op, BE_UT_OP_SET_TREE_SET_ADD, 0, 0, &seed);
	be_ut_op_set_tree_recursive(op, BE_UT_OP_SET_TREE_STATES,  0, 0, &seed);
	be_ut_op_set_tree_recursive(op, BE_UT_OP_SET_TREE_FINI,    0, 0, &seed);

	m0_free(op);
}

enum {
	BE_UT_OSR_THREAD_NR      = 0x10,
	BE_UT_OSR_WORKER_PAIR_NR = 0x20,
	BE_UT_OSR_OPS_PER_PAIR   = 0x8,
	BE_UT_OSR_ITER_PER_PAIR  = 0x10,
	BE_UT_OSR_SLEEP_MS       = 3,
};

struct be_ut_op_set_random_worker_cfg {
	bool             bosrw_waiter;
	struct m0_be_op *bosrw_ops;
	int              bosrw_ops_nr;
	unsigned        *bosrw_order;
	struct m0_be_op  bosrw_start;
	struct m0_be_op  bosrw_finished;
	struct m0_be_op *bosrw_quit;
	m0_time_t       *bosrw_time_done;
	m0_time_t       *bosrw_time_done_received;
	uint64_t         bosrw_seed;
};


static struct m0_be_op *be_ut_op_set_random_realloc(struct m0_be_op *op)
{
	struct m0_be_op *op_new;

	m0_be_op_fini(op);
	/* allow new op before freeing old to not to get the same memory */
	M0_ALLOC_PTR(op_new);
	M0_ASSERT(op_new != NULL);
	m0_free(op);
	m0_be_op_init(op_new);
	return op_new;
}

static int be_ut_op_set_random_find(struct m0_be_op *needle,
                                    struct m0_be_op *haystack,
                                    int haystack_size)
{
	int i;

	for (i = 0; i < haystack_size; ++i) {
		if (&haystack[i] == needle)
			return i;
	}
	M0_IMPOSSIBLE();
}

static void
be_ut_op_set_random_work_make(struct be_ut_op_set_random_worker_cfg *doer,
                              struct be_ut_op_set_random_worker_cfg *waiter,
                              uint64_t *seed)
{
	unsigned i;

	for (i = 0; i < doer->bosrw_ops_nr; ++i)
		doer->bosrw_order[i] = i;
	be_ut_op_set_tree_random_shuffle(doer->bosrw_order,
	                                 doer->bosrw_ops_nr, false, seed);
	for (i = 0; i < doer->bosrw_ops_nr; ++i) {
		doer->bosrw_time_done[i]            = M0_TIME_NEVER;
		waiter->bosrw_time_done_received[i] = M0_TIME_NEVER;
	}
}

static void
be_ut_op_set_random_work_check(struct be_ut_op_set_random_worker_cfg *doer,
                               struct be_ut_op_set_random_worker_cfg *waiter)
{
	int i;

	for (i = 0; i < doer->bosrw_ops_nr; ++i) {
		M0_UT_ASSERT(doer->bosrw_time_done[i] != M0_TIME_NEVER);
		M0_UT_ASSERT(waiter->bosrw_time_done_received[i] !=
			     M0_TIME_NEVER);
		M0_UT_ASSERT(doer->bosrw_time_done[i] <
		             waiter->bosrw_time_done_received[i]);
	}
}

static void
be_ut_op_set_random_worker_waiter(struct be_ut_op_set_random_worker_cfg *cfg)
{
	struct m0_be_op *trigger;
	struct m0_be_op *op;
	int              index;
	int              i;
	int              j;

	M0_ENTRY("cfg=%p cfg->bosrw_ops=%p", cfg, cfg->bosrw_ops);
	M0_ALLOC_PTR(op);
	M0_ASSERT(op != NULL);
	m0_be_op_init(op);
	for (i = 0; i < cfg->bosrw_ops_nr; ++i) {
		m0_be_op_make_set_or(op);
		for (j = 0; j < cfg->bosrw_ops_nr; ++j)
			m0_be_op_set_add(op, &cfg->bosrw_ops[j]);
		m0_be_op_set_add_finish(op);
		m0_be_op_wait(op);
		trigger = m0_be_op_set_triggered_by(op);
		if (i < cfg->bosrw_ops_nr / 4 ||
		    i > 3 * cfg->bosrw_ops_nr / 4) {
			m0_be_op_reset(op);
		} else {
			op = be_ut_op_set_random_realloc(op);
		}
		M0_LOG(M0_DEBUG, "cfg=%p cfg->bosrw_ops=%p trigger=%p",
		       cfg, cfg->bosrw_ops, trigger);
		m0_be_op_reset(trigger);
		index = be_ut_op_set_random_find(trigger, cfg->bosrw_ops,
						 cfg->bosrw_ops_nr);
		M0_UT_ASSERT(cfg->bosrw_time_done_received[index] ==
		             M0_TIME_NEVER);
		cfg->bosrw_time_done_received[index] = m0_time_now();
	}
	m0_be_op_fini(op);
	m0_free(op);
	M0_LEAVE("cfg=%p cfg->bosrw_ops=%p", cfg, cfg->bosrw_ops);
}

static void
be_ut_op_set_random_worker_doer(struct be_ut_op_set_random_worker_cfg *cfg,
                                uint64_t                              *seed)
{
	uint64_t delay;
	int      i;
	int      index;

	M0_ENTRY("cfg=%p cfg->bosrw_ops=%p", cfg, cfg->bosrw_ops);
	for (i = 0; i < cfg->bosrw_ops_nr; ++i) {
		index = cfg->bosrw_order[i];
		m0_be_op_active(&cfg->bosrw_ops[index]);
		delay = m0_rnd64(seed) % BE_UT_OSR_SLEEP_MS;
		m0_nanosleep(M0_MKTIME(0, M0_TIME_ONE_MSEC * delay), NULL);
		M0_UT_ASSERT(cfg->bosrw_time_done[index] == M0_TIME_NEVER);
		cfg->bosrw_time_done[index] = m0_time_now();
		m0_be_op_done(&cfg->bosrw_ops[index]);
	}
	M0_LEAVE("cfg=%p cfg->bosrw_ops=%p", cfg, cfg->bosrw_ops);
}

static void be_ut_op_set_random_worker(void *param)
{
	struct be_ut_op_set_random_worker_cfg *cfg = param;
	struct m0_be_op                       *op;
	uint64_t                               seed = cfg->bosrw_seed;
	uint64_t                               i;

	M0_ALLOC_PTR(op);
	M0_ASSERT(op != NULL);
	m0_be_op_init(op);
	for (i = 0;; ++i) {
		m0_be_op_make_set_or(op);
		m0_be_op_set_add(op, &cfg->bosrw_start);
		m0_be_op_set_add(op, cfg->bosrw_quit);
		m0_be_op_set_add_finish(op);
		m0_be_op_wait(op);
		if (m0_be_op_set_triggered_by(op) == cfg->bosrw_quit) {
			m0_be_op_fini(cfg->bosrw_quit);
			m0_free(cfg->bosrw_quit);
			break;
		}
		M0_UT_ASSERT(m0_be_op_set_triggered_by(op) ==
			     &cfg->bosrw_start);
		if (i % 6 < 3) {
			m0_be_op_reset(op);
		} else {
			op = be_ut_op_set_random_realloc(op);
		}
		m0_be_op_reset(&cfg->bosrw_start);
		m0_be_op_active(&cfg->bosrw_finished);
		if (cfg->bosrw_waiter)
			be_ut_op_set_random_worker_waiter(cfg);
		else
			be_ut_op_set_random_worker_doer(cfg, &seed);
		m0_be_op_done(&cfg->bosrw_finished);
	}
	m0_be_op_fini(op);
	m0_free(op);
}

struct be_ut_op_set_random_thread_cfg {
	int bosrt_index;
	int bosrt_worker_pair_nr;
	int bosrt_ops_per_pair;
	int bosrt_iter_per_pair;
};

static void be_ut_op_set_random_thread_func(void *param)
{
	struct be_ut_op_set_random_thread_cfg *cfg = param;
	struct be_ut_op_set_random_worker_cfg *wcfg;
	struct m0_be_op                       *op;
	struct m0_be_op                       *ops;
	struct m0_be_op                       *wait_op;
	uint64_t                               counter;
	uint64_t                               seed = cfg->bosrt_index;
	int                                    pair_nr;
	int                                    index;
	int                                    i;
	int                                    j;
	int                                   *pos;
	struct m0_ut_threads_descr             descr = {
		.utd_thread_func = &be_ut_op_set_random_worker,
	};

	pair_nr = cfg->bosrt_worker_pair_nr;
	M0_ALLOC_ARR(wcfg, pair_nr * 2);
	M0_ASSERT(wcfg != NULL);
	for (i = 0; i < pair_nr * 2; ++i) {
		wcfg[i] = (struct be_ut_op_set_random_worker_cfg){
			.bosrw_waiter = i % 2 != 0,
			.bosrw_ops_nr = cfg->bosrt_ops_per_pair,
			.bosrw_seed   = i,
		};
		M0_ALLOC_PTR(wcfg[i].bosrw_quit);
		M0_ASSERT(wcfg[i].bosrw_quit != NULL);
		m0_be_op_init(&wcfg[i].bosrw_start);
		m0_be_op_init(&wcfg[i].bosrw_finished);
		m0_be_op_init(wcfg[i].bosrw_quit);
		if (i % 2 == 0) {
			M0_ALLOC_ARR(wcfg[i].bosrw_ops, wcfg[i].bosrw_ops_nr);
			M0_ASSERT(wcfg[i].bosrw_ops != NULL);
			for (j = 0; j < wcfg[i].bosrw_ops_nr; ++j)
				m0_be_op_init(&wcfg[i].bosrw_ops[j]);
			M0_ALLOC_ARR(wcfg[i].bosrw_order, wcfg[i].bosrw_ops_nr);
			M0_ASSERT(wcfg[i].bosrw_order != NULL);
			M0_ALLOC_ARR(wcfg[i].bosrw_time_done,
			             wcfg[i].bosrw_ops_nr);
			M0_ASSERT(wcfg[i].bosrw_time_done != NULL);
			wcfg[i].bosrw_time_done_received = NULL;
		} else {
			wcfg[i].bosrw_ops       = wcfg[i-1].bosrw_ops;
			wcfg[i].bosrw_order     = NULL;
			wcfg[i].bosrw_time_done = NULL;
			M0_ALLOC_ARR(wcfg[i].bosrw_time_done_received,
			             wcfg[i].bosrw_ops_nr);
		}
	}
	M0_ALLOC_PTR(wait_op);
	M0_ASSERT(wait_op != NULL);
	m0_be_op_init(wait_op);
	M0_ALLOC_ARR(ops, pair_nr);
	M0_ASSERT(ops != NULL);
	M0_ALLOC_ARR(pos, pair_nr);
	M0_ASSERT(pos != NULL);
	for (i = 0; i < pair_nr; ++i) {
		m0_be_op_init(&ops[i]);
		m0_be_op_active(&ops[i]);
		m0_be_op_done(&ops[i]);
		pos[i] = -1;
	}
	m0_ut_threads_start(&descr, pair_nr * 2, wcfg, sizeof *wcfg);
	counter = 0;
	while (m0_exists(j, pair_nr, pos[j] < cfg->bosrt_iter_per_pair)) {
		m0_be_op_make_set_or(wait_op);
		for (i = 0; i < pair_nr; ++i) {
			if (pos[i] < cfg->bosrt_iter_per_pair)
				m0_be_op_set_add(wait_op, &ops[i]);
		}
		m0_be_op_set_add_finish(wait_op);
		m0_be_op_wait(wait_op);
		op = m0_be_op_set_triggered_by(wait_op);
		if (counter % (cfg->bosrt_iter_per_pair / 2) <
		    cfg->bosrt_iter_per_pair / 3) {
			m0_be_op_reset(wait_op);
		} else {
			wait_op = be_ut_op_set_random_realloc(wait_op);
		}
		index = be_ut_op_set_random_find(op, ops, pair_nr);
		m0_be_op_reset(&ops[index]);
		m0_be_op_reset(&wcfg[2 * index].bosrw_finished);
		m0_be_op_reset(&wcfg[2 * index + 1].bosrw_finished);
		if (pos[index] >= 0) {
			be_ut_op_set_random_work_check(&wcfg[2 * index],
			                               &wcfg[2 * index + 1]);
		}
		++pos[index];
		if (pos[index] < cfg->bosrt_iter_per_pair) {
			be_ut_op_set_random_work_make(&wcfg[2 * index],
			                              &wcfg[2 * index + 1],
						      &seed);
			m0_be_op_make_set_and(&ops[index]);
			m0_be_op_set_add(&ops[index],
			                 &wcfg[2 * index].bosrw_finished);
			m0_be_op_set_add(&ops[index],
			                 &wcfg[2 * index + 1].bosrw_finished);
			m0_be_op_set_add_finish(&ops[index]);
			m0_be_op_active(&wcfg[2 * index].bosrw_start);
			m0_be_op_done(&wcfg[2 * index].bosrw_start);
			m0_be_op_active(&wcfg[2 * index + 1].bosrw_start);
			m0_be_op_done(&wcfg[2 * index + 1].bosrw_start);
		}
		++counter;
	}
	for (i = 0; i < pair_nr * 2; ++i) {
		m0_be_op_active(wcfg[i].bosrw_quit);
		m0_be_op_done(wcfg[i].bosrw_quit);
	}
	m0_ut_threads_stop(&descr);
	for (i = 0; i < pair_nr; ++i)
		m0_be_op_fini(&ops[i]);
	m0_free(pos);
	m0_free(ops);
	m0_be_op_fini(wait_op);
	m0_free(wait_op);
	for (i = 0; i < pair_nr * 2; ++i) {
		m0_free(wcfg[i].bosrw_time_done_received);
		m0_free(wcfg[i].bosrw_time_done);
		m0_be_op_fini(&wcfg[i].bosrw_finished);
		m0_be_op_fini(&wcfg[i].bosrw_start);
		m0_free(wcfg[i].bosrw_order);
		if (i % 2 == 0) {
			for (j = 0; j < wcfg[i].bosrw_ops_nr; ++j)
				m0_be_op_fini(&wcfg[i].bosrw_ops[j]);
			m0_free(wcfg[i].bosrw_ops);
		}
	}
	m0_free(wcfg);
}

M0_UT_THREADS_DEFINE(be_ut_op_set_random, &be_ut_op_set_random_thread_func);

void m0_be_ut_op_set_random(void)
{
	struct be_ut_op_set_random_thread_cfg *cfg;
	int                                    i;

	M0_ALLOC_ARR(cfg, BE_UT_OSR_THREAD_NR);
	M0_ASSERT(cfg != NULL);
	for (i = 0; i < BE_UT_OSR_THREAD_NR; ++i) {
		cfg[i] = (struct be_ut_op_set_random_thread_cfg){
			.bosrt_index          = i,
			.bosrt_worker_pair_nr = i < 4 ? i + 1 :
				BE_UT_OSR_WORKER_PAIR_NR,
			.bosrt_ops_per_pair   = BE_UT_OSR_OPS_PER_PAIR,
			.bosrt_iter_per_pair  = BE_UT_OSR_ITER_PER_PAIR,
		};
	}
	M0_UT_THREADS_START(be_ut_op_set_random, BE_UT_OSR_THREAD_NR, cfg);
	M0_UT_THREADS_STOP(be_ut_op_set_random);
	m0_free(cfg);
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
