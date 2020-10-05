/* -*- C -*- */
/*
 * Copyright (c) 2020 Seagate Technology LLC and/or its Affiliates
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

#ifndef __MOTR_LIB_C11_ATOMIC_H__
#define __MOTR_LIB_C11_ATOMIC_H__

#include <stdatomic.h>

#include "lib/types.h"
#include "lib/assert.h"

/**
 * @addtogroup atomic
 *
 * Implementation of atomic operations in user space using c11 <stdatomic.h>.
 *
 */

struct m0_atomic64 {
	atomic_int_fast64_t a_value;
};

static inline void m0_atomic64_set(struct m0_atomic64 *a, int64_t num)
{
	M0_CASSERT(sizeof a->a_value == sizeof num);

	atomic_store(&a->a_value, num);
}

static inline int64_t m0_atomic64_get(const struct m0_atomic64 *a)
{
	/* Drop const. */
	return atomic_load((atomic_int_fast64_t *)&a->a_value);
}

static inline int64_t m0_atomic64_add_return(struct m0_atomic64 *a,
					     int64_t delta)
{
	return atomic_fetch_add(&a->a_value, delta) + delta;
}

static inline int64_t m0_atomic64_sub_return(struct m0_atomic64 *a,
					     int64_t delta)
{
	return atomic_fetch_sub(&a->a_value, delta) - delta;
}

static inline void m0_atomic64_add(struct m0_atomic64 *a, int64_t num)
{
	m0_atomic64_add_return(a, num);
}

static inline void m0_atomic64_sub(struct m0_atomic64 *a, int64_t num)
{
	m0_atomic64_sub_return(a, num);
}

static inline void m0_atomic64_inc(struct m0_atomic64 *a)
{
	m0_atomic64_add(a, 1);
}

static inline void m0_atomic64_dec(struct m0_atomic64 *a)
{
	m0_atomic64_sub(a, 1);
}

static inline bool m0_atomic64_inc_and_test(struct m0_atomic64 *a)
{
	return m0_atomic64_add_return(a, 1) == 0;
}

static inline bool m0_atomic64_dec_and_test(struct m0_atomic64 *a)
{
	return m0_atomic64_sub_return(a, 1) == 0;
}

static inline bool m0_atomic64_cas(int64_t *loc, int64_t oldval, int64_t newval)
{
	atomic_int_fast64_t *aloc = (void *)loc;
	return atomic_compare_exchange_weak(aloc, &oldval, newval);
}

static inline void m0_mb(void)
{
	atomic_thread_fence(memory_order_seq_cst);
}

M0_BASSERT(ATOMIC_POINTER_LOCK_FREE == 2);
M0_BASSERT(ATOMIC_INT_LOCK_FREE     == 2);
M0_BASSERT(ATOMIC_LONG_LOCK_FREE    == 2);
M0_BASSERT(ATOMIC_LLONG_LOCK_FREE   == 2);

/** @} end of atomic group */
#endif /* __MOTR_LIB_C11_ATOMIC_H__ */

/*
 *  Local variables:
 *  c-indentation-style: "K&R"
 *  c-basic-offset: 8
 *  tab-width: 8
 *  fill-column: 80
 *  scroll-step: 1
 *  End:
 */
