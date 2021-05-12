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
#include "lib/trace.h"
#include "addb2/addb2.h"
#include "be/dtm0_log.h"
#include "dtm0/addb2.h"
#include "dtm0/fop.h"
#include "dtm0/fop_xc.h"
#include "dtm0/service.h"
#include "dtm0/tx_desc.h"
#include "dtm0/tx_desc_xc.h"
#include "lib/errno.h"
#include "lib/assert.h"
#include "lib/memory.h"
#include "lib/chan.h"
#include "lib/finject.h"
#include "lib/time.h"
#include "lib/misc.h"           /* M0_IN() */
#include "fop/fop.h"
#include "fop/fom.h"
#include "fop/fom_generic.h"
#include "fop/fop_item_type.h"
#include "rpc/rpc_opcodes.h"
#include "rpc/rpc.h"
#include "rpc/rpclib.h"

static void dtm0_rpc_item_reply_cb(struct m0_rpc_item *item);

/*
  RPC item operations structures.
 */
const struct m0_rpc_item_ops dtm0_req_fop_rpc_item_ops = {
        .rio_replied = dtm0_rpc_item_reply_cb,
};

struct m0_fop_type dtm0_req_fop_fopt;
struct m0_fop_type dtm0_rep_fop_fopt;

/*
  Fom specific routines for corresponding fops.
 */
static int dtm0_fom_tick(struct m0_fom *fom);
static int dtm0_fom_create(struct m0_fop *fop, struct m0_fom **out,
			       struct m0_reqh *reqh);
static void dtm0_fom_fini(struct m0_fom *fom);
static size_t dtm0_fom_locality(const struct m0_fom *fom);

static const struct m0_fom_ops dtm0_req_fom_ops = {
	.fo_fini = dtm0_fom_fini,
	.fo_tick = dtm0_fom_tick,
	.fo_home_locality = dtm0_fom_locality
};

extern struct m0_reqh_service_type dtm0_service_type;

enum dtm0_phases {
	DTM0_REQ_WHATEVER = M0_FOPH_NR + 1,
};

static const struct m0_fom_type_ops dtm0_req_fom_type_ops = {
        .fto_create = dtm0_fom_create,
};

static void dtm0_rpc_item_reply_cb(struct m0_rpc_item *item)
{
	struct m0_fop *reply = NULL;

	M0_ENTRY("item=%p", item);

	M0_PRE(item != NULL);
	M0_PRE(M0_IN(m0_fop_opcode(m0_rpc_item_to_fop(item)),
		     (M0_DTM0_REQ_OPCODE)));

	if (m0_rpc_item_error(item) == 0) {
		reply = m0_rpc_item_to_fop(item->ri_reply);
		M0_ASSERT(M0_IN(m0_fop_opcode(reply), (M0_DTM0_REP_OPCODE)));
	}

	M0_LEAVE("reply=%p", reply);
}

M0_INTERNAL void m0_dtm0_fop_fini(void)
{
	m0_fop_type_fini(&dtm0_req_fop_fopt);
	m0_fop_type_fini(&dtm0_rep_fop_fopt);
	m0_xc_dtm0_fop_fini();
}

enum {
	M0_FOPH_DTM0_LOGGING = M0_FOPH_TYPE_SPECIFIC,
};

struct m0_sm_state_descr dtm0_phases[] = {
	[M0_FOPH_DTM0_LOGGING] = {
		.sd_name      = "logging",
		.sd_allowed   = M0_BITS(M0_FOPH_SUCCESS,
					M0_FOPH_FAILURE)
	},
};

struct m0_sm_trans_descr dtm0_phases_trans[] = {
	[ARRAY_SIZE(m0_generic_phases_trans)] =
	{"dtm0_1-fail", M0_FOPH_TYPE_SPECIFIC, M0_FOPH_FAILURE},
	{"dtm0_1-success", M0_FOPH_TYPE_SPECIFIC, M0_FOPH_SUCCESS},
};

static struct m0_sm_conf dtm0_conf = {
	.scf_name      = "dtm0-fom",
	.scf_nr_states = ARRAY_SIZE(dtm0_phases),
	.scf_state     = dtm0_phases,
	.scf_trans_nr  = ARRAY_SIZE(dtm0_phases_trans),
	.scf_trans     = dtm0_phases_trans,
};

