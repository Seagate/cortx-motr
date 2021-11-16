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

struct m0_container st_read_container;
extern struct m0_addb_ctx m0_addb_ctx;
static uint32_t unit_size = DEFAULT_PARGRP_UNIT_SIZE;
static uint64_t layout_id;

enum { MAX_READ_OID_NUM = 256 };

static int read_oid_num = 0;
static struct m0_uint128 read_oids[MAX_READ_OID_NUM];

static struct m0_uint128 read_oid_get(int idx)
{
	struct m0_uint128 oid;

	/* return non-existing oid */
	if (idx < 0 || idx >= MAX_READ_OID_NUM) {
		oid_get(&oid);
		return oid;
	}

	return read_oids[idx];
}

static int create_objs(int nr_objs)
{
	int               i;
	int               rc;
	struct m0_uint128 id;
	struct m0_op     *ops[1] = {NULL};
	struct m0_obj     obj;

	M0_CLIENT_THREAD_ENTER;

	for (i = 0; i < nr_objs; i++) {
		memset(&obj, 0, sizeof obj);
		ops[0] = NULL;

		/* get oid */
		oid_get(&id);

		/* Create an entity */
		st_obj_init(&obj,
			&st_read_container.co_realm,
			&id, layout_id);

		st_entity_create(NULL, &obj.ob_entity, &ops[0]);

		st_op_launch(ops, 1);

		/* Wait for op to complete */
		rc = st_op_wait(ops[0], M0_BITS(M0_OS_FAILED,
						       M0_OS_STABLE),
				       M0_TIME_NEVER);
		if (rc < 0)
			return rc;

		M0_ASSERT(rc == 0);
		M0_ASSERT(ops[0]->op_sm.sm_state == M0_OS_STABLE);
		M0_ASSERT(ops[0]->op_sm.sm_rc == 0);

		/* fini and release */
		st_op_fini(ops[0]);
		st_op_free(ops[0]);
		st_entity_fini(&obj.ob_entity);

		/* Save the created oid */
		read_oids[read_oid_num] = id;
		read_oid_num++;
	}

	return 0;
}

/*
 * Fill those pre-created objects with some value
 */
enum { CHAR_NUM = 6 };
static char pattern[CHAR_NUM] = {'C', 'L', 'O', 'V', 'I', 'S'};

static int write_objs(void)
{
	int                  i;
	int                  rc;
	int                  blk_cnt;
	int                  blk_size;
	char                 value;
	uint64_t             last_index;
	struct m0_uint128    id;
	struct m0_obj obj;
	struct m0_op *ops[1] = {NULL};
	struct m0_indexvec   ext;
	struct m0_bufvec     data;
	struct m0_bufvec     attr;

	M0_CLIENT_THREAD_ENTER;

	/* Prepare the data with 'value' */
	blk_cnt = 4;
	blk_size = unit_size;

	rc = m0_bufvec_alloc(&data, blk_cnt, blk_size);
	if (rc != 0)
		return rc;

	for (i = 0; i < blk_cnt; i++) {
		/*
		 * The pattern written to object has to match
		 * to those in read tests
		 */
		value = pattern[i % CHAR_NUM];
		memset(data.ov_buf[i], value, blk_size);
	}

	/* Prepare indexvec for write */
	rc = m0_bufvec_alloc(&attr, blk_cnt, 1);
	if(rc != 0) return rc;

	rc = m0_indexvec_alloc(&ext, blk_cnt);
	if (rc != 0) return rc;

	last_index = 0;
	for (i = 0; i < blk_cnt; i++) {
		ext.iv_index[i] = last_index ;
		ext.iv_vec.v_count[i] = blk_size;
		last_index += blk_size;

		/* we don't want any attributes */
		attr.ov_vec.v_count[i] = 0;
	}

	/* Write to objects one by one */
	for (i = 0; i < read_oid_num; i++) {
		memset(&obj, 0, sizeof obj);
		ops[0] = NULL;

		/* Get object id. */
		id = read_oids[i];

		/* Set the object entity we want to write */
		st_obj_init(
			&obj, &st_read_container.co_realm,
			&id, layout_id);

		st_entity_open(&obj.ob_entity);

		/* Create the write request */
		st_obj_op(&obj, M0_OC_WRITE, &ext, &data, &attr, 0, 0, &ops[0]);

		/* Launch the write request*/
		st_op_launch(ops, 1);

		/* wait */
		rc = st_op_wait(ops[0], M0_BITS(M0_OS_FAILED,
						       M0_OS_STABLE),
				       M0_TIME_NEVER);
		if (rc < 0) break;

		/* fini and release */
		st_op_fini(ops[0]);
		st_op_free(ops[0]);
		st_entity_fini(&obj.ob_entity);
	}
	m0_bufvec_free(&data);
	m0_bufvec_free(&attr);
	m0_indexvec_free(&ext);
	return rc;
}

