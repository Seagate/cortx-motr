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
#include "dix/fid_convert.h"        /* m0_dix_fid_convert_cctg2dix */
#include "motr/setup.h"


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

/* Data schema for CAS catalogues { */

/** Generalised key: counted opaque data. */
struct generic_key {
	/* Actual length of gk_data array. */
	uint64_t gk_length;
	uint8_t  gk_data[0];
};
M0_BASSERT(sizeof(struct generic_key) == M0_CAS_CTG_KEY_HDR_SIZE);

/** Generalised value: versioned counted opaque data. */
struct generic_value {
	/* Actual length of gv_data array. */
	uint64_t      gv_length;
	struct m0_crv gv_version;
	uint8_t       gv_data[0];
};
M0_BASSERT(sizeof(struct generic_value) == M0_CAS_CTG_VAL_HDR_SIZE);
M0_BASSERT(sizeof(struct m0_crv) == 8);

/** The key type used in meta and ctidx catalogues. */
struct fid_key {
	struct generic_key fk_gkey;
	struct m0_fid      fk_fid;
};
M0_BASSERT(sizeof(struct generic_key) + sizeof(struct m0_fid) ==
	   sizeof(struct fid_key));

/* The value type used in meta catalogue */
struct meta_value {
	struct generic_value mv_gval;
	struct m0_cas_ctg   *mv_ctg;
};
M0_BASSERT(sizeof(struct generic_value) + sizeof(struct m0_cas_ctg *) ==
	   sizeof(struct meta_value));

/* The value type used in ctidx catalogue */
struct layout_value {
	struct generic_value lv_gval;
	struct m0_dix_layout lv_layout;
};
M0_BASSERT(sizeof(struct generic_value) + sizeof(struct m0_dix_layout) ==
	   sizeof(struct layout_value));

/* } end of schema. */

enum cursor_phase {
	CPH_NONE = 0,
	CPH_INIT,
	CPH_GET,
	CPH_NEXT
};

static struct m0_be_seg *cas_seg(struct m0_be_domain *dom);

static bool ctg_op_is_versioned(const struct m0_ctg_op *op);
// static int  ctg_berc         (struct m0_ctg_op *ctg_op);
static int  ctg_vbuf_unpack  (struct m0_buf *buf, struct m0_crv *crv);
static int  ctg_vbuf_as_ctg  (const struct m0_buf *val,
			      struct m0_cas_ctg **ctg);
static int  ctg_kbuf_unpack  (struct m0_buf *buf);
static int  ctg_kbuf_get     (struct m0_buf *dst, const struct m0_buf *src,
			      bool enabled_fi);
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
static int  ctg_dead_exec    (struct m0_ctg_op    *ctg_op,
			      struct m0_cas_ctg   *ctg,
			      const struct m0_buf *key,
			      int                  next_phase);
static int  ctg_exec         (struct m0_ctg_op    *ctg_op,
			      struct m0_cas_ctg   *ctg,
			      const struct m0_buf *key,
			      int                  next_phase);
static void ctg_store_release(struct m0_ref *ref);

static m0_bcount_t ctg_ksize (const void *key);

static int         ctg_cmp   (const void *key0, const void *key1);

static int versioned_put_sync        (struct m0_ctg_op *ctg_op);
static int versioned_get_sync        (struct m0_ctg_op *op);
static int versioned_cursor_next_sync(struct m0_ctg_op *op, bool alive_only);
static int versioned_cursor_get_sync (struct m0_ctg_op *op, bool alive_only);

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

/**
 * Returns the number of bytes required to store the given "value"
 * in on-disk format.
 */
static m0_bcount_t ctg_vbuf_packed_size(const struct m0_buf *value)
{
	return sizeof(struct generic_value) + value->b_nob;
}

/**
 * Packs a user-provided value into on-disk representation.
 * @param src A value to be packed (in-memory representation).
 * @param dst A buffer to be filled with user data and on-disk format-specific
 *            information.
 * @param crv Encoded version of the value.
 */
static void ctg_vbuf_pack(struct m0_buf       *dst,
			  const struct m0_buf *src,
			  const struct m0_crv *crv)
{
	struct generic_value *value = dst->b_addr;
	M0_PRE(dst->b_nob >= ctg_vbuf_packed_size(src));

	value->gv_length = src->b_nob;
	value->gv_version = *crv;
	memcpy(&value->gv_data, src->b_addr, src->b_nob);
}

/**
 * Allocates memory for dst buf and fills it with CAS-specific data and
 * length from src buf.
 */
static int ctg_kbuf_get(struct m0_buf *dst, const struct m0_buf *src,
			bool enabled_fi)
{
	struct generic_key *key;

	M0_ENTRY();

	if (enabled_fi && M0_FI_ENABLED("cas_alloc_fail"))
		return M0_ERR(-ENOMEM);

	key = m0_alloc(src->b_nob + sizeof(*key));
	if (key != NULL) {
		key->gk_length = src->b_nob;
		memcpy(key->gk_data, src->b_addr, src->b_nob);
		*dst = M0_BUF_INIT(src->b_nob + sizeof(*key), key);
		return M0_RC(0);
	} else
		return M0_ERR(-ENOMEM);
}

/**
 * Unpack an on-disk value data into in-memory format.
 * The function makes "buf" to point to the user-specific data associated
 * with the value (see ::generic_value::gv_data).
 * @param[out] crv Optional storage for the version of the record.
 * @return 0 or else -EPROTO if on-disk/on-wire buffer has invalid length.
 * @see ::ctg_vbuf_pack.
 */
static int ctg_vbuf_unpack(struct m0_buf *buf, struct m0_crv *crv)
{
	struct generic_value *value;

	M0_ENTRY();

	value = buf->b_addr;

	if (buf->b_nob < sizeof(*value))
		return M0_ERR_INFO(-EPROTO, "%" PRIu64 " < %" PRIu64,
				   buf->b_nob, (uint64_t)sizeof(*value));
	if (value->gv_length != buf->b_nob - sizeof(*value))
		return M0_ERR(-EPROTO);

	if (crv != NULL)
		*crv = value->gv_version;

	buf->b_nob = value->gv_length;
	buf->b_addr = &value->gv_data[0];

	return M0_RC(0);
}

/**
 * Get a m0_cas_ctg pointer from in-memory representation of a meta-index
 * value.
 */
static int ctg_vbuf_as_ctg(const struct m0_buf *buf, struct m0_cas_ctg **ctg)
{
	struct generic_value *gv = M0_AMB(gv, buf->b_addr, gv_data);
	struct meta_value    *mv = M0_AMB(mv, gv, mv_gval);

	M0_ENTRY();

	if (buf->b_nob == sizeof(struct m0_cas_ctg *)) {
		*ctg = mv->mv_ctg;
		return M0_RC(0);
	} else
		return M0_ERR(-EPROTO);
}

/**
 * Get the layout from an unpacked ::layout_value (cctidx value).
 */
static int ctg_vbuf_as_layout(const struct m0_buf    *buf,
			      struct m0_dix_layout **layout)
{
	struct generic_value *gv = M0_AMB(gv, buf->b_addr, gv_data);
	struct layout_value  *lv = M0_AMB(lv, gv, lv_gval);

	M0_ENTRY();

	if (buf->b_nob == sizeof(struct m0_dix_layout)) {
		*layout = &lv->lv_layout;
		return M0_RC(0);
	} else
		return M0_ERR(-EPROTO);
}

/**
 * Convert a versioned variable length buffer (on-disk data) into user-specific
 * data (see generic_key::gk_data).
 */
static int ctg_kbuf_unpack(struct m0_buf *buf)
{
	struct generic_key *key;

	M0_ENTRY();

	key = buf->b_addr;

	if (buf->b_nob < sizeof(*key))
		return M0_ERR(-EPROTO);
	if (key->gk_length != buf->b_nob - sizeof(*key))
		return M0_ERR(-EPROTO);

	buf->b_nob = key->gk_length;
	buf->b_addr = &key->gk_data[0];

	return M0_RC(0);
}

