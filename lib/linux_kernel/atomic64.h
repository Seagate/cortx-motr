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
#ifndef __MOTR_LIB_LINUX_KERNEL_ATOMIC64_H__
#define __MOTR_LIB_LINUX_KERNEL_ATOMIC64_H__

#include <asm/atomic.h>  /* atomic64_set */
#include "lib/misc.h"    /* mb */

/**
   @addtogroup atomic

   Implementation of atomic operations for Linux user space uses x86_64 assembly
   language instructions (with gcc syntax). "Lock" prefix is used
   everywhere---no optimisation for non-SMP configurations in present.
 */

struct m0_atomic64 {
	atomic64_t a_value;
};

static inline void m0_atomic64_set(struct m0_atomic64 *a, int64_t num)
{
	M0_CASSERT(sizeof a->a_value == sizeof num);
	atomic64_set(&a->a_value, num);
}

static inline int64_t m0_atomic64_get(const struct m0_atomic64 *a)
{
	return	atomic64_read(&a->a_value);
}

static inline void m0_atomic64_inc(struct m0_atomic64 *a)
{
	atomic64_inc(&a->a_value);
}

static inline void m0_atomic64_dec(struct m0_atomic64 *a)
{
	atomic64_dec(&a->a_value);
}

static inline void m0_atomic64_add(struct m0_atomic64 *a, int64_t num)
{
	atomic64_add(num, &a->a_value);
}

static inline void m0_atomic64_sub(struct m0_atomic64 *a, int64_t num)
{
	atomic64_sub(num, &a->a_value);
}

static inline int64_t
m0_atomic64_add_return(struct m0_atomic64 *a, int64_t delta)
{
	return atomic64_add_return(delta, &a->a_value);
}

static inline int64_t
m0_atomic64_sub_return(struct m0_atomic64 *a, int64_t delta)
{
	return atomic64_sub_return(delta, &a->a_value);
}

static inline bool m0_atomic64_inc_and_test(struct m0_atomic64 *a)
{
	return (&a->a_value);
}

static inline bool m0_atomic64_dec_and_test(struct m0_atomic64 *a)
{
	return atomic64_dec_and_test(&a->a_value);
}

static inline bool m0_atomic64_cas(int64_t * loc, int64_t old, int64_t new)
{
	return cmpxchg64(loc, old, new) == old;
}

static inline void m0_mb(void)
{
	mb();
}

/** @} atomic */
#endif /* __MOTR_LIB_LINUX_KERNEL_ATOMIC64_H__ */

/*
 *  Local variables:
 *  c-indentation-style: "K&R"
 *  c-basic-offset: 8
 *  tab-width: 8
 *  fill-column: 80
 *  scroll-step: 1
 *  End:
 */
