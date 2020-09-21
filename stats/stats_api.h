/* -*- C -*- */
/*
 * Copyright (c) 2013-2020 Seagate Technology LLC and/or its Affiliates
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

#ifndef __MOTR_STATS_UTIL_STATS_API_H__
#define __MOTR_STATS_UTIL_STATS_API_H__
/**
 * @defgroup stats_api Stats Query API
 * This module provide stats query interfaces. These interfaces are used by
 * motr monitoring/administrating utilities/console.
 *
 * Interfaces
 *   m0_stats_query
 *   m0_stats_free
 *
 * @{
 */
struct m0_uint64_seq;
struct m0_stats_recs;
struct m0_rpc_session;

/**
 * Stats query API
 * It retrive stats from stats service of provided stats ids.
 * @param session   The session to be used for the stats query.
 * @param stats_ids Sequence of stats ids.
 * @param stats On success, stats information is returned here.  It must
 *              be released using m0_stats_free().
 * @retval Pointer to stats return by stats service. This should be freed by
 *         caller after use. It can be freed using m0_stats_free().
 */
int m0_stats_query(struct m0_rpc_session  *session,
		   struct m0_uint64_seq   *stats_ids,
		   struct m0_stats_recs  **stats);

/**
 * Free stats sequence
 * It frees stats sequence returned by m0_stats_query().
 * @param stats Stats sequence.
 */
void m0_stats_free(struct m0_stats_recs *stats);

/** @} end group stats_api */
#endif
/*
 *  Local variables:
 *  c-indentation-style: "K&R"
 *  c-basic-offset: 8
 *  tab-width: 8
 *  fill-column: 80
 *  scroll-step: 1
 *  End:
 */