#define FID_KEY_INIT(__fid) (struct fid_key) { \
	.fk_gkey = {                           \
		.gk_length = sizeof(*(__fid)), \
	},                                     \
	.fk_fid = *(__fid),                    \
}

#define GENERIC_VALUE_INIT(__size) (struct generic_value) { \
	.gv_length  = __size,                               \
	.gv_version = M0_CRV_INIT_NONE,                     \
}

#define META_VALUE_INIT(__ctg_ptr) (struct meta_value) {  \
	.mv_gval = GENERIC_VALUE_INIT(sizeof(__ctg_ptr)), \
	.mv_ctg    = (__ctg_ptr),                         \
}

#define LAYOUT_VALUE_INIT(__layout) (struct layout_value) { \
	.lv_gval = GENERIC_VALUE_INIT(sizeof(*(__layout))), \
	.lv_layout = *(__layout),                           \
}

static m0_bcount_t ctg_ksize(const void *opaque_key)
{
	const struct generic_key *key = opaque_key;
	return sizeof(*key) + key->gk_length;
}

static int ctg_cmp(const void *opaque_key_left, const void *opaque_key_right)
{
	const struct generic_key *left  = opaque_key_left;
	const struct generic_key *right = opaque_key_right;

	/*
	 * XXX: Origianally, there was an assertion to ensure on-disk data
	 * is correct, and it looked like that:
	 *     u64 len  = *(u64 *)key;
	 *     u64 knob = 8 + len;
	 *     assert(knob >= 8);
	 * The problem here is that "len" is always >= 0 (because it is a u64).
	 * Therefore, the assertion knob >= 8 is always true as well.
	 */

	return memcmp(left->gk_data, right->gk_data,
		      min_check(left->gk_length, right->gk_length)) ?:
		M0_3WAY(left->gk_length, right->gk_length);
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
	/* See comment in m0_be_btree_init(). */
	M0_SET0(&ctg->cc_chan_guard.bm_u.mutex);
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
	struct m0_btree_op b_op = {};
	int                rc;

	M0_ENTRY("ctg=%p", ctg);

	ctg->cc_inited = false;
	if (ctg->cc_tree != NULL) {
		rc = M0_BTREE_OP_SYNC_WITH_RC(&b_op,
					   m0_btree_close(ctg->cc_tree, &b_op));
		M0_ASSERT(rc == 0);
		m0_free0(&ctg->cc_tree);
	}
	m0_long_lock_fini(m0_ctg_lock(ctg));
	m0_chan_fini_lock(&ctg->cc_chan.bch_chan);
	m0_mutex_fini(&ctg->cc_chan_guard.bm_u.mutex);
}

int m0_ctg_create(struct m0_be_seg *seg, struct m0_be_tx *tx,
		  struct m0_cas_ctg **out,
		  const struct m0_fid *cas_fid, enum cas_tree_type ctype)
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
	};

	M0_PRE(M0_IN(ctype, (CTT_CTG, CTT_META, CTT_DEADIDX, CTT_CTIDX)));

	switch (ctype) {
		case CTT_CTG:
			bt.ksize = -1;
			bt.vsize = -1;
			break;
		case CTT_META:
			bt.ksize = sizeof(struct fid_key);
			bt.vsize = sizeof(struct meta_value);
			break;
		case CTT_DEADIDX:
			bt.ksize = sizeof(struct generic_key *) + sizeof(void *);
			bt.vsize = sizeof(void *);
			break;
		case CTT_CTIDX:
			bt.ksize = sizeof(struct fid_key);
			bt.vsize = sizeof(struct layout_value);
			break;
	}

	if (M0_FI_ENABLED("ctg_create_failure"))
		return M0_ERR(-EFAULT);

	M0_BE_ALLOC_ALIGN_PTR_SYNC(ctg, M0_CTG_SHIFT, seg, tx);
	if (ctg == NULL)
		return M0_ERR(-ENOMEM);

	ctg_init(ctg, seg);
	M0_ALLOC_PTR(ctg->cc_tree);
	if (ctg->cc_tree == NULL)
		return M0_ERR(-ENOMEM);

	rc = M0_BTREE_OP_SYNC_WITH_RC(&b_op,
				      m0_btree_create(&ctg->cc_node,
						      sizeof ctg->cc_node,
						      &bt, M0_BCT_NO_CRC,
						      &b_op, ctg->cc_tree,
						      seg, fid, tx, &key_cmp));
	if (rc != 0) {
		m0_free0(&ctg->cc_tree);
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
	m0_free0(&ctg->cc_tree);
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
	struct m0_buf        val = {
		.b_nob =  m0_vec_count(&rec->r_val.ov_vec),
		.b_addr = rec->r_val.ov_buf[0],
	};
	int                  rc;

	rc = ctg_vbuf_unpack(&val, NULL) ? :ctg_vbuf_as_ctg(&val, ctg);

	return rc;
}

/**
 * Lookup catalogue with provided fid in meta-catalogue synchronously.
 */
M0_INTERNAL int m0_ctg_meta_find_ctg(struct m0_cas_ctg    *meta,
			             const struct m0_fid  *ctg_fid,
			             struct m0_cas_ctg   **ctg)
{
	int                  rc;
	struct m0_btree_op   kv_op    = {};
	struct fid_key       key_data = FID_KEY_INIT(ctg_fid);
	struct m0_buf        key      = M0_BUF_INIT(sizeof(key_data),
						    &key_data);
	struct m0_btree_rec  rec      = {
		.r_key.k_data = M0_BUFVEC_INIT_BUF(&key.b_addr, &key.b_nob),
		};
	struct m0_btree_cb   get_cb = {
		.c_act = ctg_meta_find_cb,
		.c_datum = ctg,
		};

	*ctg = NULL;

	rc = M0_BTREE_OP_SYNC_WITH_RC(&kv_op,
				      m0_btree_get(meta->cc_tree, &rec.r_key,
						   &get_cb, BOF_EQUAL, &kv_op));
	return M0_RC(rc);
}


static int ctg_meta_put_cb(struct m0_btree_cb *cb, struct m0_btree_rec *rec)
{
	m0_bcount_t          ksize;
	m0_bcount_t          vsize;
	struct m0_btree_rec *user_rec = cb->c_datum;

	ksize = m0_vec_count(&user_rec->r_key.k_data.ov_vec);
	M0_ASSERT(m0_vec_count(&rec->r_key.k_data.ov_vec) == ksize);
	m0_bufvec_copy(&rec->r_key.k_data, &user_rec->r_key.k_data, ksize);

	vsize = m0_vec_count(&user_rec->r_val.ov_vec);
	M0_ASSERT(m0_vec_count(&rec->r_val.ov_vec) == vsize);
	m0_bufvec_copy(&rec->r_val, &user_rec->r_val, vsize);

	return 0;
}

M0_INTERNAL int m0_ctg__meta_insert(struct m0_btree     *meta,
				    const struct m0_fid *fid,
				    struct m0_cas_ctg   *ctg,
				    struct m0_be_tx     *tx)
{
	int                        rc;
	struct fid_key             key_data = FID_KEY_INIT(fid);
	struct meta_value          val_data = META_VALUE_INIT(ctg);
	struct m0_buf              key      = M0_BUF_INIT_PTR(&key_data);
	struct m0_buf              value    = M0_BUF_INIT_PTR(&val_data);
	struct m0_btree_op          kv_op   = {};
	struct m0_btree_rec         rec     = {
		.r_key.k_data = M0_BUFVEC_INIT_BUF(&key.b_addr, &key.b_nob),
		.r_val        = M0_BUFVEC_INIT_BUF(&value.b_addr, &value.b_nob),
		.r_crc_type   = M0_BCT_NO_CRC,
	};
	struct m0_btree_cb          put_cb  = {
		.c_act   = ctg_meta_put_cb,
		.c_datum = &rec,
	};

