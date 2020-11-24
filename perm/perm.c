/* -*- C -*- */
/*
 * Copyright (c) 2020 Seagate Technology LLC and/or its Affiliates
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

#define M0_TRACE_SUBSYTSEM M0_TRACE_SUBSYS_OTHER
#include "lib/trace.h"

#include <ucontext.h>
#include <err.h>
#include <sysexits.h>
#include <stdio.h>

#include "lib/getopts.h"
#include "lib/memory.h"
#include "lib/tlist.h"
#include "lib/types.h"
#include "lib/memory.h"
#include "motr/init.h"
#include "module/instance.h"
#include "lib/uuid.h"                  /* m0_node_uuid_string_set */

/**
 * @addtogroup perm
 * @{
 */

struct pin;
struct perm;
struct step;
struct event;
struct proc;
enum event_op;
enum ha_state;

enum pstate {
	P_CRASHED,
	P_STARTING,
	P_STARTED,

	P_NR
};

enum event_op {
	E_NONE,
	E_CRASH,
	E_START,
	E_SEND,
	E_RECV,
	E_HASTATE,
	E_TXSTATE,
	E_TXSET,
	E_CONT,
	E_REQLOST,
	E_REPLOST,

	E_NR
};

enum msg_type {
	M_NONE,
	M_REQ,
	M_REPLY,
	M_1WAY,

	M_NR
};

enum msg_op {
	O_NONE,
	O_HASET,
	O_USER,

	O_NR
};

enum ha_state {
	H_ONLINE,
	H_TRANSIENT,
	H_PERMANENT
};

struct proc {
	uint64_t        pr_magix;
	struct m0_tl    pr_hist;
	struct perm    *pr_perm;
	int             pr_idx;
	int             pr_size;
	void          (*pr_print)(const struct proc *p, const void *data);
	void          (*pr_pfree)(const struct proc *p, const void *data);
	struct m0_tlink pr_linkage;
};

struct perm {
	struct m0_tl p_step;
	struct m0_tl p_proc;
	struct m0_tl p_invariant;
	struct m0_tl p_cb;
	struct proc *p_fatum;
};

struct step {
	uint64_t        s_magix;
	uint64_t        s_nr;
	struct perm    *s_perm;
	struct m0_tl    s_event;
	struct pin     *s_cur;
	struct m0_tl    s_proc;
	struct m0_tlink s_linkage;
};

struct event {
	uint64_t        e_magix;
	uint64_t        e_seq;
	struct m0_tl    e_hist;
	struct perm    *e_perm;
	enum event_op   e_op;
	struct proc    *e_proc;
	struct proc    *e_src;
	uint64_t        e_sel0;
	uint64_t        e_sel1;
	uint64_t        e_p0;
	uint64_t        e_p1;
	struct event   *e_alternative;
	enum pstate     e_pstate;
	int             e_bcount;
	bool          (*e_enabled)(const struct event *e);
	void          (*e_print)(const struct event *e);
};

struct pin {
	uint64_t        n_vmagix;
	void           *n_vhead;
	struct step    *n_hhead;
	void           *n_data;
	struct m0_tlink n_vlinkage;
	struct m0_tlink n_hlinkage;
	uint64_t        n_hmagix;
};

struct invariant {
	uint64_t          i_magix;
	struct m0_tlink   i_linkage;
	bool            (*i_check)(const struct step *s);
};

enum cb_type {
	C_SIMPLE,
	C_THREAD
};

struct cb {
	uint64_t         c_magix;
	enum cb_type     c_type;
	struct m0_tlink  c_linkage;
	struct proc     *c_proc;
	enum event_op    c_op;
	uint64_t         c_sel0;
	uint64_t         c_sel1;
	bool           (*c_check)(const struct cb *cb, const struct event *e);
	void           (*c_invoke)(struct cb *cb, struct event *e, void *data);
};

enum tx_state {
	T_NONE,
	T_OPEN,
	T_CLOSED,
	T_LOGGED,
	T_COMMITTED
};

enum { KV_MAX = 16 };

struct kv {
	uint64_t      k_txid;
	enum tx_state k_txstate;
	uint64_t      k_key;
	uint64_t      k_val;
	void         *k_buf;
};

struct sys_proc_state {
	enum pstate   ss_pstate;
	enum ha_state ss_hastate;
	int           ss_bcount;
	uint64_t      ss_txid_last;
	int           ss_kvnr;
	struct kv     ss_kv[KV_MAX];
};

enum {
	M0_PERM_PIN_V_MAGIC = 0x3377,
	M0_PERM_PIN_VHEAD_MAGIC,
	M0_PERM_PIN_H_MAGIC,
	M0_PERM_PIN_HHEAD_MAGIC,
	M0_PERM_STEP_MAGIC,
	M0_PERM_STEP_HEAD_MAGIC,
	M0_PERM_INV_MAGIC,
	M0_PERM_INV_HEAD_MAGIC,
	M0_PERM_CB_MAGIC,
	M0_PERM_CB_HEAD_MAGIC,
	M0_PERM_PROC_MAGIC,
	M0_PERM_PROC_HEAD_MAGIC
};

