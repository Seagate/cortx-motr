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


#pragma once

#ifndef __MOTR_ADDB2_HISTOGRAM_H__
#define __MOTR_ADDB2_HISTOGRAM_H__

/**
 * @defgroup addb2
 *
 * A histogram (m0_addb2_hist) is a type of addb2 sensor behaving similar to
 * counter (m0_addb2_counter) and, in addition, maintaining a set of "buckets",
 * where each bucket records how many times the counter has had a value in the
 * range associated with the bucket.
 *
 * Whenever histogram is updated with a value (m0_addb2_hist_mod()), usual
 * counter statistics (minimum, maximum, average, dispersion) are updated. In
 * addition, the bucket corresponding to the value is updated too.
 *
 * As any counter, a histogram is reset whenever it is recorded in the trace
 * buffer. That is, histogram data delivered to addb2 CONSUMERS correspond to
 * the updates to the histigram made since the previous data were delivered.
 *
 * The number of buckets in a histogram is globally fixed
 * (M0_ADDB2_HIST_BUCKETS).
 *
 * Distribution of values into buckets is determined by two per-histogram
 * parameters: minimum (m0_addb2_hist_data::hd_min) and maximum
 * (m0_addb2_hist_data::hd_max).
 *
 *
 * @verbatim
 *
 *             min       min + STEP   min + 2*STEP      min + N*STEP
 *              |            |            |                 |
 * .....--------+------------+------------+--.....----------+------------.....
 *
 *   bucket-0       bucket-1     bucket-1          bucket-N   bucket-(N+1)
 *
 * @endverbatim
 *
 * Where N == M0_ADDB2_HIST_BUCKETS - 2 and STEP == (max - min)/N.
 *
 * That is the zeroth bucket is for values less than minimum, the first bucket
 * is for values X such that (min <= X && X < min + STEP) and so on up to the
 * last bucket, which contains the values greater than or equal to min + N*STEP.
 *
 * All calculations are done in integer arithmetic. Because of this, the start
 * of the last bucket (min + N*STEP) can be less than the histogram maximum.
 *
 * There are two ways to specify minimum and maximum. When a histogram is
 * initialised with m0_addb2_hist_add(), the minimum and maximum are supplied as
 * parameters. This is suitable for situations where reasonable values of these
 * parameters are known in advance.
 *
 * When a histogram is initialised with m0_addb2_hist_add_auto(), it remains in
 * "auto-tuning" period for the first "skip" updates (where "skip" is a
 * parameter). During auto-tuning, the histogram behaves exactly as a simple
 * counter and doesn't update buckets. When auto-tuning completes, minimum and
 * maximum values seen during auto-tuning are used to setup buckets. From this
 * point on, buckets are updated. This is suitable for situations where
 * distribution of values is now known in advance, e.g., network latencies.
 *
 * @{
 */

#include "addb2/counter.h"
#include "addb2/internal.h"               /* VALUE_MAX_NR */
#include "lib/types.h"                    /* uint64_t */

enum {
	/**
	 * Addb2 sensor can produce a maximum of VALUE_MAX_NR 64-bit values.
	 *
	 * The counter, embedded in each histogram produces
	 * M0_ADDB2_COUNTER_VALS values. Two more are needed to record minimum
	 * and maximum. The rest can be used for 32-bit buckets.
	 */
	M0_ADDB2_HIST_BUCKETS = 2 * (VALUE_MAX_NR - M0_ADDB2_COUNTER_VALS - 2)
};

M0_BASSERT(M0_ADDB2_HIST_BUCKETS >= 2);

/**
 * Data (in addition to counter data, m0_addb2_counter_data), produced by the
 * histogram.
 */
struct m0_addb2_hist_data {
	/**
	 * Minimum. Once set this is never changed.
	 *
	 * This is set either when histogram is initialised or at the end of
	 * auto-tuning period.
	 */
	int64_t  hd_min;
	/**
	 * Maximum. Once set this is never changed.
	 */
	int64_t  hd_max;
	/** Array of buckets. */
	uint32_t hd_bucket[M0_ADDB2_HIST_BUCKETS];
};

/**
 * Addb2 histogram.
 *
 * Accumulates the same statistics as addb2 counter (m0_addb2_counter), plus an
 * array of value buckets.
 */
struct m0_addb2_hist {
	struct m0_addb2_counter   hi_counter;
	/** Remaining updates in the auto-tuning period. */
	int                       hi_skip;
	struct m0_addb2_hist_data hi_data;
};

void m0_addb2_hist_add(struct m0_addb2_hist *hist, int64_t min, int64_t max,
		       uint64_t label, int idx);
void m0_addb2_hist_add_auto(struct m0_addb2_hist *hist, int skip,
			    uint64_t label, int idx);
void m0_addb2_hist_del(struct m0_addb2_hist *hist);
void m0_addb2_hist_mod(struct m0_addb2_hist *hist, int64_t val);
void m0_addb2_hist_mod_with(struct m0_addb2_hist *hist,
			    int64_t val, uint64_t datum);
int m0_addb2_hist_bucket(const struct m0_addb2_hist *hist, int64_t val);

#define M0_ADDB2_HIST(id, hist, datum, ...)				\
do {									\
	struct m0_addb2_hist *__hist = (hist);				\
	M0_ADDB2_TIMED_0((id), (datum), __VA_ARGS__);			\
	if (__hist != NULL)						\
		m0_addb2_hist_mod_with(__hist, __duration, __datum);	\
} while (0)

/** @} end of addb2 group */
#endif /* __MOTR_ADDB2_HISTOGRAM_H__ */

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
