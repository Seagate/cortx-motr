/* -*- C -*- */
/*
 * Copyright (c) 2011-2020 Seagate Technology LLC and/or its Affiliates
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
 * @addtogroup sm
 *
 * @{
 */

#define M0_TRACE_SUBSYSTEM M0_TRACE_SUBSYS_SM
#include "lib/trace.h"
#include "lib/chan.h"
#include "fop/fom.h"
#include "motr/magic.h"
#include "sm/op.h"

M0_TL_DESCR_DEFINE(smop, "smop", static, struct m0_sm_op, o_linkage, o_magix,
		   M0_SM_OP_MAGIC, M0_SM_OP_HEAD_MAGIC);
M0_TL_DEFINE(smop, static, struct m0_sm_op);

static bool op_invariant(const struct m0_sm_op *op)
{
	struct m0_sm_op *next = smop_tlink_is_in(op) ?
		smop_tlist_next(&op->o_ceo->oe_op, op) : NULL;

	return  m0_sm_invariant(&op->o_sm) &&
		_0C(op->o_subo != op) && /* No cycles. */
		/* All nested smops have the same executive. */
		_0C(ergo(op->o_subo != NULL, op_invariant(op->o_subo) &&
			 op->o_subo->o_ceo == op->o_ceo)) &&
		_0C(ergo(op->o_sm.sm_state == M0_SOS_DONE,
			 op->o_subo == NULL)) &&
		/* ceo->oe_op list matches op->o_subo list. */
		_0C(ergo(next != NULL, next == op->o_subo)) &&
		/* Next 2: return stack is valid. */
		_0C(m0_forall(i, ARRAY_SIZE(op->o_stack),
		      (op->o_stack[i] == -1 ||
		       op->o_stack[i] < op->o_sm.sm_conf->scf_nr_states))) &&
		_0C(m0_forall(i, ARRAY_SIZE(op->o_stack) - 1,
		      ergo(op->o_stack[i] == -1, op->o_stack[i + 1] == -1)));
}

static bool exec_invariant(const struct m0_sm_op_exec *ceo)
{
	return m0_tl_forall(smop, o, &ceo->oe_op,
			    op_invariant(o) && o->o_ceo == ceo &&
			    /* Only the innermost smop can be completed. */
			    ergo(o->o_sm.sm_state == M0_SOS_DONE,
				 smop_tlist_next(&ceo->oe_op, o) == NULL));
}

void m0_sm_op_init(struct m0_sm_op *op, int64_t (*tick)(struct m0_sm_op *),
		   struct m0_sm_op_exec *ceo, const struct m0_sm_conf *conf,
		   struct m0_sm_group *grp)
{
	int i;

	M0_PRE(M0_IS0(op));
	for (i = 0; i < ARRAY_SIZE(op->o_stack); ++i)
		op->o_stack[i] = -1;
	m0_sm_init(&op->o_sm, conf, M0_SOS_INIT, grp);
	op->o_ceo = ceo;
	op->o_tick = tick;
	smop_tlink_init(op);
	M0_POST(op_invariant(op));
}

void m0_sm_op_init_sub(struct m0_sm_op *op, int64_t (*tick)(struct m0_sm_op *),
		       struct m0_sm_op *super, const struct m0_sm_conf *conf)
{
	m0_sm_op_init(op, tick, super->o_ceo, conf, super->o_sm.sm_grp);
}

void m0_sm_op_fini(struct m0_sm_op *op)
{
	M0_PRE(op_invariant(op) && op->o_subo == NULL);
	smop_tlink_fini(op);
	m0_sm_fini(&op->o_sm);
	M0_SET0(op);
}

int m0_sm_op_subo(struct m0_sm_op *op, struct m0_sm_op *subo,
		  int state, bool finalise)
{
	M0_PRE(op_invariant(op) && op->o_subo == NULL);
	op->o_subo = subo;
	subo->o_finalise = finalise;
	return m0_sm_op_sub(op, M0_SOS_SUBO, state);
}

/**
 * Executes a smop as far as possible without blocking.
 *
 * Returns true iff there is more work: the smop is not done and has to wait
 * before further state transitions are possible.
 *
 * Returns false iff the smop reached M0_SOS_DONE state.
 */
