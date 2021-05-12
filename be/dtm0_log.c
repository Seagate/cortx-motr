/* -*- C -*- */
/*
 * Copyright (c) 2021 Seagate Technology LLC and/or its Affiliates
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

#define M0_TRACE_SUBSYSTEM M0_TRACE_SUBSYS_DTM0
#include "be/dtm0_log.h"
#include "be/alloc.h"
#include "dtm0/clk_src.h"
#include "lib/buf.h"
#include "be/list.h"
#include "be/op.h"
#include "be/tx.h"
#include "be/tx_credit.h"
#include "be/seg.h"
#include "lib/assert.h" /* M0_PRE */
#include "lib/errno.h"  /* ENOENT */
#include "lib/memory.h" /* M0_ALLOC */
#include "lib/trace.h"
#include "dtm0/fop.h"  /* dtm0_req_fop */
#include "motr/magic.h"


M0_TL_DESCR_DEFINE(lrec, "DTM0 Log", static, struct m0_dtm0_log_rec,
		   u.dlr_tlink, dlr_magic, M0_BE_DTM0_LOG_REC_MAGIX,
		   M0_BE_DTM0_LOG_MAGIX);
M0_TL_DEFINE(lrec, static, struct m0_dtm0_log_rec);


M0_BE_LIST_DESCR_DEFINE(lrec, "DTM0 PLog", static, struct m0_dtm0_log_rec,
			u.dlr_link, dlr_magic, M0_BE_DTM0_LOG_REC_MAGIX,
			M0_BE_DTM0_LOG_MAGIX);
M0_BE_LIST_DEFINE(lrec, static, struct m0_dtm0_log_rec);


static bool m0_be_dtm0_log__invariant(const struct m0_be_dtm0_log *log)
{
	return _0C(log != NULL) &&
	       _0C(log->dl_cs != NULL);
	/* TODO: Add an invariant check against the volatile part */
	       /* _0C(lrec_tlist_invariant(log->u.dl_inmem)); */
}

static bool m0_dtm0_log_rec__invariant(const struct m0_dtm0_log_rec *rec)
{
	return _0C(rec != NULL) &&
	       _0C(m0_dtm0_tx_desc__invariant(&rec->dlr_txd));
	       /* _0C(m0_tlink_invariant(&lrec_tl, rec)); */
}

/**
 * Allocate memory for a dtm0 volatile log structure.
 */
M0_INTERNAL int m0_be_dtm0_log_alloc(struct m0_be_dtm0_log **out)
{
	struct m0_be_dtm0_log *log;

	M0_PRE(out != NULL);

	M0_ALLOC_PTR(log);
	if (log == NULL)
		return M0_ERR(-ENOMEM);

	M0_ALLOC_PTR(log->u.dl_inmem);
	if (log->u.dl_inmem == NULL) {
		m0_free(log);
		return M0_ERR(-ENOMEM);
	}
	lrec_tlist_init(log->u.dl_inmem);
	*out = log;
	return 0;
}

/*
 * Intialize a dtm0 log. log should point to a pre-allocated
 * dtm0_log structure.
 */
M0_INTERNAL int m0_be_dtm0_log_init(struct m0_be_dtm0_log  *log,
				    struct m0_be_seg       *seg,
				    struct m0_dtm0_clk_src *cs,
				    bool                    is_plog)
{
	M0_PRE(log != NULL);
	M0_PRE(cs != NULL);

	m0_mutex_init(&log->dl_lock);
	log->dl_is_persistent = is_plog;
	log->dl_cs = cs;
	if (is_plog)
		log->dl_seg = seg;
	return 0;
}

M0_INTERNAL void m0_be_dtm0_log_fini(struct m0_be_dtm0_log *log)
{
	M0_PRE(m0_be_dtm0_log__invariant(log));
	m0_mutex_fini(&log->dl_lock);
	lrec_tlist_fini(log->u.dl_inmem);
	log->dl_cs = NULL;
}

