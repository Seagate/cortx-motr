/* -*- C -*- */
/*
 * Copyright (c) 2012-2020 Seagate Technology LLC and/or its Affiliates
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


#include "lib/buf.h"
#include "lib/errno.h"
#include "lib/assert.h"
#include "lib/memory.h"
#include "lib/chan.h"
#include "lib/finject.h"
#include "lib/time.h"
#include "lib/trace.h"
#include "lib/misc.h"           /* M0_IN() */
#include "fop/fop.h"
#include "fop/fom.h"
#include "fop/fom_generic.h"

#include "lib/user_space/misc.h"
#include "rpc/rpc.h"
#include "rpc/rpclib.h"
#include "fop/fop_item_type.h"

#include "dtm0/fop.h"
#include "dtm0/fop_xc.h"
#include "rpc/rpc_opcodes.h"

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
	struct m0_fop *reply;

        M0_PRE(item != NULL);
	M0_PRE(M0_IN(m0_fop_opcode(m0_rpc_item_to_fop(item)),
		     (M0_DTM0_REQ_OPCODE)));

	if (m0_rpc_item_error(item) == 0) {
		reply = m0_rpc_item_to_fop(item->ri_reply);
		M0_ASSERT(M0_IN(m0_fop_opcode(reply), (M0_DTM0_REP_OPCODE)));
	}
}

void m0_dtm0_fop_fini(void)
{
	m0_fop_type_fini(&dtm0_req_fop_fopt);
	m0_fop_type_fini(&dtm0_rep_fop_fopt);
	m0_xc_dtm0_fop_fini();
}

int m0_dtm0_fop_init(void)
{
	m0_xc_dtm0_fop_init();
	M0_FOP_TYPE_INIT(&dtm0_req_fop_fopt,
			 .name      = "DTM0 request",
			 .opcode    = M0_DTM0_REQ_OPCODE,
			 .xt        = dtm0_op_xc,
			 .rpc_flags = M0_RPC_MUTABO_REQ,
			 .fom_ops   = &dtm0_req_fom_type_ops,
			 .sm        = &m0_generic_conf,
			 .svc_type  = &dtm0_service_type);
	M0_FOP_TYPE_INIT(&dtm0_rep_fop_fopt,
			 .name      = "DTM0 reply",
			 .opcode    = M0_DTM0_REP_OPCODE,
			 .xt        = dtm0_rep_op_xc,
			 .rpc_flags = M0_RPC_ITEM_TYPE_REPLY,
			 .fom_ops   = &dtm0_req_fom_type_ops);
	return 0;
}

static bool dtm0_service_started(struct m0_fop  *fop,
				struct m0_reqh *reqh)
{
	struct m0_reqh_service            *svc;
	const struct m0_reqh_service_type *stype;

	stype = fop->f_type->ft_fom_type.ft_rstype;
	M0_ASSERT(stype != NULL);

	svc = m0_reqh_service_find(stype, reqh);
	M0_ASSERT(svc != NULL);

	return m0_reqh_service_state_get(svc) == M0_RST_STARTED;
}
/*
  Allocates a fop.
 */

int m0_dtm0_fop_create(enum m0_dtm0s_opcode	      opcode,
			   enum m0_dtm0s_msg	      opmsg,
			   enum m0_dtm0s_op_flags     opflags,
			   struct m0_dtm0_txr	     *txr,
			   struct m0_fop            **out)
{
	struct dtm0_op  *op;
	struct m0_fop	     *fop;
	int                rc = 0;

	*out = NULL;

	M0_ALLOC_PTR(op);
	M0_ALLOC_PTR(fop);
	if (op == NULL || fop == NULL)
		rc = -ENOMEM;
	if (rc == 0) {
		op->dto_opcode = opcode;
		op->dto_opflags = opflags;
		op->dto_opmsg = opmsg;
		op->dto_txr = txr;
		m0_fop_init(fop, &dtm0_req_fop_fopt, op, &m0_fop_release);
		*out = fop;
	}
	if (rc != 0) {
		m0_free(op);
		m0_free(fop);
	}
	return rc;
}
M0_EXPORTED(m0_dtm0_fop_create);

static struct dtm0_op *dtm0_req_op(const struct m0_fom *fom)
{
	return m0_fop_data(fom->fo_fop);
}

/*
  Allocates a fom.
 */
static int dtm0_fom_create(struct m0_fop *fop,
			  struct m0_fom **out, struct m0_reqh *reqh)
{
	struct dtm0_fom    *fom;
	struct m0_fom     *fom0;
	struct m0_fop     *repfop;
	struct dtm0_rep_op *reply;

	if (!dtm0_service_started(fop, reqh))
		return M0_ERR(-EAGAIN);

	M0_ALLOC_PTR(fom);

	repfop = m0_fop_reply_alloc(fop, &dtm0_rep_fop_fopt);
	if (fom != NULL && repfop != NULL) {
		*out = fom0 = &fom->dtf_fom;
		/** TODO: calculate credits for the operation.
		 */

		reply = m0_fop_data(repfop);
		reply->dr_txr.dt_txr_payload = M0_BUF_INIT0;
		reply->dr_rc = 0;
		m0_fom_init(fom0, &fop->f_type->ft_fom_type,
			    &dtm0_req_fom_ops, fop, repfop, reqh);
		return M0_RC(0);
	} else {
		m0_free(repfop);
		m0_free(fom);
		return M0_ERR(-ENOMEM);
	}
}

static void dtm0_fom_fini(struct m0_fom *fom)
{
	M0_PRE(fom != NULL);

        m0_fom_fini(fom);
        m0_free(fom);
}

static size_t dtm0_fom_locality(const struct m0_fom *fom)
{
	static int locality = 0;
	M0_PRE(fom != NULL);
	return locality++;
}

static void dtm0_fom_cleanup(struct dtm0_fom *fom)
{
}

M0_INTERNAL void dtm0_req_reply_fini(struct m0_dtm0_req *req)
{

}

M0_INTERNAL void dtm0_req_op_free(struct m0_dtm0_req *req)
{

}

M0_INTERNAL void m0_dtm0_req_fini(struct m0_dtm0_req *req)
{
	dtm0_req_reply_fini(req);
	dtm0_req_op_free(req);
}

static int dtm0_fom_tick(struct m0_fom *fom0)
{
	int		    rc = M0_FSO_AGAIN;
	struct dtm0_op	   *req;
	struct dtm0_rep_op *rep;
	struct dtm0_fom	   *fom = M0_AMB(fom, fom0, dtf_fom);

	if (m0_fom_phase(fom0) < M0_FOPH_NR) {
		rc = m0_fom_tick_generic(fom0);
	} else {
		rep = m0_fop_data(fom0->fo_rep_fop);
		req = dtm0_req_op(fom0);
		if (req->dto_opcode != DT_REQ) {
			rep->dr_rc = EINVAL;
			dtm0_fom_cleanup(fom);
			m0_fom_phase_move(fom0, M0_ERR(EINVAL), M0_FOPH_FAILURE);
		} else {
			m0_buf_copy(&rep->dr_txr.dt_txr_payload, &req->dto_txr->dt_txr_payload);
			rep->dr_rc = 0;
			m0_fom_phase_set(fom0, M0_FOPH_SUCCESS);
		}
	}

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
