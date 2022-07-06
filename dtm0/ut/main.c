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

#include "dtm0/clk_src.h"
#include "dtm0/fop.h"
#include "dtm0/helper.h"
#include "dtm0/service.h"
#include "dtm0/tx_desc.h"
#include "be/dtm0_log.h"
#include "net/net.h"
#include "rpc/rpclib.h"
#include "ut/ut.h"
#include "cas/cas.h"
#include "cas/cas_xc.h"
#include "dtm0/recovery.h"

#include "dtm0/ut/helper.h"

enum {
	NUM_CAS_RECS = 10,
};

struct record
{
	uint64_t key;
	uint64_t value;
};

static void cas_xcode_test(void)
{
	struct record recs[NUM_CAS_RECS];
	struct m0_cas_rec cas_recs[NUM_CAS_RECS];
	struct m0_fid fid = M0_FID_TINIT('i', 0, 0);
	void       *buf;
	m0_bcount_t len;
	int rc;
	int i;
	struct m0_cas_op *op_out;
	struct m0_cas_op op_in = {
		.cg_id  = {
			.ci_fid = fid
		},
		.cg_rec = {
			.cr_rec = cas_recs
		},
		.cg_txd = {
			.dtd_ps = {
				.dtp_nr = 1,
				.dtp_pa = &(struct m0_dtm0_tx_pa) {
					.p_state = 555,
				},
			},
		},
	};

	/* Fill array with pair: [key, value]. */
	m0_forall(i, NUM_CAS_RECS-1,
		  (recs[i].key = i, recs[i].value = i * i, true));

	for (i = 0; i < NUM_CAS_RECS - 1; i++) {
		cas_recs[i] = (struct m0_cas_rec){
			.cr_key = (struct m0_rpc_at_buf) {
				.ab_type  = 1,
				.u.ab_buf = M0_BUF_INIT(sizeof recs[i].key,
							&recs[i].key)
				},
			.cr_val = (struct m0_rpc_at_buf) {
				.ab_type  = 0,
				.u.ab_buf = M0_BUF_INIT(0, NULL)
				},
			.cr_rc = 0 };
	}
	cas_recs[NUM_CAS_RECS - 1] = (struct m0_cas_rec) { .cr_rc = ~0ULL };
	while (cas_recs[op_in.cg_rec.cr_nr].cr_rc != ~0ULL)
		++ op_in.cg_rec.cr_nr;

	rc = m0_xcode_obj_enc_to_buf(&M0_XCODE_OBJ(m0_cas_op_xc, &op_in),
				     &buf, &len);
	M0_UT_ASSERT(rc == 0);
	M0_ALLOC_PTR(op_out);
	M0_UT_ASSERT(op_out != NULL);
	rc = m0_xcode_obj_dec_from_buf(&M0_XCODE_OBJ(m0_cas_op_xc, op_out),
				       buf, len);
	M0_UT_ASSERT(rc == 0);

	m0_xcode_free_obj(&M0_XCODE_OBJ(m0_cas_op_xc, op_out));
}


enum ut_sides {
	UT_SIDE_SRV,
	UT_SIDE_CLI,
	UT_SIDE_NR
};

enum ut_client_persistence {
	UT_CP_UNSPECIFIED,
	UT_CP_VOLATILE_CLIENT,
	UT_CP_PERSISTENT_CLIENT,
};

struct m0_fid g_service_fids[UT_SIDE_NR];

struct ut_remach {
	bool                             use_real_log;
	enum ut_client_persistence       cp;

	struct m0_ut_dtm0_helper         udh;

	struct m0_dtm0_service          *svcs[UT_SIDE_NR];
	struct m0_be_op                  recovered[UT_SIDE_NR];

	/* Client-side stubs for conf objects. */
	struct m0_conf_process           cli_procs[UT_SIDE_NR];
	struct m0_mutex                  cli_proc_guards[UT_SIDE_NR];
};

struct ha_thought {
	enum ut_sides        who;
	enum m0_ha_obj_state what;
};
#define HA_THOUGHT(_who, _what) (struct ha_thought) { \
	.who = _who, .what = _what                    \
}

