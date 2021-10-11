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


#define M0_TRACE_SUBSYSTEM M0_TRACE_SUBSYS_CONF
#include "lib/trace.h"
#include "lib/finject.h"

#include "conf/db.h"
#include "conf/onwire.h"     /* m0_confx_obj, m0_confx */
#include "conf/onwire_xc.h"
#include "conf/obj.h"
#include "xcode/xcode.h"
#include "btree/btree.h"
#include "be/tx.h"
#include "be/op.h"
#include "lib/memory.h"      /* m0_alloc, m0_free */
#include "lib/errno.h"       /* EINVAL */
#include "lib/misc.h"        /* M0_SET0 */

static int confdb_objs_count(struct m0_btree *btree, size_t *result);
static void confdb_table_fini(struct m0_be_seg *seg);

struct __pack {
	struct m0_xcode_type  _p_xt;
	struct m0_xcode_field _p_field[M0_CONF_OBJ_TYPE_MAX];
} conx_obj = {
	._p_xt = {
		.xct_name = "m0_confx_obj",
	},
};

M0_BASSERT(&conx_obj._p_xt.xct_child[0] == &conx_obj._p_field[0]);

M0_INTERNAL struct m0_xcode_type *m0_confx_obj_xc = &conx_obj._p_xt;

M0_INTERNAL void m0_xc_m0_confx_obj_struct_init(void)
{}

M0_INTERNAL void m0_xc_m0_confx_obj_struct_fini(void)
{}

M0_INTERNAL size_t m0_confx_sizeof(void)
{
	return m0_confx_obj_xc->xct_sizeof;
}

static void *
_conf_xcode_alloc(struct m0_xcode_cursor *ctx M0_UNUSED, size_t nob)
{
	return m0_alloc(nob);
}

static void confx_to_xcode_obj(struct m0_confx_obj *xobj,
			       struct m0_xcode_obj *out, bool allocated)
{
	*out = M0_XCODE_OBJ(m0_confx_obj_xc, allocated ? xobj : NULL);
}

/* Note: m0_xcode_ctx_init() doesn't allow `xobj' to be const. Sigh. */
static void xcode_ctx_init(struct m0_xcode_ctx *ctx, struct m0_confx_obj *xobj,
			   bool allocated)
{
	struct m0_xcode_obj obj;

	M0_ENTRY();

	confx_to_xcode_obj(xobj, &obj, allocated);
	m0_xcode_ctx_init(ctx, &obj);
	if (!allocated)
		ctx->xcx_alloc = _conf_xcode_alloc;

	M0_LEAVE();
}

static int confx_obj_measure(struct m0_confx_obj *xobj)
{
	struct m0_xcode_ctx ctx;

	M0_ENTRY();
	xcode_ctx_init(&ctx, xobj, true);
	return M0_RC(m0_xcode_length(&ctx));
}

/* ------------------------------------------------------------------
 * Database operations
 * ------------------------------------------------------------------
 */

/**
 * Contains BE segment memory allocation details. This preallocated
 * BE segment memory is used to allocate configuration objects through
 * xcode. This avoids blocking calls to BE allocator through xcode.
 * The BE segment memory allocated to confx_allocator::a_chunk has to be
 * released as a whole and cannot be released in pieces (i.e. allocated to
 * configuration objects). Thus the confx_allocator is also added to the
 * configuration btree along with the struct m0_confx_obj. This helps in
 * releasing the memory as part of the db destruction.
 */
struct confx_allocator {
	void        *a_chunk;
	m0_bcount_t  a_total;
	m0_bcount_t  a_used;
};

/**
 * Maintains BE segment allocation details for configuration objects along with
 * the configuration object to be allocated on the BE segment through xcode.
 * Object of struct confx_obj_ctx is added to the configuration btree as a
 * whole.
 */
struct confx_obj_ctx {
	struct confx_allocator  oc_alloc;
	struct m0_confx_obj    *oc_obj;
};

