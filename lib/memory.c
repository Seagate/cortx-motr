/* -*- C -*- */
/*
 * Copyright (c) 2015-2020 Seagate Technology LLC and/or its Affiliates
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
#include "lib/arith.h"   /* min_type, m0_is_po2 */
#include "lib/assert.h"
#include "lib/atomic.h"
#include "lib/memory.h"
#include "lib/finject.h"
#include "lib/misc.h"   /* m0_round_down */
#include "lib/alloc_prof.h"
#include "motr/magic.h" /* M0_MEMORY_AP_MAGIX, M0_MEMORY_AA_MAGIX */

enum { U_POISON_BYTE = 0x5f };

#ifdef ENABLE_DEV_MODE
#define DEV_MODE (true)
#else
#define DEV_MODE (false)
#endif

#ifdef ENABLE_FREE_POISON
static void poison_before_free(void *data, size_t size)
{
	memset(data, U_POISON_BYTE, size);
}

/**
 * Returns true, iff the value is poisoned by m0_free().
 *
 * Do not compare all bytes of the value with U_POISON_BYTE, because the value
 * can be obtained by adding an offset to a poisoned pointer.
 */
static bool is_poisoned(uint64_t val)
{
	M0_CASSERT(U_POISON_BYTE == 0x5f);
	return (val & 0x00ffffffff000000ULL) == 0x005f5f5f5f000000ULL;
}

M0_INTERNAL bool m0_is_poisoned(const void *p)
{
	/*
	 * Check two cases: "p" is a ...
	 *
	 *     - pointer to a freed object, is_poisoned(*p) is true;
	 *     - pointer field within a freed object, is_poisoned(p) is true;
	 */
	return is_poisoned((uint64_t)p) || is_poisoned(*(const uint64_t *)p);
}
#else
static void poison_before_free(void *data, size_t size)
{;}

M0_INTERNAL bool m0_is_poisoned(const void *ptr)
{
	return false;
}
#endif

M0_INTERNAL void  *m0_arch_alloc       (size_t size);
M0_INTERNAL void   m0_arch_free        (void *data);
M0_INTERNAL void   m0_arch_allocated_zero(void *data, size_t size);
M0_INTERNAL void  *m0_arch_alloc_nz    (size_t size);
M0_INTERNAL void   m0_arch_memory_pagein(void *addr, size_t size);
M0_INTERNAL size_t m0_arch_alloc_size(void *data);
M0_INTERNAL void  *m0_arch_alloc_wired(size_t size, unsigned shift);
M0_INTERNAL void   m0_arch_free_wired(void *data, size_t size, unsigned shift);
M0_INTERNAL void  *m0_arch_alloc_aligned(size_t alignment, size_t size);
M0_INTERNAL void   m0_arch_free_aligned(void *data, size_t size, unsigned shft);
M0_INTERNAL int    m0_arch_pagesize_get(void);
M0_INTERNAL int    m0_arch_pageshift_get(void);
M0_INTERNAL int    m0_arch_dont_dump(void *p, size_t size);
M0_INTERNAL int    m0_arch_memory_init (void);
M0_INTERNAL void   m0_arch_memory_fini (void);

static void *alloc0(size_t size);
static void  free0(void *data);
static void *alloc0_aligned(size_t size, unsigned shift);
static void  free0_aligned(void *data, size_t size, unsigned shift);

static struct m0_atomic64 allocated;
static struct m0_atomic64 cumulative_alloc;
static struct m0_atomic64 cumulative_free;

static void alloc_tail(void *area, size_t size)
{
	if (DEV_MODE && area != NULL) {
		size_t asize = m0_arch_alloc_size(area);

		m0_atomic64_add(&allocated, asize);
		m0_atomic64_add(&cumulative_alloc, asize);
	}
}

#if USE_ALLOC_PROF

M0_INTERNAL void *m0_alloc_nz(size_t size)
{
	return m0_alloc_profiled(size, M0_ALLOC_CALLSITE("alloc-nz", 0));
}

void *m0_alloc(size_t size)
{
	return m0_alloc_profiled(size, M0_ALLOC_CALLSITE("alloc", 0));
}

M0_INTERNAL void *m0_alloc_aligned(size_t size, unsigned shift)
{
	return m0_alloc_aligned_profiled(size, shift,
					 M0_ALLOC_CALLSITE("alloc-aligned", 0));
}

