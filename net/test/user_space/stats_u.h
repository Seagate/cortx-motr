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


#pragma once

#ifndef __MOTR_NET_TEST_USER_SPACE_STATS_U_H__
#define __MOTR_NET_TEST_USER_SPACE_STATS_U_H__

#include "lib/time.h"		/* m0_time_t */
#include "net/test/stats.h"	/* m0_net_test_stats */

/**
   @addtogroup NetTestStatsDFS

   @{
 */

struct m0_net_test_stats;

/**
   Get sum of all elements from sample.
   @pre m0_net_test_stats_invariant(stats)
 */
double m0_net_test_stats_sum(const struct m0_net_test_stats *stats);

/**
   Get sample average (arithmetic mean).
   @pre m0_net_test_stats_invariant(stats)
 */
double m0_net_test_stats_avg(const struct m0_net_test_stats *stats);

/**
   Get sample standard deviation.
   @pre m0_net_test_stats_invariant(stats)
 */
double m0_net_test_stats_stddev(const struct m0_net_test_stats *stats);

/**
   @see m0_net_test_stats_time_add()
 */
m0_time_t m0_net_test_stats_time_sum(struct m0_net_test_stats *stats);

/**
   @see m0_net_test_stats_time_add()
 */
m0_time_t m0_net_test_stats_time_avg(struct m0_net_test_stats *stats);

/**
   @see m0_net_test_stats_time_add()
 */
m0_time_t m0_net_test_stats_time_stddev(struct m0_net_test_stats *stats);

/**
   @} end of NetTestStatsDFS group
 */

#endif /*  __MOTR_NET_TEST_USER_SPACE_STATS_U_H__ */

/*
 *  Local variables:
 *  c-indentation-style: "K&R"
 *  c-basic-offset: 8
 *  tab-width: 8
 *  fill-column: 79
 *  scroll-step: 1
 *  End:
 */
