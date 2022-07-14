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

#include "dtm0/net.h"
#include "fop/fom.h"         /* m0_fom */
#include "fop/fom_generic.h" /* M0_FOPH_TYPE_SPECIFIC */
#include "fop/fop.h"         /* M0_FOP_TYPE_INIT */
#include "rpc/rpc_opcodes.h" /* M0_DTM0_NET_OPCODE */
#include "dtm0/service.h"    /* m0_dtm0_service_find */
#include "dtm0/drlink.h"     /* m0_dtm0_req_post */
#include "dtm0/fop.h"        /* dtm0_req_fop */
#include "lib/memory.h"      /* M0_ALLOC_PTR */

extern struct m0_reqh_service_type dtm0_service_type;
struct m0_fop_type dtm0_net_fop_fopt;

enum {
	M0_FOPH_NET_ENTRY = M0_FOPH_TYPE_SPECIFIC,
	M0_FOPH_NET_EXIT,
};

struct m0_sm_state_descr dtm0_net_phases[] = {
	[M0_FOPH_NET_ENTRY] = {
		.sd_name      = "net-entry",
		.sd_allowed   = M0_BITS(M0_FOPH_NET_EXIT,
					M0_FOPH_FAILURE)
	},
	[M0_FOPH_NET_EXIT] = {
		.sd_name      = "net-exit",
		.sd_allowed   = M0_BITS(M0_FOPH_SUCCESS),
	},
};

struct m0_sm_trans_descr dtm0_net_phases_trans[] = {
	[ARRAY_SIZE(m0_generic_phases_trans)] =
	{"net-entry-fail",     M0_FOPH_NET_ENTRY, M0_FOPH_FAILURE },
	{"net-entry-success",  M0_FOPH_NET_ENTRY, M0_FOPH_NET_EXIT},
	{"net-exit",           M0_FOPH_NET_EXIT,  M0_FOPH_SUCCESS },
};

static struct m0_sm_conf dtm0_net_sm_conf = {
	.scf_name      = "dtm0-net-fom",
	.scf_nr_states = ARRAY_SIZE(dtm0_net_phases),
	.scf_state     = dtm0_net_phases,
	.scf_trans_nr  = ARRAY_SIZE(dtm0_net_phases_trans),
	.scf_trans     = dtm0_net_phases_trans,
};

static int    dtm0_net_fom_create(struct m0_fop  *fop, struct m0_fom **out,
				  struct m0_reqh *reqh);
static void   dtm0_net_fom_fini(struct m0_fom *fom);
static size_t dtm0_net_fom_locality(const struct m0_fom *fom);
/* XXX: static */ int    dtm0_net_fom_tick(struct m0_fom *fom);

static const struct m0_fom_ops dtm0_pmsg_fom_ops = {
	.fo_fini          = dtm0_net_fom_fini,
	.fo_tick          = dtm0_net_fom_tick,
	.fo_home_locality = dtm0_net_fom_locality
};

static const struct m0_fom_type_ops dtm0_net_fom_type_ops = {
        .fto_create = dtm0_net_fom_create,
};

M0_INTERNAL int m0_dtm0_net_mod_init(void)
{
#if 0
	M0_PRE(!m0_sm_conf_is_initialized(&dtm0_net_sm_conf));

	m0_sm_conf_extend(m0_generic_conf.scf_state, dtm0_net_phases,
			  m0_generic_conf.scf_nr_states);
	m0_sm_conf_trans_extend(&m0_generic_conf, &dtm0_net_sm_conf);
	m0_sm_conf_init(&dtm0_net_sm_conf);

	M0_FOP_TYPE_INIT(&dtm0_net_fop_fopt,
			 .name      = "DTM0 net",
			 .opcode    = M0_DTM0_NET_OPCODE,
			 .xt        = m0_dtm0_msg_xc,
			 .rpc_flags = M0_RPC_ITEM_TYPE_REQUEST,
			 .fom_ops   = &dtm0_net_fom_type_ops,
			 .sm        = &dtm0_net_sm_conf,
			 .svc_type  = &dtm0_service_type);
#endif
	return 0;
}

