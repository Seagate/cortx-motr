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

#ifndef __MOTR_LIB_LOCALITY_H__
#define __MOTR_LIB_LOCALITY_H__

/**
 * @defgroup locality
 *
 * @{
 */

/* import */
#include "lib/types.h"
#include "lib/processor.h"
#include "lib/lockers.h"
#include "lib/tlist.h"
#include "lib/time.h"
#include "lib/chan.h"
#include "lib/mutex.h"
#include "sm/sm.h"
struct m0_reqh;
struct m0_fom_domain;

/* export */
struct m0_locality;
struct m0_locality_chore;
struct m0_locality_chore_ops;

enum { M0_LOCALITY_LOCKERS_NR = 256 };
M0_LOCKERS_DECLARE(M0_EXTERN, m0_locality, M0_LOCALITY_LOCKERS_NR);

/**
 * Per-core state maintained by Motr.
 */
struct m0_locality {
	/**
	 * State machine group associated with the core.
	 *
	 * This group can be used to post ASTs to be executed on a current or
	 * specified core.
	 *
	 * This group comes from request handler locality, so that execution of
	 * ASTs is serialised with state transitions of foms in this locality.
	 */
	struct m0_sm_group        *lo_grp;
	size_t                     lo_idx;
	struct m0_fom_domain      *lo_dom;
	/** Lockers to store locality-specific private data */
	struct m0_locality_lockers lo_lockers;
	struct m0_tl               lo_chores;
};

M0_INTERNAL void m0_locality_init(struct m0_locality *loc,
				  struct m0_sm_group *grp,
				  struct m0_fom_domain *dom, size_t idx);
M0_INTERNAL void m0_locality_fini(struct m0_locality *loc);

/**
 * Returns locality corresponding to the CPU core the thread (from which
 * the call is made) was initially run on.
 *
 * Locality threads are bound to the cores (unless the affinity
 * is not reset externally, for example, by numad), see loc_thr_init().
 * All the rest threads might change their CPU cores over time. In any case,
 * the returned locality is always the same and corresponds to the CPU core
 * the thread was run the very first time.
 *
 * If the call is made from the global fallback locality thread, returns the
 * fallback locality (lg_fallback), regardless of the CPU core it runs on.
 *
 * @post result->lo_grp != NULL
 */
M0_INTERNAL struct m0_locality *m0_locality_here(void);

/**
 * Returns locality corresponding in some unspecified, but deterministic way to
 * the supplied value.
 *
 * @post result->lo_grp != NULL
 */
M0_INTERNAL struct m0_locality *m0_locality_get(uint64_t value);

M0_INTERNAL struct m0_locality *m0_locality0_get(void);

/**
 * Starts using localities from the specified domain.
 */
M0_INTERNAL void m0_locality_dom_set    (struct m0_fom_domain *dom);

/**
 * Stops using the domain, falls back to a single locality.
 */
M0_INTERNAL void m0_locality_dom_unset(struct m0_fom_domain *dom);

M0_INTERNAL int  m0_localities_init(void);
M0_INTERNAL void m0_localities_fini(void);

struct m0_locality_chore {
	const struct m0_locality_chore_ops *lc_ops;
	void                               *lc_datum;
	m0_time_t                           lc_interval;
	struct m0_mutex                     lc_lock;
	struct m0_chan                      lc_signal;
	int                                 lc_active;
	struct m0_tlink                     lc_linkage;
	struct m0_sm_ast                    lc_ast;
	size_t                              lc_datasize;
	int                                 lc_rc;
	uint64_t                            lc_magix;
};

struct m0_locality_chore_ops {
	const char *co_name;
	int (*co_enter)(struct m0_locality_chore *chore,
			struct m0_locality *loc, void *place);
	void (*co_leave)(struct m0_locality_chore *chore,
			 struct m0_locality *loc, void *place);
	void (*co_tick)(struct m0_locality_chore *chore,
			struct m0_locality *loc, void *place);
};

int m0_locality_chore_init(struct m0_locality_chore *chore,
			   const struct m0_locality_chore_ops *ops,
			   void *datum, m0_time_t interval,
			   size_t datasize);
void m0_locality_chore_quit(struct m0_locality_chore *chore);
void m0_locality_chore_fini(struct m0_locality_chore *chore);

M0_INTERNAL void m0_locality_chores_run(struct m0_locality *locality);

int   m0_locality_data_alloc(size_t nob, int (*ctor)(void *, void *),
			     void (*dtor)(void *, void *), void *datum);
void  m0_locality_data_free (int key);
void *m0_locality_data      (int key);
void  m0_locality_data_iterate(int key,
			       void (*func)(int idx, void *data, void *datum),
			       void *datum);

int m0_locality_call(struct m0_locality *loc, int (*cb)(void *), void *data);

M0_INTERNAL struct m0_fom_domain *m0_fom_dom(void);

/** @} end of locality group */

#endif /* __MOTR_LIB_LOCALITY_H__ */

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
