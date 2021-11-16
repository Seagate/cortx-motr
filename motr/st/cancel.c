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


/*
 * Client API system tests to check if Client API matches its
 * specifications.
 */

#define M0_TRACE_SUBSYSTEM M0_TRACE_SUBSYS_CLIENT
#include "lib/trace.h"
#include "lib/misc.h"                      /* M0_SET0 */
#include "motr/client.h"
#include "motr/st/st.h"
#include "motr/st/st_misc.h"
#include "motr/st/st_assert.h"

#include "lib/memory.h"

struct m0_container st_cancel_container;
extern struct m0_addb_ctx m0_addb_ctx;
static const uint32_t UNIT_SIZE = DEFAULT_PARGRP_UNIT_SIZE;
static uint64_t layout_id;

static void create_obj(struct m0_uint128 *oid)
{
	int                rc;
	struct m0_uint128  id;
	struct m0_op      *op = NULL;
	struct m0_obj      obj;

	M0_SET0(&obj);

	/* get oid */
	oid_get(&id);

	/* Create an entity */
	st_obj_init(&obj,
		&st_cancel_container.co_realm,
		&id, layout_id);

	st_entity_create(NULL, &obj.ob_entity, &op);
	M0_ASSERT(op != NULL);

	st_op_launch(&op, 1);

	/* Wait for op to complete */
	rc = st_op_wait(op, M0_BITS(M0_OS_FAILED,
					   M0_OS_STABLE),
			       M0_TIME_NEVER);
	M0_ASSERT(rc == 0);
	M0_ASSERT(op->op_sm.sm_state == M0_OS_STABLE);
	M0_ASSERT(op->op_sm.sm_rc == 0);

	/* fini and release */
	st_op_fini(op);
	st_op_free(op);
	st_entity_fini(&obj.ob_entity);

	*oid = id;
}

static void op_cancel(struct m0_op  **ops, int obj_cnt, int start_obj)
{
	int i;
	int rc;
	int cur_obj;

	for (i = 0; i < obj_cnt; ++i) {
		cur_obj = start_obj + i;
		m0_op_cancel(&ops[cur_obj], 1);
	}
	for (i = 0; i < obj_cnt; ++i) {
		cur_obj = start_obj + i;
		rc = st_op_wait(ops[cur_obj],
				       M0_BITS(M0_OS_FAILED,
					       M0_OS_STABLE),
				       M0_TIME_NEVER);
		ST_ASSERT_FATAL(rc == 0);
	}
}

/**
 * Launch write and then cancel the operation
 */
