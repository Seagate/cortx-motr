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
#include "lib/trace.h"

#include "layout/layout.h"
#include "pool/pool.h"

#include "ut/ut.h"              /* M0_UT_ASSERT */
#include "motr/ut/client.h"

/*
 * Including the c files so we can replace the M0_PRE asserts
 * in order to test them.
 */
#include "motr/cob.c"
#include "motr/obj.c"

/*
 * We need to initialise the default layout domain.
 */
static int t_register(struct m0_layout_domain *dom,
		      const struct m0_layout_type *lt)
{
	return 0;
}

static void t_unregister(struct m0_layout_domain *dom,
			 const struct m0_layout_type *lt)
{
}

static m0_bcount_t t_max_recsize(struct m0_layout_domain *dom)
{
	return 0;
}

static const struct m0_layout_type_ops ut_layout_type_ops = {
	.lto_register    = t_register,
	.lto_unregister  = t_unregister,
	.lto_max_recsize = t_max_recsize
};
static struct m0_layout_type ut_layout_type = {
	.lt_name     = "test",
	.lt_id       = M0_DEFAULT_LAYOUT_ID,
	.lt_ops      = &ut_layout_type_ops
};
static struct m0_layout ut_layout;

/* XXX export? */
struct m0_fid ut_fid;
static void
ut_layout_enum_get(const struct m0_layout_enum *e, uint32_t idx,
		   const struct m0_fid *gfid, struct m0_fid *out)
{
	m0_fid_gob_make(&ut_fid, 0, 1);
	m0_fid_convert_gob2cob(&ut_fid, out, 0);
}

static uint32_t ut_layout_nr(const struct m0_layout_enum *e)
{
	/* ut_layout_enum_get() returns always the same single id */
	return M0T1FS_LAYOUT_P;
}

static struct m0_layout_enum_ops ut_layout_enum_ops = {
	.leo_get = &ut_layout_enum_get,
	.leo_nr = &ut_layout_nr,
};
static struct m0_layout_enum_type ut_layout_enum_type;

struct m0_layout_enum ut_layout_enum;

static struct m0_layout_enum *
ut_lio_ops_to_enum(const struct m0_layout_instance *li)
{
	ut_layout_enum.le_type = &ut_layout_enum_type;
	ut_layout_enum.le_ops = &ut_layout_enum_ops;
	return &ut_layout_enum;
}

static void ut_lio_fini(struct m0_layout_instance *li)
{
}

struct m0_layout_instance_ops ut_layout_instance_ops = {
	.lio_to_enum = ut_lio_ops_to_enum,
	.lio_fini = ut_lio_fini,
};

static struct m0_layout_instance ut_layout_instance;
static int ut_m0_ops_instance_build(struct m0_layout  *layout,
				    const struct m0_fid *fid,
				    struct m0_layout_instance **linst)
{
	struct m0_fid valid_fid;

	m0_fid_gob_make(&valid_fid, 0, 1);
	ut_layout_instance.li_gfid = valid_fid;
	ut_layout_instance.li_l = layout;
	*linst = &ut_layout_instance;
	return 0;
}

void ut_m0_ops_fini(struct m0_ref *ref)
{

}

static struct m0_layout_ops ut_layout_ops = {
	.lo_instance_build = ut_m0_ops_instance_build,
	.lo_fini = ut_m0_ops_fini,
};

/* Imports */
M0_INTERNAL void m0_layout__init(struct m0_layout *l,
				 struct m0_layout_domain *dom,
				 uint64_t lid,
				 struct m0_layout_type *lt,
				 const struct m0_layout_ops *ops);
M0_INTERNAL void m0_layout__populate(struct m0_layout *l, uint32_t user_count);
static const struct m0_bob_type enum_bob = {
	.bt_name         = "enum",
	.bt_magix_offset = offsetof(struct m0_layout_enum, le_magic),
	.bt_magix        = M0_LAYOUT_ENUM_MAGIC,
	.bt_check        = NULL
};
M0_BOB_DEFINE(M0_INTERNAL, &enum_bob, m0_layout_enum);

static const struct m0_bob_type layout_instance_bob = {
	.bt_name         = "layout_instance",
	.bt_magix_offset = offsetof(struct m0_layout_instance, li_magic),
	.bt_magix        = M0_LAYOUT_INSTANCE_MAGIC,
	.bt_check        = NULL
};
M0_BOB_DEFINE(M0_INTERNAL, &layout_instance_bob, m0_layout_instance);

M0_INTERNAL void
ut_layout_init(struct m0_layout *layout,
	       struct m0_layout_domain *dom)
{
	uint64_t               layout_id;
	struct m0_pool_version pv;

	/* Workaround: fill just the necessary stuff for motr to not complain. */
	ut_layout_instance.li_ops = &ut_layout_instance_ops;
	ut_layout_enum.le_ops = &ut_layout_enum_ops;
	ut_layout_enum.le_type = &ut_layout_enum_type;
	m0_layout_instance_bob_init(&ut_layout_instance);
	m0_layout_enum_bob_init(&ut_layout_enum);

	M0_SET0(&pv);
	layout_id = m0_pool_version2layout_id(&pv.pv_id, M0_DEFAULT_LAYOUT_ID);
	m0_layout__init(layout, dom, layout_id,
			&ut_layout_type, &ut_layout_ops);
	m0_layout__populate(layout, 1);
}

M0_INTERNAL void
ut_striped_layout_init(struct m0_striped_layout *stl,
		       struct m0_layout_domain *dom)
{
	ut_layout_init(&stl->sl_base, dom);
	stl->sl_enum = &ut_layout_enum;
	ut_layout_enum.le_sl_is_set = false;
	ut_layout_enum.le_sl = NULL;
}

static void ut_layout_fini(struct m0_layout  *l,
			   struct m0_layout_domain *dom)
{
	m0_layout_put(l);

	m0_layout_enum_bob_fini(&ut_layout_enum);
	m0_layout_instance_bob_fini(&ut_layout_instance);
	ut_layout_enum.le_type = NULL;
	ut_layout_enum.le_ops = NULL;
	ut_layout_instance.li_ops = NULL;
}

M0_INTERNAL void
ut_striped_layout_fini(struct m0_striped_layout *stl,
		       struct m0_layout_domain *dom)
{
	ut_layout_fini(&stl->sl_base, dom);
}

void ut_layout_domain_fill(struct m0_client *cinst)
{
	int                      rc;
	struct m0_layout_domain *dom;

	dom = &cinst->m0c_reqh.rh_ldom;
	rc = m0_layout_type_register(dom, &ut_layout_type);
	M0_UT_ASSERT(rc == 0);
}

void ut_layout_domain_empty(struct m0_client *cinst)
{
	struct m0_layout_domain *dom;

	M0_UT_ASSERT(cinst != NULL);
	dom = &cinst->m0c_reqh.rh_ldom;
	M0_UT_ASSERT(dom != NULL);

	/* Cleanup all layouts in this domain */
	m0_layout_domain_cleanup(dom);

	/* Then safely un-register our own layout type */
	m0_layout_type_unregister(dom, &ut_layout_type);
}

/**********
 *   UTs
 **********/

/**
 * Tests m0_op_obj_invariant().
 */
