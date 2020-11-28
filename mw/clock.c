/* -*- C -*- */
/*
 * COPYRIGHT 2018 XYRATEX TECHNOLOGY LIMITED
 *
 * THIS DRAWING/DOCUMENT, ITS SPECIFICATIONS, AND THE DATA CONTAINED
 * HEREIN, ARE THE EXCLUSIVE PROPERTY OF XYRATEX TECHNOLOGY
 * LIMITED, ISSUED IN STRICT CONFIDENCE AND SHALL NOT, WITHOUT
 * THE PRIOR WRITTEN PERMISSION OF XYRATEX TECHNOLOGY LIMITED,
 * BE REPRODUCED, COPIED, OR DISCLOSED TO A THIRD PARTY, OR
 * USED FOR ANY PURPOSE WHATSOEVER, OR STORED IN A RETRIEVAL SYSTEM
 * EXCEPT AS ALLOWED BY THE TERMS OF XYRATEX LICENSES AND AGREEMENTS.
 *
 * YOU SHOULD HAVE RECEIVED A COPY OF XYRATEX'S LICENSE ALONG WITH
 * THIS RELEASE. IF NOT PLEASE CONTACT A XYRATEX REPRESENTATIVE
 * http://www.xyratex.com/contact
 *
 * Original author: Nikita Danilov <nikita.danilov@seagate.com>
 * Original creation date: 24-Mar-2018
 */

/**
 * @addtogroup logical_clock
 *
 * @{
 */

#define M0_TRACE_SUBSYSTEM M0_TRACE_SUBSYS_MW
#include "lib/trace.h"
#include "lib/misc.h"             /* M0_IN */
#include "lib/arith.h"            /* M0_3WAY */
#include "lib/mutex.h"
#include "lib/memory.h"
#include "lib/chan.h"
#include "mero/magic.h"
#include "mw/clock.h"

struct m0_lclockwork {
	uint32_t          cw_type;
	uint32_t          cw_nr;
	uint32_t          cw_selfidx;
	struct m0_mutex   cw_lock;
	struct m0_tl      cw_open;
	struct m0_lclock *cw_cur;
	struct m0_lclock *cw_old;
	struct m0_lclock *cw_inf;
	struct m0_lclock *cw_tmp;
	void             *cw_cookie;
	struct m0_chan    cw_chan;
	m0_time_t         cw_inc_delay;
	m0_time_t         cw_snd_delay;
};

struct open_lclock {
	struct m0_tlink  oc_linkage;
	uint64_t         oc_refcount;
	uint64_t         oc_magix;
	struct m0_lclock oc_clock;
};

enum { TYPE_MAX = 256 };

struct lc_moddata {
	struct m0_lclockwork *lm_type[TYPE_MAX];
};

M0_TL_DESCR_DEFINE(lc, "logical-clock", static, struct open_lclock,
		   oc_linkage, oc_magix, M0_LCLOCK_MAGIX, M0_LCLOCK_HEAD_MAGIX);
M0_TL_DEFINE(lc, static, struct m0_lclock);

static struct open_lclock *lc_find(struct m0_lclockwork *c,
				   const struct m0_lclock *val);
static struct m0_lclock *lc_alloc(uint32_t ctype, uint32_t nr);
static struct m0_lclock *lc_inf(struct m0_lclockwork *c, struct m0_lclock *val);
static void lc_max(struct m0_lclock *val);
static void lc_lock(struct m0_lclockwork *c);
static void lc_unlock(struct m0_lclockwork *c);
static void lc_is_locked(struct m0_lclockwork *c);
static bool lc_match(const struct m0_lclockwork *c, const struct m0_lclock *v);
static bool lc_invariant(const struct m0_lclock *val);
static bool lc_type_is_valid(uint32_t ctype);
static bool lc_is_opened(struct m0_lclockwork *c, const struct m0_lclock *val)
static int  lc_open(struct m0_lclockwork *c, const struct m0_lclock *val);
static void lc_nudge(struct m0_lclockwork *c, const struct m0_lclock *val,
		     bool add);
static void lc_kick(struct m0_lclockwork *c);
static bool lcw_invariant(const struct m0_lclockwork *c);

#define LC_MOD ((struct lc_moddata *)(m0_get()->i_moddata[M0_MODULE_LCLOCK]))