M0_TL_DESCR_DEFINE(v, "pin-vertical", static, struct pin, n_vlinkage,
		   n_vmagix, M0_PERM_PIN_V_MAGIC, M0_PERM_PIN_VHEAD_MAGIC);
M0_TL_DEFINE(v, static, struct pin);

M0_TL_DESCR_DEFINE(h, "pin-horizontal", static, struct pin, n_hlinkage,
		   n_hmagix, M0_PERM_PIN_H_MAGIC, M0_PERM_PIN_HHEAD_MAGIC);
M0_TL_DEFINE(h, static, struct pin);

M0_TL_DESCR_DEFINE(s, "39-steps", static, struct step, s_linkage,
		   s_magix, M0_PERM_STEP_MAGIC, M0_PERM_STEP_HEAD_MAGIC);
M0_TL_DEFINE(s, static, struct step);

M0_TL_DESCR_DEFINE(i, "invariant", static, struct invariant, i_linkage,
		   i_magix, M0_PERM_INV_MAGIC, M0_PERM_INV_HEAD_MAGIC);
M0_TL_DEFINE(i, static, struct invariant);

M0_TL_DESCR_DEFINE(cb, "call-back", static, struct cb, c_linkage,
		   c_magix, M0_PERM_CB_MAGIC, M0_PERM_CB_HEAD_MAGIC);
M0_TL_DEFINE(cb, static, struct cb);

M0_TL_DESCR_DEFINE(p, "proc", static, struct proc, pr_linkage,
		   pr_magix, M0_PERM_PROC_MAGIC, M0_PERM_PROC_HEAD_MAGIC);
M0_TL_DEFINE(p, static, struct proc);

static void perm_init(struct perm *p);
static void perm_fini(struct perm *p);

static struct proc *proc_add(struct perm *p, int size,
			     void (*pfree)(const struct proc *p,
					   const void *data));
static struct cb *cb_add(struct perm *p, struct proc *proc);
static struct invariant *invariant_add(struct perm *p);

static struct step *step_add(struct step *s, struct event *e);
static struct step *step_alloc(struct perm *p, struct step *prev);
static void happens(struct step *step, struct event *e);
static void step_fix(struct step *s);
static void step_fini(struct step *step);
static void post(struct perm *p, struct event *e);

static void perm_prep(struct perm *p);
static void perm_run(struct perm *p);

static bool matches(const struct event *e, const struct cb *cb);
static void invoke(struct step *step, struct event *e, struct cb *cb);

static bool enabled(const struct event *e);
static bool is_pending(const struct perm *perm, struct event *e);

static struct pin *pin(struct step *step, bool event, void *head);

static struct event *event_alloc(struct perm *p, enum event_op eop,
				 struct proc *proc, struct proc *src,
				 uint64_t sel0, uint64_t sel1,
				 uint64_t p0, uint64_t p1, enum pstate pstate,
				 int bcount);
static struct event *event_post(struct perm *p, enum event_op eop,
				struct proc *proc, struct proc *src,
				uint64_t sel0, uint64_t sel1,
				uint64_t p0, uint64_t p1, enum pstate pstate,
				int bcount);
static void event_fini(struct event *e);
static void *palloc(struct proc *proc);
static void *pget(struct proc *proc);

static void cont(struct proc *proc,
		 void (*invoke)(struct cb *cb, struct event *e,
				void *data)) M0_UNUSED;

static void *xalloc(size_t nob);

static bool perm_invariant(const struct perm *p);
static bool step_invariant(const struct step *s);
static bool proc_invariant(const struct proc *pr);
static bool event_invariant(const struct event *e);
static bool pin_invariant(const struct pin *pin, bool event);

static void step_print(const struct step *s);
static void event_print(const struct event *e);

static void sys_crash(struct cb *c, struct event *e, void *d);
static void sys_start(struct cb *c, struct event *e, void *d);
static void sys_prep(struct perm *p);
static void sys_proc_init(struct proc *proc);
static void sys_pfree(struct proc *proc, struct sys_proc_state *ss);
static void *sys_pinit(struct proc *proc, struct sys_proc_state *ss);
static struct sys_proc_state *sys_pget(struct proc *proc);

static uint64_t      tx_open (struct proc *proc) M0_UNUSED;
static void          tx_set  (struct proc *proc, uint64_t tid,
			      uint64_t key, uint64_t val, void *buf) M0_UNUSED;
