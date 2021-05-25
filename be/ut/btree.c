/* -*- C -*- */
/*
 * Copyright (c) 2013-2020 Seagate Technology LLC and/or its Affiliates
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


#define M0_TRACE_SUBSYSTEM M0_TRACE_SUBSYS_UT
#include "lib/trace.h"

#include "be/tx_group_fom.h"
#include "be/btree.h"
#include "lib/types.h"     /* m0_uint128_eq */
#include "lib/misc.h"      /* M0_BITS, M0_IN */
#include "lib/memory.h"    /* M0_ALLOC_PTR */
#include "lib/errno.h"     /* ENOENT */
#include "be/ut/helper.h"
#include "ut/ut.h"
#ifndef __KERNEL__
#include <stdio.h>	   /* sscanf */
#endif

static struct m0_be_ut_backend *ut_be;
static struct m0_be_ut_seg     *ut_seg;
static struct m0_be_seg        *seg;

extern void btree_dbg_print(struct m0_be_btree *tree);

static int tree_cmp(const void *key0, const void *key1)
{
	return strcmp(key0, key1);
}

static m0_bcount_t tree_kv_size(const void *kv)
{
	return kv != NULL ? strlen(kv) + 1 : 0;
}

static const struct m0_be_btree_kv_ops kv_ops = {
	.ko_type    = M0_BBT_UT_KV_OPS,
	.ko_ksize   = tree_kv_size,
	.ko_vsize   = tree_kv_size,
	.ko_compare = tree_cmp
};

enum {
	INSERT_COUNT = BTREE_FAN_OUT * 20,
	INSERT_KSIZE  = 7,
	INSERT_VSIZE  = 11,
	TXN_OPS_NR = 7,
};

static void check(struct m0_be_btree *tree);

static struct m0_be_btree *create_tree(void);

static void destroy_tree(struct m0_be_btree *tree);
static void truncate_tree(struct m0_be_btree *tree);


void m0_be_ut_btree_create_truncate(void)
{
	struct m0_be_btree *tree0;

	M0_ENTRY();
	M0_ALLOC_PTR(ut_be);
	M0_UT_ASSERT(ut_be != NULL);

	M0_ALLOC_PTR(ut_seg);
	M0_UT_ASSERT(ut_seg != NULL);
	/* Init BE */
	m0_be_ut_backend_init(ut_be);
	m0_be_ut_seg_init(ut_seg, ut_be, 1ULL << 24);
	seg = ut_seg->bus_seg;

	/* create btree */
	tree0 = create_tree();

	m0_be_ut_seg_reload(ut_seg);
	check(tree0);

	/* truncate btree */
	truncate_tree(tree0);

	m0_be_ut_seg_reload(ut_seg);
	m0_be_ut_seg_fini(ut_seg);
	m0_be_ut_backend_fini(ut_be);
	m0_free(ut_seg);
	m0_free(ut_be);

	M0_LEAVE();
}

void m0_be_ut_btree_create_destroy(void)
{
	struct m0_be_btree *tree0;

	M0_ENTRY();
	M0_ALLOC_PTR(ut_be);
	M0_UT_ASSERT(ut_be != NULL);

	M0_ALLOC_PTR(ut_seg);
	M0_UT_ASSERT(ut_seg != NULL);
	/* Init BE */
	m0_be_ut_backend_init(ut_be);
	m0_be_ut_seg_init(ut_seg, ut_be, 1ULL << 24);
	seg = ut_seg->bus_seg;

	/* create btrees */
	tree0 = create_tree();

	m0_be_ut_seg_reload(ut_seg);

	check(tree0);
	destroy_tree(tree0);

	m0_be_ut_seg_reload(ut_seg);
	m0_be_ut_seg_fini(ut_seg);
	m0_be_ut_backend_fini(ut_be);
	m0_free(ut_seg);
	m0_free(ut_be);

	M0_LEAVE();
}

static int
btree_insert(struct m0_be_btree *t, struct m0_buf *k, struct m0_buf *v,
	     int nr_left)
{
	struct m0_be_tx_credit  cred = {};
	static struct m0_be_tx *tx = NULL;
	static int              nr;
	struct m0_be_op         op = {};
	int                     rc;

	M0_ENTRY();

	if (tx == NULL) {
		nr = TXN_OPS_NR;
		M0_ALLOC_PTR(tx);
		M0_ASSERT(tx != NULL);
		m0_be_btree_insert_credit2(t, nr, INSERT_KSIZE   + 1,
					          INSERT_VSIZE*2 + 1, &cred);
		m0_be_ut_tx_init(tx, ut_be);
		m0_be_tx_prep(tx, &cred);

		rc = m0_be_tx_open_sync(tx);
		M0_UT_ASSERT(rc == 0);
	}

	rc = M0_BE_OP_SYNC_RET_WITH(&op, m0_be_btree_insert(t, tx, &op, k, v),
				    bo_u.u_btree.t_rc);

	if (--nr == 0 || nr_left == 0) {
		m0_be_tx_close_sync(tx);
		m0_be_tx_fini(tx);
		m0_free(tx);
		tx = NULL;
	}

	return M0_RC(rc);
}

static int
btree_insert_inplace(struct m0_be_btree *t, struct m0_buf *k, int v,
		     int nr_left)
{
	struct m0_be_tx_credit     cred = {};
	static struct m0_be_tx    *tx = NULL;
	static int                 nr;
	struct m0_be_op            op = {};
	struct m0_be_btree_anchor  anchor;
	int                        rc;

	M0_ENTRY();

	if (tx == NULL) {
		nr = TXN_OPS_NR;
		M0_ALLOC_PTR(tx);
		M0_UT_ASSERT(tx != NULL);
		m0_be_btree_insert_credit2(t, nr, INSERT_KSIZE + 1,
					          INSERT_VSIZE*2 + 1, &cred);
		m0_be_ut_tx_init(tx, ut_be);
		m0_be_tx_prep(tx, &cred);

		rc = m0_be_tx_open_sync(tx);
		M0_UT_ASSERT(rc == 0);
	}

	if ((v & 1) == 0)
		anchor.ba_value.b_nob = INSERT_VSIZE;
	else
		anchor.ba_value.b_nob = INSERT_VSIZE*2;
	rc = M0_BE_OP_SYNC_RET_WITH(&op, m0_be_btree_insert_inplace(t, tx, &op,
				    k, &anchor, M0_BITS(M0_BAP_NORMAL)),
				    bo_u.u_btree.t_rc);
	/* update value */
	if ((v & 1) == 0)
		sprintf(anchor.ba_value.b_addr, "%0*d", INSERT_VSIZE - 1, v);
	else
		sprintf(anchor.ba_value.b_addr, "%0*d", INSERT_VSIZE*2 - 1, v);
	m0_be_btree_release(tx, &anchor);

	if (--nr == 0 || nr_left == 0) {
		m0_be_tx_close_sync(tx);
		m0_be_tx_fini(tx);
		m0_free(tx);
		tx = NULL;
	}

	return M0_RC(rc);
}

