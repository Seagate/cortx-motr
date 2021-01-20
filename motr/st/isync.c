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
#include "lib/finject.h"

#include "motr/client.h"
#include "motr/st/st.h"
#include "motr/st/st_misc.h"
#include "motr/st/st_assert.h"

#include "lib/memory.h"

enum { ST_MAX_KEY_LEN = 64 };

#define ST_VAL_STRING  ("Client Index Test.")

enum {
	IDX_CREATE = 0,
	IDX_DELETE,
	IDX_KV_ADD,
	IDX_KV_DEL
};

struct m0_container st_isync_container;

static void idx_bufvec_free(struct m0_bufvec *bv)
{
	uint32_t i;
	if (bv == NULL)
		return;

	if (bv->ov_buf != NULL) {
		for (i = 0; i < bv->ov_vec.v_nr; ++i)
			if (bv->ov_buf[i] != NULL)
				m0_free(bv->ov_buf[i]);
		m0_free(bv->ov_buf);
	}
	m0_free(bv->ov_vec.v_count);
	m0_free(bv);
}

static struct m0_bufvec* idx_bufvec_alloc(int nr)
{
	struct m0_bufvec *bv;

	bv = m0_alloc(sizeof *bv);
	if (bv == NULL)
		return NULL;

	bv->ov_vec.v_nr = nr;
	M0_ALLOC_ARR(bv->ov_vec.v_count, nr);
	if (bv->ov_vec.v_count == NULL)
		goto FAIL;

	M0_ALLOC_ARR(bv->ov_buf, nr);
	if (bv->ov_buf == NULL)
		goto FAIL;

	return bv;

FAIL:
	m0_bufvec_free(bv);
	return NULL;
}

static int idx_fill_kv_pairs(struct m0_uint128 id, int start,
			     struct m0_bufvec *keys,
			     struct m0_bufvec *vals)
{
	int   i;
	int   rc;
	int   nr_kvp;
	int   klen;
	int   vlen;
	char *prefix = NULL;
	char *tmp_str = NULL;
	char *key_str = NULL;
	char *val_str = NULL;

	rc = -ENOMEM;
	prefix = m0_alloc(ST_MAX_KEY_LEN);
	if (prefix == NULL)
		goto ERROR;

	tmp_str = m0_alloc(ST_MAX_KEY_LEN);
	if (tmp_str == NULL)
		goto ERROR;

	/*
	 * Keys are flled with this format (index fid:key's serial number).
	 * Values are a dummy string "Client Index Test."
	 */
	nr_kvp = keys->ov_vec.v_nr;
	vlen = strlen(ST_VAL_STRING);
	for (i = 0; i < nr_kvp; i++) {
		sprintf(tmp_str,
			"%"PRIx64":%"PRIx64":%d",
			id.u_hi, id.u_lo, start + i);

		klen = strlen(tmp_str);
		key_str = m0_alloc(klen);
		if (key_str == NULL)
			goto ERROR;

		val_str = m0_alloc(vlen);
		if (val_str == NULL)
			goto ERROR;

		memcpy(key_str, tmp_str, klen);
		memcpy(val_str, ST_VAL_STRING, vlen);

		/* Set bufvec's of keys and vals*/
		keys->ov_vec.v_count[i] = klen;
		keys->ov_buf[i] = key_str;
		vals->ov_vec.v_count[i] = vlen;
		vals->ov_buf[i] = val_str;
	}

	if (prefix) m0_free(prefix);
	if (tmp_str) m0_free(tmp_str);
	return 0;

ERROR:
	if (prefix) m0_free(prefix);
	if (tmp_str) m0_free(tmp_str);
	if (key_str) m0_free(key_str);
	if (val_str) m0_free(val_str);
	return rc;
}

static int idx_add_or_del_kv_pairs(int opcode, struct m0_uint128 id,
				   int nr_kvp, bool dont_fini_op,
				   struct m0_op **op_out,
				   struct m0_idx **idx_out)
{
	int                   rc = 0;
	int                  *rcs;
	struct m0_bufvec     *keys;
	struct m0_bufvec     *vals;
	struct m0_op         *ops[1] = {NULL};
	struct m0_idx        *idx;

