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

/**
 * @addtogroup dtm0
 *
 * @{
 */

#define M0_TRACE_SUBSYSTEM M0_TRACE_SUBSYS_DTM0
#include "lib/trace.h"

#include "dtm0/log.h"

#include "lib/buf.h"            /* m0_buf */
#include "lib/vec.h"            /* m0_vec_count */
#include "lib/errno.h"          /* EINVAL */
#include "lib/assert.h"         /* M0_ASSERT */
#include "lib/string.h"         /* strlen */
#include "lib/locality.h"       /* m0_locality0_get */

#include "be/domain.h"          /* m0_be_domain_seg0_get */
#include "be/btree.h"           /* m0_btree */
#include "be/list.h"            /* m0_be_list */
#include "be/seg0.h"            /* m0_be_0type */
#include "be/seg.h"             /* m0_be_seg_allocator */
#include "be/tx.h"              /* m0_be_tx */
#include "be/op.h"              /* M0_BE_OP_SYNC */

#include "dtm0/dtm0.h"          /* m0_dtm0_redo */

struct m0_sm_group;
struct m0_be_domain;

enum {
	M0_DTM0_LOG_ROOT_NODE_SIZE = (8 * 1024),
	M0_DTM0_LOG_SHIFT = 12,
};

struct dtm0_log_data {
	uint8_t             dtld_node[M0_DTM0_LOG_ROOT_NODE_SIZE];
	struct m0_btree     dtld_transactions;
	struct m0_be_list   dtld_all_p;
} M0_XCA_RECORD M0_XCA_DOMAIN(be);

struct dtm0_log_record {
	struct m0_dtx0_descriptor lr_descriptor;
	uint32_t                  lr_payload_type
		M0_XCA_FENUM(m0_dtx0_payload_type);
	struct m0_buf             lr_payload_data;
	struct m0_be_list_link    lr_link_all_p;
	uint64_t                  lr_magic;
} M0_XCA_RECORD M0_XCA_DOMAIN(be);

static int dtm0_log0_init(struct m0_be_domain *dom, const char *suffix,
                          const struct m0_buf *data);
static void dtm0_log0_fini(struct m0_be_domain *dom, const char *suffix,
                           const struct m0_buf *data);

static bool dtm0_log_invariant(const struct m0_dtm0_log *dol)
{
	return _0C(m0_mutex_is_locked(&dol->dtl_lock)) &&
		_0C(ergo(dol->dtl_the_end, dol->dtl_allp_op == NULL)) &&
		ergo(dol->dtl_allp_op != NULL,
		    _0C(dol->dtl_allp != NULL) &&
		    _0C(dol->dtl_successful != NULL));
}

struct m0_be_0type m0_dtm0_log0 = {
	.b0_name = "M0_DTM0:LOG",
	.b0_init = &dtm0_log0_init,
	.b0_fini = &dtm0_log0_fini,
};

static m0_bcount_t dtm0_log_transactions_ksize(const void *key);
static m0_bcount_t dtm0_log_transactions_vsize(const void *key);
static int dtm0_log_transactions_compare(const void *key0, const void *key1);

M0_BE_LIST_DESCR_DEFINE(dtm0_log_all_p, "dtm0_log_data::dtld_all_p",
			static, struct dtm0_log_record,
			lr_link_all_p, lr_magic, 1 /* XXX */, 2 /* XXX */);
M0_BE_LIST_DEFINE(dtm0_log_all_p, static, struct dtm0_log_record);


M0_INTERNAL int m0_dtm0_log_open(struct m0_dtm0_log     *dol,
				 struct m0_dtm0_log_cfg *dol_cfg)
{
	struct dtm0_log_data *log_data;
	struct m0_be_domain  *dom = dol_cfg->dlc_be_domain;
	struct m0_be_seg     *seg0 = m0_be_domain_seg0_get(dom);
	struct m0_be_seg     *seg = dol_cfg->dlc_seg != NULL ? dol_cfg->dlc_seg :
					m0_be_domain_seg_first(dom);
	struct m0_buf        *log_data_buf = NULL;
	size_t                size_prefix;
	size_t                size_suffix;
	char                  name[0x100];
	int                   rc;
	struct m0_btree_op    b_op = {};
	struct m0_btree_rec_key_op keycmp = {
		keycmp.rko_keycmp = dtm0_log_transactions_compare };

	M0_PRE(dom != NULL);
	M0_PRE(dol->dtl_data == NULL);

