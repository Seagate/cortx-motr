/* -*- C -*- */
/*
 * Copyright (c) 2016-2021 Seagate Technology LLC and/or its Affiliates
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


#define M0_TRACE_SUBSYSTEM M0_TRACE_SUBSYS_CAS

#include "lib/trace.h"
#include "lib/memory.h"
#include "lib/finject.h"
#include "lib/assert.h"
#include "lib/errno.h"               /* ENOMEM, EPROTO */
#include "lib/ext.h"                 /* m0_ext */
#include "be/domain.h"               /* m0_be_domain_seg_first */
#include "be/op.h"
#include "module/instance.h"
#include "fop/fom_long_lock.h"       /* m0_long_lock */
#include "cas/ctg_store.h"
#include "cas/index_gc.h"


struct m0_ctg_store {
	/** The part of a catalogue store persisted on a disk. */
	struct m0_cas_state *cs_state;

	/** Mutex to protect cs_state counters updates. */
	struct m0_mutex      cs_state_mutex;

	/** Catalogue-index catalogue. */
	struct m0_cas_ctg   *cs_ctidx;

	/**
	 * Records from cs_meta are moved there on idx delete.
	 * To be used by deleted index garbage collector.
	 */
	struct m0_cas_ctg   *cs_dead_index;

	/**
	 * "Delete" lock to exclude possible race, where repair/re-balance
	 * service sends the old value concurrently with record deletion.
	 * @see m0_ctg_del_lock().
	 */
	struct m0_long_lock  cs_del_lock;

	/**
	 * Reference counter for number of catalogue store users.
	 * When it drops to 0, catalogue store structure is finalised.
	 */
	struct m0_ref        cs_ref;

	/**
	 * BE domain the catalogue store is working with.
	 */
	struct m0_be_domain *cs_be_domain;

	/**
	 * Flag indicating whether catalogue store is initialised or not.
	 */
	bool                 cs_initialised;
};

enum cursor_phase {
	CPH_NONE = 0,
	CPH_INIT,
	CPH_GET,
	CPH_NEXT
};

static struct m0_be_seg *cas_seg(struct m0_be_domain *dom);

static int  ctg_buf          (const struct m0_buf *val, struct m0_buf *buf);
static int  ctg_buf_get      (struct m0_buf *dst, const struct m0_buf *src,
			      bool enabled_fi);
static void ctg_fid_key_fill (void *key, const struct m0_fid *fid);
static void ctg_init         (struct m0_cas_ctg *ctg, struct m0_be_seg *seg);
static void ctg_open         (struct m0_cas_ctg *ctg, struct m0_be_seg *seg);
static void ctg_fini         (struct m0_cas_ctg *ctg);
static void ctg_destroy      (struct m0_cas_ctg *ctg, struct m0_be_tx *tx);
static int  ctg_meta_selfadd (struct m0_btree *meta, struct m0_be_tx *tx);
static void ctg_meta_delete  (struct m0_btree  *meta,
			      const struct m0_fid *fid,
			      struct m0_be_tx     *tx);
static void ctg_meta_selfrm  (struct m0_btree *meta, struct m0_be_tx *tx);

static void ctg_meta_insert_credit   (struct m0_btree_type   *bt,
				      struct m0_be_seg       *seg,
				      m0_bcount_t             nr,
				      struct m0_be_tx_credit *accum);
static void ctg_meta_delete_credit   (struct m0_btree_type   *bt,
				      struct m0_be_seg       *seg,
				      m0_bcount_t             nr,
				      struct m0_be_tx_credit *accum);
static void ctg_store_init_creds_calc(struct m0_be_seg       *seg,
				      struct m0_cas_state    *state,
				      struct m0_cas_ctg      *ctidx,
				      struct m0_be_tx_credit *cred);

static int  ctg_op_tick_ret  (struct m0_ctg_op *ctg_op, int next_state);
static int  ctg_op_exec      (struct m0_ctg_op *ctg_op, int next_phase);
static int  ctg_meta_exec    (struct m0_ctg_op    *ctg_op,
			      const struct m0_fid *fid,
			      int                  next_phase);
static int  ctg_exec         (struct m0_ctg_op    *ctg_op,
			      struct m0_cas_ctg   *ctg,
			      const struct m0_buf *key,
			      int                  next_phase);
static void ctg_store_release(struct m0_ref *ref);

static m0_bcount_t ctg_ksize (const void *key);

static int         ctg_cmp   (const void *key0, const void *key1);


/**
 * Mutex to provide thread-safety for catalogue store singleton initialisation.
 */
static struct m0_mutex cs_init_guard = M0_MUTEX_SINIT(&cs_init_guard);

/**
 * XXX: The following static structures should be either moved to m0 instance to
 * show them to everyone or be a part of high-level context structure.
 */
static       struct m0_ctg_store       ctg_store       = {};
static const        char               cas_state_key[] = "cas-state-nr";

static struct m0_be_seg *cas_seg(struct m0_be_domain *dom)
{
	struct m0_be_seg *seg = m0_be_domain_seg_first(dom);

	if (seg == NULL && cas_in_ut())
		seg = m0_be_domain_seg0_get(dom);
	return seg;
}

static struct m0_be_op *ctg_beop(struct m0_ctg_op *ctg_op)
{
	return ctg_op->co_opcode == CO_CUR ?
		&ctg_op->co_cur.bc_op : &ctg_op->co_beop;
}

static void ctg_memcpy(void *dst, const void *src, uint64_t nob)
{
	*(uint64_t *)dst = nob;
	memcpy(dst + M0_CAS_CTG_KV_HDR_SIZE, src, nob);
}

static int ctg_buf_get(struct m0_buf *dst, const struct m0_buf *src,
		       bool enabled_fi)
{
	m0_bcount_t nob = src->b_nob;

	if (enabled_fi && M0_FI_ENABLED("cas_alloc_fail"))
		return M0_ERR(-ENOMEM);
	dst->b_nob  = src->b_nob + M0_CAS_CTG_KV_HDR_SIZE;
	dst->b_addr = m0_alloc(dst->b_nob);
	if (dst->b_addr != NULL) {
		ctg_memcpy(dst->b_addr, src->b_addr, nob);
		return M0_RC(0);
	} else
		return M0_ERR(-ENOMEM);
}

/**
 * Allocate memory and unpack value having format length + data.
 *
 * @param val destination buffer
 * @param buf source buffer
 * @return 0 if ok else error code
 */
static int ctg_buf(const struct m0_buf *val, struct m0_buf *buf)
{
	int result = -EPROTO;

	M0_CASSERT(sizeof buf->b_nob == 8);
	if (val->b_nob >= 8) {
		buf->b_nob = *(uint64_t *)val->b_addr;
		if (val->b_nob == buf->b_nob + 8) {
			buf->b_addr = ((char *)val->b_addr) + 8;
			result = 0;
		}
	}
	if (result != 0)
		return M0_ERR_INFO(result, "Unexpected: %"PRIx64"/%"PRIx64,
				   val->b_nob, buf->b_nob);
	else
		return M0_RC(result);
}

static void ctg_fid_key_fill(void *key, const struct m0_fid *fid)
{
	ctg_memcpy(key, fid, sizeof(struct m0_fid));
}

static m0_bcount_t ctg_ksize(const void *key)
{
	/* Size is stored in the header. */
	return M0_CAS_CTG_KV_HDR_SIZE + *(const uint64_t *)key;
}

static int ctg_cmp(const void *key0, const void *key1)
{
	m0_bcount_t knob0 = ctg_ksize(key0);
	m0_bcount_t knob1 = ctg_ksize(key1);
	/**
	 * @todo Cannot assert on on-disk data, but no interface to report
	 * errors from here.
	 */
	M0_ASSERT(knob0 >= 8);
	M0_ASSERT(knob1 >= 8);

	return memcmp(key0 + 8, key1 + 8, min_check(knob0, knob1) - 8) ?:
		M0_3WAY(knob0, knob1);
}

static void ctg_init(struct m0_cas_ctg *ctg, struct m0_be_seg *seg)
{
	m0_format_header_pack(&ctg->cc_head, &(struct m0_format_tag){
		.ot_version = M0_CAS_CTG_FORMAT_VERSION,
		.ot_type    = M0_FORMAT_TYPE_CAS_CTG,
		.ot_footer_offset = offsetof(struct m0_cas_ctg, cc_foot)
	});
	M0_ENTRY();
	m0_long_lock_init(m0_ctg_lock(ctg));
	m0_mutex_init(&ctg->cc_chan_guard.bm_u.mutex);
	m0_chan_init(&ctg->cc_chan.bch_chan, &ctg->cc_chan_guard.bm_u.mutex);
	ctg->cc_inited = true;
	m0_format_footer_update(ctg);
}

static void ctg_open(struct m0_cas_ctg *ctg, struct m0_be_seg *seg)
{
	struct m0_btree_op         b_op = {};
	struct m0_btree_rec_key_op key_cmp = { .rko_keycmp = ctg_cmp, };
	int                        rc;

	ctg_init(ctg, seg);

	M0_ALLOC_PTR(ctg->cc_tree);
	M0_ASSERT(ctg->cc_tree);
	rc = M0_BTREE_OP_SYNC_WITH_RC(&b_op, m0_btree_open(&ctg->cc_node,
							   sizeof ctg->cc_node,
							   ctg->cc_tree, seg,
							   &b_op, &key_cmp));
	M0_ASSERT(rc == 0);
}

static void ctg_fini(struct m0_cas_ctg *ctg)
{
	M0_ENTRY("ctg=%p", ctg);

	ctg->cc_inited = false;
	m0_free0(&ctg->cc_tree);
	m0_long_lock_fini(m0_ctg_lock(ctg));
	m0_chan_fini_lock(&ctg->cc_chan.bch_chan);
	m0_mutex_fini(&ctg->cc_chan_guard.bm_u.mutex);
}

int m0_ctg_create(struct m0_be_seg *seg, struct m0_be_tx *tx,
		  struct m0_cas_ctg **out,
		  const struct m0_fid *cas_fid)
{
	struct m0_cas_ctg          *ctg;
	int                         rc;
	struct m0_btree_op          b_op    = {};
	struct m0_btree_rec_key_op  key_cmp = { .rko_keycmp = ctg_cmp, };
	struct m0_fid              *fid     = &M0_FID_TINIT('b',
							cas_fid->f_container,
							cas_fid->f_key);
	struct m0_btree_type        bt      = {
		.tt_id = M0_BT_CAS_CTG,
		.ksize = -1,
		.vsize = -1,
	};

	if (M0_FI_ENABLED("ctg_create_failure"))
		return M0_ERR(-EFAULT);

	M0_BE_ALLOC_ALIGN_PTR_SYNC(ctg, 12, seg, tx);
	if (ctg == NULL)
		return M0_ERR(-ENOMEM);

	ctg_init(ctg, seg);
	M0_ALLOC_PTR(ctg->cc_tree);
	if (ctg->cc_tree == NULL)
		return M0_ERR(-ENOMEM);

	rc = M0_BTREE_OP_SYNC_WITH_RC(&b_op,
				      m0_btree_create(&ctg->cc_node,
						      sizeof ctg->cc_node,
						      &bt, &b_op, ctg->cc_tree,
						      seg, fid, tx, &key_cmp));
	if (rc != 0) {
		ctg_fini(ctg);
		M0_BE_FREE_PTR_SYNC(ctg, seg, tx);
	}
	else
		*out = ctg;
	return M0_RC(rc);
}

