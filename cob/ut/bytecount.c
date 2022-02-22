/* -*- C -*- */
/*
 * Copyright (c) 2022 Seagate Technology LLC and/or its Affiliates
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


#include "ut/ut.h"
#include "lib/ub.h"
#include "lib/misc.h"     /* M0_SET0 */
#include "lib/memory.h"
#include "be/ut/helper.h"
#include "be/seg.h"
#include "cob/cob.h"
#include "lib/locality.h"

enum {
	KEY_VAL_NR = 10,
};

static struct m0_cob_domain_id  id = { 42 };
static struct m0_be_ut_backend  ut_be;
static struct m0_sm_group      *grp;
static struct m0_cob_domain    *dom;
static struct m0_cob           *cob;
struct m0_cob_bckey             bckey[KEY_VAL_NR] = {};
struct m0_cob_bcrec             bcrec[KEY_VAL_NR] = {};


static int ut_init(void)
{
	return 0;
}

static int ut_fini(void)
{
	return 0;
}

static void ut_tx_open(struct m0_be_tx *tx, struct m0_be_tx_credit *credit)
{
	int rc;

	m0_be_ut_tx_init(tx, &ut_be);
	m0_be_tx_prep(tx, credit);
	rc = m0_be_tx_open_sync(tx);
	M0_UT_ASSERT(rc == 0);
	M0_UT_ASSERT(m0_be_tx_state(tx) == M0_BTS_ACTIVE);
}

static void test_cob_dom_create(void)
{
	struct m0_be_seg *seg0;
	int               rc;

	M0_SET0(&ut_be);
	m0_be_ut_backend_init_cfg(&ut_be, NULL, true);
	seg0 = m0_be_domain_seg0_get(&ut_be.but_dom);

	grp = m0_be_ut_backend_sm_group_lookup(&ut_be);
	rc = m0_cob_domain_create(&dom, grp, &id, &ut_be.but_dom, seg0); /*XXX*/
	M0_UT_ASSERT(rc == 0);

	m0_cob_domain_fini(dom);
	m0_be_ut_backend_fini(&ut_be);
}

static void test_init(void)
{
	struct m0_be_seg *seg0;
	int rc;

	M0_SET0(&ut_be);
	m0_be_ut_backend_init_cfg(&ut_be, NULL, false);

	seg0 = m0_be_domain_seg0_get(&ut_be.but_dom);

	rc = m0_cob_domain_init(dom, seg0);
	M0_UT_ASSERT(rc == 0);
}

static void test_fini(void)
{
	int rc;

	m0_free(cob); /* Free m0_cob struct allocated in test_init() */
	grp = m0_be_ut_backend_sm_group_lookup(&ut_be);
	rc = m0_cob_domain_destroy(dom, grp, &ut_be.but_dom);
	M0_UT_ASSERT(rc == 0);

	m0_be_ut_backend_fini(&ut_be);
}

void test_insert(void)
{
	struct m0_be_tx         tx_;
	struct m0_be_tx        *tx = &tx_;
	int                     rc;
	struct m0_be_tx_credit  accum = {};
	int                     i;

	rc = m0_cob_alloc(dom, &cob);
	M0_UT_ASSERT(rc == 0);
	
	/* Add credits for insertion operation */
	for (i = 0; i < KEY_VAL_NR; i++) {
		m0_cob_tx_credit(dom, M0_COB_OP_BYTECOUNT_SET, &accum);
	}

	/* Populate keys and records */
	for (i = 0; i < KEY_VAL_NR; i++) {
		bckey[i].cbk_pfid = M0_FID_TINIT('k', 1, i);
		bckey[i].cbk_user_id = i;
		bcrec[i].cbr_bytecount = 100 + (i * 10);
		bcrec[i].cbr_cob_objects = 10 + i;
	}

	ut_tx_open(tx, &accum);

	/* Insert keys and records in bytecount btree */
	for (i = 0; i < KEY_VAL_NR; i++) {
		rc = m0_cob_bc_insert(cob, &bckey[i], &bcrec[i], tx);
		M0_UT_ASSERT(rc == 0);
	}

	m0_be_tx_close_sync(tx);
	m0_be_tx_fini(tx);
}

void test_entries_dump(void)
{
	int                 i;
	int                 rc;
	uint32_t            count;
	struct m0_cob_bckey dump_keys[KEY_VAL_NR];
	struct m0_cob_bcrec dump_recs[KEY_VAL_NR];
	struct m0_buf      *keys = NULL;
	struct m0_buf      *recs = NULL;
	struct m0_fid       temp_fid;
	void               *kcurr;
	void               *rcurr;

	rc = m0_cob_bc_entries_dump(cob->co_dom, keys, recs, &count);
	M0_UT_ASSERT(rc == 0);
	M0_UT_ASSERT(count == KEY_VAL_NR);

	kcurr = keys->b_addr;
	rcurr = recs->b_addr;

	for (i =0; i < count; i++) {
		memcpy(&dump_keys[i], kcurr, sizeof(struct m0_cob_bckey));
		memcpy(&dump_recs[i], rcurr, sizeof(struct m0_cob_bcrec));

		kcurr += sizeof(struct m0_cob_bckey);
		rcurr += sizeof(struct m0_cob_bcrec);

		temp_fid = M0_FID_TINIT('k', 1, i);
		M0_UT_ASSERT(m0_fid_eq(&dump_keys[i].cbk_pfid, &temp_fid));
		M0_UT_ASSERT(dump_keys[i].cbk_user_id == i);
		M0_UT_ASSERT(dump_recs[i].cbr_bytecount == 100 + (i * 10));
		M0_UT_ASSERT(dump_recs[i].cbr_cob_objects == 10 + i);
	}
}