	dol->dtl_cfg = *dol_cfg;
	dol->dtl_cfg.dlc_seg = seg;
	size_prefix = strlen(m0_dtm0_log0.b0_name);
	/* XXX check that there is at least one \0 in the array */
	size_suffix = strlen((const char *)&dol_cfg->dlc_seg0_suffix);
	if (size_prefix + size_suffix + 1 > ARRAY_SIZE(name))
		return M0_ERR(-EINVAL);
	memcpy(&name, m0_dtm0_log0.b0_name, size_prefix);
	memcpy(&name[size_prefix], &dol_cfg->dlc_seg0_suffix, size_suffix);
	name[size_prefix + size_suffix] = '\0';
	rc = m0_be_seg_dict_lookup(seg0, (const char *)&name,
				   (void **)&log_data_buf);
	if (rc != 0)
		return M0_ERR(rc);
	log_data = *(struct dtm0_log_data **)log_data_buf->b_addr;
	rc = M0_BTREE_OP_SYNC_WITH_RC(&b_op,
			m0_btree_open(&log_data->dtld_node,
					sizeof log_data->dtld_node,
					&log_data->dtld_transactions,
					seg, &b_op,
					&keycmp));
	M0_ASSERT(rc == 0);

	dol->dtl_data = log_data;
	m0_mutex_init(&dol->dtl_lock);
	dol->dtl_allp_op = NULL;
	dol->dtl_allp = NULL;
	dol->dtl_the_end = false;
	return M0_RC(0);
}

M0_INTERNAL void m0_dtm0_log_close(struct m0_dtm0_log *dol)
{
	struct m0_btree_op      b_op = {};

	M0_PRE(dol->dtl_data != NULL);
	M0_PRE(dol->dtl_allp_op == NULL);
	m0_mutex_fini(&dol->dtl_lock);
	M0_BTREE_OP_SYNC_WITH_RC(&b_op,
				 m0_btree_close(&dol->dtl_data->dtld_transactions,
						&b_op));
	dol->dtl_data = NULL;
}

M0_INTERNAL int m0_dtm0_log_create(struct m0_dtm0_log     *dol,
                                   struct m0_dtm0_log_cfg *dol_cfg)
{
	struct m0_be_tx_credit  cred = {};
	struct dtm0_log_data   *log_data;
	struct m0_be_domain    *dom = dol_cfg->dlc_be_domain;
	struct m0_sm_group     *grp = m0_locality0_get()->lo_grp;
	struct m0_be_seg       *seg = dol_cfg->dlc_seg != NULL ? dol_cfg->dlc_seg :
					m0_be_domain_seg_first(dom);
	struct m0_be_tx         tx = {};
	int                     rc;
	struct m0_btree_type    bt;
	struct m0_btree_op      b_op = {};
	struct m0_btree_rec_key_op keycmp = {
		keycmp.rko_keycmp = dtm0_log_transactions_compare };

	M0_ENTRY();


	rc = m0_dtm0_log_open(dol, dol_cfg);
	if (rc == 0)
		m0_dtm0_log_close(dol);

	if (!M0_IN(rc, (0, -ENOENT)))
		return M0_ERR(rc);

	if (rc == 0) {
		M0_LOG(M0_DEBUG, "DTM0 log already exists.");
		return M0_RC(0);
	}

	/* TODO check that there is \0 in dol_cfg->dlc_seg0_suffix */
	m0_sm_group_lock(grp);
	m0_be_tx_init(&tx, 0, dom, grp, NULL, NULL, NULL, NULL);
	m0_be_allocator_credit(m0_be_seg_allocator(seg), M0_BAO_ALLOC,
	                       sizeof(*log_data), 0, &cred);
	m0_be_0type_add_credit(dom, &m0_dtm0_log0,
			       (const char *)&dol_cfg->dlc_seg0_suffix,
	                       &M0_BUF_INIT(sizeof log_data, &log_data), &cred);

	dtm0_log_all_p_be_list_credit(M0_BLO_CREATE, 1, &cred);

	bt = (struct m0_btree_type) {
		.tt_id = M0_BT_DTM0_LOG,
		.ksize = sizeof(struct m0_dtx0_id),
		.vsize = -1,
	};
	m0_btree_create_credit(&bt, &cred, 1);

