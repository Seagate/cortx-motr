/* -*- C -*- */
/*
 * Copyright (c) 2011-2021 Seagate Technology LLC and/or its Affiliates
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


#define M0_TRACE_SUBSYSTEM M0_TRACE_SUBSYS_EXTMAP
#include "lib/trace.h"
#include "lib/arith.h"    /* M0_3WAY, m0_uint128 */
#include "lib/errno.h"     /* ENOENT */
#include "lib/vec.h"
#include "lib/types.h"
#include "lib/ub.h"
#include "lib/misc.h"
#include "lib/finject.h"
#include "ut/ut.h"
#include "be/ut/helper.h"
#include "be/extmap.h"

#define EXTMAP_UT_UNIT_SIZE 10
#define EXTMAP_UT_CS_SIZE   16

static struct m0_be_ut_backend be_ut_emap_backend;
static struct m0_be_ut_seg     be_ut_emap_seg;

static struct m0_be_tx          tx1;
static struct m0_be_tx          tx2;
static struct m0_be_emap       *emap;
static struct m0_uint128        prefix;
static struct m0_be_emap_cursor it;
static struct m0_be_emap_seg   *seg; /* cursor segment */
static struct m0_be_seg        *be_seg;
static struct m0_be_op         *it_op;

static void emap_be_alloc(struct m0_be_tx *tx)
{
	struct m0_be_tx_credit cred = {};
	int		       rc;

	M0_BE_ALLOC_CREDIT_PTR(emap, be_seg, &cred);

	m0_be_ut_tx_init(tx, &be_ut_emap_backend);
	m0_be_tx_prep(tx, &cred);

	rc = m0_be_tx_open_sync(tx);
	M0_UT_ASSERT(rc == 0);

	M0_BE_ALLOC_ALIGN_PTR_SYNC(emap, 12, be_seg, tx);
	M0_UT_ASSERT(emap != NULL);

	m0_be_tx_close_sync(tx);
	m0_be_tx_fini(tx);
}

static void emap_be_free(struct m0_be_tx *tx)
{
	struct m0_be_tx_credit cred = {};
	int		       rc;

	M0_BE_FREE_CREDIT_PTR(emap, be_seg, &cred);

	m0_be_ut_tx_init(tx, &be_ut_emap_backend);
	m0_be_tx_prep(tx, &cred);

	rc = m0_be_tx_open_sync(tx);
	M0_UT_ASSERT(rc == 0);

	M0_BE_FREE_PTR_SYNC(emap, be_seg, tx);

	m0_be_tx_close_sync(tx);
	m0_be_tx_fini(tx);
}

/* XXX DELETEME? */
static void checkpoint(void)
{
}

static void test_obj_init(struct m0_be_tx *tx)
{
	M0_BE_OP_SYNC(op, m0_be_emap_obj_insert(emap, tx, &op, &prefix, 42));
	checkpoint();
}

static void test_obj_fini(struct m0_be_tx *tx)
{
	int rc;

	rc = M0_BE_OP_SYNC_RET(
		op,
		m0_be_emap_obj_delete(emap, tx, &op, &prefix),
		bo_u.u_emap.e_rc);
	M0_UT_ASSERT(rc == 0);
	checkpoint();
}