void test_iterator(void)
{
	struct m0_cob_bc_iterator it;
	int                       rc;
	/* Make sure fid is valid. */
	struct m0_fid             fid = M0_FID_TINIT('k', 1, 0);

	rc = m0_cob_bc_iterator_init(cob, &it, &fid, 0);
	M0_UT_ASSERT(rc == 0);
	rc = m0_cob_bc_iterator_get(&it);
	M0_UT_ASSERT(rc == 0);
	rc = m0_cob_bc_iterator_next(&it);
	M0_UT_ASSERT(rc == 0);
	m0_cob_bc_iterator_fini(&it);
}

void test_lookup(void)
{
	int                     rc;
	int                     i;
	struct m0_cob_bckey     dummy_key = {};
	struct m0_cob_bcrec     out_rec = {};

	/**
	 * Lookup for the keys created in test_insert() in bytecount btree and
	 * verify the record values.
	 */
	for (i = 0; i < KEY_VAL_NR; i++) {
		rc = m0_cob_bc_lookup(cob, &bckey[i], &out_rec);
		M0_UT_ASSERT(rc == 0);
		M0_UT_ASSERT(bcrec[i].cbr_bytecount == out_rec.cbr_bytecount);
		M0_UT_ASSERT(bcrec[i].cbr_cob_objects ==
			     out_rec.cbr_cob_objects);
	}

	/**
	 * Lookup for wrong key which is not present in byteocount tree,
	 * this case lookup should fail.
	 */
	dummy_key.cbk_pfid = M0_FID_TINIT('k', 0, 1);
	dummy_key.cbk_user_id = 0;
	rc = m0_cob_bc_lookup(cob, &dummy_key, &out_rec);
	M0_UT_ASSERT(rc != 0);
}

void test_update(void)
{
	struct m0_be_tx         tx_;
	struct m0_be_tx        *tx = &tx_;
	int                     rc;
	struct m0_be_tx_credit  accum = {};
	int                     i;
	struct m0_cob_bcrec     out_rec = {};
	
	/* Add credits for update operation */
	for (i = 0; i < KEY_VAL_NR; i++) {
		m0_cob_tx_credit(dom, M0_COB_OP_BYTECOUNT_UPDATE, &accum);
	}

	/* Populate new record values */
	for (i = 0; i < KEY_VAL_NR; i++) {
		bcrec[i].cbr_bytecount = 200 + i;
		bcrec[i].cbr_cob_objects = 20 + i;
	}

	ut_tx_open(tx, &accum);

	/* Update records of keys in bytecount btree created in test_insert() */
	for (i = 0; i < KEY_VAL_NR; i++) {
		rc = m0_cob_bc_update(cob, &bckey[i], &bcrec[i], tx);
		M0_UT_ASSERT(rc == 0);
	}

	/**
	 * Lookup for the keys in bytecount btree after updating the records.
	 * Verify the record values after update operation.
	 */
	for (i = 0; i < KEY_VAL_NR; i++) {
		rc = m0_cob_bc_lookup(cob, &bckey[i], &out_rec);
		M0_UT_ASSERT(rc == 0);
		M0_UT_ASSERT(bcrec[i].cbr_bytecount == out_rec.cbr_bytecount);
		M0_UT_ASSERT(bcrec[i].cbr_cob_objects ==
			     out_rec.cbr_cob_objects);
	}

	m0_be_tx_close_sync(tx);
	m0_be_tx_fini(tx);
}

struct m0_ut_suite bytecount_ut = {
	.ts_name = "bytecount-ut",
	.ts_init = ut_init,
	.ts_fini = ut_fini,
	.ts_tests = {
		{ "cob-dom-create",   test_cob_dom_create },
		{ "cob-dom-init",     test_init },
		{ "bc-tree-insert",   test_insert },
		{ "bc-tree-iterator", test_iterator },
		{ "bc-tree-lookup",   test_lookup },
		{ "bc-tree-update",   test_update },
		{ "cob-dom-fini",     test_fini },
		{ NULL, NULL }
	}
};


/*
 *  Local variables:
 *  c-indentation-style: "K&R"
 *  c-basic-offset: 8
 *  tab-width: 8
 *  fill-column: 80
 *  scroll-step: 1
 *  End:
 */