static void m0_write_cancel(void)
{
	enum {OBJECT_NR = 20};

	int                    i;
	int                    rc;
	int                    obj_cnt;
	int                    start_obj;
	int                    cur_obj;
	int                    blk_cnt;
	int                    blk_size;
	uint64_t               last_index;
	struct m0_uint128     *ids;
	struct m0_obj         *objs;
	struct m0_op         **ops;
	struct m0_indexvec     ext;
	struct m0_bufvec       data;
	struct m0_bufvec       attr;

	MEM_ALLOC_ARR(objs, OBJECT_NR);
	ST_ASSERT_FATAL(objs != NULL);
	MEM_ALLOC_ARR(ids, OBJECT_NR);
	ST_ASSERT_FATAL(ids != NULL);
	MEM_ALLOC_ARR(ops, OBJECT_NR);
	ST_ASSERT_FATAL(ops != NULL);

	for (i = 0; i < OBJECT_NR; ++i)
		create_obj(ids + i);

	/* Prepare the data with 'value' */
	blk_cnt = 1000;
	blk_size = UNIT_SIZE;

	rc = m0_bufvec_alloc(&data, blk_cnt, blk_size);
	ST_ASSERT_FATAL(rc == 0);

	for (i = 0; i < blk_cnt; i++)
		memset(data.ov_buf[i], 'A', blk_size);

	/* Prepare indexvec for write */
	rc = m0_bufvec_alloc(&attr, blk_cnt, 1);
	ST_ASSERT_FATAL(rc == 0);
	rc = m0_indexvec_alloc(&ext, blk_cnt);
	ST_ASSERT_FATAL(rc == 0);

	last_index = 0;
	for (i = 0; i < blk_cnt; i++) {
		ext.iv_index[i] = last_index ;
		ext.iv_vec.v_count[i] = blk_size;
		last_index += blk_size;
		/* we don't want any attributes */
		attr.ov_vec.v_count[i] = 0;
	}
	for (i = 0; i < OBJECT_NR; ++i) {
		M0_SET0(&objs[i]);
		ops[i] = NULL;
		/* Set the object entity we want to write */
		st_obj_init(&objs[i],
				   &st_cancel_container.co_realm,
				   &ids[i], layout_id);
		st_entity_open(&objs[i].ob_entity);
		/* Create the write request */
		st_obj_op(&objs[i], M0_OC_WRITE, &ext, &data,
			  &attr, 0, 0, &ops[i]);
	}
	/*
	 * Launch the write request for half of the objects and
	 * cancel them immediately
	 */
	obj_cnt = OBJECT_NR / 2;
	start_obj = 0;
	st_op_launch(ops, obj_cnt);
	for (i = 0; i < obj_cnt; ++i) {
		rc = st_op_wait(ops[i],
				       M0_BITS(M0_OS_LAUNCHED),
				       M0_TIME_NEVER);
		ST_ASSERT_FATAL(rc == 0);
	}
	op_cancel(ops, obj_cnt, start_obj);

	/*
	 * Launch the write request for first half of the remaining
	 * objects and wait for the 1st write for 2s and then go for
	 * canceling all the write operations
	 */
	obj_cnt = OBJECT_NR / 4;
	start_obj = OBJECT_NR / 2;
	for (i = 0; i < obj_cnt; ++i) {
		cur_obj = start_obj + i;
		st_op_launch(&ops[cur_obj], 1);
	}
	rc = st_op_wait(ops[start_obj],
			       M0_BITS(M0_OS_FAILED,
				       M0_OS_STABLE),
			       m0_time_from_now(2, 0));

	op_cancel(ops, obj_cnt, start_obj);

	/*
	 * Launch the write request for the second half of the remaining
	 * objects and wait for the 1st write for completion and then go
	 * for canceling all the write operations
	 */
	start_obj = OBJECT_NR - OBJECT_NR / 4;
	for (i = 0; i < obj_cnt; ++i) {
		cur_obj = start_obj + i;
		st_op_launch(&ops[cur_obj], 1);
	}
	rc = st_op_wait(ops[start_obj],
			       M0_BITS(M0_OS_FAILED,
				       M0_OS_STABLE),
			       M0_TIME_NEVER);
	ST_ASSERT_FATAL(rc == 0);

	op_cancel(ops, obj_cnt, start_obj);

	for (i = 0; i < OBJECT_NR; ++i) {
		/* fini and release */
		st_op_fini(ops[i]);
		st_op_free(ops[i]);
		st_entity_fini(&objs[i].ob_entity);
	}
	m0_bufvec_free(&data);
	m0_bufvec_free(&attr);
	m0_indexvec_free(&ext);
	mem_free(objs);
	mem_free(ids);
	mem_free(ops);
}

/**
 * Initialises the suite's environment.
 */
static int st_cancel_suite_init(void)
{
	int rc;

	/*
	 * Retrieve the uber realm. We don't need to open this,
	 * as realms are not actually implemented yet
	 */
	st_container_init(&st_cancel_container,
			      NULL, &M0_UBER_REALM,
			      st_get_instance());
	rc = st_cancel_container.co_realm.re_entity.en_sm.sm_rc;
	if (rc != 0) {
		console_printf("Failed to open uber realm\n");
		return M0_RC(rc);
	}
	layout_id = m0_client_layout_id(st_get_instance());
	return M0_RC(rc);
}

/**
 * Finalises the suite's environment.
 */
static int st_cancel_suite_fini(void)
{
	return 0;
}

struct st_suite st_suite_cancel = {
	.ss_name = "cancel_st",
	.ss_init = st_cancel_suite_init,
	.ss_fini = st_cancel_suite_fini,
	.ss_tests = {
		{ "cancel_while_write",
		  &m0_write_cancel },
		{ NULL, NULL }
	}
};

#undef M0_TRACE_SUBSYSTEM

/*
 *  Local variables:
 *  c-indentation-style: "K&R"
 *  c-basic-offset: 8
 *  tab-width: 8
 *  fill-column: 80
 *  scroll-step: 1
 *  End:
 */