static void          tx_close(struct proc *proc, uint64_t tid) M0_UNUSED;
static enum tx_state tx_state(struct proc *proc, uint64_t txid) M0_UNUSED;
static void          tx_get  (struct proc *proc,
			      uint64_t key, uint64_t *val, void **bf) M0_UNUSED;
static bool tx_logged_enabled(const struct event *e);
static void tx_logged(struct cb *c, struct event *e, void *d);

static void req(struct proc *src, struct proc *dst, enum msg_op mop,
		uint64_t p0, uint64_t p1);
static struct event *lossless(struct proc *src, struct proc *dst,
			      enum msg_op mop, uint64_t p0, uint64_t p1);
#if 0
static void send(struct proc *src, struct proc *dst, enum msg_type mt,
		 enum msg_op mop, uint64_t p0, uint64_t p1, enum pstate pstate);
#endif
static enum ha_state thinks(struct proc *subject, struct proc *object);

static struct kv *kv_get(struct proc *proc);

static int crash_nr = 3;
static int hastate_nr = 3;
static int print_depth = 1000000;

static bool perm_invariant(const struct perm *p)
{
	return  m0_tl_forall(s, step, &p->p_step,
			     _0C(step->s_perm == p) &&
			     _0C(step_invariant(step))) &&
		m0_tl_forall(p, proc, &p->p_proc,
			     _0C(proc->pr_perm == p) &&
			     _0C(proc_invariant(proc)));
}

static bool step_invariant(const struct step *s)
{
	return  m0_tl_forall(h, pin, &s->s_event,
			     _0C(pin->n_hhead == s) &&
			     _0C(event_invariant(pin->n_vhead))) &&
		m0_tl_forall(h, pin, &s->s_proc,
			     _0C(pin->n_hhead == s) &&
			     _0C(proc_invariant(pin->n_vhead)));
}

static bool proc_invariant(const struct proc *pr)
{
	return _0C(m0_tl_forall(v, pin, &pr->pr_hist, pin_invariant(pin,
								    false)));
}

static bool event_invariant(const struct event *e)
{
	return  _0C(m0_tl_forall(v, pin, &e->e_hist, pin_invariant(pin,
								   true))) &&
		_0C(E_NONE < e->e_op && e->e_op < E_NR) &&
		_0C(ergo(M0_IN(e->e_op, (E_SEND, E_RECV)),
			 M_NONE < e->e_sel0 && e->e_sel0 < M_NR &&
			 O_NONE < e->e_sel1 && e->e_sel1 < O_NR &&
			 e->e_proc != NULL && e->e_src != NULL)) &&
		_0C(ergo(e->e_op == E_CONT, e->e_sel0 != 0));
}

static bool pin_invariant(const struct pin *pin, bool event)
{
	struct step *s    = pin->n_hhead;
	struct perm *p    = s->s_perm;
	struct step *prev = s_tlist_prev(&p->p_step, s);

	return  _0C(p == s->s_perm) &&
		_0C(ergo(event, s->s_nr == (prev == NULL ? 0 :
					    prev->s_nr + 1))) &&
		_0C(ergo(!event && prev != NULL, s->s_nr > prev->s_nr));
}

static void perm_init(struct perm *p)
{
	s_tlist_init(&p->p_step);
	p_tlist_init(&p->p_proc);
	i_tlist_init(&p->p_invariant);
	cb_tlist_init(&p->p_cb);
}

static void perm_fini(struct perm *p)
{
}

static void perm_run(struct perm *p)
{
	uint64_t     nr   = 0;
	struct step *last = s_tlist_tail(&p->p_step);

	last->s_cur = h_tlist_head(&last->s_event);
	while (true) {
		struct step      *next;
		struct invariant *inv;

		last = s_tlist_tail(&p->p_step);
		M0_ASSERT(perm_invariant(p));
		M0_ASSERT(last->s_cur != NULL);
		if (last->s_nr < print_depth) {
			printf("%11"PRId64" ", nr);
			step_print(last);
		}
		nr++;
		next = step_add(last, last->s_cur->n_vhead);
		happens(next, last->s_cur->n_vhead);
		step_fix(next);
		m0_tl_for(i, &next->s_perm->p_invariant, inv) {
			inv->i_check(next);
		} m0_tl_endfor;
		last = next;
		while (last->s_cur == NULL) {
			step_fini(last);
			if (s_tlist_is_empty(&p->p_step))
				return;
			last = s_tlist_tail(&p->p_step);
			last->s_cur = h_tlist_next(&last->s_event, last->s_cur);
			step_fix(last);
		}
	}
}

static void happens(struct step *step, struct event *e)
{
	struct cb *cb;

	m0_tl_for(cb, &step->s_perm->p_cb, cb) {
		if (matches(e, cb))
			invoke(step, e, cb);
	} m0_tl_endfor;
}

