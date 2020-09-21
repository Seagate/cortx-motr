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

#ifndef __MOTR_STOB_IO_FOP_H__
#define __MOTR_STOB_IO_FOP_H__

#include "lib/types.h"
#include "xcode/xcode_attr.h"

struct stob_io_fop_fid {
	uint64_t f_seq;
	uint64_t f_oid;
} M0_XCA_RECORD;

struct m0_fi_value {
	uint32_t fi_count;
	uint8_t *fi_buf;
} M0_XCA_SEQUENCE;

struct m0_stob_io_write {
	struct stob_io_fop_fid fiw_object;
	struct m0_fi_value     fiw_value;
} M0_XCA_RECORD;

struct m0_stob_io_write_rep {
	int32_t  fiwr_rc;
	uint32_t fiwr_count;
} M0_XCA_RECORD;

struct m0_stob_io_read {
	struct stob_io_fop_fid fir_object;
} M0_XCA_RECORD;

struct m0_stob_io_read_rep {
	int32_t            firr_rc;
	uint32_t           firr_count;
	struct m0_fi_value firr_value;
} M0_XCA_RECORD;

struct m0_stob_io_create {
	struct stob_io_fop_fid fic_object;
} M0_XCA_RECORD;

struct m0_stob_io_create_rep {
	int32_t ficr_rc;
} M0_XCA_RECORD;

extern struct m0_fop_type m0_stob_io_create_fopt;
extern struct m0_fop_type m0_stob_io_read_fopt;
extern struct m0_fop_type m0_stob_io_write_fopt;

extern struct m0_fop_type m0_stob_io_create_rep_fopt;
extern struct m0_fop_type m0_stob_io_read_rep_fopt;
extern struct m0_fop_type m0_stob_io_write_rep_fopt;

void m0_stob_io_fop_init(void);
void m0_stob_io_fop_fini(void);

#endif /* !__MOTR_STOB_IO_FOP_H__ */

/*
 *  Local variables:
 *  c-indentation-style: "K&R"
 *  c-basic-offset: 8
 *  tab-width: 8
 *  fill-column: 80
 *  scroll-step: 1
 *  End:
 */