static void ut_test_m0_op_obj_invariant(void)
{
	struct m0_op_obj *oo;
	bool              rc;

	/* Base cases. */
	M0_ALLOC_PTR(oo);
	m0_op_obj_bob_init(oo);
	m0_ast_rc_bob_init(&oo->oo_ar);
	m0_op_common_bob_init(&oo->oo_oc);
	oo->oo_oc.oc_op.op_size = sizeof *oo;
	rc = m0_op_obj_invariant(oo);
	M0_UT_ASSERT(rc == true);

	rc = m0_op_obj_invariant(NULL);
	M0_UT_ASSERT(rc == false);

	oo->oo_oc.oc_op.op_size = sizeof *oo - 1;
	rc = m0_op_obj_invariant(oo);
	M0_UT_ASSERT(rc == false);

	m0_free(oo);
}

/**
 * Tests m0_op_obj_ast_rc_invariant().
 */
static void ut_test_m0_op_obj_ast_rc_invariant(void)
{
	struct m0_ast_rc *ar;
	bool              rc;

	/* Base cases. */
	M0_ALLOC_PTR(ar);
	m0_ast_rc_bob_init(ar);
	rc = m0_op_obj_ast_rc_invariant(ar);
	M0_UT_ASSERT(rc == true);

	rc = m0_op_obj_ast_rc_invariant(NULL);
	M0_UT_ASSERT(rc == false);

	m0_ast_rc_bob_fini(ar);
	rc = m0_op_obj_ast_rc_invariant(ar);
	M0_UT_ASSERT(rc == false);

	m0_free(ar);
}

/**
 * Tests ios_cob_req_invariant().
 */
static void ut_test_cob_req_invariant(void)
{
	struct ios_cob_req *icr;
	bool                rc;

	/* Base cases. */
	M0_ALLOC_PTR(icr);
	ios_cob_req_bob_init(icr);
	rc = ios_cob_req_invariant(icr);
	M0_UT_ASSERT(rc == true);

	rc = ios_cob_req_invariant(NULL);
	M0_UT_ASSERT(rc == false);

	ios_cob_req_bob_fini(icr);
	rc = ios_cob_req_invariant(icr);
	M0_UT_ASSERT(rc == false);

	m0_free(icr);
}

/**
 * Tests m0_op_common_invariant().
 */
static void ut_test_m0_op_common_invariant(void)
{
	struct m0_op_common *oc;
	bool                 rc;

	/* Base cases. */
	M0_ALLOC_PTR(oc);
	m0_op_common_bob_init(oc);
	rc = m0_op_common_invariant(oc);
	M0_UT_ASSERT(rc == true);

	rc = m0_op_common_invariant(NULL);
	M0_UT_ASSERT(rc == false);

	m0_op_common_bob_fini(oc);
	rc = m0_op_common_invariant(oc);
	M0_UT_ASSERT(rc == false);

	m0_free(oc);
}

/**
 * Tests only the pre-conditions of m0_obj_layout_instance_build().
 */
static void
ut_test_m0__obj_layout_instance_build(void)
{
	int                        rc = 0; /* required */
	uint64_t                   lid = 0;
	struct m0_fid              fid;
	struct m0_layout_instance *linst;
	struct m0_client          *instance = NULL;
	struct m0_pool_version     pv;

	/* init */
	rc = ut_m0_client_init(&instance);
	M0_UT_ASSERT(rc == 0);
	ut_layout_domain_fill(instance);
	ut_layout_init(&ut_layout, &instance->m0c_reqh.rh_ldom);
	M0_SET0(&fid);
	M0_SET0(&pv);

	/* base case */
	fid.f_key = 777;
	lid = m0_pool_version2layout_id(&pv.pv_id, M0_DEFAULT_LAYOUT_ID);
	rc = m0__obj_layout_instance_build(instance, lid, &fid, &linst);
	M0_UT_ASSERT(rc == 0);
	M0_UT_ASSERT(linst == &ut_layout_instance);

	/* fini */
	ut_layout_fini(&ut_layout, &instance->m0c_reqh.rh_ldom);
	ut_layout_domain_empty(instance);
	ut_m0_client_fini(&instance);
}

/**
 * Tests obj_fid_make_name().
 */
static void ut_test_m0_fid_print(void)
{
	struct m0_fid   fid;
	char            name[M0_FID_STR_LEN];
	int             rc = 0; /* required */

	/* base case */
	m0_fid_set(&fid, 0xCAFE, 0xFEED);
	rc = m0_fid_print(name, sizeof name, &fid);
	M0_UT_ASSERT(rc > 0);
	M0_UT_ASSERT(!strcmp(name, "cafe:feed"));
}

/**
 * Tests obj_namei_op_init().
 */
static void ut_test_obj_namei_op_init(void)
{
	int               rc = 0; /* required */
	struct m0_realm   realm;
	struct m0_entity  ent;
	struct m0_op_obj  oo;
	struct m0_client *instance = NULL;
	struct m0_fid     pfid = { .f_container = 0, .f_key = 1 };
	struct m0_op     *op;

	/* init */
	rc = ut_m0_client_init(&instance);
	M0_UT_ASSERT(rc == 0);

	ut_layout_domain_fill(instance);
	ut_layout_init(&ut_layout, &instance->m0c_reqh.rh_ldom);
	ut_realm_entity_setup(&realm, &ent, instance);

	/* op_size < sizeof(m0_op_obj) */
	oo.oo_oc.oc_op.op_size = sizeof oo - 1;
	rc = obj_namei_op_init(&ent, &oo.oo_oc.oc_op);
	M0_UT_ASSERT(rc == -EMSGSIZE);

	/* base case */
	ent.en_id.u_hi = 0x0;
	ent.en_id.u_lo = 0x103000;
	op = &oo.oo_oc.oc_op;
	m0_op_bob_init(op);
	oo.oo_oc.oc_op.op_entity = &ent;
	oo.oo_oc.oc_op.op_size = sizeof oo;
	oo.oo_pver = instance->m0c_pools_common.pc_cur_pver->pv_id;
	instance->m0c_root_fid = pfid;

	m0_op_obj_bob_init(&oo);
	m0_op_common_bob_init(&oo.oo_oc);
	m0_ast_rc_bob_init(&oo.oo_ar);

	m0_fi_enable_once("m0__obj_layout_id_get", "fake_obj_layout_id");
	rc = obj_namei_op_init(&ent, &oo.oo_oc.oc_op);
	M0_UT_ASSERT(rc == 0);
	M0_UT_ASSERT(oo.oo_layout_instance == &ut_layout_instance);

	M0_UT_ASSERT(oo.oo_oc.oc_cb_launch == obj_namei_cb_launch);
	M0_UT_ASSERT(oo.oo_oc.oc_cb_fini == obj_namei_cb_fini);

#ifdef CLIENT_FOR_M0T1FS
	//M0_UT_ASSERT(oo.oo_fid.f_container == 0x0);
	M0_UT_ASSERT(oo.oo_fid.f_key == 0x103000);
	//M0_UT_ASSERT(!strcmp((char *)oo.oo_name.b_addr, "0:103"));
#endif

	m0_free(oo.oo_name.b_addr);
	m0_ast_rc_bob_fini(&oo.oo_ar);
	m0_op_common_bob_fini(&oo.oo_oc);
	m0_op_obj_bob_fini(&oo);

	/* fini */
	ut_layout_fini(&ut_layout, &instance->m0c_reqh.rh_ldom);
	ut_layout_domain_empty(instance);
	m0_entity_fini(&ent);
	ut_m0_client_fini(&instance);
}

