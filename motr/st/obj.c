/* -*- C -*- */
/*
 * Copyright (c) 2016-2020 Seagate Technology LLC and/or its Affiliates
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
#include "lib/types.h"
#include "lib/errno.h"    /* ENOENT */
#include "lib/finject.h"  /* m0_fi_enable_once */

#ifndef __KERNEL__
#include <stdlib.h>
#include <unistd.h>
#else
#include <linux/delay.h>
#endif

static struct m0_uint128 test_id;
struct m0_container st_obj_container;
static uint64_t layout_id;

/**
 * Creates an object.
 *
 * @remark This test does not call op_wait().
 */
static void obj_create_simple(void)
{
	struct m0_op           *ops[] = {NULL};
	struct m0_obj          *obj;
	struct m0_uint128       id;
	int                     rc;

	MEM_ALLOC_PTR(obj);
	ST_ASSERT_FATAL(obj != NULL);

	/* Initialise ids. */
	oid_get(&id);

	st_obj_init(obj, &st_obj_container.co_realm,
			   &id, layout_id);
	rc = st_entity_create(NULL, &obj->ob_entity, &ops[0]);
	ST_ASSERT_FATAL(rc == 0);
	ST_ASSERT_FATAL(ops[0] != NULL);
	ST_ASSERT_FATAL(ops[0]->op_sm.sm_rc == 0);

	st_op_launch(ops, ARRAY_SIZE(ops));

	rc = st_op_wait(ops[0], M0_BITS(M0_OS_FAILED,
					       M0_OS_STABLE),
				m0_time_from_now(5,0));
	ST_ASSERT_FATAL(rc == 0);
	ST_ASSERT_FATAL(ops[0]->op_sm.sm_state == M0_OS_STABLE);

	st_op_fini(ops[0]);
	st_op_free(ops[0]);
	st_entity_fini(&obj->ob_entity);
	mem_free(obj);
}

/*
 * Test error handling of object operation using fault injection.
 */
static void obj_create_error_handling(void)
{
	struct m0_op           *ops[] = {NULL};
	struct m0_obj          *obj;
	struct m0_uint128       id;
	int                     rc;

	MEM_ALLOC_PTR(obj);
	ST_ASSERT_FATAL(obj != NULL);

	/* Initialise ids. */
	oid_get(&id);

	st_obj_init(obj, &st_obj_container.co_realm,
			   &id, layout_id);
	rc = st_entity_create(NULL, &obj->ob_entity, &ops[0]);
	ST_ASSERT_FATAL(rc == 0);
	ST_ASSERT_FATAL(ops[0] != NULL);
	ST_ASSERT_FATAL(ops[0]->op_sm.sm_rc == 0);

	m0_fi_enable("cob_ios_prepare", "invalid_rpc_session");
	st_op_launch(ops, ARRAY_SIZE(ops));
	m0_fi_disable("cob_ios_prepare", "invalid_rpc_session");

	st_op_wait(ops[0], M0_BITS(M0_OS_FAILED,
					  M0_OS_STABLE),
					  m0_time_from_now(5,0));
	/*
	 * As invalid_rpc_session faults are injected during
	 * launching an op above and because of it op->op_sm.sm_rc
	 * is expected to be EINVAL.
	 */

	ST_ASSERT_FATAL(ops[0]->op_sm.sm_rc == -EINVAL);
	ST_ASSERT_FATAL(ops[0]->op_sm.sm_state != M0_OS_STABLE);

	st_op_fini(ops[0]);
	st_op_free(ops[0]);
	st_entity_fini(&obj->ob_entity);
	mem_free(obj);
}
/**
 * Tries to open an object that does not exist.
 */
