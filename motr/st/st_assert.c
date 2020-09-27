/* -*- C -*- */
/*
 * Copyright (c) 2020 Seagate Technology LLC and/or its Affiliates
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

#ifndef __KERNEL__
#include <unistd.h>
#include <errno.h>
#endif

#include "motr/client.h"
#include "motr/st/st.h"
#include "motr/st/st_assert.h"
#include "motr/st/st_misc.h"

/*XXX juan: add doxygen to static functions */

/*
 * Why a cleaner instead of a similar mechanism to cunit using setjmp/longjmp?
 * (1) Kernel doesn't support setjmp/longjmp
 * (2) It is preferred that a test developer focuses more on the logic (as
 *     a Client application developper) instead of worrying about how to clean
 *     the mess when an assert fails(and have to know things in Client and motr).
 * (3) In previous assert implementation, Tests share almost the same cleanup
 *     steps.
 *
 * A simple cleaner is triggered at the end of each test to clean up
 * things like unfinished operations, unfreed memory etc and recover
 * the test to where it starts when a ST_ASSERT fails.
 * (1) We take advantage of the fact that the things required to be cleaned
 *     are limited in a Client test.
 *      - Pending Client operations ()
 *      - Client entities
 *      - Allocated memory
 * (2) To remember what to clean, a test is required to:
 *      - use client api wrapper (xxx) instead of m0_xxx
 *      - use mem_alloc/free instead of M0_XXX
 *    In this way, those resources have a chance to be registered
 * (3) We take advantage of the fact that only one test is executed at any
 *     point of time in a worker thread to remember those resources needed to
 *     be cleaned.
 * (4) test_func --> aux_func --> assert: make sure to call ST_ASSERT
 *     right after aux_func in test_func so that the test terminates if
 *     any assert in aux_func fails and returns to run_test (TODO).
 */

static bool st_cleaner_up_flags[ST_MAX_WORKER_NUM];

/*
 * Resources that are used by a test (thread-wise)
 * To keep it simple arraies are used as the number of resources used
 * in a test normally is limited.
 */
struct st_cleaning_bin {
	/* Client ops*/
	int                       sg_op_cnt;
	struct m0_op     **sg_ops;

	/* Motr*/
	int                       sg_entity_cnt;
	struct m0_entity **sg_entities;

	/* Allocated memory address */
	int                       sg_ptr_cnt;
	void                    **sg_ptrs;
};

static struct st_cleaning_bin worker_bins[ST_MAX_WORKER_NUM];

enum {
	ST_MAX_OP_NUM      = 4096,
	ST_MAX_ENTITY_NUM  = 4096,
	ST_MAX_PTR_NUM     = 4096
};

/* ST_ASSERT|_FATAL Implementation */
bool st_assertimpl(bool c, const char *str_c, const char *file,
		   int lno, const char *func)
{
	static char            buf[1024];
	int                    idx;
	struct st_worker_stat *st;

	idx = st_get_worker_idx(m0_tid());
	st = st_get_worker_stat(idx);
	if (st == NULL)
		goto EXIT;
	st->sws_nr_asserts++;

	if (!c) {
		snprintf(buf, sizeof buf,
			"Client ST assertion failed "
			"[%s: line %d] %s on: %s\n", file, lno, func, str_c);
		console_printf("%s", buf);

		st->sws_nr_failed_asserts++;
	}

EXIT:
	return c;
}


static inline struct st_cleaning_bin *get_bin(void)
{
	int widx;

	widx = st_get_worker_idx(m0_tid());
	if (widx < 0)
		return NULL;

	return &worker_bins[widx];
}

void st_mark_op(struct m0_op *op)
{
	int                    i;
	struct st_cleaning_bin *bin;

	bin = get_bin();
	if (bin == NULL || bin->sg_ops == NULL
	    || bin->sg_op_cnt >= ST_MAX_OP_NUM)
		return;

	if (bin->sg_ops == NULL
	    || bin->sg_op_cnt >= ST_MAX_OP_NUM)
		return;

	for (i = 0; i < bin->sg_op_cnt; i++) {
		if (bin->sg_ops[i] == op)
			break;
	}

	if (i == bin->sg_op_cnt) {
		bin->sg_ops[i] = op;
		bin->sg_op_cnt++;
	}
}

void st_unmark_op(struct m0_op *op)

