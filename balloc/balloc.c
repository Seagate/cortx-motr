/* -*- C -*- */
/*
 * Copyright (c) 2012-2021 Seagate Technology LLC and/or its Affiliates
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


#define M0_TRACE_SUBSYSTEM M0_TRACE_SUBSYS_BALLOC
#include "lib/trace.h"

#include <stdio.h>         /* sprintf */
#include <stdlib.h>
#include <memory.h>
#include <sys/stat.h>
#include <sys/time.h>

#include "dtm/dtm.h"	   /* m0_dtx */
#include "be/tx_bulk.h"    /* m0_be_tx_bulk */
#include "be/op.h"         /* m0_be_op_active */
#include "lib/misc.h"	   /* M0_SET0 */
#include "lib/errno.h"
#include "lib/arith.h"	   /* min_check, m0_is_po2 */
#include "lib/memory.h"
#include "lib/byteorder.h" /* m0_byteorder_cpu_to_be64() */
#include "lib/locality.h"  /* m0_locality0_get */
#include "balloc.h"
#include "motr/magic.h"

/**
   M0 Data Block Allocator.
   BALLOC is a multi-block allocator, with pre-allocation. All metadata about
   block allocation is stored in BE segment.

 */

enum m0_balloc_allocation_status {
	M0_BALLOC_AC_FOUND    = 1,
	M0_BALLOC_AC_CONTINUE = 2,
	M0_BALLOC_AC_BREAK    = 3,
};

struct balloc_allocation_context {
	uint32_t                       bac_criteria;
	uint32_t                       bac_order2;  /* order of 2 */
	uint32_t                       bac_scanned; /* groups scanned */
	uint32_t                       bac_found;   /* count of found */
	uint32_t                       bac_status;  /* allocation status */
	uint64_t                       bac_flags;
	struct m0_balloc              *bac_ctxt;
	struct m0_be_tx               *bac_tx;
	struct m0_balloc_allocate_req *bac_req;
	struct m0_ext                  bac_orig; /*< original */
	struct m0_ext                  bac_goal; /*< after normalization */
	struct m0_ext                  bac_best; /*< best available */
	struct m0_ext                  bac_final;/*< final results */
};

static int btree_lookup_callback(struct m0_btree_cb  *cb,
				 struct m0_btree_rec *rec)
{
	struct m0_btree_rec     *datum = cb->c_datum;

	/** Copy both keys and values. Keys are copied to cover slant case. */
	m0_bufvec_copy(&datum->r_key.k_data, &rec->r_key.k_data,
		       m0_vec_count(&rec->r_key.k_data.ov_vec));
	m0_bufvec_copy(&datum->r_val, &rec->r_val,
		       m0_vec_count(&rec->r_val.ov_vec));
	return 0;
}

static inline int btree_lookup_sync(struct m0_btree     *tree,
				    const struct m0_buf *key,
				    struct m0_buf       *val,
				    bool                 slant)
{
	struct m0_btree_op   kv_op  = {};
	void                *k_ptr  = key->b_addr;
	void                *v_ptr  = val->b_addr;
	m0_bcount_t          ksize  = key->b_nob;
	m0_bcount_t          vsize  = val->b_nob;
	uint64_t             flags  = slant ? BOF_SLANT : BOF_EQUAL;

	struct m0_btree_rec  rec    = {
			    .r_key.k_data = M0_BUFVEC_INIT_BUF(&k_ptr, &ksize),
			    .r_val        = M0_BUFVEC_INIT_BUF(&v_ptr, &vsize),
			};
	struct m0_btree_cb   get_cb = {.c_act = btree_lookup_callback,
				       .c_datum = &rec,
				      };

	return M0_BTREE_OP_SYNC_WITH_RC(&kv_op,
					m0_btree_get(tree, &rec.r_key, &get_cb,
						     flags, &kv_op));
}

static int btree_insert_callback(struct m0_btree_cb  *cb,
				 struct m0_btree_rec *rec)
{
	struct m0_btree_rec     *datum = cb->c_datum;

	/** Write the Key and Value to the location indicated in rec. */
	m0_bufvec_copy(&rec->r_key.k_data,  &datum->r_key.k_data,
		       m0_vec_count(&datum->r_key.k_data.ov_vec));
	m0_bufvec_copy(&rec->r_val, &datum->r_val,
		       m0_vec_count(&rec->r_val.ov_vec));
	return 0;
}

static inline int btree_insert_sync(struct m0_btree     *tree,
				    struct m0_be_tx     *tx,
				    const struct m0_buf *key,
				    const struct m0_buf *val)
{
	struct m0_btree_op   kv_op = {};
	void                *k_ptr = key->b_addr;
	void                *v_ptr = val->b_addr;
	m0_bcount_t          ksize = key->b_nob;
	m0_bcount_t          vsize = val->b_nob;
	struct m0_btree_rec  rec = {
			    .r_key.k_data = M0_BUFVEC_INIT_BUF(&k_ptr, &ksize),
			    .r_val        = M0_BUFVEC_INIT_BUF(&v_ptr, &vsize),
			};
	struct m0_btree_cb   put_cb = {.c_act = btree_insert_callback,
				       .c_datum = &rec,
				      };

	return M0_BTREE_OP_SYNC_WITH_RC(&kv_op,
					m0_btree_put(tree, &rec, &put_cb,
						     &kv_op, tx));
}

static inline int balloc_ext_insert(struct m0_btree *tree, struct m0_be_tx *tx,
				    struct m0_ext ext)
{
	struct m0_buf key;
	struct m0_buf val;

	key = (struct m0_buf)M0_BUF_INIT_PTR(&ext.e_end);
	val = (struct m0_buf)M0_BUF_INIT_PTR(&ext.e_start);

	return  btree_insert_sync(tree, tx, &key, &val);
}

static int btree_update_callback(struct m0_btree_cb  *cb,
				 struct m0_btree_rec *rec)
{
	struct m0_btree_rec     *datum = cb->c_datum;

	/** Only update the Value to the location indicated in rec. */
	m0_bufvec_copy(&rec->r_val, &datum->r_val,
		       m0_vec_count(&datum->r_val.ov_vec));
	return 0;
}

static inline int btree_update_sync(struct m0_btree     *tree,
				    struct m0_be_tx     *tx,
				    const struct m0_buf *key,
				    const struct m0_buf *val)
{
	struct m0_btree_op   kv_op = {};
	void                *k_ptr = key->b_addr;
	void                *v_ptr = val->b_addr;
	m0_bcount_t          ksize = key->b_nob;
	m0_bcount_t          vsize = val->b_nob;
	struct m0_btree_rec  rec = {
		.r_key.k_data = M0_BUFVEC_INIT_BUF(&k_ptr, &ksize),
		.r_val        = M0_BUFVEC_INIT_BUF( &v_ptr, &vsize),
	};
	struct m0_btree_cb   update_cb = {.c_act = btree_update_callback,
					  .c_datum = &rec,
					 };

	return M0_BTREE_OP_SYNC_WITH_RC(&kv_op,
					m0_btree_update(tree, &rec, &update_cb,
							&kv_op, tx));
}

static inline int balloc_ext_update(struct m0_btree *tree, struct m0_be_tx *tx,
				    struct m0_ext ext)
{
	struct m0_buf key;
	struct m0_buf val;

	key = (struct m0_buf)M0_BUF_INIT_PTR(&ext.e_end);
	val = (struct m0_buf)M0_BUF_INIT_PTR(&ext.e_start);

	return  btree_update_sync(tree, tx, &key, &val);
}

static inline int btree_delete_sync(struct m0_btree     *tree,
				    struct m0_be_tx     *tx,
				    const struct m0_buf *key)
{
	struct m0_btree_op   kv_op = {};
	void                *k_ptr = key->b_addr;
	m0_bcount_t          ksize = key->b_nob;
	struct m0_btree_key  r_key = {
		.k_data  = M0_BUFVEC_INIT_BUF(&k_ptr, &ksize),
	};

	return M0_BTREE_OP_SYNC_WITH_RC(&kv_op,
					m0_btree_del(tree, &r_key, NULL,
						     &kv_op, tx));
}

/* Conducts the basic sanity check on freeblocks and fragments. */
static int is_group_good_enough(struct balloc_allocation_context *bac,
				m0_bcount_t maxchunk, m0_bcount_t free,
				m0_bcount_t fragments);
static m0_bindex_t zone_start_get(struct m0_balloc_group_info *grp,
				  enum m0_balloc_allocation_flag alloc_flag);
static int allocate_blocks(int cr, struct balloc_allocation_context *bac,
			   struct m0_balloc_group_info *grp, m0_bcount_t len,
			   enum m0_balloc_allocation_flag alloc_type);
static m0_bcount_t group_spare_freeblocks_get(struct m0_balloc_group_info *grp)
{
	return grp->bgi_spare.bzp_freeblocks;
}
static m0_bcount_t group_freeblocks_get(struct m0_balloc_group_info *grp)
{
	return grp->bgi_normal.bzp_freeblocks;
}
static m0_bcount_t group_maxchunk_get(struct m0_balloc_group_info *grp)
{
	return grp->bgi_normal.bzp_maxchunk;
}
static m0_bcount_t group_spare_maxchunk_get(struct m0_balloc_group_info *grp)
{
	return grp->bgi_spare.bzp_maxchunk;
}
static m0_bcount_t group_fragments_get(struct m0_balloc_group_info *grp)
{
	return grp->bgi_normal.bzp_fragments;
}
static m0_bcount_t group_spare_fragments_get(struct m0_balloc_group_info *grp)
{
	return grp->bgi_spare.bzp_fragments;
}
static struct m0_list *group_normal_ext(struct m0_balloc_group_info *grp)
{
	return &grp->bgi_normal.bzp_extents;
}
static struct m0_list *group_spare_ext(struct m0_balloc_group_info *grp)
{
	return &grp->bgi_spare.bzp_extents;
}
static void balloc_zone_init(struct m0_balloc_zone_param *zone, uint64_t type,
			     m0_bcount_t start, m0_bcount_t size,
			     m0_bcount_t freeblocks, m0_bcount_t fragments,
			     m0_bcount_t maxchunk);
static uint64_t ext_range_locate(struct m0_ext *ip_ext,
				 struct m0_balloc_group_info *grp);
static bool is_spare(uint64_t alloc_flags);
static bool is_normal(uint64_t alloc_flags);
static bool is_any(uint64_t alloc_flag);


static void balloc_debug_dump_extent(const char *tag, struct m0_ext *ex)
{
if (ENABLE_BALLOC_DUMP) {

	if (ex == NULL)
		return;

	M0_LOG(M0_DEBUG, "ex@%p:%s "EXT_F, ex, (char*) tag, EXT_P(ex));
}
}

M0_INTERNAL void m0_balloc_debug_dump_group(const char *tag,
					    struct m0_balloc_group_info *grp)
{
if (ENABLE_BALLOC_DUMP) {
	if (grp == NULL)
		return;

	M0_LOG(M0_DEBUG, "group_desc@%p:%s groupno=%08llx "
	       "normal: free=%08llx maxchunk=0x%08llx frags=0x%08llx"
	       " spare: free=%08llx maxchunk=0x%08llx frags=0x%08llx",
		grp, (char*) tag,
		(unsigned long long) grp->bgi_groupno,
		(unsigned long long) grp->bgi_normal.bzp_freeblocks,
		(unsigned long long) grp->bgi_normal.bzp_maxchunk,
		(unsigned long long) grp->bgi_normal.bzp_fragments,
		(unsigned long long) grp->bgi_spare.bzp_freeblocks,
		(unsigned long long) grp->bgi_spare.bzp_maxchunk,
		(unsigned long long) grp->bgi_spare.bzp_fragments);
}
}

M0_INTERNAL void m0_balloc_debug_dump_group_extent(const char *tag,
					struct m0_balloc_group_info *grp)
{
if (ENABLE_BALLOC_DUMP) {
	struct m0_lext	*ex;

	if (grp == NULL || grp->bgi_extents == NULL)
		return;

	M0_LOG(M0_DEBUG, "free extents@%p:%s for grp=%04llx:",
		grp, (char*) tag, (unsigned long long) grp->bgi_groupno);
	m0_list_for_each_entry(&grp->bgi_normal.bzp_extents, ex,
			       struct m0_lext, le_link)
		M0_LOG(M0_DEBUG, "normal: "EXT_F, EXT_P(&ex->le_ext));
	m0_list_for_each_entry(&grp->bgi_spare.bzp_extents, ex,
			       struct m0_lext, le_link)
		M0_LOG(M0_DEBUG, "spare: "EXT_F, EXT_P(&ex->le_ext));
}
}

M0_INTERNAL void m0_balloc_debug_dump_sb(const char *tag,
					 struct m0_balloc_super_block *sb)
{
if (ENABLE_BALLOC_DUMP) {
	if (sb == NULL)
		return;

	M0_LOG(M0_DEBUG, "dumping sb@%p:%s\n"
		"|-----magic=%llx state=%llu version=%llu\n"
		"|-----total=%llu free=%llu bs=%llu(bits=%lu)",
		sb, (char*) tag,
		(unsigned long long) sb->bsb_magic,
		(unsigned long long) sb->bsb_state,
		(unsigned long long) sb->bsb_version,
		(unsigned long long) sb->bsb_totalsize,
		(unsigned long long) sb->bsb_freeblocks,
		(unsigned long long) sb->bsb_blocksize,
		(unsigned long	   ) sb->bsb_bsbits);

	M0_LOG(M0_DEBUG, "|-----gs=%llu(bits=%lu) gc=%llu "
		" prealloc=%llu\n"
		"|-----time format=%llu\n"
		"|-----write=%llu\n"
		"|-----mnt  =%llu\n"
		"|-----last =%llu",
		(unsigned long long) sb->bsb_groupsize,
		(unsigned long	   ) sb->bsb_gsbits,
		(unsigned long long) sb->bsb_groupcount,
		(unsigned long long) sb->bsb_prealloc_count,
		(unsigned long long) sb->bsb_format_time,
		(unsigned long long) sb->bsb_write_time,
		(unsigned long long) sb->bsb_mnt_time,
		(unsigned long long) sb->bsb_last_check_time);

	M0_LOG(M0_DEBUG, "|-----mount=%llu max_mnt=%llu stripe_size=%llu",
		(unsigned long long) sb->bsb_mnt_count,
		(unsigned long long) sb->bsb_max_mnt_count,
		(unsigned long long) sb->bsb_stripe_size
		);
}
}

static inline m0_bindex_t
balloc_bn2gn(m0_bindex_t blockno, struct m0_balloc *cb)
{
	return blockno >> cb->cb_sb.bsb_gsbits;
}

M0_INTERNAL struct m0_balloc_group_info *m0_balloc_gn2info(struct m0_balloc *cb,
							   m0_bindex_t groupno)
{
	return cb->cb_group_info == NULL ? NULL : &cb->cb_group_info[groupno];
}

