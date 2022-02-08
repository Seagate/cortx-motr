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
#include "be/btree.h"           /* m0_be_btree */
#include "be/list.h"            /* m0_be_list */
#include "be/seg0.h"            /* m0_be_0type */
#include "be/seg.h"             /* m0_be_seg_allocator */
#include "be/tx.h"              /* m0_be_tx */
#include "be/op.h"              /* M0_BE_OP_SYNC */

#include "dtm0/dtm0.h"          /* m0_dtm0_redo */


struct m0_sm_group;
struct m0_be_domain;


struct dtm0_log_data {
	struct m0_be_btree dtld_transactions;
	struct m0_be_list  dtld_all_p;
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

struct m0_be_0type m0_dtm0_log0 = {
	.b0_name = "M0_DTM0:LOG",
	.b0_init = &dtm0_log0_init,
	.b0_fini = &dtm0_log0_fini,
};

static m0_bcount_t dtm0_log_transactions_ksize(const void *key);
static m0_bcount_t dtm0_log_transactions_vsize(const void *key);
static int dtm0_log_transactions_compare(const void *key0, const void *key1);

static struct m0_be_btree_kv_ops dtm0_log_transactions_kv_ops = {
	.ko_type    = M0_BBT_DTM0_LOG,
	.ko_ksize   = &dtm0_log_transactions_ksize,
	.ko_vsize   = &dtm0_log_transactions_vsize,
	.ko_compare = &dtm0_log_transactions_compare,
};

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
	struct m0_buf        *log_data_buf;
	size_t                size_prefix;
	size_t                size_suffix;
	char                  name[0x100];
	int                   rc;

	dol->dtl_cfg = *dol_cfg;
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
	m0_be_btree_init(&log_data->dtld_transactions,
			 m0_be_domain_seg_first(dom),
	                 &dtm0_log_transactions_kv_ops);
	dol->dtl_data = log_data;
	return M0_RC(0);
}

M0_INTERNAL void m0_dtm0_log_close(struct m0_dtm0_log *dol)
{
	m0_be_btree_fini(&dol->dtl_data->dtld_transactions);
}

M0_INTERNAL int m0_dtm0_log_create(struct m0_dtm0_log     *dol,
                                   struct m0_dtm0_log_cfg *dol_cfg)
{
	struct m0_be_tx_credit  cred = {};
	struct dtm0_log_data   *log_data;
	struct m0_be_domain    *dom = dol_cfg->dlc_be_domain;
	struct m0_sm_group     *grp = m0_locality0_get()->lo_grp;
	struct m0_be_btree      dummy_tree = {};
	struct m0_be_seg       *seg = m0_be_domain_seg_first(dom);
	struct m0_be_tx         tx = {};
	int                     rc;

	M0_ENTRY();
	/* TODO check that there is \0 in dol_cfg->dlc_seg0_suffix */
	m0_sm_group_lock(grp);
	m0_be_tx_init(&tx, 0, dom, grp, NULL, NULL, NULL, NULL);
	m0_be_allocator_credit(m0_be_seg_allocator(seg), M0_BAO_ALLOC,
	                       sizeof(*log_data), 0, &cred);
	m0_be_0type_add_credit(dom, &m0_dtm0_log0,
			       (const char *)&dol_cfg->dlc_seg0_suffix,
	                       &M0_BUF_INIT(sizeof log_data, &log_data), &cred);
	/*
	 * dummy_tree is a hack which allows memory allocation for m0_be_btree
	 * and m0_be_btree_create() to happen in the same BE tx.
	 */
	m0_be_btree_init(&dummy_tree, seg, &dtm0_log_transactions_kv_ops);
	m0_be_btree_create_credit(&dummy_tree, 1, &cred);
	m0_be_btree_fini(&dummy_tree);

	dtm0_log_all_p_be_list_credit(M0_BLO_CREATE, 1, &cred);