static void ctg_destroy(struct m0_cas_ctg *ctg, struct m0_be_tx *tx)
{
	struct m0_btree_op      b_op  = {};
	int                     rc;

	M0_ENTRY();

	rc = M0_BTREE_OP_SYNC_WITH_RC(&b_op, m0_btree_destroy(ctg->cc_tree,
							      &b_op, tx));
	M0_ASSERT(rc == 0);
	ctg_fini(ctg);
	M0_BE_FREE_PTR_SYNC(ctg, cas_seg(tx->t_engine->eng_domain), tx);
}

M0_INTERNAL void m0_ctg_fini(struct m0_fom     *fom,
			     struct m0_cas_ctg *ctg)
{
	if (ctg != NULL && ctg->cc_inited) {
		struct m0_dtx   *dtx = &fom->fo_tx;

		ctg_fini(ctg);
		/*
		 * TODO: implement asynchronous free after memory free API
		 * added into ctg_exec in the scope of asynchronous ctidx
		 * operations task.
		 */
		if (dtx->tx_state != M0_DTX_INVALID) {
			struct m0_be_tx *tx = &fom->fo_tx.tx_betx;
			if (m0_be_tx_state(tx) == M0_BTS_ACTIVE)
				M0_BE_FREE_PTR_SYNC(ctg,
					cas_seg(tx->t_engine->eng_domain), tx);
		}
	}
}

static int ctg_meta_find_cb(struct m0_btree_cb  *cb, struct m0_btree_rec *rec)
{
	struct m0_cas_ctg  **ctg = cb->c_datum;
	struct m0_buf        buf = {};
	struct m0_buf        val = {
		.b_nob =  m0_vec_count(&rec->r_val.ov_vec),
		.b_addr = rec->r_val.ov_buf[0],
	};
	int                  rc;

	rc = ctg_buf(&val, &buf);
	if (rc == 0) {
		if (buf.b_nob == sizeof(*ctg))
			*ctg = *(struct m0_cas_ctg **)buf.b_addr;
		else
			rc = M0_ERR_INFO(-EPROTO, "Unexpected: %"PRIx64,
					 buf.b_nob);
	}
	return rc;
}

/**
 * Lookup catalogue with provided fid in meta-catalogue synchronously.
 */
M0_INTERNAL int m0_ctg_meta_find_ctg(struct m0_cas_ctg    *meta,
			             const struct m0_fid  *ctg_fid,
			             struct m0_cas_ctg   **ctg)
{
	uint8_t              key_data[M0_CAS_CTG_KV_HDR_SIZE +
				      sizeof(struct m0_fid)];
	struct m0_buf        key;
	int                  rc;
	struct m0_btree_op   kv_op = {};
	void                *k_ptr;
	m0_bcount_t          ksize;
	struct m0_btree_rec  rec = {};
	struct m0_btree_cb   get_cb = {
		.c_act = ctg_meta_find_cb,
		.c_datum = ctg,
		};

	*ctg = NULL;

	ctg_fid_key_fill((void *)&key_data, ctg_fid);
	key   = M0_BUF_INIT_PTR(&key_data);
	k_ptr = key.b_addr;
	ksize = key.b_nob;
	rec.r_key.k_data = M0_BUFVEC_INIT_BUF(&k_ptr, &ksize);

	rc = M0_BTREE_OP_SYNC_WITH_RC(&kv_op,
				      m0_btree_get(meta->cc_tree, &rec.r_key,
						   &get_cb, BOF_EQUAL, &kv_op));
	return M0_RC(rc);
}

struct ctg_meta_put_cb_data {
	struct m0_btree_key *d_key;
	struct m0_cas_ctg   *d_ctg;
};

static int ctg_meta_put_cb(struct m0_btree_cb *cb, struct m0_btree_rec *rec)
{
	m0_bcount_t                  ksize;
	struct ctg_meta_put_cb_data *datum = cb->c_datum;
	struct m0_cas_ctg           *ctg   = datum->d_ctg;

	ksize = m0_vec_count(&datum->d_key->k_data.ov_vec);
	M0_ASSERT(m0_vec_count(&rec->r_key.k_data.ov_vec) == ksize);
	m0_bufvec_copy(&rec->r_key.k_data, &datum->d_key->k_data, ksize);

	/**
	 * Meta entry format: length + ptr to cas_ctg.
	 * Memory for ctg allocated elsewhere and must be persistent.
	 */
	*(uint64_t *)rec->r_val.ov_buf[0] = sizeof(ctg);
	*(struct m0_cas_ctg**)(rec->r_val.ov_buf[0] +
			       M0_CAS_CTG_KV_HDR_SIZE) = ctg;
	return 0;
}

M0_INTERNAL int m0_ctg__meta_insert(struct m0_btree     *meta,
				    const struct m0_fid *fid,
				    struct m0_cas_ctg   *ctg,
				    struct m0_be_tx     *tx)
{
	uint8_t                     key_data[M0_CAS_CTG_KV_HDR_SIZE +
					    sizeof(struct m0_fid)];
	struct m0_buf               key;
	int                         rc;
	struct m0_btree_op          kv_op   = {};
	void                       *k_ptr;
	void                       *v_ptr;
	m0_bcount_t                 ksize;
	m0_bcount_t                 vsize;
	struct m0_btree_rec         rec     = {};
	struct ctg_meta_put_cb_data cb_data = {
		.d_key = &rec.r_key,
		.d_ctg = ctg,
	};
	struct m0_btree_cb          put_cb  = {
		.c_act   = ctg_meta_put_cb,
		.c_datum = &cb_data,
	};

	ctg_fid_key_fill((void *)&key_data, fid);
	key = M0_BUF_INIT_PTR(&key_data);
	k_ptr = key.b_addr;
	ksize = key.b_nob;
	vsize = M0_CAS_CTG_KV_HDR_SIZE + sizeof(ctg);

	rec.r_key.k_data = M0_BUFVEC_INIT_BUF(&k_ptr, &ksize);
	rec.r_val        = M0_BUFVEC_INIT_BUF(&v_ptr, &vsize);

	rc = M0_BTREE_OP_SYNC_WITH_RC(&kv_op, m0_btree_put(meta, &rec, &put_cb,
							   &kv_op, tx));
	return M0_RC(rc);
}

static int ctg_meta_selfadd(struct m0_btree *meta, struct m0_be_tx *tx)
{
	return m0_ctg__meta_insert(meta, &m0_cas_meta_fid, NULL, tx);
}

static void ctg_meta_delete(struct m0_btree     *meta,
			    const struct m0_fid *fid,
			    struct m0_be_tx     *tx)
{
	uint8_t              key_data[M0_CAS_CTG_KV_HDR_SIZE +
				      sizeof(struct m0_fid)];
	struct               m0_buf key;
	void                *k_ptr;
	m0_bcount_t          ksize;
	struct m0_btree_key  r_key = {};
	struct m0_btree_op   kv_op = {};
	int                  rc;

	M0_ENTRY();
	r_key.k_data = M0_BUFVEC_INIT_BUF(&k_ptr, &ksize);
	ctg_fid_key_fill((void *)&key_data, fid);
	key   = M0_BUF_INIT_PTR(&key_data);
	k_ptr = key.b_addr;
	ksize = key.b_nob;

	rc = M0_BTREE_OP_SYNC_WITH_RC(&kv_op, m0_btree_del(meta, &r_key, NULL,
							   &kv_op, tx));
	M0_ASSERT(rc == 0);
}

static void ctg_meta_selfrm(struct m0_btree *meta, struct m0_be_tx *tx)
{
	return ctg_meta_delete(meta, &m0_cas_meta_fid, tx);
}

static void ctg_meta_insert_credit(struct m0_btree_type   *bt,
				   struct m0_be_seg       *seg,
				   m0_bcount_t             nr,
				   struct m0_be_tx_credit *accum)
{
	struct m0_cas_ctg *ctg;
	m0_btree_put_credit2(bt, 12, nr,
			     M0_CAS_CTG_KV_HDR_SIZE + sizeof(struct m0_fid),
			     M0_CAS_CTG_KV_HDR_SIZE + sizeof(ctg), accum);
	/*
	 * Will allocate space for cas_ctg body then put ptr to it into btree.
	 */
	M0_BE_ALLOC_CREDIT_PTR(ctg, seg, accum);
}

static void ctg_meta_delete_credit(struct m0_btree_type   *bt,
				   struct m0_be_seg       *seg,
				   m0_bcount_t             nr,
				   struct m0_be_tx_credit *accum)
{
	struct m0_cas_ctg *ctg;

	m0_btree_del_credit2(bt, 12, nr,
			     M0_CAS_CTG_KV_HDR_SIZE + sizeof(struct m0_fid),
			     M0_CAS_CTG_KV_HDR_SIZE + sizeof(ctg), accum);
	M0_BE_FREE_CREDIT_PTR(ctg, seg, accum);
}

static void ctg_store_init_creds_calc(struct m0_be_seg       *seg,
				      struct m0_cas_state    *state,
				      struct m0_cas_ctg      *ctidx,
				      struct m0_be_tx_credit *cred)
{
	struct m0_btree_type    bt = {
		.tt_id = M0_BT_CAS_CTG,
		.ksize = -1,
		.vsize = -1,
	};

	m0_be_seg_dict_insert_credit(seg, cas_state_key, cred);
	M0_BE_ALLOC_CREDIT_PTR(state, seg, cred);
	M0_BE_ALLOC_CREDIT_PTR(ctidx, seg, cred);
	/*
	 * Credits for dead_index catalogue descriptor.
	 */
	M0_BE_ALLOC_CREDIT_PTR(ctidx, seg, cred);
	/*
	 * Credits for 3 trees: meta, ctidx, dead_index.
	 */
	m0_btree_create_credit(&bt, cred, 3);
	ctg_meta_insert_credit(&bt, seg, 3, cred);
	/* Error case: tree destruction and freeing. */
	ctg_meta_delete_credit(&bt, seg, 3, cred);
	m0_btree_destroy_credit(NULL, &bt, cred, 3);
	M0_BE_FREE_CREDIT_PTR(state, seg, cred);
	M0_BE_FREE_CREDIT_PTR(ctidx, seg, cred);
	/*
	 * Free for dead_index.
	 */
	M0_BE_FREE_CREDIT_PTR(ctidx, seg, cred);
}

