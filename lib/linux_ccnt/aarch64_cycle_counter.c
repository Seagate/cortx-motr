#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/version.h>

/* Module for ARM64 linux platform to provide 
** user space access of perf monitor registers*/
#ifndef CONFIG_X86_64
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

//EXPORT_SYMBOL(start_cycle_counter);
//EXPORT_SYMBOL(finish_cycle_counter);

//module_init(start);
//module_exit(finish);
