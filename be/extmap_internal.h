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

#ifndef __MOTR_BE_EXTMAP_INTERNAL_H__
#define __MOTR_BE_EXTMAP_INTERNAL_H__

/**
   @addtogroup extmap

   <b>Extent map implementation.</b>

   Extent map collection (m0_be_emap) is a table. 128-bit prefix is used to store
   multiple extent maps in the same table.

   @{
 */

#include "lib/buf.h"       /* m0_buf */
#include "lib/buf_xc.h"
#include "lib/types.h"     /* m0_uint128 */
#include "lib/types_xc.h"
#include "btree/btree.h"      /* m0_btree */
#include "be/btree_xc.h"

#include "format/format.h"      /* m0_format_header */
#include "format/format_xc.h"
enum m0_be_emap_key_format_version {
	M0_BE_EMAP_KEY_FORMAT_VERSION_1 = 1,

	/* future versions, uncomment and update M0_BE_EMAP_KEY_FORMAT_VERSION */
	/*M0_BE_EMAP_KEY_FORMAT_VERSION_2,*/
	/*M0_BE_EMAP_KEY_FORMAT_VERSION_3,*/

	/** Current version, should point to the latest version present */
	M0_BE_EMAP_KEY_FORMAT_VERSION = M0_BE_EMAP_KEY_FORMAT_VERSION_1
};

/**
   A key used to identify a particular segment in the map collection.
 */
struct m0_be_emap_key {
	struct m0_format_header ek_header;
	/**
	    Prefix of the map the segment is part of.
	 */
	struct m0_uint128       ek_prefix;

	/**
	    Last offset of the segment's extent. That is, the key of a segment
	    ([A, B), V) has B as an offset.

	    This not entirely intuitive decision is forced by the available
	    range search interfaces of m0_db_cursor: m0_db_cursor_get()
	    positions the cursor on the least key not less than the key sought
	    for.
	 */
	m0_bindex_t             ek_offset;
	struct m0_format_footer ek_footer;
} M0_XCA_RECORD M0_XCA_DOMAIN(be);

enum m0_be_emap_rec_format_version {
	M0_BE_EMAP_REC_FORMAT_VERSION_1 = 1,

	/* future versions, uncomment and update M0_BE_EMAP_REC_FORMAT_VERSION */
	/*M0_BE_EMAP_REC_FORMAT_VERSION_2,*/
	/*M0_BE_EMAP_REC_FORMAT_VERSION_3,*/

	/** Current version, should point to the latest version present */
	M0_BE_EMAP_REC_FORMAT_VERSION = M0_BE_EMAP_REC_FORMAT_VERSION_1
};

/**
   A record stored in the table for each segment in the map collection.

   @note Note that there is a certain amount of redundancy: for any but the
   first segment in the map, its starting offset is equal to the last offset of
   the previous segment and for the first segment, the starting offset is
   0. Consequently, m0_be_emap_rec::er_start field can be eliminated reducing
   storage foot-print at the expense of increase in code complexity and
   possibility of occasional extra IO.
 */
struct m0_be_emap_rec {
	struct m0_format_header er_header;
	/**
	   Starting offset of the segment's extent.
	 */
	m0_bindex_t             er_start;
	/**
	   Value associated with the segment.
	 */
	uint64_t                er_value;
	/** unit_size */
	m0_bindex_t		er_unit_size;

	/* Note: Layout/format of emap-record (if checksum is present):
	 * - [Hdr| Balloc-Ext-Start| Balloc-Ext-Value| CS-nob| CS-Array[...]| Ftr]
	 * Record gets stored as contigious buffer
	 * ***** ChecksumArray[0...(er_cksum_nob -1)] *****
	 */
	/** checksum buffer size */
	uint32_t		er_cksum_nob;
	struct m0_format_footer er_footer;
} M0_XCA_RECORD M0_XCA_DOMAIN(be);

enum m0_be_emap_format_version {
	M0_BE_EMAP_FORMAT_VERSION_1 = 1,

	/* future versions, uncomment and update M0_BE_EMAP_FORMAT_VERSION */
	/*M0_BE_EMAP_FORMAT_VERSION_2,*/
	/*M0_BE_EMAP_FORMAT_VERSION_3,*/

	/** Current version, should point to the latest version present */
	M0_BE_EMAP_FORMAT_VERSION = M0_BE_EMAP_FORMAT_VERSION_1
};

// #define __AAL(x) __attribute__((aligned(x)))

/** Root node alignment for balloc extend and group descriptor trees. */
#define EMAP_ROOT_NODE_ALIGN 4096

enum {
	/** Root node size for balloc extend and group descriptor trees. */
	EMAP_ROOT_NODE_SIZE = 4096,
};

/**
   m0_be_emap stores a collection of related extent maps. Individual maps
   within a collection are identified by a prefix.

   @see m0_be_emap_obj_insert()
 */
struct m0_be_emap {
	struct m0_format_header em_header;
	struct m0_format_footer em_footer;
	/**
	 * The new BTree does not have a tree structure persistent on BE seg.
	 * Instead we have the root node occupying the same location where the
	 * old m0_be_btree used to be placed. To minimize the changes to the
	 * code we have the pointers to m0_btree (new BTree) placed here and the
	 * root nodes follow them aligned to a Block boundary.
	 */
	struct m0_btree        *em_mapping;

	/**
	 *  Root node for the above tree follows here. These root nodes
	 *  are aligned to Block boundary for performance reasons.
	 */
	uint8_t                 em_mp_node[EMAP_ROOT_NODE_SIZE]
				__attribute__((aligned(EMAP_ROOT_NODE_ALIGN)));
	/*
	 * volatile-only fields
	 */
	uint64_t                em_version;
	/** The segment where we are stored. */
	struct m0_be_seg       *em_seg;
	struct m0_be_rwlock     em_lock;
	struct m0_buf           em_key_buf;
	struct m0_buf           em_val_buf;
	struct m0_be_emap_key   em_key;
	struct m0_be_emap_rec   em_rec;
} M0_XCA_RECORD M0_XCA_DOMAIN(be);

/** @} end group extmap */

/* __MOTR_BE_EXTMAP_INTERNAL_H__ */
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
