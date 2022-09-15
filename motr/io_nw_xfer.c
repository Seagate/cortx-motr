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


#include "motr/client.h"
#include "motr/client_internal.h"
#include "motr/addb.h"
#include "motr/pg.h"
#include "motr/io.h"

#include "lib/memory.h"          /* m0_alloc, m0_free */
#include "lib/errno.h"           /* ENOMEM */
#include "lib/finject.h"         /* M0_FI_ */
#include "lib/cksum_utils.h"
#include "lib/cksum_data.h"
#include "fid/fid.h"             /* m0_fid */
#include "rpc/rpclib.h"          /* m0_rpc_ */
#include "lib/ext.h"             /* struct m0_ext */
#include "lib/misc.h"            /* m0_extent_vec_get_checksum_addr */
#include "fop/fom_generic.h"     /* m0_rpc_item_generic_reply_rc */
#include "sns/parity_repair.h"   /* m0_sns_repair_spare_map*/
#include "fd/fd.h"               /* m0_fd_fwd_map m0_fd_bwd_map */
#include "motr/addb.h"
#include "rpc/item.h"
#include "rpc/rpc_internal.h"

#define M0_TRACE_SUBSYSTEM M0_TRACE_SUBSYS_CLIENT
#include "lib/trace.h"           /* M0_LOG */

/** BOB types for the assorted parts of io requests and nwxfer */
const struct m0_bob_type nwxfer_bobtype;
const struct m0_bob_type tioreq_bobtype;

/** BOB definitions for the assorted parts of io requests and nwxfer */
M0_BOB_DEFINE(M0_INTERNAL, &nwxfer_bobtype,  nw_xfer_request);
M0_BOB_DEFINE(M0_INTERNAL, &tioreq_bobtype,  target_ioreq);

/** BOB initialisation for the assorted parts of io requests and nwxfer */
const struct m0_bob_type nwxfer_bobtype = {
	.bt_name         = "nw_xfer_request_bobtype",
	.bt_magix_offset = offsetof(struct nw_xfer_request, nxr_magic),
	.bt_magix        = M0_NWREQ_MAGIC,
	.bt_check        = NULL,
};

const struct m0_bob_type tioreq_bobtype = {
	.bt_name         = "target_ioreq",
	.bt_magix_offset = offsetof(struct target_ioreq, ti_magic),
	.bt_magix        = M0_TIOREQ_MAGIC,
	.bt_check        = NULL,
};

static void to_op_io_map(const struct m0_op *op,
			 struct m0_op_io *ioo)
{
	uint64_t oid  = m0_sm_id_get(&op->op_sm);
	uint64_t ioid = m0_sm_id_get(&ioo->ioo_sm);

	if (ioo->ioo_addb2_mapped++ == 0)
		M0_ADDB2_ADD(M0_AVI_CLIENT_TO_IOO, oid, ioid);
}

static void m0_op_io_to_rpc_map(const struct m0_op_io    *ioo,
				const struct m0_rpc_item *item)
{
	uint64_t rid  = m0_sm_id_get(&item->ri_sm);
	uint64_t ioid = m0_sm_id_get(&ioo->ioo_sm);
	M0_ADDB2_ADD(M0_AVI_IOO_TO_RPC, ioid, rid);
}

/**
 * Calculate the size needed for per-segment on-wire data integrity.
 * Note: Client leaves its applications to decide how to use locks on
 * objects, so it doesn't manage any lock. But file lock is needed
 * to calculate di size, a file lock is faked here to get di details.
 * Clearly, a more reliable way to get di size is needed.
 *
 * @param ioo The IO operation, to find the client instance.
 * @return the size of data integrity data.
 */
static uint32_t io_di_size(struct m0_op_io *ioo)
{
	uint32_t                rc = 0;
	const struct m0_fid    *fid;
	const struct m0_di_ops *di_ops;
	struct m0_file         *file;

	M0_PRE(ioo != NULL);

	#ifndef ENABLE_DATA_INTEGRITY
		return M0_RC(rc);
	#endif
	/* Get di details (workaround!) by setting the dom be NULL*/
	file = &ioo->ioo_flock;
	fid = &ioo->ioo_oo.oo_fid;
	m0_file_init(file, fid, NULL, M0_DI_DEFAULT_TYPE);
	di_ops = file->fi_di_ops;

	if (di_ops->do_out_shift(file) == 0)
		return M0_RC(0);

	rc = di_ops->do_out_shift(file) * M0_DI_ELEMENT_SIZE;

	return M0_RC(rc);
}

static void parity_page_pos_get(struct pargrp_iomap *map,
				m0_bindex_t          index,
				uint32_t            *row,
				uint32_t            *col)
{
	uint64_t		  pg_id;
	struct m0_pdclust_layout *play;

	M0_PRE(map != NULL);
	M0_PRE(row != NULL);
	M0_PRE(col != NULL);

	play = pdlayout_get(map->pi_ioo);

	pg_id = page_id(index, map->pi_ioo->ioo_obj);
	*row  = pg_id % rows_nr(play, map->pi_ioo->ioo_obj);
	*col  = pg_id / rows_nr(play, map->pi_ioo->ioo_obj);
}

/**
 * Allocates an index and buffer vector(in structure dgmode_rwvec) for a
 * degraded mode IO.
 * This is heavily based on m0t1fs/linux_kernel/file.c::dgmode_rwvec_alloc_init
 *
 * @param ti The target_ioreq fop asking for the allocation.
 * @return 0 for success, or -errno.
 */
static int dgmode_rwvec_alloc_init(struct target_ioreq *ti)
{
	int                       rc;
	uint64_t                  cnt;
	struct dgmode_rwvec      *dg;
	struct m0_pdclust_layout *play;
	struct m0_op_io   *ioo;

	M0_ENTRY();
	M0_PRE(ti != NULL);
	M0_PRE(ti->ti_dgvec == NULL);

	ioo = bob_of(ti->ti_nwxfer, struct m0_op_io, ioo_nwxfer,
		     &ioo_bobtype);

	M0_ALLOC_PTR(dg);
	if (dg == NULL) {
		rc = -ENOMEM;
		goto failed;
	}

	play = pdlayout_get(ioo);
	dg->dr_tioreq = ti;

	cnt = page_nr(ioo->ioo_iomap_nr
		      * layout_unit_size(play)
		      * (layout_n(play) + layout_k(play)),
		      ioo->ioo_obj);
	rc  = m0_indexvec_alloc(&dg->dr_ivec, cnt);
	if (rc != 0)
		goto failed;

	M0_ALLOC_ARR(dg->dr_bufvec.ov_buf, cnt);
	if (dg->dr_bufvec.ov_buf == NULL) {
		rc = -ENOMEM;
		goto failed;
	}

	M0_ALLOC_ARR(dg->dr_bufvec.ov_vec.v_count, cnt);
	if (dg->dr_bufvec.ov_vec.v_count == NULL) {
		rc = -ENOMEM;
		goto failed;
	}

	M0_ALLOC_ARR(dg->dr_auxbufvec.ov_buf, cnt);
	if (dg->dr_auxbufvec.ov_buf == NULL) {
		rc = -ENOMEM;
		goto failed;
	}

	M0_ALLOC_ARR(dg->dr_auxbufvec.ov_vec.v_count, cnt);
	if (dg->dr_auxbufvec.ov_vec.v_count == NULL) {
		rc = -ENOMEM;
		goto failed;
	}

	M0_ALLOC_ARR(dg->dr_pageattrs, cnt);
	if (dg->dr_pageattrs == NULL) {
		rc = -ENOMEM;
		goto failed;
	}

	/*
	 * This value is incremented every time a new segment is added
	 * to this index vector.
	 */
	dg->dr_ivec.iv_vec.v_nr = 0;

	ti->ti_dgvec = dg;
	return M0_RC(0);
failed:
	ti->ti_dgvec = NULL;
	if (dg->dr_bufvec.ov_buf != NULL)
		m0_free(dg->dr_bufvec.ov_buf);
	if (dg->dr_bufvec.ov_vec.v_count != NULL)
		m0_free(dg->dr_bufvec.ov_vec.v_count);
	if (dg->dr_auxbufvec.ov_buf != NULL)
		m0_free(dg->dr_auxbufvec.ov_buf);
	if (dg->dr_auxbufvec.ov_vec.v_count != NULL)
		m0_free(dg->dr_auxbufvec.ov_vec.v_count);
	m0_free(dg);
	return M0_ERR(rc);
}

/**
 * Free index and buffer vector stored in structure dgmode_rwvec for a
 * degraded mode IO.
 * This is heavily based on
 * m0t1fs/linux_kernel/file.c::dgmode_rwvec_dealloc_fini
 *
 * @param dg The dgmode_rwvec to be finalised.
 * @return NULL
 */
static void dgmode_rwvec_dealloc_fini(struct dgmode_rwvec *dg)
{
	M0_ENTRY();

	M0_PRE(dg != NULL);

	dg->dr_tioreq = NULL;
	/*
	 * Will need to go through array of parity groups to find out
	 * exact number of segments allocated for the index vector.
	 * Instead, a fixed number of segments is enough to avoid
	 * triggering the assert from m0_indexvec_free().
	 * The memory allocator knows the size of memory area held by
	 * dg->dr_ivec.iv_index and dg->dr_ivec.iv_vec.v_count.
	 */
	if (dg->dr_ivec.iv_vec.v_nr == 0)
		++dg->dr_ivec.iv_vec.v_nr;

	m0_indexvec_free(&dg->dr_ivec);
	m0_free(dg->dr_bufvec.ov_buf);
	m0_free(dg->dr_bufvec.ov_vec.v_count);
	m0_free(dg->dr_auxbufvec.ov_buf);
	m0_free(dg->dr_auxbufvec.ov_vec.v_count);
	m0_free(dg->dr_pageattrs);
	m0_free(dg);
}

/**
 * Generates a hash for a target-io request.
 * This is heavily based on m0t1fs/linux_kernel/file.c::tioreqs_hash_func.
 *
 * @param htable The hash table in use.
 * @param k Pointer to the key of the entry to hash.
 * @return the hash key.
 */
static uint64_t tioreqs_hash_func(const struct m0_htable *htable, const void *k)
{
	const uint64_t *key;
	M0_PRE(htable != NULL);
	M0_PRE(htable->h_bucket_nr > 0);
	M0_PRE(k != NULL);

	key = (uint64_t *)k;

	return *key % htable->h_bucket_nr;
}

/**
 * Compares keys for target-io requets.
 * This is heavily based on m0t1fs/linux_kernel/file.c::tioreq_key_eq.
 *
 * @param key1 The key of the first target-io request.
 * @param key2 The key of the second target-io request.
 * @return true or false.
 */
static bool tioreq_key_eq(const void *key1, const void *key2)
{
	const uint64_t *k1 = (uint64_t *)key1;
	const uint64_t *k2 = (uint64_t *)key2;

	M0_PRE(k1 != NULL);
	M0_PRE(k2 != NULL);

	return *k1 == *k2;
}

M0_HT_DESCR_DEFINE(tioreqht, "Hash of target_ioreq objects", M0_INTERNAL,
		   struct target_ioreq, ti_link, ti_magic,
		   M0_TIOREQ_MAGIC, M0_TLIST_HEAD_MAGIC,
		   ti_fid.f_container, tioreqs_hash_func, tioreq_key_eq);

M0_HT_DEFINE(tioreqht, M0_INTERNAL, struct target_ioreq, uint64_t);

/**
 * Checks a target_ioreq struct is correct.
 * This is heavily based on m0t1fs/linux_kernel/file.c::target_ioreq_invariant
 *
 * @param ti The target_ioreq fop to check.
 * @return true or false.
 */
static bool target_ioreq_invariant(const struct target_ioreq *ti)
{
	return M0_RC(ti != NULL &&
		     _0C(target_ioreq_bob_check(ti)) &&
		     _0C(ti->ti_session       != NULL) &&
		     _0C(ti->ti_nwxfer        != NULL) &&
		     _0C(ti->ti_bufvec.ov_buf != NULL) &&
		     _0C(ti->ti_auxbufvec.ov_buf != NULL) &&
		     _0C(m0_fid_is_valid(&ti->ti_fid)) &&
		     m0_tl_forall(iofops, iofop, &ti->ti_iofops,
			          ioreq_fop_invariant(iofop)));
}

/**
 * This is heavily based on m0t1fs/linux_kernel/file.c::nw_xfer_request_invariant
 */