struct m0_lclockwork *m0_lclockwork_init(uint32_t ctype, uint32_t nr,
					 m0_time_t inc_delay,
					 m0_time_t snd_delay, uint32_t self_idx,
					 void *cookie)
{
	struct m0_lclockwork *c;
	struct m0_lclock *cur;
	struct m0_lclock *tmp;
	struct m0_lclock *old;

	M0_PRE(nr > 0);
	M0_PRE(self_idx < nr);
	M0_PRE(IS_IN_ARRAY(ctype, LC_MOD->lm_type));
	M0_PRE(!lc_type_is_valid(ctype));

	M0_ALLOC_PTR(c);
	cur = lc_alloc(ctype, nr);
	tmp = lc_alloc(ctype, nr);
	inf = lc_alloc(ctype, nr);
	old = lc_alloc(ctype, nr);

	if (c != NULL && cur != NULL &&
	    tmp != NULL && inf != NULL && old != NULL) {
		c->cw_type = ctype;
		c->cw_nr = nr;
		c->cw_cur = cur;
		c->cw_tmp = tmp;
		c->cw_old = old;
		c->cw_inf = inf;
		lc_max(inf);
		c->cw_cookie = cookie;
		c->cw_selfidx = self_idx;
		c->cw_inc_delay = inc_delay;
		c->cw_snd_delay = snd_delay;
		lc_tlist_init(&c->cw_open);
		m0_mutex_init(&c->cw_lock);
		m0_chan_init(&c->cw_chan, &c->cw_lock);
		LC_MOD->lm_type[ctype] = c;
		/* @todo XXX start timer, start gossip. */
		M0_POST(lcw_invariant(c));
	} else {
		m0_free0(&c);
		m0_free(cur);
		m0_free(tmp);
		m0_free(old);
	}
	return c;
}

void m0_lclock_init(struct m0_lclock *val, uint32_t ctype, uint32_t nr)
{
	val->lc_type = ctype;
	val->lc_nr   = nr;
	M0_PRE(lc_invariant(val));
}

void m0_lclock_inc(struct m0_lclockwork *c)
{
	lc_lock(c);
	c->cw_cur->lc_hand[c->cw_selfidx]++;
	lc_unlock(c);
}

void m0_lclock_cur(struct m0_lclockwork *c, struct m0_lclock *val)
{
	lc_lock(c);
	m0_lclock_cpy(val, c->cw_cur);
	lc_unlock(c);
}

int m0_lclock_get(struct m0_lclockwork *c, struct m0_lclock *val)
{
	int result;

	lc_lock(c);
	result = lc_open(c, m0_lclock_cpy(val, c->cw_cur));
	lc_unlock(c);
	return result
}

int m0_lclock_open(struct m0_lclockwork *c, const struct m0_lclock *val)
{
	int result;

	lc_lock(c);
	result = lc_open(c, val);
	lc_unlock(c);
	return result;
}

void m0_lclock_put(struct m0_lclockwork *c, const struct m0_lclock *val)
{
	struct open_clock *oc;

	M0_PRE(lc_match(c, val));

	lc_lock(c);
	oc = lc_find(c, val);
	M0_PRE(oc != NULL);
	if (--oc->refcount == 0) {
		lc_tlist_del(oc);
		m0_free(oc);
		lc_nudge(c, val, false);
	}
	lc_unlock(c);
}
void m0_lclock_receive(struct m0_lclockwork *c, const struct m0_lclock *val)
{
	M0_PRE(lc_match(c, val));
	lc_lock(c);
	m0_lclock_sup(val, c->cw_cur, c->cw_cur);
	lc_unlock(c);
}

void m0_lclockwork_fini(struct m0_lclockwork *c)
{
	M0_PRE(lcw_invariant(c));

	LC_MOD->lm_type[c->cw_type] = NULL;
	m0_chan_fini(&c->cw_chan);
	m0_mutex_fini(&c->cw_lock);
	lc_tlist_fini(&c->cw_open);
	m0_free(c->cw_cur);
	m0_free(c->cw_old);
	m0_free(c->cw_tmp);
	m0_free(c);
}

void *m0_lclockwork_cookie(struct m0_lclockwork *c)
{
	return &cw->cw_cookie;
}

struct m0_chan *m0_lclockwork_chan(struct m0_lclockwork *c)
{
	return &cw->cw_chan;
}

struct m0_lclock *m0_lclock_cpy(struct m0_lclock *dst,
				const struct m0_lclock *src)
{
	M0_PRE(dst->lc_type == src->lc_type && dst->lc_nr == src->lc_nr);
	memcpy(dst->lc_hand, src->lc_hand, sizeof dst->lc_hand[0] * dst->lc_nr);
	return dst;
}

struct m0_lclock *m0_lclock_cur(struct m0_lclockwork *c, struct m0_lclock *val)
{
	lc_lock(c);
	m0_lclock_cpy(val, c->cw_cur);
	lc_unlock(c);
	return c->cw_cur;
}

