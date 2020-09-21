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


#define M0_TRACE_SUBSYSTEM M0_TRACE_SUBSYS_CLIENT
#include "lib/trace.h"

#include "motr/client.h"
#include "motr/st/st.h"
#include "motr/st/st_misc.h"
#include "motr/st/st_assert.h"

struct m0_container st_example_container;
static struct m0_uint128 test_id;
static uint64_t layout_id;

/*
 * copy-cat of st_obj_delete_multiple just to show
 * how to write a more complicated test using new assert method.
 */
static void example_abitmorecomplicated(void)
{
	uint32_t                n_objs = 5;
	uint32_t                rounds = 20;
	uint32_t                max_ops = 4; /* max. ops in one launch() */
	struct m0_uint128      *ids = NULL;
	struct m0_obj          *objs = NULL;
	struct m0_op          **ops = NULL;
	bool                   *obj_exists = NULL;
	bool                   *already_chosen = NULL;
	uint32_t                i;
	uint32_t                j;
	uint32_t                idx;
	uint32_t                n_ops;
	int                     rc;


	M0_CLIENT_THREAD_ENTER;

	/*
         * Don't worry about freeing memory, ST will do it for you
         * if you forget to do so.
         * Note: use client ST self-ownned MEM_XXX and mem_xxx rather
         * than motr stuff
         */
	MEM_ALLOC_ARR(ids, n_objs);
	ST_ASSERT_FATAL(ids != NULL);
	MEM_ALLOC_ARR(objs, n_objs);
	ST_ASSERT_FATAL(objs != NULL);
	MEM_ALLOC_ARR(obj_exists, n_objs);
	ST_ASSERT_FATAL(obj_exists != NULL);
	MEM_ALLOC_ARR(already_chosen, n_objs);
	ST_ASSERT_FATAL(already_chosen != NULL);

	MEM_ALLOC_ARR(ops, max_ops);
	ST_ASSERT_FATAL(ops != NULL);
	for (i = 0; i < max_ops; i++)
		ops[i] = NULL;

	/*
	 * Initialise the objects. Closed set.
	 *
	 * Note: use client api wrapper (xxx) instead of
	 * m0_xxx directly
	 */
	for (i = 0; i < n_objs; ++i)
		oid_get(ids + i);

	/* Repeat several rounds. */
	for (i = 0; i < rounds; ++i) {
		/* Each round, a different number of ops in the same launch. */
		n_ops = generate_random(max_ops) + 1;
		memset(already_chosen, 0, n_objs*sizeof(already_chosen[0]));

		/* Set the ops. */
		for (j = 0; j < n_ops; ++j) {
			/* Pick n_ops objects. */
			do {
				//XXX this is broken in kernel mode
				idx = generate_random(n_objs);
			} while(already_chosen[idx]);
			already_chosen[idx] = true;

			M0_SET0(&objs[idx]);
			st_obj_init(&objs[idx],
				&st_example_container.co_realm,
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
			ST_ASSERT_FATAL(rc == 0);
			ST_ASSERT_FATAL(
			    ops[j]->op_sm.sm_state == M0_OS_STABLE
			    || ops[j]->op_sm.sm_state == M0_OS_FAILED);
			st_op_fini(ops[j]);
		}
	}

	/* No operations should be pending at this point. */
	for (i = 0; i < max_ops; ++i)
		st_op_free(ops[i]);

	for (i = 0; i < n_objs; ++i)
		st_entity_fini(&objs[i].ob_entity);

	/*
	 * Release all the allocated structs. If you want, you can
	 *` ask ST to do the dirty job for you by skipping this step.
	 */
	if (ids != NULL)
		mem_free(ids);
	if (objs != NULL)
		mem_free(objs);
	if (obj_exists != NULL)
		mem_free(obj_exists);
	if (already_chosen != NULL)
		mem_free(already_chosen);
	if (ops != NULL)
		mem_free(ops);
}

/* copy-cat of st_obj_create_multiple */
static void example_simple(void)
{
	enum { CREATE_MULTIPLE_N_OBJS = 20 };
	uint32_t            i;
	struct m0_op       *ops[CREATE_MULTIPLE_N_OBJS] = {NULL};
	struct m0_obj     **objs = NULL;
	struct m0_uint128   ids[CREATE_MULTIPLE_N_OBJS];
	int                 rc;

	M0_CLIENT_THREAD_ENTER;

	MEM_ALLOC_ARR(objs, CREATE_MULTIPLE_N_OBJS);
	ST_ASSERT_FATAL(objs != NULL);
	for (i = 0; i < CREATE_MULTIPLE_N_OBJS; ++i) {
		MEM_ALLOC_PTR(objs[i]);
		ST_ASSERT_FATAL(objs[i] != NULL);
		memset(objs[i], 0, sizeof *objs[i]);
	}

	oid_get_many(ids, CREATE_MULTIPLE_N_OBJS);

	/* Create different objects. */
	for (i = 0; i < CREATE_MULTIPLE_N_OBJS; ++i) {
		st_obj_init(objs[i],
			&st_example_container.co_realm,
			&ids[i], layout_id);
		rc = st_entity_create(NULL,
					     &objs[i]->ob_entity, &ops[i]);
		ST_ASSERT_FATAL(rc == 0);
		ST_ASSERT_FATAL(ops[i] != NULL);
		ST_ASSERT_FATAL(ops[i]->op_sm.sm_rc == 0);
	}

	st_op_launch(ops, ARRAY_SIZE(ops));

	/* We wait for each op. */
	for (i = 0; i < CREATE_MULTIPLE_N_OBJS; ++i) {
		rc = st_op_wait(ops[i], M0_BITS(M0_OS_FAILED,
						    M0_OS_STABLE),
				    time_from_now(5,0));
		ST_ASSERT_FATAL(rc == 0 || rc == -ETIMEDOUT);
	}

	/* All correctly created. */
	for (i = 0; i < CREATE_MULTIPLE_N_OBJS; ++i) {
		st_op_fini(ops[i]);
		st_op_free(ops[i]);

		st_entity_fini(&objs[i]->ob_entity);
		mem_free(objs[i]);
	}

	mem_free(objs);
}

static int st_example_init(void)
{
	int rc = 0;

	/*
	 * Retrieve the uber realm. We don't need to open this,
	 * as realms are not actually implemented yet
	 */
	st_container_init(&st_example_container, NULL,
			 &M0_UBER_REALM,
			 st_get_instance());
	rc = st_example_container.co_realm.re_entity.en_sm.sm_rc;

	if (rc != 0)
		console_printf("Failed to open uber realm\n");

	test_id = M0_ID_APP;
	layout_id = m0_client_layout_id(st_get_instance());
	return rc;
}

static int st_example_fini(void)
{
	return 0;
}

struct st_suite st_suite_example = {
	.ss_name = "example_st",
	.ss_init = st_example_init,
	.ss_fini = st_example_fini,
	.ss_tests = {
		{ "example_simple",
		  &example_simple},
		{ "example_abitmorecomplicated",
		  &example_abitmorecomplicated},
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
