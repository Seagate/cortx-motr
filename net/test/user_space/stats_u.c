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


#include <math.h>	/* sqrt */
#include "lib/types.h"	/* UINT64_MAX */
#include "lib/assert.h"	/* M0_PRE */

#include "net/test/stats.h"

/**
   @addtogroup NetTestStatsInternals

   @see
   @ref net-test

   @{
 */

static double double_get(const struct m0_uint128 *v128)
{
	return v128->u_lo * 1. + v128->u_hi * ((double)UINT64_MAX + 1.);
}

double m0_net_test_stats_sum(const struct m0_net_test_stats *stats)
{
	M0_PRE(m0_net_test_stats_invariant(stats));

	return double_get(&stats->nts_sum);
}

double m0_net_test_stats_avg(const struct m0_net_test_stats *stats)
{
	double sum;

	M0_PRE(m0_net_test_stats_invariant(stats));

	sum = double_get(&stats->nts_sum);
	return stats->nts_count == 0 ? 0. : sum / stats->nts_count;
}

double m0_net_test_stats_stddev(const struct m0_net_test_stats *stats)
{
	double mean;
	double stddev;
	double N;
	double sum_sqr;

	M0_PRE(m0_net_test_stats_invariant(stats));

	if (stats->nts_count == 0 || stats->nts_count == 1)
		return 0.;

	mean	= m0_net_test_stats_avg(stats);
	N	= stats->nts_count;
	sum_sqr	= double_get(&stats->nts_sum_sqr);
	stddev	= (sum_sqr - N * mean * mean) / (N - 1.);
	stddev  = stddev < 0. ? 0. : stddev;
	stddev	= sqrt(stddev);
	return stddev;
}

static m0_time_t double2m0_time_t(double value)
{
	uint64_t seconds;
	uint64_t nanoseconds;

	seconds	    = (uint64_t) floor(value / M0_TIME_ONE_SECOND);
	nanoseconds = (uint64_t) (value - seconds * M0_TIME_ONE_SECOND);
	return m0_time(seconds, nanoseconds);
}

m0_time_t m0_net_test_stats_time_sum(struct m0_net_test_stats *stats)
{
	return double2m0_time_t(m0_net_test_stats_sum(stats));
}

m0_time_t m0_net_test_stats_time_avg(struct m0_net_test_stats *stats)
{
	return double2m0_time_t(m0_net_test_stats_avg(stats));
}

m0_time_t m0_net_test_stats_time_stddev(struct m0_net_test_stats *stats)
{
	return double2m0_time_t(m0_net_test_stats_stddev(stats));
}

/**
   @} end of NetTestStatsInternals group
 */

/*
 *  Local variables:
 *  c-indentation-style: "K&R"
 *  c-basic-offset: 8
 *  tab-width: 8
 *  fill-column: 79
 *  scroll-step: 1
 *  End:
 */