/**
 * Maintains reference to configuration object allocator along with xcode ctx.
 * struct confx_ctx::c_xcxtx is used to build configuration objects in BE
 * segment memory which is accessed through struct confx_ctx::c_alloc.
 */
struct confx_ctx {
	struct confx_allocator *c_alloc;
	struct m0_xcode_ctx     c_xctx;
};

/* ------------------------------------------------------------------
 * Tables
 * ------------------------------------------------------------------ */

static const char     btree_name[] = "conf";
static const uint32_t rnode_sz     = 4096;

static int confdb_table_init(struct m0_be_seg     *seg,
			     struct m0_btree      *btree,
			     struct m0_be_tx      *tx,
			     const struct m0_fid  *btree_fid)
{
	uint8_t                    *rnode;
	struct m0_btree_op          b_op = {};
	struct m0_btree_type        bt;
	uint32_t                    rnode_sz_shift;
	int                         rc;
	struct m0_btree_rec_key_op  keycmp = {
					.rko_keycmp = (void *)&m0_fid_cmp,
					};

	M0_ENTRY();

	M0_ASSERT(rnode_sz > 0 && m0_is_po2(rnode_sz));
	rnode_sz_shift = __builtin_ffsl(rnode_sz) - 1;
	M0_BE_ALLOC_ALIGN_ARR_SYNC(rnode, rnode_sz, rnode_sz_shift, seg, tx);
	bt = (struct m0_btree_type){ .tt_id = M0_BT_CONFDB,
				     .ksize = sizeof (struct m0_fid),
				     .vsize = -1,
				   };
	rc = M0_BTREE_OP_SYNC_WITH_RC(&b_op, m0_btree_create(rnode, rnode_sz,
							     &bt, &b_op, btree,
							     seg, btree_fid,
							     tx, &keycmp));
	if (rc != 0)
		M0_BE_FREE_ALIGN_ARR_SYNC(rnode, seg, tx);
	else {
		rc = m0_be_seg_dict_insert(seg, tx, btree_name, rnode);
		if (rc == 0 && M0_FI_ENABLED("ut_confdb_create_failure"))
			rc = -EINVAL;
		if (rc != 0) {
			M0_BTREE_OP_SYNC_WITH_RC(&b_op,
						 m0_btree_close(btree, &b_op));
			m0_confdb_destroy(seg, tx);
		}
	}

	return M0_RC(rc);
}

static void confdb_table_fini(struct m0_be_seg *seg)
{
}

static void *confdb_obj_alloc(struct m0_xcode_cursor *ctx, size_t nob)
{
	struct confx_ctx       *cctx;
	struct confx_allocator *alloc;
	char                   *addr;

	cctx = container_of(container_of(ctx, struct m0_xcode_ctx, xcx_it),
			    struct confx_ctx, c_xctx);
	alloc = cctx->c_alloc;
	M0_PRE(alloc->a_chunk != NULL);
	M0_PRE(alloc->a_used + nob <= alloc->a_total);

	addr = (char *)alloc->a_chunk + alloc->a_used;
	alloc->a_used += nob;

	return addr;
}

static int confx_obj_dup(struct confx_allocator *alloc,
			 struct m0_confx_obj **dest,
			 struct m0_confx_obj *src)
{
	int                 rc = 0;
	struct m0_xcode_obj src_obj;
	struct m0_xcode_obj dest_obj;
	struct m0_xcode_ctx *sctx = NULL;
	struct confx_ctx    *cctx = NULL;

	M0_ALLOC_PTR(sctx);
	M0_ALLOC_PTR(cctx);
	if (sctx == NULL || cctx == NULL)
		goto out;

