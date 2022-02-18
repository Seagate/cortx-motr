/* -*- C -*- */
/*
 * Copyright (c) 2017-2020 Seagate Technology LLC and/or its Affiliates
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


#define M0_TRACE_SUBSYSTEM M0_TRACE_SUBSYS_FDMI
#include "lib/trace.h"
#include "lib/memory.h"
#include "lib/finject.h" /* M0_FI_ENABLED */
#include "lib/string.h"         /* strlen */

#include "fdmi/fdmi.h"
#include "fdmi/source_dock.h"
#include "fdmi/fol_fdmi_src.h"
#include "fdmi/filter.h"
#include "fdmi/source_dock_internal.h"
#include "fdmi/module.h"
#include "fop/fop.h"            /* m0_fop_fol_frag */
#include "rpc/rpc_opcodes.h"    /* M0_CAS_PUT_FOP_OPCODE */
#include "cas/cas.h"            /* m0_cas_op */


/**
 * @addtogroup fdmi_fol_src
 *
 * <b>Implementation notes.</b>
 *
 * FDMI needs in-memory representaion of a FOL record to operate on.  So, FOL
 * source will increase backend transaction ref counter (fom->fo_tx.be_tx,
 * using m0_be_tx_get()) to make sure it is not destroyed, and pass
 * m0_fom::fo_tx as a handle to FDMI.  The refcounter will be decremented back
 * once FDMI has completed its processing and all plugins confirm they are
 * done with record.
 *
 * FDMI refc inc/dec will be kept as a separate counter inside m0_be_tx.  This
 * will help prevent/debug cases when FDMI decref calls count does not match
 * incref calls count.  At first, we used transaction lock to protect this
 * counter modification, but it caused deadlocks.  So we switched to using
 * m0_atomic64 instead.  This is OK, since inc/dec operations are never mixed,
 * they are always "in line": N inc operations, followed by N dec operations,
 * so there is no chance of race condition when it decreased to zero, and we
 * initiated tx release operation, and then "someone" decides to increase the
 * counter again.
 *
 * This is implementation of Phase1, which does not need transaction support
 * (that is, we don't need to re-send FDMI records, which would normally
 * happen in case when plugin for example crashes and re-requests FDMI records
 * starting from X in the past).  This assumption results in the following
 * implementation appoach.
 *
 * - We will keep transaction "open" until FDMI reports it has completed
 *   filter processing (m0_fdmi_src::fs_end()).  This
 *   simplifies a lot of stuff -- entire FOL record is available in memory for
 *   our manipulations (get_value and encode calls).
 * - We will NOT count incref/decref calls, and will completely release the
 *   record after m0_fdmi_src::fs_end() call.  (We will
 *   still implement the counter -- to put in a work-around for future
 *   expansion.)
 *
 * @section FDMI FOL records pruning on plugin side
 *
 * FDMI FOL records may be persisted on FDMI plugin side. In this case FDMI
 * plugin must handle duplicates (because the process with FDMI source may
 * restart before receiving "the message had been received" confirmation from
 * FDMI plugin). In case if the source of FDMI records is CAS there might also
 * be duplicates (FDMI FOL records about the same KV operation, but from
 * different CASes due to N-way replication of KV pairs in DIX), and they
 * must also be deduplicated.
 *
 * One of the deduplication approaches is to have some kind of persistence on
 * FDMI plugin side. Every record is looked up in this persistence and if there
 * is a match then it's a duplicate.
 *
 * The persistence couldn't grow indefinitely, so there should be a way to prune
 * it. One obvious thing would be to prune records after some timeout, but
 * delayed DTM recovery may make this timeout very high (days, weeks or more).
 * Another approach is to prune the records after it's known for sure that they
 * are not going to be resent again.
 *
 * Current implementation uses m0_fol_rec_header::rh_lsn to send lsn for each
 * FDMI FOL record to FDMI plugin. This lsn (log sequence number) is a
 * monotonically non-decreasing number that represents position of BE
 * transaction in BE log. FOL record is stored along with other BE tx data in BE
 * log and therefore has the same lsn as the corresponding BE tx. Several
 * transactions may have the same lsn in the current implementation, but there
 * is a limit on a number of transactions with the same lsn.
 * m0_fol_rec_header::rh_lsn_discarded is an lsn, for which every other
 * transaction with lsn less than rh_lsn_discarded is never going to be sent
 * again from the same BE domain (in the configurations that we are using
 * currently it's equivalent to the Motr process that uses this BE domain). It
 * means that every FDMI FOL record which has all rh_lsn less than corresponding
 * rh_lsn_discarded for its BE domain could be discarded from deduplication
 * persistence because there is nothing in the cluster that is going to send
 * FDMI FOL record about this operation.
 *
 * @section FDMI FOL records resend
 *
 * There are 2 major cases here:
 *
 * 1. FDMI plugin restarts, FDMI source is not. In this case FDMI source needs
 *    to resend everything FDMI plugin hasn't confirmed consumption for. This
 *    could be done by FDMI source dock fom by indefinitely resending FDMI
 *    records until either FDMI plugin confirms consumption or FDMI plugin
 *    process fails permanently.
 * 2. FDMI source restarts. The following description is about this case.
 *
 * FDMI source may restart unexpectedly (crash/restart) or it might restart
 * gracefully (graceful shutdown/startup). In either case there may be FDMI FOL
 * records that don't have consumption confirmation from FDMI plugin. Current
 * implementation takes BE tx reference until there is such confirmation from
 * FDMI plugin. BE tx reference taken means in this case that BE tx wouldn't be
 * discarded from BE log until the reference is put. BE recovery recovers all
 * transactions that were not discarded, which is very useful for FDMI FOL
 * record resend case: if all FOL records for such recovered transactions are
 * resent as FDMI FOL records then there will be no FDMI FOL record missing on
 * FDMI plugin side regardless whether FDMI source restarts or not, how and how
 * many times it restarts.
 *
 * It leads to an obvious solution: just send all FOL records as FDMI FOL
 * records during BE recovery.
 *
 * There are several ways this task could be done.
 *
 * @subsection Original FOM for each FOL record
 *
 * For each FOL record a fom with the orignal fom type is created. A special
 * flag is added to indicate that the fom was created during BE recovery . It
 * allows the fom to not to execute it's usual actions, but to close BE tx
 * immediatelly. Then, after BE tx goes to M0_BTS_LOGGED phase a generic fom
 * phase calls m0_fom_fdmi_record_post(), which sends FDMI record to FDMI plugin
 * as usual.
 *
 * @subsection Special FOM for each FOL record
 *
 * A special FOM would be created for each recovered BE tx. The purpose of the
 * fom would be post FDMI record with the FOL record for this BE tx and then
 * wait until consumption of the FDMI record is acklowledged.
 *
 * @subsection Post FDMI records for every BE tx
 *
 * FDMI FOL record for every recovered BE tx is posted as usual. FDMI source
 * dock fom would make a queue of all the records and it would send them and
 * wait for consumption acknowledgement as usual.
 *
 * @subsection A special BE recovery FOM phase
 *
 * A special FOM phase is added to every fom that stores something in BE. If the
 * fom is created during BE recovery, then initial phase of the fom is this
 * special phase. This allows each fom to handle BE recovery as it sees fit.
 * Default generic phase sequence should also include this special fom phase and
 * by default it would post FDMI FOL record in the same way it's done currently
 * for normal FOM phase sequence.
 *
 * @subsection Implementation details
 *
 * - Motr process should be able to send FDMI fops during BE recovery. BE
 *   recovery happens before DTM recovery, so at this stage only HA and Motr
 *   configuration should be brought up (along with their dependencies). It
 *   means that ioservice and DIX would be down at that time;
 * - FOL record shouldn't be discarded until it's consumed by FDMI plugin.
 *   Currently FOL record is in BE tx payload, so this requriement means that BE
 *   tx shouldn't be discarded from BE log before FDMI FOL record is consumed by
 *   FDMI plugin. It has several side effects:
 *   - current implementation recovers BE tx groups one by one, and the next
 *     group is recovered only after references to all transactions from the
 *     previous BE tx group are put. It means that FDMI plugin must consume all
 *     the records before the next BE tx group is recovered. Which doesn't sound
 *     complex on it's own, but there is a use case when it becomes critical:
 *   - if FDMI plugin needs to save FDMI record in a distributed index which is
 *     located on the same set of Motr processes as FDMI source, then we may
 *     have a deadlock even during usual cluster startup after non-clean
 *     shutdown: BE recovery would have some transactions on every participating
 *     server, so DIX wouldn't be operational. And FDMI plugin would require DIX
 *     to be at least somehow operational to consume FDMI records. This could be
 *     solved by allowing to have all BE tx groups to be recovered at the same
 *     time (which would require some rework in BE grouping and BE tx group
 *     fom), but even in this case we may get a deadlock if BE log is full. This
 *     could be solved by always having some part of BE log to be allocted to BE
 *     recovery activities, but with multiple subsequent failures during BE
 *     recovery even this part of the log may get full. It could be solved by
 *     preallocating space in BE segment to copy BE tx payloads from BE log to
 *     handle this particular case, but hey, we only want to resend FDMI records
 *     during BE recovery and record them to DIX in FDMI plugin and now we have
 *     to do several major changes to Motr components archivecture already.
 *
 * @{
 */

