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


enum {
	M0_DTM0_UT_LOG_SIMPLE_SEG_SIZE  = 0x2000000,
	M0_DTM0_UT_LOG_SIMPLE_REDO_SIZE = 0x1000,
};


void m0_dtm0_ut_log_simple(void)
{
	enum {
		TS_BASE = 0x100,
		NR_OPER = 0x10,
		NR_REC_PER_OPER = 0x10,
	};
	struct m0_dtm0_domain_cfg *dod_cfg;
	struct m0_be_ut_backend   *ut_be;
	struct m0_be_ut_seg       *ut_seg;
	struct m0_dtm0_redo       *redo;
	struct m0_dtm0_log        *dol;
	struct m0_buf              redo_buf = {};
	struct m0_fid              p_sdev_fid;
	uint64_t                   seed = 42;
	int                        rc;
	int                        i;
	int                        j;
	struct m0_dtx0_id          dtx0_id;

	M0_ALLOC_PTR(dod_cfg);
	M0_UT_ASSERT(dod_cfg != NULL);
	M0_ALLOC_PTR(ut_be);
	M0_UT_ASSERT(ut_be != NULL);
	M0_ALLOC_PTR(ut_seg);
	M0_UT_ASSERT(ut_seg != NULL);
	M0_ALLOC_PTR(redo);
	M0_UT_ASSERT(redo != NULL);
	M0_ALLOC_PTR(dol);
	M0_UT_ASSERT(dol != NULL);
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

	m0_be_ut_backend_init(ut_be);
	m0_be_ut_seg_init(ut_seg, ut_be, M0_DTM0_UT_LOG_SIMPLE_SEG_SIZE);
	rc = m0_dtm0_domain_cfg_default_dup(dod_cfg, true);
	M0_UT_ASSERT(rc == 0);
	dod_cfg->dodc_log.dlc_be_domain = &ut_be->but_dom;
	dod_cfg->dodc_log.dlc_seg =
		m0_be_domain_seg_first(dod_cfg->dodc_log.dlc_be_domain);

	rc = m0_dtm0_log_create(dol, &dod_cfg->dodc_log);
	M0_UT_ASSERT(rc == 0);
	for (i = 0; i < NR_OPER; ++i) {
		rc = m0_dtm0_log_open(dol, &dod_cfg->dodc_log);
		M0_UT_ASSERT(rc == 0);
		for (j = 0; j < NR_REC_PER_OPER; ++j) {
			redo->dtr_descriptor.dtd_id.dti_timestamp = j + TS_BASE;
			M0_BE_UT_TRANSACT(ut_be, tx, cred,
					  m0_dtm0_log_redo_add_credit(dol,
								      redo,
								      &cred),
					  rc = m0_dtm0_log_redo_add(dol, tx,
								    redo,
								    &p_sdev_fid));
			M0_UT_ASSERT(rc == 0);
		}
		for (j = 0; j < NR_REC_PER_OPER; ++j) {
			M0_BE_OP_SYNC(op,
				      m0_dtm0_log_p_get_none_left(dol, &op,
								  &dtx0_id));
			redo->dtr_descriptor.dtd_id.dti_timestamp = j + TS_BASE;
			M0_UT_ASSERT(m0_dtx0_id_eq(&dtx0_id,
						   &redo->dtr_descriptor.dtd_id));

			M0_BE_UT_TRANSACT(ut_be, tx, cred,
					  m0_dtm0_log_prune_credit(dol, &cred),
					  m0_dtm0_log_prune(dol, tx,
							&redo->dtr_descriptor.dtd_id));
		}
		m0_dtm0_log_close(dol);
	}
	m0_dtm0_log_destroy(dol);

	m0_dtm0_domain_cfg_free(dod_cfg);
	m0_be_ut_seg_fini(ut_seg);
	m0_be_ut_backend_fini(ut_be);

	m0_buf_free(&redo_buf);
	m0_free(dol);
	m0_free(redo);
	m0_free(ut_seg);
	m0_free(ut_be);
	m0_free(dod_cfg);
}


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