static int
btree_delete(struct m0_be_btree *t, struct m0_buf *k, int nr_left)
{
	struct m0_be_tx_credit  cred = {};
	static struct m0_be_tx *tx = NULL;
	static int              nr;
	struct m0_be_op         op = {};
	int                     rc;

	M0_ENTRY();

	if (tx == NULL) {
		nr = TXN_OPS_NR;
		M0_ALLOC_PTR(tx);
		M0_UT_ASSERT(tx != NULL);
		m0_be_btree_delete_credit(t, nr, INSERT_KSIZE+1,
					         INSERT_VSIZE*2 + 1, &cred);
		m0_be_ut_tx_init(tx, ut_be);
		m0_be_tx_prep(tx, &cred);

		rc = m0_be_tx_open_sync(tx);
		M0_UT_ASSERT(rc == 0);
	}

	rc = M0_BE_OP_SYNC_RET_WITH(&op, m0_be_btree_delete(t, tx, &op, k),
				    bo_u.u_btree.t_rc);

	if (--nr == 0 || nr_left == 0) {
		m0_be_tx_close_sync(tx);
		m0_be_tx_fini(tx);
		m0_free(tx);
		tx = NULL;
	}

	return M0_RC(rc);
}

static void btree_kill_one(struct m0_be_btree *tree,
			   const struct m0_buf *key,
			   uint64_t ver)
{
	struct m0_be_tx_credit  cred = {};
	static struct m0_be_tx *tx = NULL;
	struct m0_be_op         op = {};
	int                     rc;

	M0_ALLOC_PTR(tx);
	M0_UT_ASSERT(tx != NULL);
	m0_be_btree_insert_credit(tree, 1, INSERT_KSIZE, INSERT_VSIZE, &cred);
	m0_be_btree_delete_credit(tree, 1, INSERT_KSIZE, INSERT_VSIZE, &cred);
	m0_be_ut_tx_init(tx, ut_be);
	m0_be_tx_prep(tx, &cred);
	rc = m0_be_tx_open_sync(tx);
	M0_UT_ASSERT(rc == 0);
	rc = M0_BE_OP_SYNC_RET_WITH(&op,
	    m0_be_btree_kill(tree, tx, &op, key, ver),
	    bo_u.u_btree.t_rc);
	M0_UT_ASSERT(rc == 0);
	m0_be_tx_close_sync(tx);
	m0_be_tx_fini(tx);
	m0_free(tx);
}

static int btree_lookup_alive(struct m0_be_btree *tree,
			      const struct m0_buf *key,
			      struct m0_buf *val)
{
	int rc;
	struct m0_be_btree_anchor  anchor = {};
	rc = M0_BE_OP_SYNC_RET(op,
		m0_be_btree_lookup_alive_inplace(tree, &op, key, &anchor),
		bo_u.u_btree.t_rc);
	if (rc == 0)
		*val = anchor.ba_value;
	m0_be_btree_release(NULL, &anchor);
	return rc;
}

static int btree_save_ver(struct m0_be_btree *tree,
			  const struct m0_buf *key,
			  const struct m0_buf *val,
			  uint64_t ver,
			  bool overwrite)
{
	int rc;
	struct m0_be_btree_anchor  anchor = {};
	struct m0_be_tx_credit  cred = {};
	static struct m0_be_tx *tx = NULL;
	struct m0_be_op         op = {};

	anchor.ba_value.b_nob = val->b_nob;

	M0_ALLOC_PTR(tx);
	M0_UT_ASSERT(tx != NULL);
	m0_be_btree_insert_credit(tree, 1, INSERT_KSIZE, INSERT_VSIZE, &cred);
	m0_be_btree_delete_credit(tree, 1, INSERT_KSIZE, INSERT_VSIZE, &cred);
	m0_be_ut_tx_init(tx, ut_be);
	m0_be_tx_prep(tx, &cred);
	rc = m0_be_tx_open_sync(tx);
	M0_UT_ASSERT(rc == 0);
	rc = M0_BE_OP_SYNC_RET_WITH(&op,
		m0_be_btree_save_inplace(tree, tx, &op, key, ver, &anchor,
					 overwrite, M0_BITS(M0_BAP_NORMAL)),
		bo_u.u_btree.t_rc);
	M0_UT_ASSERT(rc == 0);
	if (anchor.ba_value.b_addr != NULL)
		m0_buf_memcpy(&anchor.ba_value, val);
	m0_be_btree_release(tx, &anchor);
	m0_be_tx_close_sync(tx);
	m0_be_tx_fini(tx);
	m0_free(tx);
	return rc;
}



static void shuffle_array(int a[], size_t n)
{
	if (n > 1) {
		int      i;
		uint64_t seed;

		seed = 123;
		for (i = 0; i < n - 1; ++i)
			M0_SWAP(a[i], a[m0_rnd(n, &seed)]);
	}
}

static void btree_delete_test(struct m0_be_btree *tree, struct m0_be_tx *tx)
{
	struct m0_buf key;
	struct m0_buf val;
	char          k[INSERT_KSIZE];
	char          v[INSERT_VSIZE*2];
	int          *rand_keys;
	int           rc;
	int           i;

	m0_buf_init(&key, k, INSERT_KSIZE);
	m0_buf_init(&val, v, INSERT_VSIZE);

	M0_LOG(M0_INFO, "Check error code...");
	sprintf(k, "%0*d", INSERT_KSIZE-1, INSERT_COUNT);

	rc = btree_delete(tree, &key, 0);
	M0_UT_ASSERT(rc == -ENOENT);

	btree_dbg_print(tree);

	M0_LOG(M0_INFO, "Delete random keys...");
	M0_ALLOC_ARR(rand_keys, INSERT_COUNT);
	M0_UT_ASSERT(rand_keys != NULL);
	for (i = 0; i < INSERT_COUNT; ++i)
		rand_keys[i] = i;
	shuffle_array(rand_keys, INSERT_COUNT);
	for (i = 0; i < INSERT_COUNT; i+=2) {
		sprintf(k, "%0*d", INSERT_KSIZE-1, rand_keys[i]);
		M0_LOG(M0_DEBUG, "%04d: delete key=%s", i, (char*)k);
		rc = btree_delete(tree, &key, INSERT_COUNT - i - 2);
		M0_UT_ASSERT(rc == 0);
	}

	M0_LOG(M0_INFO, "Make sure nothing deleted is left...");
	for (i = 0; i < INSERT_COUNT; i+=2) {
		sprintf(k, "%0*d", INSERT_KSIZE-1, rand_keys[i]);
		rc = btree_delete(tree, &key, INSERT_COUNT - i - 2);
		M0_UT_ASSERT(rc == -ENOENT);
	}

	M0_LOG(M0_INFO, "Insert back all deleted stuff...");
	for (i = 0; i < INSERT_COUNT; i+=2) {
		sprintf(k, "%0*d", INSERT_KSIZE-1, rand_keys[i]);
		if ((rand_keys[i] & 1) == 0) {
			m0_buf_init(&val, v, INSERT_VSIZE);
			sprintf(v, "%0*d", INSERT_VSIZE-1, rand_keys[i]);
		} else {
			m0_buf_init(&val, v, INSERT_VSIZE*2);
			sprintf(v, "%0*d", INSERT_VSIZE*2 - 1, rand_keys[i]);
		}
		rc = btree_insert(tree, &key, &val, INSERT_COUNT - i - 2);
		M0_UT_ASSERT(rc == 0);
	}

	M0_LOG(M0_INFO, "Delete everything in random order...");
	for (i = 0; i < INSERT_COUNT; i++) {
		sprintf(k, "%0*d", INSERT_KSIZE-1, rand_keys[i]);
		M0_LOG(M0_DEBUG, "%04d: delete key=%s", i, (char*)k);
		rc = btree_delete(tree, &key, INSERT_COUNT - i - 1);
		M0_UT_ASSERT(rc == 0);
	}

	M0_LOG(M0_INFO, "Make sure nothing is left...");
	for (i = 0; i < INSERT_COUNT; i++) {
		sprintf(k, "%0*d", INSERT_KSIZE-1, i);
		rc = btree_delete(tree, &key, INSERT_COUNT - i - 1);
		M0_UT_ASSERT(rc == -ENOENT);
	}

	M0_LOG(M0_INFO, "Insert everything back...");
	for (i = 0; i < INSERT_COUNT; i++) {
		sprintf(k, "%0*d", INSERT_KSIZE-1, rand_keys[i]);
		if ((rand_keys[i] & 1) == 0) {
			m0_buf_init(&val, v, INSERT_VSIZE);
			sprintf(v, "%0*d", INSERT_VSIZE-1, rand_keys[i]);
		} else {
			m0_buf_init(&val, v, INSERT_VSIZE*2);
			sprintf(v, "%0*d", INSERT_VSIZE*2 - 1, rand_keys[i]);
		}
		rc = btree_insert(tree, &key, &val, INSERT_COUNT - i - 1);
		M0_UT_ASSERT(rc == 0);
	}
	m0_free(rand_keys);

	M0_LOG(M0_INFO, "Deleting [%04d, %04d)...", INSERT_COUNT/4,
						    INSERT_COUNT*3/4);
	for (i = INSERT_COUNT/4; i < INSERT_COUNT*3/4; ++i) {
		sprintf(k, "%0*d", INSERT_KSIZE-1, i);
		M0_LOG(M0_DEBUG, "delete key=%04d", i);
		rc = btree_delete(tree, &key, INSERT_COUNT*3/4 - i - 1);
		M0_UT_ASSERT(rc == 0);
	}

	M0_LOG(M0_INFO, "Check double delete in-the-middle...");
	sprintf(k, "%0*d", INSERT_KSIZE-1, INSERT_COUNT/5 & ~1);
	M0_LOG(M0_DEBUG, "delete key=%s", (char*)k);
	rc = btree_delete(tree, &key, 0);
	M0_UT_ASSERT(rc == 0);
	M0_LOG(M0_DEBUG, "delete key=%s", (char*)k);
	rc = btree_delete(tree, &key, 0);
	M0_UT_ASSERT(rc == -ENOENT);
	M0_LOG(M0_INFO, "Insert it back.");
	m0_buf_init(&val, v, INSERT_VSIZE);
	sprintf(v, "%0*d", INSERT_VSIZE-1, INSERT_COUNT/5 & ~1);
	btree_insert(tree, &key, &val, 0);
}