/* ------------------------------------------------------------------
 * Fragments handling
 * ------------------------------------------------------------------ */

#define M0_FOL_FRAG_DATA_HANDLER_DECLARE(_opecode, _get_val_func) { \
	.ffh_opecode          = (_opecode),                        \
	.ffh_fol_frag_get_val = (_get_val_func) }

static struct ffs_fol_frag_handler ffs_frag_handler_array[] = {
	M0_FOL_FRAG_DATA_HANDLER_DECLARE(0, NULL)
};

/* ------------------------------------------------------------------
 * List of locked transactions
 * ------------------------------------------------------------------ */

M0_TL_DESCR_DEFINE(ffs_tx, "fdmi fol src tx list", M0_INTERNAL,
		   struct m0_be_tx, t_fdmi_linkage, t_magic,
		   M0_BE_TX_MAGIC, M0_BE_TX_ENGINE_MAGIC);

M0_TL_DEFINE(ffs_tx, M0_INTERNAL, struct m0_be_tx);

/* ------------------------------------------------------------------
 * Helpers
 * ------------------------------------------------------------------ */

static struct m0_dtx* ffs_get_dtx(struct m0_fdmi_src_rec *src_rec)
{
	/* This is just wrapper, so no point using ENTRY/LEAVE */

	struct m0_fol_rec *fol_rec;

