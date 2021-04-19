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


#include <setjmp.h>        /* setjmp() and longjmp() */

#include "lib/thread.h"
#include "lib/misc.h"      /* M0_SET0 */
#include "lib/errno.h"     /* errno */
#include "lib/assert.h"    /* m0_panic */

/**
   @addtogroup cookie
   @{
 */

static const struct m0_panic_ctx signal_panic = {
	.pc_expr   = "fatal signal delivered",
	.pc_func   = "unknown",
	.pc_file   = "unknown",
	.pc_lineno = 0,
	.pc_fmt    = "signo: %i"
};

/**
 * Signal handler for SIGSEGV.
 */
static void sigsegv(int sig)
{
	struct m0_thread_tls *tls = m0_thread_tls();
	jmp_buf              *buf = tls == NULL ? NULL : tls->tls_arch.tat_jmp;

	if (buf == NULL)
		m0_panic(&signal_panic, sig);
	else
		longjmp(*buf, 1);
}

/**
 * Checks the validity of an address by dereferencing the same. Occurrence of
 * an error in case of an invalid address gets handled by the
 * function sigsegv().
 */
M0_INTERNAL bool m0_arch_addr_is_sane(const void *addr)
{
	jmp_buf           buf;
	jmp_buf         **tls = &m0_thread_tls()->tls_arch.tat_jmp;
	int               ret;
	volatile uint64_t dummy M0_UNUSED;
	volatile bool     result = false;

	*tls = &buf;
	ret = setjmp(buf);
	if (ret == 0) {
		dummy = *(uint64_t *)addr;
		result = true;
	}
	*tls = NULL;

	return result;
}

/** Sets the signal handler for SIGSEGV to sigsegv() function. */
M0_INTERNAL int m0_arch_cookie_global_init(void)
{
	int              ret;
	struct sigaction sa_sigsegv = {
		.sa_handler = sigsegv,
		.sa_flags   = SA_NODEFER
	};

	ret = sigemptyset(&sa_sigsegv.sa_mask) ?:
		sigaction(SIGSEGV, &sa_sigsegv, NULL);
	if (ret != 0) {
		M0_ASSERT(ret == -1);
		return -errno;
	}
	return 0;
}

/** Sets the signal handler for SIGSEGV to the default handler. */
M0_INTERNAL void m0_arch_cookie_global_fini(void)
{
	int              ret;
	struct sigaction sa_sigsegv = { .sa_handler = SIG_DFL };

	ret = sigemptyset(&sa_sigsegv.sa_mask) ?:
		sigaction(SIGSEGV, &sa_sigsegv, NULL);
	M0_ASSERT(ret == 0);
}

/** @} end of cookie group */

/*
 *  Local variables:
 *  c-indentation-style: "K&R"
 *  c-basic-offset: 8
 *  tab-width: 8
 *  fill-column: 80
 *  scroll-step: 1
 *  End:
 */