M0_INTERNAL void m0_dtm0_net_mod_fini(void)
{
	M0_PRE(m0_sm_conf_is_initialized(&dtm0_net_sm_conf));
	m0_fop_type_fini(&dtm0_net_fop_fopt);
	m0_sm_conf_fini(&dtm0_net_sm_conf);
}

M0_INTERNAL int m0_dtm0_net_init(struct m0_dtm0_net     *dnet,
				 struct m0_dtm0_net_cfg *dnet_cfg)
{
	enum m0_dtm0_msg_type  type;
	int                    rc;
	struct m0_be_queue_cfg q_cfg = {
		.bqc_q_size_max = 10,
		.bqc_producers_nr_max = 1,
		.bqc_consumers_nr_max = 1,
		.bqc_item_length = sizeof(struct m0_dtm0_msg),
	};
	struct m0_dtm0_service *svc;

	M0_ENTRY();
	M0_PRE(M0_IS0(dnet));

	dnet->dnet_cfg = *dnet_cfg;

	for (type = 0; type < M0_DMT_NR; ++type) {
		rc = m0_be_queue_init(&dnet->dnet_input[type], &q_cfg);
		if (rc != 0)
			return M0_ERR(rc);
	}

	/*
	 * TODO: We temporary allow DTM0 net to be initialised
	 * even if DTM0 service is not up and running.
	 * It is needed to allow DTM0 net module to have control
	 * over the service in the next iterations of refactoring.
	 */
	if (dnet->dnet_cfg.dnc_reqh != NULL) {
		svc = m0_dtm0_service_find(dnet->dnet_cfg.dnc_reqh);
		if (svc != NULL)
			svc->dos_net = dnet;
		else
			M0_LOG(M0_WARN, "Transport is not bound with service.");
	} else
		M0_LOG(M0_WARN, "Transport is not bound with reqh.");

	return M0_RC(0);
}

static void dtm0_msg_fini(struct m0_dtm0_msg *mgs)
{
	/* TODO */
}

static void queue_finish(struct m0_be_queue *bq)
{
	struct m0_buf      item;
	struct m0_dtm0_msg msg = {};
	bool got = true;

	m0_be_queue_lock(bq);
	if (!bq->bq_the_end) {
		m0_be_queue_end(bq);
		while (got) {
			item = M0_BUF_INIT_PTR_CONST(&msg);
			M0_BE_OP_SYNC(op, m0_be_queue_get(bq, &op,
							  &item, &got));
			if (got)
				dtm0_msg_fini(&msg);
		}
	}
	M0_POST(bq->bq_the_end);
	m0_be_queue_unlock(bq);
}

static void dtm0_net_finish(struct m0_dtm0_net  *dnet)
{
	enum m0_dtm0_msg_type type;
	for (type = 0; type < M0_DMT_NR; ++type) {
		queue_finish(&dnet->dnet_input[type]);
	}
}

M0_INTERNAL void m0_dtm0_net_fini(struct m0_dtm0_net  *dnet)
{
	enum m0_dtm0_msg_type type;

	M0_ENTRY();
	M0_PRE(!M0_IS0(dnet));
	dtm0_net_finish(dnet);
	for (type = 0; type < M0_DMT_NR; ++type) {
		m0_be_queue_fini(&dnet->dnet_input[type]);
	}
	M0_LEAVE();
}

/* XXX */
static void dtm0_msg2req_fop(const struct m0_dtm0_msg *msg,
			     struct dtm0_req_fop      *req)
{
	*req = (struct dtm0_req_fop) {
		.dtr_msg = DTM_NET,
		.dtr_net_msg = *m0_dtm0_msg_dup(msg),
	};
}

static void dtm0_req_fop2msg(const struct dtm0_req_fop *req,
			     struct m0_dtm0_msg        *msg)
{
	*msg = *m0_dtm0_msg_dup(&req->dtr_net_msg);
}

