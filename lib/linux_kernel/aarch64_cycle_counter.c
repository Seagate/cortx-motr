/* -*- C -*- */
/*
 * Copyright (c) 2021 Seagate Technology LLC and/or its Affiliates
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


#define M0_TRACE_SUBSYSTEM M0_TRACE_SUBSYS_LIB

#include "lib/trace.h"
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/version.h>

/** Module for ARM64 linux platform to provide
 * user space access of perf monitor registers
 */
#ifdef CONFIG_AARCH64
static void enable_user_space_access_pccnt(void *dt)
{
	uint64_t pmcr_val;

	/* Enables the cycle counter register */
	asm volatile("msr pmcntenset_el0, %0" :: "r" ((u64)(1 << 31)));

	/* Performance Monitors User Enable Register. */
	asm volatile("msr pmuserenr_el0, %0" : : "r"(1 |((u64)(1 << 2))));

	/* Reset cycle counter(0 bit) and start all event(2nd bit) counters */
	asm volatile("mrs %0, pmcr_el0" : "=r" (pmcr_val));
	asm volatile("msr pmcr_el0, %0" : : "r" (pmcr_val | 1 |((u64)(1 << 2))));
}

static void disable_user_space_pccnt_access(void *dt)
{

	/* Enables the cycle counter register */
	asm volatile("msr pmcntenset_el0, %0" :: "r" (0 << 31));

	/* Disable user-mode access to counters. */
	asm volatile("msr pmuserenr_el0, %0" : : "r"((u64)0));
}

int  start_cycle_counter(void)
{

	/* Call the function on each cpu with NULL param and wait for completion*/
	on_each_cpu(enable_user_space_access_pccnt, NULL, 1);
	return 0;
}

void  finish_cycle_counter(void)
{
	
	/* Call the function on each cpu with NULL param and wait for completion*/
	on_each_cpu(disable_user_space_pccnt_access, NULL, 1);
}
#endif

#undef M0_TRACE_SUBSYSTEM

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
