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

#include "lib/assert.h"
#include "lib/locality.h"

#include "dtm/update.h"
#include "dtm/ltx.h"

static void ltx_persistent_hook(const struct m0_be_tx *tx);
static const struct m0_dtm_history_ops ltx_ops;
static const struct m0_uint128 ltxid = M0_UINT128(0x10ca1, 0x10ca1);

M0_INTERNAL void m0_dtm_ltx_init(struct m0_dtm_ltx *ltx, struct m0_dtm *dtm,
				 struct m0_be_domain *dom)
{
	m0_dtm_controlh_init(&ltx->lx_ch, dtm);
	ltx->lx_ch.ch_history.h_ops = &ltx_ops;
	ltx->lx_ch.ch_history.h_hi.hi_flags |= M0_DHF_OWNED;
	ltx->lx_ch.ch_history.h_rem = NULL;
	ltx->lx_dom = dom;
}

M0_INTERNAL void m0_dtm_ltx_open(struct m0_dtm_ltx *ltx)
{
	m0_be_tx_init(&ltx->lx_tx, 0, ltx->lx_dom, m0_locality_here()->lo_grp,
		      &ltx_persistent_hook, NULL, NULL, NULL);
}

M0_INTERNAL void m0_dtm_ltx_close(struct m0_dtm_ltx *ltx)
{
	m0_dtm_controlh_close(&ltx->lx_ch);
	m0_dtm_oper_done(&ltx->lx_ch.ch_clop, NULL);
	m0_be_tx_close(&ltx->lx_tx);
}

M0_INTERNAL void m0_dtm_ltx_fini(struct m0_dtm_ltx *ltx)
{
	m0_dtm_controlh_fini(&ltx->lx_ch);
	m0_be_tx_fini(&ltx->lx_tx);
}

M0_INTERNAL void m0_dtm_ltx_add(struct m0_dtm_ltx *ltx,
				struct m0_dtm_oper *oper)
{
	m0_dtm_controlh_add(&ltx->lx_ch, oper);
}

static void ltx_persistent_hook(const struct m0_be_tx *tx)
{
	struct m0_dtm_ltx *ltx = M0_AMB(ltx, tx, lx_tx);
	M0_ASSERT(ltx->lx_ch.ch_history.h_hi.hi_flags & M0_DHF_CLOSED);
	m0_dtm_history_persistent(&ltx->lx_ch.ch_history, ~0ULL);
}

static int ltx_find(struct m0_dtm *dtm, const struct m0_dtm_history_type *ht,
		    const struct m0_uint128 *id,
		    struct m0_dtm_history **out)
{
	M0_IMPOSSIBLE("Looking for ltx?");
	return 0;
}

static const struct m0_dtm_history_type_ops ltx_htype_ops = {
	.hito_find = ltx_find
};

enum {
	M0_DTM_HTYPE_LTX = 7
};

M0_INTERNAL const struct m0_dtm_history_type m0_dtm_ltx_htype = {
	.hit_id   = M0_DTM_HTYPE_LTX,
	.hit_name = "local transaction",
	.hit_ops  = &ltx_htype_ops
};

static const struct m0_uint128 *ltx_id(const struct m0_dtm_history *history)
{
	return &ltxid;
}

static void ltx_noop(struct m0_dtm_history *history)
{
}

static const struct m0_dtm_history_ops ltx_ops = {
	.hio_type       = &m0_dtm_ltx_htype,
	.hio_id         = &ltx_id,
	.hio_persistent = &ltx_noop,
	.hio_fixed      = &ltx_noop,
	.hio_update     = &m0_dtm_controlh_update
};


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