M0_INTERNAL void m0_be_dtm0_log_free(struct m0_be_dtm0_log **in_log)
{
	struct m0_be_dtm0_log *log = *in_log;

	M0_PRE(!log->dl_is_persistent);

	m0_free(log->u.dl_inmem);
	m0_free(log);
	*in_log = NULL;
}

M0_INTERNAL void dtm0_be_alloc_tx_desc_credit(struct m0_dtm0_tx_desc *txd,
					      struct m0_be_seg *seg,
					      struct m0_be_tx_credit *accum)
{
	M0_BE_ALLOC_CREDIT_PTR(txd, seg, accum);
	M0_BE_ALLOC_CREDIT_ARR(txd->dtd_ps.dtp_pa,
			       txd->dtd_ps.dtp_nr, seg, accum);
}

M0_INTERNAL void dtm0_be_alloc_log_rec_credit(struct m0_dtm0_tx_desc *txd,
					      struct m0_buf          *payload,
					      struct m0_be_seg       *seg,
					      struct m0_be_tx_credit *accum)
{
	struct m0_dtm0_log_rec *rec;

	M0_BE_ALLOC_CREDIT_PTR(rec, seg, accum);
	M0_BE_ALLOC_CREDIT_BUF(payload, seg, accum);
	dtm0_be_alloc_tx_desc_credit(txd, seg, accum);
	lrec_be_list_credit(M0_BLO_TLINK_CREATE, 1, accum);
}

M0_INTERNAL void dtm0_be_set_log_rec_credit(struct m0_dtm0_tx_desc *txd,
					    struct m0_buf          *payload,
					    struct m0_be_seg       *seg,
					    struct m0_be_tx_credit *accum)
{
	M0_BE_ALLOC_CREDIT_BUF(payload, seg, accum);
	m0_be_tx_credit_add(accum, &M0_BE_TX_CREDIT_TYPE(*payload));
	m0_be_tx_credit_add(accum, &M0_BE_TX_CREDIT_TYPE(*txd));
	m0_be_tx_credit_add(accum,
			    &M0_BE_TX_CREDIT(txd->dtd_ps.dtp_nr,
					     sizeof(struct m0_dtm0_tx_pa *)));
	m0_be_tx_credit_add(accum,
			    &M0_BE_TX_CREDIT(txd->dtd_ps.dtp_nr,
					     sizeof(struct m0_dtm0_tx_pa)));
	m0_be_tx_credit_add(accum, &M0_BE_TX_CREDIT_TYPE(struct m0_dtm0_log_rec));
}

M0_INTERNAL void dtm0_be_del_log_rec_credit(struct m0_be_seg       *seg,
					    struct m0_dtm0_log_rec *rec,
					    struct m0_be_tx_credit *accum)
{
	lrec_be_list_credit(M0_BLO_TLINK_DESTROY, 1, accum);
	M0_BE_FREE_CREDIT_ARR(rec->dlr_txd.dtd_ps.dtp_pa,
			      rec->dlr_txd.dtd_ps.dtp_nr, seg, accum);
	M0_BE_FREE_CREDIT_PTR(rec->dlr_payload.b_addr, seg, accum);
	M0_BE_FREE_CREDIT_PTR(rec, seg, accum);
}

M0_INTERNAL void dtm0_be_log_destroy_credit(struct m0_be_seg       *seg,
					    struct m0_be_tx_credit *accum)
{
	lrec_be_list_credit(M0_BLO_DESTROY, 1, accum);
}

M0_INTERNAL void m0_be_dtm0_log_credit(enum m0_be_dtm0_log_credit_op op,
				       struct m0_dtm0_tx_desc       *txd,
				       struct m0_buf                *payload,
				       struct m0_be_seg             *seg,
				       struct m0_dtm0_log_rec       *rec,
				       struct m0_be_tx_credit       *accum)
{
	/*TODO:Complete implementation during persistent list implementation */