static struct m0_mutex *bgi_mutex(struct m0_balloc_group_info *grp)
{
	return &grp->bgi_mutex.bm_u.mutex;
}

static void lext_del(struct m0_lext *le)
{
	m0_list_del(&le->le_link);
	if (le->le_is_alloc)
		m0_free(le);
}

static struct m0_lext* lext_create(struct m0_ext *ex)
{
	struct m0_lext *le;

	le = m0_alloc(sizeof(*le));
	if (le == NULL)
		return NULL;

	le->le_is_alloc = true;
	le->le_ext = *ex;

	return le;
}

static void extents_release(struct m0_balloc_group_info *grp,
			    enum m0_balloc_allocation_flag zone_type)
{
	struct m0_list_link         *l;
	struct m0_lext              *le;
	struct m0_balloc_zone_param *zp;
	m0_bcount_t                  frags = 0;

	zp = is_spare(zone_type) ? &grp->bgi_spare : &grp->bgi_normal;
	while ((l = m0_list_first(&zp->bzp_extents)) != NULL) {
		le = m0_list_entry(l, struct m0_lext, le_link);
		lext_del(le);
		++frags;
	}
	M0_LOG(M0_DEBUG, "zone_type = %d, grp=%p grpno=%" PRIu64 " list_frags=%d"
	       "bzp_frags=%d", (int)zone_type, grp, grp->bgi_groupno,
	       (int)frags, (int)zp->bzp_fragments);
	M0_ASSERT(ergo(frags > 0, frags == zp->bzp_fragments));
}

M0_INTERNAL int m0_balloc_release_extents(struct m0_balloc_group_info *grp)
{


	M0_PRE(m0_mutex_is_locked(bgi_mutex(grp)));

	extents_release(grp, M0_BALLOC_SPARE_ZONE);
	extents_release(grp, M0_BALLOC_NORMAL_ZONE);
	m0_free0(&grp->bgi_extents);
	return 0;
}

M0_INTERNAL void m0_balloc_lock_group(struct m0_balloc_group_info *grp)
{
	m0_mutex_lock(bgi_mutex(grp));
}

M0_INTERNAL int m0_balloc_trylock_group(struct m0_balloc_group_info *grp)
{
	return m0_mutex_trylock(bgi_mutex(grp));
}

M0_INTERNAL void m0_balloc_unlock_group(struct m0_balloc_group_info *grp)
{
	m0_mutex_unlock(bgi_mutex(grp));
}

#define MAX_ALLOCATION_CHUNK 2048ULL

M0_INTERNAL void m0_balloc_group_desc_init(struct m0_balloc_group_desc *desc)
{
	m0_format_header_pack(&desc->bgd_header, &(struct m0_format_tag){
		.ot_version = M0_BALLOC_GROUP_DESC_FORMAT_VERSION,
		.ot_type    = M0_FORMAT_TYPE_BALLOC_GROUP_DESC,
		.ot_footer_offset = offsetof(struct m0_balloc_group_desc, bgd_footer)
	});
	m0_format_footer_update(desc);
}

static void balloc_format_init(struct m0_balloc *cb)
{
	m0_format_header_pack(&cb->cb_header, &(struct m0_format_tag){
		.ot_version = M0_BALLOC_FORMAT_VERSION,
		.ot_type    = M0_FORMAT_TYPE_BALLOC,
		.ot_footer_offset = offsetof(struct m0_balloc, cb_footer)
	});
	m0_format_footer_update(cb);
}

static int balloc_group_info_init(struct m0_balloc_group_info *gi,
				  struct m0_balloc *cb)
{
	struct m0_balloc_group_desc     gd = {};
	struct m0_balloc_super_block   *sb = &cb->cb_sb;
	struct m0_buf                   key;
	m0_bindex_t                     groupno;
	struct m0_buf                   val = M0_BUF_INIT_PTR(&gd);
	m0_bcount_t                     normal_zone_size;
	m0_bcount_t                     spare_zone_size;
	int                             rc;

	groupno = m0_byteorder_cpu_to_be64(gi->bgi_groupno);
	key = (struct m0_buf)M0_BUF_INIT_PTR(&groupno);
	rc = btree_lookup_sync(cb->cb_db_group_desc, &key, &val, false);
	if (rc == 0) {
		gi->bgi_state   = M0_BALLOC_GROUP_INFO_INIT;
		gi->bgi_extents = NULL;

		spare_zone_size =
			m0_stob_ad_spares_calc(cb->cb_sb.bsb_groupsize);
		normal_zone_size = cb->cb_sb.bsb_groupsize - spare_zone_size;

		balloc_zone_init(&gi->bgi_normal, M0_BALLOC_NORMAL_ZONE,
				 gd.bgd_groupno << sb->bsb_gsbits,
				 normal_zone_size,
				 gd.bgd_freeblocks, gd.bgd_fragments,
				 gd.bgd_maxchunk);
#ifdef __SPARE__SPACE__
		balloc_zone_init(&gi->bgi_spare, M0_BALLOC_SPARE_ZONE,
				 gd.bgd_sparestart, spare_zone_size,
				 gd.bgd_spare_freeblocks, gd.bgd_spare_frags,
				 gd.bgd_spare_maxchunk);
#else
		balloc_zone_init(&gi->bgi_spare, M0_BALLOC_SPARE_ZONE,
				 ((gd.bgd_groupno) << sb->bsb_gsbits) +
				 normal_zone_size,
				 spare_zone_size, 0, 0, 0);
#endif
		m0_mutex_init(bgi_mutex(gi));
	}
	return rc;
}

static void balloc_group_info_fini(struct m0_balloc_group_info *gi)
{
	m0_mutex_fini(bgi_mutex(gi));
	m0_list_fini(&gi->bgi_normal.bzp_extents);
	m0_list_fini(&gi->bgi_spare.bzp_extents);
}

static int balloc_group_info_load(struct m0_balloc *bal)
{
	struct m0_balloc_group_info *gi;
	m0_bcount_t                  i;
	int                          rc = 0;

	M0_LOG(M0_INFO, "Loading group info...");
	for (i = 0; i < bal->cb_sb.bsb_groupcount; ++i) {
		gi = &bal->cb_group_info[i];
		gi->bgi_groupno = i;
		rc = balloc_group_info_init(gi, bal);
		if (rc != 0)
			break;

		/* TODO verify the super_block info based on the group info */
	}
	while (rc != 0 && i > 0) {
		balloc_group_info_fini(&bal->cb_group_info[--i]);
	}
	return M0_RC(rc);
}

/**
   finalization of the balloc environment.
 */
static void balloc_fini_internal(struct m0_balloc *bal)
{
	struct m0_balloc_group_info *gi;
	int                          i;
	struct m0_btree_op           b_op = {};

	M0_ENTRY();

	if (bal->cb_group_info != NULL) {
		for (i = 0 ; i < bal->cb_sb.bsb_groupcount; i++) {
			gi = &bal->cb_group_info[i];
			m0_balloc_lock_group(gi);
			m0_balloc_release_extents(gi);
			m0_balloc_unlock_group(gi);
			balloc_group_info_fini(gi);
		}
		m0_free0(&bal->cb_group_info);
	}

	M0_BTREE_OP_SYNC_WITH_RC(&b_op,
				 m0_btree_close(bal->cb_db_group_extents,
						&b_op));
	m0_free0(&bal->cb_db_group_extents);

	M0_BTREE_OP_SYNC_WITH_RC(&b_op,
				 m0_btree_close(bal->cb_db_group_desc,
						&b_op));
	m0_free0(&bal->cb_db_group_desc);

	M0_LEAVE();
}

static int ge_tree_cmp(const void *k0, const void *k1)
{
	const m0_bindex_t *bn0 = (m0_bindex_t*)k0;
	const m0_bindex_t *bn1 = (m0_bindex_t*)k1;

	return M0_3WAY(*bn0, *bn1);
}

static void balloc_sb_sync(struct m0_balloc *cb, struct m0_be_tx *tx)
{
	struct m0_balloc_super_block    *sb = &cb->cb_sb;
	struct timeval                   now;
	m0_bcount_t                      bytes;
	struct m0_buf                    buf;

	M0_ENTRY();

	M0_PRE(m0_mutex_is_locked(&cb->cb_sb_mutex.bm_u.mutex));
	M0_PRE(cb->cb_sb.bsb_state & M0_BALLOC_SB_DIRTY);

	gettimeofday(&now, NULL);
	sb->bsb_write_time = ((uint64_t)now.tv_sec) << 32 | now.tv_usec;

	sb->bsb_magic = M0_BALLOC_SB_MAGIC;

	cb->cb_sb.bsb_state &= ~M0_BALLOC_SB_DIRTY;

	m0_format_footer_update(cb);
	bytes = offsetof(typeof(*cb), cb_footer) + sizeof(cb->cb_footer);
	buf = M0_BUF_INIT(bytes, cb);
	M0_BE_TX_CAPTURE_BUF(cb->cb_be_seg, tx, &buf);

	M0_LEAVE();
}

static int sb_update(struct m0_balloc *bal, struct m0_sm_group *grp)
{
	struct m0_be_tx                  tx = {};
	struct m0_be_tx_credit           cred;
	int				 rc;

	m0_mutex_lock(&bal->cb_sb_mutex.bm_u.mutex);

	bal->cb_sb.bsb_state |= M0_BALLOC_SB_DIRTY;

	m0_be_tx_init(&tx, 0, bal->cb_be_seg->bs_domain,
		      grp, NULL, NULL, NULL, NULL);
	cred = M0_BE_TX_CREDIT_PTR(bal);
	m0_be_tx_prep(&tx, &cred);
	rc = m0_be_tx_open_sync(&tx);
	if (rc == 0) {
		balloc_sb_sync(bal, &tx);
		m0_be_tx_close_sync(&tx);
		/* XXX error handling is missing here */
	}
	m0_be_tx_fini(&tx);

	m0_mutex_unlock(&bal->cb_sb_mutex.bm_u.mutex);

	return M0_RC(rc);
}

static int balloc_sb_write(struct m0_balloc            *bal,
			   struct m0_balloc_format_req *req,
			   struct m0_sm_group          *grp)
{
	int				 rc;
	struct timeval			 now;
	m0_bcount_t			 number_of_groups;
	struct m0_balloc_super_block	*sb = &bal->cb_sb;

	M0_ENTRY();

	M0_PRE(m0_is_po2(req->bfr_blocksize));
	M0_PRE(m0_is_po2(req->bfr_groupsize));

	number_of_groups = req->bfr_totalsize / req->bfr_blocksize /
	                            req->bfr_groupsize;
	if (number_of_groups < 1)
		number_of_groups = 1;

	M0_LOG(M0_DEBUG, "total=%llu bs=%llu groupsize=%llu groups=%llu "
			 "unused=%llu",
		(unsigned long long)req->bfr_totalsize,
		(unsigned long long)req->bfr_blocksize,
		(unsigned long long)req->bfr_groupsize,
		(unsigned long long)number_of_groups,
		(unsigned long long)(req->bfr_totalsize - number_of_groups *
				     req->bfr_groupsize * req->bfr_blocksize));

	gettimeofday(&now, NULL);
	/* TODO verification of these parameters */
	sb->bsb_magic		= M0_BALLOC_SB_MAGIC;
	sb->bsb_state		= 0;
	sb->bsb_version		= M0_BALLOC_SB_VERSION;

	/*
	  Total size is rounded by number of groups so that the rest
	  of space (little piece) at the end of device is not accounted
	  as used space.
	 */
	sb->bsb_totalsize	= number_of_groups * req->bfr_groupsize *
					req->bfr_blocksize;
				  /* should be power of 2*/
	sb->bsb_blocksize	= req->bfr_blocksize;
				  /* should be power of 2*/
	sb->bsb_groupsize	= req->bfr_groupsize;
	sb->bsb_bsbits		= ffs(req->bfr_blocksize) - 1;
	sb->bsb_gsbits		= ffs(req->bfr_groupsize) - 1;
	sb->bsb_groupcount	= number_of_groups;
#ifdef __SPARE_SPACE__
				  /* should be power of 2*/
	sb->bsb_sparesize       = req->bfr_spare_reserved_blocks;
	sb->bsb_freespare       = number_of_groups *
				    req->bfr_spare_reserved_blocks;
	sb->bsb_freeblocks	= (number_of_groups << sb->bsb_gsbits) -
					sb->bsb_freespare;
#else
	sb->bsb_freeblocks      = (number_of_groups << sb->bsb_gsbits);
#endif
	sb->bsb_prealloc_count	= 16;
	sb->bsb_format_time	= ((uint64_t)now.tv_sec) << 32 | now.tv_usec;
	sb->bsb_write_time	= sb->bsb_format_time;
	sb->bsb_mnt_time	= sb->bsb_format_time;
	sb->bsb_last_check_time	= sb->bsb_format_time;
	sb->bsb_mnt_count	= 0;
	sb->bsb_max_mnt_count	= 1024;
	sb->bsb_stripe_size	= 0;

	rc = sb_update(bal, grp);
	if (rc != 0)
		M0_LOG(M0_ERROR, "super_block update failed: rc=%d", rc);

	return M0_RC(rc);
}

struct balloc_group_write_cfg {
	struct m0_balloc *bgc_bal;
	m0_bcount_t       bgc_i;
};

struct balloc_groups_write_cfg {
	struct balloc_group_write_cfg *bgs_bgc;
	struct m0_balloc              *bgs_bal;
	m0_bcount_t                    bgs_max;
	int                            bgs_rc;
	struct m0_mutex                bgs_lock;
};

static void
balloc_group_write_credit(struct m0_balloc               *bal,
                          struct m0_be_tx_bulk           *tb,
                          struct balloc_groups_write_cfg *bgs,
                          struct m0_be_tx_credit         *credit)
{
	m0_btree_put_credit(bal->cb_db_group_extents, 2,
			    M0_MEMBER_SIZE(struct m0_ext, e_start),
			    M0_MEMBER_SIZE(struct m0_ext, e_end), credit);
	m0_btree_put_credit(bal->cb_db_group_desc, 2,
			    M0_MEMBER_SIZE(struct m0_balloc_group_desc,
					   bgd_groupno),
			    sizeof(struct m0_balloc_group_desc), credit);
}

static void balloc_group_work_put(struct m0_balloc               *bal,
                                  struct m0_be_tx_bulk           *tb,
                                  struct balloc_groups_write_cfg *bgs)
{
	struct balloc_group_write_cfg *bgc;
	struct m0_be_tx_credit         credit;
	m0_bcount_t                    i;
	bool                           put_successful;
	int                            rc;

	for (i = 0; i < bgs->bgs_max; ++i) {
		m0_mutex_lock(&bgs->bgs_lock);
		rc = bgs->bgs_rc;
		m0_mutex_unlock(&bgs->bgs_lock);
		if (rc != 0)
			break;
		bgc  = &bgs->bgs_bgc[i];
		*bgc = (struct balloc_group_write_cfg){
			.bgc_bal = bgs->bgs_bal,
			.bgc_i   = i,
		};
		credit = M0_BE_TX_CREDIT(0, 0);
		balloc_group_write_credit(bal, tb, bgs, &credit);
		M0_BE_OP_SYNC(op, put_successful =
			      m0_be_tx_bulk_put(tb, &op, &credit, 0, 0, bgc));
		if (!put_successful)
			break;
	}
	m0_be_tx_bulk_end(tb);
}