/**
 * Tests obj_op_obj_invariant().
 */
static void ut_test_obj_op_obj_invariant(void)
{
	bool              valid;
	struct m0_op_obj  oo;
	struct m0_realm   realm;
	struct m0_entity  ent;
	struct m0_client *instance = NULL;
	int               rc = 0; /* required */

	/* init */
	rc = ut_m0_client_init(&instance);
	M0_UT_ASSERT(rc == 0);
	ut_realm_entity_setup(&realm, &ent, instance);

	/* base case: !invariant */
	oo.oo_oc.oc_op.op_entity = &ent;
	realm.re_instance = NULL;
	valid = obj_op_obj_invariant(&oo);
	M0_UT_ASSERT(!valid);

	/* base case: invariant */
	oo.oo_oc.oc_op.op_entity = &ent;
	realm.re_instance = instance;
	valid = obj_op_obj_invariant(&oo);
	M0_UT_ASSERT(valid);

	/* fini */
	m0_entity_fini(&ent);
	ut_m0_client_fini(&instance);
}

/**
 * Tests pre-conditions and base case for obj_op_obj_init().
 */
static void ut_test_obj_op_obj_init(void)
{
	int               rc = 0; /* required */
	struct m0_op_obj  oo;
	struct m0_realm   realm;
	struct m0_entity  ent;
	struct m0_client *instance = NULL;

	/* init */
	rc = ut_m0_client_init(&instance);
	M0_UT_ASSERT(rc == 0);
	ut_realm_entity_setup(&realm, &ent, instance);

	/* base case */
	instance->m0c_pools_common.pc_cur_pver->pv_attr.pa_P = 13;

	oo.oo_oc.oc_op.op_entity = &ent;
	oo.oo_oc.oc_op.op_size = sizeof oo;
	oo.oo_pver = instance->m0c_pools_common.pc_cur_pver->pv_id;
	oo.oo_oc.oc_op.op_code = M0_EO_CREATE;
	m0_op_common_bob_init(&oo.oo_oc);

	rc = obj_op_obj_init(&oo);
	M0_UT_ASSERT(rc == 0);

	m0_op_common_bob_fini(&oo.oo_oc);

	/* fini */
	m0_entity_fini(&ent);
	ut_m0_client_fini(&instance);
}

/**
 * Tests base case and preconditions of obj_op_prepare().
 */
static void ut_test_obj_op_prepare(void)
{
	int                         rc = 0; /* required */
	struct m0_realm      realm;
	struct m0_entity     entity;
	struct m0_obj        obj;
	struct m0_op_common *oc;
	struct m0_op_obj    *oo;
	struct m0_op        *op = NULL;
	struct m0_client    *instance = NULL;
	struct m0_container  uber_realm;
	struct m0_uint128    id;

	/* initialise */
	rc = ut_m0_client_init(&instance);
	M0_UT_ASSERT(rc == 0);
	m0_container_init(&uber_realm, NULL, &M0_UBER_REALM, instance);
	id = M0_ID_APP;
	id.u_lo++;

	M0_SET0(&entity);
	ut_realm_entity_setup(&realm, &entity, instance);
	M0_SET0(&obj);

	/* base case */
	instance->m0c_pools_common.pc_cur_pver->pv_attr.pa_P = 7;
	m0_obj_init(&obj, &uber_realm.co_realm, &id,
		    m0_client_layout_id(instance));

	/* OP Allocation fails */
	m0_fi_enable_once("m0_alloc", "fail_allocation");
	rc = obj_op_prepare(&obj.ob_entity, &op, M0_EO_CREATE);
	M0_UT_ASSERT(rc != 0);
	M0_UT_ASSERT(op == NULL);

	/* m0_op_init fails */
	m0_fi_enable_once("m0_op_init", "fail_op_init");
	rc = obj_op_prepare(&obj.ob_entity, &op, M0_EO_CREATE);
	M0_UT_ASSERT(rc != 0);
	M0_UT_ASSERT(op == NULL);

	/* we won't use 'entity' as in the error cases */
	rc = obj_op_prepare(&obj.ob_entity,
			    &op, M0_EO_CREATE);
	M0_UT_ASSERT(rc == 0);
	M0_UT_ASSERT(op != NULL);
	M0_UT_ASSERT(op->op_size == sizeof(struct m0_op_obj));
	M0_UT_ASSERT(op->op_sm.sm_state == M0_OS_INITIALISED);
	M0_UT_ASSERT(op->op_entity == &obj.ob_entity);
	oc = container_of(op, struct m0_op_common, oc_op);
	oo = container_of(oc, struct m0_op_obj, oo_oc);
	M0_UT_ASSERT(oo->oo_mds_fop == NULL);

	/* finalise client */
	m0_entity_fini(&entity);
	ut_m0_client_fini(&instance);
}

/**
 * Helper function to test entity_namei_op() client entry point.
 */
static void ut_entity_namei_op(enum m0_entity_opcode opcode)
{
	int                     rc = 0; /* required */
	struct m0_op           *op = NULL;
	struct m0_obj           obj;
	struct m0_entity       *ent;
	struct m0_realm         realm;
	struct m0_uint128       id;
	struct m0_client       *instance = NULL;
	struct m0_pool_version  pv;
	struct m0_fid           pfid = { .f_container = 0, .f_key = 1 };
	struct m0_op_obj       *oo;
	struct m0_op_common    *oc;

	/* init */
	rc = ut_m0_client_init(&instance);
	M0_UT_ASSERT(rc == 0);
	M0_SET0(&obj);
	ent = &obj.ob_entity;
	ut_layout_domain_fill(instance);
	ut_layout_init(&ut_layout, &instance->m0c_reqh.rh_ldom);
	ut_realm_entity_setup(&realm, ent, instance);

	M0_SET0(&pv);

	/* we need a valid id */
	id = M0_ID_APP;
	id.u_lo++;

	m0_fi_enable_once("m0__obj_layout_id_get", "fake_obj_layout_id");

	/* base case: no error, then check the output */
	ent->en_type = M0_ET_OBJ;
	ent->en_sm.sm_state = M0_ES_INIT;
	ent->en_id = id;
	pv.pv_attr.pa_P = 7; /* pool width */
	instance->m0c_pools_common.pc_cur_pver = &pv;
	instance->m0c_root_fid = pfid;
	rc = entity_namei_op(ent, &op, opcode);
	/* basic: detailed checks are included in the corresponding tests */
	M0_UT_ASSERT(rc == 0);
	M0_UT_ASSERT(op != NULL);
	M0_UT_ASSERT(op->op_size == sizeof(struct m0_op_obj));
	M0_UT_ASSERT(op->op_entity == ent);
	M0_UT_ASSERT(op->op_sm.sm_state == M0_OS_INITIALISED);

	/* free memory */
	oc = bob_of(op, struct m0_op_common, oc_op, &oc_bobtype);
	oo = bob_of(oc, struct m0_op_obj, oo_oc, &oo_bobtype);
	m0_free(oo->oo_name.b_addr);

	/* fini */
	m0_entity_fini(ent);
	ut_layout_fini(&ut_layout, &instance->m0c_reqh.rh_ldom);
	ut_layout_domain_empty(instance);
	ut_m0_client_fini(&instance);

	if (op) m0_free(op);
}