M0_INTERNAL bool nw_xfer_request_invariant(const struct nw_xfer_request *xfer)
{
	return xfer != NULL &&
	       _0C(nw_xfer_request_bob_check(xfer)) &&
	       _0C(xfer->nxr_state < NXS_STATE_NR) &&

	       _0C(ergo(xfer->nxr_state == NXS_INITIALIZED,
			xfer->nxr_rc == 0 && xfer->nxr_bytes == 0 &&
			m0_atomic64_get(&xfer->nxr_iofop_nr) == 0)) &&

	       _0C(ergo(xfer->nxr_state == NXS_INFLIGHT,
			!tioreqht_htable_is_empty(&xfer->nxr_tioreqs_hash))) &&

	       _0C(ergo(xfer->nxr_state == NXS_COMPLETE,
			m0_atomic64_get(&xfer->nxr_iofop_nr) == 0 &&
			m0_atomic64_get(&xfer->nxr_rdbulk_nr) == 0)) &&

	       m0_htable_forall(tioreqht, tioreq, &xfer->nxr_tioreqs_hash,
				target_ioreq_invariant(tioreq));
}

/**
 * This is heavily based on m0t1fs/linux_kernel/file.c::target_ioreq_fini
 */
void target_ioreq_fini(struct target_ioreq *ti)
{
	struct m0_op_io *ioo;
	unsigned int     opcode;

	M0_ENTRY("target_ioreq %p", ti);

	M0_PRE(target_ioreq_invariant(ti));
	M0_PRE(iofops_tlist_is_empty(&ti->ti_iofops));

	ioo = bob_of(ti->ti_nwxfer, struct m0_op_io,
		     ioo_nwxfer, &ioo_bobtype);
	opcode = ioo->ioo_oo.oo_oc.oc_op.op_code;
	target_ioreq_bob_fini(ti);
	tioreqht_tlink_fini(ti);
	iofops_tlist_fini(&ti->ti_iofops);
	ti->ti_ops     = NULL;
	ti->ti_session = NULL;
	ti->ti_nwxfer  = NULL;

	/* Resets the number of segments in vector. */
	if (ti->ti_ivec.iv_vec.v_nr == 0)
		ti->ti_ivec.iv_vec.v_nr = ti->ti_bufvec.ov_vec.v_nr;

	m0_indexvec_free(&ti->ti_ivec);
	if (opcode == M0_OC_FREE)
		m0_indexvec_free(&ti->ti_trunc_ivec);
	m0_free0(&ti->ti_bufvec.ov_buf);
	m0_free0(&ti->ti_bufvec.ov_vec.v_count);
	m0_free0(&ti->ti_auxbufvec.ov_buf);
	m0_free0(&ti->ti_auxbufvec.ov_vec.v_count);
	m0_free0(&ti->ti_pageattrs);

	if (ti->ti_dgvec != NULL)
		dgmode_rwvec_dealloc_fini(ti->ti_dgvec);
	if (ti->ti_cc_fop_inited) {
		struct m0_rpc_item *item = &ti->ti_cc_fop.crf_fop.f_item;
		M0_LOG(M0_DEBUG, "item="ITEM_FMT" osr_xid=%"PRIu64,
				  ITEM_ARG(item), item->ri_header.osr_xid);
		ti->ti_cc_fop_inited = false;
		m0_fop_put_lock(&ti->ti_cc_fop.crf_fop);
	}

	m0_indexvec_free(&ti->ti_goff_ivec);

	m0_free(ti);
	M0_LEAVE();
}

void target_ioreq_cancel(struct target_ioreq *ti)
{
	struct ioreq_fop *irfop;

	m0_tl_for (iofops, &ti->ti_iofops, irfop) {
		m0_rpc_item_cancel(&irfop->irf_iofop.if_fop.f_item);
	} m0_tl_endfor;
}

/**
 * This is heavily based on m0t1fs/linux_kernel/file.c::target_ioreq_locate
 */
static struct target_ioreq *target_ioreq_locate(struct nw_xfer_request *xfer,
						struct m0_fid          *fid)
{
	struct target_ioreq *ti;

	M0_ENTRY("nw_xfer_request %p, fid %p", xfer, fid);

	M0_PRE(nw_xfer_request_invariant(xfer));
	M0_PRE(fid != NULL);

	ti = tioreqht_htable_lookup(&xfer->nxr_tioreqs_hash, &fid->f_container);
	/* WARN: Searches only with the container but compares the whole fid. */
	M0_ASSERT(ergo(ti != NULL, m0_fid_cmp(fid, &ti->ti_fid) == 0));

	M0_LEAVE();
	return ti;
}

/*
 * For partially parity groups only data units present in the truncate range
 * will be truncated. For fully spanned parity group both data and parity
 * units will be truncated.
 */
static bool should_unit_be_truncated(bool                      partial,
				     enum m0_pdclust_unit_type unit_type,
				     enum page_attr            flags)
{
	return (!partial || unit_type == M0_PUT_DATA) &&
	       (flags & PA_WRITE);
}

/**
 * Adds an io segment to index vector and buffer vector in
 * target_ioreq structure.
 * This is heavily based on m0t1fs/linux_kernel/file.c::target_ioreq_seg_add
 *
 * @param ti The target io request.
 * @param src Where in the global file the io occurs.
 * @param tgt Where in the target servers 'tile' the io occurs.
 * @param gob_offset Offset in the global file.
 * @param count Number of bytes in this operation.
 * @param map Map of data/parity buffers, used to tie ti to the corresponding
 *            buffers.
 */
static void target_ioreq_seg_add(struct target_ioreq              *ti,
				 const struct m0_pdclust_src_addr *src,
				 const struct m0_pdclust_tgt_addr *tgt,
				 m0_bindex_t                       gob_offset,
				 m0_bcount_t                       count,
				 struct pargrp_iomap              *map)
{
	uint32_t                   seg;
	uint32_t                   tseg;
	m0_bindex_t                toff;
	m0_bindex_t                goff;
	m0_bindex_t                goff_cksum;
	m0_bindex_t                pgstart;
	m0_bindex_t                pgend;
	struct data_buf           *buf;
	struct m0_op_io           *ioo;
	struct m0_pdclust_layout  *play;
	uint64_t                   frame;
	uint64_t                   unit;
	struct m0_indexvec        *ivec;
	struct m0_indexvec        *trunc_ivec = NULL;
	struct m0_bufvec          *bvec;
	struct m0_bufvec          *auxbvec;
	enum m0_pdclust_unit_type  unit_type;
	enum page_attr            *pattr;
	uint64_t                   cnt;
	unsigned int               opcode;
	m0_bcount_t                grp_size;
	uint64_t                   page_size;
	struct m0_indexvec        *goff_ivec = NULL;

	M0_PRE(tgt != NULL);
	frame = tgt->ta_frame;
	M0_PRE(src != NULL);
	unit  = src->sa_unit;
	M0_ENTRY("tio req %p, gob_offset %" PRIu64 ", count %"PRIu64
		 " frame %" PRIu64 " unit %"PRIu64,
		 ti, gob_offset, count, frame, unit);

	M0_PRE(ti != NULL);
	M0_PRE(map != NULL);
	M0_PRE(target_ioreq_invariant(ti));

	ti->ti_goff = gob_offset;

	ioo = bob_of(ti->ti_nwxfer, struct m0_op_io,
		     ioo_nwxfer, &ioo_bobtype);
	opcode = ioo->ioo_oo.oo_oc.oc_op.op_code;
	play = pdlayout_get(ioo);

	page_size = m0__page_size(ioo);
	grp_size = data_size(play) * map->pi_grpid;
	unit_type = m0_pdclust_unit_classify(play, unit);
	M0_ASSERT(M0_IN(unit_type, (M0_PUT_DATA, M0_PUT_PARITY)));

	toff    = target_offset(frame, play, gob_offset);
	pgstart = toff;
	goff    = unit_type == M0_PUT_DATA ? gob_offset : 0;

	/* For checksum of Parity Unit the global object offset will be
	 * assumed to be aligned with PG start offset, so removing the
	 * additional value NxUS w.r.t PG start, which is added by the
	 * nw_xfer_io_distribute() function. src.sa_unit = layout_n(play) + unit
	 * Removing this offset N will help to compute PG unit idx as 0,1..,k-1
	 * which is the index of pi_paritybufs
	 */
	goff_cksum = unit_type == M0_PUT_DATA ? gob_offset :
		     (gob_offset + (src->sa_unit - layout_n(play)) *
		     layout_unit_size(play));

	/**
	 * There are scenarios where K > N in such case when the
	 * unit type is M0_PUT_PARITY, adding [(K-N) x gob offset] into
	 * global object offset so that right PG and Unit index will get
	 * computed.
	 */
	if ((unit_type == M0_PUT_PARITY) &&
	   (layout_k(play) > layout_n(play))) {
		m0_bcount_t goff_delta = (layout_k(play) -
					  layout_n(play)) *
					  gob_offset;
		goff_cksum += goff_delta;
	}

	M0_LOG(M0_DEBUG,
	       "[gpos %" PRIu64 ", count %" PRIu64 "] [%" PRIu64 ", %" PRIu64 "]"
	       "->[%" PRIu64 ",%" PRIu64 "] %c", gob_offset, count, src->sa_group,
	       src->sa_unit, tgt->ta_frame, tgt->ta_obj,
	       unit_type == M0_PUT_DATA ? 'D' : 'P');

	/* Use ti_dgvec as long as it is dgmode-read/write. */
	if (ioreq_sm_state(ioo) == IRS_DEGRADED_READING ||
	    ioreq_sm_state(ioo) == IRS_DEGRADED_WRITING)  {
		M0_ASSERT(ti->ti_dgvec != NULL);
		ivec  = &ti->ti_dgvec->dr_ivec;
		bvec  = &ti->ti_dgvec->dr_bufvec;
		auxbvec  = &ti->ti_dgvec->dr_auxbufvec;
		pattr = ti->ti_dgvec->dr_pageattrs;
		cnt = page_nr(ioo->ioo_iomap_nr * layout_unit_size(play) *
		      (layout_n(play) + layout_k(play)), ioo->ioo_obj);
		M0_LOG(M0_DEBUG, "map_nr=%" PRIu64 " req state=%u cnt=%"PRIu64,
				 ioo->ioo_iomap_nr, ioreq_sm_state(ioo), cnt);
	} else {
		ivec  = &ti->ti_ivec;
		trunc_ivec  = &ti->ti_trunc_ivec;
		bvec  = &ti->ti_bufvec;
		auxbvec = &ti->ti_auxbufvec;
		pattr = ti->ti_pageattrs;
		cnt = page_nr(ioo->ioo_iomap_nr * layout_unit_size(play) *
			      layout_n(play), ioo->ioo_obj);
		M0_LOG(M0_DEBUG, "map_nr=%" PRIu64 " req state=%u cnt=%"PRIu64,
				 ioo->ioo_iomap_nr, ioreq_sm_state(ioo), cnt);
	}

