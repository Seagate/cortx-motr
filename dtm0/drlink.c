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
#include "dtm0/fop_xc.h"             /* dtm0_req_fop_xc */
#include "dtm0/service.h"            /* m0_dtm0_service */
#include "dtm0/svc_internal.h"       /* dtm0_process */
#include "lib/coroutine.h"           /* m0_co API */
#include "lib/memory.h"              /* M0_ALLOC_PTR */
#include "reqh/reqh.h"               /* m0_reqh fields */
#include "rpc/rpc.h"                 /* m0_rpc_item_post */
#include "rpc/rpc_machine.h"         /* m0_rpc_machine */
#include "rpc/rpc_opcodes.h"         /* M0_DTM0_{RLINK,REQ}_OPCODE */
#include "lib/string.h"              /* m0_streq */

enum {
	/*
	 * TODO: DTM model assumes infinite timeouts. But this is too scary at
	 * the moment, we cannot yet rely that infinite timeout will work
	 * without issues in connect/disconnect case. These timeouts will be
	 * adjusted/reworked when we work more on stabilising the DTM.
	 */
	DRLINK_CONNECT_TIMEOUT_SEC = 1,
	DRLINK_DISCONN_TIMEOUT_SEC = DRLINK_CONNECT_TIMEOUT_SEC,
};

struct drlink_fom {
	struct m0_fom            df_gen;
	struct m0_co_context     df_co;
	struct m0_be_op         *df_op;
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
static const struct m0_fom_type_ops drlink_fom_type_ops = {
	.fto_create = NULL
};
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

/** Create a deep copy of the given request. */
static struct dtm0_req_fop *dtm0_req_fop_dup(const struct dtm0_req_fop *src)
{
	int                 rc = 0;
	struct m0_xcode_obj src_obj;
	struct m0_xcode_obj dest_obj;
	struct m0_xcode_ctx sctx;
	struct m0_xcode_ctx dctx;
	struct dtm0_req_fop *dest = NULL;

