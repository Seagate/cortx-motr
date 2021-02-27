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

#pragma once

#ifndef __MOTR___SM_OP_H__
#define __MOTR___SM_OP_H__

/**
 * @defgroup sm
 *
 * Overview
 * --------
 *
 * State machine operation (m0_sm_op, "smop") is a state machine (m0_sm)
 * suitable for implementation of non-blocking activities involving
 * sub-activities.
 *
 * smop module provides the following facilities on top of m0_sm:
 *
 *     - support for sub-operations: a smop can start a sub-operation, which
 *       itself is a smop. The sub-operation makes state transitions, blocks,
 *       starts further sub-operations, etc.---all this complexity is hidden
 *       from the parent operation;
 *
 *     - sub-routines. Often, a state machine has a sequence of state
 *       transitions that can be initiated from multiple states, for example,
 *       some resource cleanup activity. When this sequence ends, the state
 *       should return to the state depending on the initiating state. smop
 *       module provides a simple stack mechanism (m0_sm_op::o_stack[]) to
 *       implement call-return pairs (m0_sm_op_sub(), m0_sm_op_ret());
 *
 *     - "operation executive" (m0_sm_op_exec) is an abstraction that hides
 *       different ways of scheduling smop blocking and state transition
 *       execution. The module provides multiple implementations:
 *
 *           * thread executive: a smop is attached to a thread and executed
 *             there by actually blocking the thread when wait is needed;
 *
 *           * fom executive: a smop is attached to a fom. When the smop is
 *             about to block, it arranges for the fom to be woken up as
 *             necessary and unwinds the stack. When the fom is woken up, smop
 *             execution continues from the last point;
 *
 *           * ast executive: a smop is associated with a locality. smop state
 *             transitions are executed in asts (m0_sm_ast) posted to the
 *             locality;
 *
 *           * chan executive: smop state transitions are executed directly in
 *             the callbacks fired when a channel (m0_chan) is signalled.
 *
 *       Note, that the same smop can be executed by all the different
 *       executives.
 *
 * Sub-operations
 * --------------
 *
 * Consider an operation foo that has to execute an sub-operation bar.
 *
 * @code
 * // Structure representing execution of foo.
 * struct foo_op {
 *         // Generic smop for foo.
 *         struct m0_sm_op f_op;
 *         // Sub-operation bar.
 *         struct bar_op   f_bar;
 *         ... // other foo data and other sub-operations follow
 * };
 *
 * // Structure representing execution of bar.
 * struct bar_op {
 *         // Generic smop for bar.
 *         struct m0_sm_op b_op;
 *         ...
 * };
 * @endcode
 *
 * To implement foo, the user provides m0_sm_op::o_tick() method that looks like
 * the following (see sm/ut/sm.c:pc_tick() for a real-life-ish example):
 *
 * @code
 * static int64_t pc_tick(struct m0_sm_op *smop)
 * {
 *         struct foo_op *op  = M0_AMB(op, smop, foo_op);
 *         struct bar_op *bar = &op->f_bar;
 *
 *         switch (smop->o_sm.sm_state) {
 *         ...
 *         case DO_BAR:
 *                 ...
 *                 m0_sm_op_init_sub(&bar->b_op, &bar_tick, smop, &bar_conf);
 *                 return m0_sm_op_subo(smop, &bar->b_op, DONE_BAR, true);
 *         case DONE_BAR:
 *                 // bar completed execution, reached M0_SOS_DONE state
 *                 // and was finalised.
 *                 ...
 *         }
 *         ...
 * }
 * @endcode
 *
 * bar may pass though multiple states, block and resume before reaching
 * M0_SOS_DONE. It may invoke sub-operations of its own that also may experience
 * multiple state transitions, block, invoke sub-operations and so on. All this
 * complexity is hidden from foo, for which the entire execution of bar looks
 * like a simple single state transition.
 *
 * What happens behind the scene is that every smop has an additional hidden
 * state M0_SOS_SUBO. m0_sm_op_subo() transitions to this hidden state and
 * processing of the sub-operation is done by the generic smop code in
 * m0_sm_tick(). The stack of nested smop executions is recorded in the list
 * hanging off of the smop executive (m0_sm_op_exec::oe_op) and maintained by
 * m0_sm_tick().
 *
 * Typically, calls to m0_sm_op_init_sub() and m0_sm_op_subo() are wrapped in
 * some bar_do() function that initialises the bar_op appropriately.
 *
 * Sub-routines
 * ------------
 *
 * Smop module provides a very simple call-return mechanism:
 *
 *     - m0_sm_op_sub(): jump to the new state and push return state on the
 *       stack (m0_sm_op::o_stack);
 *
 *     - m0_sm_op_ret(): pop the last pushed state from the stack and jump to
 *       it.
 *
 * Executives
 * ----------
 *
 * An executive (m0_sm_op_exec) provides the mechanism that arranges execution
 * of smop state transitions, blocking and resumption.
 *
 * When the smop currently executing its state transition wants to block and to
 * be resumed later it calls m0_sm_op_prep(). This function tells the executive
 * which channel will be signalled when the smop can be resumed.
 *
 * Different executives do different things at this moment.
 *
 *     - thread executive simply waits on the channel and then returns. smop
 *       execution continues linearly, without unwinding the stack;
 *
 *     - fom, ast and chan executives arrange for a certain call-back to be
 *       executed when the channel is signalled. This call-back will wake up the
 *       fom, post an ast or execute the next state transition respectively. In
 *       the meantime, m0_sm_op_prep() returns the next state combined with
 *       M0_SMOP_WAIT flag. When this result is returned from
 *       m0_sm_op::o_tick(), the flag causes m0_sm_op_tick() to return,
 *       unwinding the stack to the top.
 *
 *       When the call-back is invoked, it calls m0_sm_op_tick() against the
 *       top-level smop, which calls to the nested sub-operation and so on,
 *       rewinding the stack back. Once the stack is restored, the innermost
 *       nested sub-operation continues with its state transitions.
 *
 * @{
 */

