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


#define M0_TRACE_SUBSYSTEM M0_TRACE_SUBSYS_CLIENT
#include "lib/trace.h"          /* M0_LOG */

#include "ut/ut.h"              /* M0_UT_ASSERT */
#include "motr/ut/client.h"

#include "lib/finject.h"
/*
 * Including the c files so we can replace the M0_PRE asserts
 * in order to test them.
 */
#include "motr/idx.c"

struct m0_ut_suite ut_suite_idx;

static struct m0_client *dummy_instance;

static struct m0_op_idx* ut_m0_op_idx_alloc()
{
	struct m0_op_idx *oi;

	M0_ALLOC_PTR(oi);
	M0_UT_ASSERT(oi != NULL);
	M0_SET0(oi);

	oi->oi_oc.oc_op.op_size = sizeof *oi;
	oi->oi_oc.oc_op.op_code = M0_EO_CREATE;
	m0_op_idx_bob_init(oi);
	m0_ast_rc_bob_init(&oi->oi_ar);
	m0_op_common_bob_init(&oi->oi_oc);

	return oi;
}

static void ut_m0_op_idx_free(struct m0_op_idx *oi)
{
	m0_free(oi);
}

/**
 * Tests m0_op_obj_invariant().
 */
static void ut__idx_op_invariant(void)
{
	bool              rc;
	struct m0_op_idx *oi;

	/* Base cases. */
	oi = ut_m0_op_idx_alloc();
	rc = m0__idx_op_invariant(oi);
	M0_UT_ASSERT(rc == true);

	rc = m0__idx_op_invariant(NULL);
	M0_UT_ASSERT(rc == false);

	oi->oi_oc.oc_op.op_size = sizeof *oi - 1;
	rc = m0__idx_op_invariant(oi);
	M0_UT_ASSERT(rc == false);

	ut_m0_op_idx_free(oi);
}

/**
 * Tests base case and preconditions of m0_idx_op_init().
 */
static void ut_idx_op_init(void)
{
	int               rc = 0; /* required */
	struct m0_realm   realm;
	struct m0_idx     idx;
	struct m0_op     *op = NULL;
	struct m0_client *instance = NULL;
	struct m0_uint128 id;

	/* initialise */
	instance = dummy_instance;
	ut_realm_entity_setup(&realm, &idx.in_entity, instance);

	/* base case */
	id = M0_ID_APP;
	id.u_lo++;
	idx.in_entity.en_id = id;

	op = m0_alloc(sizeof(struct m0_op_idx));
	op->op_size = sizeof(struct m0_op_idx);

	/* ADD FI here */
	m0_fi_enable_once("m0_op_init", "fail_op_init");
	rc = idx_op_init(&idx, M0_EO_CREATE,
			 NULL, NULL, NULL, 0, op);
	M0_UT_ASSERT(rc != 0);

	rc = idx_op_init(&idx, M0_EO_CREATE,
			 NULL, NULL, NULL, 0, op);
	M0_UT_ASSERT(rc == 0);
	M0_UT_ASSERT(op->op_code  == M0_EO_CREATE);
	m0_free(op);

	/* finalise */
	m0_entity_fini(&idx.in_entity);
}

/**
 * Tests idx_op_complete().
 */