static void balloc_group_write_do(struct m0_be_tx_bulk *tb,
                                  struct m0_be_tx      *tx,
                                  struct m0_be_op      *op,
                                  void                 *datum,
                                  void                 *user,
                                  uint64_t              worker_index,
                                  uint64_t              partition)
{
	struct balloc_groups_write_cfg *bgs = datum;
	struct balloc_group_write_cfg  *bgc = user;
	struct m0_balloc               *bal = bgc->bgc_bal;
	struct m0_balloc_group_desc     gd;
	struct m0_balloc_super_block   *sb = &bal->cb_sb;
	struct m0_ext                   ext;
	struct m0_buf                   key;
	struct m0_buf                   val;
	m0_bindex_t                     groupno;
	m0_bcount_t                     i = bgc->bgc_i;
	m0_bcount_t                     spare_size;
	int                             rc;

	m0_be_op_active(op);

	M0_LOG(M0_DEBUG, "creating group_extents for group %llu",
	       (unsigned long long)i);
	spare_size = m0_stob_ad_spares_calc(sb->bsb_groupsize);
	/* Insert non-spare extents. */
	ext.e_start = i << sb->bsb_gsbits;
	ext.e_end = ext.e_start + sb->bsb_groupsize - spare_size;
	m0_ext_init(&ext);
	balloc_debug_dump_extent("create...", &ext);

	rc = balloc_ext_insert(bal->cb_db_group_extents, tx, ext);
	if (rc != 0) {
		M0_LOG(M0_ERROR, "insert extent failed: group=%llu "
				 "rc=%d", (unsigned long long)i, rc);
		goto out;
	}
#ifdef __SPARE_SPACE__
	/* Insert extents reserved for spare. */
	ext.e_start = (i << sb->bsb_gsbits) + sb->bsb_groupsize -
		spare_size;
	ext.e_end = ext.e_start + spare_size;
	rc = balloc_ext_insert(bal->cb_db_group_extents, tx, ext);
	if (rc != 0) {
		M0_LOG(M0_ERROR, "insert extent failed for spares: group=%llu "
				 "rc=%d", (unsigned long long)i, rc);
		goto out;
	}
#endif
	M0_LOG(M0_DEBUG, "creating group_desc for group %llu",
	       (unsigned long long)i);
	gd.bgd_groupno = i;
#ifdef __SPARE_SPACE__
	gd.bgd_spare_freeblocks = sb->bsb_sparesize;
	gd.bgd_sparestart = (i << sb->bsb_gsbits) + sb->bsb_groupsize -
				sb->bsb_sparesize;
	gd.bgd_spare_frags = 1;
	gd.bgd_spare_maxchunk = sb->bsb_sparesize;
#endif
	gd.bgd_freeblocks = sb->bsb_groupsize - spare_size;
	gd.bgd_maxchunk   = sb->bsb_groupsize - spare_size;
	gd.bgd_fragments  = 1;
	m0_balloc_group_desc_init(&gd);
	groupno = m0_byteorder_cpu_to_be64(gd.bgd_groupno);
	key = (struct m0_buf)M0_BUF_INIT_PTR(&groupno);
	val = (struct m0_buf)M0_BUF_INIT_PTR(&gd);

	rc = btree_insert_sync(bal->cb_db_group_desc, tx, &key, &val);
	if (rc != 0) {
		M0_LOG(M0_ERROR, "insert gd failed: group=%llu rc=%d",
			(unsigned long long)i, rc);
	}
out:
	if (rc != 0) {
		m0_mutex_lock(&bgs->bgs_lock);
		bgs->bgs_rc = rc;
		m0_mutex_unlock(&bgs->bgs_lock);
	}
	m0_be_op_done(op);
}

static void balloc_group_write_done(struct m0_be_tx_bulk *tb,
                                    void                 *datum,
                                    void                 *user,
                                    uint64_t              worker_index,
                                    uint64_t              partition)
{
	M0_LOG(M0_DEBUG, "tb=%p datum=%p user=%p worker_index=%" PRIu64 " "
	       "partition=%"PRIu64, tb, datum, user, worker_index, partition);
}

static void balloc_zone_init(struct m0_balloc_zone_param *zone, uint64_t type,
			     m0_bcount_t start, m0_bcount_t size,
			     m0_bcount_t freeblocks, m0_bcount_t fragments,
			     m0_bcount_t maxchunk)
{
	zone->bzp_type = type;
	zone->bzp_range.e_start = start;
	zone->bzp_range.e_end = start + size;
	zone->bzp_freeblocks = freeblocks;
	zone->bzp_fragments = fragments;
	zone->bzp_maxchunk = maxchunk;
	m0_list_init(&zone->bzp_extents);
}

static int balloc_groups_write(struct m0_balloc *bal)
{
	struct balloc_groups_write_cfg  bgs;
	struct m0_balloc_super_block   *sb = &bal->cb_sb;
	struct m0_be_tx_bulk_cfg        tb_cfg;
	struct m0_be_tx_bulk            tb = {};
	int                             rc;

	M0_ENTRY();

	M0_ALLOC_ARR(bal->cb_group_info, sb->bsb_groupcount);
	if (bal->cb_group_info == NULL) {
		return M0_ERR(-ENOMEM);
	}
	bgs = (struct balloc_groups_write_cfg) {
		.bgs_bal     = bal,
		.bgs_max     = sb->bsb_groupcount,
		.bgs_rc      = 0,
	};
	M0_ALLOC_ARR(bgs.bgs_bgc, bgs.bgs_max);
	if (bgs.bgs_bgc == NULL) {
		m0_free0(&bal->cb_group_info);
		return M0_ERR(-ENOMEM);
	}
	m0_mutex_init(&bgs.bgs_lock);
	tb_cfg = (struct m0_be_tx_bulk_cfg){
		.tbc_q_cfg                 = {
			.bqc_q_size_max       = 0x100,
			.bqc_producers_nr_max = 1,
		},
		.tbc_workers_nr            = 0x40,
		.tbc_partitions_nr         = 1,
		.tbc_work_items_per_tx_max = 1,
		.tbc_dom                   = bal->cb_be_seg->bs_domain,
		.tbc_datum                 = &bgs,
		.tbc_do                    = &balloc_group_write_do,
		.tbc_done                  = &balloc_group_write_done,
	};

	rc = m0_be_tx_bulk_init(&tb, &tb_cfg);
	if (rc == 0) {
		M0_BE_OP_SYNC(op, ({
			m0_be_tx_bulk_run(&tb, &op);
			balloc_group_work_put(bal, &tb, &bgs);
		}));
		rc = m0_be_tx_bulk_status(&tb);
		m0_be_tx_bulk_fini(&tb);
	}
	m0_mutex_fini(&bgs.bgs_lock);
	m0_free(bgs.bgs_bgc);
	if (bgs.bgs_rc != 0)
		rc = bgs.bgs_rc;

	rc = rc ?: balloc_group_info_load(bal);
	if (rc != 0) {
		/* balloc_fini_internal() checks whether this pointer is NULL */
		m0_free0(&bal->cb_group_info);
	}
	return M0_RC(rc);
}

/**
   Format the container: create database, fill them with initial information.

   This routine will create a "super_block" database to store global parameters
   for this container. It will also create "group free extent" and "group_desc"
   for every group. If some groups are reserved for special purpose, then they
   will be marked as "allocated" at the format time, and those groups will not
   be used by normal allocation routines.

   @param req pointer to this format request. All configuration will be passed
	  by this parameter.
   @return 0 means success. Otherwise, error number will be returned.
 */
static int balloc_format(struct m0_balloc *bal,
			 struct m0_balloc_format_req *req,
			 struct m0_sm_group *grp)
{
	int rc;

	M0_ENTRY();

	rc = balloc_sb_write(bal, req, grp);
	if (rc != 0)
		return M0_RC(rc);

	/**
	 * XXX Kludge.
	 *
	 * It should be removed after either fdatasync() for stobs is moved to
	 * ioq thread or direct I/O is used for seg I/O.
	 *
	 * The problem is that currently fdatasync() for seg stob is done
	 * in locality0 thread. If it's locked then it's possible that
	 * engine will wait for fdatasync() and ast that does fdatasync()
	 * can't be executed because locality0 is locked.
	 */
	m0_sm_group_unlock(grp);
	rc = balloc_groups_write(bal);
	m0_sm_group_lock(grp);

	return M0_RC(rc);
}

static void balloc_gi_sync_credit(const struct m0_balloc *cb,
					struct m0_be_tx_credit *accum)
{
	m0_btree_update_credit(cb->cb_db_group_desc, 1,
			       M0_MEMBER_SIZE(struct m0_balloc_group_desc,
					      bgd_groupno),
			       sizeof(struct m0_balloc_group_desc), accum);
}

static int balloc_gi_sync(struct m0_balloc *cb,
			  struct m0_be_tx  *tx,
			  struct m0_balloc_group_info *gi)
{
	struct m0_balloc_group_desc gd  = {};
	m0_bindex_t                 groupno;
	struct m0_buf               key;
	struct m0_buf               val;
	int                         rc;

	M0_ENTRY();

	M0_PRE(gi->bgi_state & M0_BALLOC_GROUP_INFO_DIRTY);

	gd.bgd_groupno	  = gi->bgi_groupno;
	gd.bgd_freeblocks = group_freeblocks_get(gi);
	gd.bgd_fragments  = group_fragments_get(gi);
	gd.bgd_maxchunk	  = group_maxchunk_get(gi);
#ifdef __SPARE_SPACE__
	gd.bgd_spare_freeblocks = group_spare_freeblocks_get(gi);
	gd.bgd_spare_frags = group_spare_fragments_get(gi);
	gd.bgd_spare_maxchunk = group_spare_maxchunk_get(gi);
	gd.bgd_sparestart = gi->bgi_spare.bzp_range.e_start;
#endif
	m0_balloc_group_desc_init(&gd);
	groupno = m0_byteorder_cpu_to_be64(gd.bgd_groupno);
	key = (struct m0_buf)M0_BUF_INIT_PTR(&groupno);
	val = (struct m0_buf)M0_BUF_INIT_PTR(&gd);
	rc = btree_update_sync(cb->cb_db_group_desc, tx, &key, &val);

	gi->bgi_state &= ~M0_BALLOC_GROUP_INFO_DIRTY;

	return M0_RC(rc);
}

static int sb_mount(struct m0_balloc *bal, struct m0_sm_group *grp)
{
	struct timeval now;

	gettimeofday(&now, NULL);
	bal->cb_sb.bsb_mnt_time = ((uint64_t)now.tv_sec) << 32 | now.tv_usec;
	++bal->cb_sb.bsb_mnt_count;

	return sb_update(bal, grp);
}

/*
 * start transaction for init() and format() respectively.
 * One transaction maybe fail to include all update. Multiple transaction
 * is used here.
 * The same reason for format().
 */
static int balloc_init_internal(struct m0_balloc *bal,
				struct m0_be_seg *seg,
				struct m0_sm_group *grp,
				uint32_t bshift,
				m0_bcount_t container_size,
				m0_bcount_t blocks_per_group,
				m0_bcount_t spare_blocks_per_group)
{
	int                        rc;
	struct m0_btree_op         b_op = {};
	struct m0_btree_rec_key_op ge_keycmp = { .rko_keycmp = ge_tree_cmp, };

	M0_ENTRY();

	bal->cb_be_seg = seg;
	bal->cb_group_info = NULL;
	m0_mutex_init(&bal->cb_sb_mutex.bm_u.mutex);

	M0_ALLOC_PTR(bal->cb_db_group_desc);
	if (bal->cb_db_group_desc == NULL)
		return M0_ERR(-ENOMEM);

	M0_ALLOC_PTR(bal->cb_db_group_extents);
	if (bal->cb_db_group_extents == NULL) {
		m0_free0(&bal->cb_db_group_desc);
		return M0_ERR(-ENOMEM);
	}

	rc = M0_BTREE_OP_SYNC_WITH_RC(&b_op,
				      m0_btree_open(bal->cb_gd_node,
						    sizeof bal->cb_gd_node,
						    bal->cb_db_group_desc, seg,
						    &b_op, NULL));
	M0_ASSERT(rc == 0);

	rc = M0_BTREE_OP_SYNC_WITH_RC(&b_op,
				      m0_btree_open(bal->cb_ge_node,
						    sizeof bal->cb_ge_node,
						    bal->cb_db_group_extents,
						    seg, &b_op, &ge_keycmp));
	M0_ASSERT(rc == 0);

	if (bal->cb_sb.bsb_magic != M0_BALLOC_SB_MAGIC) {
		struct m0_balloc_format_req req = { 0 };

		/* let's format this container */
		req.bfr_totalsize = container_size;
		req.bfr_blocksize = 1 << bshift;
		req.bfr_groupsize = blocks_per_group;
		req.bfr_spare_reserved_blocks = spare_blocks_per_group;

		rc = balloc_format(bal, &req, grp);
		if (rc != 0)
			balloc_fini_internal(bal);
		return M0_RC(rc);
	}

	if (bal->cb_sb.bsb_blocksize != 1 << bshift) {
		rc = -EINVAL;
		goto out;
	}

	M0_LOG(M0_INFO, "Group Count = %"PRIu64, bal->cb_sb.bsb_groupcount);

	M0_ALLOC_ARR(bal->cb_group_info, bal->cb_sb.bsb_groupcount);
	rc = bal->cb_group_info == NULL ? M0_ERR(-ENOMEM) : 0;
	if (rc == 0) {
		rc = balloc_group_info_load(bal);
		if (rc != 0)
			m0_free0(&bal->cb_group_info);
	}
	rc = rc ?: sb_mount(bal, grp);
out:
	if (rc != 0)
		balloc_fini_internal(bal);
	return M0_RC(rc);
}

static int balloc_init_ac(struct balloc_allocation_context *bac,
			  struct m0_balloc *motr,
			  struct m0_be_tx  *tx,
			  struct m0_balloc_allocate_req *req)
{
	M0_ENTRY();

	M0_SET0(&req->bar_result);
	m0_ext_init(&req->bar_result);

	bac->bac_ctxt	  = motr;
	bac->bac_tx	  = tx;
	bac->bac_req	  = req;
	bac->bac_order2	  = 0;
	bac->bac_scanned  = 0;
	bac->bac_found	  = 0;
	bac->bac_flags	  = req->bar_flags;
	bac->bac_status	  = M0_BALLOC_AC_CONTINUE;
	bac->bac_criteria = 0;

	if (req->bar_goal == 0)
		req->bar_goal = motr->cb_last;