	goff_ivec = &ti->ti_goff_ivec;
	while (pgstart < toff + count) {
		pgend = min64u(pgstart + page_size,
			       toff + count);
		seg   = SEG_NR(ivec);

		INDEX(ivec, seg) = pgstart;
		COUNT(ivec, seg) = pgend - pgstart;

		if (unit_type == M0_PUT_DATA) {
			uint32_t row = map->pi_max_row;
			uint32_t col = map->pi_max_col;

			page_pos_get(map, goff, grp_size, &row, &col);
			M0_ASSERT(row <= map->pi_max_row);
			M0_ASSERT(col <= map->pi_max_col);
			buf = map->pi_databufs[row][col];

			pattr[seg] |= PA_DATA;
			M0_LOG(M0_DEBUG, "Data seg %u added", seg);
		} else {
			buf = map->pi_paritybufs[page_id(goff, ioo->ioo_obj)]
			[unit - layout_n(play)];
			pattr[seg] |= PA_PARITY;
			M0_LOG(M0_DEBUG, "Parity seg %u added", seg);
		}
		buf->db_tioreq = ti;
		if (buf->db_flags & PA_WRITE)
			ti->ti_req_type = TI_READ_WRITE;

		if (opcode == M0_OC_FREE &&
		    should_unit_be_truncated(map->pi_trunc_partial,
					     unit_type, buf->db_flags)) {
			tseg   = SEG_NR(trunc_ivec);
			INDEX(trunc_ivec, tseg) = pgstart;
			COUNT(trunc_ivec, tseg) = pgend - pgstart;
			++trunc_ivec->iv_vec.v_nr;
			M0_LOG(M0_DEBUG, "Seg id %d [%" PRIu64 ", %" PRIu64 "]"
					 "added to target ioreq with "FID_F,
					 tseg, INDEX(trunc_ivec, tseg),
					 COUNT(trunc_ivec, tseg),
					 FID_P(&ti->ti_fid));
		}

		if (opcode == M0_OC_FREE && !map->pi_trunc_partial)
			pattr[seg] |= PA_TRUNC;

		M0_ASSERT(addr_is_network_aligned(buf->db_buf.b_addr));
		bvec->ov_buf[seg] = buf->db_buf.b_addr;
		bvec->ov_vec.v_count[seg] = COUNT(ivec, seg);
		if (map->pi_rtype == PIR_READOLD &&
		    unit_type == M0_PUT_DATA) {
			M0_ASSERT(buf->db_auxbuf.b_addr != NULL);
			auxbvec->ov_buf[seg] = buf->db_auxbuf.b_addr;
			auxbvec->ov_vec.v_count[seg] = page_size;
		}
		pattr[seg] |= buf->db_flags;
		M0_LOG(M0_DEBUG, "pageaddr=%p, auxpage=%p,"
				 " index=%6" PRIu64 ", size=%4"PRIu64
				 " grpid=%3" PRIu64 " flags=%4x for "FID_F,
		                 bvec->ov_buf[seg], auxbvec->ov_buf[seg],
				 INDEX(ivec, seg), COUNT(ivec, seg),
				 map->pi_grpid, pattr[seg],
				 FID_P(&ti->ti_fid));
		M0_LOG(M0_DEBUG, "Seg id %d [%" PRIu64 ", %"PRIu64
				 "] added to target_ioreq with "FID_F
				 " with flags 0x%x: ", seg,
				 INDEX(ivec, seg), COUNT(ivec, seg),
				 FID_P(&ti->ti_fid), pattr[seg]);

		/**
		 * Storing the values of goff(checksum offset) into the
		 * goff_ivec according to target offset. This creates a
		 * mapping between target offset and cheksum offset.
		 *
		 * This mapping will be used to compute PG Index and
		 * Unit Index for each target when FOP is being prepared.
		 */
		INDEX(goff_ivec, seg) = goff_cksum;
		COUNT(goff_ivec, seg) = COUNT(ivec, seg);
		goff_ivec->iv_vec.v_nr++;
		goff_cksum += COUNT(ivec, seg);

		goff += COUNT(ivec, seg);
		++ivec->iv_vec.v_nr;
		pgstart = pgend;
	}
	M0_LEAVE();
}

/**
 * This is heavily based on m0t1fs/linux_kernel/file.c::target_fid().
 */
M0_INTERNAL struct m0_fid target_fid(struct m0_op_io *ioo,
				     struct m0_pdclust_tgt_addr *tgt)
{
	struct m0_fid fid;

	m0_poolmach_gob2cob(ioo_to_poolmach(ioo),
			    &ioo->ioo_oo.oo_fid, tgt->ta_obj,
			    &fid);
	return fid;
}

/**
 * Finds the rpc session to use to contact the server hosting a particular
 * target:fid (cob).
 * This is heavily based on m0t1fs/linux_kernel/file.c::target_session
 *
 * @param ioo The IO operation.
 * @param tfid The cob fid to look for.
 * @param a pointer to the rpc_session to use to contact this target.
 */
static inline struct m0_rpc_session *
target_session(struct m0_op_io *ioo, struct m0_fid tfid)
{
	struct m0_op           *op;
	struct m0_pool_version *pv;
	struct m0_client       *instance;

	M0_PRE(ioo != NULL);
	op = &ioo->ioo_oo.oo_oc.oc_op;
	instance = m0__op_instance(op);
	pv = m0_pool_version_find(&instance->m0c_pools_common, &ioo->ioo_pver);
	M0_ASSERT(pv != NULL);

	return m0_obj_container_id_to_session(
			pv, m0_fid_cob_device_id(&tfid));
}

/**
 * Pair data/parity buffers with the io fop rpc.
 * This is heavily based on
 * m0t1fs/linux_kernel/file.c::target_ioreq_iofops_prepare
 *
 * @param irfop The io fop that needs bulk buffers adding.
 * @param dom The network domain this rpc will be sent in.
 * @param rbuf[out] The rpc bulk buffer that contains the target-iorequest's
 *                  extents.
 * @param delta[out] The extra space in the fop needed for metadata.
 * @param maxsize Caller provided limit.
 * @return 0 for success, -errno otherwise.
 */
static int bulk_buffer_add(struct ioreq_fop        *irfop,
			   struct m0_net_domain    *dom,
			   struct m0_rpc_bulk_buf **rbuf,
			   uint32_t                *delta,
			   uint32_t                 maxsize)
{
	int                      rc;
	int                      seg_nr;
	struct m0_op_io  *ioo;
	struct m0_indexvec      *ivec;

	M0_PRE(irfop  != NULL);
	M0_PRE(dom    != NULL);
	M0_PRE(rbuf   != NULL);
	M0_PRE(delta  != NULL);
	M0_PRE(maxsize > 0);
	M0_ENTRY("ioreq_fop %p net_domain %p delta_size %d",
		 irfop, dom, *delta);

	ioo     = bob_of(irfop->irf_tioreq->ti_nwxfer, struct m0_op_io,
			 ioo_nwxfer, &ioo_bobtype);
	ivec    = M0_IN(ioreq_sm_state(ioo), (IRS_READING, IRS_WRITING)) ?
			&irfop->irf_tioreq->ti_ivec :
			&irfop->irf_tioreq->ti_dgvec->dr_ivec;
	seg_nr  = min32(m0_net_domain_get_max_buffer_segments(dom),
			SEG_NR(ivec));
	*delta += io_desc_size(dom);

	if (m0_io_fop_size_get(&irfop->irf_iofop.if_fop) + *delta < maxsize) {
		rc = m0_rpc_bulk_buf_add(&irfop->irf_iofop.if_rbulk, seg_nr,
					 0, dom, NULL, rbuf);
		if (rc != 0) {
			*delta -= io_desc_size(dom);
			return M0_ERR(rc);
		}
	} else {
		rc      = -ENOSPC;
		*delta -= io_desc_size(dom);
	}

	M0_POST(ergo(rc == 0, *rbuf != NULL));
	return M0_RC(rc);
}

/**
 * Finalises an io request fop, (releases bulk buffers).
 * This is heavily based on m0t1fs/linux_kernel/file.c::irfop_fini
 *
 * @param irfop The io request fop to finalise.
 */
static void irfop_fini(struct ioreq_fop *irfop)
{
	M0_ENTRY("ioreq_fop %p", irfop);

	M0_PRE(irfop != NULL);

	m0_rpc_bulk_buflist_empty(&irfop->irf_iofop.if_rbulk);
	ioreq_fop_fini(irfop);
	m0_free(irfop);

	M0_LEAVE();
}

/**
 * Helper function which will return the buffer address based on the page attr,
 * fop phase and aux bufvec.
 */
static void *buf_aux_chk_get(struct m0_bufvec *aux, enum page_attr p_attr,
			     uint32_t seg_idx, bool rd_in_wr)
{
	return (p_attr == PA_DATA && rd_in_wr && aux != NULL &&
		aux->ov_buf[seg_idx] != NULL) ? aux->ov_buf[seg_idx] : NULL;
}

/* This function will compute parity checksum in chksm_buf all other
 * parameter is input parameter
 */
int m0_target_calculate_checksum(struct m0_op_io *ioo, uint8_t pi_type,
				 enum page_attr filter,
				 struct fop_cksum_idx_data *cs_idx,
				 void *chksm_buf)
{
	struct m0_generic_pi       *pi;
	struct m0_pi_seed           seed;
	struct m0_bufvec            bvec={};
	enum m0_pi_calc_flag        flag;
	uint8_t                     context[M0_CKSUM_MAX_SIZE];
	int                         rc;
	int                         row;
	int                         row_seq;
	int                         b_idx = 0;
	struct m0_pdclust_layout   *play;
	struct m0_obj              *obj;
	struct pargrp_iomap        *map;
	struct data_buf          ***data;
	struct m0_buf              *buf;
	struct m0_buf              *buf_seq;
	uint64_t                    u_idx;

	u_idx = cs_idx->ci_unit_idx;
	if (cs_idx->ci_pg_idx >= ioo->ioo_iomap_nr)
		return -EINVAL;

	pi = (struct m0_generic_pi *)chksm_buf;
	map = ioo->ioo_iomaps[cs_idx->ci_pg_idx];
	play = pdlayout_get(map->pi_ioo);
	obj = map->pi_ioo->ioo_obj;

	pi->pi_hdr.pih_type = pi_type;
	flag = M0_PI_CALC_UNIT_ZERO;
	seed.pis_data_unit_offset = ((cs_idx->ci_pg_idx +
				     ioo->ioo_iomaps[0]->pi_grpid) *
				     layout_n(play)) +
				     u_idx;
	seed.pis_obj_id.f_container = ioo->ioo_obj->ob_entity.en_id.u_hi;
	seed.pis_obj_id.f_key = ioo->ioo_obj->ob_entity.en_id.u_lo;

	/* Select data pointer */
	if (filter == PA_PARITY) {
		data = map->pi_paritybufs;
		if (u_idx >= layout_k(play))
			return -EINVAL;
	} else {
		data = map->pi_databufs;
		if (u_idx >= layout_n(play))
			return -EINVAL;
	}

	rc = m0_bufvec_empty_alloc(&bvec, rows_nr(play, obj));
	if (rc != 0)
		return -ENOMEM;

	/**
	 * Populate buffer vec for give parity unit and add all buffers present
	 * in rows (page sized buffer/4K)
	 */
	for (row = 0; row < rows_nr(play, obj); ++row) {
		if (data[row][u_idx]) {
			buf = &data[row][u_idx]->db_buf;
			/*  New cycle so init buffer and count */
			bvec.ov_buf[b_idx] = buf->b_addr;
			bvec.ov_vec.v_count[b_idx] = buf->b_nob;

			row_seq = row + 1;
			while ((row_seq < rows_nr(play, obj)) &&
			       data[row_seq][u_idx]) {
				buf_seq = &data[row_seq][u_idx]->db_buf;
				if (buf->b_addr + buf->b_nob != buf_seq->b_addr)
					break;
				bvec.ov_vec.v_count[b_idx] += buf_seq->b_nob;
				row++;
				buf = &data[row][u_idx]->db_buf;
				row_seq++;
			}
			b_idx++;
		}
	}

	M0_LOG(M0_DEBUG,"COMPUTE CKSUM Typ:%d Sz:%d UTyp:[%s] [PG Idx:%" PRIu64
			"][Unit Idx:%"PRIu64"] TotalRowNum:%d ActualRowNum:%d",
			(int)pi_type, m0_cksum_get_size(pi_type),
			(filter == PA_PARITY) ? "P":"D",
			(cs_idx->ci_pg_idx + ioo->ioo_iomaps[0]->pi_grpid),
			u_idx,(int)rows_nr(play, obj),b_idx);
	bvec.ov_vec.v_nr = b_idx;

	rc = m0_client_calculate_pi(pi, &seed, &bvec, flag, context, NULL);
	m0_bufvec_free2(&bvec);
	return rc;
}

static int target_ioreq_prepare_checksum(struct m0_op_io *ioo,
					 struct ioreq_fop *irfop,
					 struct m0_fop_cob_rw *rw_fop)
{
	int                               rc = 0;
	uint8_t                           cksum_type;
	uint8_t                           *b_addr;
	uint32_t                          idx;
	uint32_t                          num_units;
	uint32_t                          computed_cksm_nob = 0;
	uint32_t                          cksum_size;
	struct fop_cksum_data            *cs_data;
	struct fop_cksum_idx_data        *cs_idx_data;


	/* Get checksum size and type */
	cksum_size = m0__obj_di_cksum_size(ioo);
	cksum_type = m0__obj_di_cksum_type(ioo);
	if (cksum_type >= M0_PI_TYPE_MAX)
		return -EINVAL;

	/* Number of units will not be zero as its already checked */
	num_units = irfop->irf_cksum_data.cd_num_units;

	/**
	 * Note: No need to free this as RPC layer will free this
	 * Allocate cksum buffer for number of units added to target_ioreq ti
	 */
	if (m0_buf_alloc(&rw_fop->crw_di_data_cksum,
			 num_units * cksum_size) != 0)
		return -ENOMEM;

