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

#ifndef __MOTR_BE_DOMAIN_H__
#define __MOTR_BE_DOMAIN_H__

#include "be/engine.h"          /* m0_be_engine */
#include "be/seg0.h"            /* m0_be_0type */
#include "be/log.h"             /* m0_be_log_cfg */
#include "be/pd.h"              /* m0_be_pd */
#include "be/log_discard.h"     /* m0_be_log_discard */
#include "stob/stob.h"          /* m0_stob_id */
#include "stob/stob_xc.h"

#include "lib/tlist.h"          /* m0_tl */
#include "module/module.h"

/**
 * @defgroup be Meta-data back-end
 *
 * @b BE domain initialisation
 *
 * Mkfs mode:
 *
 * - Create BE seg0 and a log on storage (configuration is taken from dom_cfg).
 * - Create segs_nr segments (configuration for each segment is taken from
 *   segs_cfg).
 *
 * Normal (non-mkfs) mode:
 *
 * - Load seg0 (domain should be started in mkfs mode before it can be started
 *   in normal mode).
 * - Process all 0types stored in seg0. Initialise the log and all segments
 *   in the domain, along with other 0types.
 *
 * In either case after successful initialisation BE domain is ready to work.
 *
 * @see m0_be_domain_cfg, m0_be_domain_seg_create().
 *
 * @{
 */

struct m0_stob_id;
struct m0_be_tx;
struct m0_be_0type;
struct m0_be_log;

struct m0_be_0type_seg_cfg {
	uint64_t     bsc_stob_key;
	bool	     bsc_preallocate;
	m0_bcount_t  bsc_size;
	void	    *bsc_addr;
	const char  *bsc_stob_create_cfg;
} M0_XCA_RECORD M0_XCA_DOMAIN(be);

struct m0_be_domain_cfg {
	/** BE engine configuration. */
	struct m0_be_engine_cfg      bc_engine;
	/** 0types supported by this domain. */
	const struct m0_be_0type   **bc_0types;
	unsigned                     bc_0types_nr;
	/**
	 * Stob domain location.
	 * Stobs for log and segments are in this domain.
	 */
	const char                  *bc_stob_domain_location;
	/**
	 * str_cfg_init parameter for m0_stob_domain_init() (in normal mode)
	 * and m0_stob_domain_create() (in mkfs mode).
	 */
	const char                  *bc_stob_domain_cfg_init;
	/**
	 * seg0 stob key.
	 * This field is ignored in mkfs mode.
	 */
	uint64_t                     bc_seg0_stob_key;
	bool                         bc_mkfs_mode;
	/**
	 * Percent of free space to be reserved for every allocator memory zone.
	 * The sum of all array elements should be 100.
	 */
	uint32_t                     bc_zone_pcnt[M0_BAP_NR];

	/*
	 * Next fields are for mkfs mode only.
	 * They are completely ignored in normal mode.
	 */

	/** str_cfg_create parameter for m0_stob_domain_create(). */
	const char                  *bc_stob_domain_cfg_create;
	/**
	 * Stob domain key for BE stobs. Stob domain with this key is
	 * created at m0_be_domain_cfg::bc_stob_domain_location.
	 */
	uint64_t                     bc_stob_domain_key;
	/** BE log configuration. */
	struct m0_be_log_cfg         bc_log;
	/** BE seg0 configuration. */
	struct m0_be_0type_seg_cfg   bc_seg0_cfg;
	/**
	 * Array of segment configurations.
	 * - can be NULL;
	 * - should be NULL if m0_be_domain_cfg::bc_seg_nr == 0.
	 * */
	struct m0_be_0type_seg_cfg  *bc_seg_cfg;
	/** Size of m0_be_domain_cfg::bc_seg_cfg array. */
	unsigned                     bc_seg_nr;
	struct m0_be_pd_cfg          bc_pd_cfg;
	struct m0_be_log_discard_cfg bc_log_discard_cfg;
};