	confx_to_xcode_obj(src, &dest_obj, false);
	confx_to_xcode_obj(src, &src_obj, true);
	if (M0_FI_ENABLED("ut_confx_obj_dup_failure"))
		rc = -EINVAL;
	if (rc == 0) {
		m0_xcode_ctx_init(sctx, &src_obj);
		m0_xcode_ctx_init(&cctx->c_xctx, &dest_obj);
		cctx->c_alloc = alloc;
		cctx->c_xctx.xcx_alloc = confdb_obj_alloc;
		rc = m0_xcode_dup(&cctx->c_xctx, sctx);
	}
	*dest = cctx->c_xctx.xcx_it.xcu_stack[0].s_obj.xo_ptr;
 out:
	m0_free(cctx);
	m0_free(sctx);
	return M0_RC(rc);
}

M0_INTERNAL int m0_confdb_create_credit(struct m0_be_seg *seg,
					const struct m0_confx *conf,
					struct m0_be_tx_credit *accum)
{
	int                   rc = 0;
	int                   i;
	uint32_t              rnode_sz_shift;
	struct m0_btree_type  bt;

	M0_ENTRY();

	M0_ASSERT(rnode_sz > 0 && m0_is_po2(rnode_sz));
	rnode_sz_shift = __builtin_ffsl(rnode_sz) - 1;
	bt = (struct m0_btree_type){ .tt_id = M0_BT_CONFDB,
				     .ksize = sizeof (struct m0_fid),
				     .vsize = -1,
				   };

	m0_be_allocator_credit(NULL, M0_BAO_ALLOC_ALIGNED, rnode_sz,
			       rnode_sz_shift, accum);
	m0_be_seg_dict_insert_credit(seg, btree_name, accum);
	m0_btree_create_credit(&bt, accum, 1);

	for (i = 0; i < conf->cx_nr; ++i) {
		struct m0_confx_obj *obj;

		obj = M0_CONFX_AT(conf, i);
		rc = confx_obj_measure(obj);
		if (rc < 0)
			break;
		m0_btree_put_credit2(&bt, rnode_sz, 1, sizeof(struct m0_fid),
				     rc + sizeof(struct confx_allocator),
				     accum);
		m0_btree_del_credit2(&bt, rnode_sz, 1, sizeof(struct m0_fid),
				     rc + sizeof(struct confx_allocator),
				     accum);
		rc = 0;
	}

	/* Allocate credits for db finalisation in-case of an error. */
	m0_confdb_destroy_credit(seg, accum);

	return M0_RC(rc);
}

static int confdb_alloc(struct confx_allocator *alloc, struct m0_be_seg *seg,
			struct m0_be_tx *tx, int size)
{
	M0_BE_OP_SYNC(__op,
			m0_be_alloc(m0_be_seg_allocator(seg), tx, &__op,
				&alloc->a_chunk, size));
	if (alloc->a_chunk == NULL)
		return M0_ERR(-ENOMEM);
	alloc->a_total = size;
	alloc->a_used  = 0;

	return 0;
}

static m0_bcount_t conf_sizeof(const struct m0_confx *conf)
{
	m0_bcount_t size = 0;
	int         i;
	int         rc;

	for (i = 0; i < conf->cx_nr; ++i) {
		struct m0_confx_obj *obj;

		obj = M0_CONFX_AT(conf, i);
		rc = confx_obj_measure(obj);
		if (rc < 0) {
			size = 0;
			break;
		}
		size += m0_confx_sizeof() + rc;
	}

	return size;
}

static int confx_allocator_init(struct confx_allocator *alloc,
				const struct m0_confx *conf,
				struct m0_be_seg *seg, struct m0_be_tx *tx)
{
	m0_bcount_t conf_size;
	return (conf_size = conf_sizeof(conf)) == 0 ? -EINVAL :
		confdb_alloc(alloc, seg, tx, conf_size);
}

static int confd_btree_kv_put_cb(struct m0_btree_cb  *cb,
				 struct m0_btree_rec *rec)
{
	struct m0_btree_rec     *put_data = cb->c_datum;
	struct m0_bufvec_cursor  scur;
	struct m0_bufvec_cursor  dcur;
	m0_bcount_t              ksize;
	m0_bcount_t              vsize;