	/*
	 * NOTE: ctg may be equal to NULL.
	 * Meta-catalogue is a catalogue. Also, it keeps a list of all
	 * catalogues. In order to avoid paradoxes, we allow this function to
	 * accept an empty pointer when the meta-catalogue is trying to
	 * insert itself into the list (see ::ctg_meta_selfadd).
	 * It also helps to generalise listing of catalogues
	 * (see ::m0_ctg_try_init).
	 */
	M0_PRE(ergo(ctg == NULL, m0_fid_eq(fid, &m0_cas_meta_fid)));

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
	struct fid_key       key_data = FID_KEY_INIT(fid);
	struct m0_buf        key      = M0_BUF_INIT_PTR(&key_data);
	struct m0_btree_key  r_key    = {
		.k_data = M0_BUFVEC_INIT_BUF(&key.b_addr, &key.b_nob),
		};
	struct m0_btree_op   kv_op    = {};
	int                  rc;

	M0_ENTRY();

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
	m0_btree_put_credit2(bt, M0_CTG_ROOT_NODE_SIZE, nr,
			     sizeof(struct fid_key),
			     sizeof(struct meta_value),
			     accum);
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

	m0_btree_del_credit2(bt, M0_CTG_ROOT_NODE_SIZE, nr,
			     sizeof(struct fid_key),
			     sizeof(struct meta_value),
			     accum);
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

	rc = m0_ctg_create(seg, tx, &out->cs_meta, &m0_cas_meta_fid, CTT_META);
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
	/* See comment in m0_be_btree_init(). */
	M0_SET0(&state->cs_ctg_init_mutex.bm_u.mutex);
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
	struct m0_be_tx         tx    = {};
	struct m0_be_tx_credit  cred  = M0_BE_TX_CREDIT(0, 0);
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
	/* See comment in m0_be_btree_init(). */
	M0_SET0(&state->cs_ctg_init_mutex.bm_u.mutex);
	m0_mutex_init(&state->cs_ctg_init_mutex.bm_u.mutex);

	/* Create catalog-index catalogue. */
	rc = m0_ctg_create(seg, &tx, &ctidx, &m0_cas_ctidx_fid, CTT_CTIDX);
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
	rc = m0_ctg_create(seg, &tx, &dead_index, &m0_cas_dead_index_fid,
			   CTT_DEADIDX);
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
	/* TODO: Clean up every index in memory tree allocation upon any CAS
	   operation on the index */
	ctg_fini(ctg_store->cs_state->cs_meta);
	ctg_fini(ctg_store->cs_ctidx);
	ctg_fini(ctg_store->cs_dead_index);
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
	struct m0_ctg_op  *ctg_op   = M0_AMB(ctg_op, clink, co_clink);
	struct m0_be_op   *op       = ctg_beop(ctg_op);
	int                opc      = ctg_op->co_opcode;
	int                ct       = ctg_op->co_ct;
	struct m0_be_tx   *tx       = &ctg_op->co_fom->fo_tx.tx_betx;
	struct m0_chan    *ctg_chan = &ctg_op->co_ctg->cc_chan.bch_chan;
	struct m0_cas_ctg *ctg;
	int                rc;

	if (op->bo_sm.sm_state != M0_BOS_DONE)
		return true;

	/* Versioned API is implemented with synchronous btree operations. */
	if (ctg_op->co_is_versioned) {
		M0_ASSERT(0);
		return true;
	}
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
			rc = ctg_kbuf_unpack(&ctg_op->co_out_key);
			break;
		case CTG_OP_COMBINE(CO_CUR, CT_META):
		case CTG_OP_COMBINE(CO_CUR, CT_BTREE):
			M0_PRE(m0_be_op_is_done(&ctg_op->co_cur.bc_op));
			m0_btree_cursor_kv_get(&ctg_op->co_cur,
						  &ctg_op->co_out_key,
						  &ctg_op->co_out_val);
			rc = ctg_kbuf_unpack(&ctg_op->co_out_key) ?:
				ctg_vbuf_unpack(&ctg_op->co_out_val, NULL);
			if (rc == 0 && ct == CT_META) {
				rc = ctg_vbuf_as_ctg(&ctg_op->co_out_val, &ctg);
				if (rc == 0)
					m0_ctg_try_init(ctg);
			}
			break;
		case CTG_OP_COMBINE(CO_MEM_PLACE, CT_MEM):
			/*
			 * Copy user provided buffer to the buffer allocated in
			 * BE and capture it.
			 */
			m0_buf_memcpy(&ctg_op->co_mem_buf, &ctg_op->co_val);
			M0_BE_TX_CAPTURE_BUF(cas_seg(tx->t_engine->eng_domain),
					     &ctg_op->co_fom->fo_tx.tx_betx,
					     &ctg_op->co_mem_buf);
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
		rc = 0;

	ctg_op->co_rc = M0_RC(rc);
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
	/**
	 * ctg will be created before META put operation in ctg_op_exec_normal
	 * and provided to callback, ctg_op_cb_btree to add into meta tree.
	*/
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
	struct m0_cas_ctg      *ctg;
	struct meta_value      *mv;
	m0_bcount_t             btree_ksize;
	struct m0_buf           btree_val;
	int                     rc;

	if (ctg_op->co_is_versioned) {
		M0_ASSERT(0);
		return true;
	}

	M0_PRE(M0_IN(opc,(CO_GET, CO_PUT, CO_MIN)));
	if (opc == CO_PUT) {
		btree_ksize = m0_vec_count(&rec->r_key.k_data.ov_vec);
		M0_ASSERT(btree_ksize == ctg_op->co_key.b_nob);
		m0_memmove(rec->r_key.k_data.ov_buf[0], ctg_op->co_key.b_addr,
			   btree_ksize);
	}

	switch (CTG_OP_COMBINE(opc, ct)) {
	case CTG_OP_COMBINE(CO_GET, CT_BTREE):
	case CTG_OP_COMBINE(CO_GET, CT_META):
		m0_buf_init(&ctg_op->co_out_val, rec->r_val.ov_buf[0],
			    m0_vec_count(&rec->r_val.ov_vec));
		rc = ctg_vbuf_unpack(&ctg_op->co_out_val, NULL);
		if (rc == 0 && ct == CT_META) {
			rc = ctg_vbuf_as_ctg(&ctg_op->co_out_val, &ctg);
			if (rc == 0)
				m0_ctg_try_init(ctg);
		}
		M0_ASSERT(rc == 0);
		break;
	case CTG_OP_COMBINE(CO_PUT, CT_DEAD_INDEX):
		m0_chan_broadcast_lock(ctg_chan);
		break;
	case CTG_OP_COMBINE(CO_PUT, CT_BTREE):
		m0_buf_init(&btree_val, rec->r_val.ov_buf[0],
			    m0_vec_count(&rec->r_val.ov_vec));

		ctg_vbuf_pack(&btree_val, &ctg_op->co_val, &M0_CRV_INIT_NONE);
		if (ctg_is_ordinary(ctg_op->co_ctg))
			m0_ctg_state_inc_update(tx,
				ctg_op->co_key.b_nob -
				sizeof(struct generic_key) +
				ctg_op->co_val.b_nob);
		else
			m0_chan_broadcast_lock(ctg_chan);
		break;
	case CTG_OP_COMBINE(CO_PUT, CT_META):
		ctg = datum->d_cas_ctg;
		/*
		* After successful insert inplace fill value of meta by length &
		* pointer to cas_ctg. m0_ctg_create() creates cas_ctg,
		* including memory alloc.
		*/
		mv = rec->r_val.ov_buf[0];
		*mv = META_VALUE_INIT(ctg);
		m0_chan_broadcast_lock(ctg_chan);
		break;
	case CTG_OP_COMBINE(CO_MIN, CT_BTREE):
		btree_ksize = m0_vec_count(&rec->r_key.k_data.ov_vec);
		M0_ASSERT(btree_ksize ==
			  ctg_ksize(rec->r_key.k_data.ov_buf[0]));
		m0_buf_init(&ctg_op->co_out_key, rec->r_key.k_data.ov_buf[0],
			    btree_ksize);
		break;
	}
	return 0;
}

