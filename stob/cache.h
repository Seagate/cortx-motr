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

#ifndef __MOTR_STOB_CACHE_H__
#define __MOTR_STOB_CACHE_H__

#include "lib/mutex.h"	/* m0_mutex */
#include "lib/tlist.h"	/* m0_tl */
#include "lib/types.h"	/* uint64_t */
#include "fid/fid.h"    /* m0_fid */

/**
 * @defgroup stob Storage object
 *
 * @todo more scalable object index instead of a list.
 *
 * @{
 */

struct m0_stob;
struct m0_stob_cache;

typedef void (*m0_stob_cache_eviction_cb_t)(struct m0_stob_cache *cache,
					    struct m0_stob *stob);
/**
 * @todo document
 */
struct m0_stob_cache {
	struct m0_mutex             sc_lock;
	struct m0_tl		    sc_busy;
	struct m0_tl		    sc_idle;
	uint64_t		    sc_idle_size;
	uint64_t		    sc_idle_used;
	m0_stob_cache_eviction_cb_t sc_eviction_cb;

	uint64_t		    sc_busy_hits;
	uint64_t		    sc_idle_hits;
	uint64_t		    sc_misses;
	uint64_t		    sc_evictions;
};

/**
 * Initialises stob cache.
 *
 * @param cache stob cache
 * @param idle_size idle list maximum size
 */
M0_INTERNAL int m0_stob_cache_init(struct m0_stob_cache *cache,
				   uint64_t idle_size,
				   m0_stob_cache_eviction_cb_t eviction_cb);
M0_INTERNAL void m0_stob_cache_fini(struct m0_stob_cache *cache);

/**
 * Stob cache invariant.
 *
 * @pre m0_stob_cache_is_locked(cache)
 * @post m0_stob_cache_is_locked(cache)
 */
M0_INTERNAL bool m0_stob_cache__invariant(const struct m0_stob_cache *cache);

/**
 * Adds stob to the stob cache. Stob should be deleted from the stob cache using
 * m0_stob_cache_idle().
 *
 * @pre m0_stob_cache_is_locked(cache)
 * @post m0_stob_cache_is_locked(cache)
 */
M0_INTERNAL void m0_stob_cache_add(struct m0_stob_cache *cache,
				   struct m0_stob *stob);

/**
 * Deletes item from the stob cache.
 *
 * @pre m0_stob_cache_is_locked(cache)
 * @post m0_stob_cache_is_locked(cache)
 */
M0_INTERNAL void m0_stob_cache_idle(struct m0_stob_cache *cache,
				   struct m0_stob *stob);

/**
 * Finds item in the stob cache. Stob found should be deleted from the stob
 * cache using m0_stob_cache_idle().
 *
 * @pre m0_stob_cache_is_locked(cache)
 * @post m0_stob_cache_is_locked(cache)
 */
M0_INTERNAL struct m0_stob *m0_stob_cache_lookup(struct m0_stob_cache *cache,
						 const struct m0_fid *stob_fid);

/**
 * Purges at most nr items from the idle stob cache.
 *
 * @pre m0_stob_cache_is_not_locked(cache)
 * @post m0_stob_cache_is_not_locked(cache)
 */
M0_INTERNAL void m0_stob_cache_purge(struct m0_stob_cache *cache, int nr);

M0_INTERNAL void m0_stob_cache_lock(struct m0_stob_cache *cache);
M0_INTERNAL void m0_stob_cache_unlock(struct m0_stob_cache *cache);
M0_INTERNAL bool m0_stob_cache_is_locked(const struct m0_stob_cache *cache);
M0_INTERNAL bool m0_stob_cache_is_not_locked(const struct m0_stob_cache *cache);

M0_INTERNAL void m0_stob_cache__print(struct m0_stob_cache *cache);


#endif /* __MOTR_STOB_CACHE_H__ */

/*
 *  Local variables:
 *  c-indentation-style: "K&R"
 *  c-basic-offset: 8
 *  tab-width: 8
 *  fill-column: 80
 *  scroll-step: 1
 *  End:
 */