static struct m0_dtm0_service *ut_remach_svc_get(struct ut_remach *um,
						 enum ut_sides     side)
{
	M0_UT_ASSERT(side < UT_SIDE_NR);
	M0_UT_ASSERT(um->svcs[side] != NULL);
	return um->svcs[side];
}

static struct m0_dtm0_recovery_machine *ut_remach_get(struct ut_remach *um,
						      enum ut_sides     side)
{
	return &ut_remach_svc_get(um, side)->dos_remach;
}

static struct m0_fid *ut_remach_fid_get(enum ut_sides side)
{
	M0_UT_ASSERT(side < UT_SIDE_NR);
	return &g_service_fids[side];
}

static enum ut_sides ut_remach_side_get(const struct m0_fid *svc)
{
	enum ut_sides side;

	for (side = 0; side < UT_SIDE_NR; ++side) {
		if (m0_fid_eq(ut_remach_fid_get(side), svc))
			break;
	}

	M0_UT_ASSERT(side < UT_SIDE_NR);
	return side;
}

static struct ut_remach *ut_remach_from(struct m0_dtm0_recovery_machine *m,
					const struct m0_fid *svc_fid)
{
	struct ut_remach          *um = NULL;
	struct m0_dtm0_service    *svc = M0_AMB(svc, m, dos_remach);
	struct m0_reqh            *reqh = svc->dos_generic.rs_reqh;
	struct m0_reqh_context    *rx = svc->dos_generic.rs_reqh_ctx;
	struct m0_rpc_client_ctx  *cli_ctx;
	struct m0_motr            *motr_ctx;
	struct m0_rpc_server_ctx  *srv_ctx;
	struct m0_ut_dtm0_helper  *udh;
	enum ut_sides              side = ut_remach_side_get(svc_fid);

	if (rx == NULL) {
		M0_UT_ASSERT(side == UT_SIDE_CLI);
		cli_ctx = M0_AMB(cli_ctx, reqh, rcx_reqh);
		udh = M0_AMB(udh, cli_ctx, udh_cctx);
		um = M0_AMB(um, udh, udh);
	} else {
		M0_UT_ASSERT(side == UT_SIDE_SRV);
		motr_ctx = M0_AMB(motr_ctx, rx, cc_reqh_ctx);
		srv_ctx = M0_AMB(srv_ctx, motr_ctx, rsx_motr_ctx);
		udh = M0_AMB(udh, srv_ctx, udh_sctx);
		um = M0_AMB(um, udh, udh);
	}

	M0_UT_ASSERT(um != NULL);
	M0_UT_ASSERT(ut_remach_get(um, side) == m);
	return um;
}

static void ut_remach_log_add_sync(struct ut_remach       *um,
				   enum ut_sides           side,
				   struct m0_dtm0_tx_desc *txd,
				   struct m0_buf          *payload)
{
	struct m0_dtm0_recovery_machine *m = ut_remach_get(um, side);
	struct m0_dtm0_service  *svc   = m->rm_svc;
	struct m0_be_dtm0_log   *log   = svc->dos_log;
	struct m0_be_tx         *tx    = NULL;
	struct m0_be_seg        *seg   = log->dl_seg;
	struct m0_be_tx_credit   cred  = {};
	struct m0_be_ut_backend *ut_be;
	int rc;

	if (log->dl_is_persistent) {
		M0_UT_ASSERT(svc->dos_generic.rs_reqh_ctx != NULL);
		ut_be = &svc->dos_generic.rs_reqh_ctx->rc_be;
		m0_be_dtm0_log_credit(M0_DTML_EXECUTED, txd, payload, seg,
				      NULL, &cred);
		M0_ALLOC_PTR(tx);
		M0_UT_ASSERT(tx != NULL);
		m0_be_ut_tx_init(tx, ut_be);
		m0_be_tx_prep(tx, &cred);
		rc = m0_be_tx_open_sync(tx);
		M0_UT_ASSERT(rc == 0);
	}

