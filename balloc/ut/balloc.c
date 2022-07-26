/* -*- C -*- */
/*
 * Copyright (c) 2012-2020 Seagate Technology LLC and/or its Affiliates
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


#include <stdlib.h>       /* srand, rand */
#include <errno.h>
#include <sys/time.h>
#include <err.h>

#define M0_TRACE_SUBSYSTEM M0_TRACE_SUBSYS_BALLOC
#include "lib/trace.h"
#include "lib/arith.h"    /* M0_3WAY, m0_uint128 */
#include "lib/misc.h"     /* M0_SET0 */
#include "lib/assert.h"
#include "lib/memory.h"
#include "lib/thread.h"
#include "lib/getopts.h"
#include "dtm/dtm.h"      /* m0_dtx */
#include "motr/magic.h"
#include "ut/ut.h"
#include "ut/be.h"
#include "balloc/balloc.h"
#include "be/ut/helper.h"
#include "stob/ad.h"      /* m0_stob_ad_spares_calc */

#define BALLOC_DBNAME "./__balloc_db"

#define GROUP_SIZE (BALLOC_DEF_CONTAINER_SIZE / (BALLOC_DEF_BLOCKS_PER_GROUP * \
						 (1 << BALLOC_DEF_BLOCK_SHIFT)))

#define BALLOC_DEBUG

static const int    MAX     = 10;
static m0_bcount_t  prev_free_blocks;
m0_bcount_t	   *prev_group_info_free_blocks;

enum balloc_invariant_enum {
	INVAR_ALLOC,
	INVAR_FREE,
};

bool balloc_ut_invariant(struct m0_balloc *motr_balloc,
			 struct m0_ext alloc_ext,
			 int balloc_invariant_flag)
{
	m0_bcount_t len = m0_ext_length(&alloc_ext);
	m0_bcount_t group;

	group = alloc_ext.e_start >> motr_balloc->cb_sb.bsb_gsbits;

	if (motr_balloc->cb_sb.bsb_magic != M0_BALLOC_SB_MAGIC)
		return false;

	switch (balloc_invariant_flag) {
	    case INVAR_ALLOC:
		 prev_free_blocks		    -= len;
		 prev_group_info_free_blocks[group] -= len;
		 break;
	    case INVAR_FREE:
		 prev_free_blocks		    += len;
		 prev_group_info_free_blocks[group] += len;
		 break;
	    default:
		 return false;
	}

	return motr_balloc->cb_group_info[group].bgi_normal.bzp_freeblocks ==
		prev_group_info_free_blocks[group] &&
		motr_balloc->cb_sb.bsb_freeblocks ==
		prev_free_blocks;
}

/**
 * Verifies balloc operations.
 *
 * @param is_reserve true to reserve an extent.
 *		     false to allocate an extent.
 * @return 0 on success.
 *         -errno on failure.
 */
