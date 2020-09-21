/* -*- C -*- */
/*
 * Copyright (c) 2013-2020 Seagate Technology LLC and/or its Affiliates
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
 * @addtogroup assert
 *
 * @{
 */

#include <linux/kernel.h>         /* pr_emerg */
#include <linux/bug.h>            /* BUG */
#include <linux/string.h>         /* strcmp */
#include <linux/delay.h>          /* mdelay */
/*#include <linux/kgdb.h>*/           /* kgdb_breakpoint() */

#include "lib/assert.h"           /* m0_failed_condition */
#include "motr/version.h"         /* m0_build_info */

static int m0_panic_delay_msec = 110;

void m0_arch_backtrace()
{
	dump_stack();
}

M0_INTERNAL void m0_arch_panic(const struct m0_panic_ctx *c, va_list ap)
{
	const struct m0_build_info *bi = m0_build_info_get();

	pr_emerg("Motr panic: %s at %s() %s:%i (last failed: %s) [git: %s]\n",
		 c->pc_expr, c->pc_func, c->pc_file, c->pc_lineno,
		 m0_failed_condition ?: "none", bi->bi_git_describe);
	if (c->pc_fmt != NULL) {
		pr_emerg("Motr panic reason: ");
		vprintk(c->pc_fmt, ap);
		pr_emerg("\n");
	}
	dump_stack();
	/*
	 * Delay BUG() call in in order to allow m0traced process to fetch all
	 * trace records from kernel buffer, before system-wide Kernel Panic is
	 * triggered.
	 */
	mdelay(m0_panic_delay_msec);
	BUG();
}

M0_INTERNAL void m0_debugger_invoke(void)
{
/*
#ifdef CONFIG_KGDB
	kgdb_breakpoint();
#endif
*/
}

/** @} end of assert group */

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