bool m0_sm_op_tick(struct m0_sm_op *op)
{
	int64_t               result;
	bool                  wait;
	struct m0_sm_op_exec *ceo = op->o_ceo;

	M0_ASSERT(op_invariant(op));
	smop_tlist_add_tail(&ceo->oe_op, op);
	do {
		M0_ASSERT(op->o_sm.sm_state != M0_SOS_DONE);
		/*
		 * Internal M0_SOS_SUBO state is used to handle
		 * sub-operations.
		 */
		if (op->o_sm.sm_state == M0_SOS_SUBO) {
			M0_ASSERT(op->o_subo != NULL && op->o_stack[0] != -1);
			if (m0_sm_op_tick(op->o_subo)) {
				result = M0_SMOP_WAIT | M0_SOS_SUBO;
			} else {
				result = m0_sm_op_ret(op);
				if (op->o_subo->o_finalise)
					m0_sm_op_fini(op->o_subo);
				op->o_subo = NULL;
			}
		} else
			result = op->o_tick(op);
		if (result < 0) {
			op->o_sm.sm_rc = result;
			result = M0_SOS_DONE;
		} else if (result == M0_SMOP_SAME) {
			result = M0_SMOP_WAIT | op->o_sm.sm_state;
		} else if (result == (M0_SMOP_WAIT | M0_SOS_DONE)) {
			result = M0_SOS_DONE;
		}
		wait = (result & M0_SMOP_WAIT) != 0;
		M0_ASSERT(ergo(wait, ceo->oe_vec->eo_is_armed(ceo)));
		m0_sm_state_set(&op->o_sm, result & ~M0_SMOP_WAIT);
		M0_ASSERT(op_invariant(op));
		M0_ASSERT(op->o_ceo == ceo && exec_invariant(ceo));
	} while ((result != M0_SOS_DONE) && !wait);
	M0_ASSERT(op == smop_tlist_tail(&ceo->oe_op));
	smop_tlist_del(op);
	M0_ASSERT(op_invariant(op));
	return wait;
}

int64_t m0_sm_op_prep(struct m0_sm_op *op, int state, struct m0_chan *chan)
{
	M0_ASSERT(op_invariant(op));
	return op->o_ceo->oe_vec->eo_prep(op->o_ceo, op, chan) | state;
}

int m0_sm_op_sub(struct m0_sm_op *op, int state, int ret_state)
{
	int i;

	M0_ASSERT(op_invariant(op));
	for (i = 0; i < ARRAY_SIZE(op->o_stack); ++i) {
		if (op->o_stack[i] == -1)
			break;
	}
	M0_ASSERT(i < ARRAY_SIZE(op->o_stack));
	op->o_stack[i] = ret_state;
	return state;
}

int m0_sm_op_ret(struct m0_sm_op *op)
{
	int i;
	int state;

	M0_ASSERT(op_invariant(op));
	for (i = ARRAY_SIZE(op->o_stack) - 1; i >= 0 ; --i) {
		if (op->o_stack[i] != -1)
			break;
	}
	M0_ASSERT(i >= 0);
	state = op->o_stack[i];
	op->o_stack[i] = -1;
	return state;
}

void m0_sm_op_exec_init(struct m0_sm_op_exec *ceo)
{
	smop_tlist_init(&ceo->oe_op);
}

void m0_sm_op_exec_fini(struct m0_sm_op_exec *ceo)
{
	smop_tlist_fini(&ceo->oe_op);
}

static bool fom_exec_is_armed(struct m0_sm_op_exec *ceo)
{
	struct m0_fom_exec *fe = M0_AMB(fe, ceo, fe_ceo);

	return m0_fom_is_waiting_on(fe->fe_fom);
}

static int64_t fom_exec_prep(struct m0_sm_op_exec *ceo,
			     struct m0_sm_op *op, struct m0_chan *chan)
{
	struct m0_fom_exec *fe = M0_AMB(fe, ceo, fe_ceo);

	m0_fom_wait_on(fe->fe_fom, chan, &fe->fe_fom->fo_cb);
	return M0_SMOP_WAIT;
}

static const struct m0_sm_op_exec_ops fom_exec_ops = {
	.eo_is_armed = &fom_exec_is_armed,
	.eo_prep     = &fom_exec_prep
};

void m0_fom_exec_init(struct m0_fom_exec *fe, struct m0_fom *fom)
{
	m0_sm_op_exec_init(&fe->fe_ceo);
	fe->fe_ceo.oe_vec = &fom_exec_ops;
	fe->fe_fom = fom;
}

void m0_fom_exec_fini(struct m0_fom_exec *fe)
{
	m0_sm_op_exec_fini(&fe->fe_ceo);
}

static bool chan_exec_is_armed(struct m0_sm_op_exec *ceo)
{
	struct m0_chan_exec *ce = M0_AMB(ce, ceo, ce_ceo);

	return m0_clink_is_armed(&ce->ce_clink);
}

static int64_t chan_exec_prep(struct m0_sm_op_exec *ceo,
			     struct m0_sm_op *op, struct m0_chan *chan)
{
	struct m0_chan_exec *ce = M0_AMB(ce, ceo, ce_ceo);

	m0_clink_add(chan, &ce->ce_clink);
	return M0_SMOP_WAIT;
}

