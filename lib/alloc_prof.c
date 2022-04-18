/* -*- C -*- */
/*
 * Copyright (c) 2015-2022 Seagate Technology LLC and/or its Affiliates
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



/**
 * @addtogroup memory
 *
 * @{
 */

#define M0_TRACE_SUBSYSTEM M0_TRACE_SUBSYS_MEMORY
#include "lib/trace.h"
#include "lib/alloc_prof.h"
#include "lib/mutex.h"

#if USE_ALLOC_PROF

/**
 * An instance of this struct exists for each M0_ALLOC_{PTR,ARR}() call-site in
 * each thread. Thread-local counters are accumulated there.
 */
struct prof_rec {
	struct m0_alloc_callsite *pr_parent;
	struct {
		m0_bcount_t r_nob;
		m0_bcount_t r_nr;
	}                         pr_s[AP_NR];
};

enum {
      /**
       * The number of local counter allocated per-thread.
       *
       * This should be larger tha the total number of callsites
       * (m0_alloc_callsite) in the executable.
       */
      SLOTS = 2000
};
/** Per-thread local allocation counters. */
static __thread struct prof_rec  recs[SLOTS];
static struct m0_mutex           guard;
static int                       idx;
static struct m0_alloc_callsite  eol;
static struct m0_alloc_callsite *head        = &eol;
static bool                      initialised = false;

static void init(void);

void m0_alloc_callsite_init(struct m0_alloc_callsite *cs)
{
	if (cs->ap_idx == -1) { /* On the first access, assign the index. */
		 /*
		  * Allocator can be called before m0_init(), e.g., from
		  * m0_getopts(). Initialise if necessary.
		  */
		init();
		m0_mutex_lock(&guard);
		if (cs->ap_idx == -1) { /* Re-check under the lock. */
			M0_ASSERT(cs->ap_prev == NULL);
			cs->ap_prev = head;
			head = cs;
			cs->ap_idx = idx++;
		}
		m0_mutex_unlock(&guard);
	}
}

M0_INTERNAL void m0_alloc_callsite_mod(struct m0_alloc_callsite *cs,
				       int dir, size_t nob)
{
	struct prof_rec *rec;

	M0_ASSERT(IS_IN_ARRAY(cs->ap_idx, recs));
	rec = &recs[cs->ap_idx];
	M0_ASSERT(M0_IN(rec->pr_parent, (cs, NULL)));
	M0_ASSERT(IS_IN_ARRAY(dir, rec->pr_s));
	rec->pr_parent = cs;
	rec->pr_s[dir].r_nob += nob;
	rec->pr_s[dir].r_nr++;
}

M0_INTERNAL void m0_alloc_prof_thread_init(void)
{
}

M0_INTERNAL void m0_alloc_prof_thread_fini(void)
{
	m0_alloc_prof_thread_sync();
}

/** Merges local counters into global ones. */
M0_INTERNAL void m0_alloc_prof_thread_sync(void)
{
	int i;
	int j;

	m0_mutex_lock(&guard);
	for (i = 0; i < ARRAY_SIZE(recs); ++i) {
		struct prof_rec          *scan = &recs[i];
		struct m0_alloc_callsite *ap   = scan->pr_parent;

		if (ap == NULL)
			continue;
		M0_ASSERT(ap->ap_idx == i);
		for (j = 0; j < AP_NR; ++j) {
			ap->ap_s[j].s_nob += scan->pr_s[j].r_nob;
			ap->ap_s[j].s_nr  += scan->pr_s[j].r_nr;
			if (scan->pr_s[j].r_nr > 0)
				ap->ap_s[j].s_threads++;
			/*
			 * This function can be executed multiple times for the
			 * same thread, count only once.
			 */
			scan->pr_s[j].r_nob = scan->pr_s[j].r_nr = 0;
		}
	}
	m0_mutex_unlock(&guard);
}

M0_INTERNAL int m0_alloc_prof_module_init(void)
{
	init();
	return 0;
}

M0_INTERNAL void m0_alloc_prof_module_fini(void)
{
	m0_alloc_prof_print();
	m0_mutex_fini(&guard);
}

M0_INTERNAL void m0_alloc_prof_print(void)
{
	struct m0_alloc_callsite *scan;

	m0_alloc_prof_thread_sync();
	m0_mutex_lock(&guard);
	for (scan = head; scan != &eol; scan = scan->ap_prev) {
		printf("[%10"PRId64" - %10"PRId64" = %10"PRId64"] "
		       "[%10"PRId64"] "
		       "[%5"PRId64" - %5"PRId64"] %s %d %s %s\n",
		       scan->ap_s[AP_ALLOC].s_nr, scan->ap_s[AP_FREE].s_nr,
		       scan->ap_s[AP_ALLOC].s_nr - scan->ap_s[AP_FREE].s_nr,
		       scan->ap_s[AP_ALLOC].s_nob,
		       scan->ap_s[AP_ALLOC].s_threads,
		       scan->ap_s[AP_FREE].s_threads, scan->ap_file,
		       scan->ap_line, scan->ap_func, scan->ap_obj);
	}
	m0_mutex_unlock(&guard);
}

static void init(void)
{
	if (!initialised) {
		initialised = true;
		m0_mutex_init(&guard);
	}
}

#else

M0_INTERNAL void m0_alloc_callsite_mod(struct m0_alloc_callsite *cs,
				       int dir, size_t nob)
{;}

M0_INTERNAL void m0_alloc_prof_thread_init(void)
{;}

M0_INTERNAL void m0_alloc_prof_thread_fini(void)
{;}

M0_INTERNAL void m0_alloc_prof_thread_sync(void)
{;}

M0_INTERNAL int  m0_alloc_prof_module_init(void)
{
	return 0;
}

M0_INTERNAL void m0_alloc_prof_module_fini(void)
{;}

M0_INTERNAL void m0_alloc_prof_print(void)
{;}

struct m0_alloc_callsite;
void m0_alloc_callsite_init(struct m0_alloc_callsite *cs, int dir, size_t nob)
{ /* Has to be defined: part of public API. */ ;}

#endif

#undef M0_TRACE_SUBSYSTEM

/** @} end of memory group */

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
