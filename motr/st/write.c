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

#include "motr/client.h"
#include "motr/st/st.h"
#include "motr/st/st_misc.h"
#include "motr/st/st_assert.h"

#include "lib/memory.h"

struct m0_container st_write_container;
extern struct m0_addb_ctx m0_addb_ctx;

enum { MAX_OPS = 16 };

/* Parity group aligned (in units)*/
enum {
	SMALL_OBJ_SIZE  = 1   * DEFAULT_PARGRP_DATA_SIZE,
	MEDIUM_OBJ_SIZE = 12  * DEFAULT_PARGRP_DATA_SIZE,
	LARGE_OBJ_SIZE  = 120 * DEFAULT_PARGRP_DATA_SIZE
};

static int create_obj(struct m0_uint128 *oid, int unit_size)
{
	int               rc = 0;
	struct m0_op     *ops[1] = {NULL};
	struct m0_obj     obj;
	struct m0_uint128 id;
	uint64_t          lid;

	/* Make sure everything in 'obj' is clean*/
	memset(&obj, 0, sizeof(obj));

	oid_get(&id);
	lid = m0_obj_unit_size_to_layout_id(unit_size);
	st_obj_init(&obj, &st_write_container.co_realm, &id, lid);

	st_entity_create(NULL, &obj.ob_entity, &ops[0]);
	if (ops[0] == NULL)
		return -ENOENT;

	st_op_launch(ops, ARRAY_SIZE(ops));

	rc = st_op_wait(
		ops[0], M0_BITS(M0_OS_FAILED, M0_OS_STABLE),
		m0_time_from_now(3,0));

	st_op_fini(ops[0]);
	st_op_free(ops[0]);
	st_entity_fini(&obj.ob_entity);

	*oid = id;
	return rc;
}

static int bufvec_cmp(struct m0_bufvec b1, struct m0_bufvec b2)
{
	int                     rc = 0;
	void                   *d1;
	void                   *d2;
	uint64_t                l1;
	uint64_t                l2;
	uint64_t                step;
	struct m0_bufvec_cursor c1;
	struct m0_bufvec_cursor c2;

	m0_bufvec_cursor_init(&c1, &b1);
	m0_bufvec_cursor_init(&c2, &b2);

	step = 0;
	while (!m0_bufvec_cursor_move(&c1, step)
	       && !m0_bufvec_cursor_move(&c2, step))
	{
		d1 = m0_bufvec_cursor_addr(&c1);
		d2 = m0_bufvec_cursor_addr(&c2);
		if (d1 == NULL || d2 == NULL) {
			rc = -EINVAL;
			break;
		}

		l1 = m0_bufvec_cursor_step(&c1);
		l2 = m0_bufvec_cursor_step(&c2);
		step = min_check(l1, l2);

		/* Check the contents */
		rc = memcmp(d1, d2, step);
		if (rc)
			break;
	}

	return rc;
}

