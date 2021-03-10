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

#define M0_TRACE_SUBSYSTEM M0_TRACE_SUBSYS_DTM0
#include "lib/trace.h"

#include "dtm0/clk_src.h"
#include "lib/errno.h" /* EINVAL */
#include "lib/assert.h" /* M0_ASSERT and so on */

enum {
	/* The hard limit in the now() busy loop.
	 * The program should panic() if the physical
	 * clock did not advance for too long (more than
	 * MAX_RETRIES * ADV_NS nanoseconds).
	 */
	CS_PHYS_NOW_MAX_RETRIES = 16,

	/* A soft limit in the now() busy loop.
	 * This limit should be as small as possible but
	 * big enough to make the clock advance.
	 */
	CS_PHYS_NOW_ADV_NS = 1,
};

struct m0_dtm0_clk_src_ops {
	int (*cmp)(const struct m0_dtm0_ts *left,
		   const struct m0_dtm0_ts *right);
	int (*now)(struct m0_dtm0_clk_src *cs, struct m0_dtm0_ts *ts);
	int (*observed)(struct m0_dtm0_clk_src  *cs,
			const struct m0_dtm0_ts *other_ts);
};

/* See M0_DTM0_CS_PHYS for details */
static const struct m0_dtm0_clk_src_ops cs_phys_ops;

M0_INTERNAL int m0_dtm0_clk_src_init(struct m0_dtm0_clk_src *cs,
				    enum m0_dtm0_cs_types    type)
{
	M0_PRE(M0_IN(type, (M0_DTM0_CS_PHYS)));
	m0_mutex_init(&cs->cs_phys_lock);
	cs->cs_prev = M0_DTM0_TS_MIN;
	cs->cs_ops = &cs_phys_ops;
	return 0;
}

M0_INTERNAL void m0_dtm0_clk_src_fini(struct m0_dtm0_clk_src *cs)
{
	M0_PRE(cs);
	M0_PRE(M0_IN(cs->cs_ops, (&cs_phys_ops)));
	m0_mutex_fini(&cs->cs_phys_lock);
	cs->cs_prev = M0_DTM0_TS_INIT;
	cs->cs_ops = NULL;
}

M0_INTERNAL enum m0_dtm0_ts_ord m0_dtm0_ts_cmp(const struct m0_dtm0_clk_src *cs,
					       const struct m0_dtm0_ts *left,
					       const struct m0_dtm0_ts *right)
{
	M0_PRE(m0_dtm0_ts__invariant(left));
	M0_PRE(m0_dtm0_ts__invariant(right));
	return cs->cs_ops->cmp(left, right);
}

M0_INTERNAL int m0_dtm0_clk_src_now(struct m0_dtm0_clk_src *cs,
				    struct m0_dtm0_ts      *ts)
{
	return cs->cs_ops->now(cs, ts);
}

M0_INTERNAL int m0_dtm0_clk_src_observed(struct m0_dtm0_clk_src *cs,
					 const struct m0_dtm0_ts *other_ts)
{
	M0_PRE(m0_dtm0_ts__invariant(other_ts));
	return cs->cs_ops->observed(cs, other_ts);
}

M0_INTERNAL bool m0_dtm0_ts__invariant(const struct m0_dtm0_ts *ts)
{
	const struct m0_dtm0_ts min_ts = M0_DTM0_TS_MIN;
	const struct m0_dtm0_ts max_ts = M0_DTM0_TS_MAX;

	return _0C(ts->dts_phys >= min_ts.dts_phys) &&
		_0C(ts->dts_phys <= max_ts.dts_phys);
}

/* Phys clock implementation */

static enum m0_dtm0_ts_ord cs_phys_cmp(const struct m0_dtm0_ts *left,
				       const struct m0_dtm0_ts *right)
{
	return M0_3WAY(left->dts_phys, right->dts_phys);
}

static int cs_phys_now(struct m0_dtm0_clk_src *cs, struct m0_dtm0_ts *ts)
{
	m0_time_t now = 0;
	int       nr_retries = 0;

	M0_PRE(cs);
	M0_PRE(M0_IN(cs->cs_ops, (&cs_phys_ops)));

	m0_mutex_lock(&cs->cs_phys_lock);

	now = m0_time_now();

	while (unlikely(now == cs->cs_prev.dts_phys)) {
		/* Since the soft limit (ADV_NS) is relatively small,
		 * we do not have to handle the case where nanosleep()
		 * was interrupted.
		 */
		m0_nanosleep(CS_PHYS_NOW_ADV_NS, NULL);
		nr_retries++;
		now = m0_time_now();
		/* Assumption: the clock should advance eventually. */
		M0_ASSERT_INFO(nr_retries < CS_PHYS_NOW_MAX_RETRIES,
			       "The time has been frozen for too long.");
	}

	/* Assumptions:
	 * 1. cs_phys_now() never returns duplicates.
	 * 2. nanosleep() moves the clock forward.
	 */
	M0_POST(cs->cs_prev.dts_phys < now);

	ts->dts_phys = now;
	cs->cs_prev = *ts;

	m0_mutex_unlock(&cs->cs_phys_lock);
	M0_POST(m0_dtm0_ts__invariant(ts));
	return 0;
}

static int cs_phys_observed(struct m0_dtm0_clk_src  *cs,
			    const struct m0_dtm0_ts *other_ts)
{
	struct m0_dtm0_ts now = M0_DTM0_TS_INIT;
	int              rc;

	M0_PRE(m0_dtm0_ts__invariant(other_ts));

	rc = m0_dtm0_clk_src_now(cs, &now);
	if (rc != 0)
		return rc;

	M0_ASSERT(m0_dtm0_ts__invariant(&now));

	rc = m0_dtm0_ts_cmp(cs, &now, other_ts);

	/* Assumption:
	 *   It is possible to observe an even that happened in the past.
	 * The other cases (present, future) are not allowed, and they
	 * indicate a significant clock drift.
	 */
	if (rc == M0_DTS_GT) {
		rc = 0;
	} else {
		rc = M0_ERR_INFO(-EINVAL, "Observed a future event");
	}

	return rc;
}

static const struct m0_dtm0_clk_src_ops cs_phys_ops = {
	.cmp      = cs_phys_cmp,
	.now      = cs_phys_now,
	.observed = cs_phys_observed,
};

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
/*
 * vim: tabstop=8 shiftwidth=8 noexpandtab textwidth=80 nowrap
 */