/* Check if the cursor is pointing at a kv that has the expected data. */
static void cursor_pos_verify(struct m0_be_btree_cursor *cur, int pos,
			      bool verify_value)
{
	struct m0_buf actual_k;
	struct m0_buf actual_v;
	struct m0_buf expected_k;
	struct m0_buf expected_v;
	char          ev[INSERT_VSIZE * 2];
	char          ek[INSERT_KSIZE];
	int           rc;

	m0_be_btree_cursor_kv_get(cur, &actual_k, &actual_v);

	m0_buf_init(&expected_k, ek, INSERT_KSIZE);
	rc = sprintf(ek, "%0*d", INSERT_KSIZE-1, pos);
	M0_ASSERT(rc > 0 && rc < sizeof(ek) + 1);

	M0_UT_ASSERT(m0_buf_eq(&actual_k, &expected_k));

	if (verify_value) {
		if ((pos & 1) == 0) {
			m0_buf_init(&expected_v, ev, INSERT_VSIZE);
			rc = sprintf(ev, "%0*d", INSERT_VSIZE - 1, pos);
			M0_ASSERT(rc > 0 && rc < sizeof(ev) + 1);
		} else {
			m0_buf_init(&expected_v, ev, INSERT_VSIZE*2);
			rc = sprintf(ev, "%0*d", INSERT_VSIZE*2 - 1, pos);
			M0_ASSERT(rc > 0 && rc < sizeof(ev) + 1);
		}

		M0_UT_ASSERT(m0_buf_eq(&actual_v, &expected_v));
	}
}

/*
 * XXX: These functions are not exported yet, so that
 * we define them right here. Later on we may consider
 * moving them into the public API.
 */
#if 1
static uint64_t value2bpv(struct m0_buf *value, int key_size)
{
	uint64_t offset = m0_align(key_size, sizeof(void*)) + sizeof(uint64_t);
	return *((uint64_t *) (value->b_addr - offset));
}

static uint64_t value2version(struct m0_buf *value, int key_size)
{
	return value2bpv(value, key_size) & ~(1L << 63);
}

static bool value2tbs(struct m0_buf *value, int key_size)
{
	return !!(value2bpv(value, key_size) & (1L << 63));
}
#endif

