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
#include "addb2/identifier.h"        /* M0_AVI_FOM_TO_TX */
#include "dtm0/fop.h"                /* dtm0_req_fop */
#include "dtm0/service.h"            /* m0_dtm0_service */
#include "dtm0/svc_internal.h"       /* dtm0_process */
#include "lib/coroutine.h"           /* m0_co API */
#include "lib/memory.h"              /* M0_ALLOC_PTR */
#include "reqh/reqh.h"               /* m0_reqh fields */
#include "rpc/rpc.h"                 /* m0_rpc_item_post */
#include "rpc/rpc_machine.h"         /* m0_rpc_machine */
#include "rpc/rpc_opcodes.h"         /* M0_DTM0_{RLINK,REQ}_OPCODE */

struct drlink_fom {
	struct m0_fom            df_gen;
	struct m0_co_context     df_co;
	struct m0_fid            df_tgt;
	struct m0_fop           *df_rfop;
	struct m0_dtm0_service  *df_svc;
	struct m0_co_op          df_co_op;
	bool                     df_wait_for_ack;
	uint64_t                 df_parent_sm_id;
};

static struct drlink_fom *fom2drlink_fom(struct m0_fom *fom)
{
	struct drlink_fom *df;
	M0_PRE(fom != NULL);
	return M0_AMB(df, fom, df_gen);
}

static size_t drlink_fom_locality(const struct m0_fom *fom)
{
	static size_t loc;
	/*
	 * At this moment, any locality can be the home locality
	 * for this kind of FOM.
	 */
	return loc++;
}

static void drlink_fom_fini(struct m0_fom *fom);
static int  drlink_fom_tick(struct m0_fom *fom);

static const struct m0_fom_ops drlink_fom_ops = {
	.fo_fini          = drlink_fom_fini,
	.fo_tick          = drlink_fom_tick,
	.fo_home_locality = drlink_fom_locality
};

static struct m0_fom_type drlink_fom_type;
static const struct m0_fom_type_ops drlink_fom_type_ops = {};
const static struct m0_sm_conf drlink_fom_conf;

M0_INTERNAL int m0_dtm0_rpc_link_mod_init(void)
{
	m0_fom_type_init(&drlink_fom_type,
			 M0_DTM0_RLINK_OPCODE,
			 &drlink_fom_type_ops,
			 &dtm0_service_type,
			 &drlink_fom_conf);
	return 0;
}

M0_INTERNAL void m0_dtm0_rpc_link_mod_fini(void)
{
}

/* Create a deep copy of the given request. */
static struct dtm0_req_fop *dtm0_req_fop_dup(const struct dtm0_req_fop *src)
{
	int                  rc;
	struct dtm0_req_fop *dst;

	M0_ALLOC_PTR(dst);
	if (dst == NULL)
		return NULL;

	rc = m0_dtm0_tx_desc_copy(&src->dtr_txr, &dst->dtr_txr);
	if (rc != 0) {
		M0_ASSERT(rc == -ENOMEM);
		m0_free(dst);
		return NULL;
	}

	dst->dtr_msg = src->dtr_msg;

	return dst;
}

static void dtm0_req_fop_fini(struct dtm0_req_fop *req)
{
	m0_dtm0_tx_desc_fini(&req->dtr_txr);
}

static int drlink_fom_init(struct drlink_fom            *fom,
			   struct m0_dtm0_service       *svc,
			   const struct m0_fid          *tgt,
			   const struct dtm0_req_fop    *req,
			   const struct m0_fom          *parent_fom,
			   bool                          wait_for_ack)
{
	struct m0_rpc_machine  *mach;
	struct m0_reqh         *reqh;
	struct dtm0_req_fop    *owned_req;
	struct m0_fop          *fop;

	M0_ENTRY();
	M0_PRE(fom != NULL);
	M0_PRE(svc != NULL);
	M0_PRE(req != NULL);
	M0_PRE(m0_fid_is_valid(tgt));

	reqh = svc->dos_generic.rs_reqh;
	mach = m0_reqh_rpc_mach_tlist_head(&reqh->rh_rpc_machines);

	owned_req = dtm0_req_fop_dup(req);
	if (owned_req == NULL)
		return M0_ERR(-ENOMEM);

	fop = m0_fop_alloc(&dtm0_req_fop_fopt, owned_req, mach);
	if (fop == NULL) {
		dtm0_req_fop_fini(owned_req);
		m0_free(owned_req);
		return M0_ERR(-ENOMEM);
	}

	/*
	 * When ACK is not required, the FOM may be released before
	 * the received callback is triggered.
	 * See ^1 in ::dtm0_rlink_rpc_item_reply_cb.
	 */
	fop->f_opaque = wait_for_ack ? fom : NULL;

	m0_fom_init(&fom->df_gen, &drlink_fom_type, &drlink_fom_ops,
		    NULL, NULL, reqh);

	/* TODO: can we use fom->fo_fop instead? */
	fom->df_rfop =  fop;
	fom->df_svc  =  svc;
	fom->df_tgt  = *tgt;
	fom->df_wait_for_ack = wait_for_ack;
	fom->df_parent_sm_id = m0_sm_id_get(&parent_fom->fo_sm_phase);

	m0_co_context_init(&fom->df_co);
	m0_co_op_init(&fom->df_co_op);

	M0_LEAVE();

	return M0_RC(0);
}