	m0_mutex_lock(&log->dl_lock);
	rc = m0_be_dtm0_log_update(log, tx, txd, payload);
	M0_UT_ASSERT(rc == 0);
	m0_mutex_unlock(&log->dl_lock);

	if (log->dl_is_persistent) {
		m0_be_tx_close_sync(tx);
		m0_be_tx_fini(tx);
		m0_free(tx);
	}
}


static void um_dummy_log_redo_post(struct m0_dtm0_recovery_machine *m,
				   struct m0_fom                   *fom,
				   const struct m0_fid *tgt_proc,
				   const struct m0_fid *tgt_svc,
				   struct dtm0_req_fop *redo,
				   struct m0_be_op *op)
{
	struct ut_remach   *um = NULL;
	struct m0_dtm0_recovery_machine *counterpart = NULL;

	um = ut_remach_from(m, &redo->dtr_initiator);
	counterpart = ut_remach_get(um, ut_remach_side_get(tgt_svc));
	m0_dtm0_recovery_machine_redo_post(counterpart, redo, op);
	M0_UT_ASSERT(m0_be_op_is_done(op));
}

static void um_real_log_redo_post(struct m0_dtm0_recovery_machine *m,
				  struct m0_fom                   *fom,
				  const struct m0_fid *tgt_proc,
				  const struct m0_fid *tgt_svc,
				  struct dtm0_req_fop *redo,
				  struct m0_be_op *op)
{
	struct ut_remach   *um = NULL;
	struct m0_dtm0_recovery_machine *counterpart = NULL;
	enum ut_sides tgt_side = ut_remach_side_get(tgt_svc);
	struct m0_dtm0_service *svc;
	struct m0_be_ut_backend *ut_be;

	um = ut_remach_from(m, &redo->dtr_initiator);
	counterpart = ut_remach_get(um, tgt_side);

	/* Empty REDOs are allowed only when EOL is set. */
	M0_UT_ASSERT(ergo(m0_dtm0_tx_desc_is_none(&redo->dtr_txr),
			  !!(redo->dtr_flags & M0_BITS(M0_DMF_EOL))));

	/* Emulate REDO FOM: update the log */
	if (!m0_dtm0_tx_desc_is_none(&redo->dtr_txr))
		ut_remach_log_add_sync(um, tgt_side, &redo->dtr_txr,
				       &redo->dtr_payload);

	m0_dtm0_recovery_machine_redo_post(counterpart, redo, op);
	M0_UT_ASSERT(m0_be_op_is_done(op));

	/*
	 * It is a sordid but simple way of making ::ut_remach_log_add_sync
	 * work:
	 * RPC client does not have a fully-funcitonal context, so that
	 * sm-based BE logic cannot progress because there is no BE associated
	 * with the corresponding FOM (fom -> reqh -> context -> be).
	 * However, both sides share the same set of localities,
	 * so that we can sit down right here and wait until everything
	 * is completed.
	 * It might be slow and dangerous but it is enough for a simple test.
	 */
	if (!m0_dtm0_tx_desc_is_none(&redo->dtr_txr) &&
	    tgt_side == UT_SIDE_SRV) {
		svc = counterpart->rm_svc;
		ut_be = &svc->dos_generic.rs_reqh_ctx->rc_be;
		m0_be_ut_backend_sm_group_asts_run(ut_be);
		m0_be_ut_backend_thread_exit(ut_be);
	}
}

static int um_dummy_log_iter_next(struct m0_dtm0_recovery_machine *m,
				  struct m0_be_dtm0_log_iter *iter,
				  const struct m0_fid *tgt_svc,
				  const struct m0_fid *origin_svc,
				  struct m0_dtm0_log_rec *record)
{
	M0_SET0(record);
	return -ENOENT;
}

static int um_dummy_log_iter_init(struct m0_dtm0_recovery_machine *m,
				  struct m0_be_dtm0_log_iter *iter)
{
	(void) m;
	(void) iter;
	return 0;
}

static void um_dummy_log_iter_fini(struct m0_dtm0_recovery_machine *m,
				   struct m0_be_dtm0_log_iter *iter)
{
	(void) m;
	(void) iter;
	/* nothing to do */
}