static bool matches(const struct event *e, const struct cb *cb)
{
	return  ergo(cb->c_proc  != NULL,   cb->c_proc == e->e_proc) &&
		ergo(cb->c_op    != E_NONE, cb->c_op   == e->e_op) &&
		ergo(cb->c_sel0  != 0,      cb->c_sel0 == e->e_sel0) &&
		ergo(cb->c_sel1  != 0,      cb->c_sel1 == e->e_sel1) &&
		ergo(cb->c_check != NULL,   cb->c_check(cb, e));
}

static void *pget(struct proc *proc)
{
	struct pin *last = v_tlist_tail(&proc->pr_hist);
	M0_ASSERT(last != NULL && last->n_data != NULL);
	return last->n_data;
}

static void *palloc(struct proc *proc)
{
	struct pin  *last = v_tlist_tail(&proc->pr_hist);
	struct step *step = s_tlist_tail(&proc->pr_perm->p_step);

	if (last == NULL || last->n_hhead != step) {
		struct pin *n = pin(step, false, proc);

		n->n_data = xalloc(proc->pr_size);
		return sys_pinit(proc, memcpy(n->n_data, (last ?: n)->n_data,
					      proc->pr_size));
	} else
		return last->n_data;
}

static void invoke(struct step *step, struct event *e, struct cb *cb)
{
	M0_ASSERT(cb->c_type == C_SIMPLE);
	cb->c_invoke(cb, e, palloc(cb->c_proc));
}

static bool is_pending(const struct perm *p, struct event *e)
{
	return v_tlist_tail(&e->e_hist)->n_hhead == s_tlist_tail(&p->p_step);
}

static bool enabled(const struct event *e)
{
	struct sys_proc_state *ss = pget(e->e_proc);
	return  ergo(e->e_alternative != NULL,
		     is_pending(e->e_perm, e->e_alternative)) &&
		ss->ss_pstate >= e->e_pstate &&
		ergo(e->e_bcount >= 0, e->e_bcount == ss->ss_bcount) &&
		ergo(e->e_enabled != NULL, e->e_enabled(e));
}

static struct step *step_alloc(struct perm *p, struct step *prev)
{
	struct step *step = xalloc(sizeof *step);

	h_tlist_init(&step->s_event);
	h_tlist_init(&step->s_proc);
	step->s_nr = prev != NULL ? prev->s_nr + 1 : 0;
	s_tlink_init_at_tail(step, &p->p_step);
	step->s_perm = p;
	return step;
}

static void perm_prep(struct perm *p)
{
	struct proc *proc;

	step_alloc(p, NULL);
	m0_tl_for(p, &p->p_proc, proc) {
		palloc(proc);
	} m0_tl_endfor;
	sys_prep(p);
}

static void sys_crash(struct cb *c, struct event *e, void *d)
{
	struct sys_proc_state *ss = d;

	M0_PRE(ss->ss_pstate != P_CRASHED);
	ss->ss_pstate = P_CRASHED;
	memset(ss + 1, 0, c->c_proc->pr_size - sizeof *ss);
	if (e->e_p0 + 1 < crash_nr) {
		event_post(e->e_perm, E_START, c->c_proc, NULL, 0, 0,
			   e->e_p0 + 1, 0, P_CRASHED, -1);
	}
}

static void sys_start(struct cb *c, struct event *e, void *d)
{
	struct sys_proc_state *ss = d;

	M0_PRE(ss->ss_pstate == P_CRASHED);
	ss->ss_pstate = P_STARTED;
	ss->ss_bcount++;
	if (e->e_p0 + 1 < crash_nr) {
		event_post(e->e_perm, E_CRASH, c->c_proc, NULL, 0, 0,
			   e->e_p0 + 1, 0, P_STARTING, -1);
	}
}

static void sys_hastate(struct cb *c, struct event *e, void *d)
{
	struct sys_proc_state *ss = d;
	struct proc           *proc;
	int                    has;

	M0_PRE(ss->ss_hastate != e->e_sel0);
	M0_PRE(ss->ss_hastate != H_PERMANENT);
	ss->ss_hastate = e->e_sel0;
	m0_tl_for(p, &e->e_perm->p_proc, proc) {
		if (proc != e->e_perm->p_fatum) {
			lossless(e->e_perm->p_fatum, proc, O_HASET,
				 c->c_proc->pr_idx, ss->ss_hastate);
		}
	} m0_tl_endfor;
	if (e->e_p0 + 1 < hastate_nr) {
		if (ss->ss_hastate == H_ONLINE) {
			has = H_TRANSIENT;
		} else if (ss->ss_hastate == H_TRANSIENT) {
			if (e->e_p0 + 2 < hastate_nr)
				has = H_ONLINE;
			else
				has = H_PERMANENT;
		} else
			has = -1;
		if (has >= 0)
			event_post(e->e_perm, E_HASTATE, c->c_proc, NULL,
				   has, 0, e->e_p0 + 1, 0, P_CRASHED, -1);
	}
}