M0_INTERNAL void m0_dtm0_net_send(struct m0_dtm0_net       *dnet,
				  struct m0_be_op          *op,
				  const struct m0_fid      *target,
				  const struct m0_dtm0_msg *msg,
				  const uint64_t           *parent_sm_id)
{
	struct m0_dtm0_service *svc;
	struct m0_reqh         *reqh;
	struct m0_fom           parent_fom = {
		.fo_sm_phase.sm_id = parent_sm_id == NULL ? 0 : *parent_sm_id,
	};
	struct dtm0_req_fop     req;

	dtm0_msg2req_fop(msg, &req);
	reqh = dnet->dnet_cfg.dnc_reqh;
	svc  = m0_dtm0_service_find(reqh);

	m0_dtm0_req_post(svc, op, &req, target, &parent_fom, true);
}

M0_INTERNAL void m0_dtm0_net_recv__post(struct m0_dtm0_net       *dnet,
					struct m0_be_op          *op,
					const struct m0_dtm0_msg *msg)
{
	struct m0_be_queue *q = &dnet->dnet_input[msg->dm_type];
	m0_be_queue_lock(q);
	M0_BE_QUEUE_PUT(q, op, msg);
	m0_be_queue_unlock(q);
}

M0_INTERNAL void m0_dtm0_net_recv(struct m0_dtm0_net       *dnet,
				  struct m0_be_op          *op,
				  bool                     *success,
				  struct m0_dtm0_msg       *msg,
				  enum m0_dtm0_msg_type     type)
{
	struct m0_be_queue *q = &dnet->dnet_input[type];
	m0_be_queue_lock(q);
	M0_BE_QUEUE_GET(q, op, msg, success);
	m0_be_queue_unlock(q);
}

M0_INTERNAL int m0_dtm0_msg_copy(struct m0_dtm0_msg *dst,
				 const struct m0_dtm0_msg *src)
{
	switch (src->dm_type) {
	case M0_DMT_EOL:
		*dst = *src;
		break;
	default:
		M0_IMPOSSIBLE("Not implemented yet");
	}

	return 0;
}

M0_INTERNAL struct m0_dtm0_msg *m0_dtm0_msg_dup(const struct m0_dtm0_msg *msg)
{
	int                 rc;
	struct m0_dtm0_msg *dup;

	M0_ALLOC_PTR(dup);
	if (dup != NULL) {
		rc = m0_dtm0_msg_copy(dup, msg);
		if (rc != 0) {
			M0_ASSERT(rc == 0);
			m0_free(dup);
			dup = NULL;
		}
	}
	return dup;
}


/* XXX: static */ int    dtm0_net_fom_tick(struct m0_fom *fom)
{
	int                     phase = m0_fom_phase(fom);
	int                     result = M0_FSO_AGAIN;
	struct dtm0_req_fop    *req = m0_fop_data(fom->fo_fop);
	struct m0_dtm0_service *svc;
	struct m0_dtm0_net     *dnet;
	struct m0_dtm0_msg      msg;

	M0_ENTRY("fom %p phase %d", fom, phase);

	switch (phase) {
	case M0_FOPH_INIT ... M0_FOPH_NR - 1:
		result = m0_fom_tick_generic(fom);
		break;
	case M0_FOPH_NET_ENTRY:
		svc = m0_dtm0_fom2service(fom);
		M0_ASSERT(svc != NULL);
		dnet = svc->dos_net;
		dtm0_req_fop2msg(req, &msg);
		M0_BE_OP_SYNC(op, m0_dtm0_net_recv__post(dnet, &op, &msg));
		m0_fom_phase_set(fom, M0_FOPH_NET_EXIT);
		break;
	case M0_FOPH_NET_EXIT:
		m0_fom_phase_set(fom, M0_FOPH_SUCCESS);
		break;
	default:
		M0_IMPOSSIBLE("Invalid phase");
	}

	return M0_RC(result);
}

static int    dtm0_net_fom_create(struct m0_fop  *fop, struct m0_fom **out,
				  struct m0_reqh *reqh)
{
	M0_ASSERT(0);
}

static void   dtm0_net_fom_fini(struct m0_fom *fom)
{
	M0_ASSERT(0);
}

static size_t dtm0_net_fom_locality(const struct m0_fom *fom)
{
	M0_ASSERT(0);
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