struct m0_be_domain {
	struct m0_module          bd_module;
	struct m0_be_domain_cfg   bd_cfg;
	struct m0_be_engine       bd_engine;
	/** The main lock in m0_be_engine. Also it's used in m0_be_log */
	struct m0_mutex           bd_engine_lock;
	/** Protects m0_be_domain::bd_0types and m0_be_domain::bd_segs */
	struct m0_mutex           bd_lock;
	struct m0_tl              bd_0types;
	struct m0_be_0type       *bd_0types_allocated;
	/** List of segments in this domain. First segment in which is seg0. */
	struct m0_tl              bd_segs;
	struct m0_be_seg          bd_seg0;
	struct m0_stob           *bd_seg0_stob;
	struct m0_stob_domain    *bd_stob_domain;
	struct m0_be_0type        bd_0type_seg;
	struct m0_be_pd           bd_pd;
	struct m0_be_log_discard  bd_log_discard;
};

/** Levels of m0_be_domain module. */
enum {
	M0_BE_DOMAIN_LEVEL_INIT,
	M0_BE_DOMAIN_LEVEL_0TYPES_REGISTER,
	M0_BE_DOMAIN_LEVEL_MKFS_STOB_DOMAIN_DESTROY,
	M0_BE_DOMAIN_LEVEL_MKFS_STOB_DOMAIN_CREATE,
	M0_BE_DOMAIN_LEVEL_NORMAL_STOB_DOMAIN_INIT,
	M0_BE_DOMAIN_LEVEL_LOG_CONFIGURE,
	M0_BE_DOMAIN_LEVEL_MKFS_LOG_CREATE,
	M0_BE_DOMAIN_LEVEL_NORMAL_LOG_OPEN,
	M0_BE_DOMAIN_LEVEL_PD_INIT,
	M0_BE_DOMAIN_LEVEL_NORMAL_SEG0_OPEN,
	M0_BE_DOMAIN_LEVEL_NORMAL_0TYPES_VISIT,
	M0_BE_DOMAIN_LEVEL_LOG_DISCARD_INIT,
	M0_BE_DOMAIN_LEVEL_ENGINE_INIT,
	M0_BE_DOMAIN_LEVEL_ENGINE_START,
	M0_BE_DOMAIN_LEVEL_MKFS_SEG0_CREATE,
	M0_BE_DOMAIN_LEVEL_MKFS_SEG0_STRUCTS_CREATE,
	M0_BE_DOMAIN_LEVEL_MKFS_SEG0_0TYPE,
	M0_BE_DOMAIN_LEVEL_MKFS_SEGMENTS_CREATE,
	M0_BE_DOMAIN_LEVEL_READY,
};
/*
 * To iterate segment objects
 * */
M0_INTERNAL int m0_be_segobj_opt_next(struct m0_be_seg         *dict,
			              const struct m0_be_0type *objtype,
			              struct m0_buf            *opt,
			              char                    **suffix);
M0_INTERNAL int m0_be_segobj_opt_begin(struct m0_be_seg         *dict,
			               const struct m0_be_0type *objtype,
			               struct m0_buf            *opt,
			               char                    **suffix);
M0_INTERNAL void m0_be_domain_module_setup(struct m0_be_domain *dom,
					   const struct m0_be_domain_cfg *cfg);
M0_INTERNAL void
be_domain_log_cleanup(const char *stob_domain_location,
		      struct m0_be_log_cfg *log_cfg, bool create);

/*
 * Temporary solution until segment I/O is implemented using direct I/O.
 */
M0_INTERNAL void
m0_be_domain_cleanup_by_location(const char *stob_domain_location);

M0_INTERNAL struct m0_be_tx *m0_be_domain_tx_find(struct m0_be_domain *dom,
						  uint64_t id);

M0_INTERNAL struct m0_be_engine *m0_be_domain_engine(struct m0_be_domain *dom);
M0_INTERNAL
struct m0_be_seg *m0_be_domain_seg0_get(struct m0_be_domain *dom);
/* TODO remove the function after BE log becomes a part of BE domain */
M0_INTERNAL struct m0_be_log *m0_be_domain_log(struct m0_be_domain *dom);
M0_INTERNAL bool m0_be_domain_is_locked(const struct m0_be_domain *dom);

/**
 * Returns existing BE segment if @addr is inside it. Returns NULL otherwise.
 */