static int ctg_state_create(struct m0_be_seg     *seg,
			    struct m0_be_tx      *tx,
			    struct m0_cas_state **state)
{
	struct m0_cas_state *out;
	struct m0_btree     *bt;
 	int                  rc;

	M0_ENTRY();
	*state = NULL;
	M0_BE_ALLOC_PTR_SYNC(out, seg, tx);
	if (out == NULL)
		return M0_ERR(-ENOSPC);
	m0_format_header_pack(&out->cs_header, &(struct m0_format_tag){
		.ot_version       = M0_CAS_STATE_FORMAT_VERSION,
		.ot_type          = M0_FORMAT_TYPE_CAS_STATE,
		.ot_footer_offset = offsetof(struct m0_cas_state, cs_footer)
	});

	rc = m0_ctg_create(seg, tx, &out->cs_meta, &m0_cas_meta_fid);
	if (rc == 0) {
		bt = out->cs_meta->cc_tree;
                rc = ctg_meta_selfadd(bt, tx);
                if (rc != 0)
                        ctg_destroy(out->cs_meta, tx);
	}
	if (rc != 0)
                M0_BE_FREE_PTR_SYNC(out, seg, tx);
        else
                *state = out;
        return M0_RC(rc);
}

static void ctg_state_destroy(struct m0_cas_state *state,
			      struct m0_be_seg    *seg,
			      struct m0_be_tx     *tx)
{
	struct m0_cas_ctg *meta = state->cs_meta;

	ctg_meta_selfrm(meta->cc_tree, tx);
	ctg_destroy(meta, tx);
	M0_BE_FREE_PTR_SYNC(state, seg, tx);
}

/**
 * Initialisation function when catalogue store state was found on a disk.
 */
static int ctg_store__init(struct m0_be_seg *seg, struct m0_cas_state *state)
{
	int rc;

	M0_ENTRY();

	ctg_store.cs_state = state;
	m0_mutex_init(&state->cs_ctg_init_mutex.bm_u.mutex);
	ctg_open(state->cs_meta, seg);

	/* Searching for catalogue-index catalogue. */
	rc = m0_ctg_meta_find_ctg(state->cs_meta, &m0_cas_ctidx_fid,
			          &ctg_store.cs_ctidx) ?:
	     m0_ctg_meta_find_ctg(state->cs_meta, &m0_cas_dead_index_fid,
			          &ctg_store.cs_dead_index);
	if (rc == 0) {
		ctg_open(ctg_store.cs_ctidx, seg);
		ctg_open(ctg_store.cs_dead_index, seg);
	} else {
		ctg_store.cs_ctidx = NULL;
		ctg_store.cs_dead_index = NULL;
	}
	return M0_RC(rc);
}

/**
 * Initialisation function when catalogue store state was not found on a disk.
 *
 * It creates all necessary data in BE segment and initialises catalogue store.
 */
static int ctg_store_create(struct m0_be_seg *seg)
{
	/*
	 * Currently catalog store has dictionary consisting of 3 catalogues:
	 * meta itself, ctidx and dead_index.
	 */
	struct m0_cas_state    *state = NULL;
	struct m0_cas_ctg      *ctidx = NULL;
	struct m0_cas_ctg      *dead_index = NULL;
	struct m0_be_tx         tx     = {};
	struct m0_be_tx_credit  cred   = M0_BE_TX_CREDIT(0, 0);
	struct m0_sm_group     *grp   = m0_locality0_get()->lo_grp;
	int                     rc;
	m0_bcount_t             bytes;
	struct m0_buf           buf;
	M0_ENTRY();
	m0_sm_group_lock(grp);
	m0_be_tx_init(&tx, 0, seg->bs_domain, grp, NULL, NULL, NULL, NULL);

	ctg_store_init_creds_calc(seg, state, ctidx, &cred);

	m0_be_tx_prep(&tx, &cred);
	rc = m0_be_tx_exclusive_open_sync(&tx);
	if (rc != 0) {
		m0_be_tx_fini(&tx);
		return M0_ERR(rc);
	}

	rc = ctg_state_create(seg, &tx, &state);
	if (rc != 0)
		goto end;
	m0_mutex_init(&state->cs_ctg_init_mutex.bm_u.mutex);

	/* Create catalog-index catalogue. */
	rc = m0_ctg_create(seg, &tx, &ctidx, &m0_cas_ctidx_fid);
	if (rc != 0)
		goto state_destroy;
	/*
	 * Insert catalogue-index catalogue into meta-catalogue.
	 */
	rc = m0_ctg__meta_insert(state->cs_meta->cc_tree, &m0_cas_ctidx_fid,
				 ctidx, &tx);
	if (rc != 0)
		goto ctidx_destroy;

	/*
	 * Create place for records deleted from meta (actually moved there).
	 */
	rc = m0_ctg_create(seg, &tx, &dead_index, &m0_cas_dead_index_fid);
	if (rc != 0)
		goto ctidx_destroy;
	/*
	 * Insert "dead index" catalogue into meta-catalogue.
	 */
	rc = m0_ctg__meta_insert(state->cs_meta->cc_tree,
				 &m0_cas_dead_index_fid, dead_index, &tx);
	if (rc != 0)
		goto dead_index_destroy;

	rc = m0_be_seg_dict_insert(seg, &tx, cas_state_key, state);
	if (rc != 0)
		goto dead_index_delete;

	bytes = offsetof(typeof(*ctidx), cc_foot) + sizeof(ctidx->cc_foot);
	buf   = M0_BUF_INIT(bytes, ctidx);
	M0_BE_TX_CAPTURE_BUF(seg, &tx, &buf);

	buf   = M0_BUF_INIT(bytes, dead_index);
	M0_BE_TX_CAPTURE_BUF(seg, &tx, &buf);

	m0_format_footer_update(state);
	M0_BE_TX_CAPTURE_PTR(seg, &tx, state);
	ctg_store.cs_state = state;
	ctg_store.cs_ctidx = ctidx;
	ctg_store.cs_dead_index = dead_index;
	goto end;
dead_index_delete:
	ctg_meta_delete(state->cs_meta->cc_tree,
			&m0_cas_dead_index_fid,
			&tx);
dead_index_destroy:
	ctg_destroy(dead_index, &tx);
	ctg_meta_delete(state->cs_meta->cc_tree,
			&m0_cas_ctidx_fid,
			&tx);
ctidx_destroy:
	ctg_destroy(ctidx, &tx);
state_destroy:
	ctg_state_destroy(state, seg, &tx);
end:
	m0_be_tx_close_sync(&tx);
	m0_be_tx_fini(&tx);
	m0_sm_group_unlock(grp);
	return M0_RC(rc);
}

M0_INTERNAL int m0_ctg_store_init(struct m0_be_domain *dom)
{
	struct m0_be_seg    *seg   = cas_seg(dom);
	struct m0_cas_state *state = NULL;
	int                  result;

	M0_ENTRY();
	m0_mutex_lock(&cs_init_guard);
	if (ctg_store.cs_initialised) {
		m0_ref_get(&ctg_store.cs_ref);
		result = 0;
		goto end;
	}

	/**
	 * @todo Use 0type.
	 */
	result = m0_be_seg_dict_lookup(seg, cas_state_key, (void **)&state);
	if (result == 0) {
		/**
		 * @todo Add checking, use header and footer.
		 */
		M0_ASSERT(state != NULL);
		M0_LOG(M0_DEBUG,
		       "cas_state from storage: cs_meta %p, cs_rec_nr %"PRIx64,
		       state->cs_meta, state->cs_rec_nr);
		result = ctg_store__init(seg, state);
	} else if (result == -ENOENT) {
		M0_LOG(M0_DEBUG, "Ctg store state wasn't found on a disk.");
		result = ctg_store_create(seg);
	}

	if (result == 0) {
		m0_mutex_init(&ctg_store.cs_state_mutex);
		m0_long_lock_init(&ctg_store.cs_del_lock);
		m0_ref_init(&ctg_store.cs_ref, 1, ctg_store_release);
		ctg_store.cs_be_domain = dom;
		ctg_store.cs_initialised = true;
	}
end:
	m0_mutex_unlock(&cs_init_guard);
	return M0_RC(result);
}

static void ctg_store_release(struct m0_ref *ref)
{
	struct m0_ctg_store *ctg_store = M0_AMB(ctg_store, ref, cs_ref);

	M0_ENTRY();
	m0_mutex_fini(&ctg_store->cs_state_mutex);
	ctg_store->cs_state = NULL;
	ctg_store->cs_ctidx = NULL;
	m0_long_lock_fini(&ctg_store->cs_del_lock);
	ctg_store->cs_initialised = false;
}

M0_INTERNAL void m0_ctg_store_fini(void)
{
	M0_ENTRY();
	m0_ref_put(&ctg_store.cs_ref);
}

static void ctg_state_counter_add(uint64_t *counter, uint64_t val)
{
	if (*counter != ~0ULL) {
		if (*counter + val < *counter) {
			M0_LOG(M0_WARN,
			       "Ctg store counter overflow: counter %"
			       PRIx64" addendum %"PRIx64, *counter, val);
			*counter = ~0ULL;
		} else {
			*counter += val;
		}
	}
}

static void ctg_state_counter_sub(uint64_t *counter, uint64_t val)
{
	if (*counter != ~0ULL) {
		M0_ASSERT(*counter - val <= *counter);
		*counter -= val;
	}
}

static uint64_t ctg_state_update(struct m0_be_tx *tx, uint64_t size,
				 bool is_inc)
{
	uint64_t         *recs_nr  = &ctg_store.cs_state->cs_rec_nr;
	uint64_t         *rec_size = &ctg_store.cs_state->cs_rec_size;
	struct m0_be_seg *seg      = cas_seg(tx->t_engine->eng_domain);

	if (M0_FI_ENABLED("test_overflow"))
		size = ~0ULL - 1;

	m0_mutex_lock(&ctg_store.cs_state_mutex);
	/*
	 * recs_nr and rec_size counters update is done having possible overflow
	 * in mind. A counter value is sticked to ~0ULL in the case of overflow
	 * and further counter updates are ignored.
	 */
	if (is_inc) {
		/*
		 * Overflow is unlikely. If it happens, then calculation of DIX
		 * repair/re-balance progress may be incorrect.
		 */
		ctg_state_counter_add(recs_nr, 1);
		/*
		 * Overflow is possible, because total size is not decremented
		 * on record deletion.
		 */
		ctg_state_counter_add(rec_size, size);
	} else {
		M0_ASSERT(*recs_nr != 0);
		ctg_state_counter_sub(recs_nr, 1);
		ctg_state_counter_sub(rec_size, size);
	}
	m0_format_footer_update(ctg_store.cs_state);
	m0_mutex_unlock(&ctg_store.cs_state_mutex);

	M0_LOG(M0_DEBUG, "ctg_state_update: rec_nr %"PRIx64 " rec_size %"PRIx64,
	       ctg_store.cs_state->cs_rec_nr,
	       ctg_store.cs_state->cs_rec_size);
	M0_BE_TX_CAPTURE_PTR(seg, tx, ctg_store.cs_state);
	return *recs_nr;
}

