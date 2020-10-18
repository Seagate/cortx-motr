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


/**
 * @addtogroup cookie
 *
 * The key data-structure in Lib-Cookie is m0_cookie. It holds the address of
 * an object along with a generation-count which is used to check validity of a
 * cookie.
 *
 * The constructor of an object calls m0_cookie_new, which increments a
 * global counter cookie_generation, and embeds it in the object.
 * On arrival of a query for the object, m0_cookie_init creates
 * a cookie, and embeds the address for the object in m0_cookie along with a
 * copy of cookie_generation embedded in the object.
 *
 * For subsequent requests for the same object, client communicates a cookie
 * to a server. On server, function m0_cookie_dereference validates a cookie,
 * and retrieves an address of the object for a valid cookie.
 *
 * m0_cookie_dereference checks the validity of a cookie in two steps.
 * The first step validates an address embedded inside the cookie.
 * The second step ensures that the cookie is not stale. To identify a stale
 * cookie, it compares its generation count with the generation count in the
 * object. In order to reduce the probability of false validation,
 * the function m0_cookie_global_init initializes the cookie_generation with
 * the system-time during initialisation of Motr.
 *
 * @{
 */

#ifdef __KERNEL__
#include <linux/string.h>
#else
#include <string.h>
#endif

#include "lib/types.h"
#include "lib/errno.h" /* -EPROTO */
#include "lib/cookie.h"
#include "lib/arith.h" /* M0_IS_8ALIGNED */
#include "lib/time.h"  /* m0_time_now() */

#define M0_TRACE_SUBSYSTEM M0_TRACE_SUBSYS_LIB
#include "lib/trace.h"

static uint64_t cookie_generation;

M0_INTERNAL const struct m0_cookie M0_COOKIE_NULL = {
	.co_generation = 0xffff,
	.co_addr       = 7,
};

M0_INTERNAL bool m0_arch_addr_is_sane(const void *addr);
M0_INTERNAL int m0_arch_cookie_global_init(void);
M0_INTERNAL void m0_arch_cookie_global_fini(void);

M0_INTERNAL int m0_cookie_global_init(void)
{
	cookie_generation = m0_time_now();
	return m0_arch_cookie_global_init();
}

M0_INTERNAL void m0_cookie_new(uint64_t * gen)
{
	M0_PRE(gen != NULL);

	*gen = ++cookie_generation;
}

M0_INTERNAL void m0_cookie_init(struct m0_cookie *cookie, const uint64_t *obj)
{
	M0_PRE(cookie != NULL);
	M0_PRE(obj != NULL);

	cookie->co_addr = (uint64_t)obj;
	cookie->co_generation = *obj;
}

M0_INTERNAL bool m0_addr_is_sane(const uint64_t *addr)
{
	return addr > (uint64_t *)4096 && m0_arch_addr_is_sane(addr);
}

M0_INTERNAL bool m0_addr_is_sane_and_aligned(const uint64_t *addr)
{
	return M0_IS_8ALIGNED(addr) && m0_addr_is_sane(addr);
}

M0_INTERNAL ATTRIBUTE_NO_SANITIZE_ADDRESS int
m0_cookie_dereference(const struct m0_cookie *cookie, uint64_t ** addr)
{
	uint64_t *obj;

	M0_PRE(cookie != NULL);
	M0_PRE(addr != NULL);

	obj = (uint64_t *)cookie->co_addr;
	if (m0_addr_is_sane_and_aligned(obj) && cookie->co_generation == *obj) {
		*addr = obj;
		return 0;
	} else
		return -EPROTO;
}

M0_INTERNAL bool m0_cookie_is_null(const struct m0_cookie *cookie)
{
	return cookie->co_generation == M0_COOKIE_NULL.co_generation &&
		cookie->co_addr == M0_COOKIE_NULL.co_addr;
}

M0_INTERNAL bool m0_cookie_is_eq(const struct m0_cookie *cookie1,
				 const struct m0_cookie *cookie2)
{
	return memcmp(cookie1, cookie2, sizeof *cookie1) == 0;
}

M0_INTERNAL void m0_cookie_global_fini(void)
{
	m0_arch_cookie_global_fini();
}

#undef M0_TRACE_SUBSYSTEM

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