static void drlink_fom_fini(struct m0_fom *fom)
{
	struct drlink_fom *df = fom2drlink_fom(fom);
	m0_fop_put_lock(df->df_rfop);
	m0_co_op_fini(&df->df_co_op);
	m0_fom_fini(fom);
	m0_free(fom);
}

static void co_long_write_lock(struct m0_co_context     *context,
			       struct m0_long_lock      *lk,
			       struct m0_long_lock_link *link,
			       int                       next_phase)
{
	int outcome;
	M0_CO_REENTER(context);
	outcome = M0_FOM_LONG_LOCK_RETURN(m0_long_write_lock(lk, link,
							     next_phase));
	M0_CO_YIELD_RC(context, outcome);
}

static void co_rpc_link_connect(struct m0_co_context *context,
				struct m0_rpc_link   *rlink,
				struct m0_fom        *fom,
				int                   next_phase)
{
	M0_CO_REENTER(context);

	m0_chan_lock(&rlink->rlk_wait);
	m0_fom_wait_on(fom, &rlink->rlk_wait, &fom->fo_cb);
	m0_chan_unlock(&rlink->rlk_wait);

	m0_rpc_link_connect_async(rlink, M0_TIME_NEVER, NULL, NULL);
	m0_fom_phase_set(fom, next_phase);

	M0_CO_YIELD_RC(context, M0_FSO_WAIT);
}

static int find_or_add(struct m0_dtm0_service *dtms,
		       const struct m0_fid    *tgt,
		       struct dtm0_process   **out)
{
	struct dtm0_process *process;
	int                  rc = 0;

	M0_ENTRY();
	M0_PRE(m0_mutex_is_locked(&dtms->dos_generic.rs_mutex));

	process = dtm0_service_process__lookup(&dtms->dos_generic, tgt);
	if (process == NULL) {
		rc = M0_ALLOC_PTR(process) == NULL ?
			M0_ERR(-ENOMEM) :
			dtm0_process_init(process, dtms, tgt);
		if (rc != 0)
			m0_free(process);
	}

	if (rc == 0) {
		M0_POST(process != NULL);
		*out = process;
	}
	return M0_RC(rc);
}

enum drlink_fom_state {
	DRF_INIT = M0_FOM_PHASE_INIT,
	DRF_DONE = M0_FOM_PHASE_FINISH,
	DRF_LOCKING = M0_FOM_PHASE_NR,
	DRF_CONNECTING,
	DRF_SENDING,
	DRF_WAITING_FOR_REPLY,
	DRF_FAILED,
	DRF_NR,
};

static struct m0_sm_state_descr drlink_fom_states[] = {
	/* terminal states */
	[DRF_INIT] = {
		.sd_name      = "DRF_INIT",
		.sd_allowed   = M0_BITS(DRF_LOCKING, DRF_FAILED),
		.sd_flags     = M0_SDF_INITIAL,
	},
	[DRF_DONE] = {
		.sd_name      = "DRF_DONE",
		.sd_allowed   = 0,
		.sd_flags     = M0_SDF_TERMINAL,
	},

	/* failure states */
	[DRF_FAILED] = {
		.sd_name      = "DRF_FAILED",
		.sd_allowed   = M0_BITS(DRF_DONE),
		.sd_flags     = M0_SDF_FAILURE,
	},

	/* intermediate states */
#define _ST(name, allowed)            \
	[name] = {                    \
		.sd_name    = #name,  \
		.sd_allowed = allowed \
	}
	_ST(DRF_LOCKING,           M0_BITS(DRF_CONNECTING,
					   DRF_SENDING,
					   DRF_FAILED)),
	_ST(DRF_CONNECTING,        M0_BITS(DRF_SENDING,
					   DRF_FAILED)),
	_ST(DRF_SENDING,           M0_BITS(DRF_DONE,
					   DRF_WAITING_FOR_REPLY,
					   DRF_FAILED)),
	_ST(DRF_WAITING_FOR_REPLY, M0_BITS(DRF_DONE,
					   DRF_FAILED)),
#undef _ST
};