static void obj_open_non_existent(void)
{
	int                     rc;
	struct m0_op           *ops[1] = {NULL};
	struct m0_obj          *obj;
	struct m0_uint128       id;

	MEM_ALLOC_PTR(obj);

	/* Initialise the id. */
	oid_get(&id);

	st_obj_init(obj, &st_obj_container.co_realm,
			   &id, layout_id);

	/* Try opening a non-existent object. */
	rc = m0_entity_open(&obj->ob_entity, &ops[0]);
	ST_ASSERT_FATAL(rc == 0);
	ST_ASSERT_FATAL(ops[0] != NULL);
	ST_ASSERT_FATAL(ops[0]->op_sm.sm_rc == 0);

	st_op_launch(ops, ARRAY_SIZE(ops));
	rc = st_op_wait(ops[0],
			       M0_BITS(M0_OS_FAILED,
				       M0_OS_STABLE),
			       m0_time_from_now(5,0));
	ST_ASSERT_FATAL(rc == 0);
	ST_ASSERT_FATAL(ops[0]->op_sm.sm_state == M0_OS_STABLE);
	ST_ASSERT_FATAL(ops[0]->op_sm.sm_rc == 0);
	ST_ASSERT_FATAL(ops[0]->op_rc == -ENOENT);
	ST_ASSERT_FATAL(obj->ob_entity.en_sm.sm_state ==
			       M0_ES_INIT);

	st_op_fini(ops[0]);
	st_op_free(ops[0]);
	st_entity_fini(&obj->ob_entity);
	mem_free(obj);
}

/**
 * Uses client to create multiple objects. All the operations are expected
 * to complete.
 *
 * @remarks Every object and operation used by this test are correctly
 * finalised and released.
 */
static void obj_create_multiple_objects(void)
{
	enum { CREATE_MULTIPLE_N_OBJS = 20 };
	uint32_t                i;
	struct m0_op           *ops[CREATE_MULTIPLE_N_OBJS] = {NULL};
	struct m0_obj         **objs = NULL;
	struct m0_uint128       id[CREATE_MULTIPLE_N_OBJS];
	int                     rc;

	MEM_ALLOC_ARR(objs, CREATE_MULTIPLE_N_OBJS);
	for (i = 0; i < CREATE_MULTIPLE_N_OBJS; ++i) {
		MEM_ALLOC_PTR(objs[i]);
		ST_ASSERT_FATAL(objs[i] != NULL);
	}

	/* Initialise ids. */
	for (i = 0; i < CREATE_MULTIPLE_N_OBJS; ++i) {
		oid_get(id + i);
	}

	/* Create different objects. */
	for (i = 0; i < CREATE_MULTIPLE_N_OBJS; ++i) {
		st_obj_init(objs[i], &st_obj_container.co_realm,
				   &id[i], layout_id);
		rc = st_entity_create(NULL, &objs[i]->ob_entity, &ops[i]);
		ST_ASSERT_FATAL(rc == 0);
		ST_ASSERT_FATAL(ops[i] != NULL);
		ST_ASSERT_FATAL(ops[i]->op_sm.sm_rc == 0);
	}

	st_op_launch(ops, ARRAY_SIZE(ops));

	/* We wait for each op. */
	for (i = 0; i < CREATE_MULTIPLE_N_OBJS; ++i) {
		/*
		 * Note: (Sining) M0_TIME_NEVER is not used here in order
		 * to test timeout handling (emulate a client application)
		 * in release_op(st_assert.c).
		 */
		rc = st_op_wait(ops[i], M0_BITS(M0_OS_FAILED,
						       M0_OS_STABLE),
				       m0_time_from_now(5,0));
		ST_ASSERT_FATAL(rc == 0);
	}

	/* All correctly created. */
	for (i = 0; i < CREATE_MULTIPLE_N_OBJS; ++i) {
		ST_ASSERT_FATAL(
			ops[i]->op_sm.sm_state == M0_OS_STABLE);
	}

	/* Clean up. */
	for (i = 0; i < CREATE_MULTIPLE_N_OBJS; ++i) {
		st_op_fini(ops[i]);
		st_op_free(ops[i]);
		st_entity_fini(&objs[i]->ob_entity);
		mem_free(objs[i]);
	}

	mem_free(objs);
}