	bac->bac_orig.e_start	= req->bar_goal;
	bac->bac_orig.e_end	= req->bar_goal + req->bar_len;
	m0_ext_init(&bac->bac_orig);
	bac->bac_goal = bac->bac_orig;

	M0_SET0(&bac->bac_best);
	M0_SET0(&bac->bac_final);
	m0_ext_init(&bac->bac_best);
	m0_ext_init(&bac->bac_final);

	return M0_RC(0);
}


static int balloc_use_prealloc(struct balloc_allocation_context *bac)
{
	return 0;
}

static bool is_spare(uint64_t alloc_flags)
{
	return alloc_flags & M0_BALLOC_SPARE_ZONE;
}

static bool is_normal(uint64_t alloc_flags)
{
	return alloc_flags & M0_BALLOC_NORMAL_ZONE;
}

static bool is_any(uint64_t alloc_flag)
{
	return is_spare(alloc_flag) && is_normal(alloc_flag);
}

/**
 * Checks if enough free blocks are present in super block.
 *
 * @param ctx balloc operation context environment.
 * @param blocks number of requested blocks.
 * @param alloc_flags allocation type.
 * @return true on enough free blocks.
 *	   false on requested number of free blocks not available.
 */
static bool balloc_got_freespace(const struct m0_balloc *ctx,
				 m0_bcount_t blocks, uint64_t alloc_flags)
{
	int rc;
	M0_ENTRY();

	M0_LOG(M0_DEBUG, "bsb_freeblocks=%llu blocks=%llu",
	       (unsigned long long)ctx->cb_sb.bsb_freeblocks,
	       (unsigned long long)blocks);
	rc =
#ifdef __SPARE_SPACE__
	is_any(alloc_flags) ? (ctx->cb_sb.bsb_freespare >= blocks ||
				    ctx->cb_sb.bsb_freeblocks >= blocks) :
	      is_spare(alloc_flags) ? ctx->cb_sb.bsb_freespare >= blocks :
#endif
		(ctx->cb_sb.bsb_freeblocks >= blocks);

	M0_LEAVE();
	return M0_RC(rc);
}

/*
 * here we normalize request for locality group
 */
static void
balloc_normalize_group_request(struct balloc_allocation_context *bac)
{
}

/*
 * Normalization means making request better in terms of
 * size and alignment
 */
static void
balloc_normalize_request(struct balloc_allocation_context *bac)
{
	m0_bcount_t size = m0_ext_length(&bac->bac_orig);
	M0_ENTRY();

	/* do normalize only for data requests. metadata requests
	   do not need preallocation */
	if (!(bac->bac_flags & M0_BALLOC_HINT_DATA))
		goto out;

	/* sometime caller may want exact blocks */
	if (bac->bac_flags & M0_BALLOC_HINT_GOAL_ONLY)
		goto out;

	/* caller may indicate that preallocation isn't
	 * required (it's a tail, for example) */
	if (bac->bac_flags & M0_BALLOC_HINT_NOPREALLOC)
		goto out;

	if (bac->bac_flags & M0_BALLOC_HINT_GROUP_ALLOC) {
		balloc_normalize_group_request(bac);
		goto out;
	}

	/* @todo : removing normalisation for time. */
	if (size <= 4) {
		size = 4;
	} else if (size <= 8) {
		size = 8;
	} else if (size <= 16) {
		size = 16;
	} else if (size <= 32) {
		size = 32;
	} else if (size <= 64) {
		size = 64;
	} else if (size <= 128) {
		size = 128;
	} else if (size <= 256) {
		size = 256;
	} else if (size <= 512) {
		size = 512;
	} else if (size <= 1024) {
		size = 1024;
	} else if (size <= 2048) {
		size = 2048;
	} else {
		M0_LOG(M0_WARN, "length %llu is too large, truncate to %llu",
			(unsigned long long) size, MAX_ALLOCATION_CHUNK);
		size = MAX_ALLOCATION_CHUNK;
	}

	if (size > bac->bac_ctxt->cb_sb.bsb_groupsize)
		size = bac->bac_ctxt->cb_sb.bsb_groupsize;

	/*
	  Now prepare new goal. Extra space we get will be consumed and
	  reserved by preallocation.
	*/
	bac->bac_goal.e_end = bac->bac_goal.e_start + size;

	M0_LOG(M0_DEBUG, "goal: start=%llu=(0x%08llx), size=%llu(was %llu)",
		(unsigned long long) bac->bac_goal.e_start,
		(unsigned long long) bac->bac_goal.e_start,
		(unsigned long long) m0_ext_length(&bac->bac_goal),
		(unsigned long long) m0_ext_length(&bac->bac_orig));
out:
	M0_LEAVE();
}

/* called under group lock */
#ifdef __SPARE_SPACE__
M0_INTERNAL int m0_balloc_load_extents(struct m0_balloc *cb,
				       struct m0_balloc_group_info *grp)
{
	struct m0_btree	       *db_ext = cb->cb_db_group_extents;
	struct m0_btree_cursor  cursor;
	struct m0_buf           key;
	struct m0_buf           val;
	struct m0_lext         *ex;
	struct m0_ext           spare_range;
	struct m0_ext           normal_range;
	m0_bindex_t             next_key;
	m0_bcount_t             i;
	int		        rc = 0;

	M0_ENTRY("grp=%d non-spare-frags=%d spare-frags=%d",
		 (int)grp->bgi_groupno, (int)group_fragments_get(grp),
		 (int)group_spare_fragments_get(grp));
	M0_PRE(m0_mutex_is_locked(bgi_mutex(grp)));

	if (grp->bgi_extents != NULL) {
		M0_LOG(M0_DEBUG, "Already loaded");
		return M0_RC(0);
	}

	M0_ALLOC_ARR(grp->bgi_extents, group_fragments_get(grp) +
		     group_spare_fragments_get(grp) + 1);
	if (grp->bgi_extents == NULL)
		return M0_RC(-ENOMEM);

	if (group_fragments_get(grp) == 0 &&
	    group_spare_fragments_get(grp) == 0) {
		M0_LOG(M0_NOTICE, "zero fragments");
		return M0_RC(0);
	}

	m0_btree_cursor_init(&cursor, db_ext);

	spare_range.e_start = grp->bgi_spare.bzp_range.e_start;
	spare_range.e_end = (grp->bgi_groupno + 1) << cb->cb_sb.bsb_gsbits;
	m0_ext_init(&spare_range);
	normal_range.e_start = grp->bgi_groupno << cb->cb_sb.bsb_gsbits;
	normal_range.e_end = spare_range.e_start;
	m0_ext_init(&normal_range);

	ex = grp->bgi_extents;
	ex->le_ext.e_end = (grp->bgi_groupno << cb->cb_sb.bsb_gsbits) + 1;
	next_key = ex->le_ext.e_end;
	for (i = 0; i < group_fragments_get(grp) +
	     group_spare_fragments_get(grp); i++, ex++) {
		key = (struct m0_buf)M0_BUF_INIT_PTR(&next_key);
		rc = m0_btree_cursor_get(&cursor, &key, true);
		if (rc != 0)
			break;
		m0_btree_cursor_kv_get(&cursor, &key, &val);
		ex->le_ext.e_end   = *(m0_bindex_t*)key.b_addr;
		ex->le_ext.e_end   = m0_byteorder_be64_to_cpu(ex->le_ext.e_end);
		ex->le_ext.e_start = *(m0_bindex_t*)val.b_addr;
		m0_ext_init(&ex->le_ext);
		if (m0_ext_is_partof(&normal_range, &ex->le_ext))
			m0_list_add_tail(group_normal_ext(grp), &ex->le_link);
		else if (m0_ext_is_partof(&spare_range, &ex->le_ext))
			m0_list_add_tail(group_spare_ext(grp), &ex->le_link);
		else {
			M0_LOG(M0_ERROR, "Invalid extent");
			M0_ASSERT(false);
		}
		next_key = ex->le_ext.e_end + 1;
		/* balloc_debug_dump_extent("loading...", ex); */
	}
	m0_btree_cursor_fini(&cursor);

	if (i != group_fragments_get(grp) + group_spare_fragments_get(grp))
		M0_LOG(M0_ERROR, "fragments mismatch: i=%llu frags=%lld",
			(unsigned long long)i,
			(unsigned long long)(group_fragments_get(grp) +
					     group_spare_fragments_get(grp)));
	if (rc != 0)
		m0_balloc_release_extents(grp);

	return M0_RC(rc);
}
#else
static void zone_params_update(struct m0_balloc_group_info *grp,
			       struct m0_ext *ext, uint64_t balloc_zone)
{
	struct m0_balloc_zone_param *zp;

	zp = balloc_zone == M0_BALLOC_NORMAL_ZONE ?
		&grp->bgi_normal : &grp->bgi_spare;
	zp->bzp_maxchunk = max_check(zp->bzp_maxchunk,
				     m0_ext_length(ext));
	zp->bzp_freeblocks += m0_ext_length(ext);
}

M0_INTERNAL int m0_balloc_load_extents(struct m0_balloc *cb,
				       struct m0_balloc_group_info *grp)
{
	struct m0_btree *db_ext = cb->cb_db_group_extents;
	struct m0_buf    key;
	struct m0_buf    val;
	struct m0_lext  *ex;
	struct m0_ext    spare_range;
	struct m0_ext    normal_range;
	m0_bcount_t      i;
	m0_bcount_t      normal_frags;
	m0_bcount_t      spare_frags;
	m0_bindex_t      next_key;
	int	         rc = 0;

	M0_ENTRY("grp=%d non-spare-frags=%d spare-frags=%d",
		 (int)grp->bgi_groupno, (int)group_fragments_get(grp),
		 (int)group_spare_fragments_get(grp));
	M0_PRE(m0_mutex_is_locked(bgi_mutex(grp)));

	if (grp->bgi_extents != NULL) {
		M0_LOG(M0_DEBUG, "Already loaded");
		return M0_RC(0);
	}

	if (group_fragments_get(grp) +
	    group_spare_fragments_get(grp) == 0) {
		M0_LOG(M0_NOTICE, "zero fragments");
		return M0_RC(0);
	}

	M0_ALLOC_ARR(grp->bgi_extents, group_fragments_get(grp) +
		     group_spare_fragments_get(grp) + 1);
	if (grp->bgi_extents == NULL)
		return M0_RC(-ENOMEM);

	spare_range.e_start = grp->bgi_spare.bzp_range.e_start;
	spare_range.e_end = (grp->bgi_groupno + 1) << cb->cb_sb.bsb_gsbits;
	m0_ext_init(&spare_range);
	normal_range.e_start = grp->bgi_groupno << cb->cb_sb.bsb_gsbits;
	normal_range.e_end = spare_range.e_start;
	m0_ext_init(&normal_range);

	ex = grp->bgi_extents;
	next_key = (grp->bgi_groupno << cb->cb_sb.bsb_gsbits) + 1;
	normal_frags = 0;
	spare_frags = 0;
	grp->bgi_normal.bzp_maxchunk = 0;
	grp->bgi_normal.bzp_freeblocks = 0;
	grp->bgi_spare.bzp_maxchunk = 0;
	grp->bgi_spare.bzp_freeblocks = 0;
	for (i = 0; i < group_fragments_get(grp) +
	     group_spare_fragments_get(grp); i++, ex++) {
		ex->le_ext.e_end = next_key;
		key = M0_BUF_INIT_PTR(&ex->le_ext.e_end);
		val = M0_BUF_INIT_PTR(&ex->le_ext.e_start);
		rc = btree_lookup_sync(db_ext, &key, &val, true);
		if (rc != 0)
			break;
		m0_ext_init(&ex->le_ext);
		if (m0_ext_is_partof(&normal_range, &ex->le_ext)) {
			m0_list_add_tail(group_normal_ext(grp), &ex->le_link);
			++normal_frags;
			zone_params_update(grp, &ex->le_ext,
					   M0_BALLOC_NORMAL_ZONE);
		} else if (m0_ext_is_partof(&spare_range, &ex->le_ext)) {
			m0_list_add_tail(group_spare_ext(grp), &ex->le_link);
			++spare_frags;
			zone_params_update(grp, &ex->le_ext,
					   M0_BALLOC_SPARE_ZONE);
		} else {
			M0_LOG(M0_ERROR, "Invalid extent");
			M0_ASSERT(false);
		}
		next_key = ex->le_ext.e_end + 1;
		balloc_debug_dump_extent("loading...", &ex->le_ext);
	}

	if (i != group_fragments_get(grp) + group_spare_fragments_get(grp))
		M0_LOG(M0_ERROR, "fragments mismatch: i=%llu frags=%lld",
			(unsigned long long)i,
			(unsigned long long)(group_fragments_get(grp) +
					     group_spare_fragments_get(grp)));
	if (rc != 0)
		m0_balloc_release_extents(grp);
	else {
		grp->bgi_normal.bzp_fragments = normal_frags;
		grp->bgi_spare.bzp_fragments = spare_frags;
	}

	return M0_RC(rc);
}
#endif

#if 0
/* called under group lock */
static int balloc_find_extent_exact(struct m0_balloc_allocation_context *bac,
				    struct m0_balloc_group_info *grp,
				    struct m0_ext *goal,
				    struct m0_ext *ex)
{
	m0_bcount_t	 i;
	int	 	 found = 0;
	struct m0_ext	*fragment;

	M0_PRE(m0_mutex_is_locked(bgi_mutex(grp)));

	for (i = 0; i < grp->bgi_fragments; i++) {
		fragment = &grp->bgi_extents[i];

		if (m0_ext_is_partof(fragment, goal)) {
			found = 1;
			*ex = *fragment;
			balloc_debug_dump_extent(__func__, ex);
			break;
		}
		if (fragment->e_start > goal->e_start)
			break;
	}

	return found;
}
#endif

/* called under group lock */
static int balloc_find_extent_buddy(struct balloc_allocation_context *bac,
				    struct m0_balloc_group_info *grp,
				    m0_bcount_t len,
				    enum m0_balloc_allocation_flag alloc_flag,
				    struct m0_ext *ex)
{
	int                          found = 0;
	m0_bcount_t                  flen;
	m0_bcount_t start;
	struct m0_ext               *frag;
	struct m0_lext              *le;
	struct m0_ext                min = {
					.e_start = 0,
					.e_end = 0xffffffff,
				     };
	struct m0_balloc_zone_param *zp;

	M0_PRE(m0_mutex_is_locked(bgi_mutex(grp)));

	m0_ext_init(&min);

	zp = is_spare(alloc_flag) ? &grp->bgi_spare : &grp->bgi_normal;

	M0_LOG(M0_DEBUG, "start=%" PRIu64 " len=%"PRIu64,
	       zp->bzp_range.e_start, len);

