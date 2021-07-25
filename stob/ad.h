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

#ifndef __MOTR_STOB_AD_INTERNAL_H__
#define __MOTR_STOB_AD_INTERNAL_H__

#include "be/extmap.h"		/* m0_be_emap */
#include "fid/fid.h"		/* m0_fid */
#include "lib/types.h"		/* m0_bcount_t */
#include "stob/domain.h"	/* m0_stob_domain */
#include "stob/io.h"		/* m0_stob_io */
#include "stob/stob.h"		/* m0_stob */
#include "stob/stob_xc.h"

/**
   @defgroup stobad Storage objects with extent maps.

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

struct m0_ad_balloc;
struct m0_ad_balloc_ops;
struct m0_be_seg;

/**
   Simple block allocator interface used by ad code to manage "free space" in
   the underlying storage object.

   @see mock_balloc
 */
struct m0_ad_balloc {
	const struct m0_ad_balloc_ops *ab_ops;
} M0_XCA_RECORD M0_XCA_DOMAIN(be);

struct m0_ad_balloc_ops {
	/** Initialises this balloc instance, creating its persistent state, if
	    necessary. This also destroys allocated struct m0_balloc instance
	    on failure.

	    @param block size shift in bytes, similarly to m0_stob_block_shift()
	    @param container_size Total size of the container in bytes
	    @param  blocks_per_group # of blocks per group
	 */
	int  (*bo_init)(struct m0_ad_balloc *ballroom, struct m0_be_seg *db,
			uint32_t bshift, m0_bcount_t container_size,
			m0_bcount_t blocks_per_group,
			m0_bcount_t spare_blocks_per_group);
	/** Finalises and destroys struct m0_balloc instance. */
	void (*bo_fini)(struct m0_ad_balloc *ballroom);
	/** Allocates count of blocks. On success, allocated extent, also
	    measured in blocks, is returned in out parameter. */
	int  (*bo_alloc)(struct m0_ad_balloc *ballroom, struct m0_dtx *dtx,
			 m0_bcount_t count, struct m0_ext *out,
			 uint64_t alloc_zone);
	/** Free space (possibly a sub-extent of an extent allocated
	    earlier). */
	int  (*bo_free)(struct m0_ad_balloc *ballroom, struct m0_dtx *dtx,
			struct m0_ext *ext);
	void (*bo_alloc_credit)(const struct m0_ad_balloc *ballroom, int nr,
				      struct m0_be_tx_credit *accum);
	void (*bo_free_credit)(const struct m0_ad_balloc *ballroom, int nr,
				     struct m0_be_tx_credit *accum);
	/**
	 * Marks given extent as used in the allocator.
	 *
	 * @retval 0 on success.
	 * @retval -ENOSPC on not enough space in superblock/group/fragment.
	 * @retval -EEXIST on overlap extent.
	 * @retval -errno on other errors.
	 */
	int  (*bo_reserve_extent)(struct m0_ad_balloc *ballroom,
				 struct m0_be_tx *tx, struct m0_ext *ext,
				 uint64_t alloc_zone);
};

enum { AD_PATHLEN = 4096 };

struct m0_stob_ad_domain {
	struct m0_format_header sad_header;
	uint64_t                sad_dom_key;
	struct m0_stob_id       sad_bstore_id;
	struct m0_ad_balloc    *sad_ballroom;
	m0_bcount_t             sad_container_size;
	uint32_t                sad_bshift;
	int32_t                 sad_babshift;
	m0_bcount_t             sad_blocks_per_group;
	m0_bcount_t             sad_spare_blocks_per_group;
	char                    sad_path[AD_PATHLEN];
	bool                    sad_overwrite;
	char                    sad_pad[7];
	struct m0_format_footer sad_footer;
	/*
	 * m0_be_emap has it's own volatile-only fields, so it can't be placed
	 * before the m0_format_footer, where only persistent fields allowed
	 */
	struct m0_be_emap       sad_adata;
	/*
	 * volatile-only fields
	 */
	struct m0_stob         *sad_bstore;
	struct m0_be_seg       *sad_be_seg;
	uint64_t                sad_magix;
} M0_XCA_RECORD M0_XCA_DOMAIN(be);
M0_BASSERT(sizeof(M0_FIELD_VALUE(struct m0_stob_ad_domain, sad_path)) % 8 == 0);
M0_BASSERT(sizeof(bool) == 1);

