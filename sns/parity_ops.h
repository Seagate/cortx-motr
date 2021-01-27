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

#ifndef __MOTR_SNS_PARITY_OPS_H__
#define __MOTR_SNS_PARITY_OPS_H__

#ifndef __KERNEL__
#include <isa-l.h>
#else
#include "galois/galois.h"
#endif /* __KERNEL__ */
#include "lib/assert.h"

#define M0_PARITY_ZERO (0)

#ifdef __KERNEL__
#define M0_PARITY_GALOIS_W (8)
#endif

#ifndef __KERNEL__
#define M0_PARITY_W (8)
#else
#define M0_PARITY_W M0_PARITY_GALOIS_W
#endif /* __KERNEL__ */

typedef int m0_parity_elem_t;

M0_INTERNAL int m0_parity_init(void);
M0_INTERNAL void m0_parity_fini(void);

M0_INTERNAL m0_parity_elem_t m0_parity_pow(m0_parity_elem_t x,
					   m0_parity_elem_t p);

static inline m0_parity_elem_t m0_parity_add(m0_parity_elem_t x, m0_parity_elem_t y)
{
	return x ^ y;
}

static inline m0_parity_elem_t m0_parity_sub(m0_parity_elem_t x, m0_parity_elem_t y)
{
	return x ^ y;
}

static inline m0_parity_elem_t m0_parity_mul(m0_parity_elem_t x, m0_parity_elem_t y)
{
#ifndef __KERNEL__
	return gf_mul(x, y);
#else
	/* return galois_single_multiply(x, y, M0_PARITY_GALOIS_W); */
	return galois_multtable_multiply(x, y, M0_PARITY_GALOIS_W);
#endif /* __KERNEL__ */
}

static inline m0_parity_elem_t m0_parity_div(m0_parity_elem_t x, m0_parity_elem_t y)
{
#ifndef __KERNEL__
	return gf_mul(x, gf_inv(y));
#else
	/* return galois_single_divide(x, y, M0_PARITY_GALOIS_W); */
	return galois_multtable_divide(x, y, M0_PARITY_GALOIS_W);
#endif /* __KERNEL__ */
}

static inline m0_parity_elem_t m0_parity_lt(m0_parity_elem_t x, m0_parity_elem_t y)
{
	return x < y;
}

static inline m0_parity_elem_t m0_parity_gt(m0_parity_elem_t x, m0_parity_elem_t y)
{
	return x > y;
}

/* __MOTR_SNS_PARITY_OPS_H__ */
#endif

/*
 *  Local variables:
 *  c-indentation-style: "K&R"
 *  c-basic-offset: 8
 *  tab-width: 8
 *  fill-column: 80
 *  scroll-step: 1
 *  End:
 */
