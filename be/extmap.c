/* -*- C -*- */
/*
 * Copyright (c) 2013-2021 Seagate Technology LLC and/or its Affiliates
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

#include "be/extmap.h"
#include "lib/vec.h"
#include "lib/errno.h"
#include "lib/arith.h"      /* M0_3WAY */
#include "lib/misc.h"
#include "lib/finject.h"
#include "lib/memory.h"
#include "lib/cksum_utils.h"
#include "lib/cksum.h"
#include "format/format.h"  /* m0_format_header_pack */

/* max data units that can be sent in a request */
#define MAX_DUS 8

/**
   @addtogroup extmap

   <b>Extent map implementation details.</b>

   @see extmap_internal.h

   Few notes:

   @li m0_be_emap_cursor::ec_seg is "external" representation of iteration
   state. m0_be_emap_cursor::ec_key and m0_be_emap_cursor::ec_rec are "internal";

   @li after internal state has changed it is "opened" by emap_it_open() which
   updates external stats to match changes;

   @li similarly when external state has changed, it is "packed" into internal
   one by emap_it_pack().

   @li m0_be_emap_cursor::ec_pair is descriptor of buffers for data-base
   operations. Buffers are aliased to m0_be_emap_cursor::ec_key and
   m0_be_emap_cursor::ec_rec by emap_it_init().

   @li be_emap_invariant() checks implementation invariant: an extent map is an
   collection of segments with non-empty extents forming the partition of the
   name-space and ordered by their starting offsets. This creates a separate
   cursor within the same transaction as the cursor it is called against.

   @li A segment ([A, B), V) is stored as a record (A, V) with a key (prefix,
   B). Note, that the _high_ extent end is used as a key. This way,
   m0_be_btree_cursor_get() can be used to position a cursor on a segment
   containing a
   given offset. Also note, that there is some redundancy in the persistent
   state: two consecutive segments ([A, B), V) and ([B, C), U) are stored as
   records (A, V) and (B, U) with keys (prefix, B) and (prefix, C)
   respectively. B is stored twice. Generally, starting offset of a segment
   extent can always be deduced from the key of previous segment (and for the
   first segment it's 0), so some slight economy of storage could be achieved at
   the expense of increased complexity and occasional extra storage traffic.

   @note be_emap_invariant() is potentially expensive. Consider turning it off
   conditionally.

   @{
 */

/*
static void key_print(const struct m0_be_emap_key *k)
{
	printf(U128X_F":%08lx", U128_P(&k->ek_prefix), k->ek_offset);
}
*/
static int be_emap_cmp(const void *key0, const void *key1);
static int emap_it_pack(struct m0_be_emap_cursor *it,
			int (*btree_func)(struct m0_btree     *btree,
					   struct m0_be_tx    *tx,
					   struct m0_btree_op *op,
				     const struct m0_buf      *key,
				     const struct m0_buf      *val),
			struct m0_be_tx *tx);
static bool emap_it_prefix_ok(const struct m0_be_emap_cursor *it);
static int emap_it_open(struct m0_be_emap_cursor *it, int prev_rc);
static void emap_it_init(struct m0_be_emap_cursor *it,
			 const struct m0_uint128  *prefix,
			 m0_bindex_t               offset,
			 struct m0_be_emap        *map);
static void be_emap_close(struct m0_be_emap_cursor *it);
static int emap_it_get(struct m0_be_emap_cursor *it);
static int be_emap_lookup(struct m0_be_emap        *map,
		    const struct m0_uint128        *prefix,
		          m0_bindex_t               offset,
			  struct m0_be_emap_cursor *it);
static int be_emap_next(struct m0_be_emap_cursor *it);
static int be_emap_prev(struct m0_be_emap_cursor *it);
static bool be_emap_invariant(struct m0_be_emap_cursor *it);
static int emap_extent_update(struct m0_be_emap_cursor *it,
			      struct m0_be_tx          *tx,
			const struct m0_be_emap_seg    *es);
static int be_emap_split(struct m0_be_emap_cursor *it,
			 struct m0_be_tx          *tx,
			 struct m0_indexvec       *vec,
			 m0_bindex_t               scan,
			 struct m0_buf            *cksum);
static bool be_emap_caret_invariant(const struct m0_be_emap_caret *car);

static struct m0_rwlock *emap_rwlock(struct m0_be_emap *emap)
{
	return &emap->em_lock.bl_u.rwlock;
}

static void emap_dump(struct m0_be_emap_cursor *it)
{
	int                       i;
	int                       rc;
	struct m0_be_emap_cursor *scan;
	struct m0_uint128        *prefix = &it->ec_key.ek_prefix;
	struct m0_be_emap_seg    *seg;

	M0_ALLOC_PTR(scan);
	if (scan == NULL)
		return;
	seg = &scan->ec_seg;

	m0_rwlock_read_lock(emap_rwlock(it->ec_map));
	rc = be_emap_lookup(it->ec_map, prefix, 0, scan);
	M0_ASSERT(rc == 0);

	M0_LOG(M0_DEBUG, U128X_F":", U128_P(prefix));
	for (i = 0; ; ++i) {
		M0_LOG(M0_DEBUG, "\t%5.5i %16lx .. %16lx: %16lx %10lx", i,
		       (unsigned long)seg->ee_ext.e_start,
		       (unsigned long)seg->ee_ext.e_end,
		       (unsigned long)m0_ext_length(&seg->ee_ext),
		       (unsigned long)seg->ee_val);
		if (m0_be_emap_ext_is_last(&seg->ee_ext))
			break;
		rc = be_emap_next(scan);
		M0_ASSERT(rc == 0);
	}
	be_emap_close(scan);
	m0_rwlock_read_unlock(emap_rwlock(it->ec_map));

	m0_free(scan);
}

static void emap_key_init(struct m0_be_emap_key *key)
{
	m0_format_header_pack(&key->ek_header, &(struct m0_format_tag){
		.ot_version = M0_BE_EMAP_KEY_FORMAT_VERSION,
		.ot_type    = M0_FORMAT_TYPE_BE_EMAP_KEY,
		.ot_footer_offset = offsetof(struct m0_be_emap_key, ek_footer)
	});
	m0_format_footer_update(key);
}

static void emap_rec_init(struct m0_be_emap_rec *rec)
{
	m0_format_header_pack(&rec->er_header, &(struct m0_format_tag){
		        .ot_version = M0_BE_EMAP_REC_FORMAT_VERSION,
		        .ot_type    = M0_FORMAT_TYPE_BE_EMAP_REC,
			/** cksum of size cksum_nob will be present just before
			 *  footer, update the same in emap header.
			 */
		        .ot_footer_offset = offsetof(struct m0_be_emap_rec,
			                             er_footer)
			                    + rec->er_cksum_nob
	});
	m0_format_footer_update(rec);
}