{
	int                    i;
	struct st_cleaning_bin *bin;

	bin = get_bin();
	if (bin == NULL || bin->sg_ops == NULL
	    || bin->sg_op_cnt >= ST_MAX_OP_NUM)
		return;

	for (i = 0; i < bin->sg_op_cnt; i++) {
		if (bin->sg_ops[i] == op)
			break;
	}

	/* to keep thing simple, we don't decrease the op_cnt*/
	if (i < bin->sg_op_cnt)
		bin->sg_ops[i] = NULL;
}

void st_mark_entity(struct m0_entity *entity)
{
	int                    i;
	struct st_cleaning_bin *bin;

	bin = get_bin();
	if (bin == NULL || bin->sg_entities == NULL
	    || bin->sg_entity_cnt >= ST_MAX_ENTITY_NUM)
		return;

	for (i = 0; i < bin->sg_entity_cnt; i++) {
		if (bin->sg_entities[i] == entity)
			break;
	}

	if (i == bin->sg_entity_cnt) {
		bin->sg_entities[i] = entity;
		bin->sg_entity_cnt++;
	}
}

void st_unmark_entity(struct m0_entity *entity)
{
	int                    i;
	struct st_cleaning_bin *bin;

	bin = get_bin();
	if (bin == NULL || bin->sg_entities == NULL
	    || bin->sg_entity_cnt >= ST_MAX_ENTITY_NUM)
		return;

	for (i = 0; i < bin->sg_entity_cnt; i++) {
		if (bin->sg_entities[i] == entity)
			break;
	}

	/* to keep thing simple, we don't decrease the cnt*/
	if (i < bin->sg_entity_cnt)
		bin->sg_entities[i] = NULL;
}

void st_mark_ptr(void *ptr)
{
	int                    i;
	struct st_cleaning_bin *bin;

	bin = get_bin();
	if (bin == NULL || bin->sg_ptrs == NULL
	    || bin->sg_ptr_cnt >= ST_MAX_PTR_NUM)
		return;

	for (i = 0; i < bin->sg_ptr_cnt; i++) {
		if (bin->sg_ptrs[i] == ptr)
			break;
	}

	if (i == bin->sg_ptr_cnt) {
		bin->sg_ptrs[i] = ptr;
		bin->sg_ptr_cnt++;
	}
}

void st_unmark_ptr(void *ptr)
{
	int                    i;
	struct st_cleaning_bin *bin;

	bin = get_bin();
	if (bin == NULL || bin->sg_ptrs == NULL
	    || bin->sg_ptr_cnt >= ST_MAX_PTR_NUM)
		return;

	for (i = 0; i < bin->sg_ptr_cnt; i++) {
		if (bin->sg_ptrs[i] == ptr)
			break;
	}

	/* to keep thing simple, we don't decrease the cnt*/
	if (i < bin->sg_ptr_cnt)
		bin->sg_ptrs[i] = NULL;
}

int st_cleaner_init()
{
	int                    i;
	int                    nr_workers;
	struct st_cleaning_bin *bin;

	nr_workers = ST_MAX_WORKER_NUM;

	for (i = 0; i < nr_workers; i++) {
		st_cleaner_up_flags[i] = false;

		/* allocate memory */
		bin = &worker_bins[i];

		bin->sg_op_cnt = 0;
		MEM_ALLOC_ARR(bin->sg_ops, ST_MAX_OP_NUM);
		if (bin->sg_ops == NULL)
			goto ERR_EXIT;

		bin->sg_entity_cnt = 0;
		MEM_ALLOC_ARR(bin->sg_entities, ST_MAX_ENTITY_NUM);
		if (bin->sg_entities == NULL)
			goto ERR_EXIT;

		bin->sg_ptr_cnt = 0;
		MEM_ALLOC_ARR(bin->sg_ptrs, ST_MAX_PTR_NUM);
		if (bin->sg_ptrs == NULL){
			goto ERR_EXIT;
		}
	}

	return 0;

ERR_EXIT:
	console_printf("No memory for cleaner\n");
	for (i = 0; i < nr_workers; i++) {
		bin = &worker_bins[i];
		if (bin->sg_ops)
			mem_free(bin->sg_ops);
		if (bin->sg_entities)
			mem_free(bin->sg_entities);
		if (bin->sg_ptrs)
			mem_free(bin->sg_ptrs);
	}

	return -ENOMEM;
}