	/* Validate if FOP has any checksum to be sent */
	cs_data = &irfop->irf_cksum_data;
	b_addr =  rw_fop->crw_di_data_cksum.b_addr;
	for (idx = 0; idx < num_units; idx++) {
		cs_idx_data = &cs_data->cd_idx[idx];
		/* Valid data should be populated */
		if (cs_idx_data->ci_pg_idx == UINT32_MAX ||
		    cs_idx_data->ci_unit_idx == UINT32_MAX)
			return -EINVAL;
		/* For Parity Unit only Motr can generates checksum */
		if (m0__obj_is_di_cksum_gen_enabled(ioo) ||
		    (irfop->irf_pattr == PA_PARITY)) {
			/* Compute checksum for Unit */
			rc = m0_target_calculate_checksum(ioo, cksum_type,
							  irfop->irf_pattr,
							  cs_idx_data,
							  b_addr +
							  computed_cksm_nob);
			if (rc != 0) {
				m0_buf_free(&rw_fop->crw_di_data_cksum);
				return rc;
			}
		} else {
			/* Case where application is passing checksum */
			uint32_t unit_off;
			struct m0_pdclust_layout *play = pdlayout_get(ioo);
			unit_off = cs_idx_data->ci_pg_idx * layout_n(play) +
				   cs_idx_data->ci_unit_idx;
			if (unit_off >= ioo->ioo_attr.ov_vec.v_nr)
				return -EINVAL;
			memcpy(b_addr + computed_cksm_nob,
			       ioo->ioo_attr.ov_buf[unit_off], cksum_size);
		}
		computed_cksm_nob += cksum_size;
		if (computed_cksm_nob > rw_fop->crw_di_data_cksum.b_nob)
			return -EINVAL;
	}
	return rc;
}

/* This function will compute PG Index and Unit Index at given goff (object
 * offset index). Relation between them is shown below.
 * DATA	 Units  : Gob Offset - PGStart : PGStart + NxUS => PG Index - 0 : (N-1)
 * PARITY Units : Gob Offset - PGStart : PGStart + KxUS => PG Index - 0 : (K-1)
 */
static int target_ioreq_calc_idx(struct m0_op_io *ioo,
				  struct fop_cksum_idx_gbl_data *pgdata,
				  struct ioreq_fop *irfop,
				  struct m0_ivec_cursor *goff_cur,
				  uint32_t seg_start,
				  uint32_t seg_end)
{
	m0_bindex_t                      rem_pg_sz;
	struct fop_cksum_idx_data       *cs_idx;
	struct fop_cksum_data           *fop_cs_data = &irfop->irf_cksum_data;
	uint32_t                         seg;
	m0_bindex_t                      goff;
	struct m0_pdclust_layout        *play = pdlayout_get(ioo);
	m0_bcount_t                      grp0_idx;

	/**
	 * Loop through all the segments added and check & add
	 * Units spanning those segments to FOP
	 */
	for (seg = seg_start; seg <= seg_end; seg++) {
		if (!(seg % pgdata->fg_seg_per_unit)) {
			goff = m0_ivec_cursor_index(goff_cur);
			cs_idx =
				&fop_cs_data->cd_idx[fop_cs_data->cd_num_units];
			if (cs_idx->ci_pg_idx != UINT32_MAX ||
			    cs_idx->ci_unit_idx != UINT32_MAX)
				return -EINVAL;
			grp0_idx = pgdata->fg_pgrp0_index;
			/**
			 * There are scenarios where K > N in such case when the
			 * unit type is parity, shifting grp0 index by
			 * [(K-N) x pgrp0] for parity so that right PG and Unit
			 * index will get computed.
			 */
			if((irfop->irf_pattr == PA_PARITY) &&
			   (layout_k(play) > layout_n(play))) {
				grp0_idx += (layout_k(play) -
					     layout_n(play)) *
					     pgdata->fg_pgrp0_index;
			}

			/* Compute PG Index & remaining exta wrt PG boundary */
			cs_idx->ci_pg_idx = (goff - grp0_idx) /
					    pgdata->fg_pgrp_sz;
			rem_pg_sz = (goff - grp0_idx) %
				     pgdata->fg_pgrp_sz;
			cs_idx->ci_unit_idx = rem_pg_sz/pgdata->fg_unit_sz;
			fop_cs_data->cd_num_units++;
			if (fop_cs_data->cd_num_units >
			    fop_cs_data->cd_max_units)
				return -EINVAL;
			M0_LOG(M0_DEBUG,"FOP Unit Added Num:%d GOF:%" PRIi64
			       " [PG Idx:%" PRIu64 "][Unit Idx:%" PRIu64
			       "] Seg:%d", fop_cs_data->cd_num_units, goff,
			       cs_idx->ci_pg_idx + pgdata->fg_pi_grpid,
			       cs_idx->ci_unit_idx, seg);
		}
		m0_ivec_cursor_move(goff_cur, pgdata->fg_seg_sz);
	}
	return M0_RC(0);
}

/**
 * Assembles io fops for the specified target server.
 * This is heavily based on
 * m0t1fs/linux_kernel/file.c::target_ioreq_iofops_prepare
 *
 * @param ti The target io request whose data/parity fops should be assembled.
 * @param filter Whether to restrict the set of fops to prepare.
 * @return 0 for success, -errno otherwise.
 */
static int target_ioreq_iofops_prepare(struct target_ioreq *ti,
				       enum page_attr       filter)
{
	int                          rc = 0;
	uint32_t                     seg = 0;
	uint32_t                     seg_start;
	uint32_t                     seg_end;
	uint32_t                     sz_added_to_fop;
	uint32_t                     runt_sz;
	/* Number of segments in one m0_rpc_bulk_buf structure. */
	uint32_t                     bbsegs;
	uint32_t                     maxsize;
	uint32_t                     delta;
	uint32_t                     v_nr;
	uint32_t                     num_fops = 0;
	uint32_t                     num_units_iter = 0;
	enum page_attr               rw;
	enum page_attr              *pattr;
	struct m0_bufvec            *bvec;
	struct m0_bufvec            *auxbvec;
	struct m0_op_io             *ioo;
	struct m0_obj_attr          *io_attr;
	struct m0_indexvec          *ivec;
	struct ioreq_fop            *irfop;
	struct m0_net_domain        *ndom;
	struct m0_rpc_bulk_buf      *rbuf;
	struct m0_io_fop            *iofop;
	struct m0_fop_cob_rw        *rw_fop;
	struct nw_xfer_request      *xfer;
	/* Is it in the READ phase of WRITE request. */
	bool                         read_in_write = false;
	void                        *buf;
	void                        *bufnext;
	m0_bcount_t                  max_seg_size;
	m0_bcount_t                  xfer_len;
	m0_bindex_t                  offset;
	uint32_t                     segnext;
	uint32_t                     ndom_max_segs;
	struct m0_client            *instance;
	struct m0_ivec_cursor        goff_curr;
	uint32_t                     seg_sz;
	bool                         di_enabled;
	struct fop_cksum_idx_gbl_data pgdata;

	M0_ENTRY("prepare io fops for target ioreq %p filter 0x%x, tfid "FID_F,
		 ti, filter, FID_P(&ti->ti_fid));

	M0_PRE(target_ioreq_invariant(ti));
	M0_PRE(M0_IN(filter, (PA_DATA, PA_PARITY)));

	rc = m0_rpc_session_validate(ti->ti_session);
	if (rc != 0 && (!M0_IN(rc, (-ECANCELED, -EINVAL))))
		return M0_ERR(rc);

	xfer = ti->ti_nwxfer;
	ioo = bob_of(xfer, struct m0_op_io, ioo_nwxfer, &ioo_bobtype);
	M0_ASSERT(M0_IN(ioreq_sm_state(ioo),
			(IRS_READING, IRS_DEGRADED_READING,
			 IRS_WRITING, IRS_DEGRADED_WRITING)));

	if (M0_IN(ioo->ioo_oo.oo_oc.oc_op.op_code, (M0_OC_WRITE,
						    M0_OC_FREE)) &&
	    M0_IN(ioreq_sm_state(ioo), (IRS_READING, IRS_DEGRADED_READING)))
		read_in_write = true;

	if (M0_IN(ioreq_sm_state(ioo), (IRS_READING, IRS_WRITING))) {
		ivec    = &ti->ti_ivec;
		bvec    = &ti->ti_bufvec;
		auxbvec = &ti->ti_auxbufvec;
		pattr   = ti->ti_pageattrs;
	} else {
		if (ti->ti_dgvec == NULL) {
			return M0_RC(0);
		}
		ivec    = &ti->ti_dgvec->dr_ivec;
		bvec    = &ti->ti_dgvec->dr_bufvec;
		auxbvec = &ti->ti_dgvec->dr_auxbufvec;
		pattr   = ti->ti_dgvec->dr_pageattrs;
	}

	ndom = ti->ti_session->s_conn->c_rpc_machine->rm_tm.ntm_dom;
	rw = ioreq_sm_state(ioo) == IRS_DEGRADED_WRITING ? PA_DGMODE_WRITE :
	     ioreq_sm_state(ioo) == IRS_WRITING ? PA_WRITE :
	     ioreq_sm_state(ioo) == IRS_DEGRADED_READING ? PA_DGMODE_READ :
	     PA_READ;
	maxsize = m0_rpc_session_get_max_item_payload_size(ti->ti_session);

	max_seg_size = m0_net_domain_get_max_buffer_segment_size(ndom);

	ndom_max_segs = m0_net_domain_get_max_buffer_segments(ndom);

	di_enabled = m0__obj_is_di_enabled(ioo) &&
		     ti->ti_goff_ivec.iv_vec.v_nr;

	if (di_enabled) {
		struct m0_pdclust_layout *play = pdlayout_get(ioo);

		/* Init object global offset currsor for a given target */
		m0_ivec_cursor_init(&goff_curr, &ti->ti_goff_ivec);
		seg_sz = m0__page_size(ioo);
		pgdata.fg_seg_sz = m0__page_size(ioo);

		/* Assign all the data needed for index computation. */
		pgdata.fg_unit_sz = layout_unit_size(pdlayout_get(ioo));

		/**
		 * There are scenarios where K > N in such case when the
		 * filter is PA_PARITY, increase the size of PG so that
		 * right PG and Unit index will get computed based on goff
		 */
		if (filter == PA_DATA)
			pgdata.fg_pgrp_sz = layout_n(play);
		else
			pgdata.fg_pgrp_sz = layout_n(play) > layout_k(play)?
					    layout_n(play) : layout_k(play);
		/* Unit size multiplication will give PG size in bytes */
		pgdata.fg_pgrp_sz *= pgdata.fg_unit_sz;
		/* Assign pi_grpid for logging/debug */
		pgdata.fg_pi_grpid = ioo->ioo_iomaps[0]->pi_grpid;

		/* The offset of PG-0 */
		pgdata.fg_pgrp0_index = ioo->ioo_iomaps[0]->pi_grpid *
					layout_n(play) * pgdata.fg_unit_sz;
		/* Need to update PG & Unit index for every seg_per_unit */
		pgdata.fg_seg_per_unit = layout_unit_size(pdlayout_get(ioo)) /
					 seg_sz;

		v_nr = ti->ti_goff_ivec.iv_vec.v_nr;
		M0_LOG(M0_DEBUG,"RIW=%d PGStartOff:%"PRIu64
		       " GOFF-IOVEC StIdx: %"PRIi64 " EndIdx: %"PRIi64
		       " Vnr: %"PRIi32 " Count0: %"PRIi64 " CountEnd: %"PRIi64,
		       (int)read_in_write, pgdata.fg_pgrp0_index,
		       ti->ti_goff_ivec.iv_index[0],
		       ti->ti_goff_ivec.iv_index[v_nr-1], v_nr,
		       ti->ti_goff_ivec.iv_vec.v_count[0],
		       ti->ti_goff_ivec.iv_vec.v_count[v_nr-1]);
	} else {
		memset(&pgdata, 0, sizeof(pgdata));
		seg_sz = 0;
	}

