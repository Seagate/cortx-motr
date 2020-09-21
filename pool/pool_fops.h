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


#pragma once

#ifndef __MOTR_POOL_POOL_FOPS_H__
#define __MOTR_POOL_POOL_FOPS_H__

#include "lib/types.h"
#include "fid/fid.h"
#include "fid/fid_xc.h"
#include "xcode/xcode_attr.h"

extern struct m0_fop_type m0_fop_poolmach_query_fopt;
extern struct m0_fop_type m0_fop_poolmach_query_rep_fopt;
extern struct m0_fop_type m0_fop_poolmach_set_fopt;
extern struct m0_fop_type m0_fop_poolmach_set_rep_fopt;

M0_INTERNAL void m0_poolmach_fop_fini(void);
M0_INTERNAL int m0_poolmach_fop_init(void);

struct m0_fop_poolmach_set_rep {
	int32_t  fps_rc;
} M0_XCA_RECORD M0_XCA_DOMAIN(rpc);

struct m0_fop_poolmach_dev_info {
	uint32_t                    fpi_nr;
	struct m0_fop_poolmach_dev *fpi_dev;
} M0_XCA_SEQUENCE M0_XCA_DOMAIN(rpc);

struct m0_fop_poolmach_dev {
	uint32_t      fpd_state;
	struct m0_fid fpd_fid;
} M0_XCA_RECORD M0_XCA_DOMAIN(rpc);

struct m0_fop_poolmach_set {
	uint32_t                        fps_type;
	struct m0_fop_poolmach_dev_info fps_dev_info;
} M0_XCA_RECORD M0_XCA_DOMAIN(rpc);

struct m0_fop_poolmach_query_rep {
	int32_t                         fqr_rc;
	struct m0_fop_poolmach_dev_info fqr_dev_info;
} M0_XCA_RECORD M0_XCA_DOMAIN(rpc);

struct m0_fop_poolmach_dev_idx {
	uint32_t       fpx_nr;
	struct m0_fid *fpx_fid;
} M0_XCA_SEQUENCE M0_XCA_DOMAIN(rpc);

struct m0_fop_poolmach_query {
	uint32_t                       fpq_type;
	struct m0_fop_poolmach_dev_idx fpq_dev_idx;
} M0_XCA_RECORD M0_XCA_DOMAIN(rpc);

#endif /* __MOTR_POOL_POOL_FOPS_H__ */

/*
 *  Local variables:
 *  c-indentation-style: "K&R"
 *  c-basic-offset: 8
 *  tab-width: 8
 *  fill-column: 80
 *  scroll-step: 1
 *  End:
 */