static void test_init(void)
{
	struct m0_be_domain_cfg *cfg;
	struct m0_be_tx_credit	cred = {};
	int			rc;

	M0_ENTRY();

	/* Init BE */
	/** XXX @todo break UT into small transactions */
	M0_ALLOC_PTR(cfg);
	M0_UT_ASSERT(cfg != NULL);
	M0_SET0(cfg);
	M0_SET0(&be_ut_emap_backend);
	M0_SET0(&be_ut_emap_seg);

	m0_be_ut_backend_cfg_default(cfg);
	cfg->bc_engine.bec_tx_size_max = M0_BE_TX_CREDIT(1 << 21, 1 << 26);
	cfg->bc_engine.bec_group_cfg.tgc_size_max =
		M0_BE_TX_CREDIT(1 << 22, 1 << 27);
	rc = m0_be_ut_backend_init_cfg(&be_ut_emap_backend, cfg, true);
	M0_UT_ASSERT(rc == 0);
	m0_be_ut_seg_init(&be_ut_emap_seg, &be_ut_emap_backend, 1ULL << 26);
	be_seg = be_ut_emap_seg.bus_seg;

	emap_be_alloc(&tx1);
	emap->em_seg = be_seg;

	m0_be_emap_credit(emap, M0_BEO_CREATE, 1, &cred);
	m0_be_ut_tx_init(&tx2, &be_ut_emap_backend);
	m0_be_tx_prep(&tx2, &cred);
	rc = m0_be_tx_open_sync(&tx2);
	M0_UT_ASSERT(rc == 0);

	M0_BE_OP_SYNC(op, m0_be_emap_create(emap, &tx2, &op,
					    &M0_FID_INIT(0,1)));

	m0_be_tx_close_sync(&tx2);
	m0_be_tx_fini(&tx2);

	m0_be_emap_credit(emap, M0_BEO_DESTROY, 1, &cred);
	m0_be_emap_credit(emap, M0_BEO_INSERT, 1, &cred);
	m0_be_emap_credit(emap, M0_BEO_DELETE, 1, &cred);
	m0_forall(i, 5, m0_be_emap_credit(emap, M0_BEO_SPLIT, 3, &cred), true);
	m0_be_emap_credit(emap, M0_BEO_MERGE, 5 * 3, &cred);
	m0_be_emap_credit(emap, M0_BEO_PASTE, 3 * 5, &cred);

	m0_be_ut_tx_init(&tx2, &be_ut_emap_backend);
	m0_be_tx_prep(&tx2, &cred);
	rc = m0_be_tx_open_sync(&tx2);
	M0_UT_ASSERT(rc == 0);

	m0_uint128_init(&prefix, "some random iden");
	seg = m0_be_emap_seg_get(&it);
	it_op = m0_be_emap_op(&it);

	it.ec_unit_size = EXTMAP_UT_UNIT_SIZE;

	m0_free(cfg);

	M0_LEAVE();
}

static void test_fini(void)
{
	M0_BE_OP_SYNC(op, m0_be_emap_destroy(emap, &tx2, &op));

	m0_be_tx_close_sync(&tx2);
	m0_be_tx_fini(&tx2);

	emap_be_free(&tx1);

	m0_be_ut_seg_fini(&be_ut_emap_seg);
	m0_be_ut_backend_fini(&be_ut_emap_backend);
}

static int be_emap_lookup(struct m0_be_emap        *map,
			  const struct m0_uint128  *prefix,
			  m0_bindex_t               offset,
			  struct m0_be_emap_cursor *it)
{
	int rc;

	M0_SET0(&it->ec_op);
	m0_be_op_init(&it->ec_op);
	m0_be_emap_lookup(emap, prefix, offset, it);
	m0_be_op_wait(&it->ec_op);
	rc = it->ec_op.bo_u.u_emap.e_rc;
	m0_be_op_fini(&it->ec_op);

	return rc;
}

static void test_lookup(void)
{
	int rc;
	rc = be_emap_lookup(emap, &prefix, 0, &it);
	M0_UT_ASSERT(rc == 0);
	M0_UT_ASSERT(m0_be_emap_ext_is_first(&seg->ee_ext));
	M0_UT_ASSERT(m0_be_emap_ext_is_last(&seg->ee_ext));
	M0_UT_ASSERT(seg->ee_val == 42);
	M0_UT_ASSERT(seg->ee_cksum_buf.b_nob == 0);

	m0_be_emap_close(&it);

	rc = be_emap_lookup(emap, &prefix, 1000000, &it);
	M0_UT_ASSERT(rc == 0);
	M0_UT_ASSERT(m0_be_emap_ext_is_first(&seg->ee_ext));
	M0_UT_ASSERT(m0_be_emap_ext_is_last(&seg->ee_ext));
	M0_UT_ASSERT(seg->ee_val == 42);
	M0_UT_ASSERT(seg->ee_cksum_buf.b_nob == 0);

	m0_be_emap_close(&it);

	++prefix.u_lo;
	rc = be_emap_lookup(emap, &prefix, 0, &it);
	M0_UT_ASSERT(rc == -ENOENT);
	--prefix.u_lo;

	m0_be_emap_close(&it);

	checkpoint();
}