	while (seg < SEG_NR(ivec)) {
		delta  = 0;
		bbsegs = 0;

		M0_LOG(M0_DEBUG, "pageattr = %u, filter = %u, rw = %u",
		       pattr[seg], filter, rw);

		if (!(pattr[seg] & filter) || !(pattr[seg] & rw) ||
		     (pattr[seg] & PA_TRUNC)) {
			++seg;
			/* goff_curr should be in sync with segment */
			if (di_enabled)
				m0_ivec_cursor_move(&goff_curr, seg_sz);
			continue;
		}

		M0_ALLOC_PTR(irfop);
		if (irfop == NULL) {
			rc = M0_ERR(-ENOMEM);
			goto err;
		}
		num_fops++;
		rc = ioreq_fop_init(irfop, ti, filter);
		if (rc != 0) {
			m0_free(irfop);
			goto err;
		}


		/* Init number of bytes added to fop and runt size */
		sz_added_to_fop = 0;
		/* Runt is for tracking bytes which are not accounted in unit */
		runt_sz = 0;
		iofop = &irfop->irf_iofop;
		rw_fop = io_rw_get(&iofop->if_fop);

		rc = bulk_buffer_add(irfop, ndom, &rbuf, &delta, maxsize);
		if (rc != 0) {
			ioreq_fop_fini(irfop);
			m0_free(irfop);
			goto err;
		}
		delta += io_seg_size();

		/**
		 * Adds io segments and io descriptor only if it fits within
		 * permitted size.
		 */
		/**
		 *  TODO: can this loop become a function call?
		 * -- too many levels of indentation
		 */
		while (seg < SEG_NR(ivec) &&
		       m0_io_fop_size_get(&iofop->if_fop) + delta < maxsize &&
		       bbsegs < ndom_max_segs) {
			/**
			* Adds a page to rpc bulk buffer only if it passes
			* through the filter.
			*/
			if (pattr[seg] & rw && pattr[seg] & filter &&
			    !(pattr[seg] & PA_TRUNC)) {
				delta += io_seg_size() + io_di_size(ioo);

				buf = buf_aux_chk_get(auxbvec, filter, seg,
						      read_in_write);

				if (buf == NULL)
					buf = bvec->ov_buf[seg];

				xfer_len = COUNT(ivec, seg);
				offset = INDEX(ivec, seg);

				seg_start = seg;
				/**
				 * Accommodate multiple pages in a single
				 * net buffer segment, if they are consecutive
				 * pages.
				 */
				segnext = seg + 1;
				while (segnext < SEG_NR(ivec) &&
				       xfer_len < max_seg_size) {
					bufnext = buf_aux_chk_get(auxbvec,
								  filter,
								  segnext,
								  read_in_write);
					if (bufnext == NULL)
						bufnext = bvec->ov_buf[segnext];

					if (buf + xfer_len == bufnext) {
						xfer_len += COUNT(ivec, ++seg);
						/**
						 * Next segment should be as per
						 * filter
						 */
						segnext = seg + 1;
						if (!(pattr[segnext] & filter) ||
						    !(pattr[segnext] & rw) ||
						    (pattr[segnext] & PA_TRUNC))
							break;
					} else
						break;
				}
				seg_end = seg;

				/* Get number of units added */
				if (di_enabled) {
					num_units_iter = (xfer_len + runt_sz) /
							  pgdata.fg_unit_sz;
					runt_sz = xfer_len % pgdata.fg_unit_sz;
					delta += m0__obj_di_cksum_size(ioo) *
						 num_units_iter;
				}

				rc = m0_rpc_bulk_buf_databuf_add(rbuf, buf,
								 xfer_len,
								 offset, ndom);

				if (rc == -EMSGSIZE) {
					/**
					 * Fix the number of segments in
					 * current m0_rpc_bulk_buf structure.
					 */
					rbuf->bb_nbuf->nb_buffer.ov_vec.v_nr =
						bbsegs;
					rbuf->bb_zerovec.z_bvec.ov_vec.v_nr =
						bbsegs;
					bbsegs = 0;

					delta -= (io_seg_size() +
						  io_di_size(ioo));
					/**
					 * In case of DI enabled delta will be
					 * adjusted otherwise num_units will
					 * be 0
					 */
					if (delta <= (num_units_iter *
					    m0__obj_di_cksum_size(ioo)))
						return -EINVAL;
					delta -= (num_units_iter *
						  m0__obj_di_cksum_size(ioo));

					/**
					 * Buffer must be 4k aligned to be
					 * used by network hw
					 */
					M0_ASSERT(addr_is_network_aligned(buf));
					rc     = bulk_buffer_add(irfop, ndom,
							&rbuf, &delta, maxsize);
					if (rc == -ENOSPC)
						break;
					else if (rc != 0)
						goto fini_fop;

					/**
					 * Since current bulk buffer is full,
					 * new bulk buffer is added and
					 * existing segment is attempted to
					 * be added to new bulk buffer.
					 */
					continue;
				} else if (rc == 0) {
					++bbsegs;
					sz_added_to_fop += xfer_len;
					if (di_enabled) {
						rc = target_ioreq_calc_idx(ioo,
								      &pgdata,
								      irfop,
								      &goff_curr,
								      seg_start,
								      seg_end);
						if (rc != 0)
							goto fini_fop;
					}
				}
			} else if (di_enabled)
				m0_ivec_cursor_move(&goff_curr, seg_sz);

			++seg;
		}

		if (m0_io_fop_byte_count(iofop) == 0) {
			irfop_fini(irfop);
			continue;
		}

		rbuf->bb_nbuf->nb_buffer.ov_vec.v_nr = bbsegs;
		rbuf->bb_zerovec.z_bvec.ov_vec.v_nr = bbsegs;

		rw_fop->crw_fid = ti->ti_fid;
		rw_fop->crw_pver = ioo->ioo_pver;
		rw_fop->crw_index = ti->ti_obj;

		/**
		 * Use NOHOLE by default (i.e. return error for missing
		 * units instead of zeros), unless we are in read-verify
		 * mode.
		 *
		 * Note: parity units are always present in the groups,
		 * so we always use NOHOLE for them.
		 */
		instance = m0__op_instance(&ioo->ioo_oo.oo_oc.oc_op);
		if (ioreq_sm_state(ioo) == IRS_READING && !read_in_write &&
		    (filter == PA_PARITY ||
		     (!instance->m0c_config->mc_is_read_verify &&
		      !(ioo->ioo_flags & M0_OOF_HOLE))))
			rw_fop->crw_flags |= M0_IO_FLAG_NOHOLE;

		if (ioreq_sm_state(ioo) == IRS_DEGRADED_READING &&
		    !read_in_write)
			rw_fop->crw_flags |= M0_IO_FLAG_NOHOLE;

		/* Clear FOP checksum data */
		rw_fop->crw_di_data_cksum.b_addr = NULL;
		rw_fop->crw_di_data_cksum.b_nob = 0;
		rw_fop->crw_cksum_size = 0;

		M0_LOG(M0_DEBUG, "FopNum = %d UnitSz: %d FopSz: %d Segment = %d",
		       num_fops, pgdata.fg_unit_sz, sz_added_to_fop, seg);

		di_enabled = di_enabled && irfop->irf_cksum_data.cd_num_units;

		/* Assign the checksum buffer for traget */
		if (di_enabled && m0_is_write_fop(&iofop->if_fop)) {
			rw_fop->crw_cksum_size = m0__obj_di_cksum_size(ioo);
			/**
			 * Prepare checksum data for parity as not parity buffer
			 * are populated
			 */
			if (target_ioreq_prepare_checksum(ioo, irfop,
							  rw_fop) != 0) {
				rw_fop->crw_di_data_cksum.b_addr = NULL;
				rw_fop->crw_di_data_cksum.b_nob  = 0;
				rw_fop->crw_cksum_size = 0;
				goto fini_fop;
			}
		}

		if (di_enabled && m0_is_read_fop(&iofop->if_fop) &&
		    !read_in_write) {
			/* Server side expects this to be valid if checksum is
			 * to be read */
			rw_fop->crw_cksum_size = m0__obj_di_cksum_size(ioo);
			M0_LOG(M0_DEBUG,"Read FOP enabling checksum");
		}

		if (ioo->ioo_flags & M0_OOF_SYNC)
			rw_fop->crw_flags |= M0_IO_FLAG_SYNC;
		io_attr = m0_io_attr(ioo);
		rw_fop->crw_lid = io_attr->oa_layout_id;

		/*
		 * XXX(Sining): This is a bit tricky: m0_io_fop_prepare in
		 * ioservice/io_fops.c calls io_fop_di_prepare which has only
		 * file system in mind and uses super block and file related
		 * information to do something (it returns 0 directly for user
		 * space). This is not the case for Client kernel mode!!
		 *
		 * Simply return 0 just like it does for user space at this
		 * moment.
		 */
		rc = m0_io_fop_prepare(&iofop->if_fop);
		if (rc != 0)
			goto fini_fop;

		if (m0_is_read_fop(&iofop->if_fop))
			m0_atomic64_add(&xfer->nxr_rdbulk_nr,
					m0_rpc_bulk_buf_length(
					&iofop->if_rbulk));

		m0_atomic64_inc(&ti->ti_nwxfer->nxr_iofop_nr);
		iofops_tlist_add(&ti->ti_iofops, irfop);

		M0_LOG(M0_DEBUG,
		       "fop=%p bulk=%p (%s) @"FID_F" io fops = %"PRIu64
		       " read bulks = %" PRIu64 ", list_len=%d",
		       &iofop->if_fop, &iofop->if_rbulk,
		       m0_is_read_fop(&iofop->if_fop) ? "r" : "w",
		       FID_P(&ti->ti_fid),
		       m0_atomic64_get(&xfer->nxr_iofop_nr),
		       m0_atomic64_get(&xfer->nxr_rdbulk_nr),
		       (int)iofops_tlist_length(&ti->ti_iofops));
	}

	return M0_RC(0);

fini_fop:
	irfop_fini(irfop);
err:
	m0_tl_teardown(iofops, &ti->ti_iofops, irfop) {
		irfop_fini(irfop);
	}

	return M0_ERR(rc);
}
static int target_cob_fop_prepare(struct target_ioreq *ti);
static const struct target_ioreq_ops tioreq_ops = {
	.tio_seg_add         = target_ioreq_seg_add,
	.tio_iofops_prepare  = target_ioreq_iofops_prepare,
	.tio_cc_fops_prepare = target_cob_fop_prepare,
};

static int target_cob_fop_prepare(struct target_ioreq *ti)
{
	int rc;
	M0_ENTRY("ti = %p type = %d", ti, ti->ti_req_type);
	M0_PRE(M0_IN(ti->ti_req_type, (TI_COB_CREATE, TI_COB_TRUNCATE)));

	rc = ioreq_cc_fop_init(ti);
	return M0_RC(rc);
}

/**
 * Initialises a target io request.
 * This is heavily based on m0t1fs/linux_kernel/file.c::target_ioreq_init
 *
 * @param ti[out] The target io request to initialise.
 * @param xfer The corresponding network transfer request.
 * @param cobfid The fid of the cob this request will act on.
 * @param ta_obj Which object in the global layout the cobfid corresponds to.
 * @param session The rpc session that should be used to send this request.
 * @param size The size of the request in bytes.
 * @return 0 for success, -errno otherwise.
 */
