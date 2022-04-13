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

static __thread struct m0_alloc_prof_tls  ground;
static __thread struct m0_alloc_prof_tls *anchor;

static struct m0_alloc_prof  a_ground;
static struct m0_alloc_prof *a_anchor = &a_ground;

static struct m0_mutex guard;

/**
 * Updates the local counters. On the first access, links the local counters
 * struct in per-thread single-linked list hanging off of anchor.
 */
void m0_alloc_prof_update(struct m0_alloc_prof_tls *pt, size_t nob)
{
	pt->pt_nob += nob;
	pt->pt_nr ++;
	if (pt->pt_prev == NULL) {
		pt->pt_prev = anchor;
		anchor = pt;
	}
}

M0_INTERNAL void m0_alloc_prof_thread_init(void)
{
	anchor = &ground;
}

/**
 * Merges local counters into global ones.
 *
 * On the first access, links the global counters structure in the global list
 * hanging off of a_anchor.
 */
M0_INTERNAL void m0_alloc_prof_thread_fini(void)
{
	struct m0_alloc_prof_tls *scan;

	m0_mutex_lock(&guard);
	for (scan = anchor; scan != &ground; scan = scan->pt_prev) {
		struct m0_alloc_prof *ap = scan->pt_parent;

		ap->ap_nob += scan->pt_nob;
		ap->ap_nr += scan->pt_nr;
		if (scan->pt_nr > 0)
			ap->ap_threads ++;
		/*
		 * This function can be executed multiple times for the same
		 * thread, take care to count once.
		 */
		scan->pt_nob = scan->pt_nr = 0;
		if (ap->ap_prev == NULL) {
			ap->ap_prev = a_anchor;
			a_anchor = ap;
		}
	}
	m0_mutex_unlock(&guard);
}

M0_INTERNAL int m0_alloc_prof_module_init(void)
{
	m0_mutex_init(&guard);
	m0_alloc_prof_thread_init();
	return 0;
}

M0_INTERNAL void m0_alloc_prof_module_fini(void)
{
	m0_alloc_prof_thread_fini();
	m0_alloc_prof_print();
	m0_mutex_fini(&guard);
}

M0_INTERNAL void m0_alloc_prof_print(void)
{
	struct m0_alloc_prof *scan;

	m0_alloc_prof_thread_fini(); /* Merge current thread. */
	printf("%10s %10s %10s %5s %s %s %s %s\n",
	       "type", "nr", "nob", "threads", "file", "line", "func", "obj");
	m0_mutex_lock(&guard);
	for (scan = a_anchor; scan != &a_ground; scan = scan->ap_prev) {
		printf("%10s %10"PRId64" %10"PRId64" %5"PRId64" %s %i %s %s\n",
		       scan->ap_type, scan->ap_nr, scan->ap_nob,
		       scan->ap_threads, scan->ap_file,
		       scan->ap_line, scan->ap_func, scan->ap_obj);
	}
	m0_mutex_unlock(&guard);
}

#else

M0_INTERNAL void m0_alloc_prof_thread_init(void)
{;}

M0_INTERNAL void m0_alloc_prof_thread_fini(void)
{;}

M0_INTERNAL int  m0_alloc_prof_module_init(void)
{
	return 0;
}

M0_INTERNAL void m0_alloc_prof_module_fini(void)
{;}

M0_INTERNAL void m0_alloc_prof_print(void)
{;}

struct m0_alloc_prof_tls;
void m0_alloc_prof_update(struct m0_alloc_prof_tls *pt, size_t nob)
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