static void sys_prep(struct perm *p)
{
	struct proc *proc;

	m0_tl_for(p, &p->p_proc, proc) {
		sys_proc_init(proc);
	} m0_tl_endfor;
	p->p_fatum = proc_add(p, sizeof(struct sys_proc_state), NULL);
	palloc(p->p_fatum);
}

static void sys_proc_init(struct proc *proc)
{
	struct cb             *cb;
	struct perm           *p  = proc->pr_perm;
	struct sys_proc_state *ss = sys_pget(proc);

	ss->ss_pstate  = P_STARTED;
	if (0 < crash_nr)
		event_post(p, E_CRASH, proc, NULL, 0, 0, 0, 0, P_CRASHED, -1);
	cb = cb_add(p, proc);
	cb->c_op = E_CRASH;
	cb->c_invoke = &sys_crash;
	cb = cb_add(p, proc);
	cb->c_op = E_START;
	cb->c_invoke = &sys_start;

	ss->ss_hastate = H_ONLINE;
	if (0 < hastate_nr)
		event_post(p, E_HASTATE, proc, NULL, H_TRANSIENT,
			   0, 0, 0, P_CRASHED, -1);
	cb = cb_add(p, proc);
	cb->c_op = E_HASTATE;
	cb->c_invoke = &sys_hastate;
	cb = cb_add(p, proc);
	cb->c_op = E_TXSTATE;
	cb->c_sel0 = T_LOGGED;
	cb->c_invoke = &tx_logged;
}

static void sys_pfree(struct proc *proc, struct sys_proc_state *ss)
{
	int i;

	for (i = 0; i < ss->ss_kvnr; ++i)
		m0_free(ss->ss_kv[i].k_buf);
}

static void *sys_pinit(struct proc *proc, struct sys_proc_state *ss)
{
	ss->ss_kvnr = 0;
	return ss;
}

static struct sys_proc_state *sys_pget(struct proc *proc)
{
	return palloc(proc);
}

static struct step *step_add(struct step *s, struct event *skip)
{
	struct step *next = step_alloc(s->s_perm, s);
	struct pin  *scan;

	m0_tl_for(h, &s->s_event, scan) {
		struct event *e = scan->n_vhead;

		if (scan->n_vhead != skip)
			pin(next, true, e);
	} m0_tl_endfor;
	next->s_cur = h_tlist_head(&next->s_event);
	return next;
}

static void step_fix(struct step *s)
{
	while (s->s_cur != NULL && !enabled(s->s_cur->n_vhead))
		s->s_cur = h_tlist_next(&s->s_event, s->s_cur);
}

static void step_fini(struct step *s)
{
	struct pin   *scan;
	struct event *e;
	struct proc  *proc;

	s_tlink_del_fini(s);
	m0_tl_teardown(h, &s->s_event, scan) {
		e = scan->n_vhead;
		M0_ASSERT(scan == v_tlist_tail(&e->e_hist));
		v_tlink_del_fini(scan);
		if (v_tlist_is_empty(&e->e_hist))
			event_fini(e);
		m0_free(scan);
	}
	m0_tl_teardown(h, &s->s_proc, scan) {
		proc = scan->n_vhead;
		M0_ASSERT(scan == v_tlist_tail(&proc->pr_hist));
		v_tlink_del_fini(scan);
		if (proc->pr_pfree != NULL)
			proc->pr_pfree(proc, scan->n_data);
		m0_free(scan->n_data);
		m0_free(scan);
	}
	m0_free(s);
}

static struct pin *pin(struct step *step, bool event, void *head)
{
	struct pin   *p;
	struct event *e    = head;
	struct proc  *proc = head;

	p = xalloc(sizeof *p);
	h_tlink_init_at_tail(p, event ? &step->s_event : &step->s_proc);
	v_tlink_init_at_tail(p, event ? &e->e_hist : &proc->pr_hist);
	p->n_vhead = head;
	p->n_hhead = step;
	M0_POST(pin_invariant(p, event));
	return p;
}

static void post(struct perm *p, struct event *e)
{
	struct step *s = s_tlist_tail(&p->p_step);
	struct pin  *n = pin(s, true, e);

	if (s->s_cur == NULL)
		s->s_cur = n;
}

static struct proc *proc_add(struct perm *p, int size,
			     void (*pfree)(const struct proc *p,
					   const void *data))
{
	struct proc *proc = xalloc(sizeof *proc);
	struct proc *prev = p_tlist_tail(&p->p_proc);

	M0_PRE(size >= sizeof(struct sys_proc_state));
	proc->pr_perm  = p;
	proc->pr_size  = size;
	proc->pr_pfree = pfree;
	v_tlist_init(&proc->pr_hist);
	proc->pr_idx = prev == NULL ? 0 : prev->pr_idx + 1;
	p_tlink_init_at_tail(proc, &p->p_proc);
	return proc;
}