	ksize = m0_vec_count(&put_data->r_key.k_data.ov_vec);
	vsize = m0_vec_count(&put_data->r_val.ov_vec);
	if (ksize > m0_vec_count(&rec->r_key.k_data.ov_vec) ||
	    vsize > m0_vec_count(&rec->r_val.ov_vec))
		return M0_ERR(ENOSPC);

	m0_bufvec_cursor_init(&scur, &put_data->r_key.k_data);
	m0_bufvec_cursor_init(&dcur, &rec->r_key.k_data);
	m0_bufvec_cursor_copy(&dcur, &scur, ksize);

	m0_bufvec_cursor_init(&scur, &put_data->r_val);
	m0_bufvec_cursor_init(&dcur, &rec->r_val);
	m0_bufvec_cursor_copy(&dcur, &scur, vsize);

	return 0;
}

M0_INTERNAL int m0_confdb_create(struct m0_be_seg      *seg,
				 struct m0_be_tx       *tx,
				 const struct m0_confx *conf,
				 const struct m0_fid   *btree_fid)
{
	struct m0_btree         btree;
	struct confx_allocator  alloc;
	int                     i;
	int                     rc;
	struct m0_btree_op      kv_op = {};
	struct m0_btree_op      b_op  = {};
	struct m0_btree_cb      put_cb;

	M0_ENTRY();
	M0_PRE(conf->cx_nr > 0);

	rc = confdb_table_init(seg, &btree, tx, btree_fid);
	if (rc != 0)
		return M0_RC(rc);
	rc = confx_allocator_init(&alloc, conf, seg, tx);
	for (i = 0; i < conf->cx_nr && rc == 0; ++i) {
		struct confx_obj_ctx  obj_ctx;
		m0_bcount_t           ksize;
		m0_bcount_t           vsize;
		void                 *k_ptr;
		void                 *v_ptr;
		struct m0_btree_rec   rec;

		/*
		 * Save confx_allocator information along with the configuration
		 * object in the btree.
		 * Allocator information is replicated with all the configuration
		 * objects in the btree.
		 */
		obj_ctx.oc_alloc = alloc;
		rc = confx_obj_dup(&alloc, &obj_ctx.oc_obj, M0_CONFX_AT(conf, i));
		if (rc != 0)
			break;
		M0_ASSERT(obj_ctx.oc_obj != NULL);
		/* discard const */

		k_ptr = (struct m0_fid *)m0_conf_objx_fid(obj_ctx.oc_obj);
		ksize = sizeof (struct m0_fid);

		v_ptr = &obj_ctx;
		vsize = m0_confx_sizeof() + sizeof(struct confx_allocator);

		rec.r_key.k_data   = M0_BUFVEC_INIT_BUF(&k_ptr, &ksize);
		rec.r_val          = M0_BUFVEC_INIT_BUF(&v_ptr, &vsize);

		put_cb.c_act       = confd_btree_kv_put_cb;
		put_cb.c_datum     = &rec;
		rc = M0_BTREE_OP_SYNC_WITH_RC(&kv_op,
					      m0_btree_put(&btree, &rec,
							   &put_cb, &kv_op,
							   tx));
		if (rc != 0) {
			/** Delete all objects added to the btree. */
			while (i--) {
				if (confx_obj_dup(&alloc, &obj_ctx.oc_obj,
						  M0_CONFX_AT(conf, i)) != 0)
					continue;

				k_ptr = (struct m0_fid *)
					     m0_conf_objx_fid(obj_ctx.oc_obj);
				M0_BTREE_OP_SYNC_WITH_RC(&kv_op,
							m0_btree_del(&btree,
								     &rec.r_key,
								     NULL,
								     &kv_op,
								     tx));
			}
		}
	}
	M0_BTREE_OP_SYNC_WITH_RC(&b_op, m0_btree_close(&btree, &b_op));
	if (rc != 0) {
		m0_confdb_destroy(seg, tx);
	}

	return M0_RC(rc);
}