struct pad {
	struct m0_alloc_callsite *p_cs;
	uint64_t                  p_magic;
};

void *m0_alloc_profiled(size_t size, struct m0_alloc_callsite *cs)
{
	struct pad *area = alloc0(size + sizeof(struct pad));
	if (area != NULL) {
		m0_alloc_callsite_mod(cs, AP_ALLOC, size);
		area[0] = (struct pad){ .p_cs = cs,
					.p_magic = M0_MEMORY_AP_MAGIX };
		area++;
	}
	return area;
}

struct aligned_pad {
	struct m0_alloc_callsite *a_cs;
	uint32_t                  a_shift;
	uint32_t                  a_size;
	uint64_t                  a_magic;
};

M0_INTERNAL void *m0_alloc_aligned_profiled(size_t size, unsigned shift,
					    struct m0_alloc_callsite *cs)
{
	struct aligned_pad *apad;
	void               *area;

	shift = max32(shift, 5);
	M0_PRE(sizeof *apad <= (1 << shift));
	area = alloc0_aligned(size + (1 << shift), shift);
	if (area != NULL) {
		m0_alloc_callsite_mod(cs, AP_ALLOC, size);
		apad = area += (1 << shift);
		apad[-1] = (struct aligned_pad) { .a_cs = cs, .a_shift = shift,
			     .a_size = size, .a_magic = M0_MEMORY_AA_MAGIX };
	}
	return area;
}

void m0_free(void *data)
{
	if (data != NULL) {
		struct pad *p = ((struct pad *)data) - 1;
		if (p->p_magic == M0_MEMORY_AA_MAGIX) {
			m0_free_aligned(data, 0, 0);
		} else {
			M0_ASSERT(p->p_magic == M0_MEMORY_AP_MAGIX);
			m0_alloc_callsite_mod(p->p_cs, AP_FREE, 0);
			p->p_magic = M0_MEMORY_AP_FREE_MAGIX;
			free0(p);
		}
	}
}

M0_INTERNAL void m0_free_aligned(void *data, size_t size, unsigned shift)
{
	if (data != NULL) {
		struct aligned_pad *apad = ((struct aligned_pad *)data) - 1;
		M0_ASSERT(apad->a_magic == M0_MEMORY_AA_MAGIX);
		/*
		 * @todo: IO and SNS code move buffers between network buffer
		 * pools, so sometimes a buffer is freed with a different size.
		 */
		M0_ASSERT(true || ergo(size > 0, size == apad->a_size));
		M0_ASSERT(ergo(shift > 0, shift <= apad->a_shift));
		m0_alloc_callsite_mod(apad->a_cs, AP_FREE, apad->a_size);
		data -= 1 << apad->a_shift;
		apad->a_magic = M0_MEMORY_AA_FREE_MAGIX;
		free0_aligned(data, apad->a_size + (1 << apad->a_shift),
			      apad->a_shift);
	}
}

#else /* USE_ALLOC_PROF */

M0_INTERNAL void *m0_alloc_nz(size_t size)
{
	void *area;

	M0_ENTRY("size=%zi", size);
	area = m0_arch_alloc_nz(size);
	alloc_tail(area, size);
	M0_LEAVE("ptr=%p size=%zi", area, size);
	return area;
}

void *m0_alloc(size_t size)
{
	return alloc0(size);
}

void *(m0_alloc_profiled)(size_t size)
{
	return alloc0(size);
}

void m0_free(void *data)
{
	return free0(data);
}

M0_INTERNAL void *m0_alloc_aligned(size_t size, unsigned shift)
{
	return alloc0_aligned(size, shift);
}

M0_INTERNAL void *(m0_alloc_aligned_profiled)(size_t size, unsigned shift)
{
	return alloc0_aligned(size, shift);
}

M0_INTERNAL void m0_free_aligned(void *data, size_t size, unsigned shift)
{
	return free0_aligned(data, size, shift);
}

#endif

M0_EXPORTED(m0_alloc_profiled);
M0_EXPORTED(m0_alloc);
M0_EXPORTED(m0_free);
M0_EXPORTED(m0_alloc_aligned_profiled);
M0_EXPORTED(m0_alloc_aligned);
M0_EXPORTED(m0_free_aligned);