static void btree_tbs_insert_delete(struct m0_be_btree *tree, int pos)
{
	enum value { PRESERVED, CHANGED, EMPTY };
	enum op { PUT, DEL, NONE };
	enum ver { PAST = 2, FUTURE = 3, };
	enum tbs { ALIVE, DEAD };

	struct tbs_case {
		enum ver   v_before;
		enum op    op_before;

		enum op    op_after;
		enum ver   v_after;

		enum value o_value;
		enum tbs   o_tbs;
		enum ver   o_ver;
		int        o_rc;
	};

#define BEFORE(_Op, _Ver) .v_before = _Ver, .op_before = _Op,
#define AFTER(_Op, _Ver) .v_after = _Ver, .op_after = _Op,
#define OUTCOME(_Rc, _Ver, _Tbs, _Val) .o_rc = _Rc, .o_tbs = _Tbs, \
	.o_value = _Val, .o_ver = _Ver,

	static const struct tbs_case cases[] = {
		/* PUT@past + DEL@future => OK (remove) */
		{ BEFORE(PUT, PAST) AFTER(DEL, FUTURE)
			OUTCOME(0, FUTURE, DEAD, EMPTY) },

		/* PUT@past + PUT@future => OK (overwrite) */
		{ BEFORE(PUT, PAST) AFTER(PUT, FUTURE)
			OUTCOME(0, FUTURE, ALIVE, CHANGED) },

		/* PUT@future + PUT@past => OK (value@now = value@future) */
		{ BEFORE(PUT, FUTURE) AFTER(PUT, PAST)
			OUTCOME(0, FUTURE, ALIVE, PRESERVED) },

		/* PUT@future + DEL@past => OK (value@now = value@future) */
		{ BEFORE(PUT, FUTURE) AFTER(DEL, PAST)
			OUTCOME(0, FUTURE, ALIVE, PRESERVED) },

		/* DEL@future + DEL@past => OK (ensure tombstone is set) */
		{ BEFORE(DEL, FUTURE) AFTER(DEL, PAST)
			OUTCOME(0, FUTURE, DEAD, EMPTY) },

		/* DEL@past + DEL@future => OK (ensure tombstone is set) */
		{ BEFORE(DEL, PAST) AFTER(DEL, FUTURE)
			OUTCOME(0, FUTURE, DEAD, EMPTY) },

		/* DEL@future + DEL@past => OK (ensure tombstone is set) */
		{ BEFORE(DEL, FUTURE) AFTER(DEL, PAST)
			OUTCOME(0, FUTURE, DEAD, EMPTY) },

		/* DEL@future + PUT@past => OK (value@now = value@future) */
		{ BEFORE(DEL, FUTURE) AFTER(PUT, PAST)
			OUTCOME(0, FUTURE, DEAD, EMPTY) },

		/* DEL@past + PUT@future => OK (re-insert) */
		{ BEFORE(DEL, PAST) AFTER(PUT, FUTURE)
			OUTCOME(0, FUTURE, ALIVE, CHANGED) },

		/* No key + DEL => OK (ensure tombstone is set)*/
		{ BEFORE(NONE, PAST) AFTER(DEL, FUTURE)
			OUTCOME(0, FUTURE, DEAD, EMPTY) },
	};
#undef BEFORE
#undef AFTER
#undef OUTCOME

	const struct tbs_case    *tc;
	int                       i;
	int                       rc;
	struct m0_buf             key;
	struct m0_buf             old_val;
	struct m0_buf             new_val;
	struct m0_buf             actual_val;
	bool                      is_dead;
	uint64_t                  actual_ver;
	char                      k[INSERT_KSIZE];
	char                      newv[INSERT_VSIZE*2];
	char                      oldv[INSERT_VSIZE*2];
	struct m0_be_btree_anchor anchor;

	for (i = 0; i < ARRAY_SIZE(cases); i++) {
		tc = &cases[i];

		rc = sprintf(k, "%0*d", INSERT_KSIZE-1, pos);
		M0_ASSERT(rc > 0 && rc < sizeof(k) + 1);
		m0_buf_init(&key, k, rc + 1);

		rc = sprintf(oldv, "%0*d", INSERT_VSIZE*2 - 1, pos);
		M0_ASSERT(rc > 0 && rc < sizeof(oldv) + 1);
		m0_buf_init(&old_val, oldv, rc + 1);

		rc = sprintf(newv, "%0*d", INSERT_VSIZE-1, pos);
		M0_ASSERT(rc > 0 && rc < sizeof(newv) + 1);
		m0_buf_init(&new_val, newv, rc + 1);

		actual_val = M0_BUF_INIT0;

		rc = btree_delete(tree, &key, 0);
		M0_UT_ASSERT(M0_IN(rc, (0, -ENOENT)));

		if (tc->op_before == PUT)
			(void) btree_save_ver(tree, &key, &old_val, tc->v_before,
					    true);
		else if (tc->op_before == DEL)
			btree_kill_one(tree, &key, tc->v_before);
		else
			M0_UT_ASSERT(tc->op_before == NONE);

		if (tc->op_after == PUT)
			(void) btree_save_ver(tree, &key, &new_val, tc->v_after,
					    true);
		else if (tc->op_after == DEL)
			btree_kill_one(tree, &key, tc->v_after);
		else
			M0_IMPOSSIBLE();

		/* We do not have any cases where errors are expected. */
		M0_UT_ASSERT(tc->o_rc == 0);

		rc = btree_lookup_alive(tree, &key, &actual_val);

		M0_UT_ASSERT(ergo(tc->o_value == EMPTY, rc == -ENOENT));
		M0_UT_ASSERT(ergo(tc->o_tbs == DEAD, rc == -ENOENT));
		M0_UT_ASSERT(ergo(tc->o_tbs == ALIVE && tc->o_value != EMPTY,
				  rc == 0));

		M0_SET0(&anchor);

		/* We should end up with something inserted either way. */
		rc = M0_BE_OP_SYNC_RET(op,
			m0_be_btree_lookup_inplace(tree, &op, &key, &anchor),
			bo_u.u_btree.t_rc);

		actual_val = anchor.ba_value;
		actual_ver = value2version(&actual_val, key.b_nob);
		is_dead = value2tbs(&actual_val, key.b_nob);

		if (tc->o_value == PRESERVED)
			M0_UT_ASSERT(m0_buf_eq(&actual_val, &old_val));
		else if (tc->o_value == CHANGED)
			M0_UT_ASSERT(m0_buf_eq(&actual_val, &new_val));
		else if (tc->o_value == EMPTY) {
			/*
			 * XXX: The non-version-aware functions (for example,
			 * lookup_inplace) do not know that the pair was
			 * deleted, therefore it may return some old value
			 * written by one of the previous PUTs.
			 * Because of that we have nothing to do here.
			 */
		} else
			M0_IMPOSSIBLE();

		M0_UT_ASSERT(actual_ver == tc->o_ver);
		M0_UT_ASSERT(equi(is_dead, tc->o_tbs == DEAD));

		m0_be_btree_release(NULL, &anchor);
	}
}

/*
 * This test case does a small portion of sanity testing of
 * kill(), lookup_alive(), save_inplace() and btree cursor.
 * Additionally, it verifies a set of INSERT-DELETE tests
 * with different combinations of operations and versions.
 */
