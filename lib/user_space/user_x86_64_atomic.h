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

#ifndef __MOTR_LIB_USER_X86_64_ATOMIC_H__
#define __MOTR_LIB_USER_X86_64_ATOMIC_H__

#include "lib/types.h"
#include "lib/assert.h"

/**
   @addtogroup atomic

   Implementation of atomic operations for Linux user space uses x86_64
   assembly language instructions (with gcc syntax). "Lock" prefix is used
   everywhere to ensure atomicity
   ---no optimisation for non-SMP configurations in present.
 */

struct m0_atomic64 {
	volatile long a_value;
};

static inline void m0_atomic64_set(struct m0_atomic64 *a, int64_t num)
{
	M0_CASSERT(sizeof a->a_value == sizeof num);

	a->a_value = num;
}

/**
   Returns value of an atomic counter.
 */
static inline int64_t m0_atomic64_get(const struct m0_atomic64 *a)
{
	return a->a_value;
}

/**
 atomically increment counter

 @param a pointer to atomic counter

 @return none
 */
static inline void m0_atomic64_inc(struct m0_atomic64 *a)
{
	asm volatile("lock incq %0"
		     : "=m" (a->a_value)
		     : "m" (a->a_value));
}

/**
 atomically decrement counter

 @param a pointer to atomic counter

 @return none
 */
static inline void m0_atomic64_dec(struct m0_atomic64 *a)
{
	asm volatile("lock decq %0"
		     : "=m" (a->a_value)
		     : "m" (a->a_value));
}

/**
   Atomically adds given amount to a counter
 */
static inline void m0_atomic64_add(struct m0_atomic64 *a, int64_t num)
{
	asm volatile("lock addq %1,%0"
		     : "=m" (a->a_value)
		     : "er" (num), "m" (a->a_value));
}

/**
   Atomically subtracts given amount from a counter
 */
static inline void m0_atomic64_sub(struct m0_atomic64 *a, int64_t num)
{
	asm volatile("lock subq %1,%0"
		     : "=m" (a->a_value)
		     : "er" (num), "m" (a->a_value));
}


/**
 atomically increment counter and return result

 @param a pointer to atomic counter

 @return new value of atomic counter
 */
static inline int64_t m0_atomic64_add_return(struct m0_atomic64 *a,
						  int64_t delta)
{
	long result;

	result = delta;
	asm volatile("lock xaddq %0, %1;"
		     : "+r" (delta), "+m" (a->a_value)
		     : : "memory");
	return delta + result;
}

/**
 atomically decrement counter and return result

 @param a pointer to atomic counter

 @return new value of atomic counter
 */
static inline int64_t m0_atomic64_sub_return(struct m0_atomic64 *a,
						  int64_t delta)
{
	return m0_atomic64_add_return(a, -delta);
}

static inline bool m0_atomic64_inc_and_test(struct m0_atomic64 *a)
{
	unsigned char result;

	asm volatile("lock incq %0; sete %1"
		     : "=m" (a->a_value), "=qm" (result)
		     : "m" (a->a_value) : "memory");
	return result != 0;
}

static inline bool m0_atomic64_dec_and_test(struct m0_atomic64 *a)
{
	unsigned char result;

	asm volatile("lock decq %0; sete %1"
		     : "=m" (a->a_value), "=qm" (result)
		     : "m" (a->a_value) : "memory");
	return result != 0;
}

static inline bool m0_atomic64_cas(int64_t * loc, int64_t oldval, int64_t newval)
{
	int64_t val;

	M0_CASSERT(8 == sizeof oldval);

	asm volatile("lock cmpxchgq %2,%1"
		     : "=a" (val), "+m" (*(volatile long *)(loc))
		     : "r" (newval), "0" (oldval)
		     : "memory");
	return val == oldval;
}

static inline void m0_mb(void)
{
	asm volatile("mfence":::"memory");
}

/** @} end of atomic group */
#endif /* __MOTR_LIB_USER_X86_64_ATOMIC_H__ */

/*
 *  Local variables:
 *  c-indentation-style: "K&R"
 *  c-basic-offset: 8
 *  tab-width: 8
 *  fill-column: 80
 *  scroll-step: 1
 *  End:
 */