M0_INTERNAL int m0_dtm0_fop_init(void)
{
	static int init_once = 0;

	if (init_once++ > 0)
		return 0;

	m0_sm_conf_extend(m0_generic_conf.scf_state, dtm0_phases,
			  m0_generic_conf.scf_nr_states);
	m0_sm_conf_trans_extend(&m0_generic_conf, &dtm0_conf);
	m0_sm_conf_init(&dtm0_conf);


	m0_xc_dtm0_fop_init();
	M0_FOP_TYPE_INIT(&dtm0_req_fop_fopt,
			 .name      = "DTM0 request",
			 .opcode    = M0_DTM0_REQ_OPCODE,
			 .xt        = dtm0_req_fop_xc,
			 .rpc_flags = M0_RPC_MUTABO_REQ,
			 .fom_ops   = &dtm0_req_fom_type_ops,
			 .sm        = &dtm0_conf,
			 .svc_type  = &dtm0_service_type);
	M0_FOP_TYPE_INIT(&dtm0_rep_fop_fopt,
			 .name      = "DTM0 reply",
			 .opcode    = M0_DTM0_REP_OPCODE,
			 .xt        = dtm0_rep_fop_xc,
			 .rpc_flags = M0_RPC_ITEM_TYPE_REPLY,
			 .fom_ops   = &dtm0_req_fom_type_ops);
	return m0_fop_type_addb2_instrument(&dtm0_req_fop_fopt);
}

/*
  Allocates a fop.
 */

int m0_dtm0_fop_create(struct m0_rpc_session  *session,
		       enum m0_dtm0s_msg       opmsg,
		       struct m0_dtm0_tx_desc *txr,
		       struct m0_fop         **out)
{
	struct dtm0_req_fop *op;
	struct m0_fop	    *fop;

	*out = NULL;

	fop = m0_fop_alloc_at(session, &dtm0_req_fop_fopt);
	if (fop == NULL)
		return M0_ERR(-ENOMEM);
	op = m0_fop_data(fop);
	op->dtr_msg = opmsg;
	op->dtr_txr = *txr;
	*out = fop;
	return 0;
}
M0_EXPORTED(m0_dtm0_fop_create);

/*
  Allocates a fom.
 */
static int dtm0_fom_create(struct m0_fop *fop,
			   struct m0_fom **out,
			   struct m0_reqh *reqh)
{
	struct dtm0_fom         *fom;
	struct m0_fop           *repfop;
	struct dtm0_rep_fop     *reply;
	struct m0_dtm0_pmsg_ast *pma;

	M0_ENTRY("reqh=%p", reqh);

	M0_ALLOC_PTR(fom);
	M0_ALLOC_PTR(pma);
	repfop = m0_fop_reply_alloc(fop, &dtm0_rep_fop_fopt);

	if (fom == NULL || repfop == NULL || pma == NULL) {
		m0_free(pma);
		m0_free(repfop);
		m0_free(fom);
		return M0_ERR(-ENOMEM);
	}

	*out = &fom->dtf_fom;

	/* TODO: calculate credits for the operation. */

	/* see ::m0_dtm0_dtx_post_persistent for the details */
	fop->f_opaque = pma;
	reply = m0_fop_data(repfop);
	reply->dr_txr = (struct m0_dtm0_tx_desc) {};
	reply->dr_rc = 0;
	m0_fom_init(&fom->dtf_fom, &fop->f_type->ft_fom_type,
		    &dtm0_req_fom_ops, fop, repfop, reqh);
	return M0_RC_INFO(0, "fom=%p", &fom->dtf_fom);
}

static void dtm0_fom_fini(struct m0_fom *fom)
{
	M0_PRE(fom != NULL);

	m0_fom_fini(fom);
	m0_free(fom);
}

static size_t dtm0_fom_locality(const struct m0_fom *fom)
{
	static size_t locality = 0;

	M0_PRE(fom != NULL);
	return locality++;
}