	M0_ASSERT(m0_fdmi__record_is_valid(src_rec));
	fol_rec = container_of(src_rec, struct m0_fol_rec, fr_fdmi_rec);
	return container_of(fol_rec, struct m0_dtx, tx_fol_rec);
}

static void be_tx_put_ast_cb(struct m0_sm_group *grp, struct m0_sm_ast *ast)
{
	struct m0_be_tx  *be_tx = ast->sa_datum;

	M0_ENTRY("sm_group %p, ast %p (be_tx = %p)", grp, ast, be_tx);
	M0_LOG(M0_DEBUG, "call be_tx_put direct (2)");
	m0_be_tx_put(be_tx);

	M0_LEAVE();
}

static void ffs_tx_inc_refc(struct m0_be_tx *be_tx, int64_t *counter)
{
	struct m0_fdmi_module *m = m0_fdmi_module__get();
	int64_t                cnt;

	M0_ENTRY("be_tx %p", be_tx);

	M0_ASSERT(be_tx != NULL);

	if (m0_atomic64_get(&be_tx->t_fdmi_ref) == 0) {
		/**
		 * Value = 0 means this call happened during record
		 * posting. Execution context is well-defined, all
		 * locks already acquired, no need to use AST.
		 */
		M0_LOG(M0_INFO, "first incref for a be_tx_get %p", be_tx);
		m0_be_tx_get(be_tx);
		m0_mutex_lock(&m->fdm_s.fdms_ffs_locked_tx_lock);
		ffs_tx_tlink_init_at_tail(be_tx,
			&m->fdm_s.fdms_ffs_locked_tx_list);
		m0_mutex_unlock(&m->fdm_s.fdms_ffs_locked_tx_lock);
	}

	cnt = m0_atomic64_add_return(&be_tx->t_fdmi_ref, 1);
	M0_ASSERT(cnt > 0);

	if (counter != NULL)
		*counter = cnt;
	M0_LEAVE("counter = %"PRIi64, cnt);
}