M0_INTERNAL void m0_ctg_state_inc_update(struct m0_be_tx *tx, uint64_t size)
{
	(void)ctg_state_update(tx, size, true);
}

static void ctg_state_dec_update(struct m0_be_tx *tx, uint64_t size)
{
	(void)ctg_state_update(tx, size, false);
}

/**
 * Initialise catalog meta-data volatile stuff: mutexes etc.
 */
M0_INTERNAL void m0_ctg_try_init(struct m0_cas_ctg *ctg)
{
	M0_ENTRY();
	m0_mutex_lock(&ctg_store.cs_state->cs_ctg_init_mutex.bm_u.mutex);
	/*
	 * ctg is null if this is entry for Meta: it is not filled.
	 */
	if (ctg != NULL && !ctg->cc_inited) {
		M0_LOG(M0_DEBUG, "ctg_init %p", ctg);
		ctg_open(ctg, cas_seg(ctg_store.cs_be_domain));
	} else
		M0_LOG(M0_DEBUG, "ctg %p zero or inited", ctg);
	m0_mutex_unlock(&ctg_store.cs_state->cs_ctg_init_mutex.bm_u.mutex);
}

/** Checks whether catalogue is a user catalogue (not meta). */
static bool ctg_is_ordinary(const struct m0_cas_ctg *ctg)
{
	return !M0_IN(ctg, (m0_ctg_dead_index(), m0_ctg_ctidx(),
			    m0_ctg_meta()));
}

/* Callback after completion of operation. */
static bool ctg_op_cb(struct m0_clink *clink)
{
	struct m0_ctg_op *ctg_op   = M0_AMB(ctg_op, clink, co_clink);
	struct m0_be_op  *op       = ctg_beop(ctg_op);
	int               opc      = ctg_op->co_opcode;
	int               ct       = ctg_op->co_ct;
	struct m0_be_tx  *tx       = &ctg_op->co_fom->fo_tx.tx_betx;
	struct m0_buf     cur_key  = {};
	struct m0_buf     cur_val  = {};
	struct m0_chan   *ctg_chan = &ctg_op->co_ctg->cc_chan.bch_chan;
	struct m0_buf    *dst;
	struct m0_buf    *src;
	int               rc;

	if (op->bo_sm.sm_state != M0_BOS_DONE)
		return true;

	rc = ctg_op->co_rc;
	if (rc == 0) {
		switch (CTG_OP_COMBINE(opc, ct)) {
		case CTG_OP_COMBINE(CO_DEL, CT_BTREE):
			if (ctg_is_ordinary(ctg_op->co_ctg))
				ctg_state_dec_update(tx, 0);
			/* Fall through. */
		case CTG_OP_COMBINE(CO_DEL, CT_META):
		case CTG_OP_COMBINE(CO_TRUNC, CT_BTREE):
		case CTG_OP_COMBINE(CO_DROP, CT_BTREE):
		case CTG_OP_COMBINE(CO_GC, CT_META):
			m0_chan_broadcast_lock(ctg_chan);
			break;
		case CTG_OP_COMBINE(CO_MIN, CT_BTREE):
			ctg_buf(&ctg_op->co_out_key, &cur_key);
			ctg_op->co_out_key = cur_key;
			break;
		case CTG_OP_COMBINE(CO_CUR, CT_META):
		case CTG_OP_COMBINE(CO_CUR, CT_BTREE):
			M0_PRE(m0_be_op_is_done(&ctg_op->co_cur.bc_op));
			m0_btree_cursor_kv_get(&ctg_op->co_cur,
					       &cur_key,
					       &cur_val);
			rc = ctg_buf(&cur_key, &ctg_op->co_out_key);
			if (rc == 0)
				rc = ctg_buf(&cur_val, &ctg_op->co_out_val);
			if (rc == 0 && ct == CT_META)
				m0_ctg_try_init(*(struct m0_cas_ctg **)
					     ctg_op->co_out_val.b_addr);
			break;
		case CTG_OP_COMBINE(CO_MEM_PLACE, CT_MEM):
			/*
			 * Copy user provided buffer to the buffer allocated in
			 * BE and capture it.
			 */
			src = &ctg_op->co_val;
			dst = &ctg_op->co_mem_buf;
			M0_ASSERT(src->b_nob == dst->b_nob);
			memcpy(dst->b_addr, src->b_addr, src->b_nob);
			m0_be_tx_capture(
				&ctg_op->co_fom->fo_tx.tx_betx,
				&M0_BE_REG(cas_seg(tx->t_engine->eng_domain),
					   dst->b_nob, dst->b_addr));
			break;
		case CTG_OP_COMBINE(CO_MEM_FREE, CT_MEM):
			/* Nothing to do. */
			break;
		}
	}

	if (opc == CO_CUR) {
		/* Always finalise BE operation of the cursor. */
		m0_be_op_fini(&ctg_op->co_cur.bc_op);
		M0_SET0(&ctg_op->co_cur.bc_op);
	}

	if (opc == CO_PUT &&
	    ctg_op->co_flags & COF_CREATE &&
	    rc == -EEXIST)
		ctg_op->co_rc = 0;

	m0_chan_broadcast_lock(&ctg_op->co_channel);
	/*
	 * This callback may be called directly from ctg_op_tick_ret()
	 * without adding of clink to the channel.
	 */
	if (m0_clink_is_armed(clink))
		m0_clink_del(clink);

	return true;
}

static int ctg_op_tick_ret(struct m0_ctg_op *ctg_op,
			   int               next_state)
{
	struct m0_chan  *chan  = &ctg_op->co_channel;
	struct m0_fom   *fom   = ctg_op->co_fom;
	struct m0_clink *clink = &ctg_op->co_clink;
	struct m0_be_op *op    = ctg_beop(ctg_op);
	int              ret   = M0_FSO_AGAIN;
	bool             op_is_active;

	m0_be_op_lock(op);
	M0_PRE(M0_IN(op->bo_sm.sm_state, (M0_BOS_ACTIVE, M0_BOS_DONE)));

	op_is_active = op->bo_sm.sm_state == M0_BOS_ACTIVE;
	if (op_is_active) {
		ret = M0_FSO_WAIT;
		m0_clink_add(&op->bo_sm.sm_chan, clink);

		m0_chan_lock(chan);
		m0_fom_wait_on(fom, chan, &fom->fo_cb);
		m0_chan_unlock(chan);
	}
	m0_be_op_unlock(op);

	if (!op_is_active)
		clink->cl_cb(clink);
	/*
	 * In some cases we don't want to set the next phase
	 * if this function is used as helper for something
	 * else. We use it for lookup on "del" op to populate
	 * the deleted value into the FDMI record.
	 */
	if (next_state >= 0)
		m0_fom_phase_set(fom, next_state);

	return ret;
}

struct ctg_op_cb_data {
	struct m0_ctg_op    *d_ctg_op;
	struct m0_btree_rec *d_rec;
	struct m0_cas_ctg   *d_cas_ctg;
};

/**
 * Callback while executing operation. This callback get called by btree
 * operations
 */
static int ctg_op_cb_btree(struct m0_btree_cb *cb, struct m0_btree_rec *rec)
{
	struct ctg_op_cb_data  *datum    = cb->c_datum;
	struct m0_ctg_op       *ctg_op   = datum->d_ctg_op;
	struct m0_be_tx        *tx       = &ctg_op->co_fom->fo_tx.tx_betx;
	struct m0_chan         *ctg_chan = &ctg_op->co_ctg->cc_chan.bch_chan;
	int                     opc      = ctg_op->co_opcode;
	int                     ct       = ctg_op->co_ct;
	int                     rc;

	M0_PRE(M0_IN(opc,(CO_GET, CO_PUT, CO_MIN)));
	if (opc == CO_PUT) {
		m0_bcount_t    ksize;

		ksize = m0_vec_count(&datum->d_rec->r_key.k_data.ov_vec);
		M0_ASSERT(m0_vec_count(&rec->r_key.k_data.ov_vec) == ksize);
		m0_bufvec_copy(&rec->r_key.k_data, &datum->d_rec->r_key.k_data,
			       ksize);
	}

	switch (CTG_OP_COMBINE(opc, ct)) {
	case CTG_OP_COMBINE(CO_GET, CT_BTREE):
	case CTG_OP_COMBINE(CO_GET, CT_META): {
		struct m0_buf val = {
			.b_nob =  m0_vec_count(&rec->r_val.ov_vec),
			.b_addr = rec->r_val.ov_buf[0],
		};
		rc = ctg_buf(&val, &ctg_op->co_out_val);
		if (rc == 0 && ct == CT_META) {
			if (ctg_op->co_out_val.b_nob !=
			    sizeof(struct m0_cas_ctg *))
				rc = M0_ERR(-EFAULT);
			else
				m0_ctg_try_init(*(struct m0_cas_ctg **)
						ctg_op->co_out_val.b_addr);
		}
		M0_ASSERT(rc == 0);
		break;
	}
	case CTG_OP_COMBINE(CO_PUT, CT_DEAD_INDEX):
		m0_chan_broadcast_lock(ctg_chan);
		break;
	case CTG_OP_COMBINE(CO_PUT, CT_BTREE):
		ctg_memcpy(rec->r_val.ov_buf[0], ctg_op->co_val.b_addr,
			   ctg_op->co_val.b_nob);
		if (ctg_is_ordinary(ctg_op->co_ctg))
			m0_ctg_state_inc_update(tx,
				ctg_op->co_key.b_nob -
				M0_CAS_CTG_KV_HDR_SIZE +
				ctg_op->co_val.b_nob);
		m0_chan_broadcast_lock(ctg_chan);
		break;
	case CTG_OP_COMBINE(CO_PUT, CT_META):
		/*
		* After successful insert inplace fill value of meta by length &
		* pointer to cas_ctg. m0_ctg_create() creates cas_ctg,
		* including memory alloc.
		*/
		*(uint64_t *)(rec->r_val.ov_buf[0]) = sizeof(struct m0_cas_ctg *);
		*(struct m0_cas_ctg**)(rec->r_val.ov_buf[0] +
				M0_CAS_CTG_KV_HDR_SIZE) = datum->d_cas_ctg;
		m0_chan_broadcast_lock(ctg_chan);
		break;
	case CTG_OP_COMBINE(CO_MIN, CT_BTREE): {
		struct m0_buf *out = &ctg_op->co_out_key;
		m0_bcount_t ksize  = ctg_ksize(rec->r_key.k_data.ov_buf[0]);
		M0_ASSERT(ksize == m0_vec_count(&rec->r_key.k_data.ov_vec));
		m0_buf_init(out, rec->r_key.k_data.ov_buf[0], ksize);
		break;
	}
	}
	return 0;
}

