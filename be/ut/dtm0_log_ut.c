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

#define M0_TRACE_SUBSYSTEM M0_TRACE_SUBSYS_DTM
#include "be/dtm0_log.h"
#include "be/list.h"
#include "dtm0/clk_src.h"
#include "lib/errno.h"
#include "lib/misc.h"		/* m0_forall */
#include "ut/ut.h"		/* M0_UT_ASSERT */

#define UT_DTM0_LOG_MAX_PA            3
#define UT_DTM0_LOG_MAX_LOG_REC      10
#define UT_DTM0_LOG_BUF_SIZE        256

void ut_dl_set_pa_state(struct m0_dtm0_tx_pa *pa, uint32_t state)
{
	pa->pa_state = state;
}

void ut_dl_init_pa(struct m0_dtm0_tx_pa *pa, int rand)
{
	m0_fid_set(&pa->pa_fid, rand + 1, rand + 1);
	ut_dl_set_pa_state(pa, M0_DTPS_INPROGRESS);
}

bool ut_dl_verify_pa(struct m0_dtm0_tx_pa *pa, int rand)
{
	struct m0_fid temp_fid;
	m0_fid_set(&temp_fid, rand + 1, rand + 1);

	return (m0_fid_cmp(&pa->pa_fid, &temp_fid) == 0) &&
               pa->pa_state >= 0                         &&
               pa->pa_state < M0_DTPS_NR;
}

void ut_dl_init_tid(struct m0_dtm0_tid *tid, int rand)
{
	m0_fid_set(&tid->dti_fid, rand + 1, rand + 1);
	tid->dti_ts.dts_phys = rand + 1;
}

bool ut_dl_verify_tid(struct m0_dtm0_tid *tid, int rand)
{
	struct m0_fid temp_fid;
	m0_fid_set(&temp_fid, rand + 1, rand + 1);

	return (m0_fid_cmp(&tid->dti_fid, &temp_fid) == 0) &&
               (tid->dti_ts.dts_phys == rand + 1);
}

int ut_dl_init_txd(struct m0_dtm0_tx_desc *txd, int rand)
{
	int i;
	int rc;

	rc = m0_dtm0_tx_desc_init(txd, UT_DTM0_LOG_MAX_PA);
	if (rc != 0)
		return rc;

	ut_dl_init_tid(&txd->dtd_id, rand);

	for (i = 0; i < txd->dtd_pg.dtpg_nr; ++i) {
		ut_dl_init_pa(&txd->dtd_pg.dtpg_pa[i], rand);
	}

	return rc;
}

void ut_dl_fini_txd(struct m0_dtm0_tx_desc *txd)
{
	m0_dtm0_tx_desc_fini(txd);
}

bool ut_dl_verify_txd(struct m0_dtm0_tx_desc *txd, int rand)
{
	return ut_dl_verify_tid(&txd->dtd_id, rand)        &&
               (txd->dtd_pg.dtpg_nr == UT_DTM0_LOG_MAX_PA) &&
	       m0_forall(i, txd->dtd_pg.dtpg_nr,
                         ut_dl_verify_pa(&txd->dtd_pg.dtpg_pa[i], rand));
}

int ut_dl_init_buf(struct m0_buf *buf, int rand)
{
	int rc;
	rc = m0_buf_alloc(buf, UT_DTM0_LOG_BUF_SIZE);
	if (rc != 0)
		return rc;
	memset(buf->b_addr, rand + 1, UT_DTM0_LOG_BUF_SIZE);
	return rc;
}

void ut_dl_fini_buf(struct m0_buf *buf)
{
	m0_buf_free(buf);
}

bool ut_dl_verify_buf(struct m0_buf *buf, int rand)
{
	bool          rc       = true;
	struct m0_buf temp_buf = {};

	if (buf->b_nob) {
		ut_dl_init_buf(&temp_buf, rand);
		rc = m0_buf_eq(&temp_buf, buf);
		m0_buf_free(&temp_buf);
	} else {
		rc = (m0_buf_is_set(buf) == 0);
	}

	return rc;
}