static void ffs_tx_dec_refc(struct m0_be_tx *be_tx, int64_t *counter)
{
	struct m0_fdmi_module *m = m0_fdmi_module__get();
	int64_t                cnt;

	M0_ENTRY("be_tx %p, counter ptr %p", be_tx, counter);

	M0_ASSERT(be_tx != NULL);

	cnt = m0_atomic64_sub_return(&be_tx->t_fdmi_ref, 1);
	M0_ASSERT(cnt >= 0);

	if (counter != NULL)
		*counter = cnt;

	if (cnt == 0) {
		m0_mutex_lock(&m->fdm_s.fdms_ffs_locked_tx_lock);
		ffs_tx_tlink_del_fini(be_tx);
		m0_mutex_unlock(&m->fdm_s.fdms_ffs_locked_tx_lock);
		M0_LOG(M0_DEBUG, "call be_tx_put CB");
		be_tx->t_fdmi_put_ast.sa_cb    = be_tx_put_ast_cb;
		be_tx->t_fdmi_put_ast.sa_datum = be_tx;
		m0_sm_ast_post(be_tx->t_sm.sm_grp, &be_tx->t_fdmi_put_ast);
		M0_LOG(M0_DEBUG, "last decref for a be_tx %p "
		       "(ast callback posted)", be_tx);
	}
	M0_LEAVE("counter = %"PRIi64, cnt);
}

static int64_t ffs_rec_get(struct m0_fdmi_src_rec *src_rec)
{
	struct m0_dtx *dtx;
	int64_t cnt;

	/*
	 * If this is the first time call on a new rec, some member
	 * of this struct may not be fully populated. But it's fine
	 * to get dtx.
	 */
	dtx = ffs_get_dtx(src_rec);
	M0_ASSERT(dtx != NULL);

	ffs_tx_inc_refc(&dtx->tx_betx, &cnt);

	return cnt;
}

static int64_t ffs_rec_put(struct m0_fdmi_src_rec *src_rec)
{
	struct m0_dtx *dtx;
	int64_t cnt;

	M0_ASSERT(m0_fdmi__record_is_valid(src_rec));

	dtx = ffs_get_dtx(src_rec);
	M0_ASSERT(dtx != NULL);

	ffs_tx_dec_refc(&dtx->tx_betx, &cnt);

	return cnt;
}

/* ------------------------------------------------------------------
 * FOL source interface implementation
 * ------------------------------------------------------------------ */

static int ffs_op_node_eval(struct m0_fdmi_src_rec	*src_rec,
			    struct m0_fdmi_flt_var_node *value_desc,
			    struct m0_fdmi_flt_operand  *value)
{
	struct m0_dtx          *dtx;
	struct m0_fol_rec      *fol_rec;
	uint64_t                opcode;
	struct m0_fol_frag     *rfrag;
	struct m0_fop_fol_frag *rp;
	int                     rc;

	M0_ENTRY("src_rec %p, value desc %p, value %p",
		 src_rec, value_desc, value);

	M0_ASSERT(m0_fdmi__record_is_valid(src_rec));
	M0_ASSERT(value_desc != NULL && value != NULL);

	dtx = ffs_get_dtx(src_rec);
	M0_ASSERT(dtx != NULL);

	fol_rec = &dtx->tx_fol_rec;

	/** @todo Phase 2: STUB: For now, we will not analyze filter, we just
	 * return FOL op code -- always. */