void st_cleaner_fini()
{
	int                    i;
	int                    nr_workers;
	struct st_cleaning_bin *bin;

	nr_workers = ST_MAX_WORKER_NUM;
	for (i = 0; i < nr_workers; i++) {
		st_cleaner_up_flags[i] = false;
		bin = &worker_bins[i];
		if (bin->sg_ops)
			mem_free(bin->sg_ops);
		if (bin->sg_entities)
			mem_free(bin->sg_entities);
		if (bin->sg_ptrs)
			mem_free(bin->sg_ptrs);
	}
}

bool st_is_cleaner_up()
{
	int widx;

	widx = st_get_worker_idx(m0_tid());
	if (widx < 0)
		return false;

	return st_cleaner_up_flags[widx];
}

void st_cleaner_enable()
{
	int widx;

	widx = st_get_worker_idx(m0_tid());
	if (widx < 0)
		return;

	st_cleaner_up_flags[widx] = true;
}

void st_cleaner_disable()
{
	int widx;

	widx = st_get_worker_idx(m0_tid());
	if (widx < 0)
		return;

	st_cleaner_up_flags[widx] = false;
}

static void release_op(struct m0_op *op)
{
	int max_try;
	int try_cnt;
	m0_time_t sleep_time;

	/*
	 * There is a risk here to wait for state change for
	 * long long time(may be for ever?). op_cancel is needed
	 *
	 * Sining: [comments on op_wait]
	 * 1. op_wait requires APP to specify the states it is waiting for.
	 *    This means that Client has to expose states to an
	 *    APP via internal m0_sm data structure(op_sm). Good or bad?
	 * 2. Is there any case an APP waiting for a state other than
	 *    OS_FAILED or OS_STABLE? I don't see any case at this moment.
	 *    so why not remove the states arguments?
	 * 3. Knowing the intermitte states like LAUNCHED and INITIALISED by
	 *    an APP, good or bad?
	 * 4. Timeout handling. What is the best practice for an APP handling
	 *    timeout? Ideaop_cancel is a must-have!!!
	 *    Right now client doesn't have op_cancel. Can't call op_fini and
	 *    op_free as they have asserts on op state, and if we call op_free
	 *    and launched op's reply comes back later, this will cause problem
	 *    as op has been freed!!
	 *
	 */
	try_cnt = 0;
	max_try = 120;
	while (op->op_sm.sm_state == M0_OS_LAUNCHED) {
		m0_op_wait(op,
			M0_BITS(M0_OS_FAILED,
				M0_OS_STABLE),
			m0_time_from_now(5,0));

		try_cnt++;
		if (try_cnt == max_try) {
			console_printf("ST WARNING: "
				       "It takes too long waiting for OP "
				       "to complete!!\n");
			console_printf("ST_WARNING: "
				       "Kill me if you can't stand any longer\n");
			sleep_time = m0_time(2, 0);
			m0_nanosleep(sleep_time, NULL);
			try_cnt = 0;
		}
	}

	if (M0_IN(op->op_sm.sm_state,
		  (M0_OS_INITIALISED,
		   M0_OS_FAILED,
		   M0_OS_STABLE)))
	{
		m0_op_fini(op);
	}

	m0_op_free(op);
}

void st_cleaner_empty_bin()
{
	int    i;
	struct st_cleaning_bin *bin;

	bin = get_bin();
	if (bin == NULL)
		return;

	for (i = 0; i < bin->sg_op_cnt; i++) {
		if (bin->sg_ops[i] != NULL) {
			release_op(bin->sg_ops[i]);
			bin->sg_ops[i] = NULL;
		}
	}
	bin->sg_op_cnt = 0;

	for (i = 0; i < bin->sg_entity_cnt; i++) {
		if (bin->sg_entities[i] != NULL) {
			if(M0_IN(bin->sg_entities[i]->en_sm.sm_state,
			    (M0_ES_INIT, M0_ES_FAILED))) {
				break;
			}
			m0_entity_fini(bin->sg_entities[i]);
			bin->sg_entities[i] = NULL;
		}
	}
	bin->sg_entity_cnt = 0;

	for (i = 0; i < bin->sg_ptr_cnt; i++) {
		if (bin->sg_ptrs[i] != NULL) {
			mem_free(bin->sg_ptrs[i]);
			bin->sg_ptrs[i] = NULL;
		}
	}
	bin->sg_ptr_cnt = 0;
}

/*
 *  Local variables:
 *  c-indentation-style: "K&R"
 *  c-basic-offset: 8
 *  tab-width: 8
 *  fill-column: 80
 *  scroll-step: 1
 *  End:
 */
