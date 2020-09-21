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


#include "errno.h"
#include "stdio.h"

#include "lib/memory.h"
#include "fop/fop.h"
#include "rpc/rpclib.h"
#include "stats/stats_fops.h"
#include "stats/stats_fops_xc.h"

#define M0_TRACE_SUBSYSTEM M0_TRACE_SUBSYS_STATS
#include "lib/trace.h"

static struct m0_stats_recs *stats_recs_dup(struct m0_stats_recs *stats_recs)
{
	int                   i;
	struct m0_stats_recs *recs;

	M0_PRE(stats_recs != NULL);
	M0_PRE(stats_recs->sf_nr > 0 && stats_recs->sf_stats != NULL);

	M0_ALLOC_PTR(recs);
	if (recs == NULL)
		goto error;

	recs->sf_nr = stats_recs->sf_nr;
	M0_ALLOC_ARR(recs->sf_stats, recs->sf_nr);
	if (recs->sf_stats == NULL)
		goto free_recs;

	for (i = 0; i < recs->sf_nr; ++i) {
		/* if stats type not defined. */
		if (stats_recs->sf_stats[i].ss_data.se_nr <= 0)
			continue;

		recs->sf_stats[i].ss_id = stats_recs->sf_stats[i].ss_id;
		recs->sf_stats[i].ss_data.se_nr =
			stats_recs->sf_stats[i].ss_data.se_nr;
		M0_ALLOC_ARR(recs->sf_stats[i].ss_data.se_data,
			     recs->sf_stats[i].ss_data.se_nr);
		if (recs->sf_stats[i].ss_data.se_data == NULL)
			goto free_stats;

		memcpy(recs->sf_stats[i].ss_data.se_data,
		       stats_recs->sf_stats[i].ss_data.se_data,
		       recs->sf_stats[i].ss_data.se_nr * sizeof (uint64_t));
	}

	return recs;

free_stats:
	for(; i >= 0; --i) {
		if (recs->sf_stats[i].ss_data.se_data != NULL)
			m0_free(recs->sf_stats[i].ss_data.se_data);
	}
	m0_free(recs->sf_stats);
free_recs:
	m0_free(recs);
error:
	return NULL;
}

static struct m0_fop *query_fop_alloc(void)
{
	struct m0_fop             *fop;
	struct m0_stats_query_fop *qfop;

	M0_ALLOC_PTR(fop);
	if (fop == NULL)
		goto error;

	M0_ALLOC_PTR(qfop);
	if (qfop == NULL)
		goto free_fop;

	m0_fop_init(fop, &m0_fop_stats_query_fopt, (void *)qfop,
		    m0_stats_query_fop_release);

	return fop;
free_fop:
	m0_free(fop);
error:
	return NULL;
}

int m0_stats_query(struct m0_rpc_session     *session,
		   struct m0_stats_recs     **stats)
{
	int                            rc;
	struct m0_fop                 *fop;
	struct m0_fop                 *rfop;
	struct m0_rpc_item            *item;
	struct m0_stats_query_rep_fop *qrfop;

	M0_PRE(session != NULL);
	M0_PRE(stats != NULL);

	fop = query_fop_alloc();
	if (fop == NULL)
		return M0_ERR(-ENOMEM);

	item = &fop->f_item;
	rc = m0_rpc_post_sync(fop, session, NULL, 0);
	if (rc != 0) {
		m0_fop_put_lock(fop);
		return M0_RC(rc);
	}

	rfop  = m0_rpc_item_to_fop(item->ri_reply);
	qrfop = m0_stats_query_rep_fop_get(rfop);

	*stats = stats_recs_dup(&qrfop->sqrf_stats);

	m0_fop_put_lock(fop);
	return M0_RC(rc);
}

void m0_stats_free(struct m0_stats_recs *stats)
{
	struct m0_xcode_obj obj = {
		.xo_type = m0_stats_recs_xc,
		.xo_ptr  = stats
	};

	m0_xcode_free_obj(&obj);
}

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