	rfrag = m0_rec_frag_tlist_head(&fol_rec->fr_frags);
	M0_ASSERT(rfrag != NULL);

	/**
	 * TODO: Q: (question to FOP/FOL owners) I could not find a better way
	 * to assert that this frag is of m0_fop_fol_frag_type, than to use this
	 * workaround (referencing internal _ops structure). Looks like they are
	 * ALWAYS of this type?...  Now that there is NO indication of frag
	 * type whatsoever?... */
	M0_ASSERT(rfrag->rp_ops->rpo_type == &m0_fop_fol_frag_type);

	rp = rfrag->rp_data;
	M0_ASSERT(rp != NULL);
	opcode = rp->ffrp_fop_code;
	m0_fdmi_flt_uint_opnd_fill(value, opcode);
	rc = 0;

	return M0_RC(rc);
}

static void ffs_op_get(struct m0_fdmi_src_rec *src_rec)
{
	int64_t cnt;

	M0_ASSERT(m0_fdmi__record_is_valid(src_rec));

	/* Proper transactional handling is for phase 2. */
	cnt = ffs_rec_get(src_rec);

	M0_LOG(M0_DEBUG, "src_rec %p counter=%"PRIi64, src_rec, cnt);
}

static void ffs_op_put(struct m0_fdmi_src_rec *src_rec)
{
	int64_t cnt;
	M0_ASSERT(m0_fdmi__record_is_valid(src_rec));

	/* Proper transactional handling is for phase 2. */
	cnt = ffs_rec_put(src_rec);

	M0_LOG(M0_DEBUG, "src_rec %p counter=%"PRIi64, src_rec, cnt);
}

static int ffs_op_encode(struct m0_fdmi_src_rec *src_rec,
			 struct m0_buf          *buf)
{
	struct m0_dtx      *dtx;
	struct m0_fol_rec  *fol_rec;
	struct m0_buf       local_buf = {};
	int                 rc;

	M0_ASSERT(buf != NULL);
	M0_ASSERT(m0_fdmi__record_is_valid(src_rec));
	M0_ASSERT(buf->b_addr == NULL && buf->b_nob == 0);

	M0_ENTRY("src_rec %p, cur " BUF_F, src_rec, BUF_P(buf));

	dtx = ffs_get_dtx(src_rec);
	M0_ASSERT(dtx != NULL);

	fol_rec = &dtx->tx_fol_rec;

	/**
	 * @todo Q: (for FOL owners) FOL record does not provide API call to
	 * calculate record size when encoded.  For now, I'll do double
	 * allocation.  Alloc internal buf of max size, then encode, then
	 * alloc with correct size, then copy, then dealloc inernal buf.  Can
	 * be done properly once FOL record owner exports needed api call.
	 */
	rc = m0_buf_alloc(&local_buf, FOL_REC_MAXSIZE);
	if (rc != 0) {
		return M0_ERR_INFO(rc, "Failed to allocate internal buffer "
				   "for encoded FOL FDMI record.");
	}

	rc = m0_fol_rec_encode(fol_rec, &local_buf);
	if (rc != 0) {
		M0_LOG(M0_ERROR,
		       "Failed to encoded FOL FDMI record.");
		goto done;
	}

	rc = m0_buf_alloc(buf, fol_rec->fr_header.rh_data_len);
	if (rc != 0) {
		M0_LOG(M0_ERROR,
		       "Failed to allocate encoded FOL FDMI record.");
		goto done;
	}
	memcpy(buf->b_addr, local_buf.b_addr, buf->b_nob);

	if (M0_FI_ENABLED("fail_in_final"))
		rc = -EINVAL;

done:
	/* Finalization */
	if (local_buf.b_addr != NULL)
		m0_buf_free(&local_buf);
	/* On-Error cleanup. */
	if (rc < 0) {
		if (buf->b_addr != NULL)
			m0_buf_free(buf);
	}

	return M0_RC(rc);
}

