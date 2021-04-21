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

#include "dtm0/clk_src.h"
#include "lib/assert.h" /* M0_PRE */


struct m0_dtm0_clk_src_ops {
	int  (*cso_cmp)(const struct m0_dtm0_ts *left,
			const struct m0_dtm0_ts *right);
	void (*cso_now)(struct m0_dtm0_clk_src *cs, struct m0_dtm0_ts *ts);
};

/* See M0_DTM0_CS_PHYS for details */
static const struct m0_dtm0_clk_src_ops cs_phys_ops;

M0_INTERNAL void m0_dtm0_clk_src_init(struct m0_dtm0_clk_src *cs,
				      enum m0_dtm0_cs_types   type)
{
	M0_ENTRY();
	M0_PRE(M0_IN(type, (M0_DTM0_CS_PHYS)));
	m0_mutex_init(&cs->cs_phys_lock);
	cs->cs_last = M0_DTM0_TS_MIN;
	cs->cs_ops = &cs_phys_ops;
	M0_LEAVE();
}

M0_INTERNAL void m0_dtm0_clk_src_fini(struct m0_dtm0_clk_src *cs)
{
	M0_ENTRY();
	M0_PRE(cs);
	M0_PRE(M0_IN(cs->cs_ops, (&cs_phys_ops)));
	m0_mutex_fini(&cs->cs_phys_lock);
	cs->cs_last = M0_DTM0_TS_INIT;
	cs->cs_ops = NULL;
	M0_LEAVE();
}

M0_INTERNAL
enum m0_dtm0_ts_ord m0_dtm0_ts_cmp(const struct m0_dtm0_clk_src *cs,
				   const struct m0_dtm0_ts      *left,
				   const struct m0_dtm0_ts      *right)
{
	M0_PRE(cs);
	M0_PRE(m0_dtm0_ts__invariant(left));
	M0_PRE(m0_dtm0_ts__invariant(right));
	return cs->cs_ops->cso_cmp(left, right);
}

M0_INTERNAL void m0_dtm0_clk_src_now(struct m0_dtm0_clk_src *cs,
				     struct m0_dtm0_ts      *now)
{
	M0_PRE(cs != NULL);
	cs->cs_ops->cso_now(cs, now);
	M0_POST(m0_dtm0_ts__invariant(now));
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

static void cs_phys_now(struct m0_dtm0_clk_src *cs, struct m0_dtm0_ts *now)
{
	M0_PRE(M0_IN(cs->cs_ops, (&cs_phys_ops)));

	m0_mutex_lock(&cs->cs_phys_lock);

	now->dts_phys = m0_time_now();
	if (now->dts_phys > cs->cs_last.dts_phys) {
		cs->cs_last = *now;
	} else {
		cs->cs_last.dts_phys++;
		*now = cs->cs_last;
	}

	m0_mutex_unlock(&cs->cs_phys_lock);
}

static const struct m0_dtm0_clk_src_ops cs_phys_ops = {
	.cso_cmp      = cs_phys_cmp,
	.cso_now      = cs_phys_now,
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