void um_ha_event_post(struct m0_dtm0_recovery_machine *m,
		      const struct m0_fid             *tgt_proc,
		      const struct m0_fid             *tgt_svc,
		      enum m0_conf_ha_process_event    event)
{
	struct ut_remach *um   = ut_remach_from(m, tgt_svc);
	enum ut_sides     side = ut_remach_side_get(tgt_svc);

	switch (event) {
	case M0_CONF_HA_PROCESS_DTM_RECOVERED:
		m0_be_op_done(&um->recovered[side]);
		break;
	default:
		M0_UT_ASSERT(false);
	}
}

/*
 * Unicast an HA thought to a particular side.
 */
static void ut_remach_ha_tells(struct ut_remach *um,
			       const struct ha_thought *t,
			       enum ut_sides     whom)
{
	m0_ut_remach_heq_post(ut_remach_get(um, whom),
			      ut_remach_fid_get(t->who), t->what);
}

/*
 * Multicast an HA thought to all the sides.
 */
static void ut_remach_ha_thinks(struct ut_remach        *um,
				const struct ha_thought *t)
{
	enum ut_sides side;

	for (side = 0; side < UT_SIDE_NR; ++side)
		ut_remach_ha_tells(um, t, side);
}

static const struct m0_dtm0_recovery_machine_ops*
ut_remach_ops_get(struct ut_remach *um)
{
	static struct m0_dtm0_recovery_machine_ops dummy_log_ops = {};
	static struct m0_dtm0_recovery_machine_ops real_log_ops = {};
	static bool initialized = false;

	if (!initialized) {
		/* Dummy log operations */
		dummy_log_ops = m0_dtm0_recovery_machine_default_ops;

		dummy_log_ops.log_iter_next  = um_dummy_log_iter_next;
		dummy_log_ops.log_iter_init  = um_dummy_log_iter_init;
		dummy_log_ops.log_iter_fini  = um_dummy_log_iter_fini;

		dummy_log_ops.redo_post      = um_dummy_log_redo_post;
		dummy_log_ops.ha_event_post  = um_ha_event_post;

		/* Real log operations */
		real_log_ops = m0_dtm0_recovery_machine_default_ops;

		real_log_ops.redo_post      = um_real_log_redo_post;
		real_log_ops.ha_event_post  = um_ha_event_post;
		/*
		 * Do not reassign log ops, as we need to deal with real DTM0
		 * log.
		 */

		initialized = true;
	}

	return um->use_real_log ? &real_log_ops : &dummy_log_ops;
}

static void ut_srv_remach_init(struct ut_remach *um)
{
	m0_dtm0_recovery_machine_init(ut_remach_get(um, UT_SIDE_SRV),
				      ut_remach_ops_get(um),
				      ut_remach_svc_get(um, UT_SIDE_SRV));
}

static void ut_cli_remach_conf_obj_init(struct ut_remach *um)
{
	int i;

	for (i = 0; i < UT_SIDE_NR; ++i) {
		m0_mutex_init(&um->cli_proc_guards[i]);
		m0_chan_init(&um->cli_procs[i].pc_obj.co_ha_chan,
			     &um->cli_proc_guards[i]);
	}
}

static void ut_cli_remach_conf_obj_fini(struct ut_remach *um)
{
	int i;

	for (i = 0; i < UT_SIDE_NR; ++i) {
		m0_chan_fini_lock(&um->cli_procs[i].pc_obj.co_ha_chan);
		m0_mutex_fini(&um->cli_proc_guards[i]);
	}
}


static void ut_cli_remach_init(struct ut_remach *um)
{
	ut_cli_remach_conf_obj_init(um);

	m0_dtm0_recovery_machine_init(ut_remach_get(um, UT_SIDE_CLI),
				      ut_remach_ops_get(um),
				      um->svcs[UT_SIDE_CLI]);
}

static void ut_srv_remach_fini(struct ut_remach *um)
{
	m0_dtm0_recovery_machine_fini(ut_remach_get(um, UT_SIDE_SRV));
}