static int ffs_op_decode(struct m0_buf *buf, void **handle)
{
	struct m0_fol_rec *fol_rec = 0;
	int                rc = 0;

	M0_ASSERT(buf != NULL && buf->b_addr != NULL && handle != NULL);

	M0_ENTRY("buf " BUF_F ", handle %p", BUF_P(buf), handle);

	M0_ALLOC_PTR(fol_rec);
	if (fol_rec == NULL) {
		M0_LOG(M0_ERROR, "failed to allocate m0_fol_rec object");
		rc = -ENOMEM;
		goto done;
	}
	m0_fol_rec_init(fol_rec, NULL);

	rc = m0_fol_rec_decode(fol_rec, buf);
	if (rc < 0)
		goto done;

	*handle = fol_rec;

	if (M0_FI_ENABLED("fail_in_final"))
		rc = -EINVAL;

done:
	if (rc < 0) {
		if (fol_rec != NULL) {
			m0_fol_rec_fini(fol_rec);
			m0_free0(&fol_rec);
		}
		*handle = NULL;
	}

	return M0_RC(rc);
}

static void ffs_op_begin(struct m0_fdmi_src_rec *src_rec)
{
	M0_ENTRY("src_rec %p", src_rec);

	M0_ASSERT(m0_fdmi__record_is_valid(src_rec));

	/**
	 * No need to do anything on this event for FOL Source.  Call to
	 * ffs_rec_get() done in m0_fol_fdmi_post_record below will make sure
	 * the data is already in memory and available for fast access at the
	 * moment of this call.
	 */

	(void)src_rec;

	M0_LEAVE();
}

static void ffs_op_end(struct m0_fdmi_src_rec *src_rec)
{
	M0_ENTRY("src_rec %p", src_rec);

	M0_ASSERT(m0_fdmi__record_is_valid(src_rec));

	M0_LEAVE();
}

/* ------------------------------------------------------------------
 * Init/fini
 * ------------------------------------------------------------------ */

M0_INTERNAL int m0_fol_fdmi_src_init(void)
{
	struct m0_fdmi_module *m = m0_fdmi_module__get();
	int                    rc;

	M0_ENTRY();

	M0_ASSERT(m->fdm_s.fdms_ffs_ctx.ffsc_src == NULL);
	m->fdm_s.fdms_ffs_ctx.ffsc_magic = M0_FOL_FDMI_SRC_CTX_MAGIC;

	rc = m0_fdmi_source_alloc(M0_FDMI_REC_TYPE_FOL,
		&m->fdm_s.fdms_ffs_ctx.ffsc_src);
	if (rc != 0)
		return M0_ERR(rc);

	ffs_tx_tlist_init(&m->fdm_s.fdms_ffs_locked_tx_list);
	m0_mutex_init(&m->fdm_s.fdms_ffs_locked_tx_lock);

	m->fdm_s.fdms_ffs_ctx.ffsc_src->fs_node_eval  = ffs_op_node_eval;
	m->fdm_s.fdms_ffs_ctx.ffsc_src->fs_get        = ffs_op_get;
	m->fdm_s.fdms_ffs_ctx.ffsc_src->fs_put        = ffs_op_put;
	m->fdm_s.fdms_ffs_ctx.ffsc_src->fs_begin      = ffs_op_begin;
	m->fdm_s.fdms_ffs_ctx.ffsc_src->fs_end        = ffs_op_end;
	m->fdm_s.fdms_ffs_ctx.ffsc_src->fs_encode     = ffs_op_encode;
	m->fdm_s.fdms_ffs_ctx.ffsc_src->fs_decode     = ffs_op_decode;

	rc = m0_fdmi_source_register(m->fdm_s.fdms_ffs_ctx.ffsc_src);
	if (rc != 0) {
		M0_LOG(M0_ERROR, "Failed to register FDMI FOL source.");
		goto error_free_src;
	}

	m->fdm_s.fdms_ffs_ctx.ffsc_frag_handler_vector = ffs_frag_handler_array;
	m->fdm_s.fdms_ffs_ctx.ffsc_handler_number      =
		ARRAY_SIZE(ffs_frag_handler_array);
	return M0_RC(rc);
error_free_src:
	m0_fdmi_source_deregister(m->fdm_s.fdms_ffs_ctx.ffsc_src);
	m0_fdmi_source_free(m->fdm_s.fdms_ffs_ctx.ffsc_src);
	m->fdm_s.fdms_ffs_ctx.ffsc_src = NULL;
	return M0_RC(rc);
}