	switch (op) {
	case M0_DTML_CREATE:
		M0_BE_ALLOC_CREDIT_PTR((struct m0_be_dtm0_log *) NULL, seg, accum);
		M0_BE_ALLOC_CREDIT_PTR((struct m0_be_seg *) NULL, seg, accum);
		M0_BE_ALLOC_CREDIT_PTR((struct m0_be_list *) NULL, seg, accum);
		lrec_be_list_credit(M0_BLO_CREATE, 1, accum);
		m0_be_tx_credit_add(accum, &M0_BE_TX_CREDIT_PTR(
					    (struct m0_be_dtm0_log *) NULL));
		break;
	case M0_DTML_EXECUTED:
	case M0_DTML_PERSISTENT:
		dtm0_be_alloc_log_rec_credit(txd, payload, seg, accum);
		dtm0_be_set_log_rec_credit(txd, payload, seg, accum);
		m0_be_list_credit(M0_BLO_ADD, 1, accum);
		break;
	case M0_DTML_PRUNE:
		dtm0_be_del_log_rec_credit(seg, rec, accum);
		m0_be_list_credit(M0_BLO_DEL, 1, accum);
		break;
	case M0_DTML_REDO:
	default:
		M0_IMPOSSIBLE("");
	}
}

M0_INTERNAL int m0_be_dtm0_log_create(struct m0_be_tx        *tx,
				      struct m0_be_seg       *seg,
				      struct m0_be_dtm0_log **out)
{
	struct m0_be_dtm0_log *log;

	M0_PRE(tx != NULL);
	M0_PRE(seg != NULL);

	M0_BE_ALLOC_PTR_SYNC(log, seg, tx);
	M0_ASSERT(log != NULL);

	M0_BE_ALLOC_PTR_SYNC(log->u.dl_persist, seg, tx);
	M0_ASSERT(log->u.dl_persist != NULL);

	log->dl_seg = seg;

	lrec_be_list_create(log->u.dl_persist, tx);
	M0_BE_TX_CAPTURE_PTR(seg, tx, log);
	*out = log;
	return 0;
}

M0_INTERNAL void m0_be_dtm0_log_destroy(struct m0_be_tx        *tx,
					struct m0_be_dtm0_log **log)
{
	/*
	 * TODO: write down the implementation to destroy  persistent
	 * implementation.
	 */
}

M0_INTERNAL
struct m0_dtm0_log_rec *m0_be_dtm0_log_find(struct m0_be_dtm0_log    *log,
					    const struct m0_dtm0_tid *id)
{
	M0_PRE(m0_be_dtm0_log__invariant(log));
	M0_PRE(m0_dtm0_tid__invariant(id));
	M0_PRE(m0_mutex_is_locked(&log->dl_lock));

	if (log->dl_is_persistent) {
		struct m0_dtm0_log_rec *lrec;

		m0_be_list_for(lrec, log->u.dl_persist, lrec) {
			if (m0_dtm0_tid_cmp(log->dl_cs, &lrec->dlr_txd.dtd_id,
					    id) == M0_DTS_EQ)
				break;
		} m0_be_list_endfor;
		return lrec;
	} else {
		return m0_tl_find(lrec, rec, log->u.dl_inmem,
				  m0_dtm0_tid_cmp(log->dl_cs, &rec->dlr_txd.dtd_id,
						  id) == M0_DTS_EQ);
	}
}

static int log_rec_init(struct m0_dtm0_log_rec **rec,
			struct m0_be_tx         *tx,
			struct m0_dtm0_tx_desc  *txd,
			struct m0_buf           *payload)
{
	int                     rc;
	struct m0_dtm0_log_rec *lrec;

	M0_ALLOC_PTR(lrec);
	if (lrec == NULL)
		return M0_ERR(-ENOMEM);