/* Verify each stride one by one */
static int write_verify(struct m0_bufvec *data_w, struct m0_uint128 oid,
			int start, int stride, int unit_size, int nr_ops)
{
	int                i;
	int                rc;
	struct m0_obj      obj;
	struct m0_op      *ops_r[1] = {NULL};
	struct m0_indexvec ext_r;
	struct m0_bufvec   data_r;
	struct m0_bufvec   attr_r;
	uint64_t           lid;

	rc = 0;
	lid = m0_obj_unit_size_to_layout_id(unit_size);

	/* Setup bufvec, indexvec and ops for READs */
	for (i = 0; i < nr_ops; i++) {
		M0_SET0(&obj);
		ops_r[0] = NULL;

		st_obj_init(&obj, &st_write_container.co_realm,
				   &oid, lid);

		st_entity_open(&obj.ob_entity);

		if (m0_indexvec_alloc(&ext_r, 1) ||
		    m0_bufvec_alloc(&data_r, 1, unit_size * stride) ||
		    m0_bufvec_alloc(&attr_r, 1, 1))
		{
			rc = -ENOMEM;
			goto CLEANUP;
		}

		ext_r.iv_index[0] = unit_size * (start + i * stride);
		ext_r.iv_vec.v_count[0] = unit_size * stride;
		attr_r.ov_vec.v_count[0] = 0;

		/* Create and launch the read requests */
		st_obj_op(&obj, M0_OC_READ,
			  &ext_r, &data_r, &attr_r, 0, 0, &ops_r[0]);
		if (ops_r[0] == NULL)
			goto CLEANUP;

		st_op_launch(ops_r, 1);

		/* Wait for read to finish */
		rc = st_op_wait(ops_r[0],
			    M0_BITS(M0_OS_FAILED,
				    M0_OS_STABLE),
			    M0_TIME_NEVER);

		/* Compare the data */
		if (rc == 0 && ops_r[0]->op_sm.sm_state == M0_OS_STABLE)
			rc = bufvec_cmp(data_r, data_w[i]);

		st_op_fini(ops_r[0]);
		st_op_free(ops_r[0]);

CLEANUP:
		st_entity_fini(&obj.ob_entity);

		if (ext_r.iv_vec.v_nr != 0)
			m0_indexvec_free(&ext_r);
		if (data_r.ov_buf != NULL)
			m0_bufvec_free(&data_r);
		if (attr_r.ov_buf != NULL)
			m0_bufvec_free(&attr_r);

		if (rc != 0) {
			console_printf("write_verfiy: m0_op_wait failed\n");
			break;
		}
	}

	return rc;
}

/*
 * We issue 'nr_ops' of WRITES in one go.
 * stride: in units
 */
static int write_obj(struct m0_uint128 oid, int start,
		     int stride, int unit_size, int nr_ops, bool verify)
{
	int                 i;
	int                 rc = 0;
	uint64_t            lid;
	struct m0_obj       obj;
	struct m0_op      **ops_w;
	struct m0_indexvec *ext_w;
	struct m0_bufvec   *data_w;
	struct m0_bufvec   *attr_w;

	M0_CLIENT_THREAD_ENTER;

	if (nr_ops > MAX_OPS)
		return -EINVAL;

	/* Init obj */
	M0_SET0(&obj);
	lid = m0_obj_unit_size_to_layout_id(unit_size);
	st_obj_init(&obj, &st_write_container.co_realm, &oid, lid);

	st_entity_open(&obj.ob_entity);

	/* Setup bufvec, indexvec and ops for WRITEs */
	MEM_ALLOC_ARR(ops_w, nr_ops);
	MEM_ALLOC_ARR(ext_w, nr_ops);
	MEM_ALLOC_ARR(data_w, nr_ops);
	MEM_ALLOC_ARR(attr_w, nr_ops);
	if (ops_w == NULL || ext_w == NULL || data_w == NULL || attr_w == NULL)
		goto CLEANUP;

	for (i = 0; i < nr_ops; i++) {
		if (m0_indexvec_alloc(&ext_w[i], 1) ||
		    m0_bufvec_alloc(&data_w[i], 1, unit_size * stride) ||
		    m0_bufvec_alloc(&attr_w[i], 1, 1))
		{
			rc = -ENOMEM;
			goto CLEANUP;
		}

		memset(data_w[i].ov_buf[0], 'A', unit_size * stride);
		ext_w[i].iv_index[0] = unit_size * (start +  i * stride);
		ext_w[i].iv_vec.v_count[0] = unit_size * stride;
		attr_w[i].ov_vec.v_count[0] = 0;
	}

	/* Create and launch write requests */
	for (i = 0; i < nr_ops; i++) {
		ops_w[i] = NULL;
		st_obj_op(&obj, M0_OC_WRITE,
			  &ext_w[i], &data_w[i], &attr_w[i], 0, 0, &ops_w[i]);
		if (ops_w[i] == NULL)
			break;
	}
	if (i == 0) goto CLEANUP;

	st_op_launch(ops_w, nr_ops);

	/* Wait for write to finish */
	for (i = 0; i < nr_ops; i++) {
		rc = st_op_wait(ops_w[i],
			M0_BITS(M0_OS_FAILED,
				M0_OS_STABLE),
			M0_TIME_NEVER);

		st_op_fini(ops_w[i]);
		st_op_free(ops_w[i]);
	}

	/* 3. Verify the data written */
	if (!verify)
		goto CLEANUP;
	rc = write_verify(data_w, oid, start, stride, unit_size, nr_ops);

CLEANUP:
	st_entity_fini(&obj.ob_entity);

	for (i = 0; i < nr_ops; i++) {
		if (ext_w != NULL && ext_w[i].iv_vec.v_nr != 0)
			m0_indexvec_free(&ext_w[i]);
		if (data_w != NULL && data_w[i].ov_buf != NULL)
			m0_bufvec_free(&data_w[i]);
		if (attr_w != NULL && attr_w[i].ov_buf != NULL)
			m0_bufvec_free(&attr_w[i]);
	}

	if (ops_w != NULL) mem_free(ops_w);
	if (ext_w != NULL) mem_free(ext_w);
	if (data_w != NULL) mem_free(data_w);
	if (attr_w != NULL) mem_free(attr_w);

	return rc;
}