	/* Allocate bufvec's for keys and vals. */
	keys = idx_bufvec_alloc(nr_kvp);
	vals = idx_bufvec_alloc(nr_kvp);
	M0_ALLOC_ARR(rcs, nr_kvp);
	M0_ALLOC_PTR(idx);
	if (keys == NULL || vals == NULL || rcs == NULL || idx == NULL) {
		rc = -ENOMEM;
		goto exit;
	}

	/* Fill keys and values with some data. */
	rc = idx_fill_kv_pairs(id, 0, keys, vals);
	if (rc < 0)
		goto exit;

	/* Start doing the real job. */
	ops[0] = NULL;
	memset(idx, 0, sizeof *idx);

	st_idx_init(idx, &st_isync_container.co_realm, &id);
	if (opcode == IDX_KV_ADD)
		st_idx_op(idx, M0_IC_PUT, keys, vals, rcs,
				 0, &ops[0]);
	else
		st_idx_op(idx, M0_IC_DEL, keys, NULL, rcs, 0,
				 &ops[0]);

	st_op_launch(ops, 1);
	rc = st_op_wait(ops[0],
		    M0_BITS(M0_OS_FAILED,
			    M0_OS_STABLE),
		    M0_TIME_NEVER);
	rc = rc < 0?rc:ops[0]->op_sm.sm_rc;

	if (rc == 0 && dont_fini_op == true) {
		*op_out = ops[0];
		goto exit;
	}

	/* fini and release */
	st_op_fini(ops[0]);
	st_op_free(ops[0]);

exit:
	if (keys) idx_bufvec_free(keys);
	if (vals) idx_bufvec_free(vals);
	if (rcs) m0_free0(&rcs);

	if (rc != 0 || idx_out == NULL) {
		st_idx_fini(idx);
		m0_free(idx);
	} else
		*idx_out = idx;

	return rc;
}

static int idx_create_or_delete(int opcode, struct m0_uint128 id,
				bool dont_fini_op, struct m0_op **op_out,
				struct m0_idx **idx_out)
{
	int            rc;
	struct m0_op  *ops[1] = {NULL};
	struct m0_idx *idx;

	M0_ALLOC_PTR(idx);
	if (idx == NULL)
		return -ENOMEM;
	memset(idx, 0, sizeof *idx);
	ops[0] = NULL;

	/* Set an index creation operation. */
	st_idx_init(idx,
		&st_isync_container.co_realm, &id);
	if (opcode == IDX_CREATE)
		st_entity_create(NULL, &idx->in_entity, &ops[0]);
	else {
		st_idx_open(&idx->in_entity);
		st_entity_delete(&idx->in_entity, &ops[0]);
	}

	/* Launch and wait for op to complete */
	st_op_launch(ops, 1);
	rc = st_op_wait(ops[0],
		    M0_BITS(M0_OS_FAILED,
			    M0_OS_STABLE),
		    M0_TIME_NEVER);
	rc = rc < 0?rc:ops[0]->op_sm.sm_rc;

	if (rc == 0 && dont_fini_op == true) {
		*op_out = ops[0];
		goto exit;
	}

	/* fini and release */
	st_op_fini(ops[0]);
	st_op_free(ops[0]);

exit:
	if (rc != 0 || idx_out == NULL) {
		st_idx_fini(idx);
		m0_free(idx);
	} else
		*idx_out = idx;
	return rc;
}

/**
 * Tests for error handling for an SYNC op such as failures
 * in launching op.
 */