M0_INTERNAL void m0_confdb_destroy_credit(struct m0_be_seg *seg,
					  struct m0_be_tx_credit *accum)
{
	uint8_t              *rnode;
	int                   rc;
	struct m0_btree_type  bt    = { .tt_id = M0_BT_CONFDB,
					.ksize = sizeof (struct m0_fid),
					.vsize = -1,
				      };
	uint32_t              rnode_sz_shift;

	M0_ENTRY();

	rc = m0_be_seg_dict_lookup(seg, btree_name, (void **)&rnode);
	if (rc == 0)
		m0_btree_destroy_credit(NULL, &bt, accum, 1);
	else
		/** Use the same credit count as btree_create. */
		m0_btree_create_credit(&bt, accum, 1);
	m0_be_seg_dict_delete_credit(seg, btree_name, accum);

	M0_ASSERT(rnode_sz > 0 && m0_is_po2(rnode_sz));
	rnode_sz_shift = __builtin_ffsl(rnode_sz) - 1;

	m0_be_allocator_credit(NULL, M0_BAO_ALLOC_ALIGNED, rnode_sz,
			       rnode_sz_shift, accum);

	M0_LEAVE();
}

static int __confdb_free(struct m0_btree *btree, struct m0_be_seg *seg,
			 struct m0_be_tx *tx)
{
	int                        rc;
	struct confx_allocator    *alloc = NULL;
	struct confx_obj_ctx      *obj_ctx;
	struct m0_buf              key;
	struct m0_buf              val;
	struct m0_btree_cursor     bcur;

	m0_btree_cursor_init(&bcur, btree);
	rc = m0_btree_cursor_first(&bcur);
	if (rc != 0)
		goto err;
	m0_btree_cursor_kv_get(&bcur, &key, &val);
	/**
	 * @todo check validity of key and record addresses and
	 * sizes. Specifically, check that val.b_addr points to an
	 * allocated region in a segment with appropriate size and
	 * alignment. Such checks should be done generally by (not
	 * existing) beobj interface.
	 *
	 * @todo also check that key (fid) matches m0_conf_objx_fid().
	 */
	obj_ctx = val.b_addr;
	/*
	 * Fetch the confx allocator information from the first configuration
	 * object. Release pre-allocated BE segment memory from the allocator.
	 */
	alloc = &obj_ctx->oc_alloc;
	M0_BE_FREE_PTR_SYNC(alloc->a_chunk, seg, tx);
err:
	m0_btree_cursor_fini(&bcur);

	return M0_RC(rc);
}

M0_INTERNAL int m0_confdb_destroy(struct m0_be_seg *seg,
				  struct m0_be_tx *tx)
{
	uint8_t         *rnode;
	struct m0_btree  btree;
	struct m0_btree_op  b_op = {};
	int                 rc;

	M0_ENTRY();

	rc = m0_be_seg_dict_lookup(seg, btree_name, (void **)&rnode);
	if (rc == 0) {
		rc = M0_BTREE_OP_SYNC_WITH_RC(&b_op,
					      m0_btree_open(rnode, rnode_sz,
							    &btree, seg,
							    &b_op, NULL));
		if (rc == 0) {
			rc = __confdb_free(&btree, seg, tx);
			if (rc == 0 || rc == -ENOENT) {
				M0_BTREE_OP_SYNC_WITH_RC(&b_op,
						m0_btree_destroy(&btree, &b_op,
								 tx));
				M0_BE_FREE_ALIGN_ARR_SYNC(rnode, seg, tx);
				rc = m0_be_seg_dict_delete(seg, tx, btree_name);
			}
		}
	}

	return M0_RC(rc);
}

M0_INTERNAL void m0_confdb_fini(struct m0_be_seg *seg)
{
	confdb_table_fini(seg);
}