static const struct m0_sm_op_exec_ops chan_exec_ops = {
	.eo_is_armed = &chan_exec_is_armed,
	.eo_prep     = &chan_exec_prep
};

static bool chan_cb(struct m0_clink *link)
{
	struct m0_chan_exec *ce = M0_AMB(ce, link, ce_clink);

	(void)m0_sm_op_tick(ce->ce_top);
	return true;
}

void m0_chan_exec_init(struct m0_chan_exec *ce, struct m0_sm_op *top)
{
	m0_sm_op_exec_init(&ce->ce_ceo);
	ce->ce_ceo.oe_vec = &chan_exec_ops;
	m0_clink_init(&ce->ce_clink, top != NULL ? &chan_cb : NULL);
	ce->ce_clink.cl_is_oneshot = top != NULL;
	ce->ce_top = top;
}

void m0_chan_exec_fini(struct m0_chan_exec *ce)
{
	m0_clink_fini(&ce->ce_clink);
	m0_sm_op_exec_fini(&ce->ce_ceo);
}

static bool thread_exec_is_armed(struct m0_sm_op_exec *ceo)
{
	return false;
}

static int64_t thread_exec_prep(struct m0_sm_op_exec *ceo,
				struct m0_sm_op *op, struct m0_chan *chan)
{
	struct m0_thread_exec *te = M0_AMB(te, ceo, te_ceo);
	struct m0_clink        clink;

	m0_clink_init(&clink, NULL);
	m0_clink_add(chan, &clink);
	m0_mutex_unlock(chan->ch_guard);
	m0_chan_wait(&clink);
	m0_mutex_lock(chan->ch_guard);
	m0_clink_del(&clink);
	m0_clink_fini(&clink);
	return 0;
}

static const struct m0_sm_op_exec_ops thread_exec_ops = {
	.eo_is_armed = &thread_exec_is_armed,
	.eo_prep     = &thread_exec_prep
};

void m0_thread_exec_init(struct m0_thread_exec *te)
{
	m0_sm_op_exec_init(&te->te_ceo);
	te->te_ceo.oe_vec = &thread_exec_ops;
}

void m0_thread_exec_fini(struct m0_thread_exec *te)
{
	m0_sm_op_exec_fini(&te->te_ceo);
}

static bool ast_exec_is_armed(struct m0_sm_op_exec *ceo)
{
	struct m0_ast_exec *ae = M0_AMB(ae, ceo, ae_ceo);
	return ae->ae_armed;
}

static int64_t ast_exec_prep(struct m0_sm_op_exec *ceo,
			     struct m0_sm_op *op, struct m0_chan *chan)
{
	struct m0_ast_exec *ae = M0_AMB(ae, ceo, ae_ceo);

	M0_ASSERT(!ae->ae_armed);
	ae->ae_armed = true;
	m0_clink_add(chan, &ae->ae_clink);
	return M0_SMOP_WAIT;
}

static const struct m0_sm_op_exec_ops ast_exec_ops = {
	.eo_is_armed = &ast_exec_is_armed,
	.eo_prep     = &ast_exec_prep
};

static void ast_exec_cb(struct m0_sm_group *grp, struct m0_sm_ast *ast)
{
	struct m0_ast_exec *ae = M0_AMB(ae, ast, ae_ast);

	M0_ASSERT(smop_tlist_is_empty(&ae->ae_ceo.oe_op));
	M0_ASSERT(ae->ae_top->o_ceo == &ae->ae_ceo);
	M0_ASSERT(ae->ae_top->o_sm.sm_state != M0_SOS_DONE);
	M0_ASSERT(ae->ae_armed);

	ae->ae_armed = false;
	(void)m0_sm_op_tick(ae->ae_top);
}

static bool ast_exec_chan_cb(struct m0_clink *link)
{
	struct m0_ast_exec *ae = M0_AMB(ae, link, ae_clink);

	M0_ASSERT(ae->ae_armed);
	m0_sm_ast_post(ae->ae_grp, &ae->ae_ast);
	return true;
}

void m0_ast_exec_init(struct m0_ast_exec *ae, struct m0_sm_op *top,
		      struct m0_sm_group *grp)
{
	m0_sm_op_exec_init(&ae->ae_ceo);
	m0_clink_init(&ae->ae_clink, &ast_exec_chan_cb);
	ae->ae_clink.cl_is_oneshot = true;
	ae->ae_ceo.oe_vec = &ast_exec_ops;
	ae->ae_ast.sa_cb = &ast_exec_cb;
	ae->ae_grp = grp;
	ae->ae_top = top;
}

void m0_ast_exec_fini(struct m0_ast_exec *ae)
{
	m0_clink_fini(&ae->ae_clink);
	m0_sm_op_exec_fini(&ae->ae_ceo);
}

#undef M0_TRACE_SUBSYSTEM

/** @} end of sm group */

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