/**
 * Examine if data is read correctly from an object which
 * is written in a known pattern (for example, all zero).
 */
void read_block_has_val(struct m0_bufvec *data, int block_idx, char val)
{
	int i;

	for(i = 0; i < data->ov_vec.v_count[block_idx]; i++)
		ST_ASSERT_FATAL(((char *)data->ov_buf[block_idx])[i] == val);
}

/**
 * Read a single block from an object, test the expected value is returned.
 */
static void read_one_block(void)
{
	int                   rc;
	struct m0_op         *ops[1] = {NULL};
	struct m0_obj         obj;
	struct m0_uint128     id;
	struct m0_indexvec    ext;
	struct m0_bufvec      data;
	struct m0_bufvec      attr;

	M0_CLIENT_THREAD_ENTER;

	/* we want to read 4K from the beginning of the object */
	rc = m0_indexvec_alloc(&ext, 1);
	if (rc != 0)
		return;

	ext.iv_index[0] = 0;
	ext.iv_vec.v_count[0] = DEFAULT_PARGRP_UNIT_SIZE;
	ST_ASSERT_FATAL(ext.iv_vec.v_nr == 1);

	/*
	 * this allocates 1 * 4K buffers for data, and initialises
	 * the bufvec for us.
	 */
	rc = m0_bufvec_alloc(&data, 1, DEFAULT_PARGRP_UNIT_SIZE);
	if (rc != 0)
		return;
	rc = m0_bufvec_alloc(&attr, 1, 1);
	if(rc != 0)
		return;

	/* we don't want any attributes */
	attr.ov_vec.v_count[0] = 0;

	/* Get the id of an existing object (written via m0t1fs). */
	id = read_oid_get(0);
	M0_SET0(&obj);
	st_obj_init(&obj, &st_read_container.co_realm,
			   &id, layout_id);

	st_entity_open(&obj.ob_entity);

	/* Create the read request */
	st_obj_op(&obj, M0_OC_READ, &ext, &data, &attr, 0, 0, &ops[0]);
	ST_ASSERT_FATAL(rc == 0);
	ST_ASSERT_FATAL(ops[0] != NULL);
	ST_ASSERT_FATAL(ops[0]->op_sm.sm_rc == 0);

	st_op_launch(ops, ARRAY_SIZE(ops));

	/* wait */
	rc = st_op_wait(ops[0], M0_BITS(M0_OS_FAILED,
					       M0_OS_STABLE),
			       M0_TIME_NEVER);
	ST_ASSERT_FATAL(rc == 0);
	ST_ASSERT_FATAL(ops[0]->op_sm.sm_state == M0_OS_STABLE);
	ST_ASSERT_FATAL(ops[0]->op_sm.sm_rc == 0);

	/* check if correct data is returned*/
	read_block_has_val(&data, 0, pattern[0]);

	st_op_fini(ops[0]);
	st_op_free(ops[0]);

	st_entity_fini(&obj.ob_entity);

	m0_indexvec_free(&ext);
	m0_bufvec_free(&data);
	m0_bufvec_free(&attr);
}

/**
 * Read multiple blocks from an object, test the expected value is returned.
 */