static struct cb *cb_add(struct perm *p, struct proc *proc)
{
	struct cb *cb = xalloc(sizeof *cb);

	cb_tlink_init_at_tail(cb, &p->p_cb);
	cb->c_proc = proc;
	return cb;
}

static struct invariant *invariant_add(struct perm *p)
{
	return NULL;
}

static int eseq = 0;

static struct event *event_alloc(struct perm *p, enum event_op eop,
				 struct proc *proc, struct proc *src,
				 uint64_t sel0, uint64_t sel1,
				 uint64_t p0, uint64_t p1, enum pstate pstate,
				 int bcount)
{
	struct event *e = xalloc(sizeof *e);

	e->e_seq    = eseq++;
	e->e_perm   = p;
	e->e_op     = eop;
	e->e_proc   = proc;
	e->e_src    = src;
	e->e_sel0   = sel0;
	e->e_sel1   = sel1;
	e->e_p0     = p0;
	e->e_p1     = p1;
	e->e_pstate = pstate;
	e->e_bcount = bcount;
	v_tlist_init(&e->e_hist);
	return e;
}

static struct event *event_post(struct perm *p, enum event_op eop,
				struct proc *proc, struct proc *src,
				uint64_t sel0, uint64_t sel1,
				uint64_t p0, uint64_t p1, enum pstate pstate,
				int bcount)
{
	struct event *e = event_alloc(p, eop, proc, src, sel0, sel1,
				      p0, p1, pstate, bcount);
	post(p, e);
	return e;
}

static void event_fini(struct event *e)
{
	m0_free(e);
}

static uint64_t tx_open(struct proc *proc)
{
	struct sys_proc_state *ss = sys_pget(proc);
	struct kv             *kv = kv_get(proc);

	kv->k_txid    = ++ss->ss_txid_last;
	kv->k_txstate = T_OPEN;
	return kv->k_txid;
}

static void tx_set(struct proc *proc, uint64_t tid,
		   uint64_t key, uint64_t val, void *buf)
{
	struct kv *kv = kv_get(proc);

	M0_PRE(key != 0);
	kv->k_txid    = tid;
	kv->k_txstate = T_OPEN;
	kv->k_key     = key;
	kv->k_val     = val;
	kv->k_buf     = buf;
}

static bool tx_logged_enabled(const struct event *e)
{
	uint64_t tid = e->e_p0;
	/* Transactions are logged in order of opening. */
	return m0_forall(i, tid - 1, tx_state(e->e_proc, i + 1) >= T_LOGGED);
}

static void tx_logged(struct cb *c, struct event *e, void *d)
{
	struct sys_proc_state *ss = sys_pget(c->c_proc);
	struct kv             *kv = kv_get(c->c_proc);

	kv->k_txid    = e->e_p0;
	kv->k_txstate = T_LOGGED;
	event_post(e->e_perm, E_TXSTATE, c->c_proc, NULL, T_COMMITTED, 0,
		   kv->k_txid, 0, P_STARTED, ss->ss_bcount);
}

static void tx_close(struct proc *proc, uint64_t tid)
{
	struct sys_proc_state *ss = sys_pget(proc);
	struct kv             *kv = kv_get(proc);
	struct event          *e;

	kv->k_txstate = T_CLOSED;
	kv->k_txid    = tid;
	e = event_post(proc->pr_perm, E_TXSTATE, proc, NULL, T_LOGGED, 0,
		       tid, 0, P_STARTED, ss->ss_bcount);
	e->e_enabled = &tx_logged_enabled;
}

static enum tx_state tx_state(struct proc *proc, uint64_t txid)
{
	struct pin *n;
	int         i;

	M0_PRE(txid != 0);
	for (n = v_tlist_tail(&proc->pr_hist); n != NULL;
	     n = v_tlist_prev(&proc->pr_hist, n)) {
		struct sys_proc_state *ss = n->n_data;

		for (i = ss->ss_kvnr - 1; i >= 0; --i) {
			if (ss->ss_kv[i].k_txid == txid)
				return ss->ss_kv[i].k_txstate;
		}
	}
	return T_NONE;
}

static void tx_get(struct proc *proc, uint64_t key, uint64_t *val, void **buf)
{
	struct pin   *n;
	int           i;
	enum tx_state need = T_OPEN;

	M0_PRE(key != 0);
	for (n = v_tlist_tail(&proc->pr_hist); n != NULL;
	     n = v_tlist_prev(&proc->pr_hist, n)) {
		struct sys_proc_state *ss = n->n_data;

		if (ss->ss_pstate == P_CRASHED)
			need = T_LOGGED;
		for (i = ss->ss_kvnr - 1; i >= 0; --i) {
			if (ss->ss_kv[i].k_key == key &&
			    ss->ss_kv[i].k_txstate == T_OPEN &&
			    (need == T_OPEN ||
			     tx_state(proc, ss->ss_kv[i].k_txid) >= need)) {
				*val = ss->ss_kv[i].k_val;
				*buf = ss->ss_kv[i].k_buf;
				return;
			}
		}
	}
	*val = 0;
	*buf = NULL;
}