static void ut_cli_remach_fini(struct ut_remach *um)
{
	m0_dtm0_recovery_machine_fini(ut_remach_get(um, UT_SIDE_CLI));
	ut_cli_remach_conf_obj_fini(um);
}

static void ut_remach_start(struct ut_remach *um)
{
	enum ut_sides side;
	int           rc;
	bool          is_volatile[UT_SIDE_NR] = {
		[UT_SIDE_SRV] = false,
		[UT_SIDE_CLI] = um->cp == UT_CP_VOLATILE_CLIENT,
	};

	m0_ut_remach_populate(ut_remach_get(um, UT_SIDE_CLI), um->cli_procs,
			      g_service_fids, is_volatile, UT_SIDE_NR);
	for (side = 0; side < UT_SIDE_NR; ++side) {
		rc = m0_dtm0_recovery_machine_start(ut_remach_get(um, side));
		M0_ASSERT(rc == 0);
	}
}

static void ut_remach_stop(struct ut_remach *um)
{
	enum ut_sides side;
	for (side = 0; side < UT_SIDE_NR; ++side)
		m0_dtm0_recovery_machine_stop(ut_remach_get(um, side));
}

static void ut_remach_init(struct ut_remach *um)
{
	int i;

	M0_UT_ASSERT(M0_IN(um->cp, (UT_CP_PERSISTENT_CLIENT,
				    UT_CP_VOLATILE_CLIENT)));

	for (i = 0; i < ARRAY_SIZE(um->recovered); ++i) {
		m0_be_op_init(um->recovered + i);
		m0_be_op_active(um->recovered + i);
	}

	m0_fi_enable("m0_dtm0_in_ut", "ut");
	m0_fi_enable("is_manual_ss_enabled", "ut");
	/* m0_fi_enable("m0_dtm0_is_expecting_redo_from_client", "ut"); */
	if (um->cp == UT_CP_PERSISTENT_CLIENT)
		m0_fi_enable("is_svc_volatile", "always_false");

	m0_ut_dtm0_helper_init(&um->udh);

	g_service_fids[UT_SIDE_SRV] = um->udh.udh_server_dtm0_fid;
	g_service_fids[UT_SIDE_CLI] = um->udh.udh_client_dtm0_fid;

	M0_UT_ASSERT(um->udh.udh_server_dtm0_service != NULL);
	um->svcs[UT_SIDE_SRV] = um->udh.udh_server_dtm0_service;
	M0_UT_ASSERT(um->udh.udh_client_dtm0_service != NULL);
	um->svcs[UT_SIDE_CLI] = um->udh.udh_client_dtm0_service;

	ut_srv_remach_init(um);
	ut_cli_remach_init(um);
}

static void ut_remach_fini(struct ut_remach *um)
{
	int i;

	ut_cli_remach_fini(um);
	ut_srv_remach_fini(um);
	m0_ut_dtm0_helper_fini(&um->udh);
	if (um->cp == UT_CP_PERSISTENT_CLIENT)
		m0_fi_disable("is_svc_volatile", "always_false");
	/* m0_fi_disable("m0_dtm0_is_expecting_redo_from_client", "ut"); */
	m0_fi_disable("is_manual_ss_enabled", "ut");
	m0_fi_disable("m0_dtm0_in_ut", "ut");
	for (i = 0; i < ARRAY_SIZE(um->recovered); ++i) {
		if (!m0_be_op_is_done(um->recovered + i))
			m0_be_op_done(um->recovered + i);
		m0_be_op_fini(um->recovered + i);
	}

	M0_SET_ARR0(g_service_fids);
}

static void ut_remach_reset_srv(struct ut_remach *um)
{
	int                              rc;
	struct m0_dtm0_recovery_machine *m = ut_remach_get(um, UT_SIDE_SRV);

	m0_dtm0_recovery_machine_stop(m);
	m0_dtm0_recovery_machine_fini(m);
	m0_dtm0_recovery_machine_init(m, ut_remach_ops_get(um),
				      ut_remach_svc_get(um, UT_SIDE_SRV));
	rc = m0_dtm0_recovery_machine_start(m);
	M0_UT_ASSERT(rc == 0);
}

