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

#ifndef __MOTR_DTM0_FOP_FOMS_H__
#define __MOTR_DTM0_FOP_FOMS_H__

#include "lib/types.h"
#include "xcode/xcode_attr.h"

extern struct m0_fop_type dtm0_req_fop_fopt;
extern struct m0_fop_type dtm0_rep_fop_fopt;

extern const struct m0_rpc_item_ops dtm0_req_fop_rpc_item_ops;


M0_INTERNAL int m0_dtm0_fop_init(void);
M0_INTERNAL void m0_dtm0_fop_fini(void);

struct dtm0_req_fop {
	uint64_t csr_value;
} M0_XCA_RECORD M0_XCA_DOMAIN(rpc);

struct dtm0_rep_fop {
	int32_t csr_rc;
} M0_XCA_RECORD M0_XCA_DOMAIN(rpc);

/* __MOTR_DTM0_FOP_FOMS_H__ */
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