/**
 * Tests entity_namei_op() as used when creating an object.
 */
static void ut_test_entity_namei_op_create(void)
{
	ut_entity_namei_op(M0_EO_CREATE);
}

/**
 * Tests entity_namei_op() as used when deleting an object.
 */
static void ut_test_entity_namei_op_delete(void)
{
	ut_entity_namei_op(M0_EO_DELETE);
}

static void ut_test_obj_lid_assign(void)
{
	struct m0_obj     obj;
	struct m0_entity  ent;
	struct m0_realm   realm;
	struct m0_op     *ops[1] = {NULL};
	struct m0_client *instance = NULL;
	int               rc;

	/* init */
	rc = ut_m0_client_init(&instance);
	M0_UT_ASSERT(rc == 0);

	ut_realm_entity_setup(&realm, &ent, instance);
	ent.en_realm = &realm;

	memset(&obj, 0, sizeof obj);
	obj.ob_attr.oa_buf_size = 40960;
	m0_obj_init(&obj, &realm, &ent.en_id, 0);

	rc = m0_entity_create(NULL, &obj.ob_entity, &ops[0]);
	M0_UT_ASSERT(obj.ob_attr.oa_layout_id > 0 &&
			obj.ob_attr.oa_layout_id < 15);
	M0_UT_ASSERT(rc == -EINVAL);
	M0_UT_ASSERT(ops[0] == NULL);

	ut_m0_client_fini(&instance);
}

static void ut_test_m0_entity_create(void)
{
	struct m0_obj            obj;
	struct m0_entity         ent;
	struct m0_realm          realm;
	struct m0_op            *ops[1] = {NULL};
	struct m0_client        *instance = NULL;
	int                      rc;

	/* init */
	rc = ut_m0_client_init(&instance);
	M0_UT_ASSERT(rc == 0);

	ut_realm_entity_setup(&realm, &ent, instance);
	ent.en_realm = &realm;

	memset(&obj, 0, sizeof obj);
	m0_obj_init(&obj, &realm, &ent.en_id,
			   m0_client_layout_id(instance));

	rc = m0_entity_create(NULL, &obj.ob_entity, &ops[0]);
	/*
	 * The layouts are not initialized. So we can not find layout with the
	 * specified lid.
	 */
	M0_UT_ASSERT(rc == -EINVAL);
	M0_UT_ASSERT(ops[0] == NULL);

	ut_m0_client_fini(&instance);
}

static void ut_test_m0_entity_delete(void)
{
	struct m0_obj     obj;
	struct m0_entity  ent;
	struct m0_realm   realm;
	struct m0_op     *ops[1] = {NULL};
	struct m0_client *instance = NULL;
	int               rc;

	/* init */
	rc = ut_m0_client_init(&instance);
	M0_UT_ASSERT(rc == 0);

	ut_realm_entity_setup(&realm, &ent, instance);
	ent.en_realm = &realm;

	M0_SET0(&obj);
	m0_obj_init(&obj, &realm, &ent.en_id,
		    m0_client_layout_id(instance));

	m0_fi_enable_once("obj_namei_op_init", "fake_msg_size");
	rc = m0_entity_delete(&obj.ob_entity, &ops[0]);
	M0_UT_ASSERT(rc == -EMSGSIZE);
	M0_UT_ASSERT(ops[0] == NULL);

	ut_m0_client_fini(&instance);
}

/**
 * Tests m0__entity_instance().
 */
static void ut_test_m0__entity_instance(void)
{
	struct m0_entity  ent;
	struct m0_realm   realm;
	struct m0_client *cins2;
	struct m0_client *instance = NULL;
	int               rc = 0; /* required */

	/* init */
	rc = ut_m0_client_init(&instance);
	M0_UT_ASSERT(rc == 0);

	ut_realm_entity_setup(&realm, &ent, instance);
	ent.en_realm = &realm;

	/* base case */
	cins2 = m0__entity_instance(&ent);
	M0_UT_ASSERT(cins2 == instance);

	/* finalise */
	m0_entity_fini(&ent);
	ut_m0_client_fini(&instance);
}

/**
 * Tests m0__oo_instance().
 */
static void ut_test_m0__oo_instance(void)
{
	struct m0_entity  ent;
	struct m0_realm   realm;
	struct m0_client *cins2;
	struct m0_op_obj  oo;
	struct m0_client *instance = NULL;
	int               rc = 0; /* required */

	/* initialise client */
	rc = ut_m0_client_init(&instance);
	M0_UT_ASSERT(rc == 0);

	/* init */
	ut_realm_entity_setup(&realm, &ent, instance);

	/* base case */
	oo.oo_oc.oc_op.op_entity = &ent;
	cins2 = m0__oo_instance(&oo);
	M0_UT_ASSERT(cins2 == instance);

	/* finalise */
	m0_entity_fini(&ent);
	ut_m0_client_fini(&instance);
}

/**
 * Tests cob_complete_op().
 */
static void ut_test_cob_complete_op(void)
{
	struct m0_entity    ent;
	struct m0_realm     realm;
	struct m0_client   *instance = NULL;
	struct m0_op_obj    oo;
	struct m0_sm_group *op_grp;
	struct m0_sm_group *en_grp;
	struct m0_sm_group  oo_grp;
	int                 rc = 0; /* required */

	/* init */
	M0_SET0(&oo);
	M0_SET0(&ent);
	rc = ut_m0_client_init(&instance);
	M0_UT_ASSERT(rc == 0);
	ut_realm_entity_setup(&realm, &ent, instance);

	op_grp = &oo.oo_oc.oc_op.op_sm_group;
	en_grp = &ent.en_sm_group;
	m0_sm_group_init(op_grp);
	m0_sm_group_init(en_grp);
	m0_sm_group_init(&oo_grp);
	oo.oo_sm_grp = &oo_grp;
	oo.oo_oc.oc_op.op_code = M0_EO_CREATE;

	/* base case */
	m0_sm_init(&oo.oo_oc.oc_op.op_sm, &m0_op_conf,
		   M0_OS_INITIALISED, op_grp);
	m0_op_bob_init(&oo.oo_oc.oc_op);
	oo.oo_oc.oc_op.op_entity = &ent;
	oo.oo_oc.oc_op.op_size = sizeof oo;

	m0_sm_group_lock(en_grp);
	m0_sm_move(&ent.en_sm, 0, M0_ES_CREATING);
	m0_sm_group_unlock(en_grp);

	m0_sm_group_lock(op_grp);
	m0_sm_move(&oo.oo_oc.oc_op.op_sm, 0, M0_OS_LAUNCHED);
	m0_sm_group_unlock(op_grp);

	m0_sm_group_lock(&oo_grp);
	m0_fi_enable_once("m0_op_stable", "skip_ongoing_io_ref");
	cob_complete_op(&oo.oo_oc.oc_op);
	m0_sm_group_unlock(&oo_grp);

	M0_UT_ASSERT(oo.oo_oc.oc_op.op_entity == &ent);
	M0_UT_ASSERT(ent.en_sm.sm_state == M0_ES_OPEN);
	M0_UT_ASSERT(oo.oo_oc.oc_op.op_sm.sm_state == M0_OS_STABLE);

	/* finalise */
	m0_sm_group_lock(op_grp);
	m0_sm_fini(&oo.oo_oc.oc_op.op_sm);
	m0_sm_group_unlock(op_grp);
	m0_sm_group_fini(op_grp);

	m0_sm_group_fini(&oo_grp);
	m0_op_bob_fini(&oo.oo_oc.oc_op);

	m0_entity_fini(&ent);
	ut_m0_client_fini(&instance);
}