	start = zp->bzp_range.e_start;
	m0_list_for_each_entry(&zp->bzp_extents, le, struct m0_lext, le_link) {
		frag = &le->le_ext;
		flen = m0_ext_length(frag);
		M0_LOG(M0_DEBUG, "frag="EXT_F, EXT_P(frag));
repeat:
		/*
		{
			char msg[128];
			sprintf(msg, "buddy[s=%llu:0x%08llx, l=%u:0x%08x]",
			(unsigned long long)start,
			(unsigned long long)start,
			(int)len, (int)len);
			(void)msg;
			balloc_debug_dump_extent(msg, frag);
			}
		*/
		if (frag->e_start == start && flen >= len) {
			++found;
			if (flen < m0_ext_length(&min))
				min = *frag;
			if (flen == len || found > M0_BALLOC_BUDDY_LOOKUP_MAX)
				break;
		}
		if (frag->e_start > start) {
			do {
				start += len;
			} while (frag->e_start > start);
			if (start >= zp->bzp_range.e_end)
				break;
			/* we changed the 'start'. let's restart searching. */
			goto repeat;
		}
	}

	if (found > 0)
		*ex = min;

	return found;
}

static int balloc_use_best_found(struct balloc_allocation_context *bac,
				 m0_bindex_t start)
{
	m0_bcount_t len = m0_ext_length(&bac->bac_goal);

	while (start < bac->bac_best.e_start)
		start += len;

	M0_LOG(M0_DEBUG, "start=0x%"PRIx64, start);

	if (start < bac->bac_best.e_end && len <= bac->bac_best.e_end - start)
		bac->bac_final.e_start = start;
	else
		bac->bac_final.e_start = bac->bac_best.e_start;

	bac->bac_final.e_end = bac->bac_final.e_start +
		min_check(m0_ext_length(&bac->bac_best), len);
	M0_LOG(M0_DEBUG, "final="EXT_F, EXT_P(&bac->bac_final));
	bac->bac_status = M0_BALLOC_AC_FOUND;

	return 0;
}

static int balloc_new_preallocation(struct balloc_allocation_context *bac)
{
	/* XXX No Preallocation now. So, trim the result to the original length. */

	bac->bac_final.e_end = bac->bac_final.e_start +
					min_check(m0_ext_length(&bac->bac_orig),
					          m0_ext_length(&bac->bac_final));
	return 0;
}

static void balloc_sb_sync_credit(const struct m0_balloc *bal,
					struct m0_be_tx_credit *accum)
{
	struct m0_be_tx_credit cred = M0_BE_TX_CREDIT_PTR(bal);

	m0_be_tx_credit_add(accum, &cred);
}

static void balloc_db_update_credit(const struct m0_balloc *bal, int nr,
					  struct m0_be_tx_credit *accum)
{
	const struct m0_btree  *tree = bal->cb_db_group_extents;
	struct m0_be_tx_credit  cred = {};

	m0_btree_del_credit(tree, 1,
			    M0_MEMBER_SIZE(struct m0_ext, e_start),
			    M0_MEMBER_SIZE(struct m0_ext, e_end), &cred);
	m0_btree_put_credit(tree, 1,
			    M0_MEMBER_SIZE(struct m0_ext, e_start),
			    M0_MEMBER_SIZE(struct m0_ext, e_end), &cred);
	m0_btree_update_credit(tree, 2,
			       M0_MEMBER_SIZE(struct m0_ext, e_start),
			       M0_MEMBER_SIZE(struct m0_ext, e_end), &cred);
	balloc_sb_sync_credit(bal, &cred);
	balloc_gi_sync_credit(bal, &cred);
	m0_be_tx_credit_mac(accum, &cred, nr);
}

/**
 * Checks if requested extent is free in target group.
 *
 * @param[in]  grp	  target group.
 * @param[in]  tgt	  requested extent.
 * @param[in]  alloc_type allocation type.
 * @param[out] current    free extent in target group for requested extent.
 * @return true if extent is free.
 *	   false if extent is already allocated.
 */
static bool is_extent_free(struct m0_balloc_group_info *grp,
			   const struct m0_ext *tgt, uint64_t alloc_type,
			   struct m0_ext **current)
{
	struct m0_lext              *le;
	struct m0_balloc_zone_param *zp;
	m0_bcount_t                  frags = 0;

	M0_ENTRY();

	zp = is_spare(alloc_type) ? &grp->bgi_spare : &grp->bgi_normal;

	m0_list_for_each_entry(&zp->bzp_extents, le, struct m0_lext, le_link) {
		*current = &le->le_ext;
		if (m0_ext_is_partof(*current, tgt))
			break;
		++frags;
	}
	return frags < zp->bzp_fragments;
}

static int balloc_alloc_db_update(struct m0_balloc *motr, struct m0_be_tx *tx,
				  struct m0_balloc_group_info *grp,
				  struct m0_ext *tgt, uint64_t alloc_type,
				  struct m0_ext *cur)
{
	struct m0_btree             *db  = motr->cb_db_group_extents;
	struct m0_buf                key;
	struct m0_lext              *le;
	struct m0_lext              *lcur;
	struct m0_balloc_zone_param *zp;
	int                          rc = 0;
	m0_bcount_t                  maxchunk;

	M0_ENTRY();
	M0_PRE(m0_mutex_is_locked(bgi_mutex(grp)));
	M0_PRE(!is_any(alloc_type));
	M0_PRE(M0_IN(alloc_type, (M0_BALLOC_SPARE_ZONE,
				  M0_BALLOC_NORMAL_ZONE)));
	M0_PRE(cur != NULL);

	balloc_debug_dump_extent("target=", tgt);

	zp = is_spare(alloc_type) ? &grp->bgi_spare : &grp->bgi_normal;
	maxchunk = zp->bzp_maxchunk;

	balloc_debug_dump_extent("current=", cur);

	if (m0_ext_length(cur) == zp->bzp_maxchunk) {
		/* find next to max sized chunk */
		maxchunk = 0;
		m0_list_for_each_entry(&zp->bzp_extents, le,
				       struct m0_lext, le_link) {
			if (&le->le_ext == cur)
				continue;
			maxchunk = max_check(maxchunk,
					     m0_ext_length(&le->le_ext));
		}
	}

	M0_LOG(M0_DEBUG, "bzp_maxchunk=0x%" PRIx64 " next_maxchunk=0x%"PRIx64,
	       zp->bzp_maxchunk, maxchunk);

	if (cur->e_end == tgt->e_end) {
		key = (struct m0_buf)M0_BUF_INIT_PTR(&cur->e_end);

		rc = btree_delete_sync(db, tx, &key);
		if (rc != 0)
			return M0_RC(rc);

		if (cur->e_start < tgt->e_start) {
			/* +--------------+--------------------+ */
			/* |   cur free   |     allocated      | */
			/* |      |  tgt  |                    | */
			/* +------+-------+--------------------+ */
			cur->e_end = tgt->e_start;
			rc = balloc_ext_insert(db, tx, *cur);
			if (rc != 0)
				return M0_RC(rc);
			maxchunk = max_check(maxchunk, m0_ext_length(cur));
		} else {
			/* +-------------+---------------------+ */
			/* |   cur free  |      allocated      | */
			/* |     tgt     |                     | */
			/* +-------------+---------------------+ */
			le = container_of(cur, struct m0_lext, le_ext);
			lext_del(le);
			zp->bzp_fragments--;
		}
	} else {
		struct m0_ext new = *cur;

		/* +-----------------------------------+ */
		/* |              cur free             | */
		/* |     tgt    |                      | */
		/* +------------+----------------------+ */
		cur->e_start = tgt->e_end;
		rc = balloc_ext_update(db, tx, *cur);
		if (rc != 0)
			return M0_RC(rc);

		maxchunk = max_check(maxchunk, m0_ext_length(cur));

		if (new.e_start < tgt->e_start) {
			/* +-----------------------------------+ */
			/* |              cur free             | */
			/* |  new  |   tgt   |                 | */
			/* +-------+---------+-----------------+ */
			new.e_end = tgt->e_start;
			le = lext_create(&new);
			if (le == NULL)
				return M0_RC(-ENOMEM);
			rc = balloc_ext_insert(db, tx, new);
			if (rc != 0) {
				m0_free(le);
				return M0_RC(rc);
			}
			lcur = container_of(cur, struct m0_lext, le_ext);
			m0_list_add_before(&lcur->le_link, &le->le_link);
			zp->bzp_fragments++;
			maxchunk = max_check(maxchunk, m0_ext_length(&new));
		}
	}
	zp->bzp_maxchunk = maxchunk;
	zp->bzp_freeblocks -= m0_ext_length(tgt);

	grp->bgi_state |= M0_BALLOC_GROUP_INFO_DIRTY;

	m0_mutex_lock(&motr->cb_sb_mutex.bm_u.mutex);
#ifdef __SPARE_SPACE__
	if (is_spare(alloc_type))
		motr->cb_sb.bsb_freespare -= m0_ext_length(tgt);
	else
#endif
		motr->cb_sb.bsb_freeblocks -= m0_ext_length(tgt);
	motr->cb_sb.bsb_state |= M0_BALLOC_SB_DIRTY;
	balloc_sb_sync(motr, tx);
	m0_mutex_unlock(&motr->cb_sb_mutex.bm_u.mutex);

	rc = balloc_gi_sync(motr, tx, grp);

	return M0_RC(rc);
}

static int balloc_free_db_update(struct m0_balloc *motr,
				 struct m0_be_tx *tx,
				 struct m0_balloc_group_info *grp,
				 struct m0_ext *tgt, uint64_t alloc_flag)
{
	struct m0_buf                key;
	struct m0_btree             *db = motr->cb_db_group_extents;
	struct m0_ext               *cur = NULL;
	struct m0_ext               *pre = NULL;
	struct m0_lext              *le;
	struct m0_lext              *lcur;
	struct m0_balloc_zone_param *zp;
	m0_bcount_t                  frags = 0;
	m0_bcount_t                  maxchunk;
	int                          rc = 0;
	int                          found = 0;

	M0_ENTRY();
	M0_PRE(m0_mutex_is_locked(bgi_mutex(grp)));

	balloc_debug_dump_extent("target=", tgt);

	zp = is_spare(alloc_flag) ? &grp->bgi_spare : &grp->bgi_normal;
	maxchunk = zp->bzp_maxchunk;
	m0_list_for_each_entry(&zp->bzp_extents, le, struct m0_lext, le_link) {
		cur = &le->le_ext;

		if (cur->e_start >= tgt->e_start) {
			found = 1;
			break;
		}
		pre = cur;
		++frags;
	}
	balloc_debug_dump_extent("prev=", pre);
	balloc_debug_dump_extent("current=", cur);

	if (found && cur && tgt->e_end > cur->e_start) {
		M0_LOG(M0_ERROR, "!!!!!!!!!!!!!double free: "
				 "tgt_end=%llu cur_start=%llu",
		       (unsigned long long)tgt->e_end,
		       (unsigned long long)cur->e_start);
		m0_balloc_debug_dump_group_extent(
			    "double free with cur", grp);
		return M0_RC(-EINVAL);
	}
	if (pre && pre->e_end > tgt->e_start) {
		M0_LOG(M0_ERROR, "!!!!!!!!!!!!!double free: "
				 "pre_end=%llu tgt_start=%llu",
		       (unsigned long long)pre->e_end,
		       (unsigned long long)tgt->e_start);
		m0_balloc_debug_dump_group_extent(
			    "double free with pre", grp);
		return M0_RC(-EINVAL);
	}

	lcur = container_of(cur, struct m0_lext, le_ext);

	if (!found) {
		if (frags == 0) {
			/*       No free fragments at all:       */
			/* +-----------------------------------+ */
			/* |              allocated            | */
			/* |       |     tgt free     |        | */
			/* +-------+------------------+--------+ */
			le = lext_create(tgt);
			if (le == NULL)
				return M0_RC(-ENOMEM);
			rc = balloc_ext_insert(db, tx, *tgt);
			if (rc != 0) {
				m0_free(le);
				return M0_RC(rc);
			}
			m0_list_add(&zp->bzp_extents, &le->le_link);
			++zp->bzp_fragments;
			maxchunk = max_check(maxchunk, m0_ext_length(tgt));
		} else {
			/* at the tail */
			if (cur->e_end < tgt->e_start) {
				/*    To be the last one, standalone:    */
				/* +-----------+-----------------------+ */
				/* |    cur    |                       | */
				/* |           |  |      tgt free      | */
				/* +-----------+--+--------------------+ */
				le = lext_create(tgt);
				if (le == NULL)
					return M0_RC(-ENOMEM);
				rc = balloc_ext_insert(db, tx, *tgt);
				if (rc != 0) {
					m0_free(le);
					return M0_RC(rc);
				}
				m0_list_add_after(&lcur->le_link, &le->le_link);
				++zp->bzp_fragments;
				maxchunk = max_check(maxchunk, m0_ext_length(tgt));
			} else {
				/* +-----------+-----------------------+ */
				/* |    cur    |-->                    | */
				/* |           |       tgt free        | */
				/* +-----------+-----------------------+ */
				M0_ASSERT(cur->e_end == tgt->e_start);
				key = (struct m0_buf)M0_BUF_INIT_PTR(&cur->e_end);
				rc = btree_delete_sync(db, tx, &key);
				if (rc != 0)
					return M0_RC(rc);
				cur->e_end = tgt->e_end;
				rc = balloc_ext_insert(db, tx, *cur);
				if (rc != 0)
					return M0_RC(rc);
				maxchunk = max_check(maxchunk, m0_ext_length(cur));
			}
		}
	} else if (found && pre == NULL) {
		/* on the head */
		if (tgt->e_end < cur->e_start) {
			/*       To be the first one:            */
			/* +---------------+-------------------+ */
			/* |               |     cur free      | */
			/* | |   tgt   |   |                   | */
			/* +-+---------+---+-------------------+ */
			le = lext_create(tgt);
			if (le == NULL)
				return M0_RC(-ENOMEM);
			rc = balloc_ext_insert(db, tx, *tgt);
			if (rc != 0) {
				m0_free(le);
				return M0_RC(rc);
			}
			m0_list_add_before(&lcur->le_link, &le->le_link);
			++zp->bzp_fragments;
			maxchunk = max_check(maxchunk, m0_ext_length(tgt));
		} else {
			/*      Join with the first one:         */
			/* +---------------+-------------------+ */
			/* |            <--|     cur free      | */
			/* |     |   tgt   |                   | */
			/* +-----+---------+-------------------+ */
			M0_ASSERT(tgt->e_end == cur->e_start);
			cur->e_start = tgt->e_start;
			rc = balloc_ext_update(db, tx, *cur);
			if (rc != 0)
				return M0_RC(rc);
			maxchunk = max_check(maxchunk, m0_ext_length(cur));
		}
	} else {
		/* in the middle */
		if (pre->e_end == tgt->e_start &&
		    tgt->e_end == cur->e_start) {
			/*        Joint with both:               */
			/* +-------+-------+-------------------+ */
			/* |  pre  |--> <--|    cur free       | */
			/* |       |  tgt  |                   | */
			/* +-------+-------+-------------------+ */
			key = (struct m0_buf)M0_BUF_INIT_PTR(&pre->e_end);
			rc = btree_delete_sync(db, tx, &key);
			if (rc != 0)
				return M0_RC(rc);
			cur->e_start = pre->e_start;
			rc = balloc_ext_update(db, tx, *cur);
			if (rc != 0)
				return M0_RC(rc);
			le = container_of(pre, struct m0_lext, le_ext);
			lext_del(le);
			--zp->bzp_fragments;
			maxchunk = max_check(maxchunk, m0_ext_length(cur));
		} else if (pre->e_end == tgt->e_start) {
			/*          Joint with prev:             */
			/* +-------+----------+----------------+ */
			/* |  pre  |-->       |    cur free    | */
			/* |       |  tgt  |                   | */
			/* +-------+-------+-------------------+ */
			key = (struct m0_buf)M0_BUF_INIT_PTR(&pre->e_end);
			rc = btree_delete_sync(db, tx, &key);
			if (rc != 0)
				return M0_RC(rc);
			pre->e_end = tgt->e_end;
			rc = balloc_ext_insert(db, tx, *pre);
			if (rc != 0)
				return M0_RC(rc);
			maxchunk = max_check(maxchunk, m0_ext_length(pre));
		} else if (tgt->e_end == cur->e_start) {
			/*        Joint with current:            */
			/* +-------+----------+----------------+ */
			/* |  pre  |       <--|    cur free    | */
			/* |          |  tgt  |                | */
			/* +----------+-------+----------------+ */
			cur->e_start = tgt->e_start;
			rc = balloc_ext_update(db, tx, *cur);
			if (rc != 0)
				return M0_RC(rc);
			maxchunk = max_check(maxchunk, m0_ext_length(cur));
		} else {
			/*           Add a new one:              */
			/* +-------+------------+--------------+ */
			/* |  pre  |            |   cur free   | */
			/* |          |  tgt  |                | */
			/* +----------+-------+----------------+ */
			le = lext_create(tgt);
			if (le == NULL)
				return M0_RC(-ENOMEM);
			rc = balloc_ext_insert(db, tx, *tgt);
			if (rc != 0) {
				m0_free(le);
				return M0_RC(rc);
			}
			m0_list_add_before(&lcur->le_link, &le->le_link);
			++zp->bzp_fragments;
			maxchunk = max_check(maxchunk, m0_ext_length(tgt));
		}
	}
	zp->bzp_maxchunk = maxchunk;
	zp->bzp_freeblocks += m0_ext_length(tgt);

