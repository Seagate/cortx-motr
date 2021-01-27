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



/**
 * @addtogroup dtm
 *
 * @{
 */

#include "dtm0/clk_src.h"
#include "lib/misc.h"
#define M0_TRACE_SUBSYSTEM M0_TRACE_SUBSYS_DTM
#include "dtm0/tx_desc.h"
#include "lib/assert.h" /* M0_PRE */
#include "lib/memory.h" /* M0_ALLOC */
#include "lib/errno.h"  /* ENOMEM */
#include "lib/trace.h"  /* M0_ERR */

M0_INTERNAL void m0_dtm0_tx_desc_init_none(struct m0_dtm0_tx_desc *td)
{
	M0_SET0(td);
}

M0_INTERNAL bool m0_dtm0_tx_desc_is_none(const struct m0_dtm0_tx_desc *td)
{
	return M0_IS0(td);
}

M0_INTERNAL bool m0_dtm0_tx_desc__invariant(const struct m0_dtm0_tx_desc *td)
{
	/* Assumption: a valid tx_desc should have
	 *   pre-allocated array of participants AND
	 *   the states should be inside the range [0, nr) AND
	 *   if all PAs are moved past INIT state then
	 *     the descriptor should have a valid ID.
	 */

	return _0C(td->dtd_pg.dtpg_pa != NULL) &&
		_0C(m0_forall(i, td->dtd_pg.dtpg_nr,
			      td->dtd_pg.dtpg_pa[i].pa_state >= 0 &&
			      td->dtd_pg.dtpg_pa[i].pa_state < M0_DTPS_NR)) &&
		_0C(ergo(m0_forall(i, td->dtd_pg.dtpg_nr,
			      td->dtd_pg.dtpg_pa[i].pa_state > M0_DTPS_INIT),
			 m0_dtm0_tid__invariant(&td->dtd_id)));
}

M0_INTERNAL bool m0_dtm0_tid__invariant(const struct m0_dtm0_tid *tid)
{
	return _0C(m0_dtm0_ts__invariant(&tid->dti_ts)) &&
		   _0C(m0_fid_is_set(&tid->dti_fid)) &&
		   _0C(m0_fid_is_valid(&tid->dti_fid));
}

/* Writes a deep copy of "src" into "dst". */
M0_INTERNAL int m0_dtm0_tx_desc_copy(const struct m0_dtm0_tx_desc *src,
				     struct m0_dtm0_tx_desc       *dst)
{
	int      rc;

	M0_PRE(m0_dtm0_tx_desc__invariant(src));

	rc = m0_dtm0_tx_desc_init(dst, src->dtd_pg.dtpg_nr);
	if (rc != 0)
		return rc;

	dst->dtd_id = src->dtd_id;
	memcpy(dst->dtd_pg.dtpg_pa, src->dtd_pg.dtpg_pa,
	       sizeof(src->dtd_pg.dtpg_pa[0]) * src->dtd_pg.dtpg_nr);

	M0_POST(m0_dtm0_tx_desc__invariant(dst));
	return 0;
}

M0_INTERNAL int m0_dtm0_tx_desc_init(struct m0_dtm0_tx_desc *td,
				     uint32_t nr_pa)
{
	M0_ALLOC_ARR(td->dtd_pg.dtpg_pa, nr_pa);
	if (td->dtd_pg.dtpg_pa == NULL)
		return M0_ERR(-ENOMEM);
	td->dtd_pg.dtpg_nr = nr_pa;
	M0_POST(m0_dtm0_tx_desc__invariant(td));
	return 0;
}

M0_INTERNAL void m0_dtm0_tx_desc_fini(struct m0_dtm0_tx_desc *td)
{
	m0_free(td->dtd_pg.dtpg_pa);
	M0_SET0(td);
}

M0_INTERNAL int m0_dtm0_tid_cmp(struct m0_dtm0_clk_src   *cs,
				const struct m0_dtm0_tid *left,
				const struct m0_dtm0_tid *right)
{
	return m0_dtm0_ts_cmp(cs, &left->dti_ts, &right->dti_ts) ?:
		m0_fid_cmp(&left->dti_fid, &right->dti_fid);
}

M0_INTERNAL int m0_dtm0_txr_rec_is_set(struct m0_buf *pyld)
{
	return m0_buf_is_set(pyld);
}

M0_INTERNAL void m0_dtm0_update_pa_state(enum m0_dtm0_tx_pa_state *dst,
                                         enum m0_dtm0_tx_pa_state *src)
{
	if (*dst < *src)
		*dst = *src;
}

M0_INTERNAL bool m0_dtm0_is_rec_is_stable(struct m0_dtm0_tx_pa_group *pg)
{
	return m0_forall(i, pg->dtpg_nr,
                         pg->dtpg_pa[i].pa_state == M0_DTPS_PERSISTENT);
}

#undef M0_TRACE_SUBSYSTEM

/** @} end of dtm group */

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