M0_INTERNAL int m0_be_emap_dump(struct m0_be_emap *map)
{
	struct m0_be_emap_cursor *it;
	struct m0_be_emap_seg *seg = NULL;
	m0_bcount_t            nr_segs = 0;
	m0_bcount_t            nr_cobs = 0;
	int                    rc = 0;
	struct m0_uint128      prefix;

	M0_ALLOC_PTR(it);
	if (it == NULL)
		return M0_ERR(-ENOMEM);

	m0_rwlock_read_lock(emap_rwlock(map));

	prefix = M0_UINT128(0, 0);
	rc = be_emap_lookup(map, &prefix, 0, it);
	if (rc == -ESRCH) {
		prefix = it->ec_seg.ee_pre;
		rc = be_emap_lookup(map, &prefix, 0, it);
	}
	if (rc != 0)
		goto err;

	do {
		seg = m0_be_emap_seg_get(it);
		M0_ASSERT(m0_ext_is_valid(&seg->ee_ext) &&
			  !m0_ext_is_empty(&seg->ee_ext));
		++nr_segs;
		if (m0_be_emap_ext_is_last(&seg->ee_ext))
			++nr_cobs;
		rc = be_emap_next(it);
	} while (rc == 0 || rc == -ESRCH);

	be_emap_close(it);
 err:
	m0_rwlock_read_unlock(emap_rwlock(map));

	if (seg != NULL)
		M0_LOG(M0_DEBUG, "%p %"PRIx64" %u %lu", map,
		       seg->ee_pre.u_hi, (unsigned)nr_cobs,
		       (unsigned long)nr_segs);

	m0_free(it);

	if (rc == -ENOENT)
		return M0_RC(0);

	return M0_ERR(rc);
}

static int be_emap_delete_wrapper(struct m0_btree *btree, struct m0_be_tx *tx,
			   struct m0_btree_op *op, const struct m0_buf *key,
			   const struct m0_buf *val)
{
	void                *k_ptr = key->b_addr;
	m0_bcount_t          ksize = key->b_nob;
	int                  rc;
	struct m0_btree_key  r_key = {
		.k_data  = M0_BUFVEC_INIT_BUF(&k_ptr, &ksize),
		};

	rc = M0_BTREE_OP_SYNC_WITH_RC(
				op,
				m0_btree_del(btree, &r_key, NULL, op, tx));
	return rc;
}