	grp->bgi_state |= M0_BALLOC_GROUP_INFO_DIRTY;

	m0_mutex_lock(&motr->cb_sb_mutex.bm_u.mutex);
	if (is_spare(alloc_flag))
#ifdef __SPARE_SPACE__
		motr->cb_sb.bsb_freespare += m0_ext_length(tgt);
#else
		motr->cb_sb.bsb_freeblocks += m0_ext_length(tgt);
#endif
	else
		motr->cb_sb.bsb_freeblocks += m0_ext_length(tgt);
	motr->cb_sb.bsb_state |= M0_BALLOC_SB_DIRTY;
	balloc_sb_sync(motr, tx);
	m0_mutex_unlock(&motr->cb_sb_mutex.bm_u.mutex);

	rc = balloc_gi_sync(motr, tx, grp);

	return M0_RC(rc);
}

#if 0
static int balloc_find_by_goal(struct m0_balloc_allocation_context *bac)
{
	m0_bindex_t group = balloc_bn2gn(bac->bac_goal.e_start,
					 bac->bac_ctxt);
	struct m0_balloc_group_info *grp = m0_balloc_gn2info(bac->bac_ctxt,
							     group);
	struct m0_be_tx *tx  = bac->bac_tx;
	struct m0_ext	 ex  = { 0 };
	int		 found;
	int		 ret = 0;
	M0_ENTRY();

	if (!(bac->bac_flags & M0_BALLOC_HINT_TRY_GOAL))
		goto out;

	M0_LOG(M0_DEBUG, "groupno=%llu start=%llu len=%llu groupsize=%llu",
		(unsigned long long)group,
		(unsigned long long)bac->bac_goal.e_start,
		(unsigned long long)m0_ext_length(&bac->bac_goal),
		(unsigned long long)bac->bac_ctxt->cb_sb.bsb_groupsize
	);

	m0_balloc_lock_group(grp);
	if (grp->bgi_maxchunk < m0_ext_length(&bac->bac_goal)) {
		M0_LEAVE();
		goto out_unlock;
	}
	if (grp->bgi_freeblocks < m0_ext_length(&bac->bac_goal)) {
		M0_LEAVE();
		goto out_unlock;
	}

	ret = m0_balloc_load_extents(bac->bac_ctxt, grp);
	if (ret) {
		M0_LEAVE();
		goto out_unlock;
	}

	found = balloc_find_extent_exact(bac, grp, &bac->bac_goal, &ex);
	M0_LOG(M0_DEBUG, "found?max len = %llu",
	       (unsigned long long)m0_ext_length(&ex));

	if (found) {
		bac->bac_found++;
		bac->bac_best.e_start = bac->bac_goal.e_start;
		bac->bac_best.e_end   = ex.e_end;
		ret = balloc_use_best_found(bac);
	}

	/* update db according to the allocation result */
	if (ret == 0 && bac->bac_status == M0_BALLOC_AC_FOUND) {
		if (bac->bac_goal.e_end < bac->bac_best.e_end)
			balloc_new_preallocation(bac);

		ret = balloc_alloc_db_update(bac->bac_ctxt, tx, grp,
					  &bac->bac_final);
	}

	m0_balloc_release_extents(grp);
out_unlock:
	m0_balloc_unlock_group(grp);
out:
	return M0_RC(ret);
}
#endif

/* group is locked */
static int balloc_is_good_group(struct balloc_allocation_context *bac,
				struct m0_balloc_group_info *gi)
{
	if (is_any(bac->bac_flags)) {
		return is_group_good_enough(bac, group_spare_maxchunk_get(gi),
					    group_spare_freeblocks_get(gi),
					    group_spare_fragments_get(gi)) ||
			is_group_good_enough(bac, group_maxchunk_get(gi),
					     group_freeblocks_get(gi),
					     group_fragments_get(gi));
	} else if (is_spare(bac->bac_flags)) {
		return is_group_good_enough(bac, group_spare_maxchunk_get(gi),
					    group_spare_freeblocks_get(gi),
					    group_spare_fragments_get(gi));
	} else if (is_normal(bac->bac_flags)) {
		return is_group_good_enough(bac, group_maxchunk_get(gi),
					    group_freeblocks_get(gi),
					    group_fragments_get(gi));
	} else
		M0_ASSERT(0);
	/* The case when asserts are disabled */
	return 0;
}

static int is_group_good_enough(struct balloc_allocation_context *bac,
				m0_bcount_t maxchunk, m0_bcount_t free,
				m0_bcount_t fragments)
{
	if (free == 0) {
		M0_LOG(M0_DEBUG, "bac=%p: no free blocks", bac);
		return 0;
	}

	if (fragments == 0) {
		M0_LOG(M0_DEBUG, "bac=%p: no fragments", bac);
		return 0;
	}

	switch (bac->bac_criteria) {
	case 0:
		if (maxchunk >= m0_ext_length(&bac->bac_goal))
			return 1;
		break;
	case 1:
		if ((free / fragments) >= m0_ext_length(&bac->bac_goal))
			return 2;
		break;
	case 2:
			return 3;
		break;
	default:
		M0_ASSERT(0);
	}

	M0_LOG(M0_DEBUG, "bac=%p criteria=%d: no big enough chunk: "
	       "goal=0x%08" PRIx64 " maxchunk=0x%08"PRIx64,
	       bac, bac->bac_criteria, m0_ext_length(&bac->bac_goal), maxchunk);

	return 0;
}

/* group is locked */
static int balloc_simple_scan_group(struct balloc_allocation_context *bac,
				    struct m0_balloc_group_info *grp,
				    enum m0_balloc_allocation_flag alloc_flag)
{
/*
	struct m0_balloc_super_block *sb = &bac->bac_ctxt->cb_sb;
*/
	struct m0_ext	ex;
	m0_bcount_t	len;
	m0_bindex_t     start;
	int		found = 0;

	M0_ENTRY();
	M0_PRE(bac->bac_order2 > 0);

	m0_ext_init(&ex);
	len = 1 << bac->bac_order2;
/*	for (; len <= sb->bsb_groupsize; len = len << 1) {
		M0_LOG(M0_DEBUG, "searching at %d (gs = %d) for order = %d "
			"len=%d:%x",
			(int)grp->bgi_groupno,
			(int)sb->bsb_groupsize,
			(int)bac->bac_order2,
			(int)len,
			(int)len);

		found = balloc_find_extent_buddy(bac, grp, len, &ex);
		if (found)
			break;
	}
*/

	found = balloc_find_extent_buddy(bac, grp, len, alloc_flag, &ex);
	if (found) {
		balloc_debug_dump_extent("found at simple scan", &ex);

		bac->bac_found++;
		bac->bac_best = ex;
		start = zone_start_get(grp, alloc_flag);
		balloc_use_best_found(bac, start);
	}

	M0_LEAVE();
	return 0;
}

static m0_bindex_t zone_start_get(struct m0_balloc_group_info *grp,
				  enum m0_balloc_allocation_flag alloc_flag)
{
	return is_spare(alloc_flag) ? grp->bgi_spare.bzp_range.e_start :
			grp->bgi_normal.bzp_range.e_start;
}

__attribute__((unused))
static int balloc_aligned_scan_group(struct balloc_allocation_context *bac,
				     struct m0_balloc_group_info *grp)
{
	return 0;
}

/*
 * How long balloc can look for a best extent (in found extents)
 */
#define M0_BALLOC_DEFAULT_MAX_TO_SCAN	       200

/*
 * How long balloc must look for a best extent
 */
#define M0_BALLOC_DEFAULT_MIN_TO_SCAN	       10

/*
 * How many groups balloc will scan looking for the best chunk
 */
#define M0_BALLOC_DEFAULT_MAX_GROUPS_TO_SCAN   5


static int balloc_check_limits(struct balloc_allocation_context *bac,
			       struct m0_balloc_group_info *grp,
			       int end_of_group,
			       enum m0_balloc_allocation_flag alloc_flag)
{
	int  max_to_scan = M0_BALLOC_DEFAULT_MAX_TO_SCAN;
	int  max_groups  = M0_BALLOC_DEFAULT_MAX_GROUPS_TO_SCAN;
	int  min_to_scan = M0_BALLOC_DEFAULT_MIN_TO_SCAN;
	M0_ENTRY();

	M0_LOG(M0_DEBUG, "check limits for group %llu. end = %d",
		(unsigned long long)grp->bgi_groupno,
		end_of_group);

	if (bac->bac_status == M0_BALLOC_AC_FOUND)
		return 0;

	if ((bac->bac_found > max_to_scan || bac->bac_scanned > max_groups) &&
		!(bac->bac_flags & M0_BALLOC_HINT_FIRST)) {
		bac->bac_status = M0_BALLOC_AC_BREAK;
		return 0;
	}

	if (m0_ext_length(&bac->bac_best) < m0_ext_length(&bac->bac_goal))
		return 0;

	if ((end_of_group || bac->bac_found >= min_to_scan)) {
		m0_bindex_t group = balloc_bn2gn(bac->bac_best.e_start,
						    bac->bac_ctxt);
		if (group == grp->bgi_groupno) {
			balloc_use_best_found(bac, zone_start_get(grp,
								  alloc_flag));
		}
	}

	return 0;
}

static int balloc_measure_extent(struct balloc_allocation_context *bac,
				 struct m0_balloc_group_info *grp,
				 enum m0_balloc_allocation_flag alloc_flag,
				 struct m0_ext *ex, int end_of_group)
{
	struct m0_ext *goal = &bac->bac_goal;
	struct m0_ext *best = &bac->bac_best;
	int rc = 0;
	M0_ENTRY();

	balloc_debug_dump_extent(__func__, ex);
	bac->bac_found++;

	if ((bac->bac_flags & M0_BALLOC_HINT_FIRST) ||
	     m0_ext_length(ex) == m0_ext_length(goal)) {
		*best = *ex;
		balloc_use_best_found(bac, zone_start_get(grp,
							  alloc_flag));
		M0_LEAVE();
		return 0;
	}

	if (m0_ext_length(best) == 0) {
		*best = *ex;
		M0_LEAVE();
		return 0;
	}

	if (m0_ext_length(best) < m0_ext_length(goal)) {
		/* req is still not satisfied. use the larger one */
		if (m0_ext_length(ex) > m0_ext_length(best)) {
			*best = *ex;
		}
	} else if (m0_ext_length(ex) >= m0_ext_length(goal)) {
		/* req is satisfied. but it is satisfied again.
		   use the smaller one */
		if (m0_ext_length(ex) < m0_ext_length(best)) {
			*best = *ex;
		}
	}

	if (m0_ext_length(best) >= m0_ext_length(goal))
		rc = balloc_check_limits(bac, grp, end_of_group, alloc_flag);
	M0_LEAVE();
	return M0_RC(rc);
}

/**
 * This function scans the specified group for a goal. If maximal
 * group is locked.
 */
static int balloc_wild_scan_group(struct balloc_allocation_context *bac,
				  struct m0_balloc_group_info *grp,
				  enum m0_balloc_allocation_flag alloc_flag)
{
	struct m0_list  *list;
	m0_bcount_t	 free;
	struct m0_ext	*ex;
	struct m0_lext	*le;
	int		 rc;
	int              end_of_group = 0;
	M0_ENTRY();

#ifdef __SPARE_SPACE__
	free = is_spare(bac->bac_flags) ? group_spare_freeblocks_get(grp) :
		group_freeblocks_get(grp);
	list = is_spare(alloc_flag) ? &grp->bgi_spare.bzp_extents :
		 &grp->bgi_normal.bzp_extents;
#else
	free = group_freeblocks_get(grp);
	list = &grp->bgi_normal.bzp_extents;
#endif

	/**
	 * Check to detect the block allocation request which came earlier
	 * for criteria 1 on same group and come again for criteria 2 with
	 * already set best extent.
	 * In criteria 1 bac_status not marked to M0_BALLOC_AC_FOUND
	 * because bac_found < M0_BALLOC_DEFAULT_MIN_TO_SCAN
	 * Now with criteria 2 on same group with already set best extent may
	 * not part of extent in free extent list because another requests
	 * may have updated extents in free list.
	 * Reset best extent by detecting this case so that it
	 * will find correct best extent, also set end_of_group flag so
	 * balloc_check_limits() could call balloc_use_best_found() to set
	 * final extent from best extent.
	 */
	if (bac->bac_found != 0) {
		m0_bindex_t group = balloc_bn2gn(bac->bac_best.e_start,
						 bac->bac_ctxt);
		if (group == grp->bgi_groupno) {
			end_of_group = 1;
			M0_SET0(&bac->bac_best);
			m0_ext_init(&bac->bac_best);
		}
	}