static struct kv *kv_get(struct proc *proc)
{
	struct sys_proc_state *ss = sys_pget(proc);

	M0_ASSERT(ss->ss_kvnr < ARRAY_SIZE(ss->ss_kv));
	return &ss->ss_kv[ss->ss_kvnr++];
}

static bool recv_enabled(const struct event *e)
{
	return m0_tl_forall(h, n, &s_tlist_tail(&e->e_perm->p_step)->s_event, ({
				struct event *scan = n->n_vhead;
				/*
				 * In-order delivery between pairs of processes.
				 */
				ergo(scan->e_op   == E_RECV &&
				     scan->e_proc == e->e_proc &&
				     scan->e_src  == e->e_src,
				     scan->e_seq  >= e->e_seq); }));
}

static bool lost_enabled(const struct event *e)
{
	/*
	 * A request can be lost only if it is no longer being resent.
	 *
	 * Resend stops when the source node crashes or receives an HA
	 * notification about target permanent failure (or when the reply is
	 * received, but this is irrelevant).
	 */
	return  sys_pget(e->e_src)->ss_bcount != e->e_p1 ||
		sys_pget(e->e_src)->ss_pstate != P_STARTED ||
		thinks(e->e_src, e->e_proc) == H_PERMANENT;
}

static struct event *lossless(struct proc *src, struct proc *dst,
			      enum msg_op mop, uint64_t p0, uint64_t p1)
{
	struct event *recv = event_post(src->pr_perm, E_RECV, dst, src,
					M_REQ, mop, p0, p1, P_STARTED, -1);
	recv->e_enabled = &recv_enabled;
	return recv;
}

static void req(struct proc *src, struct proc *dst, enum msg_op mop,
		uint64_t p0, uint64_t p1)
{
	struct event *recv = lossless(src, dst, mop, p0, p1);
	struct event *lost = event_post(src->pr_perm, E_REQLOST, dst, src,
					0, 0, (uint64_t)recv,
					sys_pget(dst)->ss_bcount,
					P_CRASHED, -1);
	recv->e_alternative = lost;
	lost->e_enabled = &lost_enabled;
	lost->e_alternative = recv;
}

#if 0
static void send(struct proc *src, struct proc *dst, enum msg_type mt,
		 enum msg_op mop, uint64_t p0, uint64_t p1, enum pstate pstate)
{
	struct event *e = event_post(src->pr_perm, E_RECV, dst, src,
				     mt, mop, p0, p1, pstate,
				     sys_pget(dst)->ss_bcount);
}
#endif

static void cont(struct proc *proc,
		 void (*invoke)(struct cb *cb, struct event *e, void *data))
{
	static uint64_t  sel0 = 0;
	struct cb       *cb;

	cb = cb_add(proc->pr_perm, proc);
	cb->c_op     = E_CONT;
	cb->c_proc   = proc;
	cb->c_invoke = invoke;
	cb->c_sel0   = ++sel0;
	event_post(proc->pr_perm, E_CONT, proc, NULL, sel0, 0, 0, 0, P_STARTED,
		   sys_pget(proc)->ss_bcount);
}

static enum ha_state thinks(struct proc *proc, struct proc *object)
{
	struct step *s;

	for (s = s_tlist_tail(&proc->pr_perm->p_step); s != NULL;
	     s = s_tlist_prev(&proc->pr_perm->p_step, s)) {
		struct event *cur = s->s_cur->n_vhead;

		if (cur->e_op == E_RECV && cur->e_proc == proc &&
		    cur->e_sel1 == O_HASET && object->pr_idx == cur->e_p0)
			return cur->e_p1;
	}
	return H_ONLINE;
}

static void *xalloc(size_t nob)
{
	void *data = m0_alloc(nob);

	M0_ASSERT(data != NULL);
	return data;
}

static void step_print(const struct step *s)
{
	struct pin *scan;

	printf("%*.*s%3"PRIi64":",
	       (int)s->s_nr * 4, (int)s->s_nr * 4, "", s->s_nr);
	m0_tl_for(h, &s->s_event, scan) {
		printf(" %s", scan == s->s_cur ? "^" : "");
		printf("%s", enabled(scan->n_vhead) ? "" : "-");
		event_print(scan->n_vhead);
	} m0_tl_endfor;
	printf("\n");
}