	rc = m0_dtm0_tx_desc_copy(txd, &lrec->dlr_txd);
	if (rc != 0) {
		m0_free(lrec);
		return rc;
	}

	rc = m0_buf_copy(&lrec->dlr_payload, payload);
	if (rc != 0) {
		m0_dtm0_tx_desc_fini(&lrec->dlr_txd);
		m0_free(lrec);
		return rc;
	}

	*rec = lrec;
	return 0;
}

static int plog_rec_init(struct m0_dtm0_log_rec **out,
			 struct m0_be_tx         *tx,
			 struct m0_be_seg        *seg,
			 struct m0_dtm0_tx_desc  *txd,
			 struct m0_buf           *payload)
{
	struct m0_dtm0_log_rec *rec;

	M0_BE_ALLOC_PTR_SYNC(rec, seg, tx);
	M0_ASSERT(rec != NULL);

	rec->dlr_txd.dtd_id = txd->dtd_id;
	rec->dlr_txd.dtd_ps.dtp_nr = txd->dtd_ps.dtp_nr;
	M0_BE_ALLOC_ARR_SYNC(rec->dlr_txd.dtd_ps.dtp_pa,
			     txd->dtd_ps.dtp_nr, seg, tx);
	M0_ASSERT(rec->dlr_txd.dtd_ps.dtp_pa != NULL);

	memcpy(rec->dlr_txd.dtd_ps.dtp_pa, txd->dtd_ps.dtp_pa,
	       sizeof(rec->dlr_txd.dtd_ps.dtp_pa[0]) * rec->dlr_txd.dtd_ps.dtp_nr);

	if (payload->b_nob > 0) {
		rec->dlr_payload.b_nob = payload->b_nob;
		M0_BE_ALLOC_BUF_SYNC(&rec->dlr_payload, seg, tx);
		M0_ASSERT(&rec->dlr_payload.b_addr != NULL); /* TODO: handle error */
		m0_buf_memcpy(&rec->dlr_payload, payload);
		M0_BE_TX_CAPTURE_BUF(seg, tx, &rec->dlr_payload);
	} else {
		rec->dlr_payload.b_addr = NULL;
		rec->dlr_payload.b_nob = 0;
	}

	M0_BE_TX_CAPTURE_ARR(seg, tx,
			     rec->dlr_txd.dtd_ps.dtp_pa,
			     rec->dlr_txd.dtd_ps.dtp_nr);
	M0_BE_TX_CAPTURE_PTR(seg, tx, rec);

	*out = rec;
	return 0;
}

/*
 * TODO: change convention of this function to:
 * void log_rec_fini(struct m0_dtm0_log_rec *rec, ..*tx)
 * and free rec outside the function
 */
static void log_rec_fini(struct m0_dtm0_log_rec **rec,
			 struct m0_be_tx         *tx)
{
	struct m0_dtm0_log_rec *lrec = *rec;

	M0_ASSERT_INFO(M0_IS0(&lrec->dlr_dtx), "DTX should have been finalised "
		       "already in m0_dtx0_done().");
	m0_buf_free(&lrec->dlr_payload);
	m0_dtm0_tx_desc_fini(&lrec->dlr_txd);
	m0_free(lrec);
	*rec = NULL;
}

/*
 * TODO: change convention of this function to:
 * void plog_rec_fini(struct m0_dtm0_log_rec *rec, ..*tx)
 * and allocate rec outside the function
 */
static void plog_rec_fini(struct m0_dtm0_log_rec **dl_lrec,
			  struct m0_be_dtm0_log   *log,
			  struct m0_be_tx         *tx)
{
	struct m0_be_seg       *seg = log->dl_seg;
	struct m0_dtm0_log_rec *rec = *dl_lrec;

	M0_BE_FREE_PTR_SYNC(rec->dlr_txd.dtd_ps.dtp_pa, seg, tx);
	M0_BE_FREE_PTR_SYNC(rec->dlr_payload.b_addr, seg, tx);
	M0_BE_FREE_PTR_SYNC(rec, seg, tx);
	*dl_lrec = NULL;
}