const static struct m0_sm_conf drlink_fom_conf = {
	.scf_name      = "m0_dtm0_drlink_fom",
	.scf_nr_states = ARRAY_SIZE(drlink_fom_states),
	.scf_state     = drlink_fom_states,
};

static struct drlink_fom *item2drlink_fom(struct m0_rpc_item *item)
{
	return m0_rpc_item_to_fop(item)->f_opaque;
}

static void dtm0_rlink_rpc_item_reply_cb(struct m0_rpc_item *item)
{
	struct m0_fop *reply = NULL;
	struct drlink_fom *df = item2drlink_fom(item);

	M0_ENTRY("item=%p", item);

	M0_PRE(item != NULL);
	M0_PRE(m0_fop_opcode(m0_rpc_item_to_fop(item)) == M0_DTM0_REQ_OPCODE);

	if (m0_rpc_item_error(item) == 0) {
		reply = m0_rpc_item_to_fop(item->ri_reply);
		M0_ASSERT(m0_fop_opcode(reply) == M0_DTM0_REP_OPCODE);
	}

	/* ^1: df is NULL if we do not need to wait for the reply. */
	if (df != NULL)
		m0_co_op_done(&df->df_co_op);

	M0_LEAVE("reply=%p", reply);
}

const struct m0_rpc_item_ops dtm0_req_fop_rlink_rpc_item_ops = {
        .rio_replied = dtm0_rlink_rpc_item_reply_cb,
};

static int dtm0_process_rlink_reinit(struct dtm0_process *proc,
				     struct drlink_fom   *df)
{
	struct m0_rpc_machine *mach = df->df_rfop->f_item.ri_rmachine;
	const int max_in_flight = DTM0_MAX_RPCS_IN_FLIGHT;

	if (!M0_IS0(&proc->dop_rlink)) {
		m0_rpc_link_fini(&proc->dop_rlink);
		M0_SET0(&proc->dop_rlink);
	}

	return m0_rpc_link_init(&proc->dop_rlink, mach, &proc->dop_rserv_fid,
				proc->dop_rep, max_in_flight);
}

static int dtm0_process_rlink_send(struct dtm0_process *proc,
				   struct drlink_fom   *drf)
{
	struct m0_fop          *fop = drf->df_rfop;
	struct m0_rpc_session  *session = &proc->dop_rlink.rlk_sess;
	struct m0_rpc_item     *item = &fop->f_item;

	item->ri_ops      = &dtm0_req_fop_rlink_rpc_item_ops;
	item->ri_session  = session;
	item->ri_prio     = M0_RPC_ITEM_PRIO_MID;
	item->ri_deadline = M0_TIME_IMMEDIATELY;

	if (drf->df_wait_for_ack)
		m0_co_op_active(&drf->df_co_op);

	return m0_rpc_post(item);
}

/** An aggregated status of a dtm0_process:dop_rlink */
enum dpr_state {
	/** Link is not alive but we can resurrect it. */
	DPR_TRANSIENT,
	/** Link is alive and ready to transfer items. */
	DPR_ONLINE,
	/** Link is permanently dead. */
	DPR_FAILED,
};

static enum dpr_state dpr_state_infer(struct dtm0_process *proc)
{
	/*
	 * TODO:
	 * Observe the states of the following enitities:
	 *	RPC connection
	 *	RPC session
	 *	Conf obj
	 * and then decide whether it is alive, dead or permanently dead.
	 *
	 * @verbatim
	 *	if (conf_obj is ONLINE) {
	 *		if (conn is ACTIVE && session is in (IDLE, BUSY))
	 *			return ONLINE;
	 *		else
	 *			return TRANSIENT;
	 *	} else
	 *		return FAILED;
	 * @endverbatim
	 */
	if (m0_rpc_link_is_connected(&proc->dop_rlink))
		return DPR_ONLINE;

	return DPR_TRANSIENT;
}

/*
 * Establish a relation between a DTM message (carried by an RPC item),
 * and a DTM RPC link FOM that was used to send this message:
 *   DRLINK FOM <-> RPC item.
 */
static void drlink_addb_drf2item_relate(struct drlink_fom *drf)
{
	const struct m0_rpc_item *item = &drf->df_rfop->f_item;
	M0_ADDB2_ADD(M0_AVI_FOM_TO_TX, m0_sm_id_get(&drf->df_gen.fo_sm_phase),
		     m0_sm_id_get(&item->ri_sm));
}