static int confdb_objs_count(struct m0_btree *btree, size_t *result)
{
	struct m0_btree_cursor    bcur;
	int                       rc;

	M0_ENTRY();
	*result = 0;
	m0_btree_cursor_init(&bcur, btree);
	for (rc = m0_btree_cursor_first(&bcur); rc == 0;
	     rc = m0_btree_cursor_next(&bcur)) {
		++*result;
	}
	m0_btree_cursor_fini(&bcur);
	/* Check for normal iteration completion. */
	if (rc == -ENOENT)
		rc = 0;
	return M0_RC(rc);
}

static struct m0_confx *confx_alloc(size_t nr_objs)
{
	struct m0_confx *ret;
	void            *data;

	M0_PRE(nr_objs > 0);

	M0_ALLOC_PTR(ret);
	if (ret == NULL)
		return NULL;

	M0_ALLOC_ARR(data, nr_objs * m0_confx_sizeof());
	if (data == NULL) {
		m0_free(ret);
		return NULL;
	}
	ret->cx__objs = data;
	ret->cx_nr = nr_objs;
	return ret;
}

static void confx_fill(struct m0_confx *dest, struct m0_btree *btree)
{
	struct m0_btree_cursor    bcur;
	size_t                    i; /* index in dest->cx__objs[] */
	int                       rc;

	M0_ENTRY();
	M0_PRE(dest->cx_nr > 0);

	m0_btree_cursor_init(&bcur, btree);
	for (i = 0, rc = m0_btree_cursor_first(&bcur); rc == 0;
	     rc = m0_btree_cursor_next(&bcur), ++i) {
		struct confx_obj_ctx *obj_ctx;
		struct m0_buf         key;
		struct m0_buf         val;

		m0_btree_cursor_kv_get(&bcur, &key, &val);
		M0_ASSERT(i < dest->cx_nr);
		/**
		 * @todo check validity of key and record addresses and
		 * sizes. Specifically, check that val.b_addr points to an
		 * allocated region in a segment with appropriate size and
		 * alignment. Such checks should be done generally by (not
		 * existing) beobj interface.
		 *
		 * @todo also check that key (fid) matches m0_conf_objx_fid().
		 */
		obj_ctx = val.b_addr;
		memcpy(M0_CONFX_AT(dest, i), obj_ctx->oc_obj, m0_confx_sizeof());
	}
	m0_btree_cursor_fini(&bcur);
	/** @todo handle iteration errors. */
	M0_ASSERT(rc == -ENOENT); /* end of the table */
}

M0_INTERNAL int m0_confdb_read(struct m0_be_seg *seg, struct m0_confx **out)
{
	uint8_t                    *rnode;
	struct m0_btree             btree;
	int                         rc;
	size_t                      nr_objs = 0;
	struct m0_btree_op          b_op    = {};
	struct m0_btree_rec_key_op  keycmp  = {
					.rko_keycmp = (void *) &m0_fid_cmp,
					};

	M0_ENTRY();

	rc = m0_be_seg_dict_lookup(seg, btree_name, (void **)&rnode);
	if (rc != 0)
		return M0_RC(rc);

	rc = M0_BTREE_OP_SYNC_WITH_RC(&b_op,
				      m0_btree_open(rnode, rnode_sz, &btree,
						    seg, &b_op, &keycmp));
	if (rc != 0)
		return M0_RC(rc);

	rc = confdb_objs_count(&btree, &nr_objs);
	if (rc != 0)
		goto out;

	if (nr_objs == 0) {
		rc = -ENODATA;
		goto out;
	}

	*out = confx_alloc(nr_objs);
	if (*out == NULL) {
		rc = -ENOMEM;
		goto out;
	}

	confx_fill(*out, &btree);
out:
	M0_BTREE_OP_SYNC_WITH_RC(&b_op, m0_btree_close(&btree, &b_op));
	return M0_RC(rc);
}

#undef M0_TRACE_SUBSYSTEM