static void ut_remach_log_gen_sync(struct ut_remach *um,
				   enum ut_sides side,
				   uint64_t ts_start,
				   uint64_t records_nr)
{
	struct m0_dtm0_tx_desc           txd = {};
	struct m0_buf                    payload = {};
	int                              rc;
	int                              i;

	rc = m0_dtm0_tx_desc_init(&txd, 1);
	M0_UT_ASSERT(rc == 0);
	txd.dtd_ps.dtp_pa[0] = (struct m0_dtm0_tx_pa) {
		.p_state = M0_DTPS_EXECUTED,
		.p_fid = *ut_remach_fid_get(UT_SIDE_SRV),
	};
	txd.dtd_id = (struct m0_dtm0_tid) {
		.dti_ts.dts_phys = 0,
		.dti_fid = *ut_remach_fid_get(UT_SIDE_CLI),
	};

	for (i = 0; i < records_nr; ++i) {
		txd.dtd_id.dti_ts.dts_phys = ts_start + i;
		ut_remach_log_add_sync(um, side, &txd, &payload);
	}

	m0_dtm0_tx_desc_fini(&txd);
}

/*
 * Ensures that DTM0 log A is a subset of DTM0 log B; and,
 * optionally, that A has at exactly "expected_records_nr" log records
 * (if expected_records_nr < 0 then this check is omitted).
 * Note, pairs (tid, payload) are used as comarison keys. The states
 * of participants and the other fields are ignored.
 */
static void log_subset_verify(struct ut_remach *um,
			      int               expected_records_nr,
			      enum ut_sides     a_side,
			      enum ut_sides     b_side)
{
	struct m0_be_dtm0_log     *a_log =
		ut_remach_get(um, a_side)->rm_svc->dos_log;
	struct m0_be_dtm0_log     *b_log =
		ut_remach_get(um, b_side)->rm_svc->dos_log;
	struct m0_be_dtm0_log_iter a_iter;
	struct m0_dtm0_log_rec     a_record;
	struct m0_dtm0_log_rec    *b_record;
	struct m0_buf             *a_buf;
	struct m0_buf             *b_buf;
	struct m0_dtm0_tid        *tid;
	int                        rc;
	uint64_t                   actual_records_nr = 0;

	m0_mutex_lock(&a_log->dl_lock);
	m0_mutex_lock(&b_log->dl_lock);

	m0_be_dtm0_log_iter_init(&a_iter, a_log);

	while (true) {
		rc = m0_be_dtm0_log_iter_next(&a_iter, &a_record);
		M0_UT_ASSERT(M0_IN(rc, (0, -ENOENT)));
		if (rc == -ENOENT)
			break;
		M0_UT_ASSERT(rc == 0);
		tid = &a_record.dlr_txd.dtd_id;
		b_record = m0_be_dtm0_log_find(b_log, tid);
		M0_UT_ASSERT(b_record != NULL);
		a_buf = &a_record.dlr_payload;
		b_buf = &b_record->dlr_payload;
		M0_UT_ASSERT(equi(m0_buf_is_set(a_buf), m0_buf_is_set(b_buf)));
		M0_UT_ASSERT(ergo(m0_buf_is_set(a_buf),
				  m0_buf_eq(a_buf, b_buf)));
		m0_dtm0_log_iter_rec_fini(&a_record);
		actual_records_nr++;
	}

	M0_UT_ASSERT(ergo(expected_records_nr >= 0,
			  expected_records_nr == actual_records_nr));

	m0_mutex_unlock(&b_log->dl_lock);
	m0_mutex_unlock(&a_log->dl_lock);
}

/* Case: Ensure the machine initialised properly. */
static void remach_init_fini(void)
{
	struct ut_remach um = { .cp = UT_CP_PERSISTENT_CLIENT };
	ut_remach_init(&um);
	ut_remach_fini(&um);
}

/* Case: Ensure the machine is able to start/stop. */
static void remach_start_stop(void)
{
	struct ut_remach um = { .cp = UT_CP_PERSISTENT_CLIENT };
	ut_remach_init(&um);
	ut_remach_start(&um);
	ut_remach_stop(&um);
	ut_remach_fini(&um);
}