static void obj_create_on_multiple_pools(void)
{
	int                   i;
	int                   j;
	int                   rc;
	int                   nr_pools = 2;
	int                   nr_objs_per_pool = 10;
	struct m0_op         *ops[1] = {NULL};
	struct m0_obj        *obj;
	struct m0_uint128     id;
	struct m0_fid         pool_fids[2];
	struct m0_fid        *pool;

	/*
	 * Must be the pool fid set
	 * in m0t1fs/linux_kernel/st/m0t1fs_common_inc.sh::build_conf().
	 */
	pool_fids[0].f_container = 0x6f00000000000001;
	pool_fids[0].f_key       = 0x9;
	pool_fids[1].f_container = 0x6f0000000000000a;
	pool_fids[1].f_key       = 0x1;

	for (i = 0; i < nr_pools; i++) {
		for (j = 0; j < nr_objs_per_pool; j++) {
			oid_get(&id);

			MEM_ALLOC_PTR(obj);
			M0_SET0(obj);
			pool = pool_fids + i;

			st_obj_init(
				obj, &st_obj_container.co_realm,
				&id, layout_id);

			/* Create the entity */
			ops[0] = NULL;
			rc = st_entity_create(pool, &obj->ob_entity,
						     &ops[0]);
			ST_ASSERT_FATAL(rc == 0);
			ST_ASSERT_FATAL(ops[0] != NULL);
			ST_ASSERT_FATAL(ops[0]->op_sm.sm_rc == 0);

			st_op_launch(ops, ARRAY_SIZE(ops));
			rc = st_op_wait(ops[0],
					       M0_BITS(M0_OS_FAILED,
						       M0_OS_STABLE),
					       m0_time_from_now(5,0));
			ST_ASSERT_FATAL(rc == 0);
			ST_ASSERT_FATAL(ops[0]->op_sm.sm_state ==
					       M0_OS_STABLE);

			st_op_fini(ops[0]);
			st_op_free(ops[0]);
			st_entity_fini(&obj->ob_entity);
			mem_free(obj);
		}
	}
}

/**
 * Creates an object and then issues a new op. to delete it straightaway.
 */
static void obj_create_then_delete(void)
{
	int                     i;
	int                     rc;
	int                     rounds = 20;
	struct m0_op           *ops_c[1] = {NULL};
	struct m0_op           *ops_d[1] = {NULL};
	struct m0_obj          *obj;
	struct m0_uint128       id;

	MEM_ALLOC_PTR(obj);

	/* initialise the id */
	oid_get(&id);

	for (i = 0; i < rounds; i++) {
		M0_SET0(obj);
		st_obj_init(obj, &st_obj_container.co_realm,
				   &id, layout_id);

		/* Create the entity */
		rc = st_entity_create(NULL, &obj->ob_entity, &ops_c[0]);
		ST_ASSERT_FATAL(rc == 0);
		ST_ASSERT_FATAL(ops_c[0] != NULL);
		ST_ASSERT_FATAL(ops_c[0]->op_sm.sm_rc == 0);

		st_op_launch(ops_c, ARRAY_SIZE(ops_c));
		rc = st_op_wait(ops_c[0], M0_BITS(M0_OS_FAILED,
							 M0_OS_STABLE),
				       m0_time_from_now(5,0));
		ST_ASSERT_FATAL(rc == 0);
		ST_ASSERT_FATAL(ops_c[0]->op_sm.sm_state == M0_OS_STABLE);

		/* Delete the entity */
		st_entity_delete(&obj->ob_entity, &ops_d[0]);
		ST_ASSERT_FATAL(ops_d[0] != NULL);
		ST_ASSERT_FATAL(ops_d[0]->op_sm.sm_rc == 0);
		st_op_launch(ops_d, ARRAY_SIZE(ops_d));
		rc = st_op_wait(ops_d[0], M0_BITS(M0_OS_FAILED,
							 M0_OS_STABLE),
				       m0_time_from_now(5,0));
		ST_ASSERT_FATAL(rc == 0);
		ST_ASSERT_FATAL(ops_d[0]->op_sm.sm_state == M0_OS_STABLE);

		st_op_fini(ops_d[0]);
		st_op_fini(ops_c[0]);
	}

	st_op_free(ops_d[0]);
	st_op_free(ops_c[0]);
	st_entity_fini(&obj->ob_entity);
	mem_free(obj);
}

/**
 * Arbitrarily creates and deletes objects within the same set of objects.
 * Launches different groups of operations each time.
 */