static int dtm0_log__insert(struct m0_be_dtm0_log  *log,
			    struct m0_be_tx        *tx,
			    struct m0_dtm0_tx_desc *txd,
			    struct m0_buf          *payload)
{
	int                      rc;
	struct m0_dtm0_log_rec	*rec;
	struct m0_be_seg	*seg = log->dl_seg;

	if (log->dl_is_persistent) {
		rc = plog_rec_init(&rec, tx, seg, txd, payload);
		if (rc != 0)
			return rc;
		lrec_be_tlink_create(rec, tx);
		lrec_be_list_add_tail(log->u.dl_persist, tx, rec);
	} else {
		rc = log_rec_init(&rec, tx, txd, payload);
		if (rc != 0)
			return rc;
		lrec_tlink_init_at_tail(rec, log->u.dl_inmem);
	}

	return rc;
}

static int dtm0_log__set(struct m0_be_dtm0_log        *log,
			 struct m0_be_tx              *tx,
			 const struct m0_dtm0_tx_desc *txd,
			 const struct m0_buf          *payload,
			 struct m0_dtm0_log_rec       *rec)
{
	bool              is_persistent = log->dl_is_persistent;
	struct m0_buf    *lpayload      = &rec->dlr_payload;
	struct m0_be_seg *seg           = log->dl_seg;

	M0_PRE(m0_dtm0_log_rec__invariant(rec));

	/* Attach payload to log if it is not attached */
	if (!m0_buf_is_set(lpayload) && m0_buf_is_set(payload)) {
		if (is_persistent) {
			lpayload->b_nob = payload->b_nob;
			M0_BE_ALLOC_BUF_SYNC(lpayload, seg, tx);
			M0_ASSERT(lpayload != NULL);
			m0_buf_memcpy(lpayload, payload);
			M0_BE_TX_CAPTURE_BUF(seg, tx, lpayload);
			M0_BE_TX_CAPTURE_PTR(seg, tx, lpayload);
		} else {
			m0_buf_copy(lpayload, payload);
		}
	}

	m0_dtm0_tx_desc_apply(&rec->dlr_txd, txd);
	M0_POST(m0_dtm0_log_rec__invariant(rec));
	if (is_persistent) {
		M0_BE_TX_CAPTURE_ARR(seg, tx, rec->dlr_txd.dtd_ps.dtp_pa,
				     rec->dlr_txd.dtd_ps.dtp_nr);
	}

	return 0;
}

M0_INTERNAL int m0_be_dtm0_log_update(struct m0_be_dtm0_log  *log,
				      struct m0_be_tx        *tx,
				      struct m0_dtm0_tx_desc *txd,
				      struct m0_buf          *payload)
{
	struct m0_dtm0_log_rec	*rec;

	M0_PRE(payload != NULL);
	M0_PRE(m0_be_dtm0_log__invariant(log));
	M0_PRE(m0_dtm0_tx_desc__invariant(txd));
	M0_PRE(m0_mutex_is_locked(&log->dl_lock));

	return (rec = m0_be_dtm0_log_find(log, &txd->dtd_id)) ?
		dtm0_log__set(log, tx, txd, payload, rec) :
		dtm0_log__insert(log, tx, txd, payload);
}