/*
 * Because of the current limitation of ST_ASSERT (can only be used
 * in top function), we duplicate for the following small/medium/large tests
 * to get more assert checks.
 */
#define write_objs(nr_objs, size, unit_size, verify) \
	do { \
		int               rc; \
		int               i; \
		int               j; \
		int               start = 0; \
		int               nr_units; \
		int               nr_pargrps; \
		struct m0_uint128 oid; \
                                                \
		nr_units = size / unit_size; \
		nr_pargrps = nr_units / DEFAULT_PARGRP_DATA_UNIT_NUM; \
                                                                      \
		for (i = 0; i < nr_objs; i++) { \
			rc = create_obj(&oid, unit_size); \
			ST_ASSERT_FATAL(rc == 0);\
                                                        \
			for (j = 0; j < nr_pargrps; j++) { \
				start = j * DEFAULT_PARGRP_DATA_UNIT_NUM; \
				rc = write_obj(oid, start, \
					DEFAULT_PARGRP_DATA_UNIT_NUM, \
					unit_size, 1, verify);\
				ST_ASSERT_FATAL(rc == 0);	\
			}						\
									\
			/* write the rest of data */			\
			start += DEFAULT_PARGRP_DATA_UNIT_NUM;		\
			if (start >= nr_units)				\
				continue;				\
									\
			rc = write_obj(oid, start, \
				nr_units - start, unit_size, 1, verify); \
			ST_ASSERT_FATAL(rc == 0); \
		} \
	} while(0)

static void write_small_objs(void)
{
	write_objs(10, SMALL_OBJ_SIZE, DEFAULT_PARGRP_UNIT_SIZE, true);
}

static void write_medium_objs(void)
{
	write_objs(10, MEDIUM_OBJ_SIZE, DEFAULT_PARGRP_UNIT_SIZE, false);
}

static void write_large_objs(void)
{
	write_objs(1, LARGE_OBJ_SIZE, DEFAULT_PARGRP_UNIT_SIZE, false);
}

static void write_with_layout_id(void)
{
	write_objs(1, LARGE_OBJ_SIZE, 16384, true);
}

static void write_pargrps_in_parallel_ops(void)
{
	int               i;
	int               rc;
	int               nr_rounds;
	int               nr_ops;
	struct m0_uint128 oid;

	nr_rounds = 4;

	/* Create an object */
	rc = create_obj(&oid, DEFAULT_PARGRP_UNIT_SIZE);
	ST_ASSERT_FATAL(rc == 0);

	/* We change the number of ops issued together in each round */
	nr_ops = 2^nr_rounds;
	for (i = 0; i < nr_rounds; i++) {
		rc = write_obj(oid, 0,
			       DEFAULT_PARGRP_DATA_UNIT_NUM,
			       DEFAULT_PARGRP_UNIT_SIZE,
			       nr_ops, true);
		ST_ASSERT_FATAL(rc == 0);

		nr_ops /= 2;
	}
}