/*
 * Establish a relation between a FOM (for example, a CAS PUT FOM).
 * and a DTM RPC link FOM that was created to send a DTM message:
 *   FOM <-> DRLINK FOM.
 */
static void drlink_addb_drf2parent_relate(struct drlink_fom *drf)
{
	M0_ADDB2_ADD(M0_AVI_FOM_TO_TX, drf->df_parent_sm_id,
		     m0_sm_id_get(&drf->df_gen.fo_sm_phase));
}

#define F M0_CO_FRAME_DATA
static void drlink_coro_fom_tick(struct m0_co_context *context)
{
	int                 rc   = 0;
	struct drlink_fom  *drf  = M0_AMB(drf, context, df_co);
	struct m0_fom      *fom  = &drf->df_gen;

	M0_CO_REENTER(context,
		      struct m0_long_lock_link   llink;
		      struct m0_long_lock_addb2  llock_addb2;
		      struct dtm0_process       *proc;
		      );

	drlink_addb_drf2parent_relate(drf);

	m0_mutex_lock(&drf->df_svc->dos_generic.rs_mutex);
	rc = find_or_add(drf->df_svc, &drf->df_tgt, &F(proc));
	/*
	 * Safety: it is safe to release the lock right here because
	 * ::dtm0_service_conns_term is supposed to be called only
	 * after all drlink FOMs have reached DRF_DONE state.
	 * There are no other places where processes get removed from the list.
	 */
	m0_mutex_unlock(&drf->df_svc->dos_generic.rs_mutex);

	if (rc != 0)
		goto out;

	m0_long_lock_link_init(&F(llink), fom, &F(llock_addb2));

	M0_CO_FUN(context, co_long_write_lock(context,
					      &F(proc)->dop_llock,
					      &F(llink),
					      DRF_LOCKING));
	M0_ASSERT(m0_long_is_write_locked(&F(proc)->dop_llock, fom));

	if (dpr_state_infer(F(proc)) == DPR_TRANSIENT) {
		rc = dtm0_process_rlink_reinit(F(proc), drf);
		if (rc != 0)
			goto unlock;
		M0_CO_FUN(context, co_rpc_link_connect(context,
						       &F(proc)->dop_rlink,
						       fom, DRF_CONNECTING));
	}

	if (dpr_state_infer(F(proc)) == DPR_FAILED)
			goto unlock;

	M0_ASSERT(dpr_state_infer(F(proc)) == DPR_ONLINE);
	m0_fom_phase_set(fom, DRF_SENDING);
	rc = dtm0_process_rlink_send(F(proc), drf);
	if (rc != 0)
		goto unlock;

	/* Safety: FOP (and item) can be released only in ::drlink_fom_fini. */
	drlink_addb_drf2item_relate(drf);

	if (drf->df_wait_for_ack) {
		M0_CO_YIELD_RC(context,
			       m0_co_op_tick_ret(&drf->df_co_op, fom,
						 DRF_WAITING_FOR_REPLY));
		m0_co_op_reset(&drf->df_co_op);
		rc = m0_rpc_item_error(&drf->df_rfop->f_item);
	}

unlock:
	m0_long_write_unlock(&F(proc)->dop_llock, &F(llink));
	m0_long_lock_link_fini(&F(llink));
out:
	if (rc != 0)
		m0_fom_phase_move(fom, rc, DRF_FAILED);

	m0_fom_phase_set(fom, DRF_DONE);
}
#undef F

static int drlink_fom_tick(struct m0_fom *fom)
{
	struct m0_co_context *co = &fom2drlink_fom(fom)->df_co;
	M0_CO_START(co);
	drlink_coro_fom_tick(co);
	return M0_CO_END(co) ?: M0_FSO_WAIT;
}

M0_INTERNAL int m0_dtm0_req_post(struct m0_dtm0_service    *svc,
				 const struct dtm0_req_fop *req,
				 const struct m0_fid       *tgt,
				 const struct m0_fom       *parent_fom,
				 bool                       wait_for_ack)
{
	int                  rc;
	struct drlink_fom   *fom;

	M0_ENTRY();

	rc = M0_ALLOC_PTR(fom) == NULL ?
		M0_ERR(-ENOMEM) :
		drlink_fom_init(fom, svc, tgt, req, parent_fom, wait_for_ack);

	if (rc == 0)
		m0_fom_queue(&fom->df_gen);
	else
		m0_free(fom);

	return M0_RC(rc);
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
