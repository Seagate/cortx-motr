/* -*- C -*- */
/*
 * Copyright (c) 2016-2020 Seagate Technology LLC and/or its Affiliates
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
 * @addtogroup addb2
 *
 * @{
 */

#define M0_TRACE_SUBSYSTEM M0_TRACE_SUBSYS_ADDB
#include "lib/trace.h"
#include "lib/errno.h"                    /* ENOENT */
#include "addb2/histogram.h"
#include "addb2/internal.h"               /* m0_addb2__counter_snapshot */

static const struct m0_addb2_sensor_ops hist_ops;

void m0_addb2_hist_add(struct m0_addb2_hist *hist, int64_t min, int64_t max,
		       uint64_t label, int idx)
{
	struct m0_addb2_counter *c = &hist->hi_counter;

	M0_PRE(M0_IS0(hist));
	M0_PRE(max > min);

	m0_addb2__counter_data_init(&c->co_val);
	hist->hi_data.hd_min = min;
	hist->hi_data.hd_max = max;
	m0_addb2_sensor_add(&c->co_sensor, label, VALUE_MAX_NR, idx, &hist_ops);
}

void m0_addb2_hist_add_auto(struct m0_addb2_hist *hist, int skip,
			    uint64_t label, int idx)
{
	struct m0_addb2_counter *c = &hist->hi_counter;

	M0_PRE(M0_IS0(hist));
	M0_PRE(skip > 0);

	m0_addb2__counter_data_init(&c->co_val);
	hist->hi_skip = skip;
	m0_addb2_sensor_add(&c->co_sensor, label, VALUE_MAX_NR, idx, &hist_ops);
}

void m0_addb2_hist_del(struct m0_addb2_hist *hist)
{
	m0_addb2_sensor_del(&hist->hi_counter.co_sensor);
	M0_SET0(hist);
}

void m0_addb2_hist_mod(struct m0_addb2_hist *hist, int64_t val)
{
	m0_addb2_hist_mod_with(hist, val, 0);
}

void m0_addb2_hist_mod_with(struct m0_addb2_hist *hist,
			    int64_t val, uint64_t datum)
{
	struct m0_addb2_hist_data *hd = &hist->hi_data;

	if (hist->hi_skip > 0) {
		hd->hd_min = min64(hd->hd_min, val);
		hd->hd_max = max64(hd->hd_max, val);
		hist->hi_skip--;
	} else
		hist->hi_data.hd_bucket[m0_addb2_hist_bucket(hist, val)]++;
	m0_addb2_counter_mod_with(&hist->hi_counter, val, datum);
}

int m0_addb2_hist_bucket(const struct m0_addb2_hist *hist, int64_t val)
{
	const struct m0_addb2_hist_data *hd = &hist->hi_data;
	int                              idx;

	if (val < hd->hd_min)
		idx = 0;
	else if (val >= hd->hd_max)
		idx = M0_ADDB2_HIST_BUCKETS - 1;
	else
		idx = (val - hd->hd_min) * (M0_ADDB2_HIST_BUCKETS - 2) /
			(hd->hd_max - hd->hd_min) + 1;
	M0_POST(0 <= idx && idx < M0_ADDB2_HIST_BUCKETS);
	return idx;
}

static int hist_snapshot(struct m0_addb2_sensor *s, uint64_t *area)
{
	struct m0_addb2_hist      *hist = M0_AMB(hist, s, hi_counter.co_sensor);
	struct m0_addb2_hist_data *hd   = &hist->hi_data;
	int                        i;
	int                        result;

	if (hist->hi_skip > 0)
		return -ENOENT;
	result = m0_addb2__counter_snapshot(s, area);
	if (result < 0)
		return result;
	area += M0_ADDB2_COUNTER_VALS;
	*(struct m0_addb2_hist_data *)area = *hd;
	for (i = 0; i < ARRAY_SIZE(hd->hd_bucket); ++i)
		hd->hd_bucket[i] = 0;
	return 0;
}

static void hist_fini(struct m0_addb2_sensor *s)
{;}

static const struct m0_addb2_sensor_ops hist_ops = {
	.so_snapshot = &hist_snapshot,
	.so_fini     = &hist_fini
};

#undef M0_TRACE_SUBSYSTEM

/** @} end of addb2 group */

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