static void ut_idx_op_complete(void)
{
	struct m0_entity    ent;
	struct m0_realm     realm;
	struct m0_client   *instance = NULL;
	struct m0_op_idx   *oi;
	struct m0_sm_group *op_grp;
	struct m0_sm_group *en_grp;
	struct m0_sm_group  oi_grp;

	/* init */
	M0_SET0(&ent);
	instance = dummy_instance;
	ut_realm_entity_setup(&realm, &ent, instance);

	oi = ut_m0_op_idx_alloc();
	oi->oi_oc.oc_op.op_entity = &ent;
	oi->oi_oc.oc_op.op_code = M0_EO_CREATE;
	m0_op_bob_init(&oi->oi_oc.oc_op);

	m0_sm_group_init(&oi_grp);
	oi->oi_sm_grp = &oi_grp;

	en_grp = &ent.en_sm_group;
	op_grp = &oi->oi_oc.oc_op.op_sm_group;
	m0_sm_group_init(en_grp);
	m0_sm_group_init(op_grp);

	/* base case */
	m0_sm_init(&oi->oi_oc.oc_op.op_sm, &m0_op_conf,
		   M0_OS_INITIALISED, op_grp);

	m0_sm_group_lock(en_grp);
	m0_sm_move(&ent.en_sm, 0, M0_ES_CREATING);
	m0_sm_group_unlock(en_grp);

	m0_sm_group_lock(op_grp);
	m0_sm_move(&oi->oi_oc.oc_op.op_sm, 0, M0_OS_LAUNCHED);
	m0_sm_group_unlock(op_grp);

	m0_fi_enable_once("m0_op_stable", "skip_ongoing_io_ref");
	m0_sm_group_lock(&oi_grp);
	idx_op_ast_complete(&oi_grp, &oi->oi_ar.ar_ast);
	m0_sm_group_unlock(&oi_grp);

	M0_UT_ASSERT(ent.en_sm.sm_state == M0_ES_OPEN);
	M0_UT_ASSERT(oi->oi_oc.oc_op.op_sm.sm_state == M0_OS_STABLE);

	/* finalise */
	m0_sm_group_fini(&oi_grp);
	m0_sm_group_lock(op_grp);
	m0_sm_fini(&oi->oi_oc.oc_op.op_sm);
	m0_sm_group_unlock(op_grp);
	m0_sm_group_fini(op_grp);

	m0_entity_fini(&ent);
	ut_m0_op_idx_free(oi);
}

/**
 * Tests idx_op_fail().
 */
static void ut_idx_op_fail(void)
{
	struct m0_entity    ent;
	struct m0_realm     realm;
	struct m0_client   *instance = NULL;
	struct m0_op_idx   *oi;
	struct m0_sm_group  oi_grp;
	struct m0_sm_group *op_grp;
	struct m0_sm_group *en_grp;

	/* init */
	M0_SET0(&ent);
	instance = dummy_instance;
	ut_realm_entity_setup(&realm, &ent, instance);

	oi = ut_m0_op_idx_alloc();
	oi->oi_oc.oc_op.op_entity = &ent;
	oi->oi_oc.oc_op.op_code = M0_EO_CREATE;
	m0_op_bob_init(&oi->oi_oc.oc_op);

	m0_sm_group_init(&oi_grp);
	oi->oi_sm_grp = &oi_grp;

	en_grp = &ent.en_sm_group;
	op_grp = &oi->oi_oc.oc_op.op_sm_group;
	m0_sm_group_init(en_grp);
	m0_sm_group_init(op_grp);

	/* base case */
	m0_sm_init(&oi->oi_oc.oc_op.op_sm, &m0_op_conf,
		   M0_OS_INITIALISED, op_grp);

	m0_sm_group_lock(en_grp);
	m0_sm_move(&ent.en_sm, 0, M0_ES_CREATING);
	m0_sm_group_unlock(en_grp);

	m0_sm_group_lock(op_grp);
	m0_sm_move(&oi->oi_oc.oc_op.op_sm, 0, M0_OS_LAUNCHED);
	m0_sm_group_unlock(op_grp);

	m0_sm_group_lock(&oi_grp);
	m0_fi_enable_once("m0_op_stable", "skip_ongoing_io_ref");
	idx_op_fail(oi, -1);
	m0_sm_group_unlock(&oi_grp);

	M0_UT_ASSERT(ent.en_sm.sm_state == M0_ES_OPEN);
	M0_UT_ASSERT(oi->oi_oc.oc_op.op_rc == -1);
	M0_UT_ASSERT(oi->oi_oc.oc_op.op_sm.sm_state == M0_OS_STABLE);

	/* finalise */
	m0_sm_group_fini(&oi_grp);
	m0_sm_group_lock(op_grp);
	m0_sm_fini(&oi->oi_oc.oc_op.op_sm);
	m0_sm_group_unlock(op_grp);
	m0_sm_group_fini(op_grp);

	m0_entity_fini(&ent);
	ut_m0_op_idx_free(oi);
}

/**
 * Tests only a few of the pre-conditions of idx_op_cb_launch().
 */
static int dummy_query_rc = 1;

static int idx_dummy_query(struct m0_op_idx *oi)
{
	return dummy_query_rc;
}