	m0_be_tx_prep(&tx, &cred);
	rc = m0_be_tx_exclusive_open_sync(&tx);
	M0_ASSERT(rc == 0);     /* XXX */
	M0_BE_ALLOC_ALIGN_PTR_SYNC(log_data, M0_DTM0_LOG_SHIFT,
				   seg, &tx);
	M0_ASSERT(log_data != NULL);    /* XXX */
	rc = m0_be_0type_add(&m0_dtm0_log0, dom, &tx,
			     (const char *)&dol_cfg->dlc_seg0_suffix,
			     &M0_BUF_INIT(sizeof log_data, &log_data));

	M0_ASSERT(rc == 0);     /* XXX */
	rc = M0_BTREE_OP_SYNC_WITH_RC(&b_op,
			m0_btree_create(&log_data->dtld_node,
					sizeof log_data->dtld_node,
					&bt, M0_BCT_NO_CRC, &b_op,
					&log_data->dtld_transactions, seg,
					&dol_cfg->dlc_btree_fid, &tx, &keycmp));
	M0_ASSERT(rc == 0);


	dtm0_log_all_p_be_list_create(&log_data->dtld_all_p, &tx);

	/* TODO capture log_data? */

	m0_be_tx_close_sync(&tx);
	m0_be_tx_fini(&tx);
	m0_sm_group_unlock(grp);
	return M0_RC(0);
}

M0_INTERNAL void m0_dtm0_log_destroy(struct m0_dtm0_log *dol)
{
	/* XXX No-op for now. TODO implement */
}

static int dtm0_log0_init(struct m0_be_domain *dom, const char *suffix,
                          const struct m0_buf *data)
{
	return 0;
}

static void dtm0_log0_fini(struct m0_be_domain *dom, const char *suffix,
                           const struct m0_buf *data)
{
}

static inline m0_bcount_t dtm0_log_transactions_ksize(const void *key)
{
	return sizeof ((struct dtm0_log_record *)NULL)->lr_descriptor.dtd_id;
}

static inline m0_bcount_t dtm0_log_transactions_vsize(const void *value)
{
	return sizeof(struct dtm0_log_record *);
}

static int dtm0_log_transactions_compare(const void *key0, const void *key1)
{
	return m0_dtx0_id_cmp(key0, key1);
}

