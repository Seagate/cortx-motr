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


#ifdef __KERNEL__
#include <linux/string.h>
#else
#include <string.h>
#endif

#include "lib/bitstring.h"
#include "lib/arith.h"      /* M0_3WAY */
#include "lib/memory.h"     /* m0_alloc() */

M0_INTERNAL void *m0_bitstring_buf_get(struct m0_bitstring *c)
{
        return c->b_data;
}

M0_INTERNAL uint32_t m0_bitstring_len_get(const struct m0_bitstring *c)
{
        return c->b_len;
}

M0_INTERNAL void m0_bitstring_len_set(struct m0_bitstring *c, uint32_t len)
{
        c->b_len = len;
}

M0_INTERNAL struct m0_bitstring *m0_bitstring_alloc(const char *name,
						    size_t len)
{
        struct m0_bitstring *c = m0_alloc(sizeof(*c) + len);
        if (c == NULL)
                return NULL;
        m0_bitstring_copy(c, name, len);
        return c;
}

M0_INTERNAL void m0_bitstring_free(struct m0_bitstring *c)
{
        m0_free(c);
}

M0_INTERNAL void m0_bitstring_copy(struct m0_bitstring *dst, const char *src,
				   size_t count)
{
        memcpy(m0_bitstring_buf_get(dst), src, count);
        m0_bitstring_len_set(dst, count);
}

/**
   String-like compare: alphanumeric for the length of the shortest string.
   Shorter strings precede longer strings.
   Strings may contain embedded NULLs.
 */
M0_INTERNAL int m0_bitstring_cmp(const struct m0_bitstring *s1,
				 const struct m0_bitstring *s2)
{
	/* Compare leading parts up to min_len. If they differ, return the
	 * result, otherwize compare lengths. */
	return memcmp(s1->b_data, s2->b_data, min_check(s1->b_len, s2->b_len))
		?: M0_3WAY(s1->b_len, s2->b_len);
}

/*
 *  Local variables:
 *  c-indentation-style: "K&R"
 *  c-basic-offset: 8
 *  tab-width: 8
 *  fill-column: 80
 *  scroll-step: 1
 *  End:
 */
