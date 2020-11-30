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


#include <linux/cpu.h>
#include <linux/cpumask.h>
#include <asm/processor.h>
#include <linux/topology.h>
#include <linux/slab.h>

#include "lib/assert.h"
#include "lib/errno.h"
#include "lib/list.h"
#include "lib/memory.h"
#include "lib/processor.h"

#define M0_TRACE_SUBSYSTEM M0_TRACE_SUBSYS_LIB
#include "lib/trace.h"

/**
   Convert bitmap from one format to another. Copy cpumask bitmap to m0_bitmap.

   @param dest -> Processors bitmap for Motr programs.
   @param src -> Processors bitmap used by Linux kernel.
   @param bmpsz -> Size of cpumask bitmap (src)

   @pre Assumes memory is alloacted for outbmp and it's initialized.

   @see lib/processor.h
   @see lib/bitmap.h
 */
static void processors_bitmap_copy(struct m0_bitmap *dest,
				   const cpumask_t *src,
				   uint32_t bmpsz)
{
	uint32_t bit;
	bool     val;

	M0_PRE(dest->b_nr >= bmpsz);

	for (bit = 0; bit < bmpsz; ++bit) {
		val = cpumask_test_cpu(bit, src);
		m0_bitmap_set(dest, bit, val);
	}
}

/**
   Fetch NUMA node id for a given processor.

   @param id -> id of the processor for which information is requested.

   @return id of the NUMA node to which the processor belongs.
 */
static inline uint32_t processor_numanodeid_get(m0_processor_nr_t id)
{
	return cpu_to_node(id);
}

/**
   Fetch pipeline id for a given processor.
   Curently pipeline id is same as processor id.

   @param id -> id of the processor for which information is requested.

   @return id of pipeline for the given processor.
 */
static inline uint32_t processor_pipelineid_get(m0_processor_nr_t id)
{
	return id;
}

/* ---- Processor Interfaces ---- */

M0_INTERNAL int m0_processors_init()
{
	return 0;
}

M0_INTERNAL void m0_processors_fini()
{
}

M0_INTERNAL m0_processor_nr_t m0_processor_nr_max(void)
{
	return NR_CPUS;
}

M0_INTERNAL void m0_processors_possible(struct m0_bitmap *map)
{
	processors_bitmap_copy(map, cpu_possible_mask, nr_cpu_ids);
}

M0_INTERNAL void m0_processors_available(struct m0_bitmap *map)
{
	processors_bitmap_copy(map, cpu_present_mask, nr_cpu_ids);
}

M0_INTERNAL void m0_processors_online(struct m0_bitmap *map)
{
	processors_bitmap_copy(map, cpu_online_mask, nr_cpu_ids);
}


M0_INTERNAL m0_processor_nr_t m0_processor_id_get(void)
{
	return smp_processor_id();
}

#undef M0_TRACE_SUBSYSTEM

/** @} end of processor group */

/*
 *  Local variables:
 *  c-indentation-style: "K&R"
 *  c-basic-offset: 8
 *  tab-width: 8
 *  fill-column: 80
 *  scroll-step: 1
 *  End:
 */