M0_INTERNAL int m0_be_dtm0_log_prune(struct m0_be_dtm0_log    *log,
				     struct m0_be_tx          *tx,
				     const struct m0_dtm0_tid *id)
{
	/* This assignment is meaningful as it covers the empty log case */
	int                     rc = M0_DTS_LT;
	struct m0_dtm0_log_rec *rec;
	struct m0_dtm0_log_rec *currec;

	M0_PRE(m0_be_dtm0_log__invariant(log));
	M0_PRE(m0_dtm0_tid__invariant(id));
	M0_PRE(m0_mutex_is_locked(&log->dl_lock));

	/*
	 * Iterate over the log records from the begining and check whether all
	 * the records preceeding this are persistent. If not, we cannot prune
	 * the record with the given id.
	 */

	m0_tl_for (lrec, log->u.dl_inmem, rec) {
		if (!m0_dtm0_tx_desc_state_eq(&rec->dlr_txd,
					      M0_DTPS_PERSISTENT))
			return M0_ERR(-EPROTO);

		rc = m0_dtm0_tid_cmp(log->dl_cs, &rec->dlr_txd.dtd_id, id);
		if (rc != M0_DTS_LT)
			break;
	} m0_tl_endfor;

	if (rc != M0_DTS_EQ)
		return -ENOENT;

	/* rec is a pointer to the record matching the input id. Delete all the
	 * previous records and then this record. */
	while ((currec = lrec_tlist_pop(log->u.dl_inmem)) != rec) {
		M0_ASSERT(m0_dtm0_log_rec__invariant(currec));
		log_rec_fini(&currec, tx);
	}

	log_rec_fini(&rec, tx);
	return rc;
}

M0_INTERNAL void m0_be_dtm0_log_clear(struct m0_be_dtm0_log *log)
{
	struct m0_dtm0_log_rec *rec;

	/* This function is expected to be called only on the client side where
	 * the log will always be a volatile log.
	 * TBD: can we change the name of dl_is_persistent to something more
	 * intutive.
	 */
	M0_ASSERT(!log->dl_is_persistent);

	m0_tl_teardown(lrec, log->u.dl_inmem, rec) {
		M0_ASSERT(m0_dtm0_log_rec__invariant(rec));
		M0_ASSERT(m0_dtm0_tx_desc_state_eq(&rec->dlr_dtx.dd_txd,
						   M0_DTPS_PERSISTENT));
		log_rec_fini(&rec, NULL);
	}
	M0_POST(lrec_tlist_is_empty(log->u.dl_inmem));
}

M0_INTERNAL int m0_be_dtm0_volatile_log_insert(struct m0_be_dtm0_log  *log,
					       struct m0_dtm0_log_rec *rec)
{
	int rc;

	M0_ENTRY();
	/* TODO: dissolve dlr_txd and remove this code */
	rc = m0_dtm0_tx_desc_copy(&rec->dlr_dtx.dd_txd, &rec->dlr_txd);
	if (rc != 0)
		return M0_ERR(rc);

	lrec_tlink_init_at_tail(rec, log->u.dl_inmem);
	return M0_RC(rc);
}

M0_INTERNAL void m0_be_dtm0_volatile_log_update(struct m0_be_dtm0_log  *log,
						struct m0_dtm0_log_rec *rec)
{
	/* TODO: dissolve dlr_txd and remove this code */
	m0_dtm0_tx_desc_apply(&rec->dlr_txd, &rec->dlr_dtx.dd_txd);
}

/**
 * To prune a record from the log, it needs to be persistent in the log and
 * all the records in the log with id lower than this must be persistent as
 * well. And, when we delete this record we will need to delete all such
 * records.
 * This function is used to determine whether the record with the given id
 * can be pruned from the log and if yes, it also returns a count of the
 * number of be tx credits that would be requried to delete these many
 * records.
 */

M0_INTERNAL bool m0_be_dtm0_plog_can_prune(struct m0_be_dtm0_log    *log,
					   const struct m0_dtm0_tid *id,
					   struct m0_be_tx_credit   *accum)
{
	/* This assignment is meaningful as it covers the empty log case */
	int                     rc = M0_DTS_LT;
	struct m0_dtm0_log_rec *rec;
	struct m0_be_list      *persist = log->u.dl_persist;
	struct m0_be_tx_credit  cred = M0_BE_TX_CREDIT(0, 0);