static void event_print(const struct event *e)
{
	static const char *oname[] = {
		[E_NONE]    = "",
		[E_CRASH]   = "crash",
		[E_START]   = "start",
		[E_SEND]    = "send",
		[E_RECV]    = "recv",
		[E_HASTATE] = "hastate",
		[E_TXSTATE] = "txstate",
		[E_TXSET]   = "txset",
		[E_CONT]    = "cont",
		[E_REQLOST] = "rep-lost",
		[E_REPLOST] = "req-lost"
	};
	static const char *mtname[] = {
		[M_NONE]  = "",
		[M_REQ]   = "req",
		[M_REPLY] = "reply",
		[M_1WAY]  = "1way"
	};
	static const char *mopname[] = {
		[O_NONE]  = "",
		[O_HASET] = "haset",
		[O_USER]  = "user"
	};
	static const char *hasname[] = {
		[H_ONLINE]    = "online",
		[H_TRANSIENT] = "transient",
		[H_PERMANENT] = "permanent",
	};
	printf("[");
	if (e->e_src != NULL)
		printf("%i->", e->e_src->pr_idx);
	if (e->e_proc != NULL)
		printf("%i", e->e_proc->pr_idx);
	else
		printf("*");
	printf(".%s:", oname[e->e_op]);
	if (M0_IN(e->e_op, (E_SEND, E_RECV)))
		printf("%s:%s", mtname[e->e_sel0], mopname[e->e_sel1]);
	else if (e->e_op == E_HASTATE)
		printf("%s", hasname[e->e_sel0]);
	else
		printf("%"PRIx64":%"PRIx64"", e->e_sel0, e->e_sel1);
	if (e->e_p0 != 0 || e->e_p1 != 0)
		printf("[%"PRIx64":%"PRIx64"]", e->e_p0, e->e_p1);
	if (e->e_print != NULL) {
		printf(" ");
		e->e_print(e);
	}
	printf("]");
}

/** Models. */
enum model {
      MOD_NONE,
      MOD_CONT,
      MOD_REQ,
      MOD_2PC
};


struct m2pc_state {
	struct sys_proc_state m_base;
	int                   m_c_nr;
	int                   m_a_nr;
	bool                  m_commit;
	bool                  m_done;
};

int main(int argc, char **argv)
{
	struct perm   p;
	struct m0     instance = {0};
	struct proc **proc;
	int           result;
	int           i;
	int           proc_nr  = 2;
	enum model    model    = MOD_NONE;
	int           psize;

	m0_node_uuid_string_set(NULL);
	result = m0_init(&instance);
	if (result != 0)
		err(EX_CONFIG, "Cannot initialise motr: %d", result);

	result = M0_GETOPTS("perm", argc, argv,
		   M0_FORMATARG('m', "Model", "%i", &model),
		   M0_FORMATARG('d', "Print depth", "%i", &print_depth),
		   M0_FORMATARG('c', "Crash nr", "%i", &crash_nr),
		   M0_FORMATARG('h', "Hastate nr", "%i", &hastate_nr),
		   M0_FORMATARG('p', "Number of processes", "%i", &proc_nr));
	if (result != 0)
		err(EX_CONFIG, "Wrong option.");
	perm_init(&p);
	psize = model == MOD_2PC ? sizeof(struct m2pc_state) :
		sizeof(struct sys_proc_state);
	proc = xalloc(proc_nr * sizeof proc[0]);
	for (i = 0; i < proc_nr; ++i) {
		proc[i] = proc_add(&p, psize, (void *)&sys_pfree);
	}
	invariant_add(&p);
	perm_prep(&p);

	switch (model) {
	case MOD_NONE:
		break;
	case MOD_CONT:
		cont(proc[0], LAMBDA(void, (struct cb *c,
					    struct event *e, void *d) {
				uint64_t tid = tx_open(e->e_proc);

				tx_set(e->e_proc, tid, 17, 12, NULL);
				tx_close(e->e_proc, tid);
			}));
		cont(proc[0], LAMBDA(void, (struct cb *c,
					    struct event *e, void *d) {
				uint64_t tid = tx_open(e->e_proc);

				tx_set(e->e_proc, tid, 17, 13, NULL);
				tx_close(e->e_proc, tid);
			}));
		cont(proc[0], LAMBDA(void, (struct cb *c,
					    struct event *e, void *d) {
				uint64_t val;
				void    *buf;
				tx_get(e->e_proc, 17, &val, &buf);
				M0_ASSERT(M0_IN(val, (12, 13, 0)));
				/* printf("GOT: %"PRId64" %p\n", val, buf); */
			}));
		break;
	case MOD_REQ:
		req(proc[0], proc[0], O_USER, 42, 43);
		break;
	case MOD_2PC: {
	}
		break;
	default:
		M0_IMPOSSIBLE("Wrong model.");
		break;
	}
	perm_run(&p);
	perm_fini(&p);
	m0_fini();
	return EX_OK;
}

/** @} end of perm group */
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