/**
 * Tests cob_fail_op().
 */
static void ut_test_cob_fail_op(void)
{
	int                 rc = 0; /* required */
	struct m0_entity    ent;
	struct m0_realm     realm;
	struct m0_client   *instance = NULL;
	struct m0_op_obj    oo;
	struct m0_sm_group *op_grp;
	struct m0_sm_group *en_grp;
	struct m0_sm_group  oo_grp;

	/* Init. */
	M0_SET0(&oo);
	M0_SET0(&ent);
	rc = ut_m0_client_init(&instance);
	M0_UT_ASSERT(rc == 0);
	ut_realm_entity_setup(&realm, &ent, instance);

	op_grp = &oo.oo_oc.oc_op.op_sm_group;
	en_grp = &ent.en_sm_group;
	m0_sm_group_init(op_grp);
	m0_sm_group_init(en_grp);
	m0_sm_group_init(&oo_grp);
	oo.oo_sm_grp = &oo_grp;

	m0_sm_init(&oo.oo_oc.oc_op.op_sm, &m0_op_conf,
		   M0_OS_INITIALISED, op_grp);
	oo.oo_oc.oc_op.op_entity = &ent;
	oo.oo_oc.oc_op.op_size = sizeof oo;
	m0_op_bob_init(&oo.oo_oc.oc_op);

	m0_sm_group_lock(en_grp);
	m0_sm_move(&ent.en_sm, 0, M0_ES_CREATING);
	m0_sm_group_unlock(en_grp);

	m0_sm_group_lock(op_grp);
	m0_sm_move(&oo.oo_oc.oc_op.op_sm, 0, M0_OS_LAUNCHED);
	m0_sm_group_unlock(op_grp);

	/* Base case. */
	m0_sm_group_lock(&oo_grp);
	m0_fi_enable_once("m0_op_stable", "skip_ongoing_io_ref");
	cob_fail_op(&oo.oo_oc.oc_op, 777);
	m0_sm_group_unlock(&oo_grp);

	M0_UT_ASSERT(oo.oo_oc.oc_op.op_entity == &ent);
	M0_UT_ASSERT(ent.en_sm.sm_state == M0_ES_INIT);
	M0_UT_ASSERT(oo.oo_oc.oc_op.op_rc == 777);
	M0_UT_ASSERT(oo.oo_oc.oc_op.op_sm.sm_state == M0_OS_STABLE);

	/* finalise */
	m0_sm_group_fini(&oo_grp);
	m0_sm_group_lock(op_grp);
	m0_sm_fini(&oo.oo_oc.oc_op.op_sm);
	m0_sm_group_unlock(op_grp);

	m0_op_bob_fini(&oo.oo_oc.oc_op);
	m0_entity_fini(&ent);
	ut_m0_client_fini(&instance);
}

/*
 * Tests obj_rpc_item_to_ios_cob_req().
 */
static void ut_test_rpc_item_to_ios_cob_req(void)
{
	struct ios_cob_req  icr;
	struct m0_fop       fop;
	struct ios_cob_req *ret;

	/* base case */
	fop.f_opaque = &icr;
	ret = rpc_item_to_icr(&fop.f_item);
	M0_UT_ASSERT(ret == &icr);
}

/**
 * Tests obj_icr_ast_complete().
 */
static void ut_test_icr_ast_complete(void)
{
	struct m0_client   *instance = NULL;
	struct cob_req     *cr;
	struct m0_op_obj    oo;
	struct ios_cob_req  icr;
	struct m0_sm_group *en_grp;
	struct m0_sm_group *op_grp;
	struct m0_sm_group  locality_grp;
	struct m0_fop       fop;
	struct m0_entity    ent;
	struct m0_realm     realm;
	uint64_t            pool_width;
	int                 rc = 0; /* required */

	/* initialise client */
	rc = ut_m0_client_init(&instance);
	M0_UT_ASSERT(rc == 0);
	pool_width = 1;
	instance->m0c_pools_common.pc_cur_pver->pv_attr.pa_P = pool_width;

	M0_ALLOC_PTR(cr);
	M0_UT_ASSERT(cr != NULL);
	M0_ALLOC_PTR(cr->cr_cob_attr);
	M0_UT_ASSERT(cr->cr_cob_attr != NULL);

	M0_SET0(&ent);
	M0_SET0(&oo);
	M0_SET0(cr);
	M0_SET0(&icr);
	M0_SET0(&realm);
	ios_cob_req_bob_init(&icr);
	m0_ast_rc_bob_init(&icr.icr_ar);
	cob_req_bob_init(cr);
	m0_ast_rc_bob_init(&cr->cr_ar);
	m0_op_bob_init(&oo.oo_oc.oc_op);

	ut_realm_entity_setup(&realm, &ent, instance);
	op_grp = &oo.oo_oc.oc_op.op_sm_group;
	en_grp = &ent.en_sm_group;
	m0_sm_group_init(op_grp);
	m0_sm_group_init(en_grp);
	m0_sm_group_init(&locality_grp);
	m0_mutex_init(&oo.oo_oc.oc_op.op_priv_lock);
	oo.oo_sm_grp = &locality_grp;

	/* base case: one single ios cob request */
	cr->cr_opcode = M0_EO_CREATE;
	cr->cr_cinst = instance;
	cr->cr_op = &oo.oo_oc.oc_op;
	cr->cr_cob_type = M0_COB_IO;
	cr->cr_icr_nr = pool_width;
	M0_ALLOC_ARR(cr->cr_ios_replied, pool_width);
	M0_ALLOC_ARR(cr->cr_ios_fop, pool_width);
	fop.f_item.ri_reply = (struct m0_rpc_item *)0xffff;
	fop.f_opaque = &icr;
	cr->cr_ios_fop[0] = &fop;

	icr.icr_cr = cr;
	icr.icr_index = 0;
	oo.oo_oc.oc_op.op_entity = &ent;
	oo.oo_oc.oc_op.op_size = sizeof oo;
	oo.oo_oc.oc_op.op_code = M0_EO_CREATE;

	/* TODO ADd to cob ut*/
	m0_sm_init(&oo.oo_oc.oc_op.op_sm, &m0_op_conf,
		   M0_OS_INITIALISED, op_grp);
	m0_sm_group_lock(en_grp);
	m0_sm_move(&ent.en_sm, 0, M0_ES_CREATING);
	m0_sm_group_unlock(en_grp);

	m0_sm_group_lock(op_grp);
	m0_sm_move(&oo.oo_oc.oc_op.op_sm, 0, M0_OS_LAUNCHED);
	m0_sm_group_unlock(op_grp);

	m0_fi_enable_once("icrs_complete", "skip_post_cr_ast");
	m0_fi_enable_once("m0_op_stable", "skip_ongoing_io_ref");
	m0_sm_group_lock(&locality_grp);
	icr_ast(&locality_grp, &icr.icr_ar.ar_ast);
	m0_sm_group_unlock(&locality_grp);

	M0_UT_ASSERT(oo.oo_oc.oc_op.op_entity == &ent);
	M0_UT_ASSERT(ent.en_sm.sm_state == M0_ES_OPEN);
	M0_UT_ASSERT(oo.oo_oc.oc_op.op_sm.sm_state == M0_OS_STABLE);

	/* finalise */
	m0_entity_fini(&ent);

	m0_mutex_fini(&oo.oo_oc.oc_op.op_priv_lock);
	m0_ast_rc_bob_fini(&icr.icr_ar);
	ios_cob_req_bob_fini(&icr);
	m0_op_bob_fini(&oo.oo_oc.oc_op);

	m0_sm_group_lock(op_grp);
	m0_sm_fini(&oo.oo_oc.oc_op.op_sm);
	m0_sm_group_unlock(op_grp);
	m0_sm_group_fini(op_grp);

	m0_sm_group_fini(&locality_grp);
	ut_m0_client_fini(&instance);
}

