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
 * @addtogroup dtm0
 *
 * @{
 */

#define M0_TRACE_SUBSYSTEM M0_TRACE_SUBSYS_UT
#include "lib/trace.h"

#include "dtm0/log.h"

#include "ut/ut.h"              /* M0_UT_ASSERT */
#include "lib/memory.h"         /* M0_ALLOC_PTR */
#include "lib/misc.h"           /* m0_rnd64 */
#include "be/ut/helper.h"       /* m0_be_ut_backend_init */
#include "be/domain.h"          /* m0_be_domain_seg_first */
#include "conf/objs/common.h"   /* M0_CONF__SDEV_FT_ID */
#include "fid/fid.h"            /* M0_FID_TINIT */

#include "dtm0/domain.h"        /* m0_dtm0_domain_cfg */
#include "dtm0/cfg_default.h"   /* m0_dtm0_domain_cfg_default_dup */
#include "dtm0/dtm0.h"          /* m0_dtm0_redo */
#include "be/tx_bulk.h"         /* m0_be_tx_bulk */
#include "dtm0/ut/helper.h"     /* dtm0_ut_log_ctx */

enum {
	M0_DTM0_UT_LOG_SIMPLE_REDO_SIZE = 0x100,
};

/* TODO: add dtm0_ut_log_init-fini */

void m0_dtm0_ut_log_simple(void)
{
	enum {
		TS_BASE = 0x100,
		NR_OPER = 0x10,
		NR_REC_PER_OPER = 0x10,
	};
	struct m0_dtm0_redo       *redo;
	struct m0_buf              redo_buf = {};
	struct m0_fid              p_sdev_fid;
	uint64_t                   seed = 42;
	int                        rc;
	int                        i;
	int                        j;
	struct m0_dtx0_id          dtx0_id;
	struct dtm0_ut_log_ctx    *lctx;
	bool                       successful = false;

	M0_ALLOC_PTR(redo);
	M0_UT_ASSERT(redo != NULL);
	rc = m0_buf_alloc(&redo_buf, M0_DTM0_UT_LOG_SIMPLE_REDO_SIZE);
	M0_UT_ASSERT(rc == 0);

	for (i = 0; i < redo_buf.b_nob; ++i)
		((char *)redo_buf.b_addr)[i] = m0_rnd64(&seed) & 0xff;
	p_sdev_fid = M0_FID_TINIT(M0_CONF__SDEV_FT_ID, 1, 2);
	*redo = (struct m0_dtm0_redo){
		.dtr_descriptor = {
			.dtd_id = {
				.dti_timestamp           = TS_BASE,
				.dti_originator_sdev_fid = p_sdev_fid,
			},
			.dtd_participants = {
				.dtpa_participants_nr = 1,
				.dtpa_participants    = &p_sdev_fid,
			},
		},
		.dtr_payload = {
			.dtp_type = M0_DTX0_PAYLOAD_BLOB,
			.dtp_data = {
				.ab_count = 1,
				.ab_elems = &redo_buf,
			},
		},
	};

	lctx = dtm0_ut_log_init();

	for (i = 0; i < NR_OPER; ++i) {
		rc = m0_dtm0_log_open(&lctx->dol, &lctx->dod_cfg.dodc_log);
		M0_UT_ASSERT(rc == 0);
		for (j = 0; j < NR_REC_PER_OPER; ++j) {
			redo->dtr_descriptor.dtd_id.dti_timestamp = j + TS_BASE;
			M0_BE_UT_TRANSACT(&lctx->ut_be, tx, cred,
					  m0_dtm0_log_redo_add_credit(&lctx->dol,
								      redo,
								      &cred),
					  rc = m0_dtm0_log_redo_add(&lctx->dol,
								    tx,
								    redo,
								    &p_sdev_fid));
			M0_UT_ASSERT(rc == 0);
		}
		for (j = 0; j < NR_REC_PER_OPER + 1; ++j) {
			if (j == NR_REC_PER_OPER)
				m0_dtm0_log_end(&lctx->dol);
			successful = j == NR_REC_PER_OPER;
			M0_BE_OP_SYNC(op,
				      m0_dtm0_log_p_get_none_left(&lctx->dol,
								  &op,
								  &dtx0_id,
								  &successful));
			M0_UT_ASSERT(equi(successful, j < NR_REC_PER_OPER));
			if (!successful)
				break;
			redo->dtr_descriptor.dtd_id.dti_timestamp = j + TS_BASE;
			M0_UT_ASSERT(m0_dtx0_id_eq(&dtx0_id,
						   &redo->dtr_descriptor.dtd_id));

			M0_BE_UT_TRANSACT(&lctx->ut_be, tx, cred,
					  m0_dtm0_log_prune_credit(&lctx->dol,
								   &cred),
					  m0_dtm0_log_prune(&lctx->dol, tx,
							&redo->dtr_descriptor.dtd_id));
		}
		m0_dtm0_log_close(&lctx->dol);
	}


	dtm0_ut_log_fini(lctx);

	m0_buf_free(&redo_buf);
	m0_free(redo);
}

static void dtm0_ut_log_remove(struct dtm0_ut_log_ctx *lctx,
			       int                     timestamp, int nr)
{
	int                  i;
	struct m0_dtx0_id    dtx0_id;
	bool                *seen;
	bool                 successful;

	M0_ALLOC_ARR(seen, nr);
	M0_UT_ASSERT(seen != NULL);
	M0_UT_ASSERT(m0_forall(i, nr, !seen[i]));

	for (i = 0; i < nr; ++i) {
		M0_BE_OP_SYNC(op,
			      m0_dtm0_log_p_get_none_left(&lctx->dol, &op,
							  &dtx0_id, &successful));
		M0_UT_ASSERT(successful);
		seen[dtx0_id.dti_timestamp - timestamp] = true;
		M0_BE_UT_TRANSACT(&lctx->ut_be, tx, cred,
				  m0_dtm0_log_prune_credit(&lctx->dol, &cred),
				  m0_dtm0_log_prune(&lctx->dol, tx, &dtx0_id));
	}

	M0_UT_ASSERT(m0_forall(i, nr, seen[i]));
}


void m0_dtm0_ut_log_mpsc(void)
{
	struct dtm0_ut_log_ctx    *lctx;
	struct dtm0_ut_log_mp_ctx  lmp_ctx;
	int                        rc;

	lctx = dtm0_ut_log_init();
	M0_UT_ASSERT(lctx != NULL);

	rc = m0_dtm0_log_open(&lctx->dol, &lctx->dod_cfg.dodc_log);
	M0_UT_ASSERT(rc == 0);

	dtm0_ut_log_mp_init(&lmp_ctx, lctx);
	dtm0_ut_log_mp_run(&lmp_ctx);
	m0_be_op_wait(lmp_ctx.op);
	dtm0_ut_log_mp_fini(&lmp_ctx);

	dtm0_ut_log_remove(lctx, MPSC_TS_BASE, MPSC_NR_REC_TOTAL);

	m0_dtm0_log_close(&lctx->dol);
	dtm0_ut_log_fini(lctx);
}

/* TODO: (before landing)
 * Goals:
 *   1. Land new DTM0 log code in main.
 *   2. The code must always be enabled.
 *   3. No visible changes (from Motr client user's perspective).
 *   4. Apply real load to the log.
 *
 * 1. Add a test where log_end() works fine before we are awaiting
 *    for the latest  and after we are awaiting for the latest.
 * 2. Empty log + end == !successful.
 * 3. Simple pruner (remove after added), concurrency == 1 (single FOM, single
 *    record).
 * 4. Pruner UTs:
 *   x4.1. Add to the log, run pruner, check if log is empty.
 *   x4.2. One tx_bulk that adds records to the log, run pruner.
 *   x4.3. Run pruner, empty log, nothing happens.
 *   x4.4. Add many log records, run pruner, some record may still be present
 *        in the log.
 *   4.5. Run pruner, run tx_bulk, wait until log is empty, wait 0.5 sec,
 *        repeat (tx_bulk, wait until empty).
 * 5. Run pruner in DTM0 domain (init/start/stop/fini) by default.
 * 6. DTX0 for server, and call it from CAS.
 * 7. Random dtxid if CAS request has empty dtx descriptor.
 *
 * [2.1]
 * log_add()
 * log_end()
 * log_wait() -> ok
 * log_wait() -> fail
 *
 * [2.2]
 * log_add()
 * log_wait() -> ok
 * log_end()
 * log_wait() -> fail
 *
 * [3]
 * log_end()
 * log_wait() -> fail
 *
 * TODO: (after landing)
 * 1. Wait until be tx becomes PERSISTENT.
 * 2. Simple pruner, concurrency > 1 (many FOMs).
 * 3. Simple pruner, batch delete.
 * 4. DTX0 for client is always enabled at data structure level but
 *    the state transitions may be disabled by NODTM. If that is not possible,
 *    then just generate random dtxid.
 * 5. Pruner: do not add records to FOL.
 * 6. s/dod/dodc in DTM0 log.
 * 7. Co-fom service or DTM fom service? single service for coroutines?
 */

#undef M0_TRACE_SUBSYSTEM

/** @} end of dtm0 group */

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
