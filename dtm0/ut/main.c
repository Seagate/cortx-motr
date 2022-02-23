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

#include "dtm0/clk_src.h"
#include "dtm0/fop.h"
#include "dtm0/helper.h"
#include "dtm0/service.h"
#include "dtm0/tx_desc.h"
#include "be/dtm0_log.h"
#include "net/net.h"
#include "rpc/rpclib.h"
#include "ut/ut.h"
#include "cas/cas.h"
#include "cas/cas_xc.h"


enum {
	NUM_CAS_RECS = 10,
};

struct record
{
	uint64_t key;
	uint64_t value;
};

static void cas_xcode_test(void)
{
	struct record recs[NUM_CAS_RECS];
	struct m0_cas_rec cas_recs[NUM_CAS_RECS];
	struct m0_fid fid = M0_FID_TINIT('i', 0, 0);
	void       *buf;
	m0_bcount_t len;
	int rc;
	int i;
	struct m0_cas_op *op_out;
	struct m0_cas_op op_in = {
		.cg_id  = {
			.ci_fid = fid
		},
		.cg_rec = {
			.cr_rec = cas_recs
		},
		.cg_txd = {
			.dtd_ps = {
				.dtp_nr = 1,
				.dtp_pa = &(struct m0_dtm0_tx_pa) {
					.p_state = 555,
				},
			},
		},
	};

	/* Fill array with pair: [key, value]. */
	m0_forall(i, NUM_CAS_RECS-1,
		  (recs[i].key = i, recs[i].value = i * i, true));

	for (i = 0; i < NUM_CAS_RECS - 1; i++) {
		cas_recs[i] = (struct m0_cas_rec){
			.cr_key = (struct m0_rpc_at_buf) {
				.ab_type  = 1,
				.u.ab_buf = M0_BUF_INIT(sizeof recs[i].key,
							&recs[i].key)
				},
			.cr_val = (struct m0_rpc_at_buf) {
				.ab_type  = 0,
				.u.ab_buf = M0_BUF_INIT(0, NULL)
				},
			.cr_rc = 0 };
	}
	cas_recs[NUM_CAS_RECS - 1] = (struct m0_cas_rec) { .cr_rc = ~0ULL };
	while (cas_recs[op_in.cg_rec.cr_nr].cr_rc != ~0ULL)
		++ op_in.cg_rec.cr_nr;

	rc = m0_xcode_obj_enc_to_buf(&M0_XCODE_OBJ(m0_cas_op_xc, &op_in),
				     &buf, &len);
	M0_UT_ASSERT(rc == 0);
	M0_ALLOC_PTR(op_out);
	M0_UT_ASSERT(op_out != NULL);
	rc = m0_xcode_obj_dec_from_buf(&M0_XCODE_OBJ(m0_cas_op_xc, op_out),
				       buf, len);
	M0_UT_ASSERT(rc == 0);

    m0_xcode_free_obj(&M0_XCODE_OBJ(m0_cas_op_xc, op_out));
}

extern void m0_dtm0_ut_drlink_simple(void);
extern void m0_dtm0_ut_domain_init_fini(void);

struct m0_ut_suite dtm0_ut = {
        .ts_name = "dtm0-ut",
        .ts_tests = {
                { "xcode",         &cas_xcode_test },
                { "drlink-simple", &m0_dtm0_ut_drlink_simple },
                { "domain_init-fini", &m0_dtm0_ut_domain_init_fini },
		{ NULL, NULL },
	}
};

/*
 *  Local variables:
 *  c-indentation-style: "K&R"
 *  c-basic-offset: 8
 *  tab-width: 8
 *  fill-column: 80
 *  scroll-step: 1
 *  End:
 */