static void *alloc0(size_t size)
{
	void *area;

        /* 9% of logs in m0trace log file is due to M0_ENTRY  and M0_LEAVE */
	//M0_ENTRY("size=%zi", size);
	if (M0_FI_ENABLED_IN("m0_alloc", "fail_allocation"))
		return NULL;
	area = m0_arch_alloc(size);
	alloc_tail(area, size);
	if (area != NULL) {
		m0_arch_allocated_zero(area, size);
	} else if (!M0_FI_ENABLED_IN("m0_alloc", "keep_quiet")) {
		M0_LOG(M0_ERROR, "Failed to allocate %zi bytes.", size);
		m0_backtrace();
	}
	//M0_LEAVE("ptr=%p size=%zi", area, size);
	return area;
}

static void free0(void *data)
{
	if (data != NULL) {
		size_t size = m0_arch_alloc_size(data);

                /* 5% of logs in m0trace log file is m0_free */
		//M0_LOG(M0_DEBUG, "%p", data);

		if (DEV_MODE) {
			m0_atomic64_sub(&allocated, size);
			m0_atomic64_add(&cumulative_free, size);
		}
		poison_before_free(data, size);
		m0_arch_free(data);
	}
}

M0_INTERNAL void m0_memory_pagein(void *addr, size_t size)
{
	m0_arch_memory_pagein(addr, size);
}

static void *alloc0_aligned(size_t size, unsigned shift)
{
	void  *result;
	size_t alignment;

	if (M0_FI_ENABLED_IN("m0_alloc_aligned", "fail_allocation"))
		return NULL;

	/*
	 * posix_memalign(3):
	 *
	 *         The requested alignment must be a power of 2 at least as
	 *         large as sizeof(void *).
	 */

	alignment = max_type(size_t, 1 << shift, sizeof result);
	M0_ASSERT(m0_is_po2(alignment));
	result = m0_arch_alloc_aligned(alignment, size);
	if (result != NULL)
		m0_arch_allocated_zero(result, size);
	return result;
}

static void free0_aligned(void *data, size_t size, unsigned shift)
{
	if (data != NULL) {
		M0_PRE(m0_addr_is_aligned(data, shift));
		poison_before_free(data, size);
		m0_arch_free_aligned(data, size, shift);
	}
}

M0_INTERNAL void *m0_alloc_wired(size_t size, unsigned shift)
{
	return m0_arch_alloc_wired(size, shift);
}

M0_INTERNAL void m0_free_wired(void *data, size_t size, unsigned shift)
{
	if (data != NULL) {
		poison_before_free(data, size);
		m0_arch_free_wired(data, size, shift);
	}
}

M0_INTERNAL size_t m0_allocated(void)
{
	return m0_atomic64_get(&allocated);
}
M0_EXPORTED(m0_allocated);

M0_INTERNAL size_t m0_allocated_total(void)
{
	return m0_atomic64_get(&cumulative_alloc);
}
M0_EXPORTED(m0_allocated_total);

M0_INTERNAL size_t m0_freed_total(void)
{
	return m0_atomic64_get(&cumulative_free);
}
M0_EXPORTED(m0_freed_total);

M0_INTERNAL int m0_pagesize_get(void)
{
	return m0_arch_pagesize_get();
}

M0_INTERNAL int m0_pageshift_get(void)
{
	return m0_arch_pageshift_get();
}

M0_INTERNAL int m0_dont_dump(void *p, size_t size)
{
	int pagesize = m0_pagesize_get();
	M0_PRE(((unsigned long)p / pagesize * pagesize) == (unsigned long)p);

	return m0_arch_dont_dump(p, size);
}

M0_INTERNAL int m0_memory_init(void)
{
	m0_atomic64_set(&allocated, 0);
	m0_atomic64_set(&cumulative_alloc, 0);
	m0_atomic64_set(&cumulative_free, 0);
	return m0_arch_memory_init();
}

M0_INTERNAL void m0_memory_fini(void)
{
	M0_LOG(M0_DEBUG, "allocated=%" PRIu64 " cumulative_alloc=%" PRIu64 " "
	       "cumulative_free=%"PRIu64, m0_atomic64_get(&allocated),
	       m0_atomic64_get(&cumulative_alloc),
	       m0_atomic64_get(&cumulative_free));
	m0_arch_memory_fini();
}

#undef DEV_MODE
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