#include "lib/tlist.h"
#include "lib/chan.h"
#include "sm/sm.h"

struct m0_fom;

struct m0_sm_op;
struct m0_sm_op_ops;
struct m0_sm_op_exec;

/**
 * Pre-defined smop states.
 *
 * User-defined states should follow M0_SOS_NR.
 */
enum m0_sm_op_state {
	/** Initial state in which smop is created. */
	M0_SOS_INIT,
	/**
	 * Sub-operation state. This is an internal state used by
	 * m0_sm_op_tick() to handle sub-operation execution.
	 */
	M0_SOS_SUBO,
	/** Final state. When this state is reached, the smop is done. */
	M0_SOS_DONE,
	M0_SOS_NR
};

enum {
	/** Depth of call-return stack. @see m0_sm_op_sub(), m0_sm_op_ret(). */
	M0_SMOP_STACK_LEN = 4,
	M0_SMOP_WAIT_BIT  = 50,
	M0_SMOP_SAME_BIT,
	/** Bitflag signalling that smop has to wait. */
	M0_SMOP_WAIT = M0_BITS(M0_SMOP_WAIT_BIT),
	/** Bitflag signalling that smop remains in the same state. */
	M0_SMOP_SAME = M0_BITS(M0_SMOP_SAME_BIT)
};