	m0_be_tx_prep(&tx, &cred);
	rc = m0_be_tx_exclusive_open_sync(&tx);
	M0_ASSERT(rc == 0);     /* XXX */
	M0_BE_ALLOC_PTR_SYNC(log_data, seg, &tx);
	M0_ASSERT(log_data != NULL);    /* XXX */
	rc = m0_be_0type_add(&m0_dtm0_log0, dom, &tx,
			     (const char *)&dol_cfg->dlc_seg0_suffix,
			     &M0_BUF_INIT(sizeof log_data, &log_data));
	M0_ASSERT(rc == 0);     /* XXX */
	m0_be_btree_init(&log_data->dtld_transactions, seg,
	                 &dtm0_log_transactions_kv_ops);
	M0_BE_OP_SYNC(op, m0_be_btree_create(&log_data->dtld_transactions,
	                                     &tx, &op,
					     &dol_cfg->dlc_btree_fid));
	m0_be_btree_fini(&log_data->dtld_transactions);

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

static m0_bcount_t dtm0_log_transactions_ksize(const void *key)
{
	return sizeof ((struct dtm0_log_record *)NULL)->lr_descriptor.dtd_id;
}

static m0_bcount_t dtm0_log_transactions_vsize(const void *key)
{
	return sizeof(struct dtm0_log_record *);
}

static int dtm0_log_transactions_compare(const void *key0, const void *key1)
{
	return 0; /* XXX TODO */
}

M0_INTERNAL int m0_dtm0_log_redo_add(struct m0_dtm0_log        *dol,
                                     struct m0_be_tx           *tx,
                                     const struct m0_dtm0_redo *redo,
                                     const struct m0_fid       *p_sdev_fid)
{
	struct dtm0_log_record  *rec;
	struct m0_bufvec_cursor  cur;
	struct m0_be_seg        *seg = dol->dtl_cfg.dlc_seg;
	int                      rc;

	/* TODO lookup before insert */
	/* TODO check memory allocation errors */
	M0_BE_ALLOC_PTR_SYNC(rec, seg, tx);
	rec->lr_payload_data.b_nob =
		m0_vec_count(&redo->dtr_payload.dtp_data.ov_vec);
	M0_BE_ALLOC_BUF_SYNC(&rec->lr_payload_data, seg, tx);
	rec->lr_descriptor = redo->dtr_descriptor;
	m0_bufvec_cursor_init(&cur, &redo->dtr_payload.dtp_data);
	rc = m0_bufvec_to_data_copy(&cur,
	                            rec->lr_payload_data.b_addr,
	                            rec->lr_payload_data.b_nob);
	M0_ASSERT(rc == 0); /* XXX */
	M0_BE_OP_SYNC(op, m0_be_btree_insert(
	                &dol->dtl_data->dtld_transactions, tx, &op,
	                &M0_BUF_INIT_PTR(&rec->lr_descriptor.dtd_id),
	                &M0_BUF_INIT(sizeof rec, &rec)));
	dtm0_log_all_p_be_tlink_create(rec, tx);
	dtm0_log_all_p_be_list_add_tail(&dol->dtl_data->dtld_all_p, tx, rec);
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

	m0_be_allocator_credit(a, M0_BAO_ALLOC, sizeof *rec, 0, accum);
	m0_be_allocator_credit(a, M0_BAO_ALLOC,
			       m0_vec_count(&redo->dtr_payload.dtp_data.ov_vec),
	                       0, accum);
	m0_be_btree_insert_credit(&dol->dtl_data->dtld_transactions, 1,
	                          sizeof rec->lr_descriptor.dtd_id,
	                          sizeof(void *), accum);
	dtm0_log_all_p_be_list_credit(M0_BLO_TLINK_CREATE, 1, accum);
	dtm0_log_all_p_be_list_credit(M0_BLO_ADD, 1, accum);
}

M0_INTERNAL void m0_dtm0_log_prune(struct m0_dtm0_log *dol,
                                   struct m0_be_tx    *tx,
                                   struct m0_dtx0_id  *dtx0_id)
{
	struct dtm0_log_record *rec;
	struct m0_buf           rec_buf = M0_BUF_INIT(sizeof rec, &rec);
	/* TODO handle lookup errors */
	M0_BE_OP_SYNC(op, m0_be_btree_lookup(
	                &dol->dtl_data->dtld_transactions, &op,
	                &M0_BUF_INIT_PTR(dtx0_id), &rec_buf));
	dtm0_log_all_p_be_list_del(&dol->dtl_data->dtld_all_p, tx, rec);
	dtm0_log_all_p_be_tlink_destroy(rec, tx);
	/* TODO free rec */
	/* TODO check delete result, i.e. t_rc */
	M0_BE_OP_SYNC(op, m0_be_btree_delete(
	                &dol->dtl_data->dtld_transactions, tx, &op,
	                &M0_BUF_INIT_PTR(dtx0_id)));
}

M0_INTERNAL void m0_dtm0_log_prune_credit(struct m0_dtm0_log     *dol,
                                          struct m0_be_tx_credit *accum)
{
	struct dtm0_log_record *rec = NULL; /* XXX */

	m0_be_btree_delete_credit(&dol->dtl_data->dtld_transactions, 1,
	                          sizeof rec->lr_descriptor.dtd_id,
	                          sizeof(void *), accum);
	dtm0_log_all_p_be_list_credit(M0_BLO_DEL, 1, accum);
	dtm0_log_all_p_be_list_credit(M0_BLO_TLINK_DESTROY, 1, accum);
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
