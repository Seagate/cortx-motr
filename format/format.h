/*
 * Copyright (c) 2014-2020 Seagate Technology LLC and/or its Affiliates
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
#ifndef __MOTR_FORMAT_FORMAT_H__
#define __MOTR_FORMAT_FORMAT_H__

#include "lib/types.h"  /* uint64_t */
#include "lib/misc.h"   /* M0_FIELD_VALUE */
#include "lib/mutex.h"
#include "lib/rwlock.h"
#include "lib/chan.h"   /* m0_clink */

/**
 * @defgroup format Persistent objects format
 *
 * @{
 */

/** Standard header of a persistent object. */
struct m0_format_header {
	/**
	 * Header magic, used in m0_format_footer_verify() to verify that buffer
	 * has a valid m0_format_header.
	 */
	uint64_t hd_magic;
	/**
	 * Encoding of m0_format_tag data.
	 *
	 * - 16 most significant bits  -- version number;
	 * - 16 bits in the middle     -- object type;
	 * - 32 least significant bits -- size in bytes.
	 *
	 * @see  m0_format_header_pack(), m0_format_header_unpack()
	 */
	uint64_t hd_bits;
} M0_XCA_RECORD M0_XCA_DOMAIN(be|rpc);

/** Standard footer of a persistent object. */
struct m0_format_footer {
	uint64_t ft_magic;
	uint64_t ft_checksum;
} M0_XCA_RECORD M0_XCA_DOMAIN(be|rpc);

struct m0_format_tag {
	uint16_t ot_version;
	uint16_t ot_type;
	union {
		/*
		 * these are aliases to support different semantics of this
		 * field:
		 *
		 *   size   - can be used when header and footer don't belong
		 *            to the same container struct
		 *   offset - can be used when header and footer contained
		 *            withing the same struct
		 */
		uint32_t ot_size;
		uint32_t ot_footer_offset;
		/* NOTE: size and offset measured in bytes */
	};
};

/** Format types */
enum m0_format_type {
	/*
	 * ORDER MATTERS!!!
	 * Never rearrange items, add new at the end before placeholder
	 */
	M0_FORMAT_TYPE_BEGIN = 1,
	M0_FORMAT_TYPE_RPC_PACKET = M0_FORMAT_TYPE_BEGIN,
	M0_FORMAT_TYPE_RPC_ITEM,
	M0_FORMAT_TYPE_BE_BTREE,
	M0_FORMAT_TYPE_BE_BNODE,
	M0_FORMAT_TYPE_BE_EMAP_KEY,
	M0_FORMAT_TYPE_BE_EMAP_REC,
	M0_FORMAT_TYPE_BE_EMAP,
	M0_FORMAT_TYPE_BE_LIST,
	M0_FORMAT_TYPE_BE_SEG_HDR,
	M0_FORMAT_TYPE_BALLOC,
	M0_FORMAT_TYPE_ADDB2_FRAME_HEADER,
	M0_FORMAT_TYPE_STOB_AD_0TYPE_REC,
	M0_FORMAT_TYPE_STOB_AD_DOMAIN,
	M0_FORMAT_TYPE_COB_DOMAIN,
	M0_FORMAT_TYPE_COB_NSREC,
	M0_FORMAT_TYPE_BALLOC_GROUP_DESC,
	M0_FORMAT_TYPE_EXT,
	M0_FORMAT_TYPE_CAS_INDEX, /* Unused */
	M0_FORMAT_TYPE_POOLNODE,
	M0_FORMAT_TYPE_POOLDEV,
	M0_FORMAT_TYPE_POOL_SPARE_USAGE,
	M0_FORMAT_TYPE_CAS_STATE,
	M0_FORMAT_TYPE_CAS_CTG,

	/* ---> new items go here <--- */

	/* format types counter */
	M0_FORMAT_TYPE_NR
} M0_XCA_ENUM;

/* format type IDs should fit into m0_format_tag::ot_type */
M0_BASSERT(M0_FORMAT_TYPE_NR < UINT16_MAX);

M0_INTERNAL void m0_format_header_pack(struct m0_format_header *dest,
				       const struct m0_format_tag *src);