static void obj_delete_multiple(void)
{
	uint32_t                n_objs = 5;
	uint32_t                rounds = 20;
	uint32_t                max_ops = 4; /* max. ops in one launch() */
	struct m0_uint128      *ids = NULL;
	struct m0_obj          *objs = NULL;
	struct m0_op          **ops = NULL;
	bool                   *obj_used = NULL;
	bool                   *obj_exists = NULL;
	bool                   *already_chosen = NULL;
	uint32_t                i;
	uint32_t                j;
	uint32_t                idx;
	uint32_t                n_ops;
	int                     rc;

	/* Parameters are correct. */
	ST_ASSERT_FATAL(n_objs >= max_ops);

	MEM_ALLOC_ARR(ids, n_objs);
	ST_ASSERT_FATAL(ids != NULL);
	MEM_ALLOC_ARR(objs, n_objs);
	ST_ASSERT_FATAL(objs != NULL);
	MEM_ALLOC_ARR(obj_used, n_objs);
	ST_ASSERT_FATAL(obj_used != NULL);
	MEM_ALLOC_ARR(obj_exists, n_objs);
	ST_ASSERT_FATAL(obj_exists != NULL);
	MEM_ALLOC_ARR(already_chosen, n_objs);
	ST_ASSERT_FATAL(already_chosen != NULL);

	MEM_ALLOC_ARR(ops, max_ops);
	ST_ASSERT_FATAL(ops != NULL);
	for (i = 0; i < max_ops; i++)
		ops[i] = NULL;

	/* Initialise the objects. Closed set. */
	for (i = 0; i < n_objs; ++i)
		oid_get(ids + i);

	memset(obj_exists, 0, n_objs*sizeof(obj_exists[0]));
	/* Repeat several rounds. */
	for (i = 0; i < rounds; ++i) {
		/* Each round, a different number of ops in the same launch. */
		n_ops = generate_random(max_ops) + 1;
		memset(already_chosen, 0, n_objs*sizeof(already_chosen[0]));

		/* Set the ops. */
		for (j = 0; j < n_ops; ++j) {
			/* Pick n_ops objects. */
			do {
				idx = generate_random(n_objs);
			} while(already_chosen[idx]);
			already_chosen[idx] = true;
			obj_used[idx] = true;

			M0_SET0(&objs[idx]);
			st_obj_init(&objs[idx],
				&st_obj_container.co_realm,
				&ids[idx], layout_id);
			if (obj_exists[idx]) {
				st_entity_open(&objs[idx].ob_entity);
				rc = st_entity_delete(
					&objs[idx].ob_entity, &ops[j]);
				obj_exists[idx] = false;
				ST_ASSERT_FATAL(ops[j] != NULL);
			} else {
				rc = st_entity_create(
					NULL, &objs[idx].ob_entity, &ops[j]);
				obj_exists[idx] = true;
				ST_ASSERT_FATAL(ops[j] != NULL);
			}
			ST_ASSERT_FATAL(rc == 0);
			ST_ASSERT_FATAL(ops[j] != NULL);
			ST_ASSERT_FATAL(ops[j]->op_sm.sm_rc == 0);
		}

		/* Launch and check. */
		st_op_launch(ops, n_ops);
		for (j = 0; j < n_ops; ++j) {
			rc = st_op_wait(ops[j],
					       M0_BITS(M0_OS_FAILED,
						       M0_OS_STABLE),
					       M0_TIME_NEVER);
					       //m0_time_from_now(5,0));
			ST_ASSERT_FATAL(rc == 0);
			ST_ASSERT_FATAL(ops[j]->op_sm.sm_state ==
					 M0_OS_STABLE);
			st_op_fini(ops[j]);
		}
	}

	/* Clean up. */
	for (i = 0; i < max_ops; ++i) {
		if (ops[i] != NULL)
			st_op_free(ops[i]);
	}

	/* Clean up. */
	for (i = 0; i < n_objs; ++i) {
		if (obj_used[i] == true)
			st_entity_fini(&objs[i].ob_entity);
	}

	mem_free(ops);
	mem_free(already_chosen);
	mem_free(obj_exists);
	mem_free(objs);
	mem_free(ids);
}


/**
 * Launches a create object operation but does not call m0_op_wait().
 */
static void obj_no_wait(void)
{
	struct m0_op           *ops[] = {NULL};
	struct m0_obj          *obj;
	struct m0_uint128       id;
	int                     rc;

	MEM_ALLOC_PTR(obj);
	ST_ASSERT_FATAL(obj != NULL);

	/* Initialise ids. */
	oid_get(&id);

	st_obj_init(obj, &st_obj_container.co_realm,
			   &id, layout_id);
	rc = st_entity_create(NULL, &obj->ob_entity, &ops[0]);
	ST_ASSERT_FATAL(rc == 0);
	ST_ASSERT_FATAL(ops[0] != NULL);
	ST_ASSERT_FATAL(ops[0]->op_sm.sm_rc == 0);

	st_op_launch(ops, ARRAY_SIZE(ops));

	/* A call to m0_op_wait is not strictly required. */
#ifndef __KERNEL__
	sleep(5);
#else
	msleep(5000);
#endif

	ST_ASSERT_FATAL(rc == 0);
	ST_ASSERT_FATAL(ops[0]->op_sm.sm_state == M0_OS_STABLE);

	st_op_fini(ops[0]);
	st_op_free(ops[0]);
	st_entity_fini(&obj->ob_entity);
	mem_free(obj);
}