/**
 * Tests obj_icr_ast_fail().
 */
static void ut_test_icr_ast_fail(void)
{
	int                   rc = 0; /* required */
	struct m0_client     *instance = NULL;
	struct m0_entity      ent;
	struct m0_realm       realm;
	struct m0_op_obj      oo;
	struct ios_cob_req    icr;
	struct m0_sm_group    oo_grp;
	struct m0_sm_group   *op_grp;
	struct m0_sm_group   *en_grp;
	struct cob_req       *cr;
	struct m0_fop         fop;

	/* Init. */
	M0_SET0(&oo);
	M0_SET0(&ent);
	rc = ut_m0_client_init(&instance);
	M0_UT_ASSERT(rc == 0);
	cr = m0_alloc(sizeof *cr);
	M0_ASSERT(cr != NULL);

	cob_req_bob_init(cr);
	m0_ast_rc_bob_init(&cr->cr_ar);
	cr->cr_cinst = instance;
	ut_realm_entity_setup(&realm, &ent, instance);
	op_grp = &oo.oo_oc.oc_op.op_sm_group;
	en_grp = &ent.en_sm_group;
	m0_sm_group_init(op_grp);
	m0_sm_group_init(en_grp);
	m0_sm_group_init(&oo_grp);
	m0_mutex_init(&oo.oo_oc.oc_op.op_priv_lock);
	oo.oo_sm_grp = &oo_grp;

	m0_sm_init(&oo.oo_oc.oc_op.op_sm, &m0_op_conf,
		   M0_OS_INITIALISED, op_grp);
	oo.oo_oc.oc_op.op_entity = &ent;
	oo.oo_oc.oc_op.op_size = sizeof oo;

	m0_sm_group_lock(en_grp);
	m0_sm_move(&ent.en_sm, 0, M0_ES_CREATING);
	m0_sm_group_unlock(en_grp);

	m0_sm_group_lock(op_grp);
	m0_sm_move(&oo.oo_oc.oc_op.op_sm, 0, M0_OS_LAUNCHED);
	m0_sm_group_unlock(op_grp);

	ios_cob_req_bob_init(&icr);
	m0_ast_rc_bob_init(&icr.icr_ar);
	m0_op_bob_init(&oo.oo_oc.oc_op);

	/* Base case. */
	cr->cr_opcode = M0_EO_CREATE;
	cr->cr_op = &oo.oo_oc.oc_op;
	cr->cr_icr_nr = 1;
	M0_ALLOC_ARR(cr->cr_ios_replied, cr->cr_icr_nr);
	M0_ALLOC_ARR(cr->cr_ios_fop, cr->cr_icr_nr);
	fop.f_item.ri_reply = (struct m0_rpc_item *)0xffff;
	fop.f_opaque = &icr;
	cr->cr_ios_fop[0] = &fop;
	icr.icr_cr = cr;
	icr.icr_ar.ar_rc = 111;
	icr.icr_index = 0;

	m0_fi_enable_once("icrs_fail", "skip_post_cr_ast");
	m0_fi_enable_once("m0_op_stable", "skip_ongoing_io_ref");
	m0_sm_group_lock(&oo_grp);
	icr_ast(&oo_grp, &icr.icr_ar.ar_ast);
	m0_sm_group_unlock(&oo_grp);
	M0_UT_ASSERT(oo.oo_oc.oc_op.op_rc == 111);

	/* finalise client */
	m0_ast_rc_bob_fini(&icr.icr_ar);
	ios_cob_req_bob_fini(&icr);
	m0_op_bob_fini(&oo.oo_oc.oc_op);

	m0_mutex_fini(&oo.oo_oc.oc_op.op_priv_lock);
	m0_sm_group_fini(&oo_grp);
	m0_sm_group_lock(op_grp);
	m0_sm_fini(&oo.oo_oc.oc_op.op_sm);
	m0_sm_group_unlock(op_grp);
	m0_sm_group_fini(op_grp);
	m0_entity_fini(&ent);
	ut_m0_client_fini(&instance);
}

static void ut_test_icr_ast_ver_mismatch(void)
{
}

static void ut_test_cob_rio_ios_replied(void)
{
	/* cannot avoid m0_sm_ast_post(): no base case */
}

/**
 * Tests ios_cob_fop_populate().
 */
static void ut_test_cob_ios_fop_populate(void)
{
	int                       rc = 0; /* required */
	struct m0_fop             fop;
	struct m0_fid             cob_fid;
	struct m0_op_obj         *oo;
	struct m0_fop_cob_create *cc;
	struct m0_realm           realm;
	struct m0_entity          ent;
	struct m0_poolmach       *mach;
	struct m0_poolmach_state  state;
	struct m0_client         *instance = NULL;
	struct m0_sm_group        oo_grp;

	/* initialise */
	rc = ut_m0_client_init(&instance);
	M0_UT_ASSERT(rc == 0);
	M0_SET0(&fop);
	M0_SET0(&cob_fid);
	M0_ALLOC_PTR(oo);
	M0_UT_ASSERT(oo != NULL);
	M0_ALLOC_PTR(cc);
	M0_UT_ASSERT(cc != NULL);
	M0_SET0(cc);
	ut_realm_entity_setup(&realm, &ent, instance);
	m0_sm_group_init(&oo_grp);
	oo->oo_sm_grp = &oo_grp;

	/* base case */
	fop.f_type = &m0_fop_cob_create_fopt;
	fop.f_data.fd_data = cc;
	m0_fid_set(&oo->oo_fid, 0xDEAF, 0xDAD);
	oo->oo_oc.oc_op.op_entity = &ent;
	m0_fid_set(&cob_fid, 0xBAD, 0xBEE);

	mach = &instance->m0c_pools_common.pc_cur_pver->pv_mach;
	mach->pm_state = &state;

	m0_sm_group_lock(&oo_grp);
	m0_rwlock_init(&mach->pm_lock);
	OP_OBJ2CODE(oo) = M0_EO_CREATE;
	m0_sm_group_unlock(&oo_grp);

	/*rc = cob_ios_fop_populate(oo, &fop, &cob_fid, 777);
	m0_rwlock_fini(&mach->pm_lock);
	M0_UT_ASSERT(rc == 0);*/

	/* fini */
	m0_sm_group_fini(&oo_grp);
	m0_free(oo);
	m0_entity_fini(&ent);
	ut_m0_client_fini(&instance);
}

