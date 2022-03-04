/* -*- C -*- */
/*
 * Copyright (c) 2013-2021 Seagate Technology LLC and/or its Affiliates
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

#ifndef __MOTR_STOB_PARTITION_INTERNAL_H__
#define __MOTR_STOB_PARTITION_INTERNAL_H__

#include "be/extmap.h"		/* m0_be_emap */
#include "fid/fid.h"		/* m0_fid */
#include "lib/types.h"		/* m0_bcount_t */
#include "stob/domain.h"	/* m0_stob_domain */
#include "stob/io.h"		/* m0_stob_io */
#include "stob/stob.h"		/* m0_stob */
#include "stob/type.h"		/* m0_stob_type */
#include "stob/stob_xc.h"
//#include "be/partition_table.h"
/**
   @defgroup stobpart Storage objects with extent maps.

   <b>Storage object type based on Allocation Data (AD) stored in a
   data-base.</b>

   AD storage object type (m0_stob_ad_type) manages collections of storage
   objects with in an underlying storage object. The underlying storage object
   is specified per-domain by a call to m0_ad_stob_setup() function.

   m0_stob_ad_type uses data-base (also specified as a parameter to
   m0_ad_stob_setup()) to store extent map (m0_emap) which keeps track of
   mapping between logical offsets in AD stobs and physical offsets within
   underlying stob.

   @{
 */

struct m0_be_seg;

enum { PART_PATHLEN = 4096 };
enum m0_stob_part_domain_format_version {
	M0_STOB_PART_DOMAIN_FORMAT_VERSION_1 = 1,

	/* future versions, uncomment and
	 * update M0_STOB_PART_DOMAIN_FORMAT_VERSION */
	/*M0_STOB_PART_DOMAIN_FORMAT_VERSION_2,*/
	/*M0_STOB_PART_DOMAIN_FORMAT_VERSION_3,*/

	/** Current version, should point to the latest version present */
	M0_STOB_PART_DOMAIN_FORMAT_VERSION =
		M0_STOB_PART_DOMAIN_FORMAT_VERSION_1
};

enum {
	STOB_TYPE_PARTITION = 0x03,
};


struct m0_stob_part {
	struct m0_stob  part_stob;
	m0_bcount_t     part_id;
	m0_bcount_t    *part_table;
	m0_bcount_t     part_size_in_chunks;
	m0_bcount_t     part_chunk_size_in_bits;
};

struct m0_stob_part_io {
	struct m0_stob_io *pi_fore;
	struct m0_stob_io  pi_back;
	struct m0_clink    pi_clink;
};

extern const struct m0_stob_type m0_stob_part_type;
M0_INTERNAL struct m0_stob* m0_stob_part_bstob_get(struct m0_stob *part_stob);

/** @} end group stobpart */

/* __MOTR_STOB_PARTITION_INTERNAL_H__ */
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