static struct m0_idx_query_ops idx_dummy_query_ops = {
	.iqo_namei_create = idx_dummy_query,
	.iqo_namei_delete = idx_dummy_query,
	.iqo_namei_lookup = idx_dummy_query,
	.iqo_namei_list   = idx_dummy_query,

	.iqo_get          = idx_dummy_query,
	.iqo_put          = idx_dummy_query,
	.iqo_del          = idx_dummy_query,
	.iqo_next         = idx_dummy_query,
};
static struct m0_idx_service idx_dummy_service;

static void ut_idx_op_cb_launch(void)
{
	int                 op_code;
	struct m0_client   *instance = NULL;
	struct m0_realm     realm;
	struct m0_sm_group *op_grp;
	struct m0_sm_group *en_grp;
	struct m0_sm_group  locality_grp;
	struct m0_entity    ent;
	struct m0_op_idx   *oi;

	/* Initialise client */
	M0_SET0(&ent);
	instance = dummy_instance;
	ut_realm_entity_setup(&realm, &ent, instance);

	oi = ut_m0_op_idx_alloc();
	oi->oi_oc.oc_op.op_entity = &ent;
	oi->oi_oc.oc_op.op_code = M0_EO_CREATE;
	m0_op_bob_init(&oi->oi_oc.oc_op);

	op_grp = &oi->oi_oc.oc_op.op_sm_group;
	en_grp = &ent.en_sm_group;
	m0_sm_group_init(op_grp);
	m0_sm_group_init(en_grp);
	m0_sm_group_init(&locality_grp);
	oi->oi_sm_grp = &locality_grp;

	idx_dummy_service.is_query_ops = &idx_dummy_query_ops;
	instance->m0c_idx_svc_ctx.isc_service = &idx_dummy_service;

	/* Base case 1: asynchronous queries */
	for (op_code = M0_EO_CREATE;
	     op_code <= M0_EO_DELETE; op_code++) {
		/* Ignore SYNC. */
		if (M0_IN(op_code, (M0_EO_SYNC, M0_EO_OPEN,
				    M0_EO_GETATTR)))
			continue;

		dummy_query_rc = 1;
		oi->oi_oc.oc_op.op_code = op_code;
		m0_sm_init(&oi->oi_oc.oc_op.op_sm, &m0_op_conf,
			   M0_OS_INITIALISED, op_grp);

		m0_sm_group_lock(op_grp);
		idx_op_cb_launch(&oi->oi_oc);
		m0_sm_move(&oi->oi_oc.oc_op.op_sm, 0, M0_OS_EXECUTED);
		m0_sm_move(&oi->oi_oc.oc_op.op_sm, 0, M0_OS_STABLE);
		m0_sm_fini(&oi->oi_oc.oc_op.op_sm);
		m0_sm_group_unlock(op_grp);

		m0_sm_group_lock(en_grp);
		if (op_code == M0_EO_CREATE)
			m0_sm_move(&ent.en_sm, 0, M0_ES_OPEN);
		else
			m0_sm_move(&ent.en_sm, 0, M0_ES_INIT);
		m0_sm_group_unlock(en_grp);

	}

	for (op_code = M0_IC_GET;
	     op_code <= M0_IC_NEXT; op_code++) {
		dummy_query_rc = 1;
		oi->oi_oc.oc_op.op_code = op_code;
		m0_sm_init(&oi->oi_oc.oc_op.op_sm, &m0_op_conf,
			   M0_OS_INITIALISED, op_grp);

		m0_sm_group_lock(op_grp);
		idx_op_cb_launch(&oi->oi_oc);
		m0_sm_move(&oi->oi_oc.oc_op.op_sm, 0, M0_OS_EXECUTED);
		m0_sm_move(&oi->oi_oc.oc_op.op_sm, 0, M0_OS_STABLE);
		m0_sm_fini(&oi->oi_oc.oc_op.op_sm);
		m0_sm_group_unlock(op_grp);

	}

	/* Case 2 and 3 are delayed until we find a way to start locality thread.*/
	/* Base case 2: sync query successes. */
	/* Base case 3: sync query fails. */

	/* finalise */
	m0_sm_group_fini(&locality_grp);
	m0_sm_group_fini(op_grp);

	m0_entity_fini(&ent);
	ut_m0_op_idx_free(oi);
}

