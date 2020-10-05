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

#ifndef __MOTR_LIB_ATOMIC_H__
#define __MOTR_LIB_ATOMIC_H__

#include "motr/config.h"  /* CONFIG_X86_64 CONFIG_AARCH64 */
#include "lib/assert.h"
#include "lib/types.h"

#ifdef __KERNEL__
#    include "lib/linux_kernel/atomic64.h"
#elif defined (CONFIG_AARCH64)
#    include "lib/user_space/user_aarch64_atomic.h"
#elif defined (CONFIG_X86_64)
#    if defined(M0_DARWIN)
#        define ATOMIC_USE_X86_64 (0)
#        define ATOMIC_USE___SYNC (0)
#        define ATOMIC_USE_C11    (1)
#    endif
#    if defined(M0_LINUX)
#        ifdef ENABLE_SYNC_ATOMIC
#            define ATOMIC_USE_X86_64 (0)
#            define ATOMIC_USE___SYNC (1)
#            define ATOMIC_USE_C11    (0)
#        else
#            define ATOMIC_USE_X86_64 (1)
#            define ATOMIC_USE___SYNC (0)
#            define ATOMIC_USE_C11    (0)
#        endif
#    endif
#    if ATOMIC_USE_C11
#        include "lib/user_space/c11_atomic.h"
#    endif
#    if ATOMIC_USE___SYNC
#        include "lib/user_space/__sync_atomic.h"
#    endif
#    if ATOMIC_USE_X86_64
#        include "lib/user_space/user_x86_64_atomic.h"
#    endif
#else
#    error "Platform is not supported"
#endif

/**
   @defgroup atomic

   Atomic operations on 64bit quantities.

   Implementation of these is platform-specific.

   @{
 */

/**
   atomic counter
 */
struct m0_atomic64;

/**
   Assigns a value to a counter.
 */
static inline void m0_atomic64_set(struct m0_atomic64 *a, int64_t num);

/**
   Returns value of an atomic counter.
 */
static inline int64_t m0_atomic64_get(const struct m0_atomic64 *a);

/**
   Atomically increments a counter.
 */
static inline void m0_atomic64_inc(struct m0_atomic64 *a);

/**
   Atomically decrements a counter.
 */
static inline void m0_atomic64_dec(struct m0_atomic64 *a);

/**
   Atomically adds given amount to a counter.
 */
static inline void m0_atomic64_add(struct m0_atomic64 *a, int64_t num);

/**
   Atomically subtracts given amount from a counter.
 */
static inline void m0_atomic64_sub(struct m0_atomic64 *a, int64_t num);

/**
   Atomically increments a counter and returns the result.
 */
static inline int64_t m0_atomic64_add_return(struct m0_atomic64 *a,
						  int64_t d);

/**
   Atomically decrements a counter and returns the result.
 */
static inline int64_t m0_atomic64_sub_return(struct m0_atomic64 *a,
						  int64_t d);

/**
   Atomically increments a counter and returns true iff the result is 0.
 */
static inline bool m0_atomic64_inc_and_test(struct m0_atomic64 *a);

/**
   Atomically decrements a counter and returns true iff the result is 0.
 */
static inline bool m0_atomic64_dec_and_test(struct m0_atomic64 *a);

/**
   Atomic compare-and-swap: compares value stored in @loc with @oldval and, if
   equal, replaces it with @newval, all atomic w.r.t. concurrent accesses to @loc.

   Returns true iff new value was installed.
 */
static inline bool m0_atomic64_cas(int64_t * loc, int64_t oldval,
					int64_t newval);

/**
   Atomic compare-and-swap for pointers.

   @see m0_atomic64_cas().
 */
static inline bool m0_atomic64_cas_ptr(void **loc, void *oldval, void *newval)
{
	M0_CASSERT(sizeof loc == sizeof(int64_t *));
	M0_CASSERT(sizeof oldval == sizeof(int64_t));

	return m0_atomic64_cas((int64_t *)loc, (int64_t)oldval, (int64_t)newval);
}

#define M0_ATOMIC64_CAS(loc, oldval, newval)				\
({									\
	M0_CASSERT(__builtin_types_compatible_p(typeof(*(loc)), typeof(oldval))); \
	M0_CASSERT(__builtin_types_compatible_p(typeof(oldval), typeof(newval))); \
	m0_atomic64_cas_ptr((void **)(loc), oldval, newval);		\
})

/**
   Hardware memory barrier. Forces strict CPU ordering.
 */
static inline void m0_mb(void);

/** @} end of atomic group */

/* __MOTR_LIB_ATOMIC_H__ */
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
