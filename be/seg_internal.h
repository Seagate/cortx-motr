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
#ifndef __MOTR_BE_SEG_INTERNAL_H__
#define __MOTR_BE_SEG_INTERNAL_H__

#include "be/alloc_internal.h"  /* m0_be_allocator_header */
#include "be/alloc_internal_xc.h"
#include "be/list.h"
#include "be/seg.h"
#include "be/seg_xc.h"
#include "format/format.h"      /* m0_format_header */
#include "format/format_xc.h"

/**
 * @addtogroup be
 *
 * @{
 */

enum {
	M0_BE_SEG_HDR_GEOM_ITMES_MAX = 16,
	M0_BE_SEG_HDR_VERSION_LEN_MAX = 64,
};

/** "On-disk" header for segment, stored in STOB at zero offset */
struct m0_be_seg_hdr {
	struct m0_format_header       bh_header;
	uint64_t                      bh_id;
	uint32_t                      bh_items_nr;
	struct m0_be_seg_geom         bh_items[M0_BE_SEG_HDR_GEOM_ITMES_MAX];
	char                          bh_be_version[
					       M0_BE_SEG_HDR_VERSION_LEN_MAX+1];
	uint64_t                      bh_gen;
	struct m0_format_footer       bh_footer;
	struct m0_be_allocator_header bh_alloc[M0_BAP_NR];
	struct m0_be_list             bh_dict;
} M0_XCA_RECORD M0_XCA_DOMAIN(be);

enum m0_be_seg_hdr_format_version {
	M0_BE_SEG_HDR_FORMAT_VERSION_1 = 1,

	/* future versions, uncomment and update M0_BE_SEG_HDR_FORMAT_VERSION */
	/*M0_BE_SEG_HDR_FORMAT_VERSION_2,*/
	/*M0_BE_SEG_HDR_FORMAT_VERSION_3,*/

	/** Current version, should point to the latest version present */
	M0_BE_SEG_HDR_FORMAT_VERSION = M0_BE_SEG_HDR_FORMAT_VERSION_1
};

/** @} end of be group */
#endif /* __MOTR_BE_SEG_INTERNAL_H__ */

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