static void read_multiple_blocks(void)
{
	int                rc;
	struct m0_op      *ops[1] = {NULL};
	struct m0_obj      obj;
	struct m0_uint128  id;
	struct m0_indexvec ext;
	struct m0_bufvec   data;
	struct m0_bufvec   attr;

	M0_CLIENT_THREAD_ENTER;

	/* we want to read 3x 4K from the beginning of the object */
	rc = m0_indexvec_alloc(&ext, 3);
	if (rc != 0)
		return;

	/* Extent indexes and lengths */
	ext.iv_index[0] = 0;
	ext.iv_vec.v_count[0] = DEFAULT_PARGRP_UNIT_SIZE;
	ext.iv_index[1] = DEFAULT_PARGRP_UNIT_SIZE;
	ext.iv_vec.v_count[1] = DEFAULT_PARGRP_UNIT_SIZE;
	ext.iv_index[2] = 2*DEFAULT_PARGRP_UNIT_SIZE;
	ext.iv_vec.v_count[2] = DEFAULT_PARGRP_UNIT_SIZE;

	/*
	 * this allocates 3 * 4K buffers for data, and initialises
	 * the bufvec for us.
	 */
	rc = m0_bufvec_alloc(&data, 3, DEFAULT_PARGRP_UNIT_SIZE);
	if (rc != 0)
		return;
	rc = m0_bufvec_alloc(&attr, 3, 1);
	if(rc != 0)
		return;

	/* we don't want any attributes */
	attr.ov_vec.v_count[0] = 0;
	attr.ov_vec.v_count[1] = 0;
	attr.ov_vec.v_count[2] = 0;

	/* Get the id of an existing object. */
	M0_SET0(&obj);
	id = read_oid_get(0);
	st_obj_init(&obj, &st_read_container.co_realm,
			   &id, layout_id);

	st_entity_open(&obj.ob_entity);

	/* Create the read request */
	st_obj_op(&obj, M0_OC_READ, &ext, &data, &attr, 0, 0, &ops[0]);
	ST_ASSERT_FATAL(rc == 0);
	ST_ASSERT_FATAL(ops[0] != NULL);
	ST_ASSERT_FATAL(ops[0]->op_sm.sm_rc == 0);

	st_op_launch(ops, ARRAY_SIZE(ops));

	/* wait */
	rc = st_op_wait(ops[0], M0_BITS(M0_OS_FAILED,
					       M0_OS_STABLE),
			       M0_TIME_NEVER);
	ST_ASSERT_FATAL(rc == 0);
	ST_ASSERT_FATAL(ops[0]->op_sm.sm_state == M0_OS_STABLE);
	ST_ASSERT_FATAL(ops[0]->op_sm.sm_rc == 0);

	/* check if correct data is returned*/
	read_block_has_val(&data, 0, pattern[0]);
	read_block_has_val(&data, 1, pattern[1]);
	read_block_has_val(&data, 2, pattern[2]);


	st_op_fini(ops[0]);
	st_op_free(ops[0]);

	st_entity_fini(&obj.ob_entity);

	m0_indexvec_free(&ext);
	m0_bufvec_free(&data);
	m0_bufvec_free(&attr);
}


/**
 * Read multiple blocks from an object into aligned buffers,
 * test the expected value is returned.
 */