static int target_ioreq_init(struct target_ioreq    *ti,
			     struct nw_xfer_request *xfer,
			     const struct m0_fid    *cobfid,
			     uint64_t                ta_obj,
			     struct m0_rpc_session  *session,
			     uint64_t                size)
{
	int                     rc;
	struct m0_op_io        *ioo;
	struct m0_op           *op;
	struct m0_client       *instance;
	uint32_t                nr;

	M0_PRE(cobfid  != NULL);
	M0_ENTRY("target_ioreq %p, nw_xfer_request %p, "FID_F,
		 ti, xfer, FID_P(cobfid));

	M0_PRE(ti      != NULL);
	M0_PRE(xfer    != NULL);
	M0_PRE(session != NULL);
	M0_PRE(size    >  0);

	ti->ti_rc        = 0;
	ti->ti_ops       = &tioreq_ops;
	ti->ti_fid       = *cobfid;
	ti->ti_nwxfer    = xfer;
	ti->ti_dgvec     = NULL;
	ti->ti_req_type  = TI_NONE;
	M0_SET0(&ti->ti_cc_fop);
	ti->ti_cc_fop_inited = false;

	/*
	 * Target object is usually in ONLINE state unless explicitly
	 * told otherwise.
	 */
	ti->ti_state     = M0_PNDS_ONLINE;
	ti->ti_session   = session;
	ti->ti_parbytes  = 0;
	ti->ti_databytes = 0;

	ioo      = bob_of(xfer, struct m0_op_io, ioo_nwxfer,
			  &ioo_bobtype);
	op       = &ioo->ioo_oo.oo_oc.oc_op;
	instance = m0__op_instance(op);
	M0_PRE(instance != NULL);

	ti->ti_obj = ta_obj;

	iofops_tlist_init(&ti->ti_iofops);
	tioreqht_tlink_init(ti);
	target_ioreq_bob_init(ti);

	nr = page_nr(size, ioo->ioo_obj);
	rc = m0_indexvec_alloc(&ti->ti_ivec, nr);
	if (rc != 0)
		goto out;

	if (op->op_code == M0_OC_FREE) {
		rc = m0_indexvec_alloc(&ti->ti_trunc_ivec, nr);
		if (rc != 0)
			goto fail;
	}

	/**
	 * Allocating index vector to track object global offset
	 * which is getting sent to a target. This will help to
	 * compute the PG Index and Unit Index which are being
	 * sent to target
	 */
	rc = m0_indexvec_alloc(&ti->ti_goff_ivec, nr);
	ti->ti_goff_ivec.iv_vec.v_nr = 0;
	if (rc != 0)
		goto fail;

	ti->ti_bufvec.ov_vec.v_nr = nr;
	M0_ALLOC_ARR(ti->ti_bufvec.ov_vec.v_count, nr);
	if (ti->ti_bufvec.ov_vec.v_count == NULL)
		goto fail;

	M0_ALLOC_ARR(ti->ti_bufvec.ov_buf, nr);
	if (ti->ti_bufvec.ov_buf == NULL)
		goto fail;

	/**
	 * For READOLD method, an extra bufvec is needed to remember
	 * the addresses of auxillary buffers so those auxillary
	 * buffers can be used in rpc bulk transfer to avoid polluting
	 * real data buffers which are the application's memory for IO
	 * in case zero copy method is in use.
	 */
	ti->ti_auxbufvec.ov_vec.v_nr = nr;
	M0_ALLOC_ARR(ti->ti_auxbufvec.ov_vec.v_count, nr);
	if (ti->ti_auxbufvec.ov_vec.v_count == NULL)
		goto fail;

	M0_ALLOC_ARR(ti->ti_auxbufvec.ov_buf, nr);
	if (ti->ti_auxbufvec.ov_buf == NULL)
		goto fail;

	if (M0_FI_ENABLED("no-mem-err"))
		goto fail;
	M0_ALLOC_ARR(ti->ti_pageattrs, nr);
	if (ti->ti_pageattrs == NULL)
		goto fail;

	/*
	 * This value is incremented when new segments are added to the
	 * index vector in target_ioreq_seg_add().
	 */
	ti->ti_ivec.iv_vec.v_nr = 0;
	ti->ti_trunc_ivec.iv_vec.v_nr = 0;

	M0_POST_EX(target_ioreq_invariant(ti));
	return M0_RC(0);
fail:
	m0_indexvec_free(&ti->ti_ivec);
	if (op->op_code == M0_OC_FREE)
		m0_indexvec_free(&ti->ti_trunc_ivec);
	m0_indexvec_free(&ti->ti_goff_ivec);
	m0_free(ti->ti_bufvec.ov_vec.v_count);
	m0_free(ti->ti_bufvec.ov_buf);
	m0_free(ti->ti_auxbufvec.ov_vec.v_count);
	m0_free(ti->ti_auxbufvec.ov_buf);

out:
	return M0_ERR(-ENOMEM);
}

/**
 * Retrieves (possibly allocating and initialising) a target io request for the
 * provided network transfer requests.
 * This is heavily based on m0t1fs/linux_kernel/file.c::nw_xfer_tioreq_get
 *
 * @param xfer The network transfer.
 * @param fid The cob fid that the request will operate on.
 * @param ta_obj Which object in the global layout the cobfid corresponds to.
 * @param session The session the request will be sent on.
 * @param size The size of the request.
 * @param out[out] The discovered (or allocated) target io request.
 */
static int nw_xfer_tioreq_get(struct nw_xfer_request *xfer,
			      struct m0_fid          *fid,
			      uint64_t                ta_obj,
			      struct m0_rpc_session  *session,
			      uint64_t                size,
			      struct target_ioreq   **out)
{
	int                     rc = 0;
	struct target_ioreq    *ti;
	struct m0_op_io        *ioo;
	struct m0_op           *op;
	struct m0_client       *instance;

	M0_PRE(fid != NULL);
	M0_ENTRY("nw_xfer_request %p, "FID_F, xfer, FID_P(fid));

	M0_PRE(session != NULL);
	M0_PRE(out != NULL);
	M0_PRE(nw_xfer_request_invariant(xfer));

	ioo = bob_of(xfer, struct m0_op_io, ioo_nwxfer, &ioo_bobtype);
	op = &ioo->ioo_oo.oo_oc.oc_op;
	instance = m0__op_instance(op);
	M0_PRE(instance != NULL);

	ti = target_ioreq_locate(xfer, fid);
	if (ti == NULL) {
		M0_ALLOC_PTR(ti);
		if (ti == NULL)
			return M0_ERR(-ENOMEM);

		rc = target_ioreq_init(ti, xfer, fid, ta_obj, session, size);
		if (rc == 0) {
			tioreqht_htable_add(&xfer->nxr_tioreqs_hash, ti);
			M0_LOG(M0_INFO, "New target_ioreq %p added for "FID_F,
			                ti, FID_P(fid));
		} else {
			m0_free(ti);
			return M0_ERR_INFO(rc, "target_ioreq_init() failed");
		}
	}

	if (ti->ti_dgvec == NULL && M0_IN(ioreq_sm_state(ioo),
		(IRS_DEGRADED_READING, IRS_DEGRADED_WRITING)))
		rc = dgmode_rwvec_alloc_init(ti);

	*out = ti;

	return M0_RC(rc);
}

/**
 * Sets @iomap databufs within a data unit @ext to the degraded write mode.
 * The unit boundary is ensured by the calling code at nw_xfer_io_distribute().
 */
static void databufs_set_dgw_mode(struct pargrp_iomap *iomap,
				  struct m0_pdclust_layout *play,
				  struct m0_ext *ext)
{
	uint32_t         row_start;
	uint32_t         row_end;
	uint32_t         row;
	uint32_t         col;
	m0_bcount_t      grp_off;
	struct data_buf *dbuf;

	grp_off = data_size(play) * iomap->pi_grpid;
	page_pos_get(iomap, ext->e_start, grp_off, &row_start, &col);
	page_pos_get(iomap, ext->e_end - 1, grp_off, &row_end, &col);

	for (row = row_start; row <= row_end; ++row) {
		dbuf = iomap->pi_databufs[row][col];
		if (dbuf->db_flags & PA_WRITE)
			dbuf->db_flags |= PA_DGMODE_WRITE;
	}
}

/**
 * Sets @iomap paritybufs for the parity @unit to the degraded write mode.
 */
static void paritybufs_set_dgw_mode(struct pargrp_iomap *iomap,
				    struct m0_op_io *ioo,
				    uint64_t unit)
{
	uint32_t                  row;
	uint32_t                  col;
	struct data_buf          *dbuf;
	struct m0_pdclust_layout *play = pdlayout_get(ioo);
	uint64_t                  unit_size = layout_unit_size(play);

	parity_page_pos_get(iomap, unit * unit_size, &row, &col);
	for (; row < rows_nr(play, ioo->ioo_obj); ++row) {
		dbuf = iomap->pi_paritybufs[row][col];
		if (m0_pdclust_is_replicated(play) &&
		    iomap->pi_databufs[row][0] == NULL)
			continue;
		if (dbuf->db_flags & PA_WRITE)
			dbuf->db_flags |= PA_DGMODE_WRITE;
	}
}

/**
 * Distributes file data into target_ioreq objects as required and populates
 * target_ioreq::ti_ivec and target_ioreq::ti_bufvec.
 * This is heavily based on m0t1fs/linux_kernel/file.c::nw_xfer_io_distribute
 *
 * @param xfer The network transfer request.
 * @return 0 for success, -errno otherwise.
 */
static int nw_xfer_io_distribute(struct nw_xfer_request *xfer)
{
	bool                        do_cobs = true;
	int                         rc = 0;
	unsigned int                op_code;
	uint64_t                    i;
	uint64_t                    unit;
	uint64_t                    unit_size;
	uint64_t                    count;
	uint64_t                    pgstart;
	struct m0_op               *op;
	/* Extent representing a data unit. */
	struct m0_ext               u_ext;
	/* Extent representing resultant extent. */
	struct m0_ext               r_ext;
	/* Extent representing a segment from index vector. */
	struct m0_ext               v_ext;
	struct m0_op_io            *ioo;
	struct target_ioreq        *ti;
	struct m0_ivec_cursor       cursor;
	struct m0_pdclust_layout   *play;
	enum m0_pdclust_unit_type   unit_type;
	struct m0_pdclust_src_addr  src;
	struct m0_pdclust_tgt_addr  tgt;
	struct m0_bitmap            units_spanned;
	struct pargrp_iomap        *iomap;
	struct m0_client           *instance;

	M0_ENTRY("nw_xfer_request %p", xfer);

	M0_PRE(nw_xfer_request_invariant(xfer));

	ioo       = bob_of(xfer, struct m0_op_io, ioo_nwxfer, &ioo_bobtype);
	op        = &ioo->ioo_oo.oo_oc.oc_op;
	op_code   = op->op_code,
	play      = pdlayout_get(ioo);
	unit_size = layout_unit_size(play);
	instance  = m0__op_instance(op);

	/*
	 * In non-oostore mode, all cobs are created on object creation.
	 * In oostore mode, CROW is enabled and cobs are created automatically
	 * at the server side on the 1st write request. But, because of SNS,
	 * we need to create cobs for the spare units, and to make sure all cobs
	 * are created for all units in the parity group touched by the update
	 * request. See more below.
	 */
	if (!m0__is_oostore(instance) || op_code == M0_OC_READ)
		do_cobs = false;
	/*
	 * In replicated layout (N == 1), all units in the parity group are
	 * always spanned. And there are no spare units, so...
	 */
	if (ioo->ioo_pbuf_type == M0_PBUF_IND)
		do_cobs = false;

	if (do_cobs) {
		rc = m0_bitmap_init(&units_spanned, m0_pdclust_size(play));
		if (rc != 0)
			return M0_ERR(rc);
	}

	for (i = 0; i < ioo->ioo_iomap_nr; ++i) {
		count        = 0;
		iomap        = ioo->ioo_iomaps[i];
		pgstart      = data_size(play) * iomap->pi_grpid;
		src.sa_group = iomap->pi_grpid;

		M0_LOG(M0_DEBUG, "xfer=%p map=%p [grpid=%" PRIu64 " state=%u]",
				 xfer, iomap, iomap->pi_grpid, iomap->pi_state);

		if (do_cobs)
			m0_bitmap_reset(&units_spanned);

		/* traverse parity group ivec by units */
		m0_ivec_cursor_init(&cursor, &iomap->pi_ivec);
		while (!m0_ivec_cursor_move(&cursor, count)) {
			unit = (m0_ivec_cursor_index(&cursor) - pgstart) /
				unit_size;

			u_ext.e_start = pgstart + unit * unit_size;
			u_ext.e_end   = u_ext.e_start + unit_size;

			v_ext.e_start  = m0_ivec_cursor_index(&cursor);
			v_ext.e_end    = v_ext.e_start +
				m0_ivec_cursor_step(&cursor);

			m0_ext_intersection(&u_ext, &v_ext, &r_ext);
			M0_ASSERT(m0_ext_is_valid(&r_ext));
			count = m0_ext_length(&r_ext);

			unit_type = m0_pdclust_unit_classify(play, unit);
			M0_ASSERT(unit_type == M0_PUT_DATA);

			if (ioreq_sm_state(ioo) == IRS_DEGRADED_WRITING)
				databufs_set_dgw_mode(iomap, play, &r_ext);

			src.sa_unit = unit;
			rc = xfer->nxr_ops->nxo_tioreq_map(xfer, &src, &tgt,
							   &ti);
			if (rc != 0)
				goto err;

			ti->ti_ops->tio_seg_add(ti, &src, &tgt, r_ext.e_start,
						m0_ext_length(&r_ext), iomap);
			if (op_code == M0_OC_WRITE && do_cobs &&
			    ti->ti_req_type == TI_READ_WRITE)
				m0_bitmap_set(&units_spanned, unit, true);

		}

		M0_ASSERT(ergo(M0_IN(op_code, (M0_OC_READ, M0_OC_WRITE)),
			       m0_vec_count(&ioo->ioo_ext.iv_vec) ==
			       m0_vec_count(&ioo->ioo_data.ov_vec)));

		/* process parity units */
		if (M0_IN(ioo->ioo_pbuf_type, (M0_PBUF_DIR,
					       M0_PBUF_IND)) ||
		    (ioreq_sm_state(ioo) == IRS_DEGRADED_READING &&
		     iomap->pi_state == PI_DEGRADED)) {

			for (unit = 0; unit < layout_k(play); ++unit) {
				src.sa_unit = layout_n(play) + unit;
				M0_ASSERT(m0_pdclust_unit_classify(play,
					  src.sa_unit) == M0_PUT_PARITY);

				rc = xfer->nxr_ops->nxo_tioreq_map(xfer, &src,
								   &tgt, &ti);
				if (rc != 0)
					goto err;

				if (ioreq_sm_state(ioo) == IRS_DEGRADED_WRITING)
					paritybufs_set_dgw_mode(iomap, ioo,
								unit);

				if (op_code == M0_OC_WRITE && do_cobs)
					m0_bitmap_set(&units_spanned,
						      src.sa_unit, true);

				ti->ti_ops->tio_seg_add(ti, &src, &tgt, pgstart,
							layout_unit_size(play),
							iomap);
			}

			if (!do_cobs)
				continue; /* to next iomap */

			/*
			 * Create cobs for all units not spanned by the
			 * IO request (data or spare units).
			 *
			 * If some data unit is not present in the group (hole
			 * or not complete last group), we still need to create
			 * cob for it. Otherwise, during SNS-repair the receiver
			 * will wait forever for this unit without knowing that
			 * its size is actually zero.
			 */
			for (unit = 0; unit < m0_pdclust_size(play); ++unit) {
				if (m0_bitmap_get(&units_spanned, unit))
					continue;

				src.sa_unit = unit;
				rc = xfer->nxr_ops->nxo_tioreq_map(xfer, &src,
								   &tgt, &ti);
				if (rc != 0)
					M0_LOG(M0_ERROR, "[%p] map=%p "
					       "nxo_tioreq_map() failed: rc=%d",
					       ioo, iomap, rc);
				/*
				 * Skip the case when some other parity group
				 * has spanned the particular target already.
				 */
				if (ti->ti_req_type != TI_NONE)
					continue;

				ti->ti_req_type = TI_COB_CREATE;
			}
		}
	}

	if (do_cobs)
		m0_bitmap_fini(&units_spanned);

	M0_ASSERT(ergo(M0_IN(op_code, (M0_OC_READ, M0_OC_WRITE)),
		       m0_vec_count(&ioo->ioo_ext.iv_vec) ==
		       m0_vec_count(&ioo->ioo_data.ov_vec)));

	return M0_RC(0);
err:
	m0_htable_for(tioreqht, ti, &xfer->nxr_tioreqs_hash) {
		tioreqht_htable_del(&xfer->nxr_tioreqs_hash, ti);
		target_ioreq_fini(ti);
		m0_free0(&ti);
	} m0_htable_endfor;

	return M0_ERR(rc);
}