static void ut_remach_boot(struct ut_remach *um)
{
	const struct ha_thought starting[] = {
		HA_THOUGHT(UT_SIDE_CLI, M0_NC_TRANSIENT),
		HA_THOUGHT(UT_SIDE_SRV, M0_NC_TRANSIENT),

		HA_THOUGHT(UT_SIDE_CLI, M0_NC_DTM_RECOVERING),
		HA_THOUGHT(UT_SIDE_SRV, M0_NC_DTM_RECOVERING),
	};
	const struct ha_thought started[] = {
		HA_THOUGHT(UT_SIDE_CLI, M0_NC_ONLINE),
		HA_THOUGHT(UT_SIDE_SRV, M0_NC_ONLINE),
	};
	int                     i;

	ut_remach_init(um);
	ut_remach_start(um);

	for (i = 0; i < ARRAY_SIZE(starting); ++i)
		ut_remach_ha_thinks(um, starting + i);

	for (i = 0; i < ARRAY_SIZE(um->recovered); ++i)
		m0_be_op_wait(um->recovered + i);

	for (i = 0; i < ARRAY_SIZE(started); ++i)
		ut_remach_ha_thinks(um, started + i);
}

static void ut_remach_shutdown(struct ut_remach *um)
{
	ut_remach_stop(um);
	M0_UT_ASSERT(m0_be_op_is_done(&um->recovered[UT_SIDE_SRV]));
	M0_UT_ASSERT(m0_be_op_is_done(&um->recovered[UT_SIDE_CLI]));
	ut_remach_fini(um);
}

/* Use-case: gracefull boot and shutdown of 2-node cluster. */
static void remach_boot_cluster(enum ut_client_persistence cp)
{
	struct ut_remach um = { .cp = cp };

	ut_remach_boot(&um);
	ut_remach_shutdown(&um);
}

static void remach_boot_cluster_ss(void)
{
	remach_boot_cluster(UT_CP_PERSISTENT_CLIENT);
}

static void remach_boot_cluster_cs(void)
{
	remach_boot_cluster(UT_CP_VOLATILE_CLIENT);
}

/* Use-case: re-boot an ONLINE node. */
static void remach_reboot_server(void)
{
	struct ut_remach um = { .cp = UT_CP_PERSISTENT_CLIENT };

	ut_remach_boot(&um);

	m0_be_op_reset(um.recovered + UT_SIDE_SRV);
	m0_be_op_active(um.recovered + UT_SIDE_SRV);
	ut_remach_reset_srv(&um);

	ut_remach_ha_thinks(&um, &HA_THOUGHT(UT_SIDE_SRV, M0_NC_TRANSIENT));
	ut_remach_ha_tells(&um, &HA_THOUGHT(UT_SIDE_CLI, M0_NC_ONLINE),
			   UT_SIDE_SRV);
	ut_remach_ha_thinks(&um, &HA_THOUGHT(UT_SIDE_SRV,
					     M0_NC_DTM_RECOVERING));
	m0_be_op_wait(um.recovered + UT_SIDE_SRV);
	ut_remach_ha_thinks(&um, &HA_THOUGHT(UT_SIDE_SRV, M0_NC_ONLINE));

	ut_remach_shutdown(&um);
}