int test_balloc_ut_ops(struct m0_be_ut_backend *ut_be, struct m0_be_seg *seg,
		       bool is_reserve)
{
	struct m0_sm_group     *grp;
	struct m0_balloc       *motr_balloc;
	struct m0_dtx           dtx = {};
	struct m0_be_tx        *tx  = &dtx.tx_betx;
	struct m0_be_tx_credit  cred;
	struct m0_ext           ext[MAX];
	struct m0_ext           tmp   = {};
	m0_bcount_t             count = 539;
	m0_bcount_t             spare_size;
	int                     i     = 0;
	int                     rc;
	time_t                  now;

	time(&now);
	srand(now);

	grp = m0_be_ut_backend_sm_group_lookup(ut_be);
	rc = m0_balloc_create(0, seg, grp, &motr_balloc, &M0_FID_INIT(0, 1));
	M0_UT_ASSERT(rc == 0);

	rc = motr_balloc->cb_ballroom.ab_ops->bo_init
		(&motr_balloc->cb_ballroom, seg, BALLOC_DEF_BLOCK_SHIFT,
		 BALLOC_DEF_CONTAINER_SIZE, BALLOC_DEF_BLOCKS_PER_GROUP,
		 m0_stob_ad_spares_calc(BALLOC_DEF_BLOCKS_PER_GROUP));

	if (rc != 0)
		goto out;

	prev_free_blocks = motr_balloc->cb_sb.bsb_freeblocks;
	M0_ALLOC_ARR(prev_group_info_free_blocks, GROUP_SIZE);

	for (i = 0; i < GROUP_SIZE; ++i) {
		prev_group_info_free_blocks[i] =
			motr_balloc->cb_group_info[i].bgi_normal.bzp_freeblocks;
	}

	for (i = 0; i < MAX; ++i) {
		count = rand() % 1500 + 1;

		cred = M0_BE_TX_CREDIT(0, 0);
		motr_balloc->cb_ballroom.ab_ops->bo_alloc_credit(
			&motr_balloc->cb_ballroom, 1, &cred);
		m0_ut_be_tx_begin(tx, ut_be, &cred);

		if (is_reserve) {
			tmp.e_start = tmp.e_end;
			tmp.e_end   = tmp.e_start + count;
			rc = motr_balloc->cb_ballroom.ab_ops->bo_reserve_extent(
					&motr_balloc->cb_ballroom, tx, &tmp,
					M0_BALLOC_NORMAL_ZONE);
		} else {
			rc = motr_balloc->cb_ballroom.ab_ops->bo_alloc(
					&motr_balloc->cb_ballroom, &dtx,
				        count, &tmp, M0_BALLOC_NORMAL_ZONE);
		}

		M0_UT_ASSERT(rc == 0);
		if (rc < 0) {
			M0_LOG(M0_ERROR, "Error in allocation");
			return rc;
		}

		ext[i] = tmp;

		/* The result extent length should be less than	or equal to the
		 * requested length. */
		M0_UT_ASSERT(m0_ext_length(&ext[i]) <= count);
		M0_UT_ASSERT(balloc_ut_invariant(motr_balloc, ext[i],
						 INVAR_ALLOC));
		M0_LOG(M0_INFO, "%3d:rc=%d: req=%5d, got=%5d: "
		       "[%08llx,%08llx)=[%8llu,%8llu)",
		       i, rc, (int)count,
		       (int)m0_ext_length(&ext[i]),
		       (unsigned long long)ext[i].e_start,
		       (unsigned long long)ext[i].e_end,
		       (unsigned long long)ext[i].e_start,
		       (unsigned long long)ext[i].e_end);
		m0_ut_be_tx_end(tx);
	}

	spare_size = m0_stob_ad_spares_calc(motr_balloc->cb_sb.bsb_groupsize);

	for (i = 0; i < motr_balloc->cb_sb.bsb_groupcount && rc == 0; ++i) {
		struct m0_balloc_group_info *grp = m0_balloc_gn2info(
							motr_balloc, i);
		if (grp) {
			m0_balloc_lock_group(grp);
			rc = m0_balloc_load_extents(motr_balloc, grp);
			if (rc == 0)
				m0_balloc_debug_dump_group_extent(
					"balloc ut", grp);
			m0_balloc_release_extents(grp);
			m0_balloc_unlock_group(grp);
		}
	}

	/* randomize the array */
	for (i = 0; i < MAX; ++i) {
		int a;
		int b;
		a = rand() % MAX;
		b = rand() % MAX;
		M0_SWAP(ext[a], ext[b]);
	}

	for (i = 0; i < MAX && rc == 0; ++i) {
		cred = M0_BE_TX_CREDIT(0, 0);
		motr_balloc->cb_ballroom.ab_ops->bo_free_credit(
			&motr_balloc->cb_ballroom, 1, &cred);
		m0_ut_be_tx_begin(tx, ut_be, &cred);

		rc = motr_balloc->cb_ballroom.ab_ops->bo_free(
				&motr_balloc->cb_ballroom, &dtx, &ext[i]);

		M0_UT_ASSERT(rc == 0);
		if (rc < 0) {
			M0_LOG(M0_ERROR, "Error during free for size %5d",
				(int)m0_ext_length(&ext[i]));
			return rc;
		}

		M0_UT_ASSERT(balloc_ut_invariant(motr_balloc, ext[i],
						 INVAR_FREE));
		M0_LOG(M0_INFO, "%3d:rc=%d: freed=         %5d: "
			"[%08llx,%08llx)=[%8llu,%8llu)",
			i, rc, (int)m0_ext_length(&ext[i]),
			(unsigned long long)ext[i].e_start,
			(unsigned long long)ext[i].e_end,
			(unsigned long long)ext[i].e_start,
			(unsigned long long)ext[i].e_end);
		m0_ut_be_tx_end(tx);
	}

	M0_UT_ASSERT(motr_balloc->cb_sb.bsb_freeblocks == prev_free_blocks);
	if (motr_balloc->cb_sb.bsb_freeblocks != prev_free_blocks) {
		M0_LOG(M0_ERROR, "Size mismatch during block reclaim");
		rc = -EINVAL;
	}

	for (i = 0; i < motr_balloc->cb_sb.bsb_groupcount && rc == 0; ++i) {
		struct m0_balloc_group_info *grp = m0_balloc_gn2info(
							motr_balloc, i);

		if (grp) {
			m0_balloc_lock_group(grp);
			rc = m0_balloc_load_extents(motr_balloc, grp);
			if (rc == 0)
				m0_balloc_debug_dump_group_extent(
					"balloc ut", grp);
			M0_UT_ASSERT(grp->bgi_normal.bzp_freeblocks ==
				     motr_balloc->cb_sb.bsb_groupsize -
				     spare_size);
			m0_balloc_release_extents(grp);
			m0_balloc_unlock_group(grp);
		}
	}

	motr_balloc->cb_ballroom.ab_ops->bo_fini(&motr_balloc->cb_ballroom);

out:
	m0_free(prev_group_info_free_blocks);

	M0_LOG(M0_INFO, "done. status = %d", rc);
	return rc;
}