	M0_LOG(M0_DEBUG, "Wild scanning at group %llu: freeblocks=%llu",
		(unsigned long long)grp->bgi_groupno,
		(unsigned long long)free);

	m0_list_for_each_entry(list, le, struct m0_lext, le_link) {
		ex = &le->le_ext;
		if (m0_ext_length(ex) > free) {
			M0_LOG(M0_WARN, "corrupt group=%llu "
				"ex=[0x%08llx:0x%08llx)",
				(unsigned long long)grp->bgi_groupno,
				(unsigned long long)ex->e_start,
				(unsigned long long)ex->e_end);
			return M0_RC(-EINVAL);
		}
		balloc_measure_extent(bac, grp, alloc_flag, ex, end_of_group);

		free -= m0_ext_length(ex);
		if (free == 0 || bac->bac_status != M0_BALLOC_AC_CONTINUE)
			return M0_RC(0);
	}

	rc = balloc_check_limits(bac, grp, 1, alloc_flag);
	return M0_RC(rc);
}

/*
 * TRY to use the best result found in previous iteration.
 * The best result may be already used by others who are more lucky.
 * Group lock should be taken again.
 */
static int balloc_try_best_found(struct balloc_allocation_context *bac,
				 enum m0_balloc_allocation_flag alloc_flag)
{
	struct m0_ext		    *best  = &bac->bac_best;
	m0_bindex_t		     group = balloc_bn2gn(best->e_start,
							  bac->bac_ctxt);
	struct m0_balloc_group_info *grp = m0_balloc_gn2info(bac->bac_ctxt,
							     group);
	struct m0_ext		    *ex;
	struct m0_ext		    *cur = NULL;
	struct m0_lext		    *le;
	struct m0_list              *list;
	int			     rc = -ENOENT;

	M0_ENTRY();

	m0_balloc_lock_group(grp);

	if (is_spare(alloc_flag) && group_spare_freeblocks_get(grp) <
		 m0_ext_length(best))
		goto out;
	if (is_normal(alloc_flag) && group_freeblocks_get(grp) <
		 m0_ext_length(best))
		goto out;

	rc = m0_balloc_load_extents(bac->bac_ctxt, grp);
	if (rc != 0)
		goto out;

	rc = -ENOENT;
	list = is_spare(alloc_flag) ? group_spare_ext(grp) :
		 group_normal_ext(grp);
	m0_list_for_each_entry(list, le, struct m0_lext, le_link) {
		ex = &le->le_ext;
		if (m0_ext_equal(ex, best)) {
			rc = balloc_use_best_found(bac,
						   zone_start_get(grp,
								  alloc_flag));
			break;
		} else if (ex->e_start > best->e_start)
			goto out;
	}

	/* update db according to the allocation result */
	if (rc == 0 && bac->bac_status == M0_BALLOC_AC_FOUND) {
		if (m0_ext_length(&bac->bac_goal) < m0_ext_length(best))
			balloc_new_preallocation(bac);

		balloc_debug_dump_extent(__func__, &bac->bac_final);
		M0_ASSERT(is_extent_free(grp, &bac->bac_final, alloc_flag,
					 &cur));
		rc = balloc_alloc_db_update(bac->bac_ctxt, bac->bac_tx, grp,
					    &bac->bac_final, alloc_flag, cur);
	}
out:
	m0_balloc_unlock_group(grp);
	M0_LEAVE();
	return M0_RC(rc);
}

static void
balloc_alloc_credit(const struct m0_ad_balloc *balroom, int nr,
			  struct m0_be_tx_credit *accum)
{
	const struct m0_balloc	*bal = b2m0(balroom);

	M0_ENTRY("cred=[%lu:%lu] nr=%d",
		(unsigned long)accum->tc_reg_nr,
		(unsigned long)accum->tc_reg_size, nr);
	balloc_db_update_credit(bal, nr, accum);
	M0_LEAVE("cred=[%lu:%lu]",
		(unsigned long)accum->tc_reg_nr,
		(unsigned long)accum->tc_reg_size);
}

static bool is_free_space_unavailable(struct m0_balloc_group_info *grp,
				       uint64_t alloc_flags)
{
	return
#ifdef __SPARE_SPACE__
		is_any(alloc_flags) ? (group_freeblocks_get(grp) == 0 &&
				      group_spare_freeblocks_get(grp) == 0) :
		is_spare(alloc_flags) ? group_spare_freeblocks_get(grp) == 0 :
#endif
			group_freeblocks_get(grp) == 0;
}

static int
balloc_regular_allocator(struct balloc_allocation_context *bac)
{
	m0_bcount_t ngroups;
	m0_bcount_t group;
	m0_bcount_t i;
	m0_bcount_t len;
	int         cr;
	int         rc = 0;

	ngroups = bac->bac_ctxt->cb_sb.bsb_groupcount;
	len = m0_ext_length(&bac->bac_goal);

	M0_ENTRY("goal=0x%lx len=%d",
		(unsigned long)bac->bac_goal.e_start, (int)len);

#if 0
	/* first, try the goal */
	rc = balloc_find_by_goal(bac);
	if (rc != 0 || bac->bac_status == M0_BALLOC_AC_FOUND ||
	    (bac->bac_flags & M0_BALLOC_HINT_GOAL_ONLY)) {
		M0_LEAVE();
		return M0_RC(rc);
	}
#endif

	bac->bac_order2 = 0;
	/*
	 * We search using buddy data only if the order of the request
	 * is greater than equal to the threshold.
	 */
	if (m0_is_po2(len))
		bac->bac_order2 = ffs(len) - 1;

	cr = bac->bac_order2 ? 0 : 1;
	/*
	 * cr == 0 try to get exact allocation,
	 * cr == 1 striped allocation. Not implemented currently.
	 * cr == 2 try to get anything
	 */
repeat:
	for (;cr < 3 && bac->bac_status == M0_BALLOC_AC_CONTINUE; cr++) {
		M0_LOG(M0_DEBUG, "cr=%d", cr);
		bac->bac_criteria = cr;
		/*
		 * searching for the right group start
		 * from the goal value specified
		 */
		group = balloc_bn2gn(bac->bac_goal.e_start, bac->bac_ctxt);

		for (i = 0; i < ngroups; group++, i++) {
			struct m0_balloc_group_info *grp;

			if (group >= ngroups)
				group = 0;

			grp = m0_balloc_gn2info(bac->bac_ctxt, group);
			// m0_balloc_debug_dump_group("searching group ...",
			//			 grp);

			rc = m0_balloc_trylock_group(grp);
			if (rc != 0) {
				M0_LOG(M0_DEBUG, "grp=%d is busy", (int)group);
				/* This group is under processing by others. */
				continue;
			}

			/* quick check to skip empty groups */
			if (is_free_space_unavailable(grp, bac->bac_flags)) {
				M0_LOG(M0_DEBUG, "grp=%d is empty", (int)group);
				m0_balloc_unlock_group(grp);
				continue;
			}

			rc = m0_balloc_load_extents(bac->bac_ctxt, grp);
			if (rc != 0) {
				m0_balloc_unlock_group(grp);
				goto out;
			}
			if (!balloc_is_good_group(bac, grp)) {
				m0_balloc_unlock_group(grp);
				continue;
			}
			bac->bac_scanned++;

			/* m0_balloc_debug_dump_group_extent("AAA", grp); */
			rc = 1;
#ifdef __SPARE_SPACE__
			if (is_spare(bac->bac_flags))
				rc = allocate_blocks(cr, bac, grp, len,
						     M0_BALLOC_SPARE_ZONE);
#endif
			if (rc != 0 && (is_any(bac->bac_flags) ||
			    is_normal(bac->bac_flags)))
				rc = allocate_blocks(cr, bac, grp, len,
						     M0_BALLOC_NORMAL_ZONE);
			m0_balloc_unlock_group(grp);

			if (bac->bac_status != M0_BALLOC_AC_CONTINUE)
				break;
		}
	}

	if (m0_ext_length(&bac->bac_best) > 0 &&
	    (bac->bac_status != M0_BALLOC_AC_FOUND) &&
	    !(bac->bac_flags & M0_BALLOC_HINT_FIRST)) {
		/*
		 * We've been searching too long. Let's try to allocate
		 * the best chunk we've found so far
		 */

		rc = 1;
#ifdef __SPARE_SPACE__
		if (is_spare(bac->bac_flags))
			rc = balloc_try_best_found(bac, M0_BALLOC_SPARE_ZONE);
#endif
		if (rc != 0 &&
		    (is_any(bac->bac_flags) || is_normal(bac->bac_flags))) {
			rc = balloc_try_best_found(bac,
						   M0_BALLOC_NORMAL_ZONE);
		}
		if (rc || bac->bac_status != M0_BALLOC_AC_FOUND) {
			/*
			 * Someone more lucky has already allocated it.
			 * Let's just take first found block(s).
			 */
			bac->bac_status = M0_BALLOC_AC_CONTINUE;
			M0_SET0(&bac->bac_best);
			bac->bac_flags |= M0_BALLOC_HINT_FIRST;
			cr = 2;
			M0_LOG(M0_DEBUG, "Let's repeat..........");
			goto repeat;
		}
	}
out:
	M0_LEAVE();
	if (rc == 0 && bac->bac_status != M0_BALLOC_AC_FOUND) {
		rc = -ENOSPC;
		M0_LOG(M0_ERROR, "Balloc running out of space");
	}
	return M0_RC(rc);
}

static int allocate_blocks(int cr, struct balloc_allocation_context *bac,
			   struct m0_balloc_group_info *grp, m0_bcount_t len,
			   enum m0_balloc_allocation_flag alloc_type)
{
	int	       rc;
	struct m0_ext *cur = NULL;

	M0_PRE(!is_any(alloc_type));
	M0_PRE(is_spare(alloc_type) || is_normal(alloc_type));

	if (cr == 0 ||
	    (cr == 1 && len == bac->bac_ctxt->cb_sb.bsb_stripe_size))
		rc = balloc_simple_scan_group(bac, grp, alloc_type);
	else
		rc = balloc_wild_scan_group(bac, grp, alloc_type);

	/* update db according to the allocation result */
	if (rc == 0 && bac->bac_status == M0_BALLOC_AC_FOUND) {
		if (len < m0_ext_length(&bac->bac_best))
			balloc_new_preallocation(bac);
		M0_ASSERT(is_extent_free(grp, &bac->bac_final, alloc_type,
					 &cur));
		rc = balloc_alloc_db_update(bac->bac_ctxt, bac->bac_tx, grp,
					    &bac->bac_final, alloc_type, cur);
	}
	return rc;
}

/**
   Allocate multiple blocks for some object.

   This routine will search suitable free space, and determine where to allocate
   from.  Caller can provide some hint (goal). Pre-allocation is used depending
   the character of the I/O sequences, and the current state of the active I/O.
   When trying allocate blocks from free space, we will allocate from the
   best suitable chunks, which are represented as buddy.

   Allocation will first try to use pre-allocation if it exists. Pre-allocation
   can be per-object, or group based.

   This routine will first check the group description to see if enough free
   space is available, and if largest contiguous chunk satisfy the request. This
   checking will be done group by group, until allocation succeeded or failed.
   If failed, the largest available contiguous chunk size is returned, and the
   caller can decide whether to use a smaller request.

   While searching free space from group to group, the free space extent will be
   loaded into cache.  We cache as much free space extent up to some
   specified memory limitation.	 This is a configurable parameter, or default
   value will be chosen based on system memory.

   @param ctx balloc operation context environment.
   @param req allocate request which includes all parameters.
   @return 0 means success.
	   Result allocated blocks are again stored in "req":
	   result physical block number = bar_physical,
	   result count of blocks = bar_len.
	   Upon failure, non-zero error number is returned.
 */
static
int balloc_allocate_internal(struct m0_balloc *ctx,
			     struct m0_be_tx *tx,
			     struct m0_balloc_allocate_req *req)
{
	struct balloc_allocation_context bac;
	int                              rc;

	M0_ENTRY();

	while (req->bar_len &&
	       !balloc_got_freespace(ctx, req->bar_len, req->bar_flags)) {
		req->bar_len >>= 1;
	}
	rc = req->bar_len == 0 ? -ENOSPC : 0;
	if (rc != 0)
		goto out;

	balloc_init_ac(&bac, ctx, tx, req);

	/* Step 1. query the pre-allocation */
	if (!balloc_use_prealloc(&bac)) {
		/* we did not find suitable free space in prealloc. */

		balloc_normalize_request(&bac);

		/* Step 2. Iterate over groups */
		rc = balloc_regular_allocator(&bac);
		if (rc == 0 && bac.bac_status == M0_BALLOC_AC_FOUND) {
			/* store the result in req and they will be returned */
			req->bar_result = bac.bac_final;
		}
	}
out:
	return M0_RC(rc);
}

static void balloc_free_credit(const struct m0_ad_balloc *balroom, int nr,
				     struct m0_be_tx_credit *accum)
{
	balloc_db_update_credit(b2m0(balroom), nr, accum);
}

/**
   Free multiple blocks owned by some object to free space.

   @param ctx balloc operation context environment.
   @param req block free request which includes all parameters.
   @return 0 means success. Upon failure, non-zero error number is returned.
 */
static int balloc_free_internal(struct m0_balloc *ctx,
				struct m0_be_tx  *tx,
				struct m0_balloc_free_req *req)
{
	struct m0_ext                 fex;
	struct m0_balloc_group_info  *grp;
	struct m0_balloc_super_block *sb = &ctx->cb_sb;
	m0_bcount_t                   group;
	m0_bindex_t                   start;
	m0_bindex_t                   off;
	m0_bcount_t                   len;
	m0_bcount_t                   step;
	int                           rc = 0;
	uint64_t                      alloc_flag;

	M0_ENTRY();

	start = req->bfr_physical;
	len = req->bfr_len;

	M0_LOG(M0_DEBUG, "bal=%p start=0x%llx len=0x%llx start_group=%llu "
		"end_group=%llu group_count=%llu", ctx,
		(unsigned long long)start,
		(unsigned long long)len,
		(unsigned long long)balloc_bn2gn(start, ctx),
		(unsigned long long)balloc_bn2gn(start + len, ctx),
		(unsigned long long)sb->bsb_groupcount);
	group = balloc_bn2gn(start + len, ctx);
	if (group > sb->bsb_groupcount)
		return M0_ERR(-EINVAL);

