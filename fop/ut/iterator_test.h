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

#ifndef __MOTR_FOP_UT_ITERATOR_TEST_H__
#define __MOTR_FOP_UT_ITERATOR_TEST_H__

#include "lib/types.h"
#include "xcode/xcode_attr.h"
#include "fid/fid.h"
#include "fid/fid_xc.h"

struct m0_fop_seg {
	uint64_t fs_offset;
	uint64_t fs_count;
} M0_XCA_RECORD;

struct m0_fop_vec {
	uint32_t           fv_count;
	struct m0_fop_seg *fv_seg;
} M0_XCA_SEQUENCE;

struct m0_fop_optfid {
	struct m0_fid fo_fid;
	m0_void_t     fo_none;
} M0_XCA_RECORD;

struct m0_fop_recursive1 {
	struct m0_fid        fr_fid;
	struct m0_fop_vec    fr_seq;
	struct m0_fop_optfid fr_unn;
} M0_XCA_RECORD;

struct m0_fop_recursive2 {
	struct m0_fid            fr_fid;
	struct m0_fop_recursive1 fr_seq;
} M0_XCA_RECORD;

struct m0_fop_iterator_test {
	struct m0_fid            fit_fid;
	struct m0_fop_vec        fit_vec;
	struct m0_fop_optfid     fit_opt0;
	struct m0_fop_optfid     fit_opt1;
	struct m0_fop_optfid     fit_topt;
	struct m0_fop_recursive2 fit_rec;
} M0_XCA_RECORD;

#endif /* __MOTR_FOP_UT_ITERATOR_TEST_H__ */

/*
 *  Local variables:
 *  c-indentation-style: "K&R"
 *  c-basic-offset: 8
 *  tab-width: 8
 *  fill-column: 80
 *  scroll-step: 1
 *  End:
 */
