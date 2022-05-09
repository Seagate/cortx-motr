/* -*- C -*- */
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


#define M0_TRACE_SUBSYSTEM M0_TRACE_SUBSYS_LIB
#include "lib/trace.h"

#include "lib/assert.h"
#include "lib/thread.h"
#include "lib/misc.h"            /* M0_CAT */
#include "lib/trace_internal.h"  /* m0_trace_file_path_get */
#include "motr/version.h"        /* m0_build_info */

#include <stdlib.h>

/**
   @addtogroup assert

   @{
*/

static unsigned maxstack = 0;

void m0_stack_check(void)
{
	struct m0_thread_tls *tls = m0_thread_tls();

	if (tls != NULL && tls->tls_stack != NULL) {
		void    *stack = tls->tls_stack;
		unsigned depth = abs(stack - (void *)&tls);
		if (depth > maxstack) {
			maxstack = depth;
			tls->tls_stack = NULL; /* Avoid recursion. */
			printf("New deepest stack: %u.\n", depth);
			m0_backtrace();
			tls->tls_stack = stack;
		}
	}
}

/**
 * Panic function.
 */
void m0_panic(const struct m0_panic_ctx *ctx, ...)
{
	static int repanic = 0;
	va_list    ap;
	const struct m0_build_info *bi = m0_build_info_get();

	if (repanic++ == 0) {
		M0_LOG(M0_FATAL, "panic: %s at %s() (%s:%i) %s [git: %s] %s",
		       ctx->pc_expr, ctx->pc_func, ctx->pc_file, ctx->pc_lineno,
		       m0_failed_condition ?: "", bi->bi_git_describe,
		       m0_trace_file_path_get());
		va_start(ap, ctx);
		m0_arch_panic(ctx, ap);
		va_end(ap);
	} else {
		/* The death of God left the angels in a strange position. */
		while (true) {
			;
		}
	}
}
M0_EXPORTED(m0_panic);

M0_INTERNAL void m0_panic_only(const struct m0_panic_ctx *ctx, ...)
{
	va_list ap;

	va_start(ap, ctx);
	m0_arch_panic(ctx, ap);
	va_end(ap);
}

void m0_backtrace(void)
{
	m0_arch_backtrace();
}
M0_EXPORTED(m0_backtrace);

M0_INTERNAL void m0__assertion_hook(void)
{
}
M0_EXPORTED(m0__assertion_hook);

/** @} end of assert group */
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
