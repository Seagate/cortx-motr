/* -*- C -*- */
/*
 * Copyright (c) 2012-2020 Seagate Technology LLC and/or its Affiliates
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

#ifndef __MOTR_MOTR_UT_CS_UT_FOP_FOMS_H__
#define __MOTR_MOTR_UT_CS_UT_FOP_FOMS_H__

#include "lib/types.h"
#include "xcode/xcode_attr.h"

extern struct m0_fop_type cs_ds1_req_fop_fopt;
extern struct m0_fop_type cs_ds1_rep_fop_fopt;
extern struct m0_fop_type cs_ds2_req_fop_fopt;
extern struct m0_fop_type cs_ds2_rep_fop_fopt;

extern const struct m0_rpc_item_ops cs_ds_req_fop_rpc_item_ops;

/*
  Supported service types.
 */
enum {
        CS_UT_SERVICE1 = 1,
        CS_UT_SERVICE2,
};

/*
  Builds ds1 service fop types.
  Invoked from service specific stop function.
 */
int m0_cs_ut_ds1_fop_init(void);

/*
  Finalises ds1 service fop types.
  Invoked from service specific startup function.
 */
void m0_cs_ut_ds1_fop_fini(void);

/*
  Builds ds1 service fop types.
  Invoked from service specific stop function.
 */
int m0_cs_ut_ds2_fop_init(void);

/*
  Finalises ds1 service fop types.
  Invoked from service specific startup function.
 */
void m0_cs_ut_ds2_fop_fini(void);

struct m0_fom;

void m0_ut_fom_phase_set(struct m0_fom *fom, int phase);

/*
  Dummy fops to test motr setup
 */
struct cs_ds1_req_fop {
	uint64_t csr_value;
} M0_XCA_RECORD M0_XCA_DOMAIN(rpc);

struct cs_ds1_rep_fop {
	int32_t csr_rc;
} M0_XCA_RECORD M0_XCA_DOMAIN(rpc);

struct cs_ds2_req_fop {
	uint64_t csr_value;
} M0_XCA_RECORD M0_XCA_DOMAIN(rpc);

struct cs_ds2_rep_fop {
	int32_t csr_rc;
} M0_XCA_RECORD M0_XCA_DOMAIN(rpc);

/* __MOTR_MOTR_UT_CS_UT_FOP_FOMS_H__ */
#endif

/*
 *  Local variables:
 *  c-indentation-style: "K&R"
 *  c-basic-offset: 8
 *  tab-width: 8
 *  fill-column: 80
 *  scroll-step: 1
 *  End:
 */