static int ctg_op_exec(struct m0_ctg_op *ctg_op, int next_phase)
{
	struct m0_buf             *key     = &ctg_op->co_key;
	struct m0_btree           *btree   = ctg_op->co_ctg->cc_tree;
	struct m0_btree_cursor    *cur     = &ctg_op->co_cur;
	struct m0_be_tx           *tx      = &ctg_op->co_fom->fo_tx.tx_betx;
	struct m0_be_op           *beop    = ctg_beop(ctg_op);
	int                        opc     = ctg_op->co_opcode;
	int                        ct      = ctg_op->co_ct;
	struct m0_btree_op         kv_op   = {};
	struct m0_btree_rec        rec     = {};
	struct ctg_op_cb_data      cb_data = {};
	struct m0_btree_cb         cb = {};
	int                        rc      = 0;
	void                      *k_ptr;
	void                      *v_ptr;
	m0_bcount_t                ksize;
	m0_bcount_t                vsize;

	cb_data.d_ctg_op = ctg_op;
	cb_data.d_rec    = &rec;

	cb.c_act    = ctg_op_cb_btree;
	cb.c_datum  = &cb_data;

	k_ptr = key->b_addr;
	ksize = key->b_nob;

	switch (CTG_OP_COMBINE(opc, ct)) {
	case CTG_OP_COMBINE(CO_PUT, CT_BTREE):
		m0_be_op_active(beop);

		vsize = M0_CAS_CTG_KV_HDR_SIZE + ctg_op->co_val.b_nob;
		rec.r_key.k_data = M0_BUFVEC_INIT_BUF(&k_ptr, &ksize);
		rec.r_val        = M0_BUFVEC_INIT_BUF(&v_ptr, &vsize);

		if (!!(ctg_op->co_flags & COF_OVERWRITE))
			rc = M0_BTREE_OP_SYNC_WITH_RC(&kv_op,
						      m0_btree_update(btree,
								      &rec, &cb,
								      &kv_op,
								      tx));
		else
			rc= M0_BTREE_OP_SYNC_WITH_RC(&kv_op,
						     m0_btree_put(btree, &rec,
								  &cb, &kv_op,
								  tx));
		m0_be_op_done(beop);
		break;
	case CTG_OP_COMBINE(CO_PUT, CT_META): {
		struct m0_cas_ctg *cas_ctg;

		m0_be_op_active(beop);
		M0_ASSERT(!(ctg_op->co_flags & COF_OVERWRITE));

		rc = m0_ctg_create(cas_seg(tx->t_engine->eng_domain), tx,
				   &cas_ctg,
				   /*
				    *Meta key have a fid of index/ctg store.
				    * It is located after KV header.
				    */
				   key->b_addr + M0_CAS_CTG_KV_HDR_SIZE);
		M0_ASSERT(rc == 0);

		vsize = M0_CAS_CTG_KV_HDR_SIZE + sizeof(struct m0_cas_ctg *);
		rec.r_key.k_data  = M0_BUFVEC_INIT_BUF(&k_ptr, &ksize);
		rec.r_val         = M0_BUFVEC_INIT_BUF(&v_ptr, &vsize);
		cb_data.d_cas_ctg = cas_ctg;

		rc = M0_BTREE_OP_SYNC_WITH_RC(&kv_op,
					      m0_btree_put(btree, &rec, &cb,
							   &kv_op, tx));
		if (rc)
			ctg_destroy(cas_ctg, tx);
		m0_be_op_done(beop);
		break;
	}
	case CTG_OP_COMBINE(CO_PUT, CT_DEAD_INDEX):
		m0_be_op_active(beop);
		/*
		 * No need a value in dead index, but, seems, must put something
		 * there. Do not fill anything in the callback after
		 * m0_be_btree_insert_inplace() have 0 there.
		 */

		vsize = 8;
		rec.r_key.k_data = M0_BUFVEC_INIT_BUF(&k_ptr, &ksize);
		rec.r_val        = M0_BUFVEC_INIT_BUF(&v_ptr, &vsize);

		rc = M0_BTREE_OP_SYNC_WITH_RC(&kv_op,
					      m0_btree_put(btree, &rec, &cb,
							   &kv_op, tx));
		m0_be_op_done(beop);
		break;
	case CTG_OP_COMBINE(CO_GET, CT_BTREE):
	case CTG_OP_COMBINE(CO_GET, CT_META):
		m0_be_op_active(beop);
		rec.r_key.k_data = M0_BUFVEC_INIT_BUF(&k_ptr, &ksize);

		rc = M0_BTREE_OP_SYNC_WITH_RC(&kv_op,
					      m0_btree_get(btree, &rec.r_key,
							   &cb, BOF_EQUAL,
							   &kv_op));
		m0_be_op_done(beop);
		break;
	case CTG_OP_COMBINE(CO_MIN, CT_BTREE):
		m0_be_op_active(beop);
		rc = M0_BTREE_OP_SYNC_WITH_RC(&kv_op,
					      m0_btree_minkey(btree, &cb, 0,
							      &kv_op));
		m0_be_op_done(beop);
		break;
	case CTG_OP_COMBINE(CO_TRUNC, CT_BTREE):
		m0_be_op_active(beop);
		rc = M0_BTREE_OP_SYNC_WITH_RC(&kv_op,
					      m0_btree_truncate(btree,
								ctg_op->co_cnt,
								tx, &kv_op));
		m0_be_op_done(beop);
		break;
	case CTG_OP_COMBINE(CO_DROP, CT_BTREE):
		m0_be_op_active(beop);
		rc = M0_BTREE_OP_SYNC_WITH_RC(&kv_op,
					      m0_btree_destroy(btree,
								&kv_op, tx));
		m0_be_op_done(beop);
		break;
	case CTG_OP_COMBINE(CO_DEL, CT_BTREE):
	case CTG_OP_COMBINE(CO_DEL, CT_META):
		m0_be_op_active(beop);
		rec.r_key.k_data = M0_BUFVEC_INIT_BUF(&k_ptr, &ksize);

		rc = M0_BTREE_OP_SYNC_WITH_RC(&kv_op,
					      m0_btree_del(btree, &rec.r_key,
							   NULL, &kv_op, tx));
		m0_be_op_done(beop);
		break;
	case CTG_OP_COMBINE(CO_GC, CT_META):
		m0_cas_gc_wait_async(beop);
		break;
	case CTG_OP_COMBINE(CO_CUR, CT_BTREE):
	case CTG_OP_COMBINE(CO_CUR, CT_META):
		m0_be_op_active(beop);
		M0_ASSERT(ctg_op->co_cur_phase == CPH_GET ||
			  ctg_op->co_cur_phase == CPH_NEXT);
		rec.r_key.k_data = M0_BUFVEC_INIT_BUF(&k_ptr, &ksize);

		if (ctg_op->co_cur_phase == CPH_GET)
			rc = m0_btree_cursor_get(cur, &rec.r_key,
					!!(ctg_op->co_flags & COF_SLANT));
		else
			rc = m0_btree_cursor_next(cur);
		m0_be_op_done(beop);
		break;
	}
	ctg_op->co_rc = rc;
	return ctg_op_tick_ret(ctg_op, next_phase);
}

static int ctg_mem_op_exec(struct m0_ctg_op *ctg_op, int next_phase)
{
	struct m0_be_tx *tx   = &ctg_op->co_fom->fo_tx.tx_betx;
	struct m0_be_op *beop = ctg_beop(ctg_op);
	int              opc  = ctg_op->co_opcode;
	int              ct   = ctg_op->co_ct;

	switch (CTG_OP_COMBINE(opc, ct)) {
	case CTG_OP_COMBINE(CO_MEM_PLACE, CT_MEM):
		M0_BE_ALLOC_BUF(&ctg_op->co_mem_buf,
				cas_seg(tx->t_engine->eng_domain), tx, beop);
		break;
	case CTG_OP_COMBINE(CO_MEM_FREE, CT_MEM):
		M0_BE_FREE_PTR(ctg_op->co_mem_buf.b_addr,
			       cas_seg(tx->t_engine->eng_domain), tx, beop);
		break;
	}

	return ctg_op_tick_ret(ctg_op, next_phase);
}

static int ctg_meta_exec(struct m0_ctg_op    *ctg_op,
			 const struct m0_fid *fid,
			 int                  next_phase)
{
	int ret;

	ctg_op->co_ctg = ctg_store.cs_state->cs_meta;
	ctg_op->co_ct = CT_META;

	if (ctg_op->co_opcode != CO_GC &&
	    (ctg_op->co_opcode != CO_CUR ||
	     ctg_op->co_cur_phase != CPH_NEXT))
		ctg_op->co_rc = ctg_buf_get(&ctg_op->co_key,
					    &M0_BUF_INIT_CONST(
						    sizeof(struct m0_fid),
						    fid), true);
	if (ctg_op->co_rc != 0) {
		ret = M0_FSO_AGAIN;
		m0_fom_phase_set(ctg_op->co_fom, next_phase);
	} else
		ret = ctg_op_exec(ctg_op, next_phase);

	return ret;
}


M0_INTERNAL int m0_ctg_meta_insert(struct m0_ctg_op    *ctg_op,
				   const struct m0_fid *fid,
				   int                  next_phase)
{
	M0_PRE(ctg_op != NULL);
	M0_PRE(fid != NULL);
	M0_PRE(ctg_op->co_beop.bo_sm.sm_state == M0_BOS_INIT);

	ctg_op->co_opcode = CO_PUT;

	return ctg_meta_exec(ctg_op, fid, next_phase);
}

M0_INTERNAL int m0_ctg_gc_wait(struct m0_ctg_op *ctg_op,
			       int               next_phase)
{
	M0_ENTRY();

	M0_PRE(ctg_op != NULL);
	M0_PRE(ctg_op->co_beop.bo_sm.sm_state == M0_BOS_INIT);

	ctg_op->co_opcode = CO_GC;

	return ctg_meta_exec(ctg_op, 0, next_phase);
}

M0_INTERNAL int m0_ctg_meta_lookup(struct m0_ctg_op    *ctg_op,
				   const struct m0_fid *fid,
				   int                  next_phase)
{
	M0_PRE(ctg_op != NULL);
	M0_PRE(fid != NULL);
	M0_PRE(ctg_op->co_beop.bo_sm.sm_state == M0_BOS_INIT);

	ctg_op->co_opcode = CO_GET;

	return ctg_meta_exec(ctg_op, fid, next_phase);
}

