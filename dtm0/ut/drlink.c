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

#include "dtm0/drlink.h"

#include "ut/ut.h"              /* M0_UT_ASSERT */
#include "fid/fid.h"            /* M0_FID_INIT */
#include "be/queue.h"           /* m0_be_queue_init */

#include "dtm0/service.h"       /* m0_dtm0_service */
#include "dtm0/fop.h"           /* dtm0_req_fop */

#include "dtm0/ut/helper.h"     /* m0_ut_dtm0_helper_init */


enum {
	DTM0_UT_DRLINK_SIMPLE_POST_NR = 0x100,
};

void m0_dtm0_ut_drlink_simple(void)
{
	struct m0_ut_dtm0_helper *udh;
	struct m0_dtm0_service   *svc;
	struct m0_fom             fom = {}; // just a fom for m0_dtm0_req_post()
	struct m0_dtm0_tx_pa      pa = {};
	struct dtm0_req_fop      *fop;
	struct m0_be_op          *op;
	struct m0_be_op           op_out = {};
	struct m0_fid            *fid;
	struct m0_fid             fid_out;
	bool                      successful;
	bool                      found;
	int                       rc;
	int                       i;
	int                       j;

	M0_ALLOC_PTR(udh);
	M0_ASSERT(udh != NULL);

	m0_ut_dtm0_helper_init(udh);
	svc = udh->udh_client_dtm0_service;

	M0_ALLOC_ARR(fid, DTM0_UT_DRLINK_SIMPLE_POST_NR);
	M0_UT_ASSERT(fid != NULL);
	for (i = 0; i < DTM0_UT_DRLINK_SIMPLE_POST_NR; ++i) {
		fid[i] = M0_FID_INIT(0, i+1);  /* TODO set fid type */
	}
	M0_ALLOC_ARR(fop, DTM0_UT_DRLINK_SIMPLE_POST_NR);
	M0_UT_ASSERT(fop != NULL);
	for (i = 0; i < DTM0_UT_DRLINK_SIMPLE_POST_NR; ++i) {
		fop[i] = (struct dtm0_req_fop){
			.dtr_msg = DTM_TEST,
			.dtr_txr = {
				.dtd_id = {
					.dti_fid = fid[i],
				},
				.dtd_ps = {
					.dtp_nr = 1,
					.dtp_pa = &pa,
				},
			},
		};
	}
	M0_ALLOC_ARR(op, DTM0_UT_DRLINK_SIMPLE_POST_NR);
	M0_UT_ASSERT(op != NULL);
	for (i = 0; i < DTM0_UT_DRLINK_SIMPLE_POST_NR; ++i)
		m0_be_op_init(&op[i]);

	M0_ALLOC_PTR(svc->dos_ut_queue);
	M0_UT_ASSERT(svc->dos_ut_queue != 0);
	rc = m0_be_queue_init(svc->dos_ut_queue,
			      &(struct m0_be_queue_cfg){
			.bqc_q_size_max = DTM0_UT_DRLINK_SIMPLE_POST_NR,
			.bqc_producers_nr_max = DTM0_UT_DRLINK_SIMPLE_POST_NR,
			.bqc_consumers_nr_max = 1,
			.bqc_item_length = sizeof fid[0],
                              });
	M0_UT_ASSERT(rc == 0);

	for (i = 0; i < DTM0_UT_DRLINK_SIMPLE_POST_NR; ++i) {
		rc = m0_dtm0_req_post(udh->udh_server_dtm0_service,
				      &op[i], &fop[i],
		                      &udh->udh_client_dtm0_fid, &fom, true);
		M0_UT_ASSERT(rc == 0);
	}
	m0_be_op_init(&op_out);
	for (i = 0; i < DTM0_UT_DRLINK_SIMPLE_POST_NR; ++i) {
		successful = false;
		m0_be_queue_lock(svc->dos_ut_queue);
		M0_BE_QUEUE_GET(svc->dos_ut_queue, &op_out, &fid_out,
				&successful);
		m0_be_queue_unlock(svc->dos_ut_queue);
		m0_be_op_wait(&op_out);
		M0_UT_ASSERT(successful);
		m0_be_op_reset(&op_out);
		found = false;
		for (j = 0; j < DTM0_UT_DRLINK_SIMPLE_POST_NR; ++j) {
			if (m0_fid_eq(&fid_out, &fid[j])) {
				found = true;
				fid[j] = M0_FID0;
				break;
			}
		}
		M0_UT_ASSERT(found);
	}
	m0_be_op_fini(&op_out);
	m0_be_queue_fini(svc->dos_ut_queue);
	m0_free(svc->dos_ut_queue);
	for (i = 0; i < DTM0_UT_DRLINK_SIMPLE_POST_NR; ++i)
		M0_UT_ASSERT(m0_fid_eq(&fid[i], &M0_FID0));
	m0_free(fid);
	for (i = 0; i < DTM0_UT_DRLINK_SIMPLE_POST_NR; ++i) {
		m0_be_op_wait(&op[i]);
		m0_be_op_fini(&op[i]);
	}
	m0_free(op);
	m0_free(fop);

	m0_ut_dtm0_helper_fini(udh);
	m0_free(udh);

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