/** State machine operation, smop. */
struct m0_sm_op {
	uint64_t              o_magix;
	struct m0_sm          o_sm;
	/** The executive for this operation. */
	struct m0_sm_op_exec *o_ceo;
	/**
	 * Linkage to the list of active nested smop executions hanging off of
	 * m0_sm_op_exec::oe_op.
	 */
	struct m0_tlink       o_linkage;
	/** If non-NULL, the pointer to the active sub-operation. */
	struct m0_sm_op      *o_subo;
	/** Call-return stack. @see m0_sm_op_sub(), m0_sm_op_ret(). */
	int                   o_stack[M0_SMOP_STACK_LEN];
	/**
	 * If true, m0_sm_op_tick() will finalise the operation when it reaches
	 * M0_SOS_DONE.
	 */
	bool                  o_finalise;
	/**
	 * Tick function.
	 *
	 * Performs non-blocking state transition.
	 *
	 * Returns next state optionally combined with M0_SMOP_WAIT.  If
	 * M0_SMOP_WAIT is set, the smop has to wait before the next state
	 * transition and the wakeup has been arranged.
	 *
	 * Instead of the next state, M0_SMOP_SAME can be returned to
	 * to indicate that the smop remain in the same state.
	 *
	 * Explicit return of M0_SMOP_WAIT is never needed. This flag is set by
	 * m0_sm_op_prep() if needed.
	 */
	int64_t             (*o_tick)(struct m0_sm_op *);
};

void     m0_sm_op_init(struct m0_sm_op *op, int64_t (*tick)(struct m0_sm_op *),
		       struct m0_sm_op_exec *ceo,
		       const struct m0_sm_conf *conf, struct m0_sm_group *grp);
void     m0_sm_op_fini(struct m0_sm_op *op);
bool     m0_sm_op_tick(struct m0_sm_op *op);
int64_t  m0_sm_op_prep(struct m0_sm_op *op, int state, struct m0_chan *chan);
int      m0_sm_op_sub (struct m0_sm_op *op, int state, int ret_state);
int      m0_sm_op_ret (struct m0_sm_op *op);
int      m0_sm_op_subo(struct m0_sm_op *op, struct m0_sm_op *subo, int state,
		       bool finalise);

void m0_sm_op_init_sub(struct m0_sm_op *op, int64_t (*tick)(struct m0_sm_op *),
		       struct m0_sm_op *super, const struct m0_sm_conf *conf);

struct m0_sm_op_exec_ops {
	bool    (*eo_is_armed)(struct m0_sm_op_exec *ceo);
	int64_t (*eo_prep)    (struct m0_sm_op_exec *ceo,
			       struct m0_sm_op *op, struct m0_chan *chan);
};

struct m0_sm_op_exec {
	struct m0_tl                    oe_op;
	const struct m0_sm_op_exec_ops *oe_vec;
};

void m0_sm_op_exec_init(struct m0_sm_op_exec *ceo);
void m0_sm_op_exec_fini(struct m0_sm_op_exec *ceo);

struct m0_fom_exec {
	struct m0_sm_op_exec  fe_ceo;
	struct m0_fom        *fe_fom;
};

void m0_fom_exec_init(struct m0_fom_exec *fe, struct m0_fom *fom);
void m0_fom_exec_fini(struct m0_fom_exec *fe);

struct m0_chan_exec {
	struct m0_sm_op_exec ce_ceo;
	struct m0_clink      ce_clink;
	struct m0_sm_op     *ce_top;
};

void m0_chan_exec_init(struct m0_chan_exec *ce, struct m0_sm_op *top);
void m0_chan_exec_fini(struct m0_chan_exec *ce);

struct m0_thread_exec {
	struct m0_sm_op_exec te_ceo;
};

void m0_thread_exec_init(struct m0_thread_exec *te);
void m0_thread_exec_fini(struct m0_thread_exec *te);

struct m0_ast_exec {
	struct m0_sm_op_exec ae_ceo;
	struct m0_sm_ast     ae_ast;
	struct m0_clink      ae_clink;
	struct m0_sm_group  *ae_grp;
	struct m0_sm_op     *ae_top;
	bool                 ae_armed;
};

void m0_ast_exec_init(struct m0_ast_exec *ae, struct m0_sm_op *top,
		      struct m0_sm_group *grp);
void m0_ast_exec_fini(struct m0_ast_exec *ae);

/** @} end of sm group */
#endif /* __MOTR___SM_OP_H__ */

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