static int be_emap_insert_callback(struct m0_btree_cb  *cb,
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

static int be_emap_insert_wrapper(struct m0_btree *btree, struct m0_be_tx *tx,
			  struct m0_btree_op *op, const struct m0_buf *key,
			  const struct m0_buf *val)
{
	void                *k_ptr = key->b_addr;
	void                *v_ptr = val->b_addr;
	m0_bcount_t          ksize = key->b_nob;
	m0_bcount_t          vsize = val->b_nob;
	int                  rc;

	struct m0_btree_rec  rec   = {
		.r_key.k_data = M0_BUFVEC_INIT_BUF(&k_ptr, &ksize),
		.r_val        = M0_BUFVEC_INIT_BUF(&v_ptr, &vsize),
		};
	struct m0_btree_cb   put_cb = {
		.c_act = be_emap_insert_callback,
		.c_datum = &rec,
		};
	rc = M0_BTREE_OP_SYNC_WITH_RC(
			op,
			m0_btree_put(btree, &rec, &put_cb, op, tx));
	return rc;
}

static int be_emap_update_callback(struct m0_btree_cb  *cb,
			      struct m0_btree_rec *rec)
{
	struct m0_btree_rec     *datum = cb->c_datum;

	/** Only update the Value to the location indicated in rec. */
	m0_bufvec_copy(&rec->r_val, &datum->r_val,
		       m0_vec_count(&datum->r_val.ov_vec));
	return 0;
}

static int be_emap_update_wrapper(struct m0_btree *btree, struct m0_be_tx *tx,
			  struct m0_btree_op *op, const struct m0_buf *key,
			  const struct m0_buf *val)
{
	void                *k_ptr = key->b_addr;
	void                *v_ptr = val->b_addr;
	m0_bcount_t          ksize = key->b_nob;
	m0_bcount_t          vsize = val->b_nob;
	int                  rc;

	struct m0_btree_rec  rec   = {
		.r_key.k_data = M0_BUFVEC_INIT_BUF(&k_ptr, &ksize),
		.r_val        = M0_BUFVEC_INIT_BUF( &v_ptr, &vsize),
		};
	struct m0_btree_cb   update_cb = {
		.c_act = be_emap_update_callback,
		.c_datum = &rec,
		};
 	rc = M0_BTREE_OP_SYNC_WITH_RC(
			op,
			m0_btree_update(btree, &rec, &update_cb, op, tx));
	return rc;
}

static void be_emap_init(struct m0_be_emap *map, struct m0_be_seg *db)
{
	m0_format_header_pack(&map->em_header, &(struct m0_format_tag){
		.ot_version = M0_BE_EMAP_FORMAT_VERSION,
		.ot_type    = M0_FORMAT_TYPE_BE_EMAP,
		.ot_footer_offset = offsetof(struct m0_be_emap, em_footer)
	});
	m0_rwlock_init(emap_rwlock(map));
	m0_buf_init(&map->em_key_buf, &map->em_key, sizeof map->em_key);
	m0_buf_init(&map->em_val_buf, &map->em_rec, sizeof map->em_rec);
	emap_key_init(&map->em_key);
	emap_rec_init(&map->em_rec);
	map->em_seg = db;
	map->em_version = 0;
	m0_format_footer_update(map);
}

M0_INTERNAL void
m0_be_emap_init(struct m0_be_emap *map, struct m0_be_seg *db)
{
	struct m0_btree_op         b_op = {};
	int                        rc;
	struct m0_btree_rec_key_op keycmp;
	be_emap_init(map, db);

	keycmp.rko_keycmp = be_emap_cmp;
	M0_ALLOC_PTR(map->em_mapping);
	if (map->em_mapping == NULL)
		M0_ASSERT(0);

	rc = M0_BTREE_OP_SYNC_WITH_RC(&b_op,
					m0_btree_open(&map->em_mp_node,
						sizeof map->em_mp_node,
						map->em_mapping, db,
						&b_op, &keycmp));
	M0_ASSERT(rc == 0);

}

M0_INTERNAL void m0_be_emap_fini(struct m0_be_emap *map)
{
	struct m0_btree_op b_op = {};
	int                rc = 0;

	map->em_version = 0;
	rc = M0_BTREE_OP_SYNC_WITH_RC(&b_op,
				      m0_btree_close(map->em_mapping, &b_op));
	M0_ASSERT(rc == 0);
	m0_free0(&map->em_mapping);
	m0_rwlock_fini(emap_rwlock(map));
}

M0_INTERNAL void m0_be_emap_create(struct m0_be_emap   *map,
				   struct m0_be_tx     *tx,
				   struct m0_be_op     *op,
				   const struct m0_fid *bfid)
{
	struct m0_btree_type       bt;
	struct m0_btree_op         b_op = {};
	struct m0_fid              fid;
	int                        rc;
	struct m0_btree_rec_key_op keycmp;
	M0_PRE(map->em_seg != NULL);

	m0_be_op_active(op);
	be_emap_init(map, map->em_seg);
	M0_ALLOC_PTR(map->em_mapping);
	if (map->em_mapping == NULL)
		M0_ASSERT(0);

	bt = (struct m0_btree_type) {
		.tt_id = M0_BT_EMAP_EM_MAPPING,
		.ksize = sizeof(struct m0_be_emap_key),
		.vsize = -1,
	};
	keycmp.rko_keycmp = be_emap_cmp;
	fid = M0_FID_TINIT('b', M0_BT_EMAP_EM_MAPPING, bfid->f_key);
	rc = M0_BTREE_OP_SYNC_WITH_RC(&b_op,
				      m0_btree_create(&map->em_mp_node,
						      sizeof map->em_mp_node,
						      &bt, &b_op,
						      map->em_mapping,
						      map->em_seg, &fid, tx,
						      &keycmp));
	if (rc != 0) {
		m0_free0(&map->em_mapping);
		op->bo_u.u_emap.e_rc = rc;
	}
	op->bo_u.u_emap.e_rc = 0;
	m0_be_op_done(op);
}

M0_INTERNAL void m0_be_emap_destroy(struct m0_be_emap *map,
				    struct m0_be_tx   *tx,
				    struct m0_be_op   *op)
{
	struct m0_btree_op b_op = {};
	m0_be_op_active(op);
	op->bo_u.u_emap.e_rc = M0_BTREE_OP_SYNC_WITH_RC(&b_op,
			       m0_btree_destroy(map->em_mapping, &b_op, tx));
	m0_free0(&map->em_mapping);
	m0_be_op_done(op);
}

M0_INTERNAL struct m0_be_emap_seg *
m0_be_emap_seg_get(struct m0_be_emap_cursor *it)
{
	return &it->ec_seg;
}

M0_INTERNAL bool m0_be_emap_ext_is_last(const struct m0_ext *ext)
{
	return ext->e_end == M0_BINDEX_MAX + 1;
}

M0_INTERNAL bool m0_be_emap_ext_is_first(const struct m0_ext *ext)
{
	return ext->e_start == 0;
}

M0_INTERNAL struct m0_be_op *m0_be_emap_op(struct m0_be_emap_cursor *it)
{
	return &it->ec_op;
}

M0_INTERNAL int m0_be_emap_op_rc(const struct m0_be_emap_cursor *it)
{
	return it->ec_op.bo_u.u_emap.e_rc;
}

M0_INTERNAL
struct m0_be_domain *m0_be_emap_seg_domain(const struct m0_be_emap *map)
{
	return map->em_seg->bs_domain;
}

M0_INTERNAL void m0_be_emap_lookup(struct m0_be_emap        *map,
				   const struct m0_uint128  *prefix,
				   m0_bindex_t               offset,
				   struct m0_be_emap_cursor *it)
{
	M0_PRE(offset <= M0_BINDEX_MAX);

	m0_be_op_active(&it->ec_op);
	m0_rwlock_read_lock(emap_rwlock(map));
	be_emap_lookup(map, prefix, offset, it);
	m0_rwlock_read_unlock(emap_rwlock(map));
	m0_be_op_done(&it->ec_op);

	M0_ASSERT_EX(be_emap_invariant(it));
}

M0_INTERNAL void m0_be_emap_close(struct m0_be_emap_cursor *it)
{
	M0_INVARIANT_EX(be_emap_invariant(it));
	be_emap_close(it);
}

static bool be_emap_changed(struct m0_be_emap_cursor *it, m0_bindex_t off)
{
	if (it->ec_version != it->ec_map->em_version || M0_FI_ENABLED("yes")) {
		M0_LOG(M0_DEBUG, "versions mismatch: %d != %d",
		       (int)it->ec_version, (int)it->ec_map->em_version);
		be_emap_lookup(it->ec_map, &it->ec_key.ek_prefix, off, it);
		return true;
	} else
		return false;
}

M0_INTERNAL void m0_be_emap_next(struct m0_be_emap_cursor *it)
{
	M0_PRE(!m0_be_emap_ext_is_last(&it->ec_seg.ee_ext));

	m0_be_op_active(&it->ec_op);

	m0_rwlock_read_lock(emap_rwlock(it->ec_map));
	if (!be_emap_changed(it, it->ec_key.ek_offset))
		be_emap_next(it);
	m0_rwlock_read_unlock(emap_rwlock(it->ec_map));

	m0_be_op_done(&it->ec_op);
}

M0_INTERNAL void m0_be_emap_prev(struct m0_be_emap_cursor *it)
{
	M0_PRE(!m0_be_emap_ext_is_first(&it->ec_seg.ee_ext));

	m0_be_op_active(&it->ec_op);

	m0_rwlock_read_lock(emap_rwlock(it->ec_map));
	if (!be_emap_changed(it, it->ec_rec.er_start - 1))
		be_emap_prev(it);
	m0_rwlock_read_unlock(emap_rwlock(it->ec_map));

	m0_be_op_done(&it->ec_op);
}

M0_INTERNAL void m0_be_emap_extent_update(struct m0_be_emap_cursor *it,
					  struct m0_be_tx          *tx,
				    const struct m0_be_emap_seg    *es)
{
	m0_be_op_active(&it->ec_op);
	emap_extent_update(it, tx, es);
	m0_be_op_done(&it->ec_op);
}

static int update_next_segment(struct m0_be_emap_cursor *it,
			       struct m0_be_tx          *tx,
			       m0_bindex_t               delta,
			       bool                      get_next)
{
	int rc = 0;

	if (get_next)
		rc = be_emap_next(it);

	if (rc == 0) {
		it->ec_seg.ee_ext.e_start -= delta;
		rc = emap_extent_update(it, tx, &it->ec_seg);
	}

	return M0_RC(rc);
}

M0_INTERNAL void m0_be_emap_merge(struct m0_be_emap_cursor *it,
				  struct m0_be_tx          *tx,
				  m0_bindex_t               delta)
{
	bool inserted = false;
	int  rc;

	M0_PRE(!m0_be_emap_ext_is_last(&it->ec_seg.ee_ext));
	M0_PRE(delta <= m0_ext_length(&it->ec_seg.ee_ext));
	M0_INVARIANT_EX(be_emap_invariant(it));

	m0_be_op_active(&it->ec_op);

	m0_rwlock_write_lock(emap_rwlock(it->ec_map));
	rc = emap_it_pack(it, be_emap_delete_wrapper, tx);

	if (rc == 0 && delta < m0_ext_length(&it->ec_seg.ee_ext)) {
		it->ec_seg.ee_ext.e_end -= delta;
		rc = emap_it_pack(it, be_emap_insert_wrapper, tx);
		inserted = true;
	}

	if (rc == 0)
		rc = emap_it_get(it) /* re-initialise cursor position */ ?:
			update_next_segment(it, tx, delta, inserted);
	m0_rwlock_write_unlock(emap_rwlock(it->ec_map));

	M0_ASSERT_EX(ergo(rc == 0, be_emap_invariant(it)));
	it->ec_op.bo_u.u_emap.e_rc = rc;
	m0_be_op_done(&it->ec_op);
}

M0_INTERNAL void m0_be_emap_split(struct m0_be_emap_cursor *it,
				  struct m0_be_tx          *tx,
				  struct m0_indexvec       *vec,
				  struct m0_buf            *cksum)
{
	M0_PRE(m0_vec_count(&vec->iv_vec) == m0_ext_length(&it->ec_seg.ee_ext));
	M0_INVARIANT_EX(be_emap_invariant(it));

	m0_be_op_active(&it->ec_op);
	m0_rwlock_write_lock(emap_rwlock(it->ec_map));
	be_emap_split(it, tx, vec, it->ec_seg.ee_ext.e_start, cksum);
	m0_rwlock_write_unlock(emap_rwlock(it->ec_map));
	m0_be_op_done(&it->ec_op);

	M0_ASSERT_EX(be_emap_invariant(it));
}

/* This function will paste the extent (ext) into all the existing overlapping
 * extent. It is assumed that cursor is correctly placed so ext is part of
 * cursor-segment (it->ec_seg).
 *
 * 1. Finds the overlap of current-segment with extent (ext)
 * 2. Based on the overlap, atmost 3 sub-segment can get created
 *    (term left/right w.r.t area of current segment left after removing clip area)
 *    a. Left   sub-seg : Overlap of start of cur-seg  with ext
 *        		      |  cur-seg  |
 *                             | clip - ext |
 *                        |Left| => [curr-seg:Start - clip:Start]
 *    b. Middle sub-seg : If ext part (to be pasted) fully overlaps with curr-seg (clip)
 *        			      |         cur-seg              |
 *                               | clip - ext |
 *                        | Left |   Middle   |  Right   |
 *    c. Right  sub-seg : Overalp of end of cur-seg with ext
 *        			      |  cur-seg  |
 *                | clip - ext |
 *                             |Right | => [clip:End - curr-seg:End]
 * 3. EMAP operation for these three segments are performed (not all may be needed)
 * 4. If part of extent (after removing clip) is remaining then new segment is read
 *    (be_emap_next) and again above operations are performed
 *
 * For checksum operation :
 * a. Left Opn  : Reduce the checksum number of byte from checksum of left segment
 * b. Right Opn : Update checksum new start and size
 *
 * During operation like punch, we need to find the size of single unit of checksum
 * this is derived based on unit size (one checksum unit for one data unit) and total
 * checksum size.
 *
 * COB when created has following extent: [0, infinity or -1 ), er_value: AET_HOLE
 * so when x-DU (Data Units) gets pasted extents are:
 * [0, x-DU), er_value: x-DeviceLBA
 * [x-DU, infinity or -1 ), er_value: AET_HOLE
 */
M0_INTERNAL void m0_be_emap_paste(struct m0_be_emap_cursor *it,
				  struct m0_be_tx          *tx,
				  struct m0_ext            *ext,
				  uint64_t                  val,
	void (*del)(struct m0_be_emap_seg*),
	void (*cut_left)(struct m0_be_emap_seg*, struct m0_ext*, uint64_t),
	void (*cut_right)(struct m0_be_emap_seg*, struct m0_ext*, uint64_t))
{
	struct m0_be_emap_seg *seg      = &it->ec_seg;
	struct m0_ext         *chunk    = &seg->ee_ext;
	const struct m0_ext    ext0     = *ext;
	struct m0_ext          clip;
	m0_bcount_t            length[3];
	typeof(val)            bstart[3] = {};
	struct m0_buf          cksum[3]  = {{0, NULL},
					   {0, NULL},
					   {0, NULL}};
	m0_bcount_t	       chunk_cs_count;
	m0_bcount_t            cksum_unit_size = 0;

	m0_bcount_t            consumed;
	uint64_t               val_orig;
	struct m0_indexvec     vec = {
		.iv_vec = {
			.v_nr    = ARRAY_SIZE(length),
			.v_count = length
		},
		.iv_index = bstart
	};
	int rc = 0;

	M0_PRE(m0_ext_is_in(chunk, ext->e_start));
	M0_INVARIANT_EX(be_emap_invariant(it));

	M0_BE_CREDIT_DEC(M0_BE_CU_EMAP_PASTE, tx);

	m0_be_op_active(&it->ec_op);

	/*
	 * Iterate over existing segments overlapping with the new one,
	 * calculating for each, what parts have to be deleted and what remains.
	 *
	 * In the worst case, an existing segment can split into three
	 * parts. Generally, some of these parts can be empty.
	 *
	 * Cutting and deleting segments is handled uniformly by
	 * be_emap_split(), thanks to the latter skipping empty segments.
	 *
	 * Note that the _whole_ new segment is inserted on the last iteration
	 * of the loop below (see length[1] assignment), thus violating the map
	 * invariant until the loop exits (the map is "porous" during that
	 * time).
	 */
	m0_rwlock_write_lock(emap_rwlock(it->ec_map));

	while (!m0_ext_is_empty(ext)) {
		m0_ext_intersection(ext, chunk, &clip);
		M0_LOG(M0_DEBUG, "ext="EXT_F" chunk="EXT_F" clip="EXT_F,
			EXT_P(ext), EXT_P(chunk), EXT_P(&clip));
		consumed = m0_ext_length(&clip);
		M0_ASSERT(consumed > 0);

		length[0] = clip.e_start - chunk->e_start;
		length[1] = clip.e_end == ext->e_end ? m0_ext_length(&ext0) : 0;
		length[2] = chunk->e_end - clip.e_end;
		M0_LOG(M0_DEBUG, "len123=%lx:%lx:%lx", (unsigned long)length[0],
			(unsigned long)length[1], (unsigned long)length[2]);

		bstart[1] = val;
		val_orig  = seg->ee_val;
		cksum[1] = it->ec_app_cksum_buf;

		if (seg->ee_cksum_buf.b_nob)
		{
			// Compute checksum unit size for given segment
			chunk_cs_count = m0_extent_get_num_unit_start(chunk->e_start,
			                                              m0_ext_length(chunk),
								      it->ec_unit_size);
			M0_ASSERT(chunk_cs_count);
			cksum_unit_size = seg->ee_cksum_buf.b_nob/chunk_cs_count;
			M0_ASSERT(cksum_unit_size);
		}

		if (length[0] > 0) {
			if (cut_left)
				cut_left(seg, &clip, val_orig);
			bstart[0] = seg->ee_val;
			if (seg->ee_cksum_buf.b_nob) {
				cksum[0].b_nob = m0_extent_get_checksum_nob(chunk->e_start,
				                                            length[0],
									    it->ec_unit_size,
									    cksum_unit_size);
				cksum[0].b_addr = seg->ee_cksum_buf.b_addr;
			}
		}
		if (length[2] > 0) {
			if (cut_right)
				cut_right(seg, &clip, val_orig);
			bstart[2] = seg->ee_val;
			if (seg->ee_cksum_buf.b_nob) {
				cksum[2].b_nob  = m0_extent_get_checksum_nob(clip.e_end, length[2],
				                                             it->ec_unit_size,
									     cksum_unit_size);
				cksum[2].b_addr = m0_extent_get_checksum_addr(seg->ee_cksum_buf.b_addr,
				                                              clip.e_end,
									      chunk->e_start,
									      it->ec_unit_size,
									      cksum_unit_size);
			}
		}
		if (length[0] == 0 && length[2] == 0 && del)
			del(seg);

		rc = be_emap_split(it, tx, &vec, length[0] > 0 ?
						chunk->e_start : ext0.e_start,
						cksum);
		if (rc != 0)
			break;

		ext->e_start += consumed;
		M0_ASSERT(ext->e_start <= ext->e_end);

		M0_LOG(M0_DEBUG, "left %llu",
				(unsigned long long)m0_ext_length(ext));

		if (m0_ext_is_empty(ext))
			break;
		/*
		 * If vec is empty, be_emap_split() just deletes
		 * the current extent and puts iterator to the next
		 * position automatically.
		 */
		if (!m0_vec_is_empty(&vec.iv_vec)) {
			M0_ASSERT(!m0_be_emap_ext_is_last(&seg->ee_ext));
			if (be_emap_next(it) != 0)
				break;
		}
	}
	m0_rwlock_write_unlock(emap_rwlock(it->ec_map));

	/* emap_dump(it); */ /* expensive - use for debug only */

	M0_ASSERT_EX(ergo(rc == 0, be_emap_invariant(it)));

	it->ec_op.bo_u.u_emap.e_rc = rc;

	m0_be_op_done(&it->ec_op);

	/*
	 * A tale of two keys.
	 *
	 * Primordial version of this function inserted the whole new extent (as
	 * specified by @ext) at the first iteration of the loop. From time to
	 * time the (clip.e_start == ext->e_start) assertion got violated for no
	 * apparent reason. Eventually, after a lot of tracing (by Anatoliy),
	 * the following sequence was tracked down:
	 *
	 * - on entry to m0_be_emap_paste():
	 *
	 *   map: *[0, 512) [512, 1024) [1024, 2048) [2048, ...)
	 *   ext:   [0, 1024)
	 *
	 *   (where current cursor position is starred).
	 *
	 * - at the end of the first iteration, instead of expected
	 *
	 *   map: [0, 1024) *[512, 1024) [1024, 2048) [2048, ...)
	 *
	 *   the map was
	 *
	 *   map: [0, 1024) *[1024, 2048) [2048, ...)
	 *
	 * - that is, the call to be_emap_split():
	 *
	 *   - deleted [0, 512) (as expected),
	 *   - inserted [0, 1024) (as expected),
	 *   - deleted [512, 1024) ?!
	 *
	 * The later is seemingly impossible, because the call deletes exactly
	 * one segment. The surprising explanation is that segment ([L, H), V)
	 * is stored as a record (L, V) with H as a key (this is documented at
	 * the top of this file) and the [0, 1024) segment has the same key as
	 * already existing [512, 1024) one, with the former forever masking the
	 * latter.
	 *
	 * The solution is to insert the new extent as the last step, but the
	 * more important moral of this melancholy story is
	 *
	 *         Thou shalt wit thine abstraction levels.
	 *
	 * In the present case, be_emap_split() operates on the level of
	 * records and keys which turns out to be subtly different from the
	 * level of segments and maps.
	 */
}

M0_INTERNAL int m0_be_emap_count(struct m0_be_emap_cursor *it,
				 m0_bcount_t *segs)
{
	struct m0_be_emap_seg *seg;
	struct m0_be_op       *op;
	m0_bcount_t            nr_segs = 0;
	int                    rc = 0;

	M0_INVARIANT_EX(be_emap_invariant(it));

	op = m0_be_emap_op(it);
	do {
		seg = m0_be_emap_seg_get(it);
		M0_ASSERT(m0_ext_is_valid(&seg->ee_ext) &&
			  !m0_ext_is_empty(&seg->ee_ext));
		++nr_segs;
		if (m0_be_emap_ext_is_last(&seg->ee_ext))
			break;
		M0_SET0(op);
		m0_be_op_init(op);
		m0_be_emap_next(it);
		m0_be_op_wait(op);
		rc = m0_be_emap_op_rc(it);
		m0_be_op_fini(op);
	} while (rc == 0);

	*segs = nr_segs;

	return M0_RC(rc);
}

M0_INTERNAL void m0_be_emap_obj_insert(struct m0_be_emap       *map,
				       struct m0_be_tx         *tx,
				       struct m0_be_op         *op,
				       const struct m0_uint128 *prefix,
				       uint64_t                 val)
{
	void                *k_ptr;
	void                *v_ptr;
	m0_bcount_t          ksize;
	m0_bcount_t          vsize;
	struct m0_btree_rec  rec = {};
	struct m0_btree_cb   put_cb = {};
	struct m0_btree_op   kv_op = {};

	m0_be_op_active(op);

	m0_rwlock_write_lock(emap_rwlock(map));
	map->em_key.ek_prefix = *prefix;
	map->em_key.ek_offset = M0_BINDEX_MAX + 1;
	m0_format_footer_update(&map->em_key);
	map->em_rec.er_start = 0;
	map->em_rec.er_value = val;
	map->em_rec.er_cksum_nob = 0;
	emap_rec_init(&map->em_rec);

	++map->em_version;
	M0_LOG(M0_DEBUG, "Nob: key = %"PRIu64" val = %"PRIu64" ",
			 map->em_key_buf.b_nob, map->em_val_buf.b_nob );

	k_ptr  = map->em_key_buf.b_addr;
	v_ptr  = map->em_val_buf.b_addr;
	ksize  = map->em_key_buf.b_nob;
	vsize  = map->em_val_buf.b_nob;
	rec    = (struct m0_btree_rec) {
		 .r_key.k_data = M0_BUFVEC_INIT_BUF(&k_ptr, &ksize),
		 .r_val        = M0_BUFVEC_INIT_BUF(&v_ptr, &vsize),
		 };
	put_cb = (struct m0_btree_cb) {
		 .c_act   = be_emap_insert_callback,
		 .c_datum = &rec,
		 };

	op->bo_u.u_emap.e_rc = M0_BTREE_OP_SYNC_WITH_RC(
		&kv_op,
		m0_btree_put(map->em_mapping, &rec, &put_cb, &kv_op, tx));
	m0_rwlock_write_unlock(emap_rwlock(map));

	m0_be_op_done(op);
}

M0_INTERNAL void m0_be_emap_obj_delete(struct m0_be_emap *map,
				       struct m0_be_tx   *tx,
				       struct m0_be_op   *op,
				 const struct m0_uint128 *prefix)
{
	int                       rc = -ENOMEM;
#ifdef __KERNEL__
	struct m0_be_emap_cursor *it;
	M0_ALLOC_PTR(it);
	if (it == NULL)
		goto err;
#else
	struct m0_be_emap_cursor  it_s;
	struct m0_be_emap_cursor *it = &it_s;
#endif

	/* Clear record buffer before lookup as it will use m0_buf for allocation
	 * and saving the variable size record having checksum
	 */
	it->ec_recbuf.b_addr = NULL;
	it->ec_recbuf.b_nob  = 0;

	m0_be_op_active(op);

	m0_rwlock_write_lock(emap_rwlock(map));
	rc = be_emap_lookup(map, prefix, 0, it);
	if (rc == 0) {
		M0_ASSERT(m0_be_emap_ext_is_first(&it->ec_seg.ee_ext) &&
			  m0_be_emap_ext_is_last(&it->ec_seg.ee_ext));
		rc = emap_it_pack(it, be_emap_delete_wrapper, tx);
		be_emap_close(it);
	}
	m0_rwlock_write_unlock(emap_rwlock(map));

#ifdef __KERNEL__
	m0_free(it);
 err:
#endif
	op->bo_u.u_emap.e_rc = rc;

	m0_be_op_done(op);
}

M0_INTERNAL void m0_be_emap_caret_init(struct m0_be_emap_caret  *car,
				       struct m0_be_emap_cursor *it,
				       m0_bindex_t               index)
{
	M0_PRE(index <= M0_BINDEX_MAX);
	M0_PRE(m0_ext_is_in(&it->ec_seg.ee_ext, index));
	car->ct_it    = it;
	car->ct_index = index;
	M0_ASSERT(be_emap_caret_invariant(car));
}

M0_INTERNAL void m0_be_emap_caret_fini(struct m0_be_emap_caret *car)
{
	M0_ASSERT(be_emap_caret_invariant(car));
}

M0_INTERNAL m0_bcount_t
m0_be_emap_caret_step(const struct m0_be_emap_caret *car)
{
	M0_ASSERT(be_emap_caret_invariant(car));
	return car->ct_it->ec_seg.ee_ext.e_end - car->ct_index;
}

M0_INTERNAL int m0_be_emap_caret_move(struct m0_be_emap_caret *car,
				      m0_bcount_t              count)
{
	int rc = 0;

	m0_be_op_active(&car->ct_it->ec_op);

	m0_rwlock_read_lock(emap_rwlock(car->ct_it->ec_map));
	M0_ASSERT(be_emap_caret_invariant(car));
	while (count > 0 && car->ct_index < M0_BINDEX_MAX + 1) {
		m0_bcount_t step;

		step = m0_be_emap_caret_step(car);
		if (count >= step) {
			struct m0_be_emap_cursor *it = car->ct_it;
			if (be_emap_changed(it, car->ct_index))
				rc = it->ec_op.bo_u.u_emap.e_rc;
			rc = rc ?: be_emap_next(it);
			if (rc < 0)
				break;
		} else
			step = count;
		car->ct_index += step;
		count -= step;
	}
	M0_ASSERT(be_emap_caret_invariant(car));
	m0_rwlock_read_unlock(emap_rwlock(car->ct_it->ec_map));

	m0_be_op_done(&car->ct_it->ec_op);
	return rc < 0 ? rc : car->ct_index == M0_BINDEX_MAX + 1;
}

M0_INTERNAL int m0_be_emap_caret_move_sync(struct m0_be_emap_caret *car,
				           m0_bcount_t              count)
{
	int rc = 0;

	M0_SET0(&car->ct_it->ec_op);
	m0_be_op_init(&car->ct_it->ec_op);
	rc = m0_be_emap_caret_move(car, count);
	if (rc == 0)
		m0_be_op_wait(&car->ct_it->ec_op);

	return rc;
}

M0_INTERNAL void m0_be_emap_credit(struct m0_be_emap      *map,
				   enum m0_be_emap_optype  optype,
				   m0_bcount_t             nr,
				   struct m0_be_tx_credit *accum)
{
	struct m0_btree_type  bt;
	uint64_t              emap_rec_size;

	M0_PRE(M0_IN(optype, (M0_BEO_CREATE, M0_BEO_DESTROY, M0_BEO_INSERT,
			      M0_BEO_DELETE, M0_BEO_UPDATE,
			      M0_BEO_MERGE, M0_BEO_SPLIT, M0_BEO_PASTE)));

	/* emap rec static size + size of max checksum possible */
	emap_rec_size = sizeof map->em_rec + max_cksum_size() * MAX_DUS;

	switch (optype) {
	case M0_BEO_CREATE:
		bt = (struct m0_btree_type) {
			.tt_id = M0_BT_EMAP_EM_MAPPING,
			.ksize = sizeof(struct m0_be_emap_key),
			.vsize = -1,
			};
		m0_btree_create_credit(&bt, accum, nr);
		break;
	case M0_BEO_DESTROY:
		M0_ASSERT(nr == 1);
		bt = (struct m0_btree_type) {
			.tt_id = M0_BT_EMAP_EM_MAPPING,
			.ksize = sizeof(struct m0_be_emap_key),
			.vsize = -1,
			};
		m0_btree_destroy_credit(map->em_mapping, &bt, accum, nr);
		break;
	case M0_BEO_INSERT:
		m0_btree_put_credit(map->em_mapping, nr,
			sizeof map->em_key, emap_rec_size, accum);
		break;
	case M0_BEO_DELETE:
		m0_btree_del_credit(map->em_mapping, nr,
			sizeof map->em_key, emap_rec_size, accum);
		break;
	case M0_BEO_UPDATE:
		m0_btree_update_credit(map->em_mapping, nr,
			sizeof map->em_key, emap_rec_size, accum);
		break;
	case M0_BEO_MERGE:
		m0_btree_del_credit(map->em_mapping, nr,
			sizeof map->em_key, emap_rec_size, accum);
		m0_btree_put_credit(map->em_mapping, nr,
			sizeof map->em_key, emap_rec_size, accum);
		m0_btree_update_credit(map->em_mapping, nr,
			sizeof map->em_key, emap_rec_size, accum);
		break;
	case M0_BEO_SPLIT:
		m0_btree_del_credit(map->em_mapping, 1,
			sizeof map->em_key, emap_rec_size, accum);
		m0_btree_put_credit(map->em_mapping, nr,
			sizeof map->em_key, emap_rec_size, accum);
		m0_btree_update_credit(map->em_mapping, 1,
			sizeof map->em_key, emap_rec_size, accum);
		M0_BE_CREDIT_INC(nr, M0_BE_CU_EMAP_SPLIT, accum);
		break;
	case M0_BEO_PASTE:
		m0_forall(i, nr, m0_be_emap_credit(map, M0_BEO_SPLIT, 3, accum),
			  true);
		M0_BE_CREDIT_INC(nr, M0_BE_CU_EMAP_PASTE, accum);
		break;
	default:
		M0_IMPOSSIBLE("invalid emap operation");
	}
}

static int
be_emap_cmp(const void *key0, const void *key1)
{
	const struct m0_be_emap_key *a0 = key0;
	const struct m0_be_emap_key *a1 = key1;

	return m0_uint128_cmp(&a0->ek_prefix, &a1->ek_prefix) ?:
		M0_3WAY(a0->ek_offset, a1->ek_offset);
}

static int
emap_it_pack(struct m0_be_emap_cursor *it,
             int (*btree_func)(struct m0_btree      *btree,
			        struct m0_be_tx     *tx,
			        struct m0_btree_op  *op,
			        const struct m0_buf *key,
			        const struct m0_buf *val),
	     struct m0_be_tx *tx)
{
	const struct m0_be_emap_seg *ext = &it->ec_seg;
	struct m0_be_emap_key       *key = &it->ec_key;
	struct m0_be_emap_rec       *rec = &it->ec_rec;
	struct m0_buf                rec_buf = {};
	struct m0_be_emap_rec       *rec_buf_ptr;
	int                          len, rc;
	struct m0_btree_op           kv_op = {};

	key->ek_prefix = ext->ee_pre;
	key->ek_offset = ext->ee_ext.e_end;
	emap_key_init(key);
	rec->er_start  = ext->ee_ext.e_start;
	rec->er_value  = ext->ee_val;
	rec->er_cksum_nob = ext->ee_cksum_buf.b_nob;
	rec->er_unit_size = it->ec_unit_size;

	/* Layout/format of emap-record (if checksum is present) which gets
	 * written:
	 * - [Hdr| Balloc-Ext-Start| B-Ext-Value| CS-nob| CS-Array[...]| Ftr]
	 * It gets stored as contigious buffer, so allocating buffer
	 */

	/* Total size of buffer needed for storing emap extent & assign */
	len = sizeof(struct m0_be_emap_rec) + rec->er_cksum_nob;
	if ((rc = m0_buf_alloc(&rec_buf, len)) != 0) {
		return rc;
	}


	/* Copy emap record till checksum buf start */
	rec_buf_ptr = (struct m0_be_emap_rec *)rec_buf.b_addr;
	*rec_buf_ptr = *rec;

	/* Copy checksum array into emap record */
	if (rec->er_cksum_nob ) {
		memcpy( (void *)&rec_buf_ptr->er_footer,
				ext->ee_cksum_buf.b_addr, rec->er_cksum_nob );
	}

	emap_rec_init(rec_buf_ptr);

	++it->ec_map->em_version;

	it->ec_op.bo_u.u_emap.e_rc =
			btree_func(it->ec_map->em_mapping, tx, &kv_op,
				   &it->ec_keybuf, &rec_buf);
	m0_buf_free(&rec_buf);

	return it->ec_op.bo_u.u_emap.e_rc;
}

static bool emap_it_prefix_ok(const struct m0_be_emap_cursor *it)
{
	return m0_uint128_eq(&it->ec_seg.ee_pre, &it->ec_prefix);
}

static int emap_it_open(struct m0_be_emap_cursor *it, int prev_rc)
{
	struct m0_be_emap_key *key;
	struct m0_be_emap_rec *rec;
	struct m0_buf          keybuf;
	struct m0_buf          recbuf;
	struct m0_be_emap_seg *ext = &it->ec_seg;
	int                    rc;

	rc = prev_rc;
	if (rc == 0) {
		m0_btree_cursor_kv_get(&it->ec_cursor, &keybuf, &recbuf);

		/* Key operation */
		key = keybuf.b_addr;
		it->ec_key = *key;

		/* Record operation */
		if (it->ec_recbuf.b_addr != NULL) {
			m0_buf_free(&it->ec_recbuf);
		}

		/* Layout/format of emap-record (if checksum is present) which gets
		 * written:
		 * - [Hdr| Balloc-Ext-Start| B-Ext-Value| CS-nob| CS-Array[...]| Ftr]
		 * It gets stored as contigious buffer, so allocating buffer
		 */
		rc = m0_buf_alloc(&it->ec_recbuf, recbuf.b_nob);
		if ( rc != 0)
			return rc;

		/* Copying record buffer and loading into it->ec_rec, note record
		 * will have incorrect footer in case of b_nob, but it->ec_recbuf
		 * will have all correct values.
		 */
		memcpy(it->ec_recbuf.b_addr, recbuf.b_addr, recbuf.b_nob );
		rec = it->ec_recbuf.b_addr;
		it->ec_rec = *rec;

		ext->ee_pre         = key->ek_prefix;
		ext->ee_ext.e_start = rec->er_start;
		ext->ee_ext.e_end   = key->ek_offset;
		m0_ext_init(&ext->ee_ext);
		ext->ee_val         = rec->er_value;
		ext->ee_cksum_buf.b_nob  = rec->er_cksum_nob;
		ext->ee_cksum_buf.b_addr = rec->er_cksum_nob ?
								 (void *)&rec->er_footer : NULL;
		it->ec_unit_size = rec->er_unit_size;
 		if (!emap_it_prefix_ok(it))
			rc = -ESRCH;
	}
	it->ec_op.bo_u.u_emap.e_rc = rc;

	return rc;
}

static void emap_it_init(struct m0_be_emap_cursor *it,
			 const struct m0_uint128  *prefix,
			 m0_bindex_t               offset,
			 struct m0_be_emap        *map)
{
	/* As EMAP record will now be variable we can't assign fix space */
	m0_buf_init(&it->ec_keybuf, &it->ec_key, sizeof it->ec_key);
	it->ec_key.ek_prefix = it->ec_prefix = *prefix;
	it->ec_key.ek_offset = offset + 1;
	emap_key_init(&it->ec_key);

	it->ec_map = map;
	it->ec_version = map->em_version;
	m0_btree_cursor_init(&it->ec_cursor, map->em_mapping);
}

static void be_emap_close(struct m0_be_emap_cursor *it)
{
	if (it->ec_recbuf.b_addr != NULL ) {
	   m0_buf_free(&it->ec_recbuf);
	}

	m0_btree_cursor_fini(&it->ec_cursor);
}

static int emap_it_get(struct m0_be_emap_cursor *it)
{
	int                 rc = 0;
	void               *k_ptr = it->ec_keybuf.b_addr;
	m0_bcount_t         ksize = it->ec_keybuf.b_nob;
	struct m0_btree_key r_key = {
		.k_data =  M0_BUFVEC_INIT_BUF(&k_ptr, &ksize),
		};

	rc = m0_btree_cursor_get(&it->ec_cursor, &r_key,
						    true);
	rc = emap_it_open(it, rc);
	return rc;
}

static int be_emap_lookup(struct m0_be_emap        *map,
			  const struct m0_uint128  *prefix,
			  m0_bindex_t               offset,
			  struct m0_be_emap_cursor *it)
{
	int rc;

	emap_it_init(it, prefix, offset, map);
	rc = emap_it_get(it);
	if (rc != 0)
		be_emap_close(it);

	M0_POST(ergo(rc == 0, m0_ext_is_in(&it->ec_seg.ee_ext, offset)));

	return M0_RC(rc);
}

static int be_emap_next(struct m0_be_emap_cursor *it)
{
	int rc;

	rc = m0_btree_cursor_next(&it->ec_cursor);
	rc = emap_it_open(it, rc);
	return rc;
}

static int
be_emap_prev(struct m0_be_emap_cursor *it)
{
	int rc;

	rc = m0_btree_cursor_prev(&it->ec_cursor);
	rc = emap_it_open(it, rc);
	return rc;
}

#if 1
static bool
be_emap_invariant_check(struct m0_be_emap_cursor *it)
{
	int                   rc;
	m0_bindex_t           reached	= 0;
	m0_bcount_t           total	= 0;

	if (!_0C(m0_be_emap_ext_is_first(&it->ec_seg.ee_ext)))
		return false;
	while (1) {
		if (!_0C(it->ec_seg.ee_ext.e_start == reached))
			return false;
		if (!_0C(it->ec_seg.ee_ext.e_end > reached))
			return false;
		if (!_0C(m0_format_footer_verify(&it->ec_key, true) == 0))
			return false;
		if (!_0C(m0_format_footer_verify(it->ec_recbuf.b_addr, true) == 0))
			return false;
		reached = it->ec_seg.ee_ext.e_end;
		total += m0_ext_length(&it->ec_seg.ee_ext);
		if (m0_be_emap_ext_is_last(&it->ec_seg.ee_ext))
			break;
		rc = be_emap_next(it);
		if (rc != 0)
			break;
	}
	if (!_0C(total == M0_BCOUNT_MAX))
		return false;
	if (!_0C(reached == M0_BINDEX_MAX + 1))
		return false;
	return true;
}

static bool
be_emap_invariant(struct m0_be_emap_cursor *it)
{
	bool                      is_good = true;
	int                       rc;
#ifdef __KERNEL__
	struct m0_be_emap_cursor *scan;

	M0_ALLOC_PTR(scan);
	if (scan == NULL)
		return false;
#else
	struct m0_be_emap_cursor  scan_s;
	struct m0_be_emap_cursor *scan = &scan_s;
#endif

	scan->ec_recbuf.b_addr = NULL;
	scan->ec_recbuf.b_nob  = 0;

	m0_rwlock_read_lock(emap_rwlock(it->ec_map));
	rc = be_emap_lookup(it->ec_map, &it->ec_key.ek_prefix, 0, scan);
	if (rc == 0) {
		is_good = be_emap_invariant_check(scan);
		be_emap_close(scan);
	}
	m0_rwlock_read_unlock(emap_rwlock(it->ec_map));

	if (!is_good)
		emap_dump(it);

#ifdef __KERNEL__
	m0_free(scan);
#endif

	return is_good;
}

#else

static bool
be_emap_invariant(struct m0_be_emap_cursor *it)
{
	return true;
}
#endif

static int
emap_extent_update(struct m0_be_emap_cursor *it,
		   struct m0_be_tx          *tx,
	     const struct m0_be_emap_seg    *es)
{
	M0_PRE(it != NULL);
	M0_PRE(es != NULL);
	M0_PRE(m0_uint128_eq(&it->ec_seg.ee_pre, &es->ee_pre));
	M0_PRE(it->ec_seg.ee_ext.e_end == es->ee_ext.e_end);

	it->ec_seg.ee_ext.e_start = es->ee_ext.e_start;
	it->ec_seg.ee_val = es->ee_val;
	return emap_it_pack(it, be_emap_update_wrapper, tx);
}

static int
be_emap_split(struct m0_be_emap_cursor *it,
	      struct m0_be_tx          *tx,
	      struct m0_indexvec       *vec,
	      m0_bindex_t               scan,
	      struct m0_buf            *cksum)
{
	int rc = 0;
	m0_bcount_t count;
	m0_bindex_t seg_end = it->ec_seg.ee_ext.e_end;
	uint32_t    i;

	for (i = 0; i < vec->iv_vec.v_nr; ++i) {
		count = vec->iv_vec.v_count[i];
		if (count == 0)
			continue;
		M0_BE_CREDIT_DEC(M0_BE_CU_EMAP_SPLIT, tx);
		it->ec_seg.ee_ext.e_start = scan;
		it->ec_seg.ee_ext.e_end   = scan + count;
		it->ec_seg.ee_val         = vec->iv_index[i];
		it->ec_seg.ee_cksum_buf   = cksum[i];

		if (it->ec_seg.ee_ext.e_end == seg_end)
			/* The end of original segment is reached:
			 * just update it instead of deleting and
			 * inserting again - it is cheaper.
			 * Note: the segment key in underlying btree
			 *       is the end offset of its extent. */
			rc = emap_it_pack(it, be_emap_update_wrapper, tx);
		else
			rc = emap_it_pack(it, be_emap_insert_wrapper, tx);
		if (rc != 0)
			break;
		scan += count;
	}

	/* If the vector is empty or the segment end was not reached:
	 * just delete the segment - that is used by m0_be_emap_paste(). */
	if (rc == 0 && (m0_vec_is_empty(&vec->iv_vec) ||
			it->ec_seg.ee_ext.e_end != seg_end)) {
		m0_bindex_t last_end = it->ec_seg.ee_ext.e_end;
		it->ec_seg.ee_ext.e_end = seg_end;
		rc = emap_it_pack(it, be_emap_delete_wrapper, tx);
		it->ec_key.ek_offset = last_end;
		m0_format_footer_update(&it->ec_key);
	}

	if (rc == 0)
		/* Re-initialize cursor position. */
		rc = emap_it_get(it);

	it->ec_op.bo_u.u_emap.e_rc = rc;
	return M0_RC(rc);
}

static bool
be_emap_caret_invariant(const struct m0_be_emap_caret *car)
{
	return _0C(m0_ext_is_in(&car->ct_it->ec_seg.ee_ext, car->ct_index) ||
		   (m0_be_emap_ext_is_last(&car->ct_it->ec_seg.ee_ext) &&
		    car->ct_index == M0_BINDEX_MAX + 1));
}

/** @} end group extmap */
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