/**
 * m0_op_wait() times out.
 */
static void obj_wait_no_launch(void)
{
	struct m0_op           *ops[] = {NULL};
	struct m0_obj          *obj;
	struct m0_uint128       id;
	int                     rc;

	MEM_ALLOC_PTR(obj);
	ST_ASSERT_FATAL(obj != NULL);

	/* Initialise ids. */
	oid_get(&id);

	st_obj_init(obj, &st_obj_container.co_realm,
			   &id, layout_id);
	rc = st_entity_create(NULL, &obj->ob_entity, &ops[0]);
	ST_ASSERT_FATAL(rc == 0);
	ST_ASSERT_FATAL(ops[0] != NULL);
	ST_ASSERT_FATAL(ops[0]->op_sm.sm_rc == 0);

	/* The operation is not launched so the state should not change. */

	rc = st_op_wait(ops[0], M0_BITS(M0_OS_FAILED,
					       M0_OS_STABLE),
				m0_time_from_now(3,0));
	ST_ASSERT_FATAL(rc == -ETIMEDOUT);
	ST_ASSERT_FATAL(ops[0]->op_sm.sm_state ==
			       M0_OS_INITIALISED);

	st_op_fini(ops[0]);
	st_op_free(ops[0]);
	st_entity_fini(&obj->ob_entity);
	mem_free(obj);
}

/**
 * Launches a create object operation and waits() twice on it.
 */
static void obj_wait_twice(void)
{
	struct m0_op           *ops[] = {NULL};
	struct m0_obj          *obj;
	struct m0_uint128       id;
	int                     rc;

	MEM_ALLOC_PTR(obj);
	ST_ASSERT_FATAL(obj != NULL);

	/* Initialise ids. */
	oid_get(&id);

	st_obj_init(obj, &st_obj_container.co_realm,
			   &id, layout_id);
	rc = st_entity_create(NULL, &obj->ob_entity, &ops[0]);
	ST_ASSERT_FATAL(rc == 0);
	ST_ASSERT_FATAL(ops[0] != NULL);
	ST_ASSERT_FATAL(ops[0]->op_sm.sm_rc == 0);

	st_op_launch(ops, ARRAY_SIZE(ops));

	/* Calling op_wait() several times should have an innocuous effect*/
	rc = st_op_wait(ops[0], M0_BITS(M0_OS_FAILED,
					       M0_OS_STABLE),
				m0_time_from_now(5,0));

	ST_ASSERT_FATAL(rc == 0);
	ST_ASSERT_FATAL(ops[0]->op_sm.sm_state == M0_OS_STABLE);

	rc = st_op_wait(ops[0], M0_BITS(M0_OS_FAILED,
					       M0_OS_STABLE),
				m0_time_from_now(5,0));
	ST_ASSERT_FATAL(rc == 0);
	ST_ASSERT_FATAL(ops[0]->op_sm.sm_state == M0_OS_STABLE);

	st_op_fini(ops[0]);
	st_op_free(ops[0]);
	st_entity_fini(&obj->ob_entity);
	mem_free(obj);
}

static void mock_op_cb_stable(struct m0_op *op)
{
	int *val;

	val = (int *)op->op_datum;
	*val = 'S';
}

static void mock_op_cb_failed(struct m0_op *op)
{
	int *val;

	val = (int *)op->op_datum;
	*val = 'F';
}

/**
 * m0_op_setup() for entity op.
 */