static void write_pargrps_rmw(void)
{
	int               i;
	int               rc;
	int               start;
	int               strides[10] = {1, 3, 5, 7, 11, 13, 17, 19, 23, 29};
	struct m0_uint128 oid;

	/* Create an object */
	rc = create_obj(&oid, DEFAULT_PARGRP_UNIT_SIZE);
	ST_ASSERT_FATAL(rc == 0);

	/*
 	 * Write prime number of units to an object.
	 */
	start  = 0;
	for (i = 0; i < 10; i++) {
		rc = write_obj(oid, start, strides[i],
			       DEFAULT_PARGRP_UNIT_SIZE, 1, true);
		ST_ASSERT_FATAL(rc == 0);
		start += strides[i];
	}
}

/* Verify order of segments with nr_ent chunks which were passed to write
 * out of order */
static int write_verify_unorder(struct m0_bufvec *data_w, struct m0_uint128 oid,
				int start, int stride, int unit_size,
				int nr_ops, int nr_ent)
{
	int                i;
	int                j;
	int                rc;
	struct m0_obj      obj;
	struct m0_op      *ops_r[1] = {NULL};
	struct m0_indexvec ext_r;
	struct m0_bufvec   data_r;
	struct m0_bufvec   attr_r;
	uint64_t           lid;

	rc = 0;
	lid = m0_obj_unit_size_to_layout_id(unit_size);

	/* Setup bufvec, indexvec and ops for READs */
	for (i = 0; i < nr_ops; i++) {
		M0_SET0(&obj);
		ops_r[0] = NULL;

		st_obj_init(&obj, &st_write_container.co_realm, &oid, lid);

		st_entity_open(&obj.ob_entity);

		rc = m0_indexvec_alloc(&ext_r, nr_ent) ?:
			m0_bufvec_alloc(&data_r, nr_ent, unit_size * stride) ?:
			m0_bufvec_alloc(&attr_r, nr_ent, 1);

		if (rc != 0)
			goto CLEANUP;

		for (j = 0; j < nr_ent; j++) {
			ext_r.iv_index[j] = unit_size * (start + j * stride);
			ext_r.iv_vec.v_count[j] = unit_size * stride;
			attr_r.ov_vec.v_count[j] = 0;
		}

		/* Create and launch the read requests */
		st_obj_op(&obj, M0_OC_READ, &ext_r, &data_r, &attr_r, 0,
			  0, &ops_r[0]);
		if (ops_r[0] == NULL) {
			rc = -ENOMEM;
			goto CLEANUP;
		}

		st_op_launch(ops_r, 1);

		/* Wait for read to finish */
		rc = st_op_wait(ops_r[0],
			        M0_BITS(M0_OS_FAILED,
					M0_OS_STABLE),
				M0_TIME_NEVER);

		/* Compare the data */
		if (rc == 0 && ops_r[0]->op_sm.sm_state == M0_OS_STABLE)
			rc = bufvec_cmp(data_r, data_w[i]);

		st_op_fini(ops_r[0]);
		st_op_free(ops_r[0]);

CLEANUP:
		st_entity_fini(&obj.ob_entity);

		if (ext_r.iv_vec.v_nr != 0)
			m0_indexvec_free(&ext_r);
		if (data_r.ov_buf != NULL)
			m0_bufvec_free(&data_r);
		if (attr_r.ov_buf != NULL)
			m0_bufvec_free(&attr_r);

		if (rc != 0) {
			console_printf("write_verfiy: m0_op_wait failed\n");
			break;
		}
	}

	return rc;
}

#define INDEX(ivec, i) ((ivec)->iv_index[(i)])
#define COUNT(ivec, i) ((ivec)->iv_vec.v_count[(i)])

/*
 * Issue 'nr_ops' of 'nr_ent' WRITES with unordered data and index vectors.
 * stride: in units
 */