/**
 * Completes all the target io requests in a network transfer request. Collects
 * the total number of bytes read/written, and determines the final return code.
 * This is heavily based on m0t1fs/linux_kernel/file.c::nw_xfer_req_complete
 * Call with ioo->ioo_sm.sm_grp locked.
 *
 * @param xfer The network transfer request.
 * @param rmw Whether this request was part of a multiple requests (rmw).
 */
static void nw_xfer_req_complete(struct nw_xfer_request *xfer, bool rmw)
{
	struct m0_client       *instance;
	struct m0_op_io        *ioo;
	struct target_ioreq    *ti;
	struct ioreq_fop       *irfop;
	struct m0_fop          *fop;
	struct m0_rpc_item     *item;

	M0_ENTRY("nw_xfer_request %p, rmw %s", xfer,
		 rmw ? (char *)"true" : (char *)"false");

	M0_PRE(xfer != NULL);
	xfer->nxr_state = NXS_COMPLETE;
	ioo = bob_of(xfer, struct m0_op_io, ioo_nwxfer, &ioo_bobtype);
	M0_PRE(m0_sm_group_is_locked(ioo->ioo_sm.sm_grp));

	instance = m0__op_instance(m0__ioo_to_op(ioo));
	/*
 	 * Ignore the following invariant check as there exists cases in which
 	 * io fops are created sucessfully for some target services but fail
 	 * for some services in nxo_dispatch (for example, session/connection
 	 * to a service is invalid, resulting a 'dirty' op in which
 	 * nr_iofops != 0 and nxr_state == NXS_COMPLETE.
 	 *
	 * M0_PRE_EX(m0_op_io_invariant(ioo));
	 */

	m0_htable_for(tioreqht, ti, &xfer->nxr_tioreqs_hash) {

		/* Maintains only the first error encountered. */
		if (xfer->nxr_rc == 0)
			xfer->nxr_rc = ti->ti_rc;

		xfer->nxr_bytes += ti->ti_databytes;
		ti->ti_databytes = 0;

		if (m0__is_oostore(instance) &&
		    ti->ti_req_type == TI_COB_CREATE &&
		    ioreq_sm_state(ioo) == IRS_WRITE_COMPLETE) {
			ti->ti_req_type = TI_NONE;
			continue;
		}

		if (m0__is_oostore(instance) &&
		    ti->ti_req_type == TI_COB_TRUNCATE &&
		    ioreq_sm_state(ioo) == IRS_TRUNCATE_COMPLETE) {
			ti->ti_req_type = TI_NONE;
		}

		m0_tl_teardown(iofops, &ti->ti_iofops, irfop) {
			fop = &irfop->irf_iofop.if_fop;
			item = m0_fop_to_rpc_item(fop);
			M0_LOG(M0_DEBUG, "[%p] fop %p, ref %llu, "
			       "item %p[%u], ri_error %d, ri_state %d",
			       ioo, fop,
			       (unsigned long long)m0_ref_read(&fop->f_ref),
			       item, item->ri_type->rit_opcode, item->ri_error,
			       item->ri_sm.sm_state);

			M0_ASSERT(ergo(item->ri_sm.sm_state !=
				       M0_RPC_ITEM_UNINITIALISED,
				       item->ri_rmachine != NULL));
			if (item->ri_rmachine == NULL) {
				M0_ASSERT(ti->ti_session != NULL);
				m0_fop_rpc_machine_set(fop,
					ti->ti_session->s_conn->c_rpc_machine);
			}

			M0_LOG(M0_DEBUG,
			       "[%p] item %p, target fid "FID_F"fop %p, "
			       "ref %llu", ioo, item, FID_P(&ti->ti_fid), fop,
			       (unsigned long long)m0_ref_read(&fop->f_ref));
			m0_fop_put_lock(fop);
		}

	} m0_htable_endfor;

	/** XXX morse: there are better ways of determining whether this is a
	 * read request */
	M0_LOG(M0_INFO, "Number of bytes %s = %"PRIu64,
	       ioreq_sm_state(ioo) == IRS_READ_COMPLETE ? "read" : "written",
	       xfer->nxr_bytes);

	/*
	 * This function is invoked from 4 states - IRS_READ_COMPLETE,
	 * IRS_WRITE_COMPLETE, IRS_DEGRADED_READING, IRS_DEGRADED_WRITING.
	 * And the state change is applicable only for healthy state IO,
	 * meaning for states IRS_READ_COMPLETE and IRS_WRITE_COMPLETE.
	 */
	if (M0_IN(ioreq_sm_state(ioo),
		  (IRS_READ_COMPLETE, IRS_WRITE_COMPLETE,
		   IRS_TRUNCATE_COMPLETE))) {
		if (!rmw)
			ioreq_sm_state_set_locked(ioo, IRS_REQ_COMPLETE);
		else if (ioreq_sm_state(ioo) == IRS_READ_COMPLETE)
			xfer->nxr_bytes = 0;
	}

	/*
	 * nxo_dispatch may fail if connections to services have not been
	 * established yet. In this case, ioo_rc contains error code and
	 * xfer->nxr_rc == 0, don't overwrite ioo_rc.
	 *
	 * TODO: merge this with op->op_sm.sm_rc ?
	 */
	if (xfer->nxr_rc != 0)
		ioo->ioo_rc = xfer->nxr_rc;

	M0_LEAVE();
}

/**
 * Prepares each target io request in the network transfer requests, and
 * submit the fops.
 * This is heavily based on m0t1fs/linux_kernel/file.c::nw_xfer_req_dispatch
 *
 * @param xfer The network transfer request.
 * @return 0 for success, -errno otherwise.
 */
static int nw_xfer_req_dispatch(struct nw_xfer_request *xfer)
{
	int                     rc = 0;
	int                     post_error = 0;
	int                     ri_error;
	uint64_t                nr_dispatched = 0;
	struct ioreq_fop       *irfop;
	struct m0_op_io        *ioo;
	struct m0_op           *op;
	struct target_ioreq    *ti;
	struct m0_client       *instance;
	int                     non_rpc_post_error = 0;

	M0_ENTRY();

	M0_PRE(xfer != NULL);
	ioo = bob_of(xfer, struct m0_op_io, ioo_nwxfer, &ioo_bobtype);
	op = &ioo->ioo_oo.oo_oc.oc_op;
	instance = m0__op_instance(op);
	M0_PRE(instance != NULL);

	to_op_io_map(op, ioo);

	/* FOPs' preparation */
	m0_htable_for(tioreqht, ti, &xfer->nxr_tioreqs_hash) {
		if (ti->ti_state != M0_PNDS_ONLINE) {
			M0_LOG(M0_INFO, "Skipped iofops prepare for "FID_F,
			       FID_P(&ti->ti_fid));
			continue;
		}
		ti->ti_start_time = m0_time_now();
		if (ti->ti_req_type == TI_COB_CREATE &&
		    ioreq_sm_state(ioo) == IRS_WRITING) {
			rc = ti->ti_ops->tio_cc_fops_prepare(ti);
			if (rc != 0)
				return M0_ERR_INFO(rc, "[%p] cob create fop"
						   "failed", ioo);
			continue;
		}

		if (ioreq_sm_state(ioo) == IRS_TRUNCATE) {
		    if (ti->ti_req_type == TI_READ_WRITE) {
			ti->ti_req_type = TI_COB_TRUNCATE;
			rc = ti->ti_ops->tio_cc_fops_prepare(ti);
			if (rc != 0)
				return M0_ERR(rc);
			}
			continue;
		}
		rc = ti->ti_ops->tio_iofops_prepare(ti, PA_DATA) ?:
		     ti->ti_ops->tio_iofops_prepare(ti, PA_PARITY);
		if (rc != 0)
			return M0_ERR(rc);
	} m0_htable_endfor;

	/* Submit io FOPs */
	m0_htable_for(tioreqht, ti, &xfer->nxr_tioreqs_hash) {
		struct m0_rpc_item *item = &ti->ti_cc_fop.crf_fop.f_item;

		/* Skips the target device if it is not online. */
		if (ti->ti_state != M0_PNDS_ONLINE) {
			M0_LOG(M0_INFO, "Skipped device "FID_F,
			       FID_P(&ti->ti_fid));
			continue;
		}
		if (ti->ti_req_type == TI_COB_CREATE &&
		    ioreq_sm_state(ioo) == IRS_WRITING) {
			M0_LOG(M0_DEBUG, "item="ITEM_FMT" osr_xid=%"PRIu64,
				ITEM_ARG(item), item->ri_header.osr_xid);
			rc = m0_rpc_post(item);
			/*
			 * An error returned by rpc post has been ignored.
			 * It will be handled in the respective bottom half.
			 */
			if (rc == 0)
				M0_CNT_INC(nr_dispatched);
			else {
				post_error = rc;
				rc = 0;
			}
			m0_op_io_to_rpc_map(ioo, item);
			continue;
		}
		if (op->op_code == M0_OC_FREE &&
		    ioreq_sm_state(ioo) == IRS_TRUNCATE &&
		    ti->ti_req_type == TI_COB_TRUNCATE) {
			if (ti->ti_trunc_ivec.iv_vec.v_nr > 0) {
				M0_LOG(M0_DEBUG, "item="ITEM_FMT
						 " osr_xid=%"PRIu64,
						 ITEM_ARG(item),
						 item->ri_header.osr_xid);
				rc = m0_rpc_post(item);
				/*
				 * An error returned by rpc post has been ignored.
			         * It will be handled in the respective bottom half.
			         */
				if (rc == 0)
					M0_CNT_INC(nr_dispatched);
				else {
					post_error = rc;
					rc = 0;
				}
				m0_op_io_to_rpc_map(ioo, item);
			}
			continue;
		}
		m0_tl_for (iofops, &ti->ti_iofops, irfop) {
			rc = ioreq_fop_async_submit(&irfop->irf_iofop,
						    ti->ti_session);
			ri_error = irfop->irf_iofop.if_fop.f_item.ri_error;
			M0_LOG(M0_DEBUG, "[%p] Submitted fop for device "
			       FID_F"@%p, item %p, fop_nr=%llu, rc=%d, "
			       "ri_error=%d", ioo, FID_P(&ti->ti_fid), irfop,
			       &irfop->irf_iofop.if_fop.f_item,
			       (unsigned long long)
			       m0_atomic64_get(&xfer->nxr_iofop_nr),
			       rc, ri_error);

			/* XXX: noisy */
			m0_op_io_to_rpc_map(ioo,
					&irfop->irf_iofop.if_fop.f_item);

			if (rc != 0) {
				/* This will only occur for rpc_bulk error,
				 * continue dispatch operations after updating
				 * the error response */
				ti->ti_rc = ti->ti_rc ?: rc;
				xfer->nxr_rc = xfer->nxr_rc ?: rc;
				non_rpc_post_error = rc;
				rc = 0;
			} else {
				m0_atomic64_inc(&instance->m0c_pending_io_nr);
				if (ri_error == 0)
					M0_CNT_INC(nr_dispatched);
				else if (post_error == 0)
					post_error = ri_error;
			}
		} m0_tl_endfor;
	} m0_htable_endfor;

	if (rc == 0 && nr_dispatched == 0 && post_error == 0 &&
	    non_rpc_post_error != 0) {
		/* No fop has been dispatched, bulk error has been detected,
		 * dispatch can fail immediately with error
		 */
		rc = non_rpc_post_error;
	} else if (rc == 0 && nr_dispatched == 0 && post_error == 0) {
		/* No fop has been dispatched.
		 *
		 * This might happen in dgmode reading:
		 *    In 'parity verify' mode, a whole parity group, including
		 *    data and parity units are all read from ioservices.
		 *    If some units failed to read, no need to read extra unit.
		 *    The units needed for recovery are ready.
		 */
		M0_ASSERT(ioreq_sm_state(ioo) == IRS_DEGRADED_READING);
		if (op->op_code == M0_OC_READ &&
		    instance->m0c_config->mc_is_read_verify) {
			M0_LOG(M0_DEBUG, "As per design in Read verify mode");
		} else {
			M0_LOG(M0_ERROR, "More than K targets are offline");
			rc = -EIO;
		}

		ioreq_sm_state_set_locked(ioo, IRS_READ_COMPLETE);
	} else if (rc == 0)
		xfer->nxr_state = NXS_INFLIGHT;
	M0_LOG(M0_DEBUG, "[%p] nxr_iofop_nr %llu, nxr_rdbulk_nr %llu, "
	       "nr_dispatched %llu", ioo,
	       (unsigned long long)m0_atomic64_get(&xfer->nxr_iofop_nr),
	       (unsigned long long)m0_atomic64_get(&xfer->nxr_rdbulk_nr),
	       (unsigned long long)nr_dispatched);

	return M0_RC(rc);
}