/**
 * Tests m0_obj_container_id_to_session().
 */
static void ut_test_m0_obj_container_id_to_session(void)
{
	int                         rc;
	struct m0_reqh_service_ctx *ctx;
	struct m0_rpc_session      *session;
	struct m0_client           *instance = NULL;
	struct m0_pool_version     *pv;

	/* initialise client */
	rc = ut_m0_client_init(&instance);
	M0_UT_ASSERT(rc == 0 && instance != NULL);
	pv = instance->m0c_pools_common.pc_cur_pver;

	/* base case */
	M0_ALLOC_PTR(pv->pv_pc);
	M0_ALLOC_ARR(pv->pv_pc->pc_dev2svc, 1);
	M0_ALLOC_PTR(ctx);

	ctx->sc_type = M0_CST_IOS;
	pv->pv_pc->pc_nr_devices = 1;
	pv->pv_pc->pc_dev2svc[0].pds_ctx = ctx;
	session = m0_obj_container_id_to_session(pv, 0);
	M0_UT_ASSERT(session == &ctx->sc_rlink.rlk_sess);
	m0_free(pv->pv_pc->pc_dev2svc);
	m0_free(pv->pv_pc);
	m0_free(ctx);

	/* fini */
	ut_m0_client_fini(&instance);
}

static void ut_test_m0_cob_ios_fop_fini(void)
{
}

/**
 * Cannot test the base case without calling to m0_rpc_post().
 */
static void ut_test_cob_ios_send(void)
{
}

/**
 * Tests obj_rpc_item_to_cr().
 */
static void ut_test_rpc_item_to_cr(void)
{
	struct m0_fop   fop;
	struct cob_req *cr;
	struct cob_req  cob_req;

	/* base case */
	cob_req_bob_init(&cob_req);
	m0_ast_rc_bob_init(&cob_req.cr_ar);
	fop.f_opaque = &cob_req;
	cr = rpc_item_to_cr(&fop.f_item);
	M0_UT_ASSERT(cr == &cob_req);
	m0_ast_rc_bob_fini(&cob_req.cr_ar);
	cob_req_bob_fini(&cob_req);
}

static void ut_test_cob_ast_fail_oo(void)
{
}

/**
 * Cannot test the base case without calling m0_rpc_post().
 */
static void ut_test_cob_rio_mds_replied(void)
{
}

/**
 * Tests cob_name_mem2wire().
 */
static void ut_test_cob_name_mem2wire(void)
{
	struct m0_fop_str tgt;
	struct m0_buf     name;
	char             *str = "HavantFTW";
	int               rc = 0; /* required */

	/* base case */
	M0_SET0(&tgt);
	m0_buf_init(&name, str, strlen(str));
	M0_UT_ASSERT(name.b_nob ==  strlen(str));
	rc = cob_name_mem2wire(&tgt, &name);
	M0_UT_ASSERT(rc == 0);
	M0_UT_ASSERT(tgt.s_len == name.b_nob);
	M0_UT_ASSERT(!strncmp((char *)tgt.s_buf, (char *)name.b_addr, tgt.s_len));

	m0_free(tgt.s_buf);
}

/**
 * Tests cob_body_mem2wire().
 */
static void ut_test_cob_body_mem2wire(void)
{
	int                rc;
	struct m0_cob_attr attr;
	struct m0_fop_cob  body;
	struct cob_req     cr;
	struct m0_client  *instance = NULL;

	/* initialise client */
	rc = ut_m0_client_init(&instance);
	M0_UT_ASSERT(rc == 0);

	/* init */
	M0_SET0(&body);
	M0_SET0(&cr);
	attr.ca_nlink = 1;
	attr.ca_valid = M0_COB_NLINK;

	/* base case */
	m0_fid_set(&cr.cr_fid, 0x66, 0x77);
#ifdef CLIENT_FOR_M0T1FS
	cr.cr_cinst = instance;
#endif

	cob_body_mem2wire(&body, &attr, attr.ca_valid, &cr);

	M0_UT_ASSERT(body.b_nlink == 1);
	M0_UT_ASSERT(body.b_valid & M0_COB_NLINK);
	M0_UT_ASSERT(body.b_tfid.f_container == 0x66);
	M0_UT_ASSERT(body.b_tfid.f_key == 0x77);
#ifdef CLIENT_FOR_M0T1FS
	M0_UT_ASSERT(body.b_pfid.f_container == instance->m0c_root_fid.f_container);
	M0_UT_ASSERT(body.b_pfid.f_key == instance->m0c_root_fid.f_key);
#endif
	ut_m0_client_fini(&instance);
}

/**
 * Tests cob_mds_fop_populate().
 */
static void ut_test_cob_mds_fop_populate(void)
{
	struct m0_op_obj    *oo;
	struct m0_fop        fop;
	struct cob_req      *cr;
	struct m0_fop_create create;
	struct m0_fop_type   ft;
	int                  rc = 0;
#ifdef CLIENT_FOR_M0T1FS
	char                *str = "HavantFTW";
#endif
	struct m0_fid        fid = { .f_container = 66,
				     .f_key = 77 };
	struct m0_client    *instance = NULL;

	/* initialise client */
	rc = ut_m0_client_init(&instance);
	M0_UT_ASSERT(rc == 0);

	/* base case */
	M0_ALLOC_PTR(cr);
	M0_UT_ASSERT(cr != NULL);
	M0_ALLOC_PTR(cr->cr_cob_attr);
	M0_UT_ASSERT(cr->cr_cob_attr != NULL);

	M0_ALLOC_PTR(oo);
	M0_UT_ASSERT(oo != NULL);

	oo->oo_fid = fid;
	OP_OBJ2CODE(oo) = M0_EO_CREATE;
#ifdef CLIENT_FOR_M0T1FS
	m0_buf_init(&oo->oo_name, str, strlen(str));
#endif

	cr->cr_op    = &oo->oo_oc.oc_op;
	cr->cr_fid   = fid;
	cr->cr_cob_attr->ca_lid = 1;
	cr->cr_cinst = instance;
	cr->cr_opcode = M0_EO_CREATE;

	fop.f_data.fd_data = &create;
	ft.ft_rpc_item_type.rit_opcode = M0_MDSERVICE_CREATE_OPCODE;
	fop.f_type = &ft;

	rc = cob_mds_fop_populate(cr, &fop);
	M0_UT_ASSERT(rc == 0);
	M0_UT_ASSERT(!m0_fid_cmp(&create.c_body.b_tfid, &oo->oo_fid));
	ut_m0_client_fini(&instance);
}

/**
 * Tests obj_filename_to_mds_session().
 */
#ifdef CLIENT_FOR_M0T1FS
static void ut_test_filename_to_mds_session(void)
{
	struct m0_rpc_session      *session;
	struct m0_reqh_service_ctx *ctx;
	char                       *filename = "filename";
	m0_bcount_t                 len;
	struct m0_client           *instance = NULL;
	int                         rc;

	/* initialise client */
	rc = ut_m0_client_init(&instance);
	M0_UT_ASSERT(rc == 0 && instance != NULL);

	len = (m0_bcount_t)strlen(filename);

	/* base case */
	M0_ALLOC_PTR(ctx);
	M0_ALLOC_ARR(instance->m0c_pools_common.pc_mds_map, 1);
	instance->m0c_pools_common.pc_mds_map[0] = ctx;
	instance->m0c_pools_common.pc_nr_svcs[M0_CST_MDS] = 1;
	session =
	     filename_to_mds_session(instance, (unsigned char *)filename, len);
	M0_UT_ASSERT(session == &ctx->sc_rlink.rlk_sess);
	m0_free(instance->m0c_pools_common.pc_mds_map);
	m0_free(ctx);

	/* fini */
	ut_m0_client_fini(&instance);
}
#endif

