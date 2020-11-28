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

#pragma once

#ifndef __MERO_MW_CLOCK_H__
#define __MERO_MW_CLOCK_H__

/**
 * @defgroup lclock
 *
 * @{
 */

#include "lib/time.h"
#include "lib/types.h"                      /* uint32_t, uint64_t */
#include "xcode/xcode_attr.h"

struct m0_chan;

struct m0_lclock {
	uint32_t lc_type;
	uint32_t lc_nr;
	uint64_t lc_hand[0];
};

struct m0_lclockdata {
	uint32_t  lcd_nr;
	uint64_t *lcd_hand;
} M0_XCA_SEQUENCE M0_XCA_DOMAIN(rpc|be);

struct m0_lclockwire {
	uint32_t             lcw_type;
	struct m0_lclockdata lcw_data;
} M0_XCA_RECORD M0_XCA_DOMAIN(rpc|be);

/**
 * Defines logical clock structure along with the storage for clock data.
 *
 * @code
 * struct bar {
 *         ...
 *         M0_LCLOCK(b_clock, 1);
 *         ...
 * } *b;
 * ...
 * m0_lclock_get(c, &b->b_clock);
 * ...
 * M0_LCLOCK(clock, 16) foo;
 * ...
 * m0_lclock_cur(c, &foo.clock);
 * @endcode
 */
#define M0_LCLOCK(name, N)				\
/*								\
 * gcc "Unnamed Structure and Union Fields" extension.		\
 * https://gcc.gnu.org/onlinedocs/gcc/Unnamed-Fields.html	\
 */								\
struct {							\
	struct m0_lclock name;					\
	uint64_t         name ## __lc_data[N];			\
}

struct m0_lclockwork;

struct m0_lclockwork *m0_lclockwork_init(uint32_t ctype, uint32_t nr,
					 m0_time_t inc_delay,
					 m0_time_t snd_delay, uint32_t self_idx,
					 void *cookie);
void m0_lclock_init     (struct m0_lclock *val, uint32_t ctype, uint32_t nr);
void m0_lclock_inc      (struct m0_lclockwork *c);
void m0_lclock_cur      (struct m0_lclockwork *c, struct m0_lclock *val);
int  m0_lclock_get      (struct m0_lclockwork *c, struct m0_lclock *val);
int  m0_lclock_open     (struct m0_lclockwork *c, const struct m0_lclock *val);
void m0_lclock_put      (struct m0_lclockwork *c, const struct m0_lclock *val);
void m0_lclock_receive  (struct m0_lclockwork *c, const struct m0_lclock *val);
bool m0_lclock_is_opened(struct m0_lclockwork *c, const struct m0_lclock *val);
bool m0_lclock_is_closed(struct m0_lclockwork *c, const struct m0_lclock *val);
void m0_lclock_earliest (struct m0_lclockwork *c, struct m0_lclock *val);
void m0_lclockwork_fini (struct m0_lclockwork *c);

void           *m0_lclockwork_cookie(struct m0_lclockwork *c);
struct m0_chan *m0_lclockwork_chan  (struct m0_lclockwork *c);

enum m0_porder_t {
	M0_P_LT,
	M0_P_GT,
	M0_P_EQ,
	M0_P_NO,

	M0_P_NR
};

enum m0_porder_t m0_lclock_cmp(const struct m0_lclock *v0,
			       const struct m0_lclock *v1);
bool m0_lclock_lt(const struct m0_lclock *v0, const struct m0_lclock *v1);
bool m0_lclock_le(const struct m0_lclock *v0, const struct m0_lclock *v1);
bool m0_lclock_eq(const struct m0_lclock *v0, const struct m0_lclock *v1);
bool m0_lclock_ge(const struct m0_lclock *v0, const struct m0_lclock *v1);
bool m0_lclock_gt(const struct m0_lclock *v0, const struct m0_lclock *v1);
bool m0_lclock_no(const struct m0_lclock *v0, const struct m0_lclock *v1);

struct m0_lclock *m0_lclock_sup(const struct m0_lclock *v0,
				const struct m0_lclock *v1,
				struct m0_lclock *out);
struct m0_lclock *m0_lclock_inf(const struct m0_lclock *v0,
				const struct m0_lclock *v1,
				struct m0_lclock *out);
struct m0_lclock *m0_lclock_cpy(struct m0_lclock *dst,
				const struct m0_lclock *src);

void m0_lclock_decode(const struct m0_lclockwire *cw, struct m0_lclock *out);
void m0_lclock_encode(const struct m0_lclock *val, struct m0_lclockwire *out);

int  m0_lclock_mod_init(void);
void m0_lclock_mod_fini(void);

/** @} end of lclock group */
#endif /* __MERO_MW_CLOCK_H__ */

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
