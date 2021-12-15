/* -*- C -*- */
/*
 * Copyright (c) 2012-2021 Seagate Technology LLC and/or its Affiliates
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

#if !defined(__KERNEL__) && defined(HAVE_ISA_L_H)
#include <isa-l.h>
#else
#define gf_mul(x, y) (0)
#endif

#include "lib/assert.h"

#define M0_PARITY_ZERO		(0)
#define M0_PARITY_GALOIS_W	(8)

typedef int m0_parity_elem_t;

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
	return gf_mul(x, y);
}

static inline m0_parity_elem_t m0_parity_div(m0_parity_elem_t x, m0_parity_elem_t y)
{
	return gf_mul(x, gf_inv(y));
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