int ut_dl_init(struct m0_dtm0_tx_desc *txd, struct m0_buf *buf, int rand)
{
	int rc;

	rc = ut_dl_init_txd(txd, rand);
	if (rc != 0)
		return rc;

	rc = ut_dl_init_buf(buf, rand);
	if (rc != 0)
		m0_dtm0_tx_desc_fini(txd);

	return rc;
}

void ut_dl_fini(struct m0_dtm0_tx_desc *txd, struct m0_buf *buf)
{
	ut_dl_fini_txd(txd);
	ut_dl_fini_buf(buf);
}

bool ut_dl_verify_log_rec(struct m0_dtm0_log_rec *rec, int rand)
{
	return ut_dl_verify_buf(&rec->dlr_pyld, rand) &&
               ut_dl_verify_txd(&rec->dlr_txd, rand);
}


void test_volatile_dtm0_log(void)
{
	int                     i;
	int                     rc;
	struct m0_dtm0_clk_src  cs;
	struct m0_dtm0_tx_desc  txd[UT_DTM0_LOG_MAX_LOG_REC];
	struct m0_buf           buf[UT_DTM0_LOG_MAX_LOG_REC] = {};
	struct m0_buf           empty_buf                    = {};
	struct m0_dtm0_log_rec *rec[UT_DTM0_LOG_MAX_LOG_REC] = {};
	struct m0_be_dtm0_log  *log                          = NULL;

	rc = m0_dtm0_clk_src_init(&cs, M0_DTM0_CS_PHYS);
	M0_UT_ASSERT(rc == 0);

	rc = m0_be_dtm0_log_init(&log, &cs, true);
	M0_UT_ASSERT(rc == 0);

	for (i = 0; i < UT_DTM0_LOG_MAX_LOG_REC; ++i) {
		rc = ut_dl_init(&txd[i], &buf[i], i);
		M0_UT_ASSERT(rc == 0);
	}

	/* pa[0].st = EX pa[1].st = IP pa[2].st = IP, pyld = valid */
	ut_dl_set_pa_state(&txd[0].dtd_pg.dtpg_pa[0], M0_DTPS_EXECUTED);
	m0_mutex_lock(&log->dl_lock);
	rc = m0_be_dtm0_log_update(log, NULL, &txd[0], &buf[0]);
	M0_UT_ASSERT(rc == 0);
	rec[0] = m0_be_dtm0_log_find(log, &txd[0].dtd_id);
	M0_UT_ASSERT(rec[0] != NULL);
	rc = ut_dl_verify_log_rec(rec[0], 0);
	M0_UT_ASSERT(rc != 0);

	ut_dl_set_pa_state(&txd[0].dtd_pg.dtpg_pa[1], M0_DTPS_PERSISTENT);
	ut_dl_set_pa_state(&txd[0].dtd_pg.dtpg_pa[2], M0_DTPS_PERSISTENT);
	rc = m0_be_dtm0_log_update(log, NULL, &txd[0], &buf[0]);
	M0_UT_ASSERT(rc == 0);
	rec[0] = m0_be_dtm0_log_find(log, &txd[0].dtd_id);
	M0_UT_ASSERT(rec[0] != NULL);
	rc = ut_dl_verify_log_rec(rec[0], 0);
	M0_UT_ASSERT(rc != 0);

	rc = m0_be_dtm0_log_prune(log, NULL, &txd[0].dtd_id);
	M0_UT_ASSERT(rc == -EPROTO);

	ut_dl_set_pa_state(&txd[0].dtd_pg.dtpg_pa[0], M0_DTPS_PERSISTENT);
	rc = m0_be_dtm0_log_update(log, NULL, &txd[0], &buf[0]);
	M0_UT_ASSERT(rc == 0);

	rc = m0_be_dtm0_log_prune(log, NULL, &txd[0].dtd_id);
	M0_UT_ASSERT(rc == 0);

	rec[0] = m0_be_dtm0_log_find(log, &txd[0].dtd_id);
	M0_UT_ASSERT(rec[0] == NULL);

	rc = m0_be_dtm0_log_prune(log, NULL, &txd[0].dtd_id);
	M0_UT_ASSERT(rc == -ENOENT);

	ut_dl_set_pa_state(&txd[0].dtd_pg.dtpg_pa[0], M0_DTPS_INPROGRESS);
	ut_dl_set_pa_state(&txd[0].dtd_pg.dtpg_pa[1], M0_DTPS_INPROGRESS);
	ut_dl_set_pa_state(&txd[0].dtd_pg.dtpg_pa[2], M0_DTPS_PERSISTENT);
	rc = m0_be_dtm0_log_update(log, NULL, &txd[0], &empty_buf);
	M0_UT_ASSERT(rc == 0);
	rec[0] = m0_be_dtm0_log_find(log, &txd[0].dtd_id);
	M0_UT_ASSERT(rec[0] != NULL);
	rc = ut_dl_verify_log_rec(rec[0], 0);
	M0_UT_ASSERT(rc != 0);

	ut_dl_set_pa_state(&txd[0].dtd_pg.dtpg_pa[0], M0_DTPS_PERSISTENT);
	ut_dl_set_pa_state(&txd[0].dtd_pg.dtpg_pa[1], M0_DTPS_PERSISTENT);
	rc = m0_be_dtm0_log_update(log, NULL, &txd[0], &buf[0]);
	M0_UT_ASSERT(rc == 0);
	rec[0] = m0_be_dtm0_log_find(log, &txd[0].dtd_id);
	M0_UT_ASSERT(rec[0] != NULL);
	rc = ut_dl_verify_log_rec(rec[0], 0);
	M0_UT_ASSERT(rc != 0);
	rc = m0_be_dtm0_log_prune(log, NULL, &txd[0].dtd_id);
	M0_UT_ASSERT(rc == 0);
	rec[0] = NULL;

	for (i = 0; i < UT_DTM0_LOG_MAX_LOG_REC; ++i) {
		ut_dl_set_pa_state(&txd[i].dtd_pg.dtpg_pa[0],
                                   M0_DTPS_PERSISTENT);
		ut_dl_set_pa_state(&txd[i].dtd_pg.dtpg_pa[1],
                                   M0_DTPS_PERSISTENT);
		ut_dl_set_pa_state(&txd[i].dtd_pg.dtpg_pa[2],
                                   M0_DTPS_PERSISTENT);
		rc = m0_be_dtm0_log_update(log, NULL, &txd[i], &buf[i]);
		M0_UT_ASSERT(rc == 0);
		rec[i] = m0_be_dtm0_log_find(log, &txd[i].dtd_id);
		M0_UT_ASSERT(rec[i] != NULL);
		rc = ut_dl_verify_log_rec(rec[i], i);
		M0_UT_ASSERT(rc != 0);
		rec[i] = NULL;
	}

	rec[5] = m0_be_dtm0_log_find(log, &txd[5].dtd_id);
	M0_UT_ASSERT(rec[5] != NULL);
	rc = ut_dl_verify_log_rec(rec[5], 5);
	M0_UT_ASSERT(rc != 0);
	rec[5] = NULL;

	rc = m0_be_dtm0_log_prune(log, NULL, &txd[9].dtd_id);
	M0_UT_ASSERT(rc == 0);
	rec[9] = m0_be_dtm0_log_find(log, &txd[9].dtd_id);
	M0_UT_ASSERT(rec[9] == NULL);

	/* Finalization */
	for (i = 0; i < UT_DTM0_LOG_MAX_LOG_REC; ++i) {
		ut_dl_fini(&txd[i], &buf[i]);
	}

	m0_mutex_unlock(&log->dl_lock);
	m0_be_dtm0_log_fini(&log, true);
	m0_dtm0_clk_src_fini(&cs);
}

struct m0_ut_suite dtm0_log_ut = {
	.ts_name   = "dtm0-log-ut",
	.ts_init   = NULL,
	.ts_fini   = NULL,
	.ts_tests  = {
		{ "dtm0-log-list", test_volatile_dtm0_log},
		{ NULL, NULL }
	}
};

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
/*
 * vim: tabstop=8 shiftwidth=8 noexpandtab textwidth=80 nowrap
 */