	M0_PRE(m0_be_dtm0_log__invariant(log));
	M0_PRE(m0_dtm0_tid__invariant(id));
	M0_PRE(m0_mutex_is_locked(&log->dl_lock));

	m0_be_list_for(lrec, persist, rec) {
		if (!m0_dtm0_tx_desc_state_eq(&rec->dlr_txd, M0_DTPS_PERSISTENT))
			return false;

		if (accum != NULL)
			m0_be_dtm0_log_credit(M0_DTML_PRUNE, NULL, NULL,
					      log->dl_seg, rec, &cred);

		rc = m0_dtm0_tid_cmp(log->dl_cs, &rec->dlr_txd.dtd_id, id);
		if (rc != M0_DTS_LT)
			break;
	} m0_be_list_endfor;

	if (rc != M0_DTS_EQ)
		return false;
	if (accum != NULL)
		m0_be_tx_credit_add(accum, &cred);

	return true;
}

M0_INTERNAL int m0_be_dtm0_plog_prune(struct m0_be_dtm0_log    *log,
				      struct m0_be_tx          *tx,
				      const struct m0_dtm0_tid *id)
{
	struct m0_dtm0_log_rec *rec;
	struct m0_dtm0_tid cur_id = {};

	M0_PRE(m0_be_dtm0_log__invariant(log));
	M0_PRE(m0_dtm0_tid__invariant(id));
	M0_PRE(m0_mutex_is_locked(&log->dl_lock));

	m0_be_list_for(lrec, log->u.dl_persist, rec) {
		cur_id = rec->dlr_txd.dtd_id;

		lrec_be_list_del(log->u.dl_persist, tx, rec);
		lrec_be_tlink_destroy(rec, tx);
		plog_rec_fini(&rec, log, tx);
		if (m0_dtm0_tid_cmp(log->dl_cs, &cur_id, id) == M0_DTS_EQ)
			break;
	} m0_be_list_endfor;

	/*
	 * TODO: change the function signature to void if we are always going to
	 * returning 0.
	 */
	return 0;
}

M0_INTERNAL void m0_be_dtm0_log_pmsg_post(struct m0_be_dtm0_log *log,
					  struct m0_fop         *fop)
{
	struct m0_dtm0_log_rec       *rec;
	struct dtm0_req_fop          *req = m0_fop_data(fop);
	const struct m0_dtm0_tx_desc *txd = &req->dtr_txr;
	bool                          is_persistent;

	M0_PRE(log != NULL);
	M0_PRE(!log->dl_is_persistent);
	M0_PRE(fop->f_type == &dtm0_req_fop_fopt);
	M0_PRE(m0_dtm0_tx_desc__invariant(txd));

	M0_ENTRY();

	m0_mutex_lock(&log->dl_lock);
	rec = m0_be_dtm0_log_find(log, &txd->dtd_id);
	/* TODO: We do not handle the case where a P msg is received before
	 * the corresponding DTX enters INPROGRESS state.
	 */
	M0_ASSERT_INFO(rec != NULL, "Log record must be inserted into the log "
		       "in m0_dtx0_close().");
	is_persistent = m0_dtm0_tx_desc_state_eq(&rec->dlr_txd,
						 M0_DTPS_PERSISTENT);
	m0_mutex_unlock(&log->dl_lock);
	/* NOTE: we do not need to hold the global mutex any longer
	 * because of the following assumptions:
	 * 1. Log record cannot be removed unless it is stable.
	 *    We explicitly check that.
	 * 2. The log record linkage is not shared with dtx_post_persistent,
	 *    i.e. it should not try to remove or insert records.
	 *    This point is not enforced.
	 */
	if (!is_persistent)
		m0_dtm0_dtx_pmsg_post(&rec->dlr_dtx, fop);

	M0_LEAVE();
}

#undef M0_TRACE_SUBSYSTEM

/** @} end of dtm group */

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