static void isync_error_handling(void)
{
	int               rc;
	int               nr_kvp;
	struct m0_uint128 id;
	struct m0_fid     idx_fid;
	struct m0_op     *sync_op = NULL;
	struct m0_idx    *idx_to_sync[1] = {NULL};

	/* get index's fid. */
	oid_get(&id);
	idx_fid = M0_FID_TINIT('x', id.u_hi, id.u_lo);
	id.u_hi = idx_fid.f_container;
	id.u_lo = idx_fid.f_key;

	/* Create an index. */
	rc = idx_create_or_delete(IDX_CREATE, id, false, NULL, NULL);
	ST_ASSERT_FATAL(rc == 0);

	/* Insert K-V pairs into index. */
	nr_kvp = 10;
	rc = idx_add_or_del_kv_pairs(IDX_KV_ADD, id,
				     nr_kvp, false, NULL, idx_to_sync);
	ST_ASSERT_FATAL(rc == 0);

	/* Launch and wait on sync op. */
	rc = m0_sync_op_init(&sync_op);
	M0_ASSERT(rc == 0);
	rc = m0_sync_entity_add(sync_op, &idx_to_sync[0]->in_entity);
	M0_ASSERT(rc == 0);

	m0_fi_enable_once("sync_request_launch", "launch_failed");
	st_op_launch(&sync_op, 1);
	rc = st_op_wait(
		sync_op, M0_BITS(M0_OS_FAILED, M0_OS_STABLE),
		m0_time_from_now(3,0));
	ST_ASSERT_FATAL(rc == 0);
	st_op_fini(sync_op);
	st_op_free(sync_op);

	/* Finalise the index. */
	st_idx_fini(idx_to_sync[0]);
}
static void isync_by_sync_op(void)
{
	int               rc;
	int               nr_kvp;
	struct m0_uint128 id;
	struct m0_fid     idx_fid;
	struct m0_op     *sync_op = NULL;
	struct m0_idx    *idx_to_sync[1] = {NULL};

	/* get index's fid. */
	oid_get(&id);
	idx_fid = M0_FID_TINIT('x', id.u_hi, id.u_lo);
	id.u_hi = idx_fid.f_container;
	id.u_lo = idx_fid.f_key;

	/* Create an index. */
	rc = idx_create_or_delete(IDX_CREATE, id, false, NULL, NULL);
	ST_ASSERT_FATAL(rc == 0);

	/* Insert K-V pairs into index. */
	nr_kvp = 10;
	rc = idx_add_or_del_kv_pairs(IDX_KV_ADD, id,
				     nr_kvp, false, NULL, idx_to_sync);
	ST_ASSERT_FATAL(rc == 0);

	/* Launch and wait on sync op. */
	rc = m0_sync_op_init(&sync_op);
	M0_ASSERT(rc == 0);
	rc = m0_sync_entity_add(sync_op, &idx_to_sync[0]->in_entity);
	M0_ASSERT(rc == 0);

	st_op_launch(&sync_op, 1);
	rc = st_op_wait(
		sync_op, M0_BITS(M0_OS_FAILED, M0_OS_STABLE),
		m0_time_from_now(3,0));
	ST_ASSERT_FATAL(rc == 0);
	st_op_fini(sync_op);
	st_op_free(sync_op);

	/* Finalise the index. */
	st_idx_fini(idx_to_sync[0]);
}

static void isync_on_op(void)
{
	int               rc;
	int               nr_kvp;
	struct m0_uint128 id;
	struct m0_fid     idx_fid;
	struct m0_op     *sync_op = NULL;
	struct m0_op     *op_to_sync[1] = {NULL};
	struct m0_idx    *idx_to_sync[1] = {NULL};

	/* get index's fid. */
	oid_get(&id);
	idx_fid = M0_FID_TINIT('x', id.u_hi, id.u_lo);
	id.u_hi = idx_fid.f_container;
	id.u_lo = idx_fid.f_key;

	/* Create an index. */
	rc = idx_create_or_delete(IDX_CREATE, id, false, NULL, NULL);
	ST_ASSERT_FATAL(rc == 0);

	/* Insert K-V pairs into index. */
	nr_kvp = 10;
	rc = idx_add_or_del_kv_pairs(IDX_KV_ADD, id, nr_kvp,
				     true, op_to_sync, idx_to_sync);
	ST_ASSERT_FATAL(rc == 0);

	/* Launch and wait on sync op. */
	rc = m0_sync_op_init(&sync_op);
	M0_ASSERT(rc == 0);
	rc = m0_sync_op_add(sync_op, op_to_sync[0]);
	M0_ASSERT(rc == 0);

	st_op_launch(&sync_op, 1);
	rc = st_op_wait(
		sync_op, M0_BITS(M0_OS_FAILED, M0_OS_STABLE),
		m0_time_from_now(3,0));
	ST_ASSERT_FATAL(rc == 0);
	st_op_fini(sync_op);
	st_op_free(sync_op);

	/* Finalise the index. */
	st_idx_fini(idx_to_sync[0]);
}