static int m0_dtm0_send_msg(struct m0_fom                *fom,
			    enum m0_dtm0s_msg             msg_type,
			    const struct m0_fid          *tgt,
			    const struct m0_dtm0_tx_desc *txd)
{
	struct m0_fop          *fop;
	struct m0_rpc_session  *session;
	struct m0_rpc_item     *item;
	struct dtm0_req_fop    *req;
	uint64_t                phase_sm_id;
	uint64_t                rpc_sm_id;
	int                     rc;
	struct m0_dtm0_service *dtms = m0_dtm0_service_find(
					fom->fo_service->rs_reqh);

	M0_ENTRY("reqh=%p, target service " FID_F, dtms->dos_generic.rs_reqh,
		  FID_P(tgt));

	session = m0_dtm0_service_process_session_get(&dtms->dos_generic, tgt);
	M0_ASSERT(session != NULL);
	if (session == NULL)
		return M0_ERR(-ENOENT);

	fop = m0_fop_alloc_at(session, &dtm0_req_fop_fopt);
	if (fop == NULL)
		return M0_ERR(-ENOMEM);

	item              = &fop->f_item;
	item->ri_ops      = &dtm0_req_fop_rpc_item_ops;
	item->ri_session  = session;
	item->ri_prio     = M0_RPC_ITEM_PRIO_MID;
	item->ri_deadline = M0_TIME_IMMEDIATELY;

	req               = m0_fop_data(fop);
	req->dtr_msg      = msg_type;
	rc = m0_dtm0_tx_desc_copy(txd, &req->dtr_txr) ?: m0_rpc_post(item);
	if (rc == 0) {
		phase_sm_id = m0_sm_id_get(&fom->fo_sm_phase);
		rpc_sm_id   = m0_sm_id_get(&item->ri_sm);
		M0_ADDB2_ADD(M0_AVI_FOM_TO_TX, phase_sm_id, rpc_sm_id);

		m0_fop_put_lock(fop);

		M0_LOG(M0_DEBUG, "Sent %d msg " FID_F " -> " FID_F, msg_type,
		       FID_P(&dtms->dos_generic.rs_service_fid), FID_P(tgt));
	}
	return M0_RC(rc);
}

M0_INTERNAL int m0_dtm0_logrec_update(struct m0_be_dtm0_log  *log,
				      struct m0_be_tx        *tx,
				      struct m0_dtm0_tx_desc *txd,
				      struct m0_buf          *pyld)
{
	int rc;

	M0_ENTRY();

	m0_mutex_lock(&log->dl_lock);
	rc = m0_be_dtm0_log_update(log, tx, txd, pyld);
	m0_mutex_unlock(&log->dl_lock);

	return M0_RC(rc);
}

M0_INTERNAL int m0_dtm0_on_committed(struct m0_fom            *fom,
				     const struct m0_dtm0_tid *id)
{
	struct m0_dtm0_service *dtms = m0_dtm0_service_find(
					fom->fo_service->rs_reqh);
	struct m0_be_dtm0_log  *log = dtms->dos_log;
	struct m0_dtm0_log_rec *rec;
	struct m0_dtm0_tx_desc  txd;
	int                     rc;
	int                     i;

	M0_PRE(log != NULL);
	/*
	 * TODO: uncomment the following line after the connections code
	 * is updated to detect and set volatile and persistent dtm0 service
	 * instances. At present, the all2all test treats all connections as
	 * volatile.
	 * M0_PRE(log->dl_is_persistent);
	 */

	M0_ENTRY();

	m0_mutex_lock(&log->dl_lock);
	/* Get the latest state of the log record. */
	rec = m0_be_dtm0_log_find(log, id);
	M0_ASSERT_INFO(rec != NULL, "Log record must be inserted into the log "
		       "in cas_fom_tick().");
	m0_dtm0_tx_desc_copy(&rec->dlr_txd, &txd);
	m0_mutex_unlock(&log->dl_lock);

	/* Notify the originator */
	rc = m0_dtm0_send_msg(fom, DTM_PERSISTENT, &txd.dtd_id.dti_fid, &txd);

	if (rc != 0)
		M0_ERR_INFO(rc, "failed to send PERSISTENT msg to originator "
			    FID_F" -> " FID_F,
			    FID_P(&dtms->dos_generic.rs_service_fid),
			    FID_P(&txd.dtd_ps.dtp_pa->p_fid));
	/* Send P msgs to the rest of the participants. */
	for (i = 0; i < txd.dtd_ps.dtp_nr; ++i) {
		if (m0_fid_eq(&txd.dtd_ps.dtp_pa[i].p_fid,
			      &dtms->dos_generic.rs_service_fid))
			continue;
		rc = m0_dtm0_send_msg(fom, DTM_PERSISTENT,
				      &txd.dtd_ps.dtp_pa[i].p_fid, &txd);
		if (rc != 0)
			M0_ERR_INFO(rc, "failed to send PERSISTENT msg "
				    FID_F " -> " FID_F,
				    FID_P(&dtms->dos_generic.rs_service_fid),
			FID_P(&txd.dtd_ps.dtp_pa[i].p_fid));
	}
	m0_dtm0_tx_desc_fini(&txd);

	return M0_RC(rc);
}