static void btree_tbs_ver_test(struct m0_be_btree *tree)
{

	struct m0_buf             key;
	struct m0_buf             val;
	struct m0_buf             actual_val = {};
	struct m0_buf             nv;
	char                      k[INSERT_KSIZE];
	char                      v[INSERT_VSIZE*2];
	char                      nextv[INSERT_VSIZE*2];
	char                      actv[INSERT_VSIZE*2];
	int                       rc;
	int                       i;
	struct m0_be_btree_cursor cursor = {};

	enum { YESTERDAY = 1, TODAY = 2, TOMORROW = 3, };

	m0_buf_init(&key, k, INSERT_KSIZE);
	m0_buf_init(&val, v, INSERT_VSIZE);
	m0_buf_init(&nv,  nextv, INSERT_VSIZE);
	m0_buf_init(&actual_val,  actv, INSERT_VSIZE);

	rc = sprintf(k, "%0*d", INSERT_KSIZE-1, INSERT_COUNT - 1);
	M0_ASSERT(rc > 0 && rc < sizeof(k) + 1);
	rc = sprintf(v, "%s", "VALUE00000");
	M0_ASSERT(rc > 0 && rc < sizeof(v) + 1);
	rc = sprintf(nextv, "%s", "VALUE11111");
	M0_ASSERT(rc > 0 && rc < sizeof(nextv) + 1);

	/* We have a key ... */
	rc = M0_BE_OP_SYNC_RET(
		op, m0_be_btree_lookup(tree, &op, &key, &actual_val),
		bo_u.u_btree.t_rc);
	M0_UT_ASSERT(rc == 0);

	/* and we want to delete it, */
	rc = btree_delete(tree, &key, 0);
	M0_UT_ASSERT(rc == 0);

	/* and we want to ensure it was deleted. */
	rc = M0_BE_OP_SYNC_RET(
		op, m0_be_btree_lookup(tree, &op, &key, &actual_val),
		bo_u.u_btree.t_rc);
	M0_UT_ASSERT(rc == -ENOENT);

	/* Now, kill a record and */
	btree_kill_one(tree, &key, YESTERDAY);

	/* ensure it is not alive. */
	rc = btree_lookup_alive(tree, &key, &actual_val);
	M0_UT_ASSERT(rc == -ENOENT);

	/* The record still has to be visible. */
	rc = M0_BE_OP_SYNC_RET(
		op, m0_be_btree_lookup(tree, &op, &key, &actual_val),
		bo_u.u_btree.t_rc);
	M0_UT_ASSERT(rc == 0);

	/* Delete the record, so that we can play with versions. */
	rc = btree_delete(tree, &key, 0);
	M0_UT_ASSERT(rc == 0);
	rc = M0_BE_OP_SYNC_RET(
		op, m0_be_btree_lookup(tree, &op, &key, &actual_val),
		bo_u.u_btree.t_rc);
	M0_UT_ASSERT(rc == -ENOENT);

	/* Insert (K1,V1)@1 */
	rc = btree_save_ver(tree, &key, &val, YESTERDAY, true);
	M0_UT_ASSERT(rc == 0);

	rc = M0_BE_OP_SYNC_RET(
		op, m0_be_btree_lookup(tree, &op, &key, &actual_val),
		bo_u.u_btree.t_rc);
	M0_UT_ASSERT(rc == 0);
	M0_UT_ASSERT(m0_buf_eq(&actual_val, &val));

	/* Insert (K1,V1)@2 */
	rc = btree_save_ver(tree, &key, &val, TODAY, true);
	M0_UT_ASSERT(rc == 0);

	rc = M0_BE_OP_SYNC_RET(
		op, m0_be_btree_lookup(tree, &op, &key, &actual_val),
		bo_u.u_btree.t_rc);
	M0_UT_ASSERT(rc == 0);
	M0_UT_ASSERT(m0_buf_eq(&actual_val, &val));

	/* Try to insert (K1,V2)@1 */
	rc = btree_save_ver(tree, &key, &nv, YESTERDAY, true);
	M0_UT_ASSERT(rc == 0);

	rc = M0_BE_OP_SYNC_RET(
		op, m0_be_btree_lookup(tree, &op, &key, &actual_val),
		bo_u.u_btree.t_rc);
	M0_UT_ASSERT(rc == 0);
	M0_UT_ASSERT(m0_buf_eq(&actual_val, &val));

	/* Ensure (K1,V2)@1 was not inserted */
	rc = btree_lookup_alive(tree, &key, &actual_val);
	M0_UT_ASSERT(rc == 0);

	rc = btree_delete(tree, &key, 0);
	M0_UT_ASSERT(rc == 0);
	rc = btree_insert(tree, &key, &val, 0);
	M0_UT_ASSERT(rc == 0);

	/*
	 * Now let's try to insert some tombstones. Then
	 * we will try to iterate over them.
	 */

	/* Deelete a handful of records somewhere in the middle */
	for (i = 1; i < 11; i++) {
		rc = sprintf(k, "%0*d", INSERT_KSIZE-1, i);
		M0_ASSERT(rc > 0 && rc < sizeof(k) + 1);
		rc = btree_delete(tree, &key, 0);
		M0_UT_ASSERT(rc == 0);
	}


	/* insert 3 entries: pair1@1,pair2-tombstone@1,pair3@1 */

	/* pair1 */
	rc = sprintf(k, "%0*d", INSERT_KSIZE-1, 1);
	M0_ASSERT(rc > 0 && rc < sizeof(k) + 1);
	rc = sprintf(v, "%0*d", INSERT_VSIZE*2-1, 1);
	M0_ASSERT(rc > 0 && rc < sizeof(v) + 1);
	m0_buf_init(&val, v, INSERT_VSIZE*2);
	rc = btree_save_ver(tree, &key, &val, 1, true);
	M0_UT_ASSERT(rc == 0);

	/* pair3 */
	rc = sprintf(k, "%0*d", INSERT_KSIZE-1, 3);
	M0_ASSERT(rc > 0 && rc < sizeof(k) + 1);
	rc = sprintf(v, "%0*d", INSERT_VSIZE*2-1, 3);
	M0_ASSERT(rc > 0 && rc < sizeof(v) + 1);
	m0_buf_init(&val, v, INSERT_VSIZE*2);
	rc = btree_save_ver(tree, &key, &val, 1, true);
	M0_UT_ASSERT(rc == 0);

	/* pair2-tombstone */
	rc = sprintf(k, "%0*d", INSERT_KSIZE-1, 2);
	M0_ASSERT(rc > 0 && rc < sizeof(k) + 1);
	btree_kill_one(tree, &key, 1);

	/* Now let's try to find pair1@1 and the let's see what we can find. */

	m0_be_btree_cursor_init(&cursor, tree);
	rc = sprintf(k, "%0*d", INSERT_KSIZE-1, 1);
	M0_ASSERT(rc > 0 && rc < sizeof(k) + 1);
	rc = M0_BE_OP_SYNC_RET_WITH(&cursor.bc_op,
			      m0_be_btree_cursor_alive_get(&cursor, &key, true),
			      bo_u.u_btree.t_rc);
	M0_UT_ASSERT(rc == 0);
	cursor_pos_verify(&cursor, 1, true);
	M0_SET0(&cursor.bc_op);

	rc = M0_BE_OP_SYNC_RET_WITH(&cursor.bc_op,
				      m0_be_btree_cursor_alive_next(&cursor),
				      bo_u.u_btree.t_rc);
	M0_UT_ASSERT(rc == 0);
	cursor_pos_verify(&cursor, 3, true);
	M0_SET0(&cursor.bc_op);

	/* Let's check if we skip tombstones */
	m0_be_btree_cursor_init(&cursor, tree);
	rc = sprintf(k, "%0*d", INSERT_KSIZE-1, 2);
	M0_ASSERT(rc > 0 && rc < sizeof(k) + 1);
	rc = M0_BE_OP_SYNC_RET_WITH(&cursor.bc_op,
			      m0_be_btree_cursor_alive_get(&cursor, &key, true),
			      bo_u.u_btree.t_rc);
	M0_UT_ASSERT(rc == 0);
	cursor_pos_verify(&cursor, 3, true);
	M0_SET0(&cursor.bc_op);

	/* Ensure an ordinary cursor sees dead pairs */
	m0_be_btree_cursor_init(&cursor, tree);
	rc = sprintf(k, "%0*d", INSERT_KSIZE-1, 2);
	M0_ASSERT(rc > 0 && rc < sizeof(k) + 1);
	rc = M0_BE_OP_SYNC_RET_WITH(&cursor.bc_op,
			      m0_be_btree_cursor_get(&cursor, &key, false),
			      bo_u.u_btree.t_rc);
	M0_UT_ASSERT(rc == 0);
	cursor_pos_verify(&cursor, 2, false);
	M0_SET0(&cursor.bc_op);

	/* Do a series of insert-delete tests. */
	btree_tbs_insert_delete(tree, 1);

	/* Restore the handful of records that we have deleted initialy. */
	for (i = 1; i < 11; i++) {
		m0_buf_init(&key, k, INSERT_KSIZE);
		rc = sprintf(k, "%0*d", INSERT_KSIZE-1, i);
		M0_ASSERT(rc > 0 && rc < sizeof(k) + 1);
		rc = btree_delete(tree, &key, 0);
		M0_UT_ASSERT(M0_IN(rc, (0, -ENOENT)));
		if ((i & 1) == 0) {
			m0_buf_init(&val, v, INSERT_VSIZE);
			rc = sprintf(v, "%0*d", INSERT_VSIZE-1, i);
			M0_ASSERT(rc > 0 && rc < sizeof(v) + 1);
		} else {
			m0_buf_init(&val, v, INSERT_VSIZE*2);
			rc = sprintf(v, "%0*d", INSERT_VSIZE*2 - 1, i);
			M0_ASSERT(rc > 0 && rc < sizeof(v) + 1);
		}
		rc = btree_insert(tree, &key, &val, 0);
		M0_UT_ASSERT(rc == 0);

	}
}