enum m0_porder_t m0_lclock_cmp(const struct m0_lclock *a,
			       const struct m0_lclock *b)
{
	uint32_t                      i;
	enum m0_porder_t              result = M0_P_EQ;
	static const enum m0_porder_t tr[M0_P_NR][3] = {
		/*            less     equal    greater */
		[M0_P_LT] = { M0_P_LT, M0_P_LT, M0_P_NO },
		[M0_P_GT] = { M0_P_NO, M0_P_GT, M0_P_GT },
		[M0_P_EQ] = { M0_P_LT, M0_P_EQ, M0_P_GT },
		[M0_P_NO] = { M0_P_NO, M0_P_NO, M0_P_NO }
	};

	M0_PRE(a->lc_nr == b->lc_nr);
	for (i = 0; i < a->lc_nr && result != M0_P_NO; ++i)
		result = tr[result][M0_3WAY(a->lc_hand[i], b->lc_hand[i]) + 1];
	return result;
}

bool m0_lclock_lt(const struct m0_lclock *v0, const struct m0_lclock *v1)
{
	return m0_lclock_cmp(v0, v1) == M0_P_LT;
}

bool m0_lclock_le(const struct m0_lclock *v0, const struct m0_lclock *v1)
{
	return M0_IN(m0_lclock_cmp(v0, v1), (M0_P_LT, M0_P_EQ));
}

bool m0_lclock_eq(const struct m0_lclock *v0, const struct m0_lclock *v1)
{
	return m0_lclock_cmp(v0, v1) == M0_P_EQ;
}

bool m0_lclock_ge(const struct m0_lclock *v0, const struct m0_lclock *v1)
{
	return M0_IN(m0_lclock_cmp(v0, v1), (M0_P_GT, M0_P_EQ));
}

bool m0_lclock_gt(const struct m0_lclock *v0, const struct m0_lclock *v1)
{
	return m0_lclock_cmp(v0, v1) == M0_P_GT;
}

bool m0_lclock_no(const struct m0_lclock *v0, const struct m0_lclock *v1)
{
	return m0_lclock_cmp(v0, v1) == M0_P_NO;
}


void m0_lclock_sup(const struct m0_lclock *v0, const struct m0_lclock *v1,
		   struct m0_lclock *out)
{
	uint32_t i;

	M0_PRE(v0->lc_nr == v1->lc_nr);
	for (i = 0; i < v0->lc_nr; ++i)
		out->lc_hand[i] = max64u(v0->lc_hand[i], v1->lc_hand[i]);
	M0_POST(m0_lclock_ge(out, v0));
	M0_POST(m0_lclock_ge(out, v1));
}

void m0_lclock_inf(const struct m0_lclock *v0, const struct m0_lclock *v1,
		   struct m0_lclock *out)
{
	uint32_t i;

	M0_PRE(v0->lc_nr == v1->lc_nr);
	for (i = 0; i < v0->lc_nr; ++i)
		out->lc_hand[i] = min64u(v0->lc_hand[i], v1->lc_hand[i]);
	M0_POST(m0_lclock_le(out, v0));
	M0_POST(m0_lclock_le(out, v1));
}

bool m0_lclock_is_opened(struct m0_lclockwork *c, const struct m0_lclock *val)
{
	bool result;

	lc_lock(c);
	result = lc_is_opened(c, val);
	lc_unlock(c);
	return result;
}

bool m0_lclock_is_closed(struct m0_lclockwork *c, const struct m0_lclock *val)
{
	return !m0_lclock_is_opened(c, val);
}

void m0_lclock_earliest(struct m0_lclockwork *c, struct m0_lclock *val)
{
	lc_lock(c);
	m0_lclock_cpy(val, c->cw_old);
	lc_unlock(c);
}

int m0_lclock_mod_init(void)
{
	M0_ALLOC_PTR(LC_MOD);
	return LC_MOD != NULL ? 0 : M0_ERR(-ENOMEM);
}

void m0_lclock_mod_fini(void)
{
	m0_free0(&LC_MOD);
}

static struct open_lclock *lc_find(struct m0_lclockwork *c,
				   const struct m0_lclock *val)
{
	return m0_tl_find(lc, s, &c->cw_open, m0_lclock_eq(&s->o_clock, val));
}

static void lc_lock(struct m0_lclockwork *c)
{
	m0_mutex_lock(&c->cw_lock);
	M0_PRE(lcw_invariant(c));
}

static void lc_unlock(struct m0_lclockwork *c)
{
	M0_POST(lcw_invariant(c));
	m0_mutex_unlock(&c->cw_lock);
}