static int dtm0_fom_tick(struct m0_fom *fom)
{
	int                       rc;
	int                       result = M0_FSO_AGAIN;
	struct   m0_dtm0_service *svc;
	struct   m0_buf           buf = {};
	struct   dtm0_rep_fop    *rep;
	struct   dtm0_req_fop    *req = m0_fop_data(fom->fo_fop);
	int                       phase = m0_fom_phase(fom);

	M0_ENTRY("fom %p phase %d", fom, phase);

	if (m0_fom_phase(fom) < M0_FOPH_NR) {
		result = m0_fom_tick_generic(fom);
		if (req->dtr_msg == DTM_PERSISTENT &&
		    m0_dtm0_is_a_persistent_dtm(fom->fo_service)) {
			if (m0_fom_phase(fom) == M0_FOPH_TXN_OPEN) {
				struct m0_be_tx_credit dtm0logrec_cred = {};
				M0_ASSERT(phase == M0_FOPH_TXN_INIT);
				/* get tx credits. */
				m0_be_dtm0_log_credit(M0_DTML_PERSISTENT,
						      &req->dtr_txr,
						      &buf,
						      m0_fom_reqh(fom)->rh_beseg,
						      NULL,
						      &dtm0logrec_cred);
				m0_be_tx_credit_add(&fom->fo_tx.tx_betx_cred, &dtm0logrec_cred);
			}
			/*
			 * XXX: Find out if we need to wait for the tx to be logged.
			 */
		}
	} else {
		M0_ASSERT(m0_fom_phase(fom) == M0_FOPH_TYPE_SPECIFIC);
		rep = m0_fop_data(fom->fo_rep_fop);
		M0_SET0(&rep->dr_txr);
		svc = m0_dtm0_service_find(fom->fo_service->rs_reqh);
		M0_ASSERT(svc != NULL);

		M0_LOG(M0_DEBUG, "Processing non-generic phase %" PRIu32
		       " %d, " FID_F ", %p", req->dtr_msg,
		       !!m0_dtm0_is_a_volatile_dtm(fom->fo_service),
		       FID_P(&fom->fo_service->rs_service_fid),
		       fom->fo_service->rs_reqh);

		if (m0_dtm0_in_ut() && req->dtr_msg == DMT_EXECUTE &&
		    m0_dtm0_is_a_persistent_dtm(fom->fo_service)) {
			rc = m0_dtm0_tx_desc_copy(&req->dtr_txr, &rep->dr_txr);
			M0_ASSERT(rc == 0);
			m0_fom_phase_move(fom, M0_ERR(rc), M0_FOPH_FAILURE);
			rc = m0_dtm0_send_msg(fom, DMT_EXECUTED,
					      &req->dtr_txr.dtd_id.dti_fid,
					      &req->dtr_txr);
			M0_ASSERT(rc == 0);
			m0_fom_phase_move(fom, M0_ERR(rc), M0_FOPH_FAILURE);
		} else if (req->dtr_msg == DTM_PERSISTENT) {
			if (m0_dtm0_is_a_volatile_dtm(fom->fo_service)) {
				m0_be_dtm0_log_pmsg_post(svc->dos_log, fom->fo_fop);
			} else {
				rc = m0_dtm0_logrec_update(svc->dos_log, &fom->fo_tx.tx_betx, &req->dtr_txr, &buf);
				if (rc == 0 && m0_dtm0_in_ut()) {
					rc = m0_dtm0_tx_desc_copy(&req->dtr_txr, &rep->dr_txr);
					M0_ASSERT(rc == 0);
					m0_fom_phase_move(fom, M0_ERR(rc), M0_FOPH_FAILURE);
					rc = m0_dtm0_send_msg(fom, DTM_PERSISTENT,
						&req->dtr_txr.dtd_id.dti_fid,
						&req->dtr_txr);
					M0_ASSERT(rc == 0);
					m0_fom_phase_move(fom, M0_ERR(rc), M0_FOPH_FAILURE);
				}
			}
		}
		rep->dr_rc = rc;
		m0_fom_phase_set(fom, M0_FOPH_SUCCESS);
		result = M0_FSO_AGAIN;
	}
	return M0_RC(result);
}

/*
 *  Local variables:
 *  c-indentation-style: "K&R"
 *  c-basic-offset: 8
 *  tab-width: 8
 *  fill-column: 80
 *  scroll-step: 1
 *  End:
 */
