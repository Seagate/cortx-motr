/* -*- C -*- */
/*
 * Copyright (c) 2021 Seagate Technology LLC and/or its Affiliates
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

#ifndef __MOTR_LIB_USER_AARCH64_ATOMIC_H__
#define __MOTR_LIB_USER_AARCH64_ATOMIC_H__

#include "lib/types.h"
#include "lib/assert.h"

/**
   @addtogroup atomic

   Implementation of atomic operations for Linux user space uses aarch64
   assembly language instructions (with gcc syntax). The aarch64 uses its own set of atomic
   assembely instruction to ensure atomicity ---no optimisation for non-SMP
   configurations in present.
 */

struct m0_atomic64 {
	long a_value;
};

static inline void m0_atomic64_add(struct m0_atomic64 *a, int64_t num);
static inline void m0_atomic64_sub(struct m0_atomic64 *a, int64_t num);

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
	m0_atomic64_add(a, (int64_t)1);
}

/**
 atomically decrement counter

 @param a pointer to atomic counter

 @return none
 */
static inline void m0_atomic64_dec(struct m0_atomic64 *a)
{
	m0_atomic64_sub(a, (int64_t)1);
}

/**
   Atomically adds given amount to a counter
 */
static inline void m0_atomic64_add(struct m0_atomic64 *a, int64_t num)
{
	long		result;
	unsigned long	tmp;
	asm volatile("// atomic64_add \n"		\
		     "  prfm    pstl1strm, %2\n"	\
		     "1:ldxr    %0, %2\n"		\
		     "  add     %0, %0, %3\n"   	\
		     "  stxr    %w1, %0, %2\n"  	\
		     "  cbnz    %w1, 1b"		\
		     : "=&r" (result), "=&r" (tmp), "+Q" (a->a_value) \
		     : "Ir" (num));
}

/**
   Atomically subtracts given amount from a counter
 */
static inline void m0_atomic64_sub(struct m0_atomic64 *a, int64_t num)
{
	long		result;
	unsigned long	tmp;
	
	asm volatile("// atomic64_sub \n"		\
		     "  prfm    pstl1strm, %2\n"	\
		     "1:ldxr    %0, %2\n"		\
		     "  sub     %0, %0, %3\n"   	\
		     "  stxr    %w1, %0, %2\n"  	\
		     "  cbnz    %w1, 1b"		\
		     : "=&r" (result), "=&r" (tmp), "+Q" (a->a_value) \
		     : "Ir" (num));
}


/**
 atomically increment counter and return result

 @param a pointer to atomic counter

 @return new value of atomic counter
 */
static inline int64_t m0_atomic64_add_return(struct m0_atomic64 *a,
						  int64_t delta)
{
	int64_t		result;
	uint64_t	tmp;

	asm volatile("// atomic64_add_return \n"	\
		     "  prfm    pstl1strm, %2\n"	\
		     "1:ldxr    %0, %2\n"		\
		     "  add     %0, %0, %3\n"   	\
		     "  stlxr   %w1, %0, %2\n"  	\
		     "  cbnz    %w1, 1b\n"		\
		     "  dmb ish"			\
		     : "=&r" (result), "=&r" (tmp), "+Q" (a->a_value) \
		     : "Ir" (delta)  			\
		     : "memory");
	return result;
}

/**
 atomically decrement counter and return result

 @param a pointer to atomic counter

 @return new value of atomic counter
 */
static inline int64_t m0_atomic64_sub_return(struct m0_atomic64 *a,
						  int64_t delta)
{
	int64_t		result;
	uint64_t	tmp;

	asm volatile( "// atomic64_sub_return \n"		\
		      "  prfm    pstl1strm, %2\n"		\
		      "1:ldxr    %0, %2\n"			\
		      "  sub     %0, %0, %3\n"  		\
		      "  stlxr   %w1, %0, %2\n" 		\
		      "  cbnz    %w1, 1b\n"			\
		      "  dmb ish"				\
		      : "=&r" (result), "=&r" (tmp), "+Q" (a->a_value) \
		      : "Ir" (delta)				\
		      : "memory");
	return result;
}

static inline bool m0_atomic64_inc_and_test(struct m0_atomic64 *a)
{
	return (m0_atomic64_add_return(a, 1) == 0);
}

static inline bool m0_atomic64_dec_and_test(struct m0_atomic64 *a)
{
	return (m0_atomic64_sub_return(a, 1) == 0);
}

static inline bool m0_atomic64_cas(int64_t * loc, int64_t oldval, int64_t newval)
{
/**
 * Since the undersigend commented code being processor specific assembly instruction,
 * This would act more comprehensible while porting to other platform.
 * Since this code has few flaws, not debugged yet.It is not working as of now.
 * The gcc specific routine is called in place of it which has no public source code available.
 * So it is kept as part of documentation for future reference and implementation.
 */
/*	unsigned long	tmp;
	int64_t	 old=0;

	M0_CASSERT(8 == sizeof oldval);

	asm volatile("// atomic64_cas_return \n"		\
		     "  prfm    pstl1strm, %[v]\n"		\
		     "1:ldxr    %[old], %[v]\n"  		\
		     "  eor     %[tmp], %[oldval], %[old]\n"	\
		     "  cbnz    %[tmp], 2f\n"			\
		     "  stxr    %w[tmp], %[newval], %[v]\n"  	\
		     "  cbnz    %w[tmp], 1b\n"			\
		     "  \n" 					\
		     "2:"					\
		     : [tmp] "=&r" (tmp), [oldval] "=&r" (oldval), \
		       [v] "+Q" (*(unsigned long *)loc) 	\
		     : [old] "Lr" (old), [newval] "r" (newval)  \
		     :);
	return old == oldval;// need to be reviewed
*/

	M0_CASSERT(8 == sizeof oldval);
	return __atomic_compare_exchange_n(loc, &oldval, newval, 0, __ATOMIC_SEQ_CST, __ATOMIC_SEQ_CST);
}

static inline void m0_mb(void)
{
	asm volatile("dsb sy":::"memory");
}

/** @} end of atomic group */
#endif /* __MOTR_LIB_USER_AARCH64_ATOMIC_H__ */

/*
 *  Local variables:
 *  c-indentation-style: "K&R"
 *  c-basic-offset: 8
 *  tab-width: 8
 *  fill-column: 80
 *  scroll-step: 1
 *  End:
 */
