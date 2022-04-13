/*
 * Copyright (c) 2012-2022 Seagate Technology LLC and/or its Affiliates
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

#ifndef __MOTR_LIB_ALLOC_PROF_H__
#define __MOTR_LIB_ALLOC_PROF_H__

#include "lib/types.h"

/**
 * @defgroup memory Memory allocation handling functions
 *
 * @{
 *
 * Allocator profiling
 * -------------------
 *
 * alloc_prof.[ch] implements a simple profiler for the memory allocator. For
 * each allocation done through M0_ALLOC_{PTR,ARR}() macros, the profiler counts
 * the number of allocation requests together and the total number of bytes
 * allocated. All allocations done directly through m0_alloc() are counted
 * together.
 *
 * The implementation tries to reduce the run-time overhead of profiling by
 * keeping counters in a per-thread structures, which are merged into global
 * counters when the thread terminates.
 *
 * m0_alloc_prof_print() writes the current global counters (including current
 * thread and terminated threads, the counters of concurrently running threads
 * are ignored) to stdout.
 *
 * The profiler is user-space only and is activated by a configuration option
 * "./configure --enable-alloc-prof". It is disabled by default due to potential
 * overhead. When the profiler is enabled, m0_fini() calls m0_alloc_prof_print()
 * at the very end of motr finalisation:
 *
 * @verbatim
 *     type         nr        nob threads file line func obj
 *    alloc          1       4104     1 addb2/addb2.c 774 m0_addb2_module_init am
 *    alloc          2        880     1 addb2/sys.c 186 m0_addb2_sys_init sys
 *    alloc          5      61320     1 addb2/addb2.c 525 m0_addb2_mach_init mach
 *    alloc          1         80     1 ha/epoch.c 148 m0_ha_global_init hg
 *    ...
 *    alloc         22       1936     2 addb2/addb2.c 1019 buffer_alloc buf
 *    alloc          2     131072     1 lib/memory.c 128 m0_alloc m0_alloc
 * @endverbatim
 *
 * prof_alloc.[ch] can be used to profile other things, besides the memory
 * allocator. It is useful in a situation where fast counters are needed,
 * synchronisation is expensive and the number of threads is not too large.
 *
 * @todo profile calls to m0_free(); match frees with allocations; detect memory
 * leaks. The easiest way is to put all struct m0_alloc_prof-s in a special
 * section, so that they can be treated as an array. Place the index in this
 * array just before the allocated memory area (increase the size
 * appropriately).
 *
 * @todo Make m0_alloc() a macro, so that "direct" allocation sites can be
 * profiled individually.
 */

#if defined(ENABLE_ALLOC_PROF) && !defined(__KERNEL__)
#define USE_ALLOC_PROF (1)

/**
 * An instance of this struct exists for each M0_ALLOC_{PTR,ARR}()
 * call-site. Global (that ts, across all threads) counters for the call-site
 * are accumulated here.
 */
struct m0_alloc_prof {
	m0_bcount_t           ap_nob;
	m0_bcount_t           ap_nr;
	m0_bcount_t           ap_threads;
	const char           *ap_file;
	int                   ap_line;
	const char           *ap_func;
	const char           *ap_obj;
	const char           *ap_type;
	struct m0_alloc_prof *ap_prev;
};

/**
 * An instance of this struct exists for each M0_ALLOC_{PTR,ARR}() call-site in
 * each thread. Thread-local counters are accumulated there.
 */
struct m0_alloc_prof_tls {
	struct m0_alloc_prof     *pt_parent;
	struct m0_alloc_prof_tls *pt_prev;
	m0_bcount_t               pt_nob;
	m0_bcount_t               pt_nr;
};

void m0_alloc_prof_update(struct m0_alloc_prof_tls *pt, size_t nob);

/**
 * Defines global and local counter structs. Links them together. Updates local
 * counters.
 */
#define M0_ALLOC_PROF(type, obj, nob)				\
({								\
	static struct m0_alloc_prof __ap = {			\
		.ap_type = type,				\
		.ap_file = __FILE__,				\
		.ap_line = __LINE__,				\
		.ap_obj  = obj,				\
		.ap_func = __func__				\
	};							\
	static __thread struct m0_alloc_prof_tls __pt = {	\
		.pt_parent = &__ap				\
	};							\
	m0_alloc_prof_update(&__pt, nob);			\
})

#else
#define USE_ALLOC_PROF (0)
#define M0_ALLOC_PROF(type, obj, nob) ((void)0)
#endif

M0_INTERNAL void m0_alloc_prof_thread_init(void);
M0_INTERNAL void m0_alloc_prof_thread_fini(void);
M0_INTERNAL int  m0_alloc_prof_module_init(void);
M0_INTERNAL void m0_alloc_prof_module_fini(void);
M0_INTERNAL void m0_alloc_prof_print(void);

/** @} end of memory group */
#endif /* __MOTR_LIB_ALLOC_PROF_H__ */

/*
 *  Local variables:
 *  c-indentation-style: "K&R"
 *  c-basic-offset: 8
 *  tab-width: 8
 *  fill-column: 80
 *  scroll-step: 1
 *  End:
 */