M0_INTERNAL void m0_fol_fdmi_src_fini(void)
{
	M0_ENTRY();
	m0_fol_fdmi_src_deinit();
	M0_LEAVE();
}

M0_INTERNAL int m0_fol_fdmi_src_deinit(void)
{
	struct m0_fdmi_module   *m = m0_fdmi_module__get();
	struct m0_fdmi_src_ctx  *src_ctx;
	struct m0_be_tx		*be_tx;
	int rc = 0;

	M0_ENTRY();

	M0_PRE(m->fdm_s.fdms_ffs_ctx.ffsc_src != NULL);
	src_ctx = container_of(m->fdm_s.fdms_ffs_ctx.ffsc_src,
			       struct m0_fdmi_src_ctx, fsc_src);

	M0_PRE(src_ctx->fsc_registered);
	M0_PRE(m->fdm_s.fdms_ffs_ctx.ffsc_src->fs_record_post != NULL);

	/**
	 * The deregister below does not call for fs_put/fs_end, so
	 * we'll have to do call m0_be_tx_put explicitly here, over
	 * all transactions we've locked.
	 */
	m0_mutex_lock(&m->fdm_s.fdms_ffs_locked_tx_lock);
	m0_tlist_for(&ffs_tx_tl, &m->fdm_s.fdms_ffs_locked_tx_list, be_tx) {
		ffs_tx_tlink_del_fini(be_tx);
		m0_be_tx_put(be_tx);
		/**
		 * Note we don't reset t_fdmi_ref here, it's a flag
		 * the record is not yet released by plugins.
		 */
	} m0_tlist_endfor;
	m0_mutex_unlock(&m->fdm_s.fdms_ffs_locked_tx_lock);
	m0_mutex_fini(&m->fdm_s.fdms_ffs_locked_tx_lock);

	ffs_tx_tlist_fini(&m->fdm_s.fdms_ffs_locked_tx_list);

	m0_fdmi_source_deregister(m->fdm_s.fdms_ffs_ctx.ffsc_src);
	m0_fdmi_source_free(m->fdm_s.fdms_ffs_ctx.ffsc_src);
	m->fdm_s.fdms_ffs_ctx.ffsc_src = NULL;
	return M0_RC(rc);
}

/* ------------------------------------------------------------------
 * Entry point for FOM to start FDMI processing
 * ------------------------------------------------------------------ */