static void split(m0_bindex_t offset, int nr, bool commit)
{
	int i;
	int rc;
	m0_bcount_t len[] = { 100, 50, 0, 0 };
	uint64_t    val[] = { 1,   2, 3, 4 };
	struct m0_indexvec vec = {
		.iv_vec = {
			.v_nr    = ARRAY_SIZE(len),
			.v_count = len
		},
		.iv_index = val
	};

	struct m0_buf          cksum[4] = { {0, NULL},
					    {0, NULL},
					    {0, NULL},
					    {0, NULL}};

	rc = be_emap_lookup(emap, &prefix, offset, &it);
	M0_UT_ASSERT(rc == 0);

	m0_buf_alloc(&cksum[0], (EXTMAP_UT_CS_SIZE * len[0])/EXTMAP_UT_UNIT_SIZE);
	m0_buf_alloc(&cksum[1], (EXTMAP_UT_CS_SIZE * len[1])/EXTMAP_UT_UNIT_SIZE);

	memset(cksum[0].b_addr, 'A', cksum[0].b_nob);
	memset(cksum[1].b_addr, 'B', cksum[1].b_nob);

	M0_LOG(M0_INFO, "off=%lu nr=%d", (unsigned long)offset, nr);
	for (i = 0; i < nr; ++i) {
		m0_bcount_t seglen;
		m0_bcount_t total;

		seglen = m0_ext_length(&seg->ee_ext);
		M0_LOG(M0_DEBUG, "%3i: seglen=%llx", i,
					(unsigned long long)seglen);
		total  = len[0]+len[1]; /* 100 + 50, the sum of elements in len[]. */
		M0_UT_ASSERT(seglen > total);
		len[ARRAY_SIZE(len) - 1] = seglen - total;
		M0_SET0(it_op);
		m0_be_op_init(it_op);
		m0_be_emap_split(&it, &tx2, &vec, cksum);
		m0_be_op_wait(it_op);
		M0_UT_ASSERT(it.ec_op.bo_u.u_emap.e_rc == 0);
		m0_be_op_fini(it_op);
		M0_UT_ASSERT(m0_ext_length(&seg->ee_ext) ==
						len[ARRAY_SIZE(len) - 1]);
	}

	m0_be_emap_close(&it);
	m0_buf_free(&cksum[0]);
	m0_buf_free(&cksum[1]);
	if (commit)
		checkpoint();
}

static void test_split(void)
{
	split(0, 5, true);
}

static int test_print(void)
{
	int i, j;
	int rc;

	rc = be_emap_lookup(emap, &prefix, 0, &it);
	M0_UT_ASSERT(rc == 0);

	M0_LOG(M0_DEBUG, U128X_F":", U128_P(&prefix));
	for (i = 0; ; ++i) {
		M0_LOG(M0_DEBUG, "\t%5.5i %16lx .. %16lx: %16lx %10lx", i,
		       (unsigned long)seg->ee_ext.e_start,
		       (unsigned long)seg->ee_ext.e_end,
		       (unsigned long)m0_ext_length(&seg->ee_ext),
		       (unsigned long)seg->ee_val);

		M0_LOG(M0_DEBUG,"Number of bytes for checksum %lu", (unsigned long)seg->ee_cksum_buf.b_nob);

		if (seg->ee_cksum_buf.b_nob > 0) {
			char array[seg->ee_cksum_buf.b_nob + 1];
			for (j = 0; j < seg->ee_cksum_buf.b_nob; j++) {
				array[j] = *(char *)(seg->ee_cksum_buf.b_addr + j);
			}
			array[j] = '\0';
			M0_LOG(M0_DEBUG, "checksum value %s", (char *)array);
		}

		if (m0_be_emap_ext_is_last(&seg->ee_ext))
			break;
		M0_SET0(it_op);
		m0_be_op_init(it_op);
		m0_be_emap_next(&it);
		m0_be_op_wait(it_op);
		M0_UT_ASSERT(it_op->bo_u.u_emap.e_rc == 0);
		m0_be_op_fini(it_op);
	}
	m0_be_emap_close(&it);

	return i;
}

static void test_next_prev(void)
{
	int i;
	int rc;
	int n;

	n = test_print();

	rc = be_emap_lookup(emap, &prefix, 0, &it);
	M0_UT_ASSERT(rc == 0);

	m0_fi_enable_once("be_emap_changed", "yes");
	for (i = 0; ; ++i) {
		if (m0_be_emap_ext_is_last(&seg->ee_ext))
			break;
		M0_SET0(it_op);
		m0_be_op_init(it_op);
		m0_be_emap_next(&it);
		m0_be_op_wait(it_op);
		M0_UT_ASSERT(it_op->bo_u.u_emap.e_rc == 0);
		m0_be_op_fini(it_op);
	}
	M0_UT_ASSERT(i == n);

	m0_fi_enable_once("be_emap_changed", "yes");
	for (i = 0; ; ++i) {
		if (m0_be_emap_ext_is_first(&seg->ee_ext))
			break;
		M0_SET0(it_op);
		m0_be_op_init(it_op);
		m0_be_emap_prev(&it);
		m0_be_op_wait(it_op);
		M0_UT_ASSERT(it_op->bo_u.u_emap.e_rc == 0);
		m0_be_op_fini(it_op);
	}
	M0_UT_ASSERT(i == n);
	m0_be_emap_close(&it);

}