void test_balloc()
{
	struct m0_be_ut_backend	 ut_be;
	struct m0_be_ut_seg	 ut_seg;
	int			 rc;

	M0_SET0(&ut_be);
	/* Init BE */
	m0_be_ut_backend_init(&ut_be);
	m0_be_ut_seg_init(&ut_seg, &ut_be, 1ULL << 24);
	rc = test_balloc_ut_ops(&ut_be, ut_seg.bus_seg, false);
	M0_UT_ASSERT(rc == 0);

	m0_be_ut_seg_fini(&ut_seg);
	m0_be_ut_backend_fini(&ut_be);
}

void test_reserve_extent()
{
	struct m0_be_ut_backend	 ut_be;
	struct m0_be_ut_seg	 ut_seg;
	int			 rc;

	M0_SET0(&ut_be);
	/* Init BE */
	m0_be_ut_backend_init(&ut_be);
	m0_be_ut_seg_init(&ut_seg, &ut_be, 1ULL << 24);
	rc = test_balloc_ut_ops(&ut_be, ut_seg.bus_seg, true);
	M0_UT_ASSERT(rc == 0);

	m0_be_ut_seg_fini(&ut_seg);
	m0_be_ut_backend_fini(&ut_be);
}

static int test_balloc_ut_suite_init(void)
{
	m0_btree_glob_init();
	return 0;
}

static int test_balloc_ut_suite_fini(void)
{
	m0_btree_glob_fini();
	return 0;
}

struct m0_ut_suite balloc_ut = {
        .ts_name  = "balloc-ut",
	.ts_init = test_balloc_ut_suite_init,
	.ts_fini = test_balloc_ut_suite_fini,
        .ts_tests = {
		{ "balloc", test_balloc},
		{ "reserve blocks for extmap", test_reserve_extent},
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