static int write_unordered_obj(struct m0_uint128 oid, int start, int stride,
			       int unit_size, int nr_ops, int nr_ent,
			       bool verify)
{
	int                 i;
	int                 j;
	int                 rc = 0;
	uint64_t            lid;
	struct m0_obj       obj;
	struct m0_op      **ops_w;
	struct m0_indexvec *ext_w;
	struct m0_bufvec   *data_w;
	struct m0_bufvec   *attr_w;

	M0_CLIENT_THREAD_ENTER;

	if (nr_ops > MAX_OPS)
		return -EINVAL;

	/* Init obj */
	M0_SET0(&obj);
	lid = m0_obj_unit_size_to_layout_id(unit_size);
	st_obj_init(&obj, &st_write_container.co_realm, &oid, lid);

	st_entity_open(&obj.ob_entity);

	/* Setup bufvec, indexvec and ops for WRITEs */
	MEM_ALLOC_ARR(ops_w, nr_ops);
	MEM_ALLOC_ARR(ext_w, nr_ops);
	MEM_ALLOC_ARR(data_w, nr_ops);
	MEM_ALLOC_ARR(attr_w, nr_ops);
	if (ops_w == NULL || ext_w == NULL || data_w == NULL || attr_w == NULL)
		goto CLEANUP;

	for (j = 0; j < nr_ops; j++) {
		if (m0_indexvec_alloc(&ext_w[j], nr_ent) ||
		    m0_bufvec_alloc(&data_w[j], nr_ent, unit_size * stride) ||
		    m0_bufvec_alloc(&attr_w[j], nr_ent, 1))
		{
			rc = -ENOMEM;
			goto CLEANUP;
		}

		for (i = 0; i < nr_ent; i++) {
			memset(data_w[j].ov_buf[i], 'A' + i,
			       unit_size * stride);
			ext_w[j].iv_index[i] = unit_size * (start + i * stride);
			ext_w[j].iv_vec.v_count[i] = unit_size * stride;
			attr_w[j].ov_vec.v_count[i] = 0;
		}

		/* Reorder index and data vectors */
		for (i = 0; i < nr_ent; i+=2) {
			M0_SWAP(INDEX(&ext_w[j], i), INDEX(&ext_w[j], i + 1));
			M0_SWAP(COUNT(&ext_w[j], i), COUNT(&ext_w[j], i + 1));
			memset(data_w[j].ov_buf[i], 'A' + i + 1,
			       unit_size * stride);
			memset(data_w[j].ov_buf[i + 1], 'A' + i,
			       unit_size * stride);
		}
	}

	/* Create and launch write requests */
	for (i = 0; i < nr_ops; i++) {
		ops_w[i] = NULL;
		st_obj_op(&obj, M0_OC_WRITE, &ext_w[i], &data_w[i], &attr_w[i],
			  0, 0, &ops_w[i]);
		if (ops_w[i] == NULL) {
			rc = -ENOMEM;
			break;
		}
	}
	if (i == 0)
		goto CLEANUP;

	st_op_launch(ops_w, nr_ops);

	/* Wait for write to finish */
	for (i = 0; i < nr_ops; i++) {
		rc = st_op_wait(ops_w[i],
				M0_BITS(M0_OS_FAILED,
					M0_OS_STABLE),
				M0_TIME_NEVER);

		st_op_fini(ops_w[i]);
		st_op_free(ops_w[i]);
	}

	for (j = 0; j < nr_ops; j++)
		for (i = 0; i < nr_ent; i+=2) {
			memset(data_w[j].ov_buf[i], 'A' + i,
			       unit_size * stride);
			memset(data_w[j].ov_buf[i + 1], 'A' + i + 1,
			       unit_size * stride);
		}
	/* 3. Verify the data written */
	if (!verify)
		goto CLEANUP;

	rc = write_verify_unorder(&data_w[0], oid, start, stride, unit_size,
			          nr_ops, nr_ent);

CLEANUP:
	st_entity_fini(&obj.ob_entity);

	for (i = 0; i < nr_ops; i++) {
		if (ext_w != NULL && ext_w[i].iv_vec.v_nr != 0)
			m0_indexvec_free(&ext_w[i]);
		if (data_w != NULL && data_w[i].ov_buf != NULL)
			m0_bufvec_free(&data_w[i]);
		if (attr_w != NULL && attr_w[i].ov_buf != NULL)
			m0_bufvec_free(&attr_w[i]);
	}

	if (ops_w != NULL) mem_free(ops_w);
	if (ext_w != NULL) mem_free(ext_w);
	if (data_w != NULL) mem_free(data_w);
	if (attr_w != NULL) mem_free(attr_w);

	return rc;
}