M0_INTERNAL void m0_fol_fdmi_post_record(struct m0_fom *fom)
{
	struct m0_fdmi_src_dock *src_dock = m0_fdmi_src_dock_get();
	struct m0_fdmi_module   *m = m0_fdmi_module__get();
	struct m0_dtx           *dtx;
	struct m0_be_tx         *be_tx;

	M0_ENTRY("fom: %p", fom);

	M0_ASSERT(fom != NULL);
	M0_ASSERT(m->fdm_s.fdms_ffs_ctx.ffsc_src->fs_record_post != NULL);

	if (!src_dock->fsdc_started) {
		/*
		 * It should really be M0_ASSERT() here, because posting FDMI
		 * records when there is nothing running to process them is a
		 * bug (FDMI ensures guaranteed delivery and it's impossible if
		 * there is nothing running that could take care of delivery.
		 *
		 * But the current codebase is not ready for such change.
		 * Let's mark is as just another "Phase 2" TODO.
		 */
		M0_LOG(M0_ERROR, "src dock fom is not running");
		return;
	}
	if (!src_dock->fsdc_filters_defined) {
		/* No filters defined. Let's not post the records. */
		return;
	}


	/**
	 * There is no "unpost record" method, so we have to prepare
	 * everything that may fail -- before calling to post method.
	 */

	dtx   = &fom->fo_tx;
	be_tx = &fom->fo_tx.tx_betx;


	m0_be_tx_lsn_get(be_tx, &dtx->tx_fol_rec.fr_header.rh_lsn,
	                 &dtx->tx_fol_rec.fr_header.rh_lsn_discarded);
	dtx->tx_fol_rec.fr_fdmi_rec.fsr_src  = m->fdm_s.fdms_ffs_ctx.ffsc_src;
	dtx->tx_fol_rec.fr_fdmi_rec.fsr_dryrun = false;
	dtx->tx_fol_rec.fr_fdmi_rec.fsr_data = NULL;

	/* Post record. */
	M0_FDMI_SOURCE_POST_RECORD(&dtx->tx_fol_rec.fr_fdmi_rec);
	M0_LOG(M0_DEBUG, "M0_FDMI_SOURCE_POST_RECORD fr_fdmi_rec=%p "
	       "fsr_rec_id="U128X_F, &dtx->tx_fol_rec.fr_fdmi_rec,
	       U128_P(&dtx->tx_fol_rec.fr_fdmi_rec.fsr_rec_id));

	/* Aftermath. */

	/**
	 * NOTE: IMPORTANT! Do not call anything that may fail here! It is
	 * not possible to un-post the record; anything that may fail, must be
	 * done before the M0_FDMI_SOURCE_POST_RECORD call above.
	 */

	M0_LEAVE();
}

M0_INTERNAL bool
m0_fol_fdmi__filter_kv_substring_match(struct m0_buf  *value,
                                       const char    **substrings)
{
	struct m0_buf s;
	m0_bcount_t   j;
	bool          match;
	int           i;

	for (i = 0; substrings[i] != NULL; ++i) {
		s = M0_BUF_INIT_CONST(strlen(substrings[i]), substrings[i]);
		if (value->b_nob < s.b_nob)
			return false;
		match = false;
		/* brute-force */
		for (j = 0; j <= value->b_nob - s.b_nob; ++j) {
			if (m0_buf_eq(&s, &M0_BUF_INIT(s.b_nob,
			                               value->b_addr + j))) {
				match = true;
				break;
			}
		}
		if (!match)
			return false;
	}
	return true;
}

M0_INTERNAL int
m0_fol_fdmi_filter_kv_substring(struct m0_fdmi_eval_ctx      *ctx,
                                struct m0_conf_fdmi_filter   *filter,
                                struct m0_fdmi_eval_var_info *var_info)
{
	struct m0_fdmi_src_rec *src_rec = var_info->user_data;
	struct m0_fop_fol_frag *fop_fol_frag;
	struct m0_fol_frag     *fol_frag;
	struct m0_fol_rec      *fol_rec;
	struct m0_cas_rec      *cas_rec;
	struct m0_cas_op       *cas_op;
	int                     i;

	fol_rec = container_of(src_rec, struct m0_fol_rec, fr_fdmi_rec);
	m0_tl_for(m0_rec_frag, &fol_rec->fr_frags, fol_frag) {
		if (fol_frag->rp_ops->rpo_type != &m0_fop_fol_frag_type)
			continue;
		fop_fol_frag = fol_frag->rp_data;
		if (fop_fol_frag->ffrp_fop_code != M0_CAS_PUT_FOP_OPCODE &&
		    fop_fol_frag->ffrp_fop_code != M0_CAS_DEL_FOP_OPCODE)
			continue;
		cas_op = fop_fol_frag->ffrp_fop;
		M0_ASSERT(cas_op != NULL);
		for (i = 0; i < cas_op->cg_rec.cr_nr; ++i) {
			cas_rec = &cas_op->cg_rec.cr_rec[i];
			if (m0_fol_fdmi__filter_kv_substring_match(
				   &cas_rec->cr_val.u.ab_buf,
				   filter->ff_substrings))
				return 1;
		}
	} m0_tl_endfor;
	return 0;
}

/**
 * @} addtogroup fdmi_fol_src
 */

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