M0_INTERNAL void m0_format_header_unpack(struct m0_format_tag *dest,
					 const struct m0_format_header *src);

M0_INTERNAL void m0_format_footer_generate(struct m0_format_footer *footer,
					   const void              *buffer,
					   uint32_t                 size);

/**
 * Updates existing footer in a struct.
 *
 * Expects a corretcly filled m0_format_header to be present at the beginning of
 * a buffer.
 */
M0_INTERNAL void m0_format_footer_update(const void *buffer);

/**
 * Verifies format footer, ensuring that checksum in the footer matches the data.
 * It can be used when header and footer aren't contained within the same
 * parent/container struct
 *
 * @param footer - actual footer, with original checksum
 * @param buffer - beginning of the data for which checksum is calculated
 * @param size   - size of the data
 * @param iem    - true for metadata operations.
 *                 false for non metadata operations and tools.
 */
M0_INTERNAL int m0_format_footer_verify_generic(
			const struct m0_format_footer *footer,
			const void                    *buffer,
			uint32_t                       size,
			bool                           iem);

/**
 * A wrapper around m0_format_footer_verify_generic() for the case when header
 * and footer are contained withing the same parent/container struct.
 *
 * @param buffer - should point to a struct with m0_format_header and
 *                 m0_format_footer inside; header should be right at the
 *                 beginning of the struct; checksum is verified for the header
 *                 and all other fields in the struct, which come after it but
 *                 before the footer.
 * @param iem    - true for metadata operations.
 *                 false for non metadata operations and tools.
*/
M0_INTERNAL int m0_format_footer_verify(const void *buffer, bool iem);

#ifdef FORMAT_BE_STRUCT_CONVERTED

/*
 * Kernel doesn't store BE structure to a persistent storage. Therefore,
 * padding doesn't have to be the same for kernelspace and ondisk format.
 * Formula is the next: actual size of original structure plus ~20%.
 * Numbers are calculated for debug kernel.
 */
#ifdef __KERNEL__
#define M0_BE_MUTEX_PAD (184 + 56)
#define M0_BE_RWLOCK_PAD (144 + 48)
#define M0_BE_CLINK_PAD (152 + 48)
#else
#define M0_BE_MUTEX_PAD (56 + 16)
#define M0_BE_RWLOCK_PAD (56 + 16)
#define M0_BE_CLINK_PAD (88 + 24)
#endif /* __KERNEL__ */

#else /* !FORMAT_BE_STRUCT_CONVERTED */

/* These values are for "teacake" compatibility. */
#define M0_BE_MUTEX_PAD 168
#define M0_BE_RWLOCK_PAD 256
#define M0_BE_CLINK_PAD sizeof(struct m0_clink)

#endif /* FORMAT_BE_STRUCT_CONVERTED */

struct m0_be_mutex {
	union {
		struct m0_mutex mutex;
		char            pad[M0_BE_MUTEX_PAD];
	} bm_u;
} M0_XCA_BLOB M0_XCA_DOMAIN(be);
M0_BASSERT(sizeof(struct m0_mutex) <=
	   sizeof(M0_FIELD_VALUE(struct m0_be_mutex, bm_u.pad)));

struct m0_be_rwlock {
	union {
		struct m0_rwlock rwlock;
		char             pad[M0_BE_RWLOCK_PAD];
	} bl_u;
} M0_XCA_BLOB M0_XCA_DOMAIN(be);
M0_BASSERT(sizeof(struct m0_rwlock) <=
	   sizeof(M0_FIELD_VALUE(struct m0_be_rwlock, bl_u.pad)));

struct m0_be_clink {
	union {
		struct m0_clink clink;
		char            pad[M0_BE_CLINK_PAD];
	} bc_u;
} M0_XCA_BLOB M0_XCA_DOMAIN(be);
M0_BASSERT(sizeof(struct m0_clink) <=
	   sizeof(M0_FIELD_VALUE(struct m0_be_clink, bc_u.pad)));

struct m0_be_chan {
	struct m0_chan bch_chan;
} M0_XCA_BLOB M0_XCA_DOMAIN(be);

/** @} end of format group */
#endif /* __MOTR_FORMAT_FORMAT_H__ */

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
