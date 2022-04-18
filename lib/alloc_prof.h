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
 * allocated. All allocations done directly through m0_alloc() and
 * m0_alloc_aligned() are counted together. The identity of the callsite is
 * recorded at the beginning of every allocated memory region and used by
 * m0_free() to identify and update the callsite counters.
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
[     alloc -       free =     remain] [     total] [ tha  -   thf]
[         2 -          2 =          0] [       144] [    1 -     1] lib/thread_pool.c 160 pool_threads_init pool->pp_queue
[         2 -          2 =          0] [       360] [    1 -     1] lib/thread_pool.c 159 pool_threads_init pool->pp_qlinks
[         2 -          2 =          0] [      3680] [    1 -     1] lib/thread_pool.c 158 pool_threads_init pool->pp_threads
[         1 -          1 =          0] [        80] [    1 -     1] lib/ut/zerovec.c 103 zerovec_init_bufs bufs
[         1 -          1 =          0] [        80] [    1 -     1] lib/ut/zerovec.c 65 zerovec_init_bvec bufvec.ov_buf
[         1 -          1 =          0] [        80] [    1 -     1] lib/ut/zerovec.c 63 zerovec_init_bvec bufvec.ov_vec.v_count
[         3 -          3 =          0] [       240] [    1 -     1] lib/vec.c 842 m0_0vec_init zvec->z_bvec.ov_buf
[         3 -          3 =          0] [       240] [    1 -     1] lib/vec.c 836 m0_0vec_init zvec->z_bvec.ov_vec.v_count
[         3 -          3 =          0] [       240] [    1 -     1] lib/vec.c 832 m0_0vec_init zvec->z_index
[    525000 -     525000 =          0] [   1134000] [    1 -     1] lib/ut/vec.c 676 split vec->ov_buf[pos]
[     42000 -      42000 =          0] [   4200000] [    1 -     1] lib/ut/vec.c 671 split vec->ov_buf
[     42000 -      42000 =          0] [   4200000] [    1 -     1] lib/ut/vec.c 669 split vec->ov_vec.v_count
[     42000 -          0 =      42000] [   1008000] [    1 -     0] lib/ut/vec.c 667 split vec
[         1 -          1 =          0] [      1208] [    1 -     1] lib/ut/vec.c 164 test_indexvec_varr_cursor ivv
...
[    191870 -     191861 =          9] [  93475945] [  275 -    18] lib/memory.c 131 m0_alloc alloc
 * @endverbatim
 *
 * Each line represents an allocation call-site, identified by file, line
 * number, function and allocated object name (in case of M0_ALLOC_*() macros).
 * Here
 *
 *     - "alloc" is the total number of allocations made at this call-site,
 *
 *     - "free" total number of corresponding free calls,
 *
 *     - "remain" is the number of still allocated objects,
 *
 *     - "total" is the total amount of bytes allocated at the site, "total"
 *       only goes up and is not decreased by freeing calls,
 *
 *     - "tha" is the number of threads that allocated memory at this site and
 *
 *     - "thf" is the number of threads that freed memory at this site.
 *
 * To identify large memory users, sort by the "total".
 *
 * To identify memory leaks, see which callsites have non-zero "remain". In the
 * example above, lib/ut/vec.c:667 clearly leaks, none of allocations made at
 * this line is freed. Negative "remain" values mean that some threads that
 * allocated at this callsite haven't terminated.
 *
 * @todo Make m0_alloc() a macro, so that "direct" allocation sites can be
 * profiled individually.
 *
 * @todo Use m0_alloc_callsite::ap_flags to track certain callsites. For
 * example, when a certain flag is set, the current stack trace is logged on
 * every allocation or free call on the callsite. Provide macros to set the
 * flags. This would provide a mechanism to trace memory leaks.
 */

#if defined(ENABLE_ALLOC_PROF) && !defined(__KERNEL__)
#define USE_ALLOC_PROF (1)
enum { AP_ALLOC, AP_FREE, AP_NR };

/**
 * An instance of this struct exists for each M0_ALLOC_{PTR,ARR}()
 * call-site. Global (that ts, across all threads) counters for the call-site
 * are accumulated here.
 */
struct m0_alloc_callsite {
	struct {
		/** Total number of bytes allocated or freed. */
		m0_bcount_t s_nob; /* Sine nobilitate. */
		/** Total number of alloc or free calls. */
		m0_bcount_t s_nr;
		/**
		 * Total number of threads that allocated or freed at this site.
		 */
		m0_bcount_t s_threads;
	}                         ap_s[AP_NR];
	/**
	 * Global callsite index, assigned lazily by
	 * m0_alloc_callsite_init().
	 */
	int                       ap_idx;
	const char               *ap_file;
	int                       ap_line;
	const char               *ap_func;
	/** Allocated variable, @see M0_ALLOC_PTR(). */
	const char               *ap_obj;
	/** Currently unused, see @todo above. */
	uint64_t                  ap_flags;
	/**
	 * Linkage to the global list of callsites, starting at
	 * alloc_prof.c:head.
	 */
	struct m0_alloc_callsite *ap_prev;
};

void m0_alloc_callsite_init(struct m0_alloc_callsite *cs);

#define M0_ALLOC_CALLSITE(obj, flags)			\
({							\
	static struct m0_alloc_callsite __cs = {	\
		.ap_idx   = -1,				\
		.ap_flags = (flags),			\
		.ap_file  = __FILE__,			\
		.ap_line  = __LINE__,			\
		.ap_obj   = (obj),			\
		.ap_func  = __func__			\
	};						\
	m0_alloc_callsite_init(&__cs);			\
	&__cs;						\
})

#else
#define USE_ALLOC_PROF (0)
#define M0_ALLOC_CALLSITE(obj, flags) (NULL)
#endif

struct m0_alloc_callsite;
M0_INTERNAL void m0_alloc_callsite_mod(struct m0_alloc_callsite *cs,
				       int dir, size_t nob);
M0_INTERNAL void m0_alloc_prof_thread_init(void);
M0_INTERNAL void m0_alloc_prof_thread_sync(void);
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