static void test_merge(void)
{
	int rc;

	M0_LOG(M0_INFO, "Merge all segments...");
	rc = be_emap_lookup(emap, &prefix, 0, &it);
	M0_UT_ASSERT(rc == 0);

	while (!m0_be_emap_ext_is_last(&seg->ee_ext)) {
		M0_SET0(it_op);
		m0_be_op_init(it_op);
		m0_be_emap_merge(&it, &tx2, m0_ext_length(&seg->ee_ext));
		m0_be_op_wait(it_op);
		M0_UT_ASSERT(it_op->bo_u.u_emap.e_rc == 0);
		m0_be_op_fini(it_op);
	}
	m0_be_emap_close(&it);
	checkpoint();
}

static void test_paste(void)
{
	int		 rc, e_val;
	struct m0_ext	 e3, e2, e1, e;
	struct m0_buf   cksum = {};

	rc = be_emap_lookup(emap, &prefix, 0, &it);
	M0_UT_ASSERT(rc == 0);

	e.e_start = 10;
	e.e_end   = 20;
	e1 = e;
	e_val = 12;

	m0_buf_alloc(&cksum, (EXTMAP_UT_CS_SIZE * m0_ext_length(&e))/EXTMAP_UT_UNIT_SIZE);
	memset(cksum.b_addr, 'C', cksum.b_nob);
        it.ec_unit_size = EXTMAP_UT_UNIT_SIZE;
	M0_LOG(M0_INFO, "Paste [%d, %d)...", (int)e.e_start, (int)e.e_end);
	M0_SET0(it_op);
	m0_be_op_init(it_op);
	m0_buf_init(&it.ec_app_cksum_buf, cksum.b_addr, cksum.b_nob);
	m0_be_emap_paste(&it, &tx2, &e1, e_val, NULL, NULL, NULL);
	m0_be_op_wait(it_op);
	M0_UT_ASSERT(it_op->bo_u.u_emap.e_rc == 0);
	m0_be_op_fini(it_op);

	M0_UT_ASSERT(seg->ee_ext.e_start == e.e_end);
	M0_UT_ASSERT(seg->ee_ext.e_end   == M0_BINDEX_MAX + 1);

	test_print();

	rc = be_emap_lookup(emap, &prefix, 0, &it);
	M0_UT_ASSERT(rc == 0);

	M0_UT_ASSERT(seg->ee_ext.e_start ==  0);
	M0_UT_ASSERT(seg->ee_ext.e_end   == e.e_start );
	M0_UT_ASSERT(seg->ee_cksum_buf.b_nob == 0);

	rc = be_emap_lookup(emap, &prefix, e.e_start, &it);
	M0_UT_ASSERT(rc == 0);
	M0_UT_ASSERT(seg->ee_cksum_buf.b_nob == cksum.b_nob);
	M0_UT_ASSERT(memcmp(seg->ee_cksum_buf.b_addr, cksum.b_addr, cksum.b_nob) == 0);

	M0_UT_ASSERT(seg->ee_ext.e_start == e.e_start);
	M0_UT_ASSERT(seg->ee_ext.e_end   == e.e_end );
	M0_UT_ASSERT(seg->ee_val         == e_val);

	rc = be_emap_lookup(emap, &prefix, e.e_end, &it);
	M0_UT_ASSERT(rc == 0);

	M0_UT_ASSERT(seg->ee_ext.e_start == e.e_end );
	M0_UT_ASSERT(seg->ee_ext.e_end   == M0_BINDEX_MAX + 1);
	M0_UT_ASSERT(seg->ee_cksum_buf.b_nob == 0);

	rc = be_emap_lookup(emap, &prefix, 0, &it);
	M0_UT_ASSERT(rc == 0);

	m0_buf_free(&cksum);
	it.ec_app_cksum_buf.b_nob = 0;
	it.ec_app_cksum_buf.b_addr = NULL;

	e.e_start = 5;
	e.e_end   = 25;
	e2 = e;
	e_val = 11;

	M0_LOG(M0_INFO, "Paste [%d, %d)...", (int)e.e_start, (int)e.e_end);
	M0_SET0(it_op);
	m0_be_op_init(it_op);
	m0_be_emap_paste(&it, &tx2, &e2, e_val, NULL, NULL, NULL);
	m0_be_op_wait(it_op);
	M0_UT_ASSERT(it_op->bo_u.u_emap.e_rc == 0);
	m0_be_op_fini(it_op);

	M0_UT_ASSERT(seg->ee_ext.e_start == e.e_end);
	M0_UT_ASSERT(seg->ee_ext.e_end   == M0_BINDEX_MAX + 1);

	test_print();

	rc = be_emap_lookup(emap, &prefix, 0, &it);
	M0_UT_ASSERT(rc == 0);

	M0_UT_ASSERT(seg->ee_ext.e_start == 0);
	M0_UT_ASSERT(seg->ee_ext.e_end   == e.e_start);

	rc = be_emap_lookup(emap, &prefix, e.e_start, &it);
	M0_UT_ASSERT(rc == 0);

	M0_UT_ASSERT(seg->ee_ext.e_start ==  e.e_start);
	M0_UT_ASSERT(seg->ee_ext.e_end   == e.e_end);
	M0_UT_ASSERT(seg->ee_val         == e_val);

	rc = be_emap_lookup(emap, &prefix, e.e_end, &it);
	M0_UT_ASSERT(rc == 0);

	M0_UT_ASSERT(seg->ee_ext.e_start == e.e_end);
	M0_UT_ASSERT(seg->ee_ext.e_end   == M0_BINDEX_MAX + 1);

	rc = be_emap_lookup(emap, &prefix, 0, &it);
	M0_UT_ASSERT(rc == 0);

	e.e_start = 0;
	e.e_end   = M0_BINDEX_MAX + 1;
	e3 = e;

	M0_LOG(M0_INFO, "Paste [%d, %d)...", (int)e.e_start, (int)e.e_end);
	M0_SET0(it_op);
	m0_be_op_init(it_op);
	m0_be_emap_paste(&it, &tx2, &e3, 0, NULL, NULL, NULL);
	m0_be_op_wait(it_op);
	M0_UT_ASSERT(it_op->bo_u.u_emap.e_rc == 0);
	m0_be_op_fini(it_op);

	test_print();

	m0_be_emap_close(&it);
}

