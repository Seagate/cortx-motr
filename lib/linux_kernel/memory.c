/*
 * Copyright (c) 2011-2020 Seagate Technology LLC and/or its Affiliates
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


#include <linux/slab.h>
#include <linux/module.h>

#define M0_TRACE_SUBSYSTEM M0_TRACE_SUBSYS_LIB
#include "lib/assert.h"               /* M0_PRE */
#include "lib/memory.h"

/**
   @addtogroup memory

   <b>Linux kernel kmalloc based allocator.</b>

   @{
*/

M0_INTERNAL void *m0_arch_alloc(size_t size)
{
	/**
	 * GFP_NOFS is used here to avoid deadlocks.
	 *
	 * Normal kernel allocation mode (GFP_KERNEL) allows kernel memory
	 * allocator to call file-system to free some cached objects (pages,
	 * inodes, etc.). This introduces a danger of deadlock when a memory
	 * allocation is done under a lock and to free cached object the same
	 * lock has to be taken.
	 *
	 * m0t1fs liberally allocated memory under critical locks (e.g., rpc
	 * machine lock), which is inherently dead-lock prone.
	 *
	 * Using GFP_NOFS disabled re-entering file systems from the allocator,
	 * eliminating dead-locks at the risk of getting -ENOMEM earlier than
	 * necessary.
	 *
	 * @todo The proper solution is to introduce an additional interface
	 * m0_alloc_safe(), to be called outside of critical locks and using
	 * GFP_KERNEL.
	 */
	return kzalloc(size, GFP_NOFS);
}

M0_INTERNAL void m0_arch_free(void *data)
{
	kfree(data);
}

M0_INTERNAL void m0_memmove(void *tgt, void *src, size_t size)
{
	memmove(tgt,src,size);
}

M0_INTERNAL void m0_arch_allocated_zero(void *data, size_t size)
{
	/* do nothing already zeroed. */
}

M0_INTERNAL void *m0_arch_alloc_nz(size_t size)
{
	/** @see m0_arch_alloc() */
	return kmalloc(size, GFP_NOFS);
}

M0_INTERNAL void m0_arch_memory_pagein(void *addr, size_t size)
{
	/* kernel memory is not swappable */
}

M0_INTERNAL size_t m0_arch_alloc_size(void *data)
{
	return ksize(data);
}

M0_INTERNAL void *m0_arch_alloc_aligned(size_t alignment, size_t size)
{
	/*
	 * Currently it supports alignment of PAGE_SHIFT only.
	 */
	M0_PRE(alignment == PAGE_SIZE);
	return size == 0 ? NULL : alloc_pages_exact(size,
						    GFP_NOFS | __GFP_ZERO);
}

M0_INTERNAL void m0_arch_free_aligned(void *addr, size_t size, unsigned shift)
{
	M0_PRE(shift == PAGE_SHIFT);
	free_pages_exact(addr, size);
}

M0_INTERNAL void *m0_arch_alloc_wired(size_t size, unsigned shift)
{
	return m0_alloc_aligned(size, shift);
}

M0_INTERNAL void m0_arch_free_wired(void *data, size_t size, unsigned shift)
{
	m0_free_aligned(data, size, shift);
}

M0_INTERNAL size_t m0_arch_allocated(void)
{
	return 0;
}

M0_INTERNAL int m0_arch_dont_dump(void *p, size_t size)
{
	return 0;
}

M0_INTERNAL int m0_arch_memory_init(void)
{
	return 0;
}

M0_INTERNAL void m0_arch_memory_fini(void)
{
}

M0_INTERNAL int m0_arch_pagesize_get(void)
{
	return PAGE_SIZE;
}

M0_INTERNAL int m0_arch_pageshift_get(void)
{
	return PAGE_SHIFT;
}

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