	while (rc == 0 && len > 0) {
		group = balloc_bn2gn(start, ctx);
		grp = m0_balloc_gn2info(ctx, group);
		m0_balloc_lock_group(grp);

		rc = m0_balloc_load_extents(ctx, grp);
		if (rc != 0) {
			m0_balloc_unlock_group(grp);
			goto out;
		}

		m0_balloc_debug_dump_group_extent("FFF", grp);
		off = start & (sb->bsb_groupsize - 1);
		step = (off + len > sb->bsb_groupsize) ?
			sb->bsb_groupsize - off : len;

		fex.e_start = start;
		fex.e_end   = start + step;
		m0_ext_init(&fex);
		alloc_flag = ext_range_locate(&fex, grp);
		/*
		 * For an extent spanning both zones first release
		 * non-spare part, and then release spare part.
		 */
		if (is_any(alloc_flag)) {
			fex.e_end  = grp->bgi_spare.bzp_range.e_start;
			rc = balloc_free_db_update(ctx, tx, grp, &fex,
						   M0_BALLOC_NORMAL_ZONE);
			if (rc != 0)
				break;
			fex.e_start = zone_start_get(grp,
						     M0_BALLOC_SPARE_ZONE);
			fex.e_end = start + step;
			alloc_flag = M0_BALLOC_SPARE_ZONE;
		}
		if (rc == 0)
			rc = balloc_free_db_update(ctx, tx, grp, &fex,
						   alloc_flag);
		m0_balloc_unlock_group(grp);
		start += step;
		len -= step;
	}

out:
	M0_LEAVE();
	return M0_RC(rc);
}

static uint64_t ext_range_locate(struct m0_ext *ip_ext,
				 struct m0_balloc_group_info *grp)
{
	if (m0_ext_is_partof(&grp->bgi_spare.bzp_range, ip_ext))
		return M0_BALLOC_SPARE_ZONE;
	else if (m0_ext_is_partof(&grp->bgi_normal.bzp_range, ip_ext))
		return M0_BALLOC_NORMAL_ZONE;
	else
		return M0_BALLOC_SPARE_ZONE + M0_BALLOC_NORMAL_ZONE;
}
/**
   Discard the pre-allocation for object.

   @param ctx balloc operation context environment.
   @param req discard request which includes all parameters.
   @return 0 means success. Upon failure, non-zero error number is returned.
 */
__attribute__((unused))
static int balloc_discard_prealloc(struct m0_balloc *ctx,
				   struct m0_balloc_discard_req *req)
{
	return 0;
}

/**
   modify the allocation status forcibly.

   This function may be used by fsck or some other tools to modify the
   allocation status directly.

   @param ctx balloc operation context environment.
   @param alloc true to make the specifed extent as allocated, otherwise make
	  the extent as free.
   @param ext user supplied extent to check.
   @return 0 means success. Upon failure, non-zero error number is returned.
 */
__attribute__((unused))
static int balloc_enforce(struct m0_balloc *ctx, bool alloc,
			  struct m0_ext *ext)
{
	return 0;
}


/**
   Query the allocation status.

   @param ctx balloc operation context environment.
   @param ext user supplied extent to check.
   @return true if the extent is fully allocated. Otherwise, false is returned.
 */
__attribute__((unused))
static bool balloc_query(struct m0_balloc *ctx, struct m0_ext *ext)
{

	return false;
}

/**
 * Marks given extent as used in the allocator.
 *
 * @param ext requested extent for block reservation.
 * @param alloc_zone allocation zone.
 * @return 0 on successful reservation of the extent.
 *	   -ENOSPC on not enough space in superblock/group/fragment.
 *         -EEXIST on overlap extent.
 *         -errno on other failures, non-zero error number is returned.
 */
static int balloc_reserve_extent(struct m0_ad_balloc *ballroom,
				 struct m0_be_tx *tx, struct m0_ext *ext,
				 uint64_t alloc_zone)
{
	struct m0_balloc		 *ctx = b2m0(ballroom);
	m0_bcount_t             	  ext_len;
	m0_bcount_t 			  group;
	struct m0_balloc_group_info      *grp;
	struct balloc_allocation_context  bac;
	struct m0_ext	                 *cur = NULL;
	int                     	  rc;

	M0_ENTRY("bal=%p extent="EXT_F, ctx, EXT_P(ext));

	ext_len = m0_ext_length(ext);
	/* Check total free space. */
	if (!balloc_got_freespace(ctx, ext_len, alloc_zone)) {
		rc = M0_ERR(-ENOSPC);
		M0_LOG(M0_DEBUG, "Balloc out of free space");
		goto out;
	}
	/* Fetch the group for this extent. */
	group = balloc_bn2gn(ext->e_start, ctx);

	/* Get the group. */
	grp = m0_balloc_gn2info(ctx, group);

	/* Take lock of the group. */
	m0_balloc_lock_group(grp);

	/* Check if space is available in group. */
	if (is_free_space_unavailable(grp, alloc_zone)) {
		rc = M0_ERR(-ENOSPC);
		M0_LOG(M0_DEBUG, "grp=%" PRIu64 " is empty", group);
		goto out_unlock;
	}

	rc = m0_balloc_load_extents(ctx, grp);
	if (rc != 0)
		goto out_unlock;

	bac.bac_flags    = alloc_zone;
	bac.bac_criteria = 0; /* Find exact extent. */
	bac.bac_goal     = *ext;
	m0_ext_init(&bac.bac_goal);

	if (!balloc_is_good_group(&bac, grp)) {
		rc = M0_ERR(-ENOSPC);
		M0_LOG(M0_DEBUG, "grp=%" PRIu64 " is not good", group);
		goto out_unlock;
	}

	if (!is_extent_free(grp, ext, alloc_zone, &cur)) {
		rc = M0_ERR(-EEXIST);
		goto out_unlock;
	}

	rc = balloc_alloc_db_update(ctx, tx, grp, ext, alloc_zone, cur);

out_unlock:
	m0_balloc_unlock_group(grp);
out:
	return M0_RC(rc);
}

/**
 * allocate from underlying container.
 * @param count count of bytes. count will be first aligned to block boundry.
 * @param out result is stored there. space is still in bytes.
 */
static int balloc_alloc(struct m0_ad_balloc *ballroom, struct m0_dtx *tx,
			m0_bcount_t count, struct m0_ext *out,
			uint64_t alloc_zone)
{
	struct m0_balloc              *motr = b2m0(ballroom);
	struct m0_balloc_allocate_req  req;
	m0_bcount_t                    freeblocks;
	int                            rc;

	M0_ENTRY("bal=%p goal=0x%lx count=%lu", motr,
			(unsigned long)out->e_start,
			(unsigned long)count);
	M0_PRE(count > 0);

	req.bar_goal  = out->e_start; /* this also plays as the goal */
	req.bar_len   = count;
#ifdef __SPARE_SPACE__
	req.bar_flags = alloc_zone /*M0_BALLOC_HINT_DATA | M0_BALLOC_HINT_TRY_GOAL*/;
#else
	req.bar_flags = M0_BALLOC_NORMAL_ZONE;
#endif

	M0_SET0(out);

	freeblocks = motr->cb_sb.bsb_freeblocks;
	rc = balloc_allocate_internal(motr, &tx->tx_betx, &req);
	if (rc == 0) {
		if (m0_ext_is_empty(&req.bar_result)) {
			rc = -ENOENT;
		} else {
			out->e_start = req.bar_result.e_start;
			out->e_end   = req.bar_result.e_end;
			m0_ext_init(out);
			m0_mutex_lock(&motr->cb_sb_mutex.bm_u.mutex);
			motr->cb_last = out->e_end;
			m0_mutex_unlock(&motr->cb_sb_mutex.bm_u.mutex);
		}
	}
	M0_LOG(M0_DEBUG, "BAlloc=%p rc=%d freeblocks %llu -> %llu",
			 motr, rc,
			 (unsigned long long)freeblocks,
			 (unsigned long long)motr->cb_sb.bsb_freeblocks);


	return M0_RC(rc);
}

/**
 * free spaces to container.
 * @param ext the space to be freed. This space must align to block boundry.
 */
static int balloc_free(struct m0_ad_balloc *ballroom, struct m0_dtx *tx,
		       struct m0_ext *ext)
{
	struct m0_balloc          *motr = b2m0(ballroom);
	struct m0_balloc_free_req  req;
	m0_bcount_t                freeblocks;
	int                        rc;

	req.bfr_physical = ext->e_start;
	req.bfr_len	 = m0_ext_length(ext);

	freeblocks = motr->cb_sb.bsb_freeblocks;
	rc = balloc_free_internal(motr, &tx->tx_betx, &req);
	M0_LOG(M0_DEBUG, "BFree=%p rc=%d freeblocks %llu -> %llu",
			 motr, rc,
			 (unsigned long long)freeblocks,
			 (unsigned long long)motr->cb_sb.bsb_freeblocks);
	if (rc == 0)
		motr->cb_last = ext->e_start;

	return M0_RC(rc);
}

static int balloc_init(struct m0_ad_balloc *ballroom, struct m0_be_seg *db,
		       uint32_t bshift, m0_bcount_t container_size,
		       m0_bcount_t blocks_per_group,
		       m0_bcount_t spare_blocks_per_group)
{
	struct m0_balloc   *motr;
	struct m0_sm_group *grp = m0_locality0_get()->lo_grp;   /* XXX */
	int                 rc;
	M0_ENTRY();

	motr = b2m0(ballroom);

	m0_sm_group_lock(grp);
	rc = balloc_init_internal(motr, db, grp, bshift, container_size,
				  blocks_per_group, spare_blocks_per_group);
	m0_sm_group_unlock(grp);

	return M0_RC(rc);
}

static void balloc_fini(struct m0_ad_balloc *ballroom)
{
	struct m0_balloc *motr = b2m0(ballroom);

	M0_ENTRY();

	balloc_fini_internal(motr);

	M0_LEAVE();
}

static const struct m0_ad_balloc_ops balloc_ops = {
	.bo_init	   = balloc_init,
	.bo_fini	   = balloc_fini,
	.bo_alloc	   = balloc_alloc,
	.bo_free	   = balloc_free,
	.bo_alloc_credit   = balloc_alloc_credit,
	.bo_free_credit    = balloc_free_credit,
	.bo_reserve_extent = balloc_reserve_extent,
};

static int balloc_trees_create(struct m0_balloc    *bal,
			       struct m0_be_seg    *seg,
			       struct m0_be_tx     *tx,
			       const struct m0_fid *bfid)
{
	int                        rc;
	struct m0_btree_type       bt;
	struct m0_btree_op         b_op = {};
	struct m0_fid              fid;
	struct m0_btree_rec_key_op ge_keycmp = {.rko_keycmp = ge_tree_cmp, };

	M0_ALLOC_PTR(bal->cb_db_group_extents);
	if (bal->cb_db_group_extents == NULL)
		return M0_ERR(-ENOMEM);

	M0_ALLOC_PTR(bal->cb_db_group_desc);
	if (bal->cb_db_group_desc == NULL) {
		m0_free0(&bal->cb_db_group_extents);
		return M0_ERR(-ENOMEM);
	}

	bt = (struct m0_btree_type){.tt_id = M0_BT_BALLOC_GROUP_EXTENTS,
		.ksize = M0_MEMBER_SIZE(struct m0_ext, e_start),
		.vsize = M0_MEMBER_SIZE(struct m0_ext, e_end),
	};
	fid = M0_FID_TINIT('b', M0_BT_BALLOC_GROUP_EXTENTS, bfid->f_key);
	rc = M0_BTREE_OP_SYNC_WITH_RC(&b_op,
				      m0_btree_create(bal->cb_ge_node,
						      sizeof bal->cb_ge_node,
						      &bt, &b_op,
						      bal->cb_db_group_extents,
						      seg, &fid, tx,
						      &ge_keycmp));
	if (rc != 0) {
		m0_free0(&bal->cb_db_group_extents);
		m0_free0(&bal->cb_db_group_desc);
		return M0_ERR(rc);
	}

	bt = (struct m0_btree_type){.tt_id = M0_BT_BALLOC_GROUP_DESC,
		.ksize = M0_MEMBER_SIZE(struct m0_balloc_group_desc,
					bgd_groupno),
		.vsize = sizeof(struct m0_balloc_group_desc),
	};
	fid = M0_FID_TINIT('b', M0_BT_BALLOC_GROUP_DESC, bfid->f_key);
	rc = M0_BTREE_OP_SYNC_WITH_RC(&b_op,
				      m0_btree_create(bal->cb_gd_node,
						      sizeof bal->cb_gd_node,
						      &bt, &b_op,
						      bal->cb_db_group_desc,
						      seg, &fid, tx, NULL));

	if (rc != 0) {
		M0_BTREE_OP_SYNC_WITH_RC(&b_op,
				m0_btree_destroy(bal->cb_db_group_extents,
						 &b_op, tx));
		m0_free0(&bal->cb_db_group_extents);
		m0_free0(&bal->cb_db_group_desc);
	}

	return M0_RC(rc);
}

M0_INTERNAL void m0_balloc_init(struct m0_balloc *cb)
{
	cb->cb_ballroom.ab_ops = &balloc_ops;
}

M0_INTERNAL int m0_balloc_create(uint64_t              cid,
				 struct m0_be_seg     *seg,
				 struct m0_sm_group   *grp,
				 struct m0_balloc    **out,
				 const struct m0_fid  *fid)
{
	struct m0_balloc       *cb;
	struct m0_be_tx         tx    = {};
	struct m0_be_tx_credit  cred  = {};
	int                     rc;
	struct m0_btree_type    bt;

	M0_PRE(seg != NULL);
	M0_PRE(out != NULL);

	m0_be_tx_init(&tx, 0, seg->bs_domain,
		      grp, NULL, NULL, NULL, NULL);
	M0_BE_ALLOC_CREDIT_PTR(cb, seg, &cred);
	m0_be_tx_credit_add(&cred, &M0_BE_TX_CREDIT_PTR(cb));

	bt = (struct m0_btree_type){.tt_id = M0_BT_BALLOC_GROUP_EXTENTS,
		.ksize = M0_MEMBER_SIZE(struct m0_ext, e_start),
		.vsize = M0_MEMBER_SIZE(struct m0_ext, e_end),
	};
	m0_btree_create_credit(&bt, &cred, 1);

	bt = (struct m0_btree_type){.tt_id = M0_BT_BALLOC_GROUP_DESC,
		.ksize = M0_MEMBER_SIZE(struct m0_balloc_group_desc,
					bgd_groupno),
		.vsize = sizeof(struct m0_balloc_group_desc),
	};
	m0_btree_create_credit(&bt, &cred, 1);

	m0_be_tx_prep(&tx, &cred);
	rc = m0_be_tx_open_sync(&tx);

	if (rc == 0) {
		M0_BE_ALLOC_ALIGN_PTR_SYNC(cb, 10, seg, &tx);
		if (cb == NULL) {
			rc = -ENOMEM;
		} else {
			cb->cb_container_id = cid;

			balloc_format_init(cb);
			M0_BE_TX_CAPTURE_PTR(seg, &tx, cb);
			rc = balloc_trees_create(cb, seg, &tx, fid);
			if (rc == 0)
				*out = cb;
		}
		m0_be_tx_close_sync(&tx);
	}
	m0_be_tx_fini(&tx);

	if (rc == 0)
		m0_balloc_init(*out);

	return M0_RC(rc);
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
