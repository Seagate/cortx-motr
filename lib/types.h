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


#pragma once

#ifndef __MOTR_LIB_TYPES_H__
#define __MOTR_LIB_TYPES_H__

#ifdef __KERNEL__
#  include "lib/linux_kernel/types.h"
#else
#  include "lib/user_space/types.h"
#endif
#include "xcode/xcode_attr.h"

struct m0_uint128 {
	uint64_t u_hi;
	uint64_t u_lo;
} M0_XCA_RECORD M0_XCA_DOMAIN(rpc);

#define M0_UINT128(hi, lo) (struct m0_uint128) { .u_hi = (hi), .u_lo = (lo) }

#define U128X_F "%"PRIx64":%"PRIx64
#define U128D_F "%"PRId64":%"PRId64
#define U128_P(x) (x)->u_hi, (x)->u_lo
#define U128_S(u) &(u)->u_hi, &(u)->u_lo

#define U128X_F_SAFE "%s%"PRIx64":%"PRIx64
#define U128_P_SAFE(x) \
	((x) != NULL ? "" : "(null) "), \
	((x) != NULL ? (x)->u_hi : 0), ((x) != NULL ? (x)->u_lo : 0)

#define U128_P_SAFE_EX(y, x) \
	((y) != NULL ? "" : "(null) "), \
	((y) != NULL ? (x)->u_hi : 0), ((y) != NULL ? (x)->u_lo : 0)

M0_INTERNAL bool m0_uint128_eq(const struct m0_uint128 *u0,
			       const struct m0_uint128 *u1);
M0_INTERNAL int m0_uint128_cmp(const struct m0_uint128 *u0,
			       const struct m0_uint128 *u1);
M0_INTERNAL void m0_uint128_init(struct m0_uint128 *u128, const char *magic);
/** res = a + b; */
M0_INTERNAL void m0_uint128_add(struct m0_uint128 *res,
				const struct m0_uint128 *a,
				const struct m0_uint128 *b);
/** res = a * b; */
M0_INTERNAL void m0_uint128_mul64(struct m0_uint128 *res, uint64_t a,
				  uint64_t b);
M0_INTERNAL int m0_uint128_sscanf(const char *s, struct m0_uint128 *u128);

/** count of bytes (in extent, IO operation, etc.) */
typedef uint64_t m0_bcount_t;
/** an index (offset) in a linear name-space (e.g., in a file, storage object,
    storage device, memory buffer) measured in bytes */
typedef uint64_t m0_bindex_t;

enum {
	M0_BCOUNT_MAX = 0xffffffffffffffff,
	M0_BINDEX_MAX = M0_BCOUNT_MAX - 1,
	M0_BSIGNED_MAX = 0x7fffffffffffffff
};

#endif /* __MOTR_LIB_TYPES_H__ */

/*
 *  Local variables:
 *  c-indentation-style: "K&R"
 *  c-basic-offset: 8
 *  tab-width: 8
 *  fill-column: 80
 *  scroll-step: 1
 *  End:
 */