/* This UT will write :
 * 1. 50 - 100 with CS = A
 * 2.100 - 150 with CS = B
 * 3. 80 - 130 with CS = P
 * Validate the segement written along with checksum value
 * Using this as reference other cases can be created e.g.
 * - 50 - 100 with CS = A and then 70 -  90 with CS = P
 * - 50 - 100 with CS = A and then 90 - 120 with CS = P
 * - 50 - 100 with CS = A and then 20 -  70 with CS = P
 */
static void test_paste_checksum_validation(void)
{
	int		 rc;
	int 	 idx;
	int 	 e_val[3];
	struct m0_ext	e_temp[3], e;
	struct m0_ext	es[3];
	struct m0_buf   cksum[3] = {};

	rc = be_emap_lookup(emap, &prefix, 0, &it);
	M0_UT_ASSERT(rc == 0);
	M0_UT_ASSERT(seg->ee_ext.e_start == 0);
	M0_UT_ASSERT(seg->ee_ext.e_end   == M0_BINDEX_MAX + 1);

	idx = 0;
	e.e_start = 50;
	e.e_end   = 100;
	es[idx] = e_temp[idx] = e;
	e_val[idx] = 12;

	m0_buf_alloc(&cksum[idx], (EXTMAP_UT_CS_SIZE *
					 	    	m0_ext_length(&e))/EXTMAP_UT_UNIT_SIZE);
	memset(cksum[idx].b_addr, 'A', cksum[idx].b_nob);
    it.ec_unit_size = EXTMAP_UT_UNIT_SIZE;

	M0_LOG(M0_INFO, "Paste [%d, %d)...", (int)e.e_start, (int)e.e_end);
	M0_SET0(it_op);
	m0_be_op_init(it_op);
	m0_buf_init(&it.ec_app_cksum_buf, cksum[idx].b_addr, cksum[idx].b_nob);
	m0_be_emap_paste(&it, &tx2, &e_temp[idx], e_val[idx], NULL, NULL, NULL);
	m0_be_op_wait(it_op);
	M0_UT_ASSERT(it_op->bo_u.u_emap.e_rc == 0);
	m0_be_op_fini(it_op);
	M0_UT_ASSERT(seg->ee_ext.e_start == e.e_end);
	M0_UT_ASSERT(seg->ee_ext.e_end   == M0_BINDEX_MAX + 1);

	test_print();

	/* Segment 0 lookup */
	rc = be_emap_lookup(emap, &prefix, 0, &it);
	M0_UT_ASSERT(rc == 0);
	M0_UT_ASSERT(seg->ee_ext.e_start ==  0);
	M0_UT_ASSERT(seg->ee_ext.e_end   == e.e_start );
	M0_UT_ASSERT(seg->ee_cksum_buf.b_nob == 0);

	/* Segment 1 - Pasted Chunk lookup */
	rc = be_emap_lookup(emap, &prefix, e.e_start, &it);
	M0_UT_ASSERT(rc == 0);
	M0_UT_ASSERT(seg->ee_ext.e_start == e.e_start);
	M0_UT_ASSERT(seg->ee_ext.e_end   == e.e_end );
	M0_UT_ASSERT(seg->ee_val         == e_val[idx]);
	M0_UT_ASSERT(seg->ee_cksum_buf.b_nob == cksum[idx].b_nob);
	M0_UT_ASSERT(memcmp(seg->ee_cksum_buf.b_addr, cksum[idx].b_addr, cksum[idx].b_nob) == 0);

	/* Segment 2 - End Chunk lookup */
	rc = be_emap_lookup(emap, &prefix, e.e_end, &it);
	M0_UT_ASSERT(rc == 0);
	M0_UT_ASSERT(seg->ee_ext.e_start == e.e_end );
	M0_UT_ASSERT(seg->ee_ext.e_end   == M0_BINDEX_MAX + 1);
	M0_UT_ASSERT(seg->ee_cksum_buf.b_nob == 0);

	/*
	 * New segment paste operation 1
	 */
	idx = 1;
	e.e_start = 100;
	e.e_end   = 150;
	es[idx] = e_temp[idx] = e;
	e_val[idx] = 11;

	m0_buf_alloc(&cksum[idx], (EXTMAP_UT_CS_SIZE *
					 	    	m0_ext_length(&e))/EXTMAP_UT_UNIT_SIZE);
	memset(cksum[idx].b_addr, 'B', cksum[idx].b_nob);
    it.ec_unit_size = EXTMAP_UT_UNIT_SIZE;

	M0_LOG(M0_INFO, "Paste [%d, %d)...", (int)e.e_start, (int)e.e_end);
	M0_SET0(it_op);
	m0_be_op_init(it_op);
	it.ec_app_cksum_buf = cksum[idx];
	m0_be_emap_paste(&it, &tx2, &e_temp[idx], e_val[idx], NULL, NULL, NULL);
	m0_be_op_wait(it_op);
	M0_UT_ASSERT(it_op->bo_u.u_emap.e_rc == 0);
	m0_be_op_fini(it_op);
	M0_UT_ASSERT(seg->ee_ext.e_start == e.e_end);
	M0_UT_ASSERT(seg->ee_ext.e_end   == M0_BINDEX_MAX + 1);

	/* Segment 0 lookup : Hole */
	rc = be_emap_lookup(emap, &prefix, 0, &it);
	M0_UT_ASSERT(rc == 0);
	M0_UT_ASSERT(seg->ee_ext.e_start ==  0);
	M0_UT_ASSERT(seg->ee_ext.e_end   == es[0].e_start );
	M0_UT_ASSERT(seg->ee_cksum_buf.b_nob == 0);

	/* Segment 1 - Pasted Chunk lookup : CS = A */
	rc = be_emap_lookup(emap, &prefix, es[0].e_start, &it);
	M0_UT_ASSERT(rc == 0);
	M0_UT_ASSERT(seg->ee_ext.e_start == es[0].e_start);
	M0_UT_ASSERT(seg->ee_ext.e_end   == es[0].e_end );
	M0_UT_ASSERT(seg->ee_val         == e_val[0]);
	M0_UT_ASSERT(seg->ee_cksum_buf.b_nob == cksum[0].b_nob);
	M0_UT_ASSERT(memcmp(seg->ee_cksum_buf.b_addr, cksum[0].b_addr, cksum[0].b_nob) == 0);

	/* Segment 2 - Pasted Chunk lookup : CS = B */
	rc = be_emap_lookup(emap, &prefix, e.e_start, &it);
	M0_UT_ASSERT(rc == 0);
	M0_UT_ASSERT(seg->ee_ext.e_start == e.e_start);
	M0_UT_ASSERT(seg->ee_ext.e_end   == e.e_end );
	M0_UT_ASSERT(seg->ee_val         == e_val[idx]);
	M0_UT_ASSERT(seg->ee_cksum_buf.b_nob == cksum[idx].b_nob);
	M0_UT_ASSERT(memcmp(seg->ee_cksum_buf.b_addr, cksum[idx].b_addr, cksum[idx].b_nob) == 0);

	/* Segment 3 - End Chunk lookup */
	rc = be_emap_lookup(emap, &prefix, e.e_end, &it);
	M0_UT_ASSERT(rc == 0);
	M0_UT_ASSERT(seg->ee_ext.e_start == e.e_end );
	M0_UT_ASSERT(seg->ee_ext.e_end   == M0_BINDEX_MAX + 1);
	M0_UT_ASSERT(seg->ee_cksum_buf.b_nob == 0);

	/*
	 * New segment overwrite paste operation
	 */
	idx = 2;
	e.e_start = 80;
	e.e_end   = 130;
	es[idx] = e_temp[idx] = e;
	e_val[idx] = 13;

	m0_buf_alloc(&cksum[idx], (EXTMAP_UT_CS_SIZE *
					 	    	m0_ext_length(&e))/EXTMAP_UT_UNIT_SIZE);
	memset(cksum[idx].b_addr, 'P', cksum[idx].b_nob);
    it.ec_unit_size = EXTMAP_UT_UNIT_SIZE;

	rc = be_emap_lookup(emap, &prefix, e.e_start, &it);
	M0_UT_ASSERT(rc == 0);

	M0_LOG(M0_INFO, "Paste [%d, %d)...", (int)e.e_start, (int)e.e_end);
	M0_SET0(it_op);
	m0_be_op_init(it_op);
	it.ec_app_cksum_buf = cksum[idx];
	m0_be_emap_paste(&it, &tx2, &e_temp[idx], e_val[idx], NULL, NULL, NULL);
	m0_be_op_wait(it_op);
	M0_UT_ASSERT(it_op->bo_u.u_emap.e_rc == 0);
	m0_be_op_fini(it_op);
	M0_UT_ASSERT(seg->ee_ext.e_start == e.e_end);
	M0_UT_ASSERT(seg->ee_ext.e_end   == es[1].e_end );

	/* Segment 0 lookup : Hole */
	rc = be_emap_lookup(emap, &prefix, 0, &it);
	M0_UT_ASSERT(rc == 0);
	M0_UT_ASSERT(seg->ee_ext.e_start ==  0);
	M0_UT_ASSERT(seg->ee_ext.e_end   == es[0].e_start );
	M0_UT_ASSERT(seg->ee_cksum_buf.b_nob == 0);

	/* Segment 1 - Pasted Chunk lookup : CS = A */
	rc = be_emap_lookup(emap, &prefix, es[0].e_start, &it);
	M0_UT_ASSERT(rc == 0);
	M0_UT_ASSERT(seg->ee_ext.e_start == es[0].e_start);
	M0_UT_ASSERT(seg->ee_ext.e_end   == es[2].e_start );
	M0_UT_ASSERT(seg->ee_val         == e_val[0]);
	M0_UT_ASSERT(seg->ee_cksum_buf.b_nob == ((cksum[0].b_nob)* (es[2].e_start - es[0].e_start))/(es[0].e_end - es[0].e_start) );
	M0_UT_ASSERT(memcmp(seg->ee_cksum_buf.b_addr, cksum[0].b_addr, seg->ee_cksum_buf.b_nob) == 0);

	/* Segment 2 - Pasted Chunk lookup : CS = P */
	rc = be_emap_lookup(emap, &prefix, e.e_start, &it);
	M0_UT_ASSERT(rc == 0);
	M0_UT_ASSERT(seg->ee_ext.e_start == e.e_start);
	M0_UT_ASSERT(seg->ee_ext.e_end   == e.e_end );
	M0_UT_ASSERT(seg->ee_val         == e_val[idx]);
	M0_UT_ASSERT(seg->ee_cksum_buf.b_nob == cksum[idx].b_nob);
	M0_UT_ASSERT(memcmp(seg->ee_cksum_buf.b_addr, cksum[idx].b_addr, cksum[idx].b_nob) == 0);

	/* Segment 3 - Pasted Chunk lookup : CS = B */
	rc = be_emap_lookup(emap, &prefix, es[2].e_end, &it);
	M0_UT_ASSERT(rc == 0);
	M0_UT_ASSERT(seg->ee_ext.e_start == es[2].e_end );
	M0_UT_ASSERT(seg->ee_ext.e_end   == es[1].e_end );
	M0_UT_ASSERT(seg->ee_val         == e_val[1]);
	M0_UT_ASSERT(seg->ee_cksum_buf.b_nob == ((cksum[1].b_nob)* (es[1].e_end - es[2].e_end))/(es[1].e_end - es[1].e_start) );
	M0_UT_ASSERT(memcmp(seg->ee_cksum_buf.b_addr, cksum[1].b_addr, seg->ee_cksum_buf.b_nob) == 0);

	/* Segment 4 - End Chunk lookup */
	rc = be_emap_lookup(emap, &prefix, es[1].e_end, &it);
	M0_UT_ASSERT(rc == 0);
	M0_UT_ASSERT(seg->ee_ext.e_start == es[1].e_end );
	M0_UT_ASSERT(seg->ee_ext.e_end   == M0_BINDEX_MAX + 1);
	M0_UT_ASSERT(seg->ee_cksum_buf.b_nob == 0);

	/* Cleanup code otherwise object delete code gives assert */
	rc = be_emap_lookup(emap, &prefix, 0, &it);
	M0_UT_ASSERT(rc == 0);

	e.e_start = 0;
	e.e_end   = M0_BINDEX_MAX + 1;

	M0_LOG(M0_INFO, "Paste [%d, %d)...", (int)e.e_start, (int)e.e_end);
	M0_SET0(it_op);
	m0_be_op_init(it_op);
	m0_buf_init(&it.ec_app_cksum_buf, NULL, 0);
	m0_be_emap_paste(&it, &tx2, &e, M0_BINDEX_MAX + 1, NULL, NULL, NULL);
	m0_be_op_wait(it_op);
	M0_UT_ASSERT(it_op->bo_u.u_emap.e_rc == 0);
	m0_be_op_fini(it_op);

	M0_UT_ASSERT(seg->ee_ext.e_start   == 0 );
	M0_UT_ASSERT(seg->ee_ext.e_end   == M0_BINDEX_MAX + 1);
	M0_UT_ASSERT(seg->ee_cksum_buf.b_nob == 0);

	m0_buf_free( &cksum[0] );
	m0_buf_free( &cksum[1] );
	m0_buf_free( &cksum[2] );
	m0_be_emap_close(&it);
}