/**
 * Tests obj_namei_cb_free().
 */
static void ut_idx_op_cb_free(void)
{
	struct m0_op_idx *oi;
	struct m0_entity  ent;
	struct m0_realm   realm;
	struct m0_client *instance = NULL;

	/* init */
	M0_SET0(&ent);
	instance = dummy_instance;
	ut_realm_entity_setup(&realm, &ent, instance);

	/* base case */
	oi = ut_m0_op_idx_alloc();
	oi->oi_oc.oc_op.op_entity = &ent;
	idx_op_cb_free(&oi->oi_oc);

	/* fini */
	m0_entity_fini(&ent);
}

/**
 * Tests obj_namei_cb_fini().
 */
static void ut_idx_op_cb_fini(void)
{
	struct m0_op_idx *oi;
	struct m0_realm   realm;
	struct m0_entity  ent;
	struct m0_client *instance = NULL;

	/* init */
	M0_SET0(&ent);
	instance = dummy_instance;
	ut_realm_entity_setup(&realm, &ent, instance);

	/* base cases */
	oi = ut_m0_op_idx_alloc();
	oi->oi_oc.oc_op.op_entity = &ent;
	idx_op_cb_fini(&oi->oi_oc);

	/* fini */
	m0_entity_fini(&ent);
	ut_m0_op_idx_free(oi);
}

/**
 * Tests idx_op_ast_complete().
 */
static void ut_idx_op_ast_complete(void)
{
	struct m0_client   *instance = NULL;
	struct m0_realm     realm;
	struct m0_sm_group *op_grp;
	struct m0_sm_group *en_grp;
	struct m0_sm_group  locality_grp;
	struct m0_entity    ent;
	struct m0_op_idx    oi;

	/* initialise client */
	instance = dummy_instance;
	M0_SET0(&ent);
	M0_SET0(&oi);
	M0_SET0(&realm);
	m0_op_bob_init(&oi.oi_oc.oc_op);
	m0_ast_rc_bob_init(&oi.oi_ar);
	m0_op_idx_bob_init(&oi);

	ut_realm_entity_setup(&realm, &ent, instance);
	op_grp = &oi.oi_oc.oc_op.op_sm_group;
	en_grp = &ent.en_sm_group;
	m0_sm_group_init(op_grp);
	m0_sm_group_init(en_grp);
	m0_sm_group_init(&locality_grp);
	oi.oi_sm_grp = &locality_grp;

	/* base case: one single ios cob request */
	oi.oi_oc.oc_op.op_entity = &ent;
	oi.oi_oc.oc_op.op_size = sizeof oi;
	oi.oi_oc.oc_op.op_code = M0_EO_CREATE;

	m0_sm_init(&oi.oi_oc.oc_op.op_sm, &m0_op_conf,
		   M0_OS_INITIALISED, op_grp);
	m0_sm_group_lock(en_grp);
	m0_sm_move(&ent.en_sm, 0, M0_ES_CREATING);
	m0_sm_group_unlock(en_grp);

	m0_sm_group_lock(op_grp);
	m0_sm_move(&oi.oi_oc.oc_op.op_sm, 0, M0_OS_LAUNCHED);
	m0_sm_group_unlock(op_grp);

	m0_fi_enable_once("m0_op_stable", "skip_ongoing_io_ref");
	m0_sm_group_lock(&locality_grp);
	idx_op_ast_complete(&locality_grp, &oi.oi_ar.ar_ast);
	m0_sm_group_unlock(&locality_grp);

	M0_UT_ASSERT(oi.oi_oc.oc_op.op_entity == &ent);
	M0_UT_ASSERT(ent.en_sm.sm_state == M0_ES_OPEN);
	M0_UT_ASSERT(oi.oi_oc.oc_op.op_sm.sm_state == M0_OS_STABLE);

	/* finalise */
	m0_op_bob_fini(&oi.oi_oc.oc_op);

	m0_sm_group_lock(op_grp);
	m0_sm_fini(&oi.oi_oc.oc_op.op_sm);
	m0_sm_group_unlock(op_grp);

	m0_sm_group_fini(&locality_grp);
	m0_sm_group_fini(op_grp);
	m0_entity_fini(&ent);
}

/**
 * Tests idx_op_ast_fail().
 */