M0_INTERNAL
struct m0_cas_ctg *m0_ctg_meta_lookup_result(struct m0_ctg_op *ctg_op)
{
	struct m0_cas_ctg *ctg = NULL;

	M0_PRE(ctg_op != NULL);
	M0_PRE(ctg_op->co_beop.bo_sm.sm_state == M0_BOS_DONE);
	M0_PRE(ctg_op->co_opcode == CO_GET);
	M0_PRE(ctg_op->co_ct == CT_META);

	if (ctg_op->co_rc == 0)
		ctg = *(struct m0_cas_ctg **)ctg_op->co_out_val.b_addr;

	return ctg;
}

M0_INTERNAL int m0_ctg_meta_delete(struct m0_ctg_op    *ctg_op,
				   const struct m0_fid *fid,
				   int                  next_phase)
{
	M0_PRE(ctg_op != NULL);
	M0_PRE(fid != NULL);
	M0_PRE(ctg_op->co_beop.bo_sm.sm_state == M0_BOS_INIT);

	ctg_op->co_opcode = CO_DEL;

	return ctg_meta_exec(ctg_op, fid, next_phase);
}

M0_INTERNAL int m0_ctg_dead_index_insert(struct m0_ctg_op  *ctg_op,
					 struct m0_cas_ctg *ctg,
					 int                next_phase)
{
	int ret;

	ctg_op->co_ctg = m0_ctg_dead_index();
	ctg_op->co_ct = CT_DEAD_INDEX;
	ctg_op->co_opcode = CO_PUT;
	/*
	 * Actually we need just a catalogue.
	 * Put struct m0_cas_ctg* into dead_index as a key, keep value empty.
	 */
	ctg_op->co_rc = ctg_buf_get(&ctg_op->co_key,
				    &M0_BUF_INIT_CONST(sizeof(ctg), &ctg), true);
	if (ctg_op->co_rc != 0) {
		ret = M0_FSO_AGAIN;
		m0_fom_phase_set(ctg_op->co_fom, next_phase);
	} else
		ret = ctg_op_exec(ctg_op, next_phase);
	return ret;
}

static int ctg_exec(struct m0_ctg_op    *ctg_op,
		    struct m0_cas_ctg   *ctg,
		    const struct m0_buf *key,
		    int                  next_phase)
{
	int ret = M0_FSO_AGAIN;

	ctg_op->co_ctg = ctg;
	ctg_op->co_ct = CT_BTREE;

	if (!M0_IN(ctg_op->co_opcode, (CO_MIN, CO_TRUNC, CO_DROP)) &&
	    (ctg_op->co_opcode != CO_CUR ||
	     ctg_op->co_cur_phase != CPH_NEXT))
		ctg_op->co_rc = ctg_buf_get(&ctg_op->co_key, key, true);

	if (ctg_op->co_rc != 0)
		m0_fom_phase_set(ctg_op->co_fom, next_phase);
	else
		ret = ctg_op_exec(ctg_op, next_phase);

	return ret;
}

static int ctg_mem_exec(struct m0_ctg_op *ctg_op,
			int               next_phase)
{
	ctg_op->co_ct = CT_MEM;
	return ctg_mem_op_exec(ctg_op, next_phase);
}

M0_INTERNAL int m0_ctg_insert(struct m0_ctg_op    *ctg_op,
			      struct m0_cas_ctg   *ctg,
			      const struct m0_buf *key,
			      const struct m0_buf *val,
			      int                  next_phase)
{
	M0_PRE(ctg_op != NULL);
	M0_PRE(ctg != NULL);
	M0_PRE(key != NULL);
	M0_PRE(val != NULL);
	M0_PRE(ctg_op->co_beop.bo_sm.sm_state == M0_BOS_INIT);

	ctg_op->co_opcode = CO_PUT;
	ctg_op->co_val = *val;
	return ctg_exec(ctg_op, ctg, key, next_phase);
}

M0_INTERNAL int m0_ctg_lookup_delete(struct m0_ctg_op    *ctg_op,
				     struct m0_cas_ctg   *ctg,
				     const struct m0_buf *key,
				     struct m0_buf       *val,
				     int                  flags,
				     int                  next_phase)
{
	struct m0_fom             *fom0;
	int                        ret = M0_FSO_AGAIN;

	M0_PRE(ctg_op != NULL);
	M0_PRE(ctg != NULL);
	M0_PRE(key != NULL);
	M0_PRE(val != NULL);
	M0_PRE(ctg_op->co_beop.bo_sm.sm_state == M0_BOS_INIT);

	fom0 = ctg_op->co_fom;

	/* Lookup the value first. */
	ctg_op->co_ctg = ctg;
	ctg_op->co_ct = CT_BTREE;
	ctg_op->co_opcode = CO_GET;

	/* Here pass true to allow failures injection. */
	ctg_op->co_rc = ctg_buf_get(&ctg_op->co_key, key, true);

	/* Pass -1 as the next_phase to indicate we don't set it now. */
	if (ctg_op->co_rc == 0)
		ctg_op_exec(ctg_op, -1);
	if (ctg_op->co_rc != 0) {
		m0_fom_phase_set(fom0, next_phase);
		return ret;
	}

	/*
	 * Copy value with allocation because it refers the memory
	 * chunk that will not be avilable after the delete op.
	 */
	m0_buf_copy(val, &ctg_op->co_out_val);
	m0_ctg_op_fini(ctg_op);

	/* Now delete the value by key. */
	m0_ctg_op_init(ctg_op, fom0, flags);

	ctg_op->co_ctg = ctg;
	ctg_op->co_ct = CT_BTREE;
	ctg_op->co_opcode = CO_DEL;

	/*
	 * Here pass false to disallow failures injecations. Some
	 * UT are tailored with idea in mind that only one injection
	 * of particular type is touched at a run. ctg_buf_get() has
	 * the falure injection that we need to avoid here to make
	 * cas UT happy.
	 */
	ctg_op->co_rc = ctg_buf_get(&ctg_op->co_key, key, false);

	if (ctg_op->co_rc != 0) {
		m0_buf_free(val);
		m0_fom_phase_set(fom0, next_phase);
		return ret;
	}

	ret = ctg_op_exec(ctg_op, next_phase);
	if (ctg_op->co_rc != 0)
		m0_buf_free(val);

	return ret;
}

M0_INTERNAL int m0_ctg_delete(struct m0_ctg_op    *ctg_op,
			      struct m0_cas_ctg   *ctg,
			      const struct m0_buf *key,
			      int                  next_phase)
{
	M0_PRE(ctg_op != NULL);
	M0_PRE(ctg != NULL);
	M0_PRE(key != NULL);
	M0_PRE(ctg_op->co_beop.bo_sm.sm_state == M0_BOS_INIT);

	ctg_op->co_opcode = CO_DEL;

	return ctg_exec(ctg_op, ctg, key, next_phase);
}

M0_INTERNAL int m0_ctg_lookup(struct m0_ctg_op    *ctg_op,
			      struct m0_cas_ctg   *ctg,
			      const struct m0_buf *key,
			      int                  next_phase)
{
	M0_PRE(ctg_op != NULL);
	M0_PRE(ctg != NULL);
	M0_PRE(key != NULL);
	M0_PRE(ctg_op->co_beop.bo_sm.sm_state == M0_BOS_INIT);

	ctg_op->co_opcode = CO_GET;

	return ctg_exec(ctg_op, ctg, key, next_phase);
}

M0_INTERNAL void m0_ctg_lookup_result(struct m0_ctg_op *ctg_op,
				      struct m0_buf    *buf)
{
	M0_PRE(ctg_op != NULL);
	M0_PRE(ctg_op->co_beop.bo_sm.sm_state == M0_BOS_DONE);
	M0_PRE(ctg_op->co_opcode == CO_GET);
	M0_PRE(ctg_op->co_ct == CT_BTREE);
	M0_PRE(ctg_op->co_rc == 0);

	*buf = ctg_op->co_out_val;
}

M0_INTERNAL int m0_ctg_minkey(struct m0_ctg_op  *ctg_op,
			      struct m0_cas_ctg *ctg,
			      int                next_phase)
{
	M0_PRE(ctg_op != NULL);
	M0_PRE(ctg != NULL);

	ctg_op->co_opcode = CO_MIN;

	return ctg_exec(ctg_op, ctg, NULL, next_phase);
}


M0_INTERNAL int m0_ctg_truncate(struct m0_ctg_op    *ctg_op,
				struct m0_cas_ctg   *ctg,
				m0_bcount_t          limit,
				int                  next_phase)
{
	M0_PRE(ctg_op != NULL);
	M0_PRE(ctg != NULL);

	ctg_op->co_opcode = CO_TRUNC;
	ctg_op->co_cnt = limit;

	return ctg_exec(ctg_op, ctg, NULL, next_phase);
}

M0_INTERNAL int m0_ctg_drop(struct m0_ctg_op    *ctg_op,
			    struct m0_cas_ctg   *ctg,
			    int                  next_phase)
{
	M0_PRE(ctg_op != NULL);
	M0_PRE(ctg != NULL);

	ctg_op->co_opcode = CO_DROP;

	return ctg_exec(ctg_op, ctg, NULL, next_phase);
}

M0_INTERNAL bool m0_ctg_cursor_is_initialised(struct m0_ctg_op *ctg_op)
{
	M0_PRE(ctg_op != NULL);

	return ctg_op->co_cur_initialised;
}

M0_INTERNAL void m0_ctg_cursor_init(struct m0_ctg_op  *ctg_op,
				    struct m0_cas_ctg *ctg)
{
	M0_PRE(ctg_op != NULL);
	M0_PRE(ctg != NULL);

	if (ctg_op->co_cur_initialised)
		return;

	ctg_op->co_ctg = ctg;
	ctg_op->co_opcode = CO_CUR;
	ctg_op->co_cur_phase = CPH_INIT;
	M0_SET0(&ctg_op->co_cur.bc_op);

	m0_btree_cursor_init(&ctg_op->co_cur, ctg_op->co_ctg->cc_tree);
	ctg_op->co_cur_initialised = true;
}

M0_INTERNAL int m0_ctg_cursor_get(struct m0_ctg_op    *ctg_op,
				  const struct m0_buf *key,
				  int                  next_phase)
{
	M0_PRE(ctg_op != NULL);
	M0_PRE(key != NULL);

	ctg_op->co_cur_phase = CPH_GET;

	/*
	 * Key may be set if cursor get is called twice between ctg operation
	 * init()/fini(), free it to avoid memory leak.
	 */
	m0_buf_free(&ctg_op->co_key);
	/* Cursor has its own beop. */
	m0_be_op_init(&ctg_op->co_cur.bc_op);

	return ctg_exec(ctg_op, ctg_op->co_ctg, key, next_phase);
}