static int btree_save(struct m0_be_btree *tree, struct m0_buf *k,
		      struct m0_buf *v, bool overwrite)
{
	struct m0_be_tx        *tx;
	struct m0_be_tx_credit  cred = {};
	struct m0_be_op         op = {};
	int                     rc;

	M0_ALLOC_PTR(tx);
	M0_UT_ASSERT(tx != NULL);
	m0_be_btree_insert_credit(tree, 1, INSERT_KSIZE, INSERT_VSIZE, &cred);
	if (overwrite)
		m0_be_btree_delete_credit(tree, 1, INSERT_KSIZE, INSERT_VSIZE,
					  &cred);
	m0_be_ut_tx_init(tx, ut_be);
	m0_be_tx_prep(tx, &cred);

	rc = m0_be_tx_open_sync(tx);
	M0_UT_ASSERT(rc == 0);
	rc = M0_BE_OP_SYNC_RET_WITH(&op,
			m0_be_btree_save(tree, tx, &op, k, v, overwrite),
			bo_u.u_btree.t_rc);
	m0_be_tx_close_sync(tx);
	m0_be_tx_fini(tx);
	m0_free(tx);
	return rc;
}

static void btree_save_test(struct m0_be_btree *tree)
{
	struct m0_buf    key;
	struct m0_buf    val;
	struct m0_buf    ret_val;
	struct m0_be_op *op;
	char             k[INSERT_KSIZE];
	char             v[INSERT_VSIZE];
	char             r[INSERT_VSIZE];
	int              rc;

	M0_ALLOC_PTR(op);
	M0_ASSERT(op != NULL);

	m0_buf_init(&key, k, sizeof k);
	m0_buf_init(&val, v, sizeof v);
	m0_buf_init(&ret_val, r, sizeof r);

	/* Hope that a0a0 is not in the tree. */
	sprintf(k, "%0*x", INSERT_KSIZE-1, 0xa0a0);
	sprintf(v, "%0*x", INSERT_VSIZE-1, 0xa0a0);

	/* Check that key is not already inserted. */
	rc = M0_BE_OP_SYNC_RET_WITH(
		op, m0_be_btree_lookup(tree, op, &key, &ret_val),
		bo_u.u_btree.t_rc);
	M0_UT_ASSERT(rc == -ENOENT);

	M0_LOG(M0_INFO, "Save new key...");
	rc = btree_save(tree, &key, &val, false);
	M0_UT_ASSERT(rc == 0);
	btree_dbg_print(tree);
	M0_SET0(op);
	rc = M0_BE_OP_SYNC_RET_WITH(
		op, m0_be_btree_lookup(tree, op, &key, &ret_val),
		bo_u.u_btree.t_rc);
	M0_UT_ASSERT(rc == 0);
	M0_UT_ASSERT(strcmp(ret_val.b_addr, v) == 0);

	M0_LOG(M0_INFO, "Save existing key without overwrite flag...");
	rc = btree_save(tree, &key, &val, false);
	M0_UT_ASSERT(rc == -EEXIST);

	M0_LOG(M0_INFO, "Save existing key with overwrite flag...");
	sprintf(v, "%0*x", INSERT_VSIZE-1, 0xb0b0);
	rc = btree_save(tree, &key, &val, true);
	M0_UT_ASSERT(rc == 0);
	btree_dbg_print(tree);
	M0_SET0(op);
	rc = M0_BE_OP_SYNC_RET_WITH(
		op, m0_be_btree_lookup(tree, op, &key, &ret_val),
		bo_u.u_btree.t_rc);
	M0_UT_ASSERT(rc == 0);
	M0_UT_ASSERT(strcmp(ret_val.b_addr, v) == 0);

	M0_LOG(M0_INFO, "Cleanup the key...");
	btree_delete(tree, &key, 0);
}

static struct m0_be_btree *create_tree(void)
{
	struct m0_be_tx_credit *cred;
	struct m0_be_btree     *tree;
	struct m0_be_tx        *tx;
	struct m0_buf           key;
	struct m0_buf           val;
	char                    k[INSERT_KSIZE];
	char                    v[INSERT_VSIZE * 2];
	char                    v2[INSERT_VSIZE * 3];
	struct m0_be_op        *op;
	int                     rc;
	int                     i;

	M0_ENTRY();

	M0_ALLOC_PTR(cred);
	M0_UT_ASSERT(cred != NULL);

	{ /* XXX: should calculate these credits not for dummy tree,
	   but for allocated below. This needs at least two transactions. */
		struct m0_be_btree *t = M0_ALLOC_PTR(t);
		M0_UT_ASSERT(t != NULL);
		*t = (struct m0_be_btree) { .bb_seg = seg };
		m0_be_btree_create_credit(t, 1, cred);
		m0_free(t);
	}
	M0_BE_ALLOC_CREDIT_PTR(tree, seg, cred);

	M0_ALLOC_PTR(op);
	M0_UT_ASSERT(op != NULL);
	M0_ALLOC_PTR(tx);
	M0_UT_ASSERT(tx != NULL);
	m0_be_ut_tx_init(tx, ut_be);
	m0_be_tx_prep(tx, cred);
	rc = m0_be_tx_open_sync(tx);
	M0_UT_ASSERT(rc == 0);

	/* start */
	M0_BE_ALLOC_PTR_SYNC(tree, seg, tx);
	m0_be_btree_init(tree, seg, &kv_ops);

	M0_BE_OP_SYNC_WITH(op,
		   m0_be_btree_create(tree, tx, op, &M0_FID_TINIT('b', 0, 1)));
	M0_UT_ASSERT(m0_fid_eq(&tree->bb_backlink.bli_fid,
			       &M0_FID_TINIT('b', 0, 1)));
	M0_UT_ASSERT(m0_be_btree_is_empty(tree));
	m0_be_tx_close_sync(tx); /* Make things persistent. */
	m0_be_tx_fini(tx);

	M0_SET0(op);
	rc = M0_BE_OP_SYNC_RET_WITH(op, m0_be_btree_minkey(tree, op, &key),
	                            bo_u.u_btree.t_rc);
	M0_UT_ASSERT(rc == -ENOENT && key.b_addr == NULL && key.b_nob == 0);

	M0_SET0(op);
	rc = M0_BE_OP_SYNC_RET_WITH(op, m0_be_btree_maxkey(tree, op, &key),
	                            bo_u.u_btree.t_rc);
	M0_UT_ASSERT(rc == -ENOENT && key.b_addr == NULL && key.b_nob == 0);

	m0_buf_init(&key, k, INSERT_KSIZE);
	M0_LOG(M0_INFO, "Inserting...");
	/* insert */
	for (i = 0; i < INSERT_COUNT/2; ++i) {
		sprintf(k, "%0*d", INSERT_KSIZE-1, i);
		if ((i & 1) == 0) {
			m0_buf_init(&val, v, INSERT_VSIZE);
			sprintf(v, "%0*d", INSERT_VSIZE - 1, i);
		} else {
			m0_buf_init(&val, v, INSERT_VSIZE*2);
			sprintf(v, "%0*d", INSERT_VSIZE*2 - 1, i);
		}
		btree_insert(tree, &key, &val, INSERT_COUNT/2 - i - 1);
	}
	M0_UT_ASSERT(!m0_be_btree_is_empty(tree));