static void ut_idx_op_ast_fail(void)
{
	struct m0_client   *instance = NULL;
	struct m0_realm     realm;
	struct m0_sm_group *op_grp;
	struct m0_sm_group *en_grp;
	struct m0_sm_group  locality_grp;
	struct m0_entity    ent;
	struct m0_op_idx    oi;

	/* initialise client */
	instance = dummy_instance;
	M0_SET0(&ent);
	M0_SET0(&oi);
	M0_SET0(&realm);
	m0_op_bob_init(&oi.oi_oc.oc_op);
	m0_ast_rc_bob_init(&oi.oi_ar);
	m0_op_idx_bob_init(&oi);

	ut_realm_entity_setup(&realm, &ent, instance);
	op_grp = &oi.oi_oc.oc_op.op_sm_group;
	en_grp = &ent.en_sm_group;
	m0_sm_group_init(op_grp);
	m0_sm_group_init(en_grp);
	m0_sm_group_init(&locality_grp);
	oi.oi_sm_grp = &locality_grp;

	/* base case: one single ios cob request */
	oi.oi_oc.oc_op.op_entity = &ent;
	oi.oi_oc.oc_op.op_size = sizeof oi;
	oi.oi_oc.oc_op.op_code = M0_EO_CREATE;
	oi.oi_ar.ar_rc = -1;

	m0_sm_init(&oi.oi_oc.oc_op.op_sm, &m0_op_conf,
		   M0_OS_INITIALISED, op_grp);
	m0_sm_group_lock(en_grp);
	m0_sm_move(&ent.en_sm, 0, M0_ES_CREATING);
	m0_sm_group_unlock(en_grp);

	m0_sm_group_lock(op_grp);
	m0_sm_move(&oi.oi_oc.oc_op.op_sm, 0, M0_OS_LAUNCHED);
	m0_sm_group_unlock(op_grp);

	m0_fi_enable_once("m0_op_stable", "skip_ongoing_io_ref");
	m0_sm_group_lock(&locality_grp);
	idx_op_ast_fail(&locality_grp, &oi.oi_ar.ar_ast);
	m0_sm_group_unlock(&locality_grp);

	M0_UT_ASSERT(oi.oi_oc.oc_op.op_entity == &ent);
	M0_UT_ASSERT(ent.en_sm.sm_state == M0_ES_OPEN);
	M0_UT_ASSERT(oi.oi_oc.oc_op.op_rc == -1);
	M0_UT_ASSERT(oi.oi_oc.oc_op.op_sm.sm_state == M0_OS_STABLE);

	/* finalise */
	m0_op_bob_fini(&oi.oi_oc.oc_op);

	m0_sm_group_lock(op_grp);
	m0_sm_fini(&oi.oi_oc.oc_op.op_sm);
	m0_sm_group_unlock(op_grp);

	m0_sm_group_fini(&locality_grp);
	m0_sm_group_fini(op_grp);
	m0_entity_fini(&ent);
}

/**
 * Tests the pre and post conditions of the m0_idx_init()
 * entry point. Also checks the object is correctly initialised.
 * The testee is seen as a black box that has to react as expected
 * to some specific input and generate some valid output.
 */
static void ut_m0_idx_init(void)
{
	struct m0_idx       idx;
	struct m0_uint128   id;
	struct m0_client   *instance = NULL;
	struct m0_container uber_realm;

	/* initialise client */
	instance = dummy_instance;
	m0_container_init(&uber_realm, NULL,
			  &M0_UBER_REALM,
			  instance);

	/* we need a valid id */
	id = M0_ID_APP;
	id.u_lo++;

	/* base case: no error */
	m0_idx_init(&idx, &uber_realm.co_realm, &id);

	/* check the initialisation */
	M0_UT_ASSERT(idx.in_entity.en_type == M0_ET_IDX);
	M0_UT_ASSERT(m0_uint128_cmp(&idx.in_entity.en_id, &id) == 0);
	M0_UT_ASSERT(idx.in_entity.en_realm == &uber_realm.co_realm);
}

/**
 * Tests m0_idx_fini().
 */