/* Use-case: reboot a node when it started to recover. */
static void remach_reboot_twice(void)
{
	struct ut_remach um = { .cp = UT_CP_PERSISTENT_CLIENT };

	ut_remach_boot(&um);

	m0_be_op_reset(um.recovered + UT_SIDE_SRV);
	m0_be_op_active(um.recovered + UT_SIDE_SRV);
	ut_remach_reset_srv(&um);

	/*
	 * Do not tell the client about failure.
	 * No REDOs would be sent, so that we can see what happens
	 * in the case where recovery machine has to be stopped
	 * in the middle of awaiting for REDOs.
	 */
	ut_remach_ha_tells(&um, &HA_THOUGHT(UT_SIDE_SRV, M0_NC_TRANSIENT),
			   UT_SIDE_SRV);
	ut_remach_ha_tells(&um, &HA_THOUGHT(UT_SIDE_CLI, M0_NC_ONLINE),
			   UT_SIDE_SRV);
	ut_remach_ha_tells(&um, &HA_THOUGHT(UT_SIDE_SRV, M0_NC_DTM_RECOVERING),
			   UT_SIDE_SRV);
	ut_remach_reset_srv(&um);
	M0_UT_ASSERT(!m0_be_op_is_done(&um.recovered[UT_SIDE_SRV]));

	ut_remach_ha_thinks(&um, &HA_THOUGHT(UT_SIDE_SRV, M0_NC_TRANSIENT));
	ut_remach_ha_tells(&um, &HA_THOUGHT(UT_SIDE_CLI, M0_NC_ONLINE),
			   UT_SIDE_SRV);
	ut_remach_ha_thinks(&um, &HA_THOUGHT(UT_SIDE_SRV,
					     M0_NC_DTM_RECOVERING));
	m0_be_op_wait(um.recovered + UT_SIDE_SRV);
	ut_remach_ha_thinks(&um, &HA_THOUGHT(UT_SIDE_SRV, M0_NC_ONLINE));

	ut_remach_shutdown(&um);
}

/* Use-case: replay an empty DTM0 log. */
static void remach_boot_real_log(void)
{
	struct ut_remach um = {
		.cp = UT_CP_PERSISTENT_CLIENT,
		.use_real_log = true
	};
	ut_remach_boot(&um);
	ut_remach_shutdown(&um);
}

/* Use-case: replay a non-empty client log to the server. */
static void remach_real_log_replay(void)
{
	struct ut_remach um = {
		.cp = UT_CP_PERSISTENT_CLIENT,
		.use_real_log = true
	};
	/* cafe bell */
	const uint64_t since = 0xCAFEBELL;
	const uint64_t records_nr = 10;

	ut_remach_boot(&um);

	ut_remach_log_gen_sync(&um, UT_SIDE_CLI, since, records_nr);

	m0_be_op_reset(um.recovered + UT_SIDE_SRV);
	m0_be_op_active(um.recovered + UT_SIDE_SRV);
	ut_remach_reset_srv(&um);

	ut_remach_ha_thinks(&um, &HA_THOUGHT(UT_SIDE_SRV, M0_NC_TRANSIENT));
	ut_remach_ha_tells(&um, &HA_THOUGHT(UT_SIDE_CLI, M0_NC_ONLINE),
			   UT_SIDE_SRV);
	ut_remach_ha_thinks(&um, &HA_THOUGHT(UT_SIDE_SRV,
					     M0_NC_DTM_RECOVERING));
	m0_be_op_wait(um.recovered + UT_SIDE_SRV);
	log_subset_verify(&um, records_nr, UT_SIDE_CLI, UT_SIDE_SRV);
	ut_remach_ha_thinks(&um, &HA_THOUGHT(UT_SIDE_SRV, M0_NC_ONLINE));

	ut_remach_shutdown(&um);
}

extern void m0_dtm0_ut_drlink_simple(void);
extern void m0_dtm0_ut_domain_init_fini(void);

struct m0_ut_suite dtm0_ut = {
	.ts_name = "dtm0-ut",
	.ts_tests = {
		{ "xcode",                  cas_xcode_test        },
		{ "drlink-simple",         &m0_dtm0_ut_drlink_simple },
		{ "domain_init-fini",      &m0_dtm0_ut_domain_init_fini },
		{ "remach-init-fini",       remach_init_fini      },
		{ "remach-start-stop",      remach_start_stop     },
		{ "remach-boot-cluster-ss", remach_boot_cluster_ss },
		{ "remach-boot-cluster-cs", remach_boot_cluster_cs },
		{ "remach-reboot-server",   remach_reboot_server  },
		{ "remach-reboot-twice",    remach_reboot_twice   },
		{ "remach-boot-real-log",   remach_boot_real_log  },
		{ "remach-real-log-replay", remach_real_log_replay  },
		{ NULL, NULL },
	}
};

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