	M0_LOG(M0_INFO, "Inserting inplace...");
	/* insert inplace */
	for (i = INSERT_COUNT/2; i < INSERT_COUNT; ++i) {
		sprintf(k, "%0*d", INSERT_KSIZE-1, i);
		btree_insert_inplace(tree, &key, i, INSERT_COUNT - i - 1);
	}
	btree_dbg_print(tree);

	btree_delete_test(tree, tx);
	btree_tbs_ver_test(tree);
	btree_save_test(tree);
	M0_LOG(M0_INFO, "Updating...");
	m0_be_ut_tx_init(tx, ut_be);
	*cred = M0_BE_TX_CREDIT(0, 0);
	m0_be_btree_update_credit(tree, 1, INSERT_VSIZE, cred);
	m0_be_tx_prep(tx, cred);
	rc = m0_be_tx_open_sync(tx);
	M0_UT_ASSERT(rc == 0);

	sprintf(k, "%0*d", INSERT_KSIZE-1, INSERT_COUNT - 1);
	sprintf(v, "XYZ");
	val.b_nob = 4;
	M0_SET0(op);
	M0_BE_OP_SYNC_WITH(op, m0_be_btree_update(tree, tx, op, &key, &val));

	m0_be_tx_close_sync(tx); /* Make things persistent. */
	m0_be_tx_fini(tx);

	btree_dbg_print(tree);

	M0_LOG(M0_INFO, "Updating with longer value...");
	m0_be_ut_tx_init(tx, ut_be);
	*cred = M0_BE_TX_CREDIT(0, 0);
	m0_be_btree_update_credit2(tree, 1, INSERT_KSIZE, INSERT_VSIZE * 3,
				   cred);
	m0_be_tx_prep(tx, cred);
	rc = m0_be_tx_open_sync(tx);
	M0_UT_ASSERT(rc == 0);

	sprintf(k, "%0*d", INSERT_KSIZE-1, INSERT_COUNT - 2);
	snprintf(v2, sizeof v2, "%s", "ABCDEFGHI");
	m0_buf_init(&val, v2, strlen(v2)+1);
	M0_SET0(op);
	M0_BE_OP_SYNC_WITH(op, m0_be_btree_update(tree, tx, op, &key, &val));

	m0_be_tx_close_sync(tx); /* Make things persistent. */
	m0_be_tx_fini(tx);
	m0_free(tx);
	m0_free(op);
	m0_free(cred);

	btree_dbg_print(tree);

	M0_LEAVE();
	return tree;
}

static void truncate_tree(struct m0_be_btree *tree)
{
	struct m0_be_tx_credit  cred = {};
	struct m0_be_tx        *tx;
	struct m0_be_op        *op;
	struct m0_buf           key;
	char                    k[INSERT_KSIZE];
	int                     rc;

	M0_ENTRY();

	m0_buf_init(&key, k, sizeof k);
	m0_be_btree_destroy_credit(tree, &cred);
	M0_BE_FREE_CREDIT_PTR(tree, seg, &cred);

	M0_ALLOC_PTR(tx);
	M0_UT_ASSERT(tx != NULL);
	M0_ALLOC_PTR(op);
	M0_ASSERT(op != NULL);
	m0_be_ut_tx_init(tx, ut_be);
	m0_be_tx_prep(tx, &cred);

	rc = m0_be_tx_open_sync(tx);
	M0_UT_ASSERT(rc == 0);
	btree_dbg_print(tree);
	M0_BE_OP_SYNC_WITH(op, m0_be_btree_truncate(tree, tx,
							op, INSERT_COUNT));
	M0_UT_ASSERT(m0_be_btree_is_empty(tree));
	M0_BE_FREE_PTR_SYNC(tree, seg, tx);
	m0_be_tx_close_sync(tx); /* Make things persistent. */
	m0_be_tx_fini(tx);
	m0_free(op);
	m0_free(tx);
	M0_LEAVE();
}

static void destroy_tree(struct m0_be_btree *tree)
{
	struct m0_be_tx_credit	cred = {};
	struct m0_be_tx        *tx;
	struct m0_be_op        *op;
	struct m0_buf           key;
	char                    k[INSERT_KSIZE];
	int                     rc;
	int                     i;

	M0_ENTRY();

	m0_buf_init(&key, k, sizeof k);

	M0_LOG(M0_INFO, "Delete everything...");
	for (i = 0; i < INSERT_COUNT; i++) {
		sprintf(k, "%0*d", INSERT_KSIZE-1, i);
		btree_delete(tree, &key, INSERT_COUNT - i - 1);
	}

	m0_be_btree_destroy_credit(tree, &cred);
	M0_BE_FREE_CREDIT_PTR(tree, seg, &cred);

	M0_ALLOC_PTR(tx);
	M0_UT_ASSERT(tx != NULL);
	M0_ALLOC_PTR(op);
	M0_ASSERT(op != NULL);
	m0_be_ut_tx_init(tx, ut_be);
	m0_be_tx_prep(tx, &cred);

	rc = m0_be_tx_open_sync(tx);
	M0_UT_ASSERT(rc == 0);

	M0_LOG(M0_INFO, "Btree %p destroy...", tree);
	M0_BE_OP_SYNC_WITH(op, m0_be_btree_destroy(tree, tx, op));
	btree_dbg_print(tree);
	M0_BE_FREE_PTR_SYNC(tree, seg, tx);
	m0_be_tx_close_sync(tx); /* Make things persistent. */
	m0_be_tx_fini(tx);
	m0_free(op);
	m0_free(tx);
	M0_LEAVE();
}

