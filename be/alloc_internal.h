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

#ifndef __MOTR_BE_ALLOC_INTERNAL_H__
#define __MOTR_BE_ALLOC_INTERNAL_H__

#include "lib/types.h"	/* m0_bcount_t */
#include "be/list.h"	/* m0_be_list */
#include "be/list_xc.h"
#include "be/alloc.h"	/* m0_be_allocator_stats */
#include "be/alloc_xc.h"
#include "be/fl.h"      /* m0_be_fl */
#include "be/fl_xc.h"

/**
 * @defgroup be Meta-data back-end
 *
 * @{
 */

/**
 * @brief Allocator chunk.
 *
 * - resides in the allocator space;
 * - located just before allocated memory block;
 * - there is at least one chunk in the allocator.
 */
struct be_alloc_chunk {
	/**
	 * M0_BE_ALLOC_MAGIC0
	 * Used to find invalid memory access after allocated chunk.
	 */
	uint64_t                   bac_magic0;
	/** for m0_be_allocator_header.bah_chunks list */
	struct m0_be_list_link     bac_linkage;
	/** magic for bac_linkage */
	uint64_t                   bac_magic;
	/** for m0_be_allocator_header.bah_fl list */
	struct m0_be_list_link     bac_linkage_free;
	/** magic for bac_linkage_free */
	uint64_t                   bac_magic_free;
	/** size of chunk */
	m0_bcount_t                bac_size;
	/** is chunk free? */
	bool                       bac_free;
	/** Allocator zone where chunk resides. */
	uint8_t                    bac_zone M0_XCA_FENUM(m0_be_alloc_zone_type);
	/** Is chunk header aligned? */
	bool                       bac_chunk_align;
	/** Chunk header or memory alignement as pow-of-2. */
	uint16_t                   bac_align_shift;
	/**
	 * M0_BE_ALLOC_MAGIC1
	 * Used to find invalid memory access before allocated chunk.
	 */
	/** M0_BE_ALLOC_MAGIC1 */
	uint64_t                   bac_magic1;
	/**
	 * m0_be_alloc() and m0_be_alloc_aligned() will return address
	 * of bac_mem for allocated chunk.
	 */
	char                       bac_mem[0];
} M0_XCA_RECORD M0_XCA_DOMAIN(be);
M0_BASSERT(sizeof(struct be_alloc_chunk) % (1UL << M0_BE_ALLOC_SHIFT_MIN) == 0);

/**
 * @brief Allocator memory zone.
 *
 * @see m0_be_alloc_zone_type.
 */
struct m0_be_alloc_zone {
	/** Zone size in bytes. */
	m0_bcount_t baz_size;
	/** Number of free bytes in zone. */
	m0_bcount_t baz_free;
} M0_XCA_RECORD M0_XCA_DOMAIN(be);


/**
 * @brief Allocator header.
 *
 * - allocator space begins at m0_be_allocator_header.bah_addr and have size
 *   m0_be_allocator_header.bah_size bytes;
 * - resides in a segment address space;
 * - is a part of a segment header. It may be changed in the future;
 * - contains list headers for chunks lists.
 */
struct m0_be_allocator_header {
	struct m0_be_list             bah_chunks;	/**< all chunks */
	struct m0_be_fl               bah_fl;           /**< free lists */
	struct m0_be_allocator_stats  bah_stats;	/**< XXX not used now */
	m0_bcount_t                   bah_size;		/**< memory size */
	void			     *bah_addr;		/**< memory address */
} M0_XCA_RECORD M0_XCA_DOMAIN(be);

/** @} end of be group */

#endif /* __MOTR_BE_ALLOC_INTERNAL_H__ */


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