static void obj_op_setup(void)
{
	int                      rc;
	int                      val = 0;
	struct m0_uint128        id;
	struct m0_obj           *obj;
	struct m0_op            *ops[] = {NULL};
	struct m0_op_ops         cbs;

	MEM_ALLOC_PTR(obj);
	ST_ASSERT_FATAL(obj != NULL);

	/*
	 * Set callbacks for operations.
	 * oop_executed is not supported (see comments in motr/client.h)
	 */
	cbs.oop_executed = NULL;
	cbs.oop_stable = mock_op_cb_stable;
	cbs.oop_failed = mock_op_cb_failed;

	/* Initilise an CREATE op. */
	oid_get(&id);
	st_obj_init(obj, &st_obj_container.co_realm,
			   &id, layout_id);
	rc = st_entity_create(NULL, &obj->ob_entity, &ops[0]);
	ST_ASSERT_FATAL(rc == 0);
	ST_ASSERT_FATAL(ops[0] != NULL);
	ST_ASSERT_FATAL(ops[0]->op_sm.sm_rc == 0);

	/* Test callback functions for OS_STABLE*/
	ops[0]->op_datum = (void *)&val;
	m0_op_setup(ops[0], &cbs, 0);
	st_op_launch(ops, ARRAY_SIZE(ops));
	rc = st_op_wait(ops[0], M0_BITS(M0_OS_FAILED,
					       M0_OS_STABLE),
				m0_time_from_now(5,0));
	if (rc == 0) {
		ST_ASSERT_FATAL(ops[0]->op_sm.sm_state ==
				       M0_OS_STABLE);
		ST_ASSERT_FATAL(val == 'S');
	}

	st_op_fini(ops[0]);
	st_op_free(ops[0]);
	st_entity_fini(&obj->ob_entity);

	/* Test callback function for OS_FAILED*/
	/* @TODO: The following procedure is invalid since EO_DELETE cannot be
	 * done from ES_INIT state anymore. Need to revisit this later. */
#if 0
	memset(obj, 0, sizeof *obj);
	oid_get(&id);

	st_obj_init(obj, &st_obj_container.co_realm,
			   &id, layout_id);
	rc = st_entity_delete(&obj->ob_entity, &ops[0]);
	ST_ASSERT_FATAL(rc == 0);

	ops[0]->op_datum = (void *)&val;
	m0_op_setup(ops[0], &cbs, 0);
	st_op_launch(ops, ARRAY_SIZE(ops));
	rc = st_op_wait(ops[0], M0_BITS(M0_OS_FAILED,
					       M0_OS_STABLE),
				m0_time_from_now(5,0));
	if (rc == 0) {
		ST_ASSERT_FATAL(ops[0]->op_sm.sm_state
				       == M0_OS_FAILED);
		ST_ASSERT_FATAL(val == 'F');
	}

	st_op_fini(ops[0]);
	st_op_free(ops[0]);
	st_entity_fini(&obj->ob_entity);
#endif

	/* End of the test */
	mem_free(obj);
}

/**
 * Initialises the obj suite's environment.
 */
static int st_obj_suite_init(void)
{
	int rc = 0;

	/*
	 * Retrieve the uber realm. We don't need to open this,
	 * as realms are not actually implemented yet
	 */
	m0_container_init(&st_obj_container, NULL,
				 &M0_UBER_REALM,
				 st_get_instance());
	rc = st_obj_container.co_realm.re_entity.en_sm.sm_rc;

	if (rc != 0)
		console_printf("Failed to open uber realm\n");

	test_id = M0_ID_APP;
	test_id.u_lo += generate_random(0xffff);

	layout_id = m0_client_layout_id(st_get_instance());
	return rc;
}

/**
 * Finalises the obj suite's environment.
 */
static int st_obj_suite_fini(void)
{
	return 0;
}

struct st_suite st_suite_obj = {
	.ss_name = "obj_st",
	.ss_init = st_obj_suite_init,
	.ss_fini = st_obj_suite_fini,
	.ss_tests = {
		{ "obj_create_simple",
		  &obj_create_simple},
		{ "obj_create_error_handling",
		  &obj_create_error_handling},
		{ "obj_open_non_existent",
		  &obj_open_non_existent},
		{ "obj_create_multiple_objects",
		  &obj_create_multiple_objects},
		{ "obj_create_on_multiple_pools",
		  &obj_create_on_multiple_pools},
		{ "obj_create_then_delete",
		  &obj_create_then_delete},
		{ "obj_delete_multiple",
		  &obj_delete_multiple},
		{ "obj_no_wait",
		  &obj_no_wait},
		{ "obj_wait_twice",
		  &obj_wait_twice},
		{ "obj_wait_no_launch",
		  &obj_wait_no_launch},
		{ "obj_op_setup",
		  &obj_op_setup},
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