static void isync_on_idx_delete(void)
{
	int               rc;
	struct m0_uint128 id;
	struct m0_fid     idx_fid;
	struct m0_op     *sync_op = NULL;
	struct m0_idx    *idx_to_sync[1] = {NULL};

	/* get index's fid. */
	oid_get(&id);
	idx_fid = M0_FID_TINIT('x', id.u_hi, id.u_lo);
	id.u_hi = idx_fid.f_container;
	id.u_lo = idx_fid.f_key;

	/* Create an index. */
	rc = idx_create_or_delete(IDX_CREATE, id, false, NULL, NULL);
	ST_ASSERT_FATAL(rc == 0);

	/* Delete this index. */
	rc = idx_create_or_delete(IDX_DELETE, id, false, NULL, idx_to_sync);
	ST_ASSERT_FATAL(rc == 0);

	/* Launch and wait on sync op. */
	rc = m0_sync_op_init(&sync_op);
	M0_ASSERT(rc == 0);
	rc = m0_sync_entity_add(sync_op, &idx_to_sync[0]->in_entity);
	M0_ASSERT(rc == 0);

	st_op_launch(&sync_op, 1);
	rc = st_op_wait(
		sync_op, M0_BITS(M0_OS_FAILED, M0_OS_STABLE),
		m0_time_from_now(3,0));
	ST_ASSERT_FATAL(rc == 0);
	st_op_fini(sync_op);
	st_op_free(sync_op);

	/* Finalise the index. */
	st_idx_fini(idx_to_sync[0]);
}

static void isync_on_kv_delete(void)
{
	int               rc;
	int               nr_kvp;
	struct m0_uint128 id;
	struct m0_fid     idx_fid;
	struct m0_op     *sync_op = NULL;
	struct m0_op     *op_to_sync[1] = {NULL};
	struct m0_idx    *idx_to_sync[1] = {NULL};

	/* get index's fid. */
	oid_get(&id);
	idx_fid = M0_FID_TINIT('x', id.u_hi, id.u_lo);
	id.u_hi = idx_fid.f_container;
	id.u_lo = idx_fid.f_key;

	/* Create an index. */
	rc = idx_create_or_delete(IDX_CREATE, id, false, NULL, NULL);
	ST_ASSERT_FATAL(rc == 0);

	/* Insert K-V pairs into index. */
	nr_kvp = 10;
	rc = idx_add_or_del_kv_pairs(IDX_KV_ADD, id, nr_kvp, false, NULL, NULL);
	ST_ASSERT_FATAL(rc == 0);

	/* Delete a few K-V pairs. */
	nr_kvp = 5;
	rc = idx_add_or_del_kv_pairs(IDX_KV_DEL, id, nr_kvp,
				     true, op_to_sync, idx_to_sync);
	ST_ASSERT_FATAL(rc == 0);

	/* Launch and wait on sync op. */
	rc = m0_sync_op_init(&sync_op);
	M0_ASSERT(rc == 0);
	rc = m0_sync_op_add(sync_op, op_to_sync[0]);
	M0_ASSERT(rc == 0);

	st_op_launch(&sync_op, 1);
	rc = st_op_wait(
		sync_op, M0_BITS(M0_OS_FAILED, M0_OS_STABLE),
		m0_time_from_now(3,0));
	ST_ASSERT_FATAL(rc == 0);
	st_op_fini(sync_op);
	st_op_free(sync_op);

	/* Finalise the index. */
	st_idx_fini(idx_to_sync[0]);
}

/* Initialises the Client environment.*/
static int st_isync_init(void)
{
	int rc = 0;

	/*
	 * Retrieve the uber realm. We don't need to open this,
	 * as realms are not actually implemented yet
	 */
	st_container_init(&st_isync_container,
			      NULL, &M0_UBER_REALM,
			      st_get_instance());
	rc = st_isync_container.co_realm.re_entity.en_sm.sm_rc;

	if (rc != 0)
		console_printf("Failed to open uber realm\n");

	return rc;
}

/* Finalises the Client environment.*/
static int st_isync_fini(void)
{
	return 0;
}

struct st_suite st_suite_isync = {
	.ss_name = "isync_st",
	.ss_init = st_isync_init,
	.ss_fini = st_isync_fini,
	.ss_tests = {
		{ "isync_error_handling",   &isync_error_handling},
		{ "isync_by_sync_op",       &isync_by_sync_op},
		{ "isync_on_op",            &isync_on_op},
		{ "isync_on_idx_delete",    &isync_on_idx_delete},
		{ "isync_on_kv_delete",     &isync_on_kv_delete},
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