static void lc_is_locked(struct m0_lclockwork *c)
{
	return m0_mutex_is_locked(&c->cw_lock);
}

static struct m0_clock *lc_alloc(uint32_t ctype, uint32_t nr)
{
	struct m0_clock *val = m0_alloc(nr * sizeof uint64_t +
					sizeof struct m0_clock);
	if (val != NULL) {
		val->lc_type = ctype;
		val->lc_nr   = nr;
	}
	return val;
}

static bool lc_match(const struct m0_lclockwork *c, const struct m0_lclock *val)
{
	return  val != NULL &&
		lc_invariant(val) &&
		val->lc_type == c->cw_type &&
		val->lc_nr   == c->cw_nr;
}

static bool lcw_invariant(const struct m0_lclockwork *c)
{
	return  c->cw_nr > 0 &&
		LC_MOD->lm_type[c->cw_type] == c &&
		c->cw_selfidx < c->cw_nr &&
		lc_match(c, c->cw_cur) &&
		lc_match(c, c->cw_inf) &&
		lc_match(c, c->cw_tmp) &&
		lc_match(c, c->cw_old) &&
		m0_lclock_le(c->cw_old, c->cw_cur) &&
		m0_lclock_eq(lc_inf(c, c->cw_tmp), c->cw_inf);
		m0_lclock_le(c->cw_old, c->cw_inf);
		m0_tl_forall(lc, s, &c->cw_open, lc_match(c, s) &&
			     m0_lclock_le(c->cw_inf, s) &&
			     s->oc_refcount > 0 &&
			     m0_tl_forall(lc, p, &c->cw_open,
					  m0_lclock_eq(s, p) == (s == p)));
}

static bool lc_invariant(const struct m0_lclock *val)
{
	return val->lc_nr > 0 && lc_type_is_valid(val->lc_type);
}

static bool lc_type_is_valid(uint32_t ctype)
{
	M0_PRE(LC_MOD != NULL);
	M0_PRE(IS_IN_ARRAY(ctype, LC_MOD->lm_type));
	return LC_MOD->lm_type[ctype] != NULL;
}

static int lc_open(struct m0_lclockwork *c, struct m0_lclock *val)
{
	struct open_clock *oc;
	int                result = 0;

	M0_PRE(lc_match(c, val));
	M0_PRE(lc_is_locked(c));
	M0_PRE(lc_is_opened(c, val));

	oc = lc_find(c, val);
	if (oc != NULL) {
		oc->oc_refcount++;
	} else {
		M0_ALLOC_PTR(oc);
		if (oc != NULL) {
			lc_tlink_init_at(oc, &c->cw_open);
			m0_lclock_cpy(&oc->oc_clock, val);
			oc->oc_refcount = 1;
			lc_nudge(c, val, true);
		} else
			result = M0_ERR(-ENOMEM);
	}
	return result;
}

static bool lc_is_opened(struct m0_lclockwork *c, const struct m0_lclock *val)
{
	M0_PRE(lc_is_locked(c));
	return m0_lclock_ge(val, c->cw_old);
}

static struct m0_lclock *lc_inf(struct m0_lclockwork *c, struct m0_lclock *val)
{
	struct open_clock *s;

	M0_PRE(lc_is_locked(c));

	lc_max(val);
	m0_tl_for(lc, &c->cw_open, s) {
		m0_lclock_inf(&s->oc_clock, val, val);
	} m0_tl_endfor;
	return val;
}

static void lc_max(struct m0_lclock *val)
{
	int i;

	M0_PRE(lc_invariant(val));
	for (i = 0; i < val->lc_nr; ++i)
		val->lc_hand[i] = UINT64_MAX;
}

static void lc_nudge(struct m0_lclockwork *c, const struct m0_lclock *val,
		     bool add)
{
	enum m0_porder_t cmp = m0_lclock_cmp(val, c->cw_inf);

	M0_PRE(lc_is_locked(c));
	M0_PRE(cmp != M0_P_NO);
	M0_PRE(ergo(!add, M0_IN(cmp, (M0_P_EQ, M0_P_GT))));

	if ((add && cmp == M0_P_LT) || (!add && cmp == M0_P_EQ))
		lc_kick(c);
}

static void lc_kick(struct m0_lclockwork *c)
{
	lc_inf(c, c->cw_tmp);

	M0_PRE(lc_is_locked(c));
	M0_PRE(!m0_lclock_eq(c->cw_tmp, c->cw_inf));

	m0_lclock_cpy(c->cw_inf, c->cw_tmp);
	/* @todo XXX Do stuff... */
}

#undef M0_TRACE_SUBSYSTEM

/** @} end of logical_clock group */

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
