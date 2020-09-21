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

#ifndef __MOTR_STATS_STATS_FOPS_H__
#define __MOTR_STATS_STATS_FOPS_H__

#include "lib/types.h"
#include "xcode/xcode_attr.h"

/**
 * @defgroup stats_fop Stats FOP
 * @{
 */

struct m0_fop;
struct m0_ref;

extern struct m0_fop_type m0_fop_stats_update_fopt;
extern struct m0_fop_type m0_fop_stats_query_fopt;
extern struct m0_fop_type m0_fop_stats_query_rep_fopt;

/* @note Same fop definations will be defined from monitoring infra
 *       Need to merge these changes properly.
 *       Please remove tis note after merge.
 */

struct m0_uint64_seq {
	uint32_t  se_nr;
	/** Stats summary data */
	uint64_t *se_data;
} M0_XCA_SEQUENCE M0_XCA_DOMAIN(rpc);

/**
 * Motr nodes send sequence of m0_stats_sum.
 */
struct m0_stats_sum {
	uint32_t             ss_id;
	/** Stats summary data */
	struct m0_uint64_seq ss_data;
} M0_XCA_RECORD M0_XCA_DOMAIN(rpc);

struct m0_stats_recs {
	/** Stats sequence length */
	uint64_t	      sf_nr;
	/** Stats sequence data */
	struct m0_stats_sum  *sf_stats;
} M0_XCA_SEQUENCE M0_XCA_DOMAIN(rpc);

/** stats update fop */
struct m0_stats_update_fop {
	struct m0_stats_recs suf_stats;
} M0_XCA_RECORD M0_XCA_DOMAIN(rpc);

/** stats query fop */
struct m0_stats_query_fop {
	/** Stats ids */
	struct m0_uint64_seq sqf_ids;
} M0_XCA_RECORD M0_XCA_DOMAIN(rpc);

/** stats query reply fop */
struct m0_stats_query_rep_fop {
	int32_t              sqrf_rc;
	struct m0_stats_recs sqrf_stats;
} M0_XCA_RECORD M0_XCA_DOMAIN(rpc);

/**
 * Get stats update fop
 * @param fop pointer to fop
 * @return pointer to stats update fop.
 */
M0_INTERNAL struct m0_stats_update_fop *
m0_stats_update_fop_get(struct m0_fop *fop);

/**
 * Get stats query fop
 * @param fop pointer to fop
 * @return pointer to stats query fop.
 */
M0_INTERNAL struct m0_stats_query_fop *
m0_stats_query_fop_get(struct m0_fop *fop);

/**
 * Get stats query reply fop
 * @param fop pointer to fop
 * @return pointer to stats query reply fop.
 */
M0_INTERNAL struct m0_stats_query_rep_fop *
m0_stats_query_rep_fop_get(struct m0_fop *fop);

/**
 * m0_stats_query_fop_release
 */
M0_INTERNAL void m0_stats_query_fop_release(struct m0_ref *ref);

M0_INTERNAL int  m0_stats_fops_init(void);
M0_INTERNAL void m0_stats_fops_fini(void);

/** @} end group stats_fop */
#endif /* __MOTR_STATS_STATS_FOPS_H_ */
/*
 *  Local variables:
 *  c-indentation-style: "K&R"
 *  c-basic-offset: 8
 *  tab-width: 8
 *  fill-column: 80
 *  scroll-step: 1
 *  End:
 */
