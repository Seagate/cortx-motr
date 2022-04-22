/*
 * Copyright (c) 2011-2021 Seagate Technology LLC and/or its Affiliates
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


#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <sys/mman.h>

#include "lib/arith.h"   /* min_type, m0_is_po2 */
#include "lib/assert.h"
#include "lib/memory.h"

#define M0_TRACE_SUBSYSTEM M0_TRACE_SUBSYS_MEMORY
#include "lib/trace.h"

/**
   @addtogroup memory

   <b>User level malloc(3) based implementation.</b>

   The only interesting detail is implementation of m0_allocated(). No standard
   function returns the amount of memory allocated in the arena.

   GNU Libc defines mallinfo() function, returning the amount of allocated
   memory among other things. In OS X (of all places) there is malloc_size()
   function that, given a pointer to an allocated block of memory, returns its
   size. On other platforms m0_allocates() is always 0.

   @{
*/

#ifdef HAVE_MALLINFO

#include <malloc.h>

M0_INTERNAL size_t m0_arch_alloc_size(void *data)
{
	return malloc_usable_size(data);
}

/* HAVE_MALLINFO */
#elif HAVE_MALLOC_SIZE

#include <malloc/malloc.h>

M0_INTERNAL size_t m0_arch_alloc_size(void *data)
{
	return malloc_size(data);
}

/* HAVE_MALLOC_SIZE */
#else

M0_INTERNAL size_t m0_arch_alloc_size(void *data)
{
	return 0;
}

#endif

void *m0_arch_alloc(size_t size)
{
	return malloc(size);
}

void m0_arch_free(void *data)
{
	free(data);
}

M0_INTERNAL void m0_memmove(void *tgt, void *src, size_t size)
{
	memmove(tgt,src,size);
}

M0_INTERNAL void m0_arch_allocated_zero(void *data, size_t size)
{
	memset(data, 0, size);
}

M0_INTERNAL void *m0_arch_alloc_nz(size_t size)
{
	return m0_arch_alloc(size);
}

M0_INTERNAL void m0_arch_free_aligned(void *data, size_t size, unsigned shift)
{
	free(data);
}

M0_INTERNAL void *m0_arch_alloc_aligned(size_t alignment, size_t size)
{
	int   rc;
	void *result;

	rc = posix_memalign(&result, alignment, size);
	if (rc != 0)
		result = NULL;
	return result;
}

M0_INTERNAL void *m0_arch_alloc_wired(size_t size, unsigned shift)
{
	void *res;
	int   rc;

	res = m0_alloc_aligned(size, shift);
	if (res == NULL)
		goto out;

	rc = mlock(res, 1);
	if (rc == -1) {
		M0_LOG(M0_ERROR, "mlock() failed: rc=%d", errno);
		m0_free_aligned(res, size, shift);
		res = NULL;
		goto out;
	}

	rc = madvise((void*)((unsigned long)res & ~(m0_pagesize_get() - 1)), 1,
		     MADV_DONTFORK);
	if (rc == -1) {
		M0_LOG(M0_ERROR, "madvise() failed: rc=%d", errno);
		m0_free_wired(res, size, shift);
		res = NULL;
	}
out:
	return res;
}

M0_INTERNAL void m0_arch_free_wired(void *data, size_t size, unsigned shift)
{
	int rc;

	rc = madvise((void*)((unsigned long)data & ~(m0_pagesize_get() - 1)), 1,
		     MADV_DOFORK);
	if (rc == -1)
		M0_LOG(M0_WARN, "madvise() failed: rc=%d", errno);
	munlock(data, 1);
	m0_free_aligned(data, size, shift);
}

M0_INTERNAL void m0_arch_memory_pagein(void *addr, size_t size)
{
	char *current_byte = addr;
	char *end_byte     = (char *)addr + size;
	int   page_size    = m0_pagesize_get();

	if (addr == NULL || size == 0)
		return;
	/*
	 * It reads and writes the first byte of the allocated block
	 * and then the first byte of each page in the allocated block.
	 */
	M0_CASSERT(sizeof(current_byte) == sizeof(uint64_t));
	*current_byte = 0xCC;
	for (current_byte = (char *)m0_round_up((uint64_t)current_byte + 1,
	                                        page_size);
	     current_byte < end_byte; current_byte += page_size)
		*current_byte = 0xCC;
}

/**
 * sysctl vm.max_map_count default value is 65530. Half of this value
 * can be marked as MADV_DONTDUMP.
 * We need to set it to a larger number in our production system
 * with large memory, e.g.:
 * sysctl -w vm.max_map_count=30000000
 */
M0_INTERNAL int m0_arch_dont_dump(void *p, size_t size)
{
	int rc;
	rc = madvise(p, size, MADV_DONTDUMP);
	if (rc != 0) {
		rc = -errno;
		M0_LOG(M0_ERROR, "madvised failed: %d. Please "
				 "sysctl -w vm.max_map_count=a_larger_number",
				 rc);
	}

	return rc;
}

M0_INTERNAL int m0_arch_memory_init(void)
{
	void *nothing;

	/*
	 * m0_bitmap_init() relies on non-NULL-ness of m0_alloc(0) result.
	 */
	nothing = m0_alloc(0);
	M0_ASSERT(nothing != NULL);
	m0_free(nothing);
	return 0;
}

M0_INTERNAL void m0_arch_memory_fini(void)
{
}

M0_INTERNAL int m0_arch_pagesize_get()
{
	return getpagesize();
}

M0_INTERNAL int m0_arch_pageshift_get()
{
	return ffs(getpagesize());
}

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