void m0_be_ut_emap(void)
{
	test_init();
	test_obj_init(&tx2);
	test_lookup();
	test_split();
	test_print();
	test_next_prev();
	test_merge();
	test_paste();
	test_paste_checksum_validation();
	test_obj_fini(&tx2);
	test_fini();
}

#if 0 /* XXX RESTOREME */
struct m0_ut_suite m0_be_ut_emap = {
	.ts_name = "be-emap-ut",
	.ts_tests = {
		{ "emap-init", test_init },
		{ "obj-init", test_obj_init },
		{ "lookup", test_lookup },
		{ "split", test_split },
		{ "print", test_print },
		{ "merge", test_merge },
		{ "obj-fini", test_obj_fini },
		{ "emap-fini", test_fini },
		{ NULL, NULL }
	}
};
#endif

/*
 * UB
 */
//
//enum {
//	UB_ITER = 100000,
//	UB_ITER_TX = 10000
//};
//
//static int ub_init(const char *opts M0_UNUSED)
//{
//	test_init();
//	return 0;
//}
//
//static void ub_fini(void)
//{
//	test_fini();
//}
//
//static struct m0_uint128 p;
//
//static void ub_obj_init(int i)
//{
//	p = prefix;
//
//	p.u_hi += i;
//	p.u_lo -= i*i;
//
//	m0_be_emap_obj_insert(emap, &tx, &op, &p, 42);
//	M0_ASSERT(m0_be_op_is_done(&op));
//	checkpoint();
//}
//
//static void ub_obj_fini(int i)
//{
//	p = prefix;
//
//	p.u_hi += i;
//	p.u_lo -= i*i;
//
//	m0_be_emap_obj_delete(emap, &tx, &op, &p);
//	M0_ASSERT(m0_be_op_is_done(&op));
//	checkpoint();
//}
//
//static void ub_obj_init_same(int i)
//{
//	p = prefix;
//
//	p.u_hi += i;
//	p.u_lo -= i*i;
//
//	m0_be_emap_obj_insert(emap, &tx, &op, &p, 42);
//	M0_ASSERT(m0_be_op_is_done(&op));
//}
//
//static void ub_obj_fini_same(int i)
//{
//	p = prefix;
//
//	p.u_hi += i;
//	p.u_lo -= i*i;
//
//	m0_be_emap_obj_delete(emap, &tx, &op, &p);
//	M0_ASSERT(m0_be_op_is_done(&op));
//}
//
//static void ub_split(int i)
//{
//	split(5000, 1, false);
//}
//
//struct m0_ub_set m0_be_emap_ub = {
//	.us_name = "emap-ub",
//	.us_init = ub_init,
//	.us_fini = ub_fini,
//	.us_run  = {
//		{ .ub_name = "obj-init",
//		  .ub_iter = UB_ITER,
//		  .ub_round = ub_obj_init },
//
//		{ .ub_name = "obj-fini",
//		  .ub_iter = UB_ITER,
//		  .ub_round = ub_obj_fini },
//
//		{ .ub_name = "obj-init-same-tx",
//		  .ub_iter = UB_ITER_TX,
//		  .ub_round = ub_obj_init_same },
//
//		{ .ub_name = "obj-fini-same-tx",
//		  .ub_iter = UB_ITER_TX,
//		  .ub_round = ub_obj_fini_same },
//
//		{ .ub_name = "split",
//		  .ub_iter = UB_ITER/5,
//		  .ub_init = test_obj_init,
//		  .ub_round = ub_split },
//
//		{ .ub_name = NULL }
//	}
//};

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

