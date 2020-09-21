/* -*- C -*- */
/*
 * Copyright (c) 2017-2020 Seagate Technology LLC and/or its Affiliates
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
 * @addtogroup Coroutine
 *
 * @{
 */

#define M0_TRACE_SUBSYSTEM M0_TRACE_SUBSYS_LIB
#include "lib/trace.h"
#include "lib/coroutine.h"
#include "lib/memory.h"

static int locals_alloc_init(struct m0_co_locals_allocator *alloc)
{
	alloc->la_pool = m0_alloc_aligned(M0_MCC_LOCALS_ALLOC_SZ,
					  M0_MCC_LOCALS_ALLOC_SHIFT);
	alloc->la_frame = 0;
	return alloc->la_pool == NULL ? -ENOMEM : 0;
}

static void locals_alloc_fini(struct m0_co_locals_allocator *alloc)
{
	M0_PRE(alloc->la_frame == 0);
	m0_free_aligned(alloc->la_pool, M0_MCC_LOCALS_ALLOC_SZ,
			M0_MCC_LOCALS_ALLOC_SHIFT);
}

static void *locals_alloc(struct m0_co_locals_allocator *alloc, uint64_t frame,
			  uint64_t size)
{
	struct m0_co_la_item *curr;
	struct m0_co_la_item *prev;
	uint64_t              i;
	uint64_t              aligned_sz = m0_align(size +
						    M0_MCC_LOCALS_ALLOC_PAD_SZ,
						    M0_MCC_LOCALS_ALLOC_ALIGN);
	M0_PRE(alloc->la_frame == frame);

	curr = &alloc->la_items[alloc->la_frame];
	if (alloc->la_frame == 0) {
		curr->lai_addr = alloc->la_pool;
		curr->lai_size = aligned_sz;
		alloc->la_total = curr->lai_size;
	} else {
		prev = &alloc->la_items[alloc->la_frame - 1];
		curr->lai_addr = prev->lai_addr + prev->lai_size;
		M0_ASSERT(m0_is_aligned((uint64_t) curr->lai_addr,
					M0_MCC_LOCALS_ALLOC_ALIGN));
		curr->lai_size = aligned_sz;
		alloc->la_total += curr->lai_size;
	}

	M0_ASSERT(alloc->la_total < M0_MCC_LOCALS_ALLOC_SZ);
	M0_ASSERT(alloc->la_frame < M0_MCC_STACK_NR);

	/* test memory's zeroed */
	for (i = 0; i < curr->lai_size; ++i)
		M0_ASSERT(((uint8_t*) curr->lai_addr)[i] == 0x00);

	memset(curr->lai_addr, 0xCC, aligned_sz);
	alloc->la_frame++;

	return curr->lai_addr;
}

static void locals_free(struct m0_co_locals_allocator *alloc, uint64_t frame)
{
	uint64_t              i;
	struct m0_co_la_item *curr;

	curr = &alloc->la_items[--alloc->la_frame];
	M0_PRE(alloc->la_frame >= 0);
	M0_PRE(alloc->la_frame == frame);

	/* test pad is CC-ed */
	for (i = curr->lai_size - M0_MCC_LOCALS_ALLOC_PAD_SZ;
	     i < curr->lai_size; ++i)
		M0_ASSERT(((uint8_t*) curr->lai_addr)[i] == 0xCC);

	memset(curr->lai_addr, 0x00, curr->lai_size);
	alloc->la_total -= curr->lai_size;
	curr->lai_addr = NULL;
	curr->lai_size = 0;

	M0_ASSERT(ergo(frame == 0, alloc->la_total == 0));
}

M0_INTERNAL void m0_co_context_locals_alloc(struct m0_co_context *context,
					    uint64_t size)
{
	context->mc_locals[context->mc_frame] =
		locals_alloc(&context->mc_alloc, context->mc_frame, size);

	M0_LOG(M0_CALL, "alloc=%p size=%"PRIu64,
	       context->mc_locals[context->mc_frame], size);
}

M0_INTERNAL void m0_co_context_locals_free(struct m0_co_context *context)
{
	M0_LOG(M0_CALL, "free=%p", context->mc_locals[context->mc_frame]);

	locals_free(&context->mc_alloc, context->mc_frame);
	context->mc_locals[context->mc_frame] = NULL;
}

M0_INTERNAL void *m0_co_context_locals(struct m0_co_context *context)
{
	return context->mc_locals[context->mc_yield ? context->mc_yield_frame :
				  context->mc_frame];
}

M0_INTERNAL int m0_co_context_init(struct m0_co_context *context)
{
	*context = (struct m0_co_context) { .mc_yield = false };
	return locals_alloc_init(&context->mc_alloc);
}

M0_INTERNAL void m0_co_context_fini(struct m0_co_context *context)
{
	locals_alloc_fini(&context->mc_alloc);
}

#undef M0_TRACE_SUBSYSTEM

/** @} end of Coroutine group */

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