/**
 * should_spare_be_mapped() decides whether given IO request should be
 * redirected to the spare unit device or not.
 *
 * For normal IO, M0_IN(ioreq_sm_state, (IRS_READING, IRS_WRITING)),
 * such redirection is not needed, with the exception of read IO case
 * when the failed device is in REPAIRED state.
 *
 * Note: req->ir_sns_state is used only to differentiate between two
 *       possible use cases during the degraded mode write.
 *
 * Here are possible combinations of different parameters on which
 * the decision is made.
 *
 * Input parameters:
 *
 * - State of IO request.
 *   Sample set {IRS_DEGRADED_READING, IRS_DEGRADED_WRITING}
 *
 * - State of current device.
 *   Sample set {M0_PNDS_SNS_REPAIRING, M0_PNDS_SNS_REPAIRED}
 *
 * - State of SNS repair process with respect to current global fid.
 *   Sample set {SRS_REPAIR_DONE, SRS_REPAIR_NOTDONE}
 *
 * Degraded read case (IRS_DEGRADED_READING):
 *
 * 1. device_state == M0_PNDS_SNS_REPAIRING
 *
 *    Not redirected. The extent is assigned to the failed device itself
 *    but it is filtered at the level of io_req_fop.
 *
 * 2. device_state == M0_PNDS_SNS_REPAIRED
 *
 *    Redirected.
 *
 * Degraded write case (IRS_DEGRADED_WRITING):
 *
 * 1. device_state == M0_PNDS_SNS_REPAIRED
 *
 *    Redirected.
 *
 * 2. device_state == M0_PNDS_SNS_REPAIRING &&
 *    req->ir_sns_state == SRS_REPAIR_DONE
 *
 *    Redirected. Repair is finished for the current global fid.
 *
 * 3. device_state == M0_PNDS_SNS_REPAIRING &&
 *    req->ir_sns_state == SRS_REPAIR_NOTDONE
 *
 *    Not redirected. Repair is not finished for this global fid yet.
 *    So we just drop all pages directed towards the failed device.
 *    The data will be restored by SNS-repair in the due time later.
 *
 * 4. device_state == M0_PNDS_SNS_REPAIRED &&
 *    req->ir_sns_state == SRS_REPAIR_NOTDONE
 *
 *    This should not be possible.
 */
static bool should_spare_be_mapped(struct m0_op_io *ioo,
				   enum m0_pool_nd_state dev_state)
{
	return (M0_IN(ioreq_sm_state(ioo),
		      (IRS_READING, IRS_DEGRADED_READING)) &&
		dev_state == M0_PNDS_SNS_REPAIRED)
	                          ||
	       (ioreq_sm_state(ioo) == IRS_DEGRADED_WRITING &&
		(dev_state == M0_PNDS_SNS_REPAIRED ||
		 (dev_state == M0_PNDS_SNS_REPAIRING &&
		  ioo->ioo_sns_state == SRS_REPAIR_DONE)));

}

/**
 * Determines which targets (spare or not) of the io map the network transfer
 * requests should be mapped to.
 * This is heavily based on m0t1fs/linux_kernel/file.c::nw_xfer_tioreq_map
 *
 * @param xfer The network transfer request.
 * @param src unit address in the parity groups.
 * @param tgt[out] unit address in the target devices.
 * @param tio[out] The retrieved (or allocated) target request.
 * @return 0 for success, -errno otherwise.
 */
static int nw_xfer_tioreq_map(struct nw_xfer_request           *xfer,
			      const struct m0_pdclust_src_addr *src,
			      struct m0_pdclust_tgt_addr       *tgt,
			      struct target_ioreq             **tio)
{
	int                         rc;
	struct m0_fid               tfid;
	const struct m0_fid        *gfid;
	struct m0_op_io            *ioo;
	struct m0_rpc_session      *session;
	struct m0_pdclust_layout   *play;
	struct m0_pdclust_instance *play_instance;
	enum m0_pool_nd_state       dev_state;
	enum m0_pool_nd_state       dev_state_prev;
	uint32_t                    spare_slot;
	uint32_t                    spare_slot_prev;
	struct m0_pdclust_src_addr  spare;
	struct m0_poolmach         *pm;

	M0_ENTRY("nw_xfer_request=%p", xfer);

	M0_PRE(nw_xfer_request_invariant(xfer));
	M0_PRE(src != NULL);
	M0_PRE(tgt != NULL);
	M0_PRE(tio != NULL);

	ioo = bob_of(xfer, struct m0_op_io, ioo_nwxfer, &ioo_bobtype);

	play = pdlayout_get(ioo);
	M0_PRE(play != NULL);
	play_instance = pdlayout_instance(layout_instance(ioo));
	M0_PRE(play_instance != NULL);

	spare = *src;
	m0_fd_fwd_map(play_instance, src, tgt);
	tfid = target_fid(ioo, tgt);
	M0_LOG(M0_DEBUG, "src_id[%" PRIu64 ":%" PRIu64 "] -> "
			 "dest_id[%" PRIu64 ":%" PRIu64 "] @ tfid="FID_F,
	       src->sa_group, src->sa_unit, tgt->ta_frame, tgt->ta_obj,
	       FID_P(&tfid));

	pm = ioo_to_poolmach(ioo);
	M0_ASSERT(pm != NULL);
	rc = m0_poolmach_device_state(pm, tgt->ta_obj, &dev_state);
	if (rc != 0)
		return M0_RC(rc);

	if (M0_FI_ENABLED("poolmach_client_repaired_device1")) {
		if (tfid.f_container == 1)
			dev_state = M0_PNDS_SNS_REPAIRED;
	}

	M0_LOG(M0_INFO, "[%p] tfid="FID_F" dev_state=%d\n",
	                 ioo, FID_P(&tfid), dev_state);

	if (should_spare_be_mapped(ioo, dev_state)) {
		gfid = &ioo->ioo_oo.oo_fid;
		rc = m0_sns_repair_spare_map(pm, gfid, play, play_instance,
					     src->sa_group, src->sa_unit,
					     &spare_slot, &spare_slot_prev);
		if (rc != 0)
			return M0_RC(rc);

		/* Check if there is an effective-failure. */
		if (spare_slot_prev != src->sa_unit) {
			spare.sa_unit = spare_slot_prev;
			m0_fd_fwd_map(play_instance, &spare, tgt);
			tfid = target_fid(ioo, tgt);
			rc = m0_poolmach_device_state(pm, tgt->ta_obj,
						      &dev_state_prev);
			if (rc != 0)
				return M0_RC(rc);
		} else
			dev_state_prev = M0_PNDS_SNS_REPAIRED;

		if (dev_state_prev == M0_PNDS_SNS_REPAIRED) {
			spare.sa_unit = spare_slot;
			m0_fd_fwd_map(play_instance, &spare, tgt);
			tfid = target_fid(ioo, tgt);
		}
		dev_state = dev_state_prev;
	}

	session = target_session(ioo, tfid);

	rc = nw_xfer_tioreq_get(xfer, &tfid, tgt->ta_obj, session,
			layout_unit_size(play) * ioo->ioo_iomap_nr, tio);

	if (M0_IN(ioreq_sm_state(ioo), (IRS_DEGRADED_READING,
					IRS_DEGRADED_WRITING)) &&
	    dev_state != M0_PNDS_SNS_REPAIRED)
		(*tio)->ti_state = dev_state;

	return M0_RC(rc);
}

static const struct nw_xfer_ops xfer_ops = {
	.nxo_distribute = nw_xfer_io_distribute,
	.nxo_complete   = nw_xfer_req_complete,
	.nxo_dispatch   = nw_xfer_req_dispatch,
	.nxo_tioreq_map = nw_xfer_tioreq_map,
};

M0_INTERNAL void nw_xfer_request_init(struct nw_xfer_request *xfer)
{
	uint64_t                  bucket_nr;
	struct m0_op_io          *ioo;
	struct m0_pdclust_layout *play;

	M0_ENTRY("nw_xfer_request : %p", xfer);

	M0_PRE(xfer != NULL);

	ioo = bob_of(xfer, struct m0_op_io, ioo_nwxfer, &ioo_bobtype);
	nw_xfer_request_bob_init(xfer);
	xfer->nxr_rc        = 0;
	xfer->nxr_bytes     = 0;
	m0_atomic64_set(&xfer->nxr_iofop_nr, 0);
	m0_atomic64_set(&xfer->nxr_rdbulk_nr, 0);
	xfer->nxr_state     = NXS_INITIALIZED;
	xfer->nxr_ops       = &xfer_ops;
	m0_mutex_init(&xfer->nxr_lock);

	play = pdlayout_get(ioo);
	bucket_nr = layout_n(play) + 2 * layout_k(play);
	xfer->nxr_rc = tioreqht_htable_init(&xfer->nxr_tioreqs_hash,
					    bucket_nr);

	M0_POST_EX(nw_xfer_request_invariant(xfer));
	M0_LEAVE();
}

M0_INTERNAL void nw_xfer_request_fini(struct nw_xfer_request *xfer)
{
	M0_ENTRY("nw_xfer_request : %p", xfer);

	M0_PRE(xfer != NULL);
	M0_PRE(M0_IN(xfer->nxr_state, (NXS_COMPLETE, NXS_INITIALIZED)));
	M0_PRE(nw_xfer_request_invariant(xfer));
	M0_LOG(M0_DEBUG, "nw_xfer_request : %p, nxr_rc = %d",
	       xfer, xfer->nxr_rc);

	xfer->nxr_ops = NULL;
	m0_mutex_fini(&xfer->nxr_lock);
	nw_xfer_request_bob_fini(xfer);
	tioreqht_htable_fini(&xfer->nxr_tioreqs_hash);

	M0_LEAVE();
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