M0_INTERNAL struct m0_be_seg *m0_be_domain_seg(const struct m0_be_domain *dom,
					       const void                *addr);
/**
 * Returns existing be-segment by its @id. If no segments found return NULL.
 */
M0_INTERNAL struct m0_be_seg *
m0_be_domain_seg_by_id(const struct m0_be_domain *dom, uint64_t id);

/**
 * Returns be-segment first after seg0 or NULL if not existing.
 */
M0_INTERNAL struct m0_be_seg *
m0_be_domain_seg_first(const struct m0_be_domain *dom);

M0_INTERNAL void
m0_be_domain_seg_create_credit(struct m0_be_domain              *dom,
			       const struct m0_be_0type_seg_cfg *seg_cfg,
			       struct m0_be_tx_credit           *cred);
M0_INTERNAL void m0_be_domain_seg_destroy_credit(struct m0_be_domain    *dom,
						 struct m0_be_seg       *seg,
						 struct m0_be_tx_credit *cred);

M0_INTERNAL int
m0_be_domain_seg_create(struct m0_be_domain               *dom,
			struct m0_be_tx                   *tx,
			const struct m0_be_0type_seg_cfg  *seg_cfg,
			struct m0_be_seg                 **out);
/**
 * Destroy functions for the segment dictionary and for the segment allocator
 * are not called.
 *
 * Current code doesn't do all necessary cleaning to be sure that nothing is
 * allocated and there is no entry in segment dictionary before segment is
 * destroyed.
 *
 * However, it may be a problem with seg0 entries if objects they are describing
 * were on destroyed segment. It may even lead to ABA problem. There is no such
 * mechanism (yet) to keep track of these "lost" entries, so they are
 * responsibility of a developer.
 */
M0_INTERNAL int m0_be_domain_seg_destroy(struct m0_be_domain *dom,
					 struct m0_be_tx     *tx,
					 struct m0_be_seg    *seg);

/* for internal be-usage only */
M0_INTERNAL void m0_be_domain__0type_register(struct m0_be_domain *dom,
					      struct m0_be_0type  *type);
M0_INTERNAL void m0_be_domain__0type_unregister(struct m0_be_domain *dom,
						struct m0_be_0type  *type);

M0_INTERNAL void m0_be_domain_tx_size_max(struct m0_be_domain    *dom,
                                          struct m0_be_tx_credit *cred,
                                          m0_bcount_t            *payload_size);

M0_INTERNAL void m0_be_domain__group_limits(struct m0_be_domain *dom,
                                            uint32_t            *group_nr,
                                            uint32_t            *tx_per_group);

/**
 * Check if the stob with the stob_id is used as a log stob
 * for the given domain.
 */
M0_INTERNAL bool m0_be_domain_is_stob_log(struct m0_be_domain     *dom,
                                          const struct m0_stob_id *stob_id);
/**
 * Check if the stob with the stob_id is used as a seg stob
 * for the given domain.
 */
M0_INTERNAL bool m0_be_domain_is_stob_seg(struct m0_be_domain     *dom,
                                          const struct m0_stob_id *stob_id);

/**
 * Returns m0_be_seg the void * pointer is a part of.
 * Returns NULL if there is no such segment.
 *
 * Note: please don't cache m0_be_seg if it doesn't significantly improve
 * the general performance significanly.
 * @see m0_be_domain_seg_is_valid().
 */
M0_INTERNAL struct m0_be_seg *
m0_be_domain_seg_by_addr(struct m0_be_domain *dom,
                         void                *addr);

/**
 * The primary use for this function is to use it with conjunction with
 * m0_be_domain_seg_by_addr(). The typical use case is to cache the segment and
 * then check on each access if it's still valid. The pointer can't be just
 * dereferenced directly because the segment can be destroyed at any moment and
 * current interface doesn't provide a way to track this event.
 */
M0_INTERNAL bool m0_be_domain_seg_is_valid(struct m0_be_domain *dom,
                                           struct m0_be_seg    *seg);


/** @} end of be group */
#endif /* __MOTR_BE_DOMAIN_H__ */

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