static void read_multiple_blocks_into_aligned_buffers(void)
{
	int                rc;
	struct m0_op      *ops[1] = {NULL};
	struct m0_obj      obj;
	struct m0_uint128  id;
	struct m0_indexvec ext;
	struct m0_bufvec   data;
	struct m0_bufvec   attr;
	void              *stashed_buffers[2];

	M0_CLIENT_THREAD_ENTER;

	/* we want to read 2 units starting with the second blockof  the object */
	rc = m0_indexvec_alloc(&ext, 2);
	if (rc != 0)
		return;

	/* Extent indexes and lengths */
	ext.iv_index[0] = unit_size;
	ext.iv_vec.v_count[0] = unit_size;
	ext.iv_index[1] = 2 * unit_size;
	ext.iv_vec.v_count[1] = unit_size;

	/*
	 * this allocates 2 * unit_size buffers for data, and initialises
	 * the bufvec for us.
	 */
	rc = m0_bufvec_alloc(&data, 2, unit_size);
	if (rc != 0)
		return;
	rc = m0_bufvec_alloc(&attr, 2, 1);
	if(rc != 0)
		return;

	/*
	 * Stash the m0_bufvec_alloc'd buffers, and replace them with
	 * aligned buffers.
	 */
	stashed_buffers[0] = data.ov_buf[0];
	data.ov_buf[0] = m0_alloc_aligned(4096, 12);
	ST_ASSERT_FATAL(data.ov_buf[0] != NULL);
	stashed_buffers[1] = data.ov_buf[1];
	data.ov_buf[1] = m0_alloc_aligned(4096, 12);
	ST_ASSERT_FATAL(data.ov_buf[1] != NULL);

	/* we don't want any attributes */
	attr.ov_vec.v_count[0] = 0;
	attr.ov_vec.v_count[1] = 0;

	/* Get the id of an existing object. */
	M0_SET0(&obj);
	id = read_oid_get(0);
	st_obj_init(&obj, &st_read_container.co_realm,
			   &id, layout_id);

	st_entity_open(&obj.ob_entity);

	/* Create the read request */
	st_obj_op(&obj, M0_OC_READ, &ext, &data, &attr, 0, 0, &ops[0]);
	ST_ASSERT_FATAL(rc == 0);
	ST_ASSERT_FATAL(ops[0] != NULL);
	ST_ASSERT_FATAL(ops[0]->op_sm.sm_rc == 0);

	st_op_launch(ops, ARRAY_SIZE(ops));

	/* wait */
	rc = st_op_wait(ops[0], M0_BITS(M0_OS_FAILED,
					       M0_OS_STABLE),
			       M0_TIME_NEVER);
	ST_ASSERT_FATAL(rc == 0);
	ST_ASSERT_FATAL(ops[0]->op_sm.sm_state == M0_OS_STABLE);
	ST_ASSERT_FATAL(ops[0]->op_sm.sm_rc == 0);

	/* check if correct data is returned*/
	read_block_has_val(&data, 0, pattern[1]);
	read_block_has_val(&data, 1, pattern[2]);

	st_op_fini(ops[0]);
	st_op_free(ops[0]);

	st_entity_fini(&obj.ob_entity);

	/* Swap the buffers back */
	m0_free_aligned(data.ov_buf[0], 4096, 12);
	data.ov_buf[0] = stashed_buffers[0];
	m0_free_aligned(data.ov_buf[1], 4096, 12);
	data.ov_buf[1] = stashed_buffers[1];

	m0_indexvec_free(&ext);
	m0_bufvec_free(&data);
	m0_bufvec_free(&attr);
}