static void cursor_test(struct m0_be_btree *tree)
{
	/* the structure is too large for kernel stack to be local */
	static struct m0_be_btree_cursor *cursor;
	struct m0_buf                    key;
	struct m0_buf                    val;
	char                             sbuf[INSERT_KSIZE+INSERT_VSIZE];
	struct m0_buf                    start = M0_BUF_INIT(sizeof sbuf, sbuf);
	int                              v;
	int                              i;
	int                              rc;

	M0_ALLOC_PTR(cursor);
	M0_UT_ASSERT(cursor != NULL);

	m0_be_btree_cursor_init(cursor, tree);

	sprintf(sbuf, "%0*d", INSERT_KSIZE-1, INSERT_COUNT/2);
	rc = m0_be_btree_cursor_get_sync(cursor, &start, true);
	M0_UT_ASSERT(rc == 0);

	m0_be_btree_cursor_kv_get(cursor, &key, &val);

	for (i = 0; i < INSERT_COUNT/4; ++i) {
		sscanf(key.b_addr, "%d", &v);
		M0_LOG(M0_DEBUG, "i=%i k=%d", i, v);
		M0_UT_ASSERT(v == i + INSERT_COUNT*3/4);

		M0_SET0(&cursor->bc_op);
		m0_be_op_init(&cursor->bc_op);
		m0_be_btree_cursor_next(cursor);
		m0_be_op_wait(&cursor->bc_op);
		rc = cursor->bc_op.bo_u.u_btree.t_rc;
		m0_be_op_fini(&cursor->bc_op);
		if (i < INSERT_COUNT/4 - 1)
			M0_UT_ASSERT(rc == 0);

		m0_be_btree_cursor_kv_get(cursor, &key, &val);
	}

	M0_UT_ASSERT(key.b_addr == NULL);
	M0_UT_ASSERT(rc == -ENOENT);

	sprintf(sbuf, "%0*d", INSERT_KSIZE-1, INSERT_COUNT/4 - 1);
	rc = m0_be_btree_cursor_get_sync(cursor, &start, false);
	M0_UT_ASSERT(rc == 0);

	m0_be_btree_cursor_kv_get(cursor, &key, &val);

	for (i = INSERT_COUNT/4 - 1; i >= 0; --i) {
		sscanf(key.b_addr, "%d", &v);
		M0_LOG(M0_DEBUG, "i=%i k=%d", i, v);
		M0_UT_ASSERT(v == i);

		M0_SET0(&cursor->bc_op);
		m0_be_op_init(&cursor->bc_op);
		m0_be_btree_cursor_prev(cursor);
		m0_be_op_wait(&cursor->bc_op);
		rc = cursor->bc_op.bo_u.u_btree.t_rc;
		m0_be_op_fini(&cursor->bc_op);
		if (i > 0)
			M0_UT_ASSERT(rc == 0);

		m0_be_btree_cursor_kv_get(cursor, &key, &val);
	}

	M0_UT_ASSERT(key.b_addr == NULL);
	M0_UT_ASSERT(rc == -ENOENT);

	sprintf(sbuf, "%0*d", INSERT_KSIZE-1, INSERT_COUNT);
	/* just to avoid CppCheck warning about changing unused sbuf below */
	start = M0_BUF_INIT(sizeof sbuf, sbuf);
	rc = m0_be_btree_cursor_get_sync(cursor, &start, true);
	M0_UT_ASSERT(rc == -ENOENT);

	rc = m0_be_btree_cursor_last_sync(cursor);
	M0_UT_ASSERT(rc == 0);
	m0_be_btree_cursor_kv_get(cursor, &key, NULL);
	sprintf(sbuf, "%0*d", INSERT_KSIZE-1, INSERT_COUNT -1);
	M0_UT_ASSERT(strcmp(key.b_addr, sbuf) == 0);

	rc = m0_be_btree_cursor_first_sync(cursor);
	M0_UT_ASSERT(rc == 0);
	m0_be_btree_cursor_kv_get(cursor, &key, &val);
	sprintf(sbuf, "%0*d", INSERT_KSIZE-1, 0);
	M0_UT_ASSERT(strcmp(key.b_addr, sbuf) == 0);
	sprintf(sbuf, "%0*d", INSERT_VSIZE-1, 0);
	M0_UT_ASSERT(strcmp(val.b_addr, sbuf) == 0);

	m0_be_btree_cursor_fini(cursor);
	m0_free(cursor);
}

static void check(struct m0_be_btree *tree)
{
	struct m0_be_op *op;
	struct m0_buf    key;
	struct m0_buf    val;
	char             k[INSERT_KSIZE];
	char             v[INSERT_VSIZE * 2];
	char             v2[INSERT_VSIZE * 3];
	char             s[INSERT_VSIZE * 2];
	int              i;
	int              rc;

	M0_ALLOC_PTR(op);
	M0_ASSERT(op != NULL);

	m0_be_btree_init(tree, seg, &kv_ops);

	m0_buf_init(&key, k, INSERT_KSIZE);

	/* lookup */
	for (i = 0; i < INSERT_COUNT; ++i) {
		sprintf(k, "%0*d", INSERT_KSIZE-1, i);
		M0_SET0(op);

		if (i == INSERT_COUNT - 2)
			m0_buf_init(&val, v2, ARRAY_SIZE(v2));
		else
			m0_buf_init(&val, v, INSERT_VSIZE*2);

		rc = M0_BE_OP_SYNC_RET_WITH(
			op, m0_be_btree_lookup(tree, op, &key, &val),
			bo_u.u_btree.t_rc);

		if (INSERT_COUNT/4 <= i && i < INSERT_COUNT*3/4)
			M0_UT_ASSERT(rc == -ENOENT);
		else if (i == INSERT_COUNT - 1)
			M0_UT_ASSERT(strcmp(v, "XYZ") == 0);
		else if (i == INSERT_COUNT - 2)
			M0_UT_ASSERT(strcmp(v2, "ABCDEFGHI") == 0);
		else {
			if ((i & 1) == 0) {
				sprintf(s, "%0*d", INSERT_VSIZE-1, i);
				M0_UT_ASSERT(strcmp(v, s) == 0);
			} else {
				sprintf(s, "%0*d", INSERT_VSIZE*2 - 1, i);
				M0_UT_ASSERT(strcmp(v, s) == 0);
			}
		}
	}

	/* lookup inplace */
	for (i = 0; i < INSERT_COUNT; ++i) {
		struct m0_be_btree_anchor anchor;

		sprintf(k, "%0*d", INSERT_KSIZE-1, i);
		M0_SET0(op);
		rc = M0_BE_OP_SYNC_RET_WITH(
			op,
			m0_be_btree_lookup_inplace(tree, op, &key, &anchor),
			bo_u.u_btree.t_rc);
		val = anchor.ba_value;

		if (INSERT_COUNT/4 <= i && i < INSERT_COUNT*3/4)
			M0_UT_ASSERT(rc == -ENOENT);
		else if (i == INSERT_COUNT - 1)
			M0_UT_ASSERT(strcmp(val.b_addr, "XYZ") == 0);
		else if (i == INSERT_COUNT - 2)
			M0_UT_ASSERT(strcmp(val.b_addr, "ABCDEFGHI") == 0);
		else {
			if ((i & 1) == 0) {
				sprintf(s, "%0*d", INSERT_VSIZE-1, i);
				M0_UT_ASSERT(strcmp(val.b_addr, s) == 0);
			} else {
				sprintf(s, "%0*d", INSERT_VSIZE*2 - 1, i);
				M0_UT_ASSERT(strcmp(val.b_addr, s) == 0);
			}
		}

		m0_be_btree_release(NULL, &anchor);
	}

	M0_SET0(op);
	rc = M0_BE_OP_SYNC_RET_WITH(op, m0_be_btree_minkey(tree, op, &key),
	                            bo_u.u_btree.t_rc);
	M0_UT_ASSERT(rc == 0);
	sprintf(s, "%0*d", INSERT_KSIZE-1, 0);
	M0_UT_ASSERT(strcmp(key.b_addr, s) == 0);

	sprintf(k, "%0*d", INSERT_KSIZE-1, INSERT_COUNT - 1);
	M0_SET0(op);
	rc = M0_BE_OP_SYNC_RET_WITH(op, m0_be_btree_maxkey(tree, op, &key),
	                            bo_u.u_btree.t_rc);
	M0_UT_ASSERT(rc == 0);
	M0_UT_ASSERT(strcmp(key.b_addr, k) == 0);

	cursor_test(tree);
	btree_dbg_print(tree);
	m0_be_btree_fini(tree);
	m0_free(op);
}

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
/*
 * vim: tabstop=8 shiftwidth=8 noexpandtab textwidth=80 nowrap
 */