M0_INTERNAL int m0_ctg_cursor_next(struct m0_ctg_op *ctg_op,
				   int               next_phase)
{
	M0_PRE(ctg_op != NULL);
	M0_PRE(ctg_op->co_cur_phase != CPH_INIT);

	ctg_op->co_cur_phase = CPH_NEXT;

	/* BE operation must be initialised each time when next is called. */
	m0_be_op_init(&ctg_op->co_cur.bc_op);

	return ctg_exec(ctg_op, ctg_op->co_ctg, NULL, next_phase);
}

M0_INTERNAL int m0_ctg_meta_cursor_next(struct m0_ctg_op *ctg_op,
					int               next_phase)
{
	M0_PRE(ctg_op != NULL);
	M0_PRE(ctg_op->co_cur_phase != CPH_INIT);

	ctg_op->co_cur_phase = CPH_NEXT;

	/* BE operation must be initialised each time when next called. */
	m0_be_op_init(&ctg_op->co_cur.bc_op);

	return ctg_op_exec(ctg_op, next_phase);
}

M0_INTERNAL void m0_ctg_cursor_kv_get(struct m0_ctg_op *ctg_op,
				      struct m0_buf    *key,
				      struct m0_buf    *val)
{
	M0_PRE(ctg_op != NULL);
	M0_PRE(key != NULL);
	M0_PRE(val != NULL);
	M0_PRE(ctg_op->co_opcode == CO_CUR);
	M0_PRE(ctg_op->co_rc == 0);

	*key = ctg_op->co_out_key;
	*val = ctg_op->co_out_val;
}

M0_INTERNAL void m0_ctg_meta_cursor_init(struct m0_ctg_op *ctg_op)
{
	M0_PRE(ctg_op != NULL);

	m0_ctg_cursor_init(ctg_op, ctg_store.cs_state->cs_meta);
}

M0_INTERNAL int m0_ctg_meta_cursor_get(struct m0_ctg_op    *ctg_op,
				       const struct m0_fid *fid,
				       int                  next_phase)
{
	M0_PRE(ctg_op != NULL);
	M0_PRE(fid != NULL);

	ctg_op->co_cur_phase = CPH_GET;

	/* Cursor has its own beop. */
	m0_be_op_init(&ctg_op->co_cur.bc_op);

	return ctg_meta_exec(ctg_op, fid, next_phase);
}

M0_INTERNAL void m0_ctg_cursor_put(struct m0_ctg_op *ctg_op)
{
	m0_btree_cursor_put(&ctg_op->co_cur);
}

M0_INTERNAL void m0_ctg_cursor_fini(struct m0_ctg_op *ctg_op)
{
	M0_PRE(ctg_op != NULL);

	m0_btree_cursor_fini(&ctg_op->co_cur);
	M0_SET0(&ctg_op->co_cur);
	ctg_op->co_cur_initialised = false;
	ctg_op->co_cur_phase = CPH_NONE;
}

M0_INTERNAL void m0_ctg_op_init(struct m0_ctg_op *ctg_op,
				struct m0_fom    *fom,
				uint32_t          flags)
{
	M0_ENTRY("ctg_op=%p flags=0x%x", ctg_op, flags);
	M0_PRE(ctg_op != NULL);
	M0_PRE(fom != NULL);

	M0_SET0(ctg_op);

	m0_be_op_init(&ctg_op->co_beop);
	m0_mutex_init(&ctg_op->co_channel_lock);
	m0_chan_init(&ctg_op->co_channel, &ctg_op->co_channel_lock);
	m0_clink_init(&ctg_op->co_clink, ctg_op_cb);
	ctg_op->co_fom = fom;
	ctg_op->co_flags = flags;
	ctg_op->co_cur_phase = CPH_NONE;
}

M0_INTERNAL int m0_ctg_op_rc(struct m0_ctg_op *ctg_op)
{
	M0_PRE(ctg_op != NULL);

	if (M0_FI_ENABLED("be-failure"))
		return M0_ERR(-ENOMEM);
	return ctg_op->co_rc;
}

M0_INTERNAL void m0_ctg_op_fini(struct m0_ctg_op *ctg_op)
{
	M0_ENTRY("ctg_op=%p", ctg_op);
	M0_PRE(ctg_op != NULL);
	M0_PRE(ctg_op->co_cur_initialised == false);

	m0_buf_free(&ctg_op->co_key);
	m0_chan_fini_lock(&ctg_op->co_channel);
	m0_mutex_fini(&ctg_op->co_channel_lock);
	m0_clink_fini(&ctg_op->co_clink);
	m0_be_op_fini(&ctg_op->co_beop);
	M0_SET0(ctg_op);
	M0_LEAVE();
}

M0_INTERNAL void m0_ctg_mark_deleted_credit(struct m0_be_tx_credit *accum)
{
	m0_bcount_t         knob;
	m0_bcount_t         vnob;
	struct m0_btree    *btree  = ctg_store.cs_state->cs_meta->cc_tree;
	struct m0_btree    *mbtree = m0_ctg_dead_index()->cc_tree;
	struct m0_cas_ctg  *ctg;

	knob = M0_CAS_CTG_KV_HDR_SIZE + sizeof(struct m0_fid);
	vnob = M0_CAS_CTG_KV_HDR_SIZE + sizeof(ctg);
	/* Delete from meta. */
	m0_btree_del_credit(btree, 1, knob, vnob, accum);
	knob = M0_CAS_CTG_KV_HDR_SIZE + sizeof(ctg);
	vnob = 8;
	/* Insert into dead index. */
	m0_btree_put_credit(mbtree, 1, knob, vnob, accum);
}

M0_INTERNAL void m0_ctg_create_credit(struct m0_be_tx_credit *accum)
{
	m0_bcount_t             knob;
	m0_bcount_t             vnob;
	struct m0_btree        *btree = ctg_store.cs_state->cs_meta->cc_tree;
	struct m0_cas_ctg      *ctg;
	struct m0_btree_type    bt    = {
		.tt_id = M0_BT_CAS_CTG,
		.ksize = -1,
		.vsize = -1,
		};

	m0_btree_create_credit(&bt, accum, 1);

	knob = M0_CAS_CTG_KV_HDR_SIZE + sizeof(struct m0_fid);
	vnob = M0_CAS_CTG_KV_HDR_SIZE + sizeof(ctg);
	m0_btree_put_credit(btree, 1, knob, vnob, accum);
	/*
	 * That are credits for cas_ctg body.
	 */
	m0_be_allocator_credit(NULL, M0_BAO_ALLOC, sizeof *ctg, 0, accum);
}

M0_INTERNAL void m0_ctg_drop_credit(struct m0_fom          *fom,
				    struct m0_be_tx_credit *accum,
				    struct m0_cas_ctg      *ctg,
				    m0_bcount_t            *limit)
{
	m0_btree_truncate_credit(m0_fom_tx(fom), ctg->cc_tree, accum, limit);
}

M0_INTERNAL void m0_ctg_dead_clean_credit(struct m0_be_tx_credit *accum)
{
	struct m0_cas_ctg  *ctg;
	m0_bcount_t         knob;
	m0_bcount_t         vnob;
	struct m0_btree    *btree = m0_ctg_dead_index()->cc_tree;
	/*
	 * Define credits for delete from dead index.
	 */
	knob = M0_CAS_CTG_KV_HDR_SIZE + sizeof(ctg);
	vnob = M0_CAS_CTG_KV_HDR_SIZE;
	m0_btree_del_credit(btree, 1, knob, vnob, accum);
	/*
	 * Credits for ctg free.
	 */
	m0_be_allocator_credit(NULL, M0_BAO_FREE, sizeof *ctg, 0, accum);
}

M0_INTERNAL void m0_ctg_insert_credit(struct m0_cas_ctg      *ctg,
				      m0_bcount_t             knob,
				      m0_bcount_t             vnob,
				      struct m0_be_tx_credit *accum)
{
	m0_btree_put_credit(ctg->cc_tree, 1, knob, vnob, accum);
}

M0_INTERNAL void m0_ctg_delete_credit(struct m0_cas_ctg      *ctg,
				      m0_bcount_t             knob,
				      m0_bcount_t             vnob,
				      struct m0_be_tx_credit *accum)
{
	m0_btree_del_credit(ctg->cc_tree, 1, knob, vnob, accum);
}

static void ctg_ctidx_op_credits(struct m0_cas_id       *cid,
				 bool                    insert,
				 struct m0_be_tx_credit *accum)
{
	const struct m0_dix_imask *imask;
	struct m0_btree           *btree = ctg_store.cs_ctidx->cc_tree;
	m0_bcount_t                knob;
	m0_bcount_t                vnob;

	knob = M0_CAS_CTG_KV_HDR_SIZE + sizeof(struct m0_fid);
	vnob = M0_CAS_CTG_KV_HDR_SIZE + sizeof(struct m0_dix_layout);
	if (insert)
		m0_btree_put_credit(btree, 1, knob, vnob, accum);
	else
		m0_btree_del_credit(btree, 1, knob, vnob, accum);

	imask = &cid->ci_layout.u.dl_desc.ld_imask;
	if (!m0_dix_imask_is_empty(imask)) {
		if (insert)
			m0_be_allocator_credit(NULL, M0_BAO_ALLOC,
					       imask->im_nr *
					       sizeof(imask->im_range[0]),
					       0, accum);
		else
			m0_be_allocator_credit(NULL, M0_BAO_FREE,
					       imask->im_nr *
					       sizeof(imask->im_range[0]),
					       0, accum);

	}
}

M0_INTERNAL void m0_ctg_ctidx_insert_credits(struct m0_cas_id       *cid,
					     struct m0_be_tx_credit *accum)
{
	M0_PRE(cid != NULL);
	M0_PRE(accum != NULL);

	ctg_ctidx_op_credits(cid, true, accum);
}

M0_INTERNAL void m0_ctg_ctidx_delete_credits(struct m0_cas_id       *cid,
					     struct m0_be_tx_credit *accum)
{
	M0_PRE(cid != NULL);
	M0_PRE(accum != NULL);

	ctg_ctidx_op_credits(cid, false, accum);
}

static int ctg_ctidx_get_cb(struct m0_btree_cb *cb, struct m0_btree_rec *rec)
{
	int           rc;
	struct m0_buf buf = {};
	struct m0_buf val = {
		.b_nob =  m0_vec_count(&rec->r_val.ov_vec),
		.b_addr = rec->r_val.ov_buf[0],
	};
	struct m0_dix_layout **layout = cb->c_datum;
	rc = ctg_buf(&val, &buf);
	if (rc == 0) {
		if (buf.b_nob == sizeof(struct m0_dix_layout))
			*layout = (struct m0_dix_layout *)buf.b_addr;
		else
			rc = M0_ERR_INFO(-EPROTO, "Unexpected: %"PRIx64,
					 buf.b_nob);
	}
	return rc;
}