/**
 * write unordered parity group of data to an object then read and verify
 * if the order was corrected.
 */
static void write_unorder_pargrp(void)
{
	int               rc;
	int               nr_segs;
	int               nr_ops;
	int               start;
	int               stride;
	struct m0_uint128 oid;

	/* 1. Create an object */
	rc = create_obj(&oid, DEFAULT_PARGRP_UNIT_SIZE);
	ST_ASSERT_FATAL(rc == 0);

	/*
	 * 2. We want to write/read one group with nr_segs chunks
	 * from the beginning of the object
	 */
	start  = 0;
	stride = 100; /* or DEFAULT_PARGRP_DATA_UNIT_NUM */;
	nr_segs = 10;
	nr_ops = 1;
	rc = write_unordered_obj(oid, start, stride, DEFAULT_PARGRP_UNIT_SIZE,
				 nr_ops, nr_segs, true);
	ST_ASSERT_FATAL(rc == 0);
}

/**
 * write a number of parity groups of data to an object then read.
 */
static void write_pargrps(void)
{
	int               i;
	int               rc;
	int               nr_rounds;
	int               start;
	int               stride;
	struct m0_uint128 oid;

	/* 1. Create an object */
	rc = create_obj(&oid, DEFAULT_PARGRP_UNIT_SIZE);
	ST_ASSERT_FATAL(rc == 0);

	/*
	 * 2. We want to write/read one group from the beginning
	 *    of the object
	 */
	start  = 0;
	stride = 100; /* or DEFAULT_PARGRP_DATA_UNIT_NUM */;
	nr_rounds = 2;
	for (i = 0; i < nr_rounds; i++) {
		rc = write_obj(oid, start, stride,
			       DEFAULT_PARGRP_UNIT_SIZE, 1, true);
		ST_ASSERT_FATAL(rc == 0);
		start += stride;
	}
}

/* Initialises the Client environment.*/
static int st_write_init(void)
{
	int rc = 0;

#if 0
	st_obj_prev_trace_level = m0_trace_level;
	m0_trace_level = M0_DEBUG;
#endif

	/*
	 * Retrieve the uber realm. We don't need to open this,
	 * as realms are not actually implemented yet
	 */
	st_container_init(&st_write_container,
			      NULL, &M0_UBER_REALM,
			      st_get_instance());
	rc = st_write_container.co_realm.re_entity.en_sm.sm_rc;

	if (rc != 0)
		console_printf("Failed to open uber realm\n");

	(void)m0_client_layout_id(st_get_instance());
	return rc;
}

/* Finalises the Client environment.*/
static int st_write_fini(void)
{
	//m0_trace_level = st_obj_prev_trace_level;
	return 0;
}

struct st_suite st_suite_m0_write = {
	.ss_name = "m0_write_st",
	.ss_init = st_write_init,
	.ss_fini = st_write_fini,
	.ss_tests = {
		{ "write_unorder_pargrp", &write_unorder_pargrp},
		{ "write_pargrps", &write_pargrps},
		{ "write_pargrps_rmw", &write_pargrps_rmw},
		{ "write_pargrps_in_parallel_ops", &write_pargrps_in_parallel_ops},
		{ "write_small_objs",  &write_small_objs},
		{ "write_medium_objs", &write_medium_objs},
		{ "write_large_objs", &write_large_objs},
		{ "write_with_layout_id", &write_with_layout_id},
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