static int ctg_op_exec_normal(struct m0_ctg_op *ctg_op, int next_phase)
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
	struct m0_btree_cb         cb      = {};
	int                        rc      = 0;
	void                      *k_ptr;
	void                      *v_ptr;
	m0_bcount_t                ksize;
	m0_bcount_t                vsize;

	cb_data.d_ctg_op = ctg_op;
	cb.c_act   = ctg_op_cb_btree;
	cb.c_datum = &cb_data;

	k_ptr = key->b_addr;
	ksize = key->b_nob;

	switch (CTG_OP_COMBINE(opc, ct)) {
	case CTG_OP_COMBINE(CO_PUT, CT_BTREE):
		m0_be_op_active(beop);

		vsize = sizeof(struct generic_value) + ctg_op->co_val.b_nob;
		rec.r_key.k_data = M0_BUFVEC_INIT_BUF(&k_ptr, &ksize);
		rec.r_val        = M0_BUFVEC_INIT_BUF(&v_ptr, &vsize);
		rec.r_crc_type   = M0_BCT_NO_CRC;

		if (!!(ctg_op->co_flags & COF_OVERWRITE)) {
			rc = M0_BTREE_OP_SYNC_WITH_RC(&kv_op,
					m0_btree_update(btree, &rec, &cb,
							BOF_INSERT_IF_NOT_FOUND,
							&kv_op, tx));
			M0_ASSERT(rc == 0);
		}
		else
			rc = M0_BTREE_OP_SYNC_WITH_RC(&kv_op,
						      m0_btree_put(btree, &rec,
								   &cb, &kv_op,
								   tx));
		m0_be_op_done(beop);
		break;
	case CTG_OP_COMBINE(CO_PUT, CT_META): {
		struct m0_cas_ctg *cas_ctg;
		struct fid_key    *fk = ctg_op->co_key.b_addr;

		m0_be_op_active(beop);
		M0_ASSERT(!(ctg_op->co_flags & COF_OVERWRITE));

		rc = m0_ctg_create(cas_seg(tx->t_engine->eng_domain), tx,
				   &cas_ctg,
				   /*
				    *Meta key have a fid of index/ctg store.
				    * It is located after KV header.
				    */
				   &fk->fk_fid, CTT_CTG);
		M0_ASSERT(rc == 0);

		vsize = sizeof(struct meta_value);
		rec.r_key.k_data  = M0_BUFVEC_INIT_BUF(&k_ptr, &ksize);
		rec.r_val         = M0_BUFVEC_INIT_BUF(&v_ptr, &vsize);
		rec.r_crc_type    = M0_BCT_NO_CRC;
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

		vsize = sizeof(void *);
		rec.r_key.k_data = M0_BUFVEC_INIT_BUF(&k_ptr, &ksize);
		rec.r_val        = M0_BUFVEC_INIT_BUF(&v_ptr, &vsize);
		rec.r_crc_type   = M0_BCT_NO_CRC;

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
	case CTG_OP_COMBINE(CO_DEL, CT_DEAD_INDEX):
		ksize = sizeof(struct generic_key *) + sizeof(void *);
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
		M0_ASSERT(M0_IN(ctg_op->co_cur_phase, (CPH_GET, CPH_NEXT)));
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
	return M0_RC(ctg_op_tick_ret(ctg_op, next_phase));
}

static int ctg_op_exec_versioned(struct m0_ctg_op *ctg_op, int next_phase)
{
	int              rc;
	int              opc = ctg_op->co_opcode;
	int              ct  = ctg_op->co_ct;
	struct m0_be_op *beop = ctg_beop(ctg_op);
	bool             alive_only = !(ctg_op->co_flags & COF_SHOW_DEAD);
	M0_ENTRY();

	M0_PRE(ctg_is_ordinary(ctg_op->co_ctg));
	M0_PRE(ct == CT_BTREE);
	M0_PRE(ergo(opc == CO_CUR,
		    M0_IN(ctg_op->co_cur_phase, (CPH_GET, CPH_NEXT))));
	switch (CTG_OP_COMBINE(opc, ct)) {
	case CTG_OP_COMBINE(CO_PUT, CT_BTREE):
	case CTG_OP_COMBINE(CO_DEL, CT_BTREE):
		rc = versioned_put_sync(ctg_op);
		break;
	case CTG_OP_COMBINE(CO_GET, CT_BTREE):
		rc = versioned_get_sync(ctg_op);
		break;
	case CTG_OP_COMBINE(CO_CUR, CT_BTREE):
		if (ctg_op->co_cur_phase == CPH_GET)
			rc = versioned_cursor_get_sync(ctg_op, alive_only);
		else
			rc = versioned_cursor_next_sync(ctg_op, alive_only);
		break;
	default:
		M0_IMPOSSIBLE("The other operations are not allowed here.");
	}
	ctg_op->co_rc = rc;

	/* It is either untouched or DONE right away. */
	M0_ASSERT(M0_IN(beop->bo_sm.sm_state, (M0_BOS_INIT, M0_BOS_DONE)));
	/* Only cursor operation may reach DONE state. */
	M0_ASSERT(ergo(beop->bo_sm.sm_state == M0_BOS_DONE, opc == CO_CUR));

	if (opc == CO_CUR) {
		/*
		 * BE cursor has its own BE operation. It gets initialised
		 * everytime in m0_ctg_cursor_* and m0_ctg_meta_cursor_*.
		 * Since a ctg op may be re-used (for example, as a part
		 * of GET+NEXT sequence of calls), we need to make it
		 * ready to be initialised again.
		 */
		m0_be_op_fini(beop);
		M0_SET0(beop);
	} else {
		/*
		 * Non-CO_CUR operations are completely ignoring the BE op
		 * embedded into ctg_op. They are operating in synchrnonous
		 * manner on local ops. Thus, we need to advance the state
		 * of the embedded op to let the other parts function
		 * properly (for example, ::m0_ctg_lookup_result).
		 */
		m0_be_op_active(beop);
		m0_be_op_done(beop);
	}

	if (next_phase >= 0)
		m0_fom_phase_set(ctg_op->co_fom, next_phase);
	return M0_RC(M0_FSO_AGAIN);
}

static int ctg_op_exec(struct m0_ctg_op *ctg_op, int next_phase)
{
	M0_ENTRY();
	ctg_op->co_is_versioned = ctg_op_is_versioned(ctg_op);

	return ctg_op->co_is_versioned ?
		ctg_op_exec_versioned(ctg_op, next_phase) :
		ctg_op_exec_normal(ctg_op, next_phase);
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
	M0_ENTRY();

	ctg_op->co_ctg = ctg_store.cs_state->cs_meta;
	ctg_op->co_ct = CT_META;

	if (ctg_op->co_opcode != CO_GC &&
	    (ctg_op->co_opcode != CO_CUR ||
	     ctg_op->co_cur_phase != CPH_NEXT))
		ctg_op->co_rc = ctg_kbuf_get(&ctg_op->co_key,
					     &M0_BUF_INIT_PTR_CONST(fid), true);
	if (ctg_op->co_rc != 0) {
		ret = M0_FSO_AGAIN;
		m0_fom_phase_set(ctg_op->co_fom, next_phase);
	} else
		ret = ctg_op_exec(ctg_op, next_phase);

	return M0_RC(ret);
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
	M0_ENTRY();

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

	ctg_op->co_rc = ctg_op->co_rc ?:
		ctg_vbuf_as_ctg(&ctg_op->co_out_val, &ctg);

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
	ctg_op->co_opcode = CO_PUT;
	/* Dead index value is empty */
	ctg_op->co_val = M0_BUF_INIT0;
	/* Dead index key is a pointer to a catalogue */
	return ctg_dead_exec(ctg_op, ctg, &M0_BUF_INIT_PTR(&ctg), next_phase);
}

M0_INTERNAL int m0_ctg_dead_delete(struct m0_ctg_op    *ctg_op,
				   struct m0_cas_ctg   *ctg,
				   const struct m0_buf *key,
				   int                  next_phase)
{
	M0_PRE(ctg_op != NULL);
	M0_PRE(ctg != NULL);
	M0_PRE(key != NULL);
	M0_PRE(ctg_op->co_beop.bo_sm.sm_state == M0_BOS_INIT);

	ctg_op->co_opcode = CO_DEL;

	return ctg_dead_exec(ctg_op, ctg, key, next_phase);
}

static int ctg_dead_exec(struct m0_ctg_op    *ctg_op,
			 struct m0_cas_ctg   *ctg,
			 const struct m0_buf *key,
			 int                  next_phase)
{
	int ret = M0_FSO_AGAIN;

	ctg_op->co_ctg = m0_ctg_dead_index();
	ctg_op->co_ct  = CT_DEAD_INDEX;

	if (!M0_IN(ctg_op->co_opcode, (CO_MIN, CO_TRUNC, CO_DROP)) &&
	    (ctg_op->co_opcode != CO_CUR ||
	     ctg_op->co_cur_phase != CPH_NEXT))
		ctg_op->co_rc = ctg_kbuf_get(&ctg_op->co_key, key, true);

	if (ctg_op->co_rc != 0)
		m0_fom_phase_set(ctg_op->co_fom, next_phase);
	else
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
	ctg_op->co_ct  = CT_BTREE;

	if (!M0_IN(ctg_op->co_opcode, (CO_MIN, CO_TRUNC, CO_DROP)) &&
	    (ctg_op->co_opcode != CO_CUR ||
	     ctg_op->co_cur_phase != CPH_NEXT))
		ctg_op->co_rc = ctg_kbuf_get(&ctg_op->co_key, key, true);

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
	struct m0_fom *fom0;
	int            ret = M0_FSO_AGAIN;
	int            rc;
	M0_ENTRY();

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
	ctg_op->co_rc = ctg_kbuf_get(&ctg_op->co_key, key, true);

	/* Pass -1 as the next_phase to indicate we don't set it now. */
	if (ctg_op->co_rc == 0)
		ctg_op_exec(ctg_op, -1);

	/* It's fine if NOT found (e.g. for DEAD record)*/
	if (ctg_op->co_rc != 0 && ctg_op->co_rc != -ENOENT) {
		m0_fom_phase_set(fom0, next_phase);
		return ret;
	}

	/*
	 * Copy value with allocation because it refers the memory
	 * chunk that will not be avilable after the delete op.
	 */
	rc = m0_buf_copy(val, &ctg_op->co_out_val);
	M0_ASSERT(rc == 0);
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
	ctg_op->co_rc = ctg_kbuf_get(&ctg_op->co_key, key, false);

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

M0_INTERNAL void m0_ctg_op_get_ver(struct m0_ctg_op *ctg_op,
				   struct m0_crv    *out)
{
	M0_PRE(ctg_op != NULL);
	M0_PRE(out != NULL);
	M0_PRE(ergo(!m0_crv_is_none(&ctg_op->co_out_ver),
		    ctg_op->co_is_versioned));

	*out = ctg_op->co_out_ver;
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
	ctg_op->co_is_versioned = false;
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

	knob = sizeof(struct fid_key);
	vnob = sizeof(struct meta_value);
	/* Delete from meta. */
	m0_btree_del_credit(btree, 1, knob, vnob, accum);
	knob = sizeof(struct generic_key) + sizeof(ctg);
	vnob = sizeof(struct generic_value);
	/* Insert into dead index. */
	m0_btree_put_credit(mbtree, 1, knob, vnob, accum);
}

M0_INTERNAL void m0_ctg_create_credit(struct m0_be_tx_credit *accum)
{
	m0_bcount_t             knob;
	m0_bcount_t             vnob;
	struct m0_btree        *btree = ctg_store.cs_state->cs_meta->cc_tree;
	struct m0_cas_ctg      *ctg = NULL;
	struct m0_btree_type    bt    = {
		.tt_id = M0_BT_CAS_CTG,
		.ksize = -1,
		.vsize = -1,
		};

	m0_btree_create_credit(&bt, accum, 1);

	knob = sizeof(struct fid_key);
	vnob = sizeof(struct meta_value);
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
	knob = sizeof(struct generic_key) + sizeof(ctg);
	vnob = sizeof(struct generic_value);
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
	/* XXX: performance.
	 * At this moment we do not know for sure if the CAS operation should
	 * have version-aware behavior. So that we are doing a pessimistic
	 * estimate here: reserving credits for both delete and insert.
	 */
	if (ctg_is_ordinary(ctg)) {
		m0_btree_put_credit(ctg->cc_tree, 1, knob, vnob, accum);
	}
}

static void ctg_ctidx_op_credits(struct m0_cas_id       *cid,
				 bool                    insert,
				 struct m0_be_tx_credit *accum)
{
	const struct m0_dix_imask *imask;
	struct m0_btree           *btree = ctg_store.cs_ctidx->cc_tree;
	m0_bcount_t                knob;
	m0_bcount_t                vnob;

	knob = sizeof(struct fid_key);
	vnob = sizeof(struct layout_value);
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
	int                    rc;
	struct                 m0_buf btree_val;
	struct m0_dix_layout **layout = cb->c_datum;

	m0_buf_init(&btree_val, rec->r_val.ov_buf[0],
		    m0_vec_count(&rec->r_val.ov_vec));
	rc = ctg_vbuf_unpack(&btree_val, NULL) ?:
	     ctg_vbuf_as_layout(&btree_val, layout);
	return rc;
}

M0_INTERNAL int m0_ctg_ctidx_lookup_sync(const struct m0_fid  *fid,
					 struct m0_dix_layout **layout)
{
	struct fid_key       key_data  = FID_KEY_INIT(fid);
	struct m0_buf        key       = M0_BUF_INIT_PTR(&key_data);
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

	k_ptr = key.b_addr;
	ksize = key.b_nob;

	rec.r_key.k_data = M0_BUFVEC_INIT_BUF(&k_ptr, &ksize);

	rc = M0_BTREE_OP_SYNC_WITH_RC(&kv_op,
				      m0_btree_get(ctidx->cc_tree, &rec.r_key,
						   &get_cb, BOF_EQUAL, &kv_op));

	return M0_RC(rc);
}

struct ctg_ctidx_put_cb_data {
	const struct m0_cas_id *d_cid;
	struct m0_be_tx        *d_tx;
};

static int ctg_ctidx_put_cb(struct m0_btree_cb *cb, struct m0_btree_rec *rec)
{
	struct ctg_ctidx_put_cb_data *datum      = cb->c_datum;
	const struct m0_cas_id       *cid        = datum->d_cid;
	struct m0_be_tx              *tx         = datum->d_tx;
	struct fid_key                key_data   = FID_KEY_INIT(&cid->ci_fid);
	struct layout_value           value_data =
		LAYOUT_VALUE_INIT(&cid->ci_layout);
	struct m0_dix_layout         *layout;
	struct m0_ext                *im_range;
	const struct m0_dix_imask    *imask;
	m0_bcount_t                   size;

	/* Copy key in btree */
	M0_ASSERT(m0_vec_count(&rec->r_key.k_data.ov_vec) == sizeof key_data);
	m0_memmove(rec->r_key.k_data.ov_buf[0], &key_data, sizeof key_data);

	/* Copy value in btree */
	M0_ASSERT(m0_vec_count(&rec->r_val.ov_vec) == sizeof value_data);
	m0_memmove(rec->r_val.ov_buf[0], &value_data, sizeof value_data);

	layout = &((struct layout_value *)
			   (rec->r_val.ov_buf[0]))->lv_layout;
	imask = &cid->ci_layout.u.dl_desc.ld_imask;
	if (!m0_dix_imask_is_empty(imask)) {
		/*
		* Alloc memory in BE segment for imask ranges
		* and copy them.
		*/
		/** @todo Make it asynchronous. */
		M0_BE_ALLOC_ARR_SYNC(im_range, imask->im_nr,
				     cas_seg(tx->t_engine->eng_domain), tx);
		size = imask->im_nr * sizeof(struct m0_ext);
		memcpy(im_range, imask->im_range, size);
		m0_be_tx_capture(tx, &M0_BE_REG(
					cas_seg(tx->t_engine->eng_domain),
					size, im_range));
		/* Assign newly allocated imask ranges. */
		layout->u.dl_desc.ld_imask.im_range = im_range;
	}

	return 0;
}
M0_INTERNAL int m0_ctg_ctidx_insert_sync(const struct m0_cas_id *cid,
					 struct m0_be_tx        *tx)
{
	/* The key is a component catalogue FID. */
	int                          rc;
	struct fid_key               key_data   = FID_KEY_INIT(&cid->ci_fid);
	struct m0_buf                key        = M0_BUF_INIT_PTR(&key_data);
	struct layout_value          value_data =
		LAYOUT_VALUE_INIT(&cid->ci_layout);
	struct m0_buf                value      = M0_BUF_INIT_PTR(&value_data);
	struct m0_cas_ctg           *ctidx      = m0_ctg_ctidx();
	struct m0_btree_op           kv_op      = {};
	struct m0_btree_rec          rec        = {
		.r_key.k_data = M0_BUFVEC_INIT_BUF(&key.b_addr, &key.b_nob),
		.r_val        = M0_BUFVEC_INIT_BUF(&value.b_addr, &value.b_nob),
		.r_crc_type   = M0_BCT_NO_CRC,
	};
	struct ctg_ctidx_put_cb_data cb_data    = {
		.d_cid = cid,
		.d_tx  = tx,
		};
	struct m0_btree_cb            put_cb    = {
		.c_act   = ctg_ctidx_put_cb,
		.c_datum = &cb_data,
		};

	rc = M0_BTREE_OP_SYNC_WITH_RC(&kv_op,
				      m0_btree_put(ctidx->cc_tree, &rec,
						   &put_cb, &kv_op, tx));
	M0_ASSERT(rc == 0);
	m0_chan_broadcast_lock(&ctidx->cc_chan.bch_chan);
	return M0_RC(rc);
}

M0_INTERNAL int m0_ctg_ctidx_delete_sync(const struct m0_cas_id *cid,
					 struct m0_be_tx        *tx)
{
	struct m0_dix_layout *layout;
	struct m0_dix_imask  *imask;
	int                   rc;
	/* The key is a component catalogue FID. */
	struct fid_key        key_data = FID_KEY_INIT(&cid->ci_fid);
	struct m0_buf         key      = M0_BUF_INIT_PTR(&key_data);
	struct m0_cas_ctg    *ctidx    = m0_ctg_ctidx();
	struct m0_btree_op    kv_op  = {};
	struct m0_btree_key   r_key  = {
		.k_data  = M0_BUFVEC_INIT_BUF(&key.b_addr, &key.b_nob),
	};

	/* Firstly we should free buffer allocated for imask ranges array. */
	rc = m0_ctg_ctidx_lookup_sync(&cid->ci_fid, &layout);
	if (rc != 0)
		return M0_ERR(rc);

	imask = &layout->u.dl_desc.ld_imask;
	if (!m0_dix_imask_is_empty(imask)) {
		/** @todo Make it asynchronous. */
		M0_BE_FREE_PTR_SYNC(imask->im_range,
				    cas_seg(tx->t_engine->eng_domain),
				    tx);
		imask->im_range = NULL;
		imask->im_nr = 0;
	}

	/** @todo Make it asynchronous. */
	rc = M0_BTREE_OP_SYNC_WITH_RC(&kv_op, m0_btree_del(ctidx->cc_tree,
							   &r_key, NULL,
							   &kv_op, tx));

	M0_ASSERT(rc == 0);
	m0_chan_broadcast_lock(&ctidx->cc_chan.bch_chan);
	return rc == 0 ? rc : M0_ERR(rc);
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

M0_INTERNAL void
ctg_index_btree_dump_one_rec(struct m0_buf* key,
			     struct m0_buf* val, bool dump_in_hex)
{
	int i;
	int key_header_len = M0_CAS_CTG_KEY_HDR_SIZE;
	int val_header_len = M0_CAS_CTG_VAL_HDR_SIZE;
	char *kbuf, *vbuf;

	if (!dump_in_hex) {
		m0_console_printf("{key: %.*s}, {val: %.*s}\n",
				  (int)key->b_nob - key_header_len,
				  (const char*)(key->b_addr + key_header_len),
				  (int)val->b_nob - val_header_len,
				  (const char*)(val->b_addr + val_header_len));
		return;
	}

	M0_ALLOC_ARR(kbuf, (key->b_nob - key_header_len) * 2 + 1);
	for (i = 0; i < key->b_nob - key_header_len; i++)
		sprintf(kbuf + 2 * i, "%2x",
			*(uint8_t*)(key->b_addr + key_header_len + i));
	m0_console_printf("{key: %s}, ", kbuf);
	m0_free(kbuf);

	M0_ALLOC_ARR(vbuf, (val->b_nob - val_header_len) * 2 + 1);
	for (i = 0; i < val->b_nob - val_header_len; i++)
		sprintf(vbuf + 2 * i, "%2x",
			*(uint8_t*)(val->b_addr + val_header_len + i));
	m0_console_printf("{val: %s} \n", vbuf);
	m0_free(vbuf);
}

M0_INTERNAL int ctg_index_btree_dump(struct m0_motr *motr_ctx,
                                     struct m0_cas_ctg *ctg,
				     bool dump_in_hex)
{
	struct m0_buf           key;
	struct m0_buf           val;
	struct m0_btree_cursor  cursor;
	int                     rc;

	ctg_open(ctg, cas_seg(&motr_ctx->cc_reqh_ctx.rc_be.but_dom));

	m0_btree_cursor_init(&cursor, ctg->cc_tree);
	for (rc = m0_btree_cursor_first(&cursor); rc == 0;
			     rc = m0_btree_cursor_next(&cursor)) {
		m0_btree_cursor_kv_get(&cursor, &key, &val);
		ctg_index_btree_dump_one_rec(&key, &val, dump_in_hex);
	}
	m0_btree_cursor_fini(&cursor);
	ctg_fini(ctg);

	return 0;
}

int ctgdump(struct m0_motr *motr_ctx, char *fidstr, char *dump_in_hex_str)
{
	int rc;
	struct m0_fid dfid;
	struct m0_fid out_fid = { 0, 0};
	struct m0_fid gfid = { 0, 0};
	struct m0_buf key;
	struct m0_buf val;
	struct m0_btree_cursor cursor;
	struct m0_cas_ctg *ctg = NULL;
	bool dumped = false;
	bool dump_in_hex = false;
	struct fid_key *fkey;

	if (m0_streq("hex", dump_in_hex_str))
		dump_in_hex = true;

	rc = m0_fid_sscanf(fidstr, &dfid);
	if (rc < 0)
		return rc;
	m0_fid_tassume(&dfid, &m0_dix_fid_type);

	rc = m0_ctg_store_init(&motr_ctx->cc_reqh_ctx.rc_be.but_dom);
	M0_ASSERT(rc == 0);

	m0_btree_cursor_init(&cursor, ctg_store.cs_state->cs_meta->cc_tree);
	for (rc = m0_btree_cursor_first(&cursor); rc == 0;
	     rc = m0_btree_cursor_next(&cursor)) {
		m0_btree_cursor_kv_get(&cursor, &key, &val);
		fkey = key.b_addr;
		out_fid = fkey->fk_fid;
		if (!m0_dix_fid_validate_cctg(&out_fid))
			continue;
		m0_dix_fid_convert_cctg2dix(&out_fid, &gfid);
		M0_LOG(M0_DEBUG, "Found cfid="FID_F" gfid=:"FID_F" dfid="FID_F,
				  FID_P(&out_fid), FID_P(&gfid), FID_P(&dfid));
		if (m0_fid_eq(&gfid, &dfid)) {
			rc = ctg_vbuf_unpack(&val, NULL) ?:
				ctg_vbuf_as_ctg(&val, &ctg);
			if (rc != 0) {
				M0_LOG(M0_ERROR, "val unpack error:%d", rc);
				break;
			}

			M0_LOG(M0_DEBUG, "Dumping dix fid="FID_F" ctg=%p",
					  FID_P(&dfid), ctg);
			ctg_index_btree_dump(motr_ctx, ctg, dump_in_hex);
			dumped = true;
			break;
		}
	}
	m0_btree_cursor_fini(&cursor);

	return rc? : dumped? 0 : -ENOENT;
}

static bool ctg_op_is_versioned(const struct m0_ctg_op *ctg_op)
{
	const struct m0_cas_op       *cas_op;
	bool                          has_txd_ts;

	/*
	 * Since the versioned behavior is optional, it may get turned off
	 * for a variety of resons. These reasons are listed here
	 * as M0_RC_INFO messages to aid debugging of the cases
	 * where the behavior should have not been turned off.
	 */

	M0_ENTRY("ctg_op=%p", ctg_op);

	if (!ctg_is_ordinary(ctg_op->co_ctg))
		return M0_RC_INFO(false, "Non-ordinary catalogue.");

	if (ctg_op->co_fom == NULL)
		return M0_RC_INFO(false, "No fom, no versions.");

	if (ctg_op->co_fom->fo_fop == NULL)
		return M0_RC_INFO(false, "No fop, no versions.");

	cas_op = m0_fop_data(ctg_op->co_fom->fo_fop);
	if (cas_op == NULL)
		return M0_RC_INFO(false, "No cas op, no versions.");

	if ((ctg_op->co_flags & COF_VERSIONED) == 0)
		return M0_RC_INFO(false, "CAS request is not versioned.");

	has_txd_ts = !m0_dtm0_ts_is_none(&cas_op->cg_txd.dtd_id.dti_ts);

	switch (CTG_OP_COMBINE(ctg_op->co_opcode, ctg_op->co_ct)) {
	case CTG_OP_COMBINE(CO_PUT, CT_BTREE):
	case CTG_OP_COMBINE(CO_DEL, CT_BTREE):
		M0_ASSERT(has_txd_ts);
	case CTG_OP_COMBINE(CO_GET, CT_BTREE):
	case CTG_OP_COMBINE(CO_CUR, CT_BTREE):
		return M0_RC(true);
	default:
		break;
	}

	return M0_RC_INFO(false, "Non-versioned operation.");
}

static void ctg_op_version_get(const struct m0_ctg_op *ctg_op,
			       struct m0_crv          *out)
{
	const struct m0_dtm0_tid     *tid;
	const struct m0_cas_op       *cas_op;
	bool                          tbs;

	M0_ENTRY();
	M0_PRE(ctg_op->co_is_versioned);

	if (M0_IN(ctg_op->co_opcode, (CO_PUT, CO_DEL))) {
		M0_ASSERT_INFO(ctg_op->co_fom != NULL,
			       "Versioned op without FOM?");
		cas_op = m0_fop_data(ctg_op->co_fom->fo_fop);
		M0_ASSERT_INFO(cas_op != NULL, "Versioned op without cas_op?");
		tbs = ctg_op->co_opcode == CO_DEL;
		tid = &cas_op->cg_txd.dtd_id;
		m0_crv_init(out, &tid->dti_ts, tbs);

		M0_LEAVE("ctg_op=%p, cas_op=%p, txid=" DTID0_F ", ver=%" PRIu64
			 ", ver_enc=%" PRIu64, ctg_op,
			 cas_op, DTID0_P(tid), tid->dti_ts.dts_phys,
			 out->crv_encoded);
	} else {
		*out = M0_CRV_INIT_NONE;
		M0_LEAVE("ctg_op=%p, no version", ctg_op);
	}
}

static int versioned_put_get_cb(struct m0_btree_cb *cb,
			        struct m0_btree_rec *rec)
{
	struct m0_crv *old_version = cb->c_datum;
	struct m0_buf  btree_val;
	int            rc;

	m0_buf_init(&btree_val, rec->r_val.ov_buf[0],
		    m0_vec_count(&rec->r_val.ov_vec));
	rc = ctg_vbuf_unpack(&btree_val, old_version);
	return rc;
}

struct ver_update_datum {
	struct m0_crv    *d_new_version;
	struct m0_ctg_op *d_ctg_op;

};

static int versioned_put_update_cb(struct m0_btree_cb *cb,
				   struct m0_btree_rec *rec)
{
	struct ver_update_datum *datum       = cb->c_datum;
	struct m0_ctg_op        *ctg_op      = datum->d_ctg_op;
	struct m0_crv           *new_version = datum->d_new_version;
	struct m0_be_tx         *tx          = &ctg_op->co_fom->fo_tx.tx_betx;
	struct m0_buf            btree_val;
	m0_bcount_t              ksize;

	ksize = m0_vec_count(&rec->r_key.k_data.ov_vec);
	M0_ASSERT(ctg_op->co_key.b_nob == ksize);
	m0_memmove(rec->r_key.k_data.ov_buf[0], ctg_op->co_key.b_addr, ksize);

	m0_buf_init(&btree_val, rec->r_val.ov_buf[0],
		    m0_vec_count(&rec->r_val.ov_vec));
	ctg_vbuf_pack(&btree_val, &ctg_op->co_val, new_version);

	m0_ctg_state_inc_update(tx,
				ksize - sizeof(struct generic_key) +
				ctg_op->co_val.b_nob);
	return 0;
}

/*
 * Synchronously inserts/updates a key-value record considering the version
 * and the tombstone that were specified in the operation ("new_version")
 * and the version+tombstone that were stored in btree ("old_version").
 */
static int versioned_put_sync(struct m0_ctg_op *ctg_op)
{
	struct m0_btree_op       kv_op        = {};
	struct m0_btree         *btree        = ctg_op->co_ctg->cc_tree;
	struct m0_buf           *key          = &ctg_op->co_key;
	struct m0_be_tx         *tx           = &ctg_op->co_fom->fo_tx.tx_betx;
	struct m0_crv            new_version  = M0_CRV_INIT_NONE;
	struct m0_crv            old_version  = M0_CRV_INIT_NONE;
	struct m0_btree_rec      rec          = {
		.r_key.k_data = M0_BUFVEC_INIT_BUF(&key->b_addr, &key->b_nob),
	};
	struct ver_update_datum  update_datum = {
		.d_new_version = &new_version,
		.d_ctg_op      = ctg_op,
	};
	struct m0_btree_cb       ver_put_cb   = {
		.c_act   = versioned_put_get_cb,
		.c_datum = &old_version,
		};
	void                    *v_ptr;
	m0_bcount_t              vsize;
	int                      rc;

	M0_PRE(ctg_op->co_is_versioned);
	M0_ENTRY();

	ctg_op_version_get(ctg_op, &new_version);
	M0_ASSERT_INFO(!m0_crv_is_none(&new_version),
		       "Versioned PUT or DEL without a valid version?");

	/*
	 * TODO: Performance.
	 * This lookup call can be avoided if the version comparison
	 * was shifted into btree. However, at this moment the cost of
	 * such a lookup is much lower than the cost of re-working btree API.
	 * See the be_btree_search() call inside ::btree_save. It is the
	 * point where btree_save does its own "lookup". Versions could
	 * be compared right in there. Alternatively, btree_save could
	 * use btree_cursor as a "hint".
	 */
	rc = M0_BTREE_OP_SYNC_WITH_RC(&kv_op,
				      m0_btree_get(btree, &rec.r_key,
						   &ver_put_cb, BOF_EQUAL,
						   &kv_op));
	if (!M0_IN(rc, (0, -ENOENT)))
		return M0_ERR(rc);

	/*
	 * Since we're actually doing a put of a tombstone, the
	 * result of a delete gets weird. We therefore fake
	 * it here, so that a delete with no existing value
	 * returns an error.
	 *
	 * We may want to reuse the COF_OVERWRITE flag (or add
	 * a new one) as part of the tombstone cleanup effort
	 * so it's possible to insert a tombstone without an
	 * existing value.
	 */
	if (ctg_op->co_opcode == CO_DEL &&
	    (rc == -ENOENT || m0_crv_tbs(&old_version))) {
		return M0_ERR(-ENOENT);
	}

	/*
	 * Skip overwrite op if the existing record is "newer" (version-wise)
	 * than the record to be inserted. Note, <= 0 means that we filter out
	 * the operations with the exact same version and tombstone.
	 */
	if (!m0_crv_is_none(&old_version) &&
	    m0_crv_cmp(&new_version, &old_version) <= 0)
		return M0_RC(0);

	M0_LOG(M0_DEBUG, "Overwriting " CRV_F " with " CRV_F ".",
	       CRV_P(&old_version), CRV_P(&new_version));

	vsize = ctg_vbuf_packed_size(&ctg_op->co_val);
	rec.r_val = M0_BUFVEC_INIT_BUF(&v_ptr, &vsize),
	ver_put_cb.c_act = versioned_put_update_cb;
	ver_put_cb.c_datum = &update_datum;

	/*
	 * Several cases allow overwriting existing values:
	 * 1. COF_OVERWRITE (only allowed on CO_PUT).
	 * 2. CO_DEL. From the check above we know there's
	 *    a value to be deleted, so do an overwrite.
	 * 3. The old version is a tombstone.
	 *    Logically there's no value there, so COF_OVERWRITE
	 *    shouldn't be required on put. We know we're putting
	 *    a newer version, so just replace the old tombstone.
	 */
	if (ctg_op->co_flags & COF_OVERWRITE ||
	    ctg_op->co_opcode == CO_DEL ||
	    m0_crv_tbs(&old_version)) {
		rc = M0_BTREE_OP_SYNC_WITH_RC(
			&kv_op,
			m0_btree_update(btree, &rec, &ver_put_cb,
					BOF_INSERT_IF_NOT_FOUND,
					&kv_op, tx));
	} else {
		rc = M0_BTREE_OP_SYNC_WITH_RC(
			&kv_op,
			m0_btree_put(btree, &rec, &ver_put_cb,
				     &kv_op, tx));
	}
	return M0_RC(rc);
}

static int versioned_get_cb(struct m0_btree_cb  *cb, struct m0_btree_rec *rec)
{
	int rc;
	struct m0_ctg_op *ctg_op = cb->c_datum;
	struct m0_buf        btree_val;

	m0_buf_init(&btree_val, rec->r_val.ov_buf[0],
		    m0_vec_count(&rec->r_val.ov_vec));
	rc = ctg_vbuf_unpack(&btree_val, &ctg_op->co_out_ver);

	if (rc == 0) {
		if (m0_crv_tbs(&ctg_op->co_out_ver))
			rc = M0_ERR(-ENOENT);
		else
			ctg_op->co_out_val = btree_val;
	}
	return rc;
}
/*
 * Gets an alive record (without tombstone set) in btree.
 * Returns -ENOENT if tombstone is set.
 * Always sets co_out_ver if the record physically exists in the tree.
 */
static int versioned_get_sync(struct m0_ctg_op *ctg_op)
{
	int                  rc;
	struct m0_btree_op   kv_op  = {};
	struct m0_btree      *btree = ctg_op->co_ctg->cc_tree;
	struct m0_buf        *key   = &ctg_op->co_key;
	struct m0_btree_key   r_key = {
		.k_data = M0_BUFVEC_INIT_BUF(&key->b_addr, &key->b_nob),
		};
	struct m0_btree_cb   ver_get_cb = {
		.c_act = versioned_get_cb,
		.c_datum = ctg_op,
		};
	M0_ENTRY();

	M0_PRE(ctg_op->co_is_versioned);

	rc = M0_BTREE_OP_SYNC_WITH_RC(&kv_op,
				      m0_btree_get(btree, &r_key, &ver_get_cb,
						   BOF_EQUAL, &kv_op));
	return M0_RC(rc);
}

/*
 * Synchronously advances the cursor until it reaches an alive
 * key-value pair (alive_only=true), next key-value pair (alive_only=false),
 * or the end of the tree.
 */
static int versioned_cursor_next_sync(struct m0_ctg_op *ctg_op, bool alive_only)
{
	struct m0_be_op           *beop   = ctg_beop(ctg_op);
	int                        rc;

	do {
		rc = m0_btree_cursor_next(&ctg_op->co_cur);

		if (rc != 0)
			break;

		m0_btree_cursor_kv_get(&ctg_op->co_cur,
					  &ctg_op->co_out_key,
					  &ctg_op->co_out_val);
		rc = ctg_kbuf_unpack(&ctg_op->co_out_key) ?:
			ctg_vbuf_unpack(&ctg_op->co_out_val,
					&ctg_op->co_out_ver);
		if (rc != 0)
			break;

		m0_be_op_reset(beop);

	} while (m0_crv_tbs(&ctg_op->co_out_ver) && alive_only);

	/* It should never return dead values. */
	if (m0_crv_tbs(&ctg_op->co_out_ver))
		ctg_op->co_out_val = M0_BUF_INIT0;

	return M0_RC(rc);
}

/*
 * Positions the cursor at the alive record that corresponds to the
 * specified key or at the alive record next to the specified key
 * depending on the slant flag. If alive_only=false then it treats
 * all records as alive.
 *
 * The relations between tombstones and slant flag:
 *   Non-slant:
 *     record_at(key) is alive: returns the record.
 *     record_at(key) has tombstone: returns -ENOENT.
 *   Slant:
 *     record_at(key) is alive: returns the record.
 *     record_at(key) has tombstone: finds next alive record.
 */
static int versioned_cursor_get_sync(struct m0_ctg_op *ctg_op, bool alive_only)
{
	struct m0_be_op           *beop   = ctg_beop(ctg_op);
	struct m0_buf              value  = M0_BUF_INIT0;
	struct m0_buf             *key    = &ctg_op->co_key;
	void                      *k_ptr  = key->b_addr;
	m0_bcount_t                ksize  = key->b_nob;
	struct m0_btree_key        r_key  = {
		.k_data = M0_BUFVEC_INIT_BUF(&k_ptr, &ksize),
	};
	bool                       slant  = (ctg_op->co_flags & COF_SLANT) != 0;
	int                        rc;

	M0_PRE(ctg_op->co_is_versioned);

	rc = m0_btree_cursor_get(&ctg_op->co_cur, &r_key, slant);

	if (rc == 0) {
		m0_btree_cursor_kv_get(&ctg_op->co_cur,
					  &ctg_op->co_out_key,
					  &value);
		rc = ctg_kbuf_unpack(&ctg_op->co_out_key) ?:
			ctg_vbuf_unpack(&value, &ctg_op->co_out_ver);
		if (rc == 0) {
			if (m0_crv_tbs(&ctg_op->co_out_ver))
				rc = alive_only ?
					(slant ? -EAGAIN : -ENOENT) : 0;
			else
				ctg_op->co_out_val = value;
		}
	} else
		M0_ASSERT_INFO(rc != EAGAIN,
			       "btree cursor op returned EAGAIN?");

	m0_be_op_reset(beop);

	if (rc == -EAGAIN)
		rc = versioned_cursor_next_sync(ctg_op, alive_only);

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
/*
 * vim: tabstop=8 shiftwidth=8 noexpandtab textwidth=80 nowrap
 */