struct ad_domain_map {
       char                      adm_path[MAXPATHLEN];
       struct m0_stob_ad_domain *adm_dom;
       struct m0_tlink           adm_linkage;
       uint64_t                  adm_magic;
};

enum m0_stob_ad_domain_format_version {
	M0_STOB_AD_DOMAIN_FORMAT_VERSION_1 = 1,

	/* future versions, uncomment and update M0_STOB_AD_DOMAIN_FORMAT_VERSION */
	/*M0_STOB_AD_DOMAIN_FORMAT_VERSION_2,*/
	/*M0_STOB_AD_DOMAIN_FORMAT_VERSION_3,*/

	/** Current version, should point to the latest version present */
	M0_STOB_AD_DOMAIN_FORMAT_VERSION = M0_STOB_AD_DOMAIN_FORMAT_VERSION_1
};

enum {
	STOB_TYPE_AD = 0x02,
	/*
	 * As balloc space is exhausted or fragmented -
	 * we may not succeed the allocation in one request.
	 * From another side, we need to establish maximum
	 * possible number of fragments we may have from balloc
	 * to calculate BE credits at stob_ad_write_credit().
	 * That's why we have this constant. Too big value
	 * will result in excess credits, too low value will
	 * result in early ENOSPC errors when there is still
	 * some space available in balloc. So here might be
	 * the tradeoff.
	 */
	BALLOC_FRAGS_MAX = 2,
};

/**
   Types of allocation extents.

   Values of this enum are stored as "physical extent start" in allocation
   extents.
 */
enum stob_ad_allocation_extent_type {
	/**
	    Minimal "special" extent type. All values less than this are valid
	    start values of normal allocated extents.
	 */
	AET_MIN = M0_BINDEX_MAX - (1ULL << 32),
	/**
	   This value is used to tag a hole in the storage object.
	 */
	AET_HOLE
};

struct m0_stob_ad {
	struct m0_stob          ad_stob;
};

struct m0_stob_ad_io {
	struct m0_stob_io *ai_fore;
	struct m0_stob_io  ai_back;
	struct m0_clink    ai_clink;
	/**
	 * Balloc zone to which allocation be made. Note
	 * that these flags should be explicitly set before
	 * launching IO in order to succeed allocation, as
	 * they are not set by default. See @enum m0_balloc_allocation_flag.
	 */
	uint64_t           ai_balloc_flags;
};

extern const struct m0_stob_type m0_stob_ad_type;

M0_INTERNAL struct m0_balloc *
m0_stob_ad_domain2balloc(const struct m0_stob_domain *dom);

M0_INTERNAL bool m0_stob_ad_domain__invariant(struct m0_stob_ad_domain *adom);

M0_INTERNAL void m0_stob_ad_init_cfg_make(char **str, struct m0_be_domain *dom);
M0_INTERNAL void m0_stob_ad_cfg_make(char **str,
				     const struct m0_be_seg *seg,
				     const struct m0_stob_id *bstore_id,
				     const m0_bcount_t size);
M0_INTERNAL int stob_ad_cursor(struct m0_stob_ad_domain *adom,
			       struct m0_stob *obj,
			       uint64_t offset,
			       struct m0_be_emap_cursor *it);

/**
 * Sets the flags associated with the balloc zones (spare/non-spare).
 */
M0_INTERNAL void m0_stob_ad_balloc_set(struct m0_stob_io *io, uint64_t flags);

/**
 * Clears allocation context from previous IO.
 */
M0_INTERNAL void m0_stob_ad_balloc_clear(struct m0_stob_io *io);

/**
 * Calculates the count of spare blocks to be reserved for a given group.
 */
M0_INTERNAL m0_bcount_t m0_stob_ad_spares_calc(m0_bcount_t grp);

/**
 * Calculates checksum address for a cob segment and unit size
 */
M0_INTERNAL void * m0_stob_ad_get_checksum_addr(struct m0_stob_io *io, m0_bindex_t off);


/** @} end group stobad */

/* __MOTR_STOB_AD_INTERNAL_H__ */
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