static void ut_m0_idx_fini(void)
{
	struct m0_uint128    id;
	struct m0_idx        idx;
	struct m0_client    *instance = NULL;
	struct m0_container  uber_realm;

	/* initialise client */
	instance = dummy_instance;
	m0_container_init(&uber_realm, NULL,
			  &M0_UBER_REALM,
			  instance);

	/* Create an entity we can use */
	id = M0_ID_APP;
	id.u_lo++;
	m0_idx_init(&idx, &uber_realm.co_realm, &id);

	/* Base case: m0_idx_fini works */
	m0_idx_fini(&idx);
}

/**
 * Tests base case and preconditions of m0_idx_op().
 */
static void ut_m0_idx_op(void)
{
	int               rc = 0; /* required */
	struct m0_op     *op = NULL;
	struct m0_idx     idx;
	struct m0_uint128 id;
	struct m0_client *instance = NULL;
	struct m0_realm   realm;
	struct m0_bufvec  keys;
	struct m0_bufvec  vals;
	int               rcs;

	instance = dummy_instance;
	ut_realm_entity_setup(&realm, &idx.in_entity, instance);

	id = M0_ID_APP;
	id.u_lo++;
	idx.in_entity.en_id = id;

	/* Base case */
	keys.ov_vec.v_nr = 0x01;
	vals.ov_vec.v_nr = 0x01;
	rc = m0_idx_op(&idx, M0_IC_GET, &keys, &vals, &rcs, 0,
		       &op);
	M0_UT_ASSERT(rc == 0);
	M0_UT_ASSERT(op != NULL);
	m0_free(op);

	m0_entity_fini(&idx.in_entity);
}

/**
 * Tests base case and preconditions of m0_idx_op_namei().
 */
static void ut_m0_idx_op_namei(void)
{
	int                rc = 0; /* required */
	struct m0_op      *op = NULL;
	struct m0_idx      idx;
	struct m0_uint128  id;
	struct m0_client  *instance = NULL;
	struct m0_realm    realm;

	instance = dummy_instance;
	ut_realm_entity_setup(&realm, &idx.in_entity, instance);

	id = M0_ID_APP;
	id.u_lo++;
	idx.in_entity.en_id = id;

	/* Base case */
	rc = m0_idx_op_namei(&idx.in_entity, &op, (int)M0_IC_GET);
	M0_UT_ASSERT(rc == 0);
	M0_UT_ASSERT(op != NULL);
	m0_free(op);

	m0_entity_fini(&idx.in_entity);
}

/**
 * Tests for m0_idx_service_config()
 */
static void ut_m0_idx_service_config(void)
{
	int               svc_id = 0;
	struct m0_client *instance = dummy_instance;

	m0_idx_service_config(instance, svc_id, (void *)0x100);
}

M0_INTERNAL int ut_idx_init(void)
{
	int rc;

	rc = ut_m0_client_init(&dummy_instance);
	M0_UT_ASSERT(rc == 0);

	return 0;
}

M0_INTERNAL int ut_idx_fini(void)
{
	ut_m0_client_fini(&dummy_instance);

	return 0;
}
struct m0_ut_suite ut_suite_idx = {
	.ts_name = "idx-ut",
	.ts_init = ut_idx_init,
	.ts_fini = ut_idx_fini,
	.ts_tests = {
		{ "_idx_op_invariant",
			&ut__idx_op_invariant},
		{ "idx_op_init",
			&ut_idx_op_init},
		{ "idx_op_complete",
			&ut_idx_op_complete},
		{ "idx_op_fail",
			&ut_idx_op_fail},
		{ "idx_op_cb_launch",
			&ut_idx_op_cb_launch},
		{ "idx_op_cb_free",
			&ut_idx_op_cb_free},
		{ "idx_op_cb_fini",
			&ut_idx_op_cb_fini},
		{ "idx_op_ast_complete",
			&ut_idx_op_ast_complete},
		{ "idx_op_ast_fail",
			&ut_idx_op_ast_fail},
		{ "m0_idx_init",
			&ut_m0_idx_init},
		{ "m0_idx_fini",
			&ut_m0_idx_fini},
		{ "m0_idx_op",
			&ut_m0_idx_op},
		{ "m0_idx_op_namei",
			&ut_m0_idx_op_namei},
		{ "m0_idx_service_config",
			&ut_m0_idx_service_config},
		{ NULL, NULL },
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
