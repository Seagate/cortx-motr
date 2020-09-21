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


#pragma once

#ifndef __MOTR_LIB_BITSTRING_H__
#define __MOTR_LIB_BITSTRING_H__

#include "lib/types.h"

/**
   @defgroup adt Basic abstract data types
   @{
*/

struct m0_bitstring {
	uint32_t b_len;
	char     b_data[0];
} M0_XCA_RECORD M0_XCA_DOMAIN(be);

/**
  Get a pointer to the data in the bitstring.
  Data may be read or written here.

  User is responsible for allocating large enough contiguous memory.
 */
M0_INTERNAL void *m0_bitstring_buf_get(struct m0_bitstring *c);
/**
 Report the bitstring length
 */
M0_INTERNAL uint32_t m0_bitstring_len_get(const struct m0_bitstring *c);
/**
 Set the bitstring valid length
 */
M0_INTERNAL void m0_bitstring_len_set(struct m0_bitstring *c, uint32_t len);
/**
 String-like compare: alphanumeric for the length of the shortest string.
 Shorter strings are "less" than matching longer strings.
 Bitstrings may contain embedded NULLs.
 */
M0_INTERNAL int m0_bitstring_cmp(const struct m0_bitstring *c1,
				 const struct m0_bitstring *m0);

/**
 Copy @src to @dst.
*/
M0_INTERNAL void m0_bitstring_copy(struct m0_bitstring *dst,
				   const char *src, size_t count);

/**
 Alloc memory for a string of passed len and copy name to it.
*/
M0_INTERNAL struct m0_bitstring *m0_bitstring_alloc(const char *name,
						    size_t len);

/**
 Free memory of passed @c.
*/
M0_INTERNAL void m0_bitstring_free(struct m0_bitstring *c);

/** @} end of adt group */
#endif /* __MOTR_LIB_BITSTRING_H__ */

/*
 *  Local variables:
 *  c-indentation-style: "K&R"
 *  c-basic-offset: 8
 *  tab-width: 8
 *  fill-column: 80
 *  scroll-step: 1
 *  End:
 */