static void ut_test_cob_mds_send(void)
{
}

static void ut_test_obj_namei_cb_launch(void)
{
}

/**
 * Tests obj_namei_cb_free().
 */
static void ut_test_obj_namei_cb_free(void)
{
	struct m0_op_obj    *oo;
	struct m0_entity     ent;
	struct m0_realm      realm;
	struct m0_client    *instance = NULL;
	int                  rc = 0; /* required */

	/* initialise client */
	rc = ut_m0_client_init(&instance);
	M0_UT_ASSERT(rc == 0);

	/* init */
	ut_realm_entity_setup(&realm, &ent, instance);

	/* base case */
	M0_ALLOC_PTR(oo);
	m0_op_obj_bob_init(oo);
	oo->oo_oc.oc_op.op_size = sizeof *oo;
	oo->oo_oc.oc_op.op_entity = &ent;
	obj_namei_cb_free(&oo->oo_oc);

	/* fini */
	m0_entity_fini(&ent);
	ut_m0_client_fini(&instance);
}

/**
 * Tests obj_namei_cb_fini().
 */
static void ut_test_obj_namei_cb_fini(void)
{
	int               rc = 0; /* required */
	struct m0_op_obj  oo;
	struct m0_realm   realm;
	struct m0_entity  ent;
	struct m0_client *instance = NULL;

	/* initialise client */
	rc = ut_m0_client_init(&instance);
	M0_UT_ASSERT(rc == 0);

	/* init */
	ut_layout_domain_fill(instance);
	ut_layout_init(&ut_layout, &instance->m0c_reqh.rh_ldom);
	ut_realm_entity_setup(&realm, &ent, instance);

	/* base cases */
	M0_SET0(&oo);
	m0_op_obj_bob_init(&oo);
	m0_ast_rc_bob_init(&oo.oo_ar);
	m0_op_common_bob_init(&oo.oo_oc);
	oo.oo_oc.oc_op.op_entity = &ent;
	oo.oo_oc.oc_op.op_size = sizeof oo;

	/* base case: most fields are NULL */
	obj_namei_cb_fini(&oo.oo_oc);


	/* fini */
	m0_entity_fini(&ent);
	ut_layout_fini(&ut_layout, &instance->m0c_reqh.rh_ldom);
	ut_layout_domain_empty(instance);
	ut_m0_client_fini(&instance);
}

struct m0_ut_suite ut_suite_obj;

M0_INTERNAL int ut_object_init(void)
{
#ifndef __KERNEL__
	ut_shuffle_test_order(&ut_suite_obj);
#endif

	m0_fi_enable("m0__obj_pool_version_get", "fake_pool_version");

	return 0;
}

M0_INTERNAL int ut_object_fini(void)
{
	m0_fi_disable("m0__obj_pool_version_get", "fake_pool_version");

	return 0;
}

struct m0_ut_suite ut_suite_obj = {
	.ts_name = "obj-ut",
	.ts_init = ut_object_init,
	.ts_fini = ut_object_fini,
	.ts_tests = {

		/* XXX
		 */
		{ "m0_op_obj_invariant",
			&ut_test_m0_op_obj_invariant},

		/* Set the namespace operation. */
		{ "m0_op_obj_invariant",
			&ut_test_m0_op_obj_invariant},
		{ "m0_op_obj_ast_rc_invariant",
			&ut_test_m0_op_obj_ast_rc_invariant},
		{ "m0_op_common_invariant",
			ut_test_m0_op_common_invariant},
		{ "cob_req_invariant",
			&ut_test_cob_req_invariant},
		{ "entity_namei_op(create)",
			&ut_test_entity_namei_op_create},
		{ "entity_namei_op(delete)",
			&ut_test_entity_namei_op_delete},
		{ "m0_entity_create(object)",
			&ut_test_m0_entity_create},
		{ "m0_entity_delete(object)",
			&ut_test_m0_entity_delete},
		{ "m0__obj_layout_instance_build",
			&ut_test_m0__obj_layout_instance_build},
		{ "m0_fid_print",
				&ut_test_m0_fid_print},
		{ "obj_namei_op_init",
				&ut_test_obj_namei_op_init},
		{ "obj_op_obj_invariant",
				&ut_test_obj_op_obj_invariant},
		{ "obj_op_obj_init",
				&ut_test_obj_op_obj_init},
		{ "obj_op_prepare",
			&ut_test_obj_op_prepare},

		/* Processing a namespace object operation. */
		{ "m0__entity_instance",
			&ut_test_m0__entity_instance},
		{ "m0__oo_instance",
			&ut_test_m0__oo_instance},
		{ "cob_complete_op",
			&ut_test_cob_complete_op},
		{ "cob_fail_op",
			&ut_test_cob_fail_op},
		{ "rpc_item_to_ios_cob_req",
			&ut_test_rpc_item_to_ios_cob_req},
		{ "icr_ast_complete",
			&ut_test_icr_ast_complete},
		{ "icr_ast_fail",
			&ut_test_icr_ast_fail},
		{ "icr_ast_ver_mismatch",
			&ut_test_icr_ast_ver_mismatch},
		{ "cob_rio_ios_replied",
			&ut_test_cob_rio_ios_replied},
		{ "cob_ios_fop_populate",
			&ut_test_cob_ios_fop_populate},
		{ "m0_obj_container_id_to_session",
			&ut_test_m0_obj_container_id_to_session},
		{ "m0_cob_ios_fop_fini",
			&ut_test_m0_cob_ios_fop_fini},
		{ "cob_ios_send",
			&ut_test_cob_ios_send},
		{ "rpc_item_to_cr",
			&ut_test_rpc_item_to_cr},
		{ "cob_ast_fail_oo",
			&ut_test_cob_ast_fail_oo},
		{ "cob_rio_mds_replied",
			&ut_test_cob_rio_mds_replied},
		{ "cob_name_mem2wire",
			&ut_test_cob_name_mem2wire},
		{ "cob_body_mem2wire",
			&ut_test_cob_body_mem2wire},
		{ "cob_mds_fop_populate",
			&ut_test_cob_mds_fop_populate},
#ifdef CLIENT_FOR_M0T1FS
		{ "filename_to_mds_session",
			&ut_test_filename_to_mds_session},
#endif
		{ "cob_mds_send",
			&ut_test_cob_mds_send},
		{ "obj_namei_cb_launch",
			&ut_test_obj_namei_cb_launch},

		/* Freeing a namespace object operation. */
		{ "obj_namei_cb_free",
			&ut_test_obj_namei_cb_free},

		/* Finalising a namespace object operation. */
		{ "obj_namei_cb_fini",
			&ut_test_obj_namei_cb_fini},

		/*
		 * Assignment of optimal layout id for object accoriding to
		 * initial buffer size.
		 */
		{ "obj_optimal_lid_set",
			&ut_test_obj_lid_assign},
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
