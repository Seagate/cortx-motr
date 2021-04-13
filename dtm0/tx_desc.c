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



/**
 * @addtogroup dtm
 *
 * @{
 */

#include "dtm0/clk_src.h"
#include "lib/misc.h"
#define M0_TRACE_SUBSYSTEM M0_TRACE_SUBSYS_DTM0
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
	/*
	 * Assumption: a valid tx_desc should have
	 * pre-allocated array of participants AND
	 * the states should be inside the range [0, nr) AND
	 * if all PAs are moved past INIT state then
	 * the descriptor should have a valid ID.
	 */

	return _0C(td->dtd_ps.dtp_pa != NULL) &&
		_0C(m0_forall(i, td->dtd_ps.dtp_nr,
			      td->dtd_ps.dtp_pa[i].p_state >= 0 &&
			      td->dtd_ps.dtp_pa[i].p_state < M0_DTPS_NR)) &&
		_0C(ergo(m0_forall(i, td->dtd_ps.dtp_nr,
			      td->dtd_ps.dtp_pa[i].p_state > M0_DTPS_INIT),
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
	int rc;

	M0_ENTRY();

	M0_PRE(m0_dtm0_tx_desc__invariant(src));

	rc = m0_dtm0_tx_desc_init(dst, src->dtd_ps.dtp_nr);
	if (rc == 0) {
		dst->dtd_id = src->dtd_id;
		memcpy(dst->dtd_ps.dtp_pa, src->dtd_ps.dtp_pa,
		       sizeof(src->dtd_ps.dtp_pa[0]) * src->dtd_ps.dtp_nr);

		M0_POST(m0_dtm0_tx_desc__invariant(dst));
	}

	return M0_RC(rc);
}

M0_INTERNAL int m0_dtm0_tx_desc_init(struct m0_dtm0_tx_desc *td,
				     uint32_t nr_pa)
{
	M0_ENTRY();

	M0_ALLOC_ARR(td->dtd_ps.dtp_pa, nr_pa);
	if (td->dtd_ps.dtp_pa == NULL)
		return M0_ERR(-ENOMEM);
	td->dtd_ps.dtp_nr = nr_pa;
	M0_POST(m0_dtm0_tx_desc__invariant(td));

	return M0_RC(0);
}

M0_INTERNAL void m0_dtm0_tx_desc_fini(struct m0_dtm0_tx_desc *td)
{
	M0_ENTRY();
	m0_free(td->dtd_ps.dtp_pa);
	M0_SET0(td);
	M0_LEAVE();
}

M0_INTERNAL int m0_dtm0_tid_cmp(struct m0_dtm0_clk_src   *cs,
				const struct m0_dtm0_tid *left,
				const struct m0_dtm0_tid *right)
{
	return m0_dtm0_ts_cmp(cs, &left->dti_ts, &right->dti_ts) ?:
		m0_fid_cmp(&left->dti_fid, &right->dti_fid);
}

M0_INTERNAL void m0_dtm0_tx_desc_apply(struct m0_dtm0_tx_desc *tgt,
				       const struct m0_dtm0_tx_desc *upd)
{
	int                   i;
	struct m0_dtm0_tx_pa *tgt_pa;
	struct m0_dtm0_tx_pa *upd_pa;

	M0_ENTRY();

	M0_PRE(m0_dtm0_tx_desc__invariant(tgt));
	M0_PRE(m0_dtm0_tx_desc__invariant(upd));
	M0_PRE(memcmp(&tgt->dtd_id, &upd->dtd_id, sizeof(tgt->dtd_id)) == 0);
	M0_PRE(upd->dtd_ps.dtp_nr == tgt->dtd_ps.dtp_nr);
	M0_PRE(m0_forall(i, upd->dtd_ps.dtp_nr,
			 m0_fid_cmp(&tgt->dtd_ps.dtp_pa[i].p_fid,
				    &upd->dtd_ps.dtp_pa[i].p_fid) == 0));

	for (i = 0; i < upd->dtd_ps.dtp_nr; ++i) {
		tgt_pa = &tgt->dtd_ps.dtp_pa[i];
		upd_pa = &upd->dtd_ps.dtp_pa[i];

		tgt_pa->p_state = max_check(tgt_pa->p_state,
					     upd_pa->p_state);
	}

	M0_LEAVE();
}

M0_INTERNAL bool m0_dtm0_tx_desc_state_eq(const struct m0_dtm0_tx_desc *txd,
					  enum m0_dtm0_tx_pa_state      state)
{
	return m0_forall(i, txd->dtd_ps.dtp_nr,
			 txd->dtd_ps.dtp_pa[i].p_state == state);
}

/* XXX: This function has only one purpose -- to aid debugging with GDB */
M0_INTERNAL void m0_dtm0_tx_desc_print(const struct m0_dtm0_tx_desc *txd)
{
	int i;

	static const struct {
		char name;
	} smap[M0_DTPS_NR] = {
		[M0_DTPS_INIT       ] = { .name = '0' },
		[M0_DTPS_INPROGRESS ] = { .name = 'I' },
		[M0_DTPS_EXECUTED   ] = { .name = 'E' },
		[M0_DTPS_PERSISTENT ] = { .name = 'P' },
	};

	M0_LOG(M0_ALWAYS, "txid=" DTID0_F, DTID0_P(&txd->dtd_id));

	for (i = 0; i < txd->dtd_ps.dtp_nr; ++i) {
		M0_LOG(M0_ALWAYS, "\tpa=" FID_F ", %c",
		       FID_P(&txd->dtd_ps.dtp_pa[i].p_fid),
		       smap[txd->dtd_ps.dtp_pa[i].p_state].name);
	}
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