	/* There is no const version of xcode objects, we'll have to cast it. */
	src_obj = M0_XCODE_OBJ(dtm0_req_fop_xc, (struct dtm0_req_fop *)src);
	dest_obj = M0_XCODE_OBJ(dtm0_req_fop_xc, NULL);
	m0_xcode_ctx_init(&sctx, &src_obj);
	m0_xcode_ctx_init(&dctx, &dest_obj);
	dctx.xcx_alloc = m0_xcode_alloc;
	rc = m0_xcode_dup(&dctx, &sctx);
	if (rc == 0)
		dest = dctx.xcx_it.xcu_stack[0].s_obj.xo_ptr;
	return dest;
}

static void dtm0_req_fop_fini(struct dtm0_req_fop *req)
{
	m0_dtm0_tx_desc_fini(&req->dtr_txr);
	m0_buf_free(&req->dtr_payload);
}

static int drlink_fom_init(struct drlink_fom            *fom,
			   struct m0_dtm0_service       *svc,
			   struct m0_be_op              *op,
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

	fop = m0_fop_alloc(req->dtr_msg == DTM_REDO ?
			   &dtm0_redo_fop_fopt:
			   &dtm0_req_fop_fopt,
			   owned_req, mach);
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
	fom->df_op   = op;
	fom->df_tgt  = *tgt;
	fom->df_wait_for_ack = wait_for_ack;
	fom->df_parent_sm_id = m0_sm_id_get(&parent_fom->fo_sm_phase);

	m0_co_context_init(&fom->df_co);
	m0_co_op_init(&fom->df_co_op);

	if (op != NULL)
		m0_be_op_active(op);

	return M0_RC(0);
}

static void drlink_fom_fini(struct m0_fom *fom)
{
	struct drlink_fom *df = fom2drlink_fom(fom);
	if (df->df_rfop != NULL)
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
	DRF_DISCONNECTING,
	DRF_CONNECTING,
	DRF_SENDING,
	DRF_WAITING_FOR_REPLY,
	DRF_FAILED,
	DRF_NR,
};

static struct m0_sm_state_descr drlink_fom_states[] = {
	[DRF_INIT] = {
		.sd_name      = "DRF_INIT",
		.sd_allowed   = M0_BITS(DRF_LOCKING, DRF_FAILED),
		.sd_flags     = M0_SDF_INITIAL,
	},
	/* terminal states */
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
	_ST(DRF_LOCKING,           M0_BITS(DRF_DISCONNECTING,
					   DRF_CONNECTING,
					   DRF_SENDING,
					   DRF_FAILED)),
	_ST(DRF_DISCONNECTING,        M0_BITS(DRF_CONNECTING,
					      DRF_FAILED)),
	_ST(DRF_CONNECTING,        M0_BITS(DRF_SENDING,
					   DRF_CONNECTING,
					   DRF_DISCONNECTING,
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
	uint32_t req_opcode = m0_fop_opcode(m0_rpc_item_to_fop(item));

	M0_ENTRY("item=%p", item);

	M0_PRE(item != NULL);
	M0_PRE(M0_IN(req_opcode, (M0_DTM0_REQ_OPCODE,
				  M0_DTM0_REDO_OPCODE)));

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
		m0_rpc_conn_sessions_cancel(&proc->dop_rlink.rlk_conn);
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

	M0_ENTRY("remote: proc=" FID_F ", ep=%s",
		 FID_P(&proc->dop_rproc_fid), proc->dop_rep);

	item->ri_ops      = &dtm0_req_fop_rlink_rpc_item_ops;
	item->ri_session  = session;
	item->ri_prio     = M0_RPC_ITEM_PRIO_MID;
	item->ri_deadline = M0_TIME_IMMEDIATELY;

	if (drf->df_wait_for_ack)
		m0_co_op_active(&drf->df_co_op);

	return M0_RC(m0_rpc_post(item));
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

/**
 * Connect-disconnect context for an rpc link.
 * The context tightens together clink-based notification from rpc link
 * channel and a co_op.
 */
struct rlink_cd_ctx {
	struct m0_clink dc_clink;
	struct m0_co_op dc_op;
};

static bool rlink_cd_cb(struct m0_clink *clink)
{
	struct rlink_cd_ctx *dc = M0_AMB(dc, clink, dc_clink);
	m0_co_op_done(&dc->dc_op);
	return false;
}

static void co_rlink_do(struct m0_co_context *context,
			struct dtm0_process  *proc,
			enum drlink_fom_state what)
{
	struct drlink_fom  *drf  = M0_AMB(drf, context, df_co);
	struct m0_fom      *fom  = &drf->df_gen;
	int                 timeout_sec = 0;
	void              (*action)(struct m0_rpc_link *, m0_time_t,
				    struct m0_clink *) = NULL;

	M0_CO_REENTER(context, struct rlink_cd_ctx dc;);

	M0_SET0(&F(dc));
	m0_clink_init(&F(dc).dc_clink, rlink_cd_cb);
	F(dc).dc_clink.cl_flags |= M0_CF_ONESHOT;
	m0_co_op_init(&F(dc).dc_op);
	m0_co_op_active(&F(dc).dc_op);

	switch (what) {
	case DRF_CONNECTING:
		timeout_sec = DRLINK_CONNECT_TIMEOUT_SEC;
		action = m0_rpc_link_connect_async;
		break;
	case DRF_DISCONNECTING:
		m0_rpc_conn_sessions_cancel(&proc->dop_rlink.rlk_conn);
		timeout_sec = DRLINK_DISCONN_TIMEOUT_SEC;
		action = m0_rpc_link_disconnect_async;
		break;
	default:
		M0_IMPOSSIBLE("%d is something that cannot be done", what);
		break;
	}

	action(&proc->dop_rlink, m0_time_from_now(timeout_sec, 0),
	       &F(dc).dc_clink);

	M0_CO_YIELD_RC(context, m0_co_op_tick_ret(&F(dc).dc_op, fom, what));
	m0_chan_wait(&F(dc).dc_clink);

	m0_co_op_fini(&F(dc).dc_op);
	m0_clink_fini(&F(dc).dc_clink);
}

static bool has_volatile_param(struct m0_conf_obj     *obj)
{
	struct m0_conf_service *svc;
	const char            **param;

	svc = M0_CONF_CAST(obj, m0_conf_service);
	M0_ASSERT(svc->cs_params != NULL);

	for (param = svc->cs_params; *param != NULL; ++param) {
		if (m0_streq(*param, "origin:in-volatile"))
			return true;
		else if (m0_streq(*param, "origin:in-persistent"))
			return false;
	}

	M0_IMPOSSIBLE("Service origin is not defined in the config?");
}

static void drlink_coro_fom_tick(struct m0_co_context *context)
{
	int                 rc     = 0;
	struct drlink_fom  *drf    = M0_AMB(drf, context, df_co);
	struct m0_fom      *fom    = &drf->df_gen;
	const char         *reason = "Unknown";
	struct m0_conf_obj *obj    = NULL;
	struct m0_confc    *confc  = m0_reqh2confc(m0_fom2reqh(fom));
	bool                always_reconnect = false;

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

	if (rc != 0) {
		reason = "Cannot find-or-add remote process";
		goto out;
	}

	m0_long_lock_link_init(&F(llink), fom, &F(llock_addb2));

	M0_CO_FUN(context, co_long_write_lock(context,
					      &F(proc)->dop_llock,
					      &F(llink),
					      DRF_LOCKING));
	M0_ASSERT(m0_long_is_write_locked(&F(proc)->dop_llock, fom));

	m0_conf_cache_lock(&confc->cc_cache);
	obj = m0_conf_cache_lookup(&confc->cc_cache, &F(proc)->dop_rserv_fid);
	M0_ASSERT(obj != NULL);
	if (has_volatile_param(obj) && obj->co_ha_state != M0_NC_ONLINE) {
		M0_LOG(M0_DEBUG, "Force state transition %s -> %s for " FID_F,
		       m0_ha_state2str(obj->co_ha_state),
		       m0_ha_state2str(obj->co_ha_state),
		       FID_P(&F(proc)->dop_rserv_fid));
		obj->co_ha_state = M0_NC_ONLINE;
		m0_chan_broadcast(&obj->co_ha_chan);
		always_reconnect = true;
	}
	m0_conf_cache_unlock(&confc->cc_cache);

	/* Reconnect if the session was canceled */
	if (m0_rpc_link_is_connected(&F(proc)->dop_rlink) &&
	    F(proc)->dop_rlink.rlk_sess.s_cancelled) {
		always_reconnect = true;
	}

	/* XXX:
	 * At this moment we cannot detect client falures.
	 * Because of that, we cannot detect the case where
	 * the client drops RPC items because it cannot
	 * find the corresponding connection.
	 * As a workaround, we force drlink to re-connect
	 * whenever it tries to send a message.
	 */

	if (always_reconnect) {
		if (m0_rpc_link_is_connected(&F(proc)->dop_rlink)) {
			M0_CO_FUN(context, co_rlink_do(context, F(proc),
						       DRF_DISCONNECTING));
		}

		rc = dtm0_process_rlink_reinit(F(proc), drf);
		if (rc != 0) {
			reason = "Cannot reinit RPC link.";
			goto unlock;
		}
		/*
		 * TODO handle network failure after link is connected, but
		 * before the message is successfully sent
		 */
		M0_CO_FUN(context, co_rlink_do(context, F(proc),
					       DRF_CONNECTING));
	} else {
		if (!m0_rpc_link_is_connected(&F(proc)->dop_rlink)) {
			rc = dtm0_process_rlink_reinit(F(proc), drf);
			if (rc != 0) {
				reason = "Cannot reinit RPC link.";
				goto unlock;
			}
			M0_CO_FUN(context, co_rlink_do(context, F(proc),
						       DRF_CONNECTING));
		}
	}

	M0_ASSERT(ergo(m0_rpc_link_is_connected(&F(proc)->dop_rlink),
		       !F(proc)->dop_rlink.rlk_sess.s_cancelled));

	m0_fom_phase_set(fom, DRF_SENDING);
	rc = dtm0_process_rlink_send(F(proc), drf);
	if (rc != 0) {
		reason = "Failed to post a message to RPC layer.";
		goto unlock;
	}

	/* Safety: FOP (and item) can be released only in ::drlink_fom_fini. */
	drlink_addb_drf2item_relate(drf);

	if (drf->df_wait_for_ack) {
		M0_CO_YIELD_RC(context,
			       m0_co_op_tick_ret(&drf->df_co_op, fom,
						 DRF_WAITING_FOR_REPLY));
		m0_co_op_reset(&drf->df_co_op);
		rc = m0_rpc_item_error(&drf->df_rfop->f_item);
		if (rc != 0) {
			reason = "Rpc item error";
			goto unlock;
		}
	}

unlock:
	if (drf->df_rfop != NULL) {
		m0_fop_put_lock(drf->df_rfop);
		drf->df_rfop = NULL;
	}

	m0_long_write_unlock(&F(proc)->dop_llock, &F(llink));
	m0_long_lock_link_fini(&F(llink));
out:
	/* TODO handle the error */
	if (rc != 0) {
		M0_LOG(M0_ERROR, "Failed to send a message to" FID_F ", rc=%d, "
		       "reason=%s", FID_P(&drf->df_tgt), rc, reason);
		m0_fom_phase_move(fom, rc, DRF_FAILED);
	}
	if (drf->df_op != NULL)
		m0_be_op_done(drf->df_op);

	m0_fom_phase_set(fom, DRF_DONE);
}
#undef F

static int drlink_fom_tick(struct m0_fom *fom)
{
	int                   rc;
	struct m0_co_context *co = &fom2drlink_fom(fom)->df_co;

	M0_ENTRY("fom=%p", fom);

	M0_CO_START(co);
	drlink_coro_fom_tick(co);
	rc = M0_CO_END(co);

	M0_POST(M0_IN(rc, (0, M0_FSO_AGAIN, M0_FSO_WAIT)));
	/*
	 * Zero means the coroutine has nothing more to do.
	 * Replace it with FSO_WAIT as per the convention (see fom_exec).
	 */
	rc = rc ?: M0_FSO_WAIT;
	return M0_RC(rc);
}

M0_INTERNAL int m0_dtm0_req_post(struct m0_dtm0_service    *svc,
                                 struct m0_be_op           *op,
				 const struct dtm0_req_fop *req,
				 const struct m0_fid       *tgt,
				 const struct m0_fom       *parent_fom,
				 bool                       wait_for_ack)
{
	int                  rc;
	struct drlink_fom   *fom;

	M0_ENTRY();

	M0_ALLOC_PTR(fom);
	if (fom == NULL)
		return M0_ERR(-ENOMEM);

	rc = drlink_fom_init(fom, svc, op, tgt, req, parent_fom, wait_for_ack);

	if (rc == 0)
		m0_fom_queue(&fom->df_gen);
	else
		m0_free(fom);

	return rc == 0 ? M0_RC(rc) : M0_ERR(rc);
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