static void read_objs_in_parallel(void)
{
	int                 i;
	int                 rc = 0;
	int                 nr_objs = read_oid_num;
	struct m0_uint128   id;
	struct m0_obj      *objs;
	struct m0_op      **ops;
	struct m0_indexvec *ext;
	struct m0_bufvec   *data;
	struct m0_bufvec   *attr;

	M0_CLIENT_THREAD_ENTER;

	/* Allocate memory object array */
	MEM_ALLOC_ARR(objs, nr_objs);
	ST_ASSERT_FATAL (objs != NULL)

	/* Setup bufvec, indexvec and ops for READs */
	MEM_ALLOC_ARR(ops, nr_objs);
	MEM_ALLOC_ARR(ext, nr_objs);
	MEM_ALLOC_ARR(data, nr_objs);
	MEM_ALLOC_ARR(attr, nr_objs);
	if (ops == NULL || ext == NULL || data == NULL || attr == NULL)
		goto CLEANUP;

	for (i = 0; i < nr_objs; i++) {
		ops[i] = NULL;
		memset(&ext[i], 0, sizeof ext[i]);
		memset(&data[i], 0, sizeof data[i]);
		memset(&attr[i], 0, sizeof attr[i]);
	}

	for (i = 0; i < nr_objs; i++) {
		if (m0_indexvec_alloc(&ext[i], 1) ||
		    m0_bufvec_alloc(&data[i], 1, unit_size) ||
		    m0_bufvec_alloc(&attr[i], 1, 1))
		{
			rc = -ENOMEM;
			goto CLEANUP;
		}

		ext[i].iv_index[0] = 0;
		ext[i].iv_vec.v_count[0] = unit_size;
		attr[i].ov_vec.v_count[0] = 0;
	}

	/* Create and launch write requests */
	for (i = 0; i < nr_objs; i++) {
		M0_SET0(&objs[i]);
		id = read_oid_get(i);

		st_obj_init(&objs[i], &st_read_container.co_realm,
				   &id, layout_id);

		st_entity_open(&objs[i].ob_entity);

		st_obj_op(&objs[i], M0_OC_READ,
			  &ext[i], &data[i], &attr[i], 0, 0, &ops[i]);
		if (ops[i] == NULL)
			break;
	}
	if (i != nr_objs) goto CLEANUP;

	st_op_launch(ops, nr_objs);

	/* Wait for write to finish */
	for (i = 0; i < nr_objs; i++) {
		rc = st_op_wait(ops[i],
			M0_BITS(M0_OS_FAILED,
				M0_OS_STABLE),
			M0_TIME_NEVER);
		ST_ASSERT_FATAL(rc == 0);
		ST_ASSERT_FATAL(
			ops[i]->op_sm.sm_state == M0_OS_STABLE);
		ST_ASSERT_FATAL(ops[i]->op_sm.sm_rc == 0);

		st_op_fini(ops[i]);
		st_op_free(ops[i]);
	}

CLEANUP:
	for (i = 0; i < nr_objs; i++) {
		if (ops[i] != NULL)
			st_entity_fini(&objs[i].ob_entity);
	}
	mem_free(objs);

	for (i = 0; i < nr_objs; i++) {
		if (ext != NULL && ext[i].iv_vec.v_nr != 0)
			m0_indexvec_free(&ext[i]);
		if (data != NULL && data[i].ov_buf != NULL)
			m0_bufvec_free(&data[i]);
		if (attr != NULL && attr[i].ov_buf != NULL)
			m0_bufvec_free(&attr[i]);
	}

	if (ops != NULL) mem_free(ops);
	if (ext != NULL) mem_free(ext);
	if (data != NULL) mem_free(data);
	if (attr != NULL) mem_free(attr);
}

/**
 * Initialises the read suite's environment.
 */
static int st_read_suite_init(void)
{
	int rc = 0;
	int nr_objs;

	/*
	 * Retrieve the uber realm. We don't need to open this,
	 * as realms are not actually implemented yet
	 */
	st_container_init(&st_read_container,
			      NULL, &M0_UBER_REALM,
			      st_get_instance());
	rc = st_read_container.co_realm.re_entity.en_sm.sm_rc;

	if (rc != 0) {
		console_printf("Failed to open uber realm\n");
		goto EXIT;
	}

	layout_id = m0_client_layout_id(st_get_instance());

	/*
	 * Create objects for tests including a few more used
	 * in read_after_delete test.
	 */
	nr_objs = 1;
	rc = create_objs(nr_objs + 1);
	if (rc < 0) {
		console_printf("Failed to create objects for READ tests\n");
		goto EXIT;
	}

	rc = write_objs();
	if (rc < 0)
		console_printf("Failed to write objects for READ tests\n");

EXIT:
	return rc;
}

/**
 * Finalises the read suite's environment.
 */
static int st_read_suite_fini(void)
{
	return 0;
}

struct st_suite st_suite_m0_read = {
	.ss_name = "m0_read_st",
	.ss_init = st_read_suite_init,
	.ss_fini = st_read_suite_fini,
	.ss_tests = {
		{ "read_one_block",
		  &read_one_block },
		{ "read_multiple_blocks",
		  &read_multiple_blocks } ,
		{ "read_multiple_blocks_into_aligned_buffers",
		  &read_multiple_blocks_into_aligned_buffers},
		{ "read_objs_in_parallel",
		  &read_objs_in_parallel },
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