static int redo_insert_callback(struct m0_btree_cb  *cb,
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

static int redo_insert(struct m0_btree *tree, struct m0_be_tx *tx,
			     struct m0_buf *key, struct m0_buf *val)
{
	struct m0_btree_op   kv_op        = {};
	void                *k_ptr = key->b_addr;
	void                *v_ptr = val->b_addr;
	m0_bcount_t          ksize = key->b_nob;
	m0_bcount_t          vsize = val->b_nob;
	struct m0_btree_rec  rec = {
			   .r_key.k_data = M0_BUFVEC_INIT_BUF(&k_ptr, &ksize),
			   .r_val        = M0_BUFVEC_INIT_BUF(&v_ptr, &vsize),
			   .r_crc_type   = M0_BCT_NO_CRC,
			};
	struct m0_btree_cb   redo_insert_cb = {.c_act = redo_insert_callback,
					  .c_datum = &rec,
					 };
	return M0_BTREE_OP_SYNC_WITH_RC(&kv_op,
					m0_btree_put(tree, &rec, &redo_insert_cb,
						     &kv_op, tx));
}

M0_INTERNAL int m0_dtm0_log_redo_add(struct m0_dtm0_log        *dol,
                                     struct m0_be_tx           *tx,
                                     const struct m0_dtm0_redo *redo,
                                     const struct m0_fid       *p_sdev_fid)
{
	struct dtm0_log_record  *rec;
	struct m0_be_seg        *seg = dol->dtl_cfg.dlc_seg;

	M0_LOG(M0_DEBUG, "fid:"FID_F, FID_P(p_sdev_fid));
	m0_mutex_lock(&dol->dtl_lock);
	M0_PRE(!dol->dtl_the_end);
	M0_PRE(dtm0_log_invariant(dol));
	m0_mutex_unlock(&dol->dtl_lock);

	/* TODO lookup before insert */
	/* TODO check memory allocation errors */
	M0_BE_ALLOC_PTR_SYNC(rec, seg, tx);
	M0_ASSERT(redo->dtr_payload.dtp_data.ab_count == 1);
	rec->lr_payload_data.b_nob =
		redo->dtr_payload.dtp_data.ab_elems[0].b_nob;
	M0_BE_ALLOC_BUF_SYNC(&rec->lr_payload_data, seg, tx);
	/* TODO check if memcpy can be avoided */
	m0_buf_memcpy(&rec->lr_payload_data,
		      &redo->dtr_payload.dtp_data.ab_elems[0]);
	rec->lr_descriptor = redo->dtr_descriptor;
	M0_LOG(M0_DEBUG, "redo add txid: " DTID1_F,
		       DTID1_P(&rec->lr_descriptor.dtd_id));
	redo_insert(&dol->dtl_data->dtld_transactions, tx,
		    &M0_BUF_INIT_PTR(&rec->lr_descriptor.dtd_id),
	            &M0_BUF_INIT(sizeof rec, &rec));
	dtm0_log_all_p_be_tlink_create(rec, tx);

	m0_mutex_lock(&dol->dtl_lock);
	M0_ASSERT(!dol->dtl_the_end);
	if (dtm0_log_all_p_be_list_head(&dol->dtl_data->dtld_all_p) == NULL &&
	    dol->dtl_allp_op != NULL) {
		M0_ASSERT(dol->dtl_allp != NULL);
		*dol->dtl_allp = rec->lr_descriptor.dtd_id;
		*dol->dtl_successful = true;
		m0_be_op_done(dol->dtl_allp_op);
		dol->dtl_allp_op = NULL;
	}
	dtm0_log_all_p_be_list_add_tail(&dol->dtl_data->dtld_all_p, tx, rec);
	m0_mutex_unlock(&dol->dtl_lock);
	/* TODO capture rec->lr_payload_data */
	/* TODO capture rec */
	return 0;
}

M0_INTERNAL void m0_dtm0_log_redo_add_credit(struct m0_dtm0_log        *dol,
                                             const struct m0_dtm0_redo *redo,
                                             struct m0_be_tx_credit    *accum)
{
	struct dtm0_log_record *rec = NULL; /* XXX */
	struct m0_be_seg       *seg = dol->dtl_cfg.dlc_seg;
	struct m0_be_allocator *a   = m0_be_seg_allocator(seg);

	M0_PRE(redo->dtr_payload.dtp_data.ab_count == 1);

	m0_be_allocator_credit(a, M0_BAO_ALLOC, sizeof *rec, 0, accum);
	m0_be_allocator_credit(a, M0_BAO_ALLOC,
			       redo->dtr_payload.dtp_data.ab_elems[0].b_nob,
	                       0, accum);
	m0_btree_put_credit(&dol->dtl_data->dtld_transactions, 1,
	                          sizeof rec->lr_descriptor.dtd_id,
	                          sizeof(void *), accum);
	dtm0_log_all_p_be_list_credit(M0_BLO_TLINK_CREATE, 1, accum);
	dtm0_log_all_p_be_list_credit(M0_BLO_ADD, 1, accum);
}

static int redo_log_lookup_callback(struct m0_btree_cb *cb,
				     struct m0_btree_rec *rec)
{
	struct m0_btree_rec     *datum = cb->c_datum;

	/** Only copy the Value for the caller. */
	m0_bufvec_copy(&datum->r_val, &rec->r_val,
		       m0_vec_count(&rec->r_val.ov_vec));
	return 0;
}

static int redo_log_lookup(struct m0_btree *tree, struct m0_buf *key,
			    struct m0_buf *out)
{
	struct m0_btree_op   kv_op     = {};
	void                *k_ptr     = key->b_addr;
	void                *v_ptr     = out->b_addr;
	m0_bcount_t          ksize     = key->b_nob;
	m0_bcount_t          vsize     = out->b_nob;
	struct m0_btree_rec  rec       = {
			    .r_key.k_data = M0_BUFVEC_INIT_BUF(&k_ptr, &ksize),
			    .r_val        = M0_BUFVEC_INIT_BUF(&v_ptr, &vsize),
			};
	struct m0_btree_cb   lookup_cb = {.c_act = redo_log_lookup_callback,
					  .c_datum = &rec,
					 };

	return M0_BTREE_OP_SYNC_WITH_RC(&kv_op,
					m0_btree_get(tree, &rec.r_key,
						     &lookup_cb,
						     BOF_EQUAL, &kv_op));
}

static void redo_log_delete(struct m0_btree *tree, struct m0_be_tx *tx,
			    struct m0_buf *key)
{
        struct m0_btree_op   kv_op        = {};
        void                *k_ptr = key->b_addr;
        m0_bcount_t          ksize = key->b_nob;
        struct m0_btree_key  r_key = {
                                  .k_data  = M0_BUFVEC_INIT_BUF(&k_ptr, &ksize),
                                };

        M0_BTREE_OP_SYNC_WITH_RC(&kv_op,
                                 m0_btree_del(tree, &r_key, NULL,
                                             &kv_op, tx));
}

M0_INTERNAL void m0_dtm0_log_prune(struct m0_dtm0_log *dol,
                                   struct m0_be_tx    *tx,
                                   struct m0_dtx0_id  *dtx0_id)
{
	struct dtm0_log_record *rec = NULL;
	struct m0_buf           rec_buf = M0_BUF_INIT(sizeof rec, &rec);
	int                     rc;

	M0_LOG(M0_DEBUG, "DTX id: " DTID1_F, DTID1_P(dtx0_id));
	/* TODO handle lookup errors */
	rc = redo_log_lookup(&dol->dtl_data->dtld_transactions,
			     &M0_BUF_INIT_PTR(dtx0_id), &rec_buf);
	M0_ASSERT(ergo(rc != 0, rec == NULL));
	M0_ASSERT(M0_IN(rc, (0, -ENOENT)));
	m0_mutex_lock(&dol->dtl_lock);
	M0_ASSERT(dtm0_log_invariant(dol));
	dtm0_log_all_p_be_list_del(&dol->dtl_data->dtld_all_p, tx, rec);
	m0_mutex_unlock(&dol->dtl_lock);
	dtm0_log_all_p_be_tlink_destroy(rec, tx);
	/* TODO free rec */
	/* TODO check delete result, i.e. t_rc */
	redo_log_delete(&dol->dtl_data->dtld_transactions, tx,
			&M0_BUF_INIT_PTR(dtx0_id));
}

M0_INTERNAL void m0_dtm0_log_prune_credit(struct m0_dtm0_log     *dol,
                                          struct m0_be_tx_credit *accum)
{
	struct dtm0_log_record *rec = NULL; /* XXX */

	m0_btree_del_credit(&dol->dtl_data->dtld_transactions, 1,
	                          sizeof rec->lr_descriptor.dtd_id,
	                          sizeof(void *), accum);
	dtm0_log_all_p_be_list_credit(M0_BLO_DEL, 1, accum);
	dtm0_log_all_p_be_list_credit(M0_BLO_TLINK_DESTROY, 1, accum);
}


M0_INTERNAL void m0_dtm0_log_p_get_none_left(struct m0_dtm0_log *dol,
					     struct m0_be_op    *op,
					     struct m0_dtx0_id  *dtx0_id,
					     bool               *successful)
{
	struct dtm0_log_record *rec;

	m0_be_op_active(op);
	m0_mutex_lock(&dol->dtl_lock);
	M0_PRE(dtm0_log_invariant(dol));
	M0_PRE(dol->dtl_allp_op == NULL);

	if (dol->dtl_the_end) {
		*successful = false;
		m0_be_op_done(op);
		m0_mutex_unlock(&dol->dtl_lock);
		return;
	}

	rec = dtm0_log_all_p_be_list_head(&dol->dtl_data->dtld_all_p);
	if (rec != NULL) {
		*dtx0_id = rec->lr_descriptor.dtd_id;
		*successful = true;
		m0_be_op_done(op);
	} else {
		dol->dtl_allp_op = op;
		dol->dtl_allp = dtx0_id;
		dol->dtl_successful = successful;
	}
	m0_mutex_unlock(&dol->dtl_lock);
}

M0_INTERNAL void m0_dtm0_log_end(struct m0_dtm0_log *dol)
{
	m0_mutex_lock(&dol->dtl_lock);
	M0_PRE(dtm0_log_invariant(dol));
	M0_PRE(!dol->dtl_the_end);
	dol->dtl_the_end = true;
	if (dol->dtl_allp_op != NULL) {
		*dol->dtl_successful = false;
		m0_be_op_done(dol->dtl_allp_op);
		dol->dtl_allp_op = NULL;
	}
	m0_mutex_unlock(&dol->dtl_lock);
}

M0_INTERNAL bool m0_dtm0_log_is_empty(struct m0_dtm0_log *dol)
{
	bool result;

	M0_PRE(dol != NULL);
	m0_mutex_lock(&dol->dtl_lock);
	M0_PRE(dol->dtl_data != NULL);
	result = m0_btree_is_empty(&dol->dtl_data->dtld_transactions) &&
		dtm0_log_all_p_be_list_head(&dol->dtl_data->dtld_all_p) == NULL;
	m0_mutex_unlock(&dol->dtl_lock);
	return M0_RC(!!result);
}

#undef M0_TRACE_SUBSYSTEM

/** @} end of dtm0 group */

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