M0_INTERNAL int m0_ctg_ctidx_lookup_sync(const struct m0_fid  *fid,
					 struct m0_dix_layout **layout)
{
	uint8_t              key_data[M0_CAS_CTG_KV_HDR_SIZE +
				      sizeof(struct m0_fid)];
	struct m0_buf        key;
	struct m0_cas_ctg   *ctidx     = m0_ctg_ctidx();
	struct m0_btree_op   kv_op     = {};
	struct m0_btree_rec  rec       = {};
	void                *k_ptr;
	m0_bcount_t          ksize;
	int                  rc;
	struct m0_btree_cb   get_cb    = {
		.c_act   = ctg_ctidx_get_cb,
		.c_datum = layout,
		};

	M0_PRE(fid != NULL);
	M0_PRE(layout != NULL);

	if (ctidx == NULL)
		return M0_ERR(-EFAULT);

	*layout = NULL;

	ctg_fid_key_fill(&key_data, fid);
	key = M0_BUF_INIT_PTR(&key_data);
	k_ptr = key.b_addr;
	ksize = key.b_nob;

	rec.r_key.k_data = M0_BUFVEC_INIT_BUF(&k_ptr, &ksize);

	rc = M0_BTREE_OP_SYNC_WITH_RC(&kv_op,
				      m0_btree_get(ctidx->cc_tree, &rec.r_key,
						   &get_cb, BOF_EQUAL, &kv_op));

	return M0_RC(rc);
}

struct ctg_ctidx_put_cb_data {
	struct m0_cas_ctg      *d_ctidx;
	const struct m0_cas_id *d_cid;
	struct m0_be_tx        *d_tx;
	struct m0_btree_key    *d_key;
};

static int ctg_ctidx_put_cb(struct m0_btree_cb *cb, struct m0_btree_rec *rec)
{
	m0_bcount_t                    ksize;
	struct ctg_ctidx_put_cb_data  *datum = cb->c_datum;
	struct m0_cas_ctg             *ctidx = datum->d_ctidx;
	const struct m0_cas_id        *cid   = datum->d_cid;
	struct m0_be_tx               *tx    = datum->d_tx;
	struct m0_dix_layout          *layout;
	struct m0_ext                 *im_range;
	const struct m0_dix_imask     *imask;
	m0_bcount_t                    size;

	ksize = m0_vec_count(&datum->d_key->k_data.ov_vec);
	M0_ASSERT(m0_vec_count(&rec->r_key.k_data.ov_vec) == ksize);
	m0_bufvec_copy(&rec->r_key.k_data, &datum->d_key->k_data, ksize);

	ctg_memcpy(rec->r_val.ov_buf[0], &cid->ci_layout,
	 	   sizeof(cid->ci_layout));
	imask = &cid->ci_layout.u.dl_desc.ld_imask;
	if (!m0_dix_imask_is_empty(imask)) {
		/*
		* Alloc memory in BE segment for imask ranges
		* and copy them.
		*/
		/** @todo Make it asynchronous. */
		M0_BE_ALLOC_ARR_SYNC(im_range, imask->im_nr,
					cas_seg(tx->t_engine->eng_domain),
					tx);
		size = imask->im_nr * sizeof(struct m0_ext);
		memcpy(im_range, imask->im_range, size);
		m0_be_tx_capture(tx, &M0_BE_REG(
					cas_seg(tx->t_engine->eng_domain),
					size, im_range));
		/* Assign newly allocated imask ranges. */
		layout = (struct m0_dix_layout *)
				(rec->r_val.ov_buf +
				M0_CAS_CTG_KV_HDR_SIZE);
		layout->u.dl_desc.ld_imask.im_range = im_range;
	}
	m0_chan_broadcast_lock(&ctidx->cc_chan.bch_chan);

	return 0;
}
M0_INTERNAL int m0_ctg_ctidx_insert_sync(const struct m0_cas_id *cid,
					 struct m0_be_tx        *tx)
{
	uint8_t                      key_data[M0_CAS_CTG_KV_HDR_SIZE +
					      sizeof(struct m0_fid)];
	struct m0_cas_ctg           *ctidx  = m0_ctg_ctidx();
	struct m0_buf                key;
	struct m0_btree_op           kv_op  = {};
	struct m0_btree_rec          rec    = {};
	void                        *k_ptr;
	void                        *v_ptr;
	m0_bcount_t                  ksize;
	m0_bcount_t                  vsize;
	int                          rc;
	struct ctg_ctidx_put_cb_data cb_data = {
		.d_ctidx = ctidx,
		.d_cid = cid,
		.d_tx  = tx,
		};
	struct m0_btree_cb   put_cb = {
		.c_act   = ctg_ctidx_put_cb,
		.c_datum = &cb_data,
		};

	/* The key is a component catalogue FID. */
	ctg_fid_key_fill((void *)&key_data, &cid->ci_fid);
	key   = M0_BUF_INIT_PTR(&key_data);
	k_ptr = key.b_addr;
	ksize = key.b_nob;
	vsize = M0_CAS_CTG_KV_HDR_SIZE + sizeof(struct m0_dix_layout);

	rec.r_key.k_data = M0_BUFVEC_INIT_BUF(&k_ptr, &ksize);
	rec.r_val        = M0_BUFVEC_INIT_BUF(&v_ptr, &vsize);

	rc = M0_BTREE_OP_SYNC_WITH_RC(&kv_op,
				      m0_btree_put(ctidx->cc_tree, &rec,
						   &put_cb, &kv_op, tx));
	M0_ASSERT(rc == 0);
	return M0_RC(rc);
}

M0_INTERNAL int m0_ctg_ctidx_delete_sync(const struct m0_cas_id *cid,
					 struct m0_be_tx        *tx)
{
	struct m0_dix_layout *layout;
	struct m0_dix_imask  *imask;
	uint8_t               key_data[M0_CAS_CTG_KV_HDR_SIZE +
		                       sizeof(struct m0_fid)];
	struct m0_cas_ctg    *ctidx  = m0_ctg_ctidx();
	struct m0_buf         key;
	struct m0_btree_op    kv_op  = {};
	struct m0_btree_key   r_key  = {};
	void                 *k_ptr;
	m0_bcount_t           ksize;
	int                   rc;

	/* Firstly we should free buffer allocated for imask ranges array. */
	rc = m0_ctg_ctidx_lookup_sync(&cid->ci_fid, &layout);
	if (rc != 0)
		return rc;
	imask = &layout->u.dl_desc.ld_imask;
	if (!m0_dix_imask_is_empty(imask)) {
		/** @todo Make it asynchronous. */
		M0_BE_FREE_PTR_SYNC(imask->im_range,
				    cas_seg(tx->t_engine->eng_domain),
				    tx);
		imask->im_range = NULL;
		imask->im_nr = 0;
	}
	/* The key is a component catalogue FID. */
	ctg_fid_key_fill((void *)&key_data, &cid->ci_fid);
	key = M0_BUF_INIT_PTR(&key_data);
	k_ptr = key.b_addr;
	ksize = key.b_nob;
	r_key.k_data  = M0_BUFVEC_INIT_BUF(&k_ptr, &ksize);

	/** @todo Make it asynchronous. */
	rc = M0_BTREE_OP_SYNC_WITH_RC(&kv_op, m0_btree_del(ctidx->cc_tree,
							   &r_key, NULL,
							   &kv_op, tx));

	m0_chan_broadcast_lock(&ctidx->cc_chan.bch_chan);
	return rc;
}

M0_INTERNAL int m0_ctg_mem_place(struct m0_ctg_op    *ctg_op,
				 const struct m0_buf *buf,
				 int                  next_phase)
{
	M0_PRE(ctg_op != NULL);
	M0_PRE(buf->b_nob != 0);
	M0_PRE(buf->b_addr != NULL);
	M0_PRE(ctg_op->co_beop.bo_sm.sm_state == M0_BOS_INIT);

	ctg_op->co_opcode = CO_MEM_PLACE;
	ctg_op->co_val = *buf;
	ctg_op->co_mem_buf.b_nob = buf->b_nob;
	return ctg_mem_exec(ctg_op, next_phase);
}

M0_INTERNAL void m0_ctg_mem_place_get(struct m0_ctg_op *ctg_op,
				      struct m0_buf    *buf)
{
	M0_PRE(ctg_op != NULL);
	M0_PRE(ctg_op->co_beop.bo_sm.sm_state == M0_BOS_DONE);
	M0_PRE(ctg_op->co_opcode == CO_MEM_PLACE);
	M0_PRE(ctg_op->co_ct == CT_MEM);
	M0_PRE(ctg_op->co_rc == 0);

	*buf = ctg_op->co_mem_buf;
}

M0_INTERNAL int m0_ctg_mem_free(struct m0_ctg_op *ctg_op,
				void             *area,
				int               next_phase)
{
	M0_PRE(ctg_op != NULL);
	M0_PRE(area != NULL);
	M0_PRE(ctg_op->co_beop.bo_sm.sm_state == M0_BOS_INIT);

	ctg_op->co_opcode = CO_MEM_FREE;
	ctg_op->co_mem_buf.b_addr = area;
	return ctg_mem_exec(ctg_op, next_phase);
}

M0_INTERNAL struct m0_cas_ctg *m0_ctg_meta(void)
{
	return ctg_store.cs_state->cs_meta;
}

M0_INTERNAL struct m0_cas_ctg *m0_ctg_ctidx(void)
{
	return ctg_store.cs_ctidx;
}

M0_INTERNAL struct m0_cas_ctg *m0_ctg_dead_index(void)
{
	return ctg_store.cs_dead_index;
}

M0_INTERNAL uint64_t m0_ctg_rec_nr(void)
{
	M0_ASSERT(ctg_store.cs_state != NULL);
	return ctg_store.cs_state->cs_rec_nr;
}

M0_INTERNAL uint64_t m0_ctg_rec_size(void)
{
	M0_ASSERT(ctg_store.cs_state != NULL);
        return ctg_store.cs_state->cs_rec_size;
}

M0_INTERNAL struct m0_long_lock *m0_ctg_del_lock(void)
{
	return &ctg_store.cs_del_lock;
}

M0_INTERNAL struct m0_long_lock *m0_ctg_lock(struct m0_cas_ctg *ctg)
{
	return &ctg->cc_lock.bll_u.llock;
}

static const struct m0_btree_rec_key_op key_cmp = { .rko_keycmp = ctg_cmp, };
M0_INTERNAL const struct m0_btree_rec_key_op *m0_ctg_btree_ops(void)
{
	return &key_cmp;
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
