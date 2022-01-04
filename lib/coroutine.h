/* -*- C -*- */
/*
 * Copyright (c) 2017-2020 Seagate Technology LLC and/or its Affiliates
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

#ifndef __MOTR_LIB_COROUTINE_H__
#define __MOTR_LIB_COROUTINE_H__

/**
 * @defgroup Coroutine
 *
 * @{
 */

#include "lib/types.h"
#include "lib/errno.h"

enum {
	M0_MCC_STACK_NR            = 0x20,
	M0_MCC_LOCALS_ALLOC_SZ     = 4096,
	M0_MCC_LOCALS_ALLOC_SHIFT  = 3,
	M0_MCC_LOCALS_ALLOC_ALIGN  = 1ULL << M0_MCC_LOCALS_ALLOC_SHIFT,
	M0_MCC_LOCALS_ALLOC_PAD_SZ = 2 * M0_MCC_LOCALS_ALLOC_ALIGN,
};

struct m0_co_la_item {
	void     *lai_addr;
	uint64_t  lai_size;
};

/**
 * Simple stacked allocator used to save local to current coroutine context
 * variables, defined inside each `yielded' function with M0_CO_REENTER().
 */
struct m0_co_locals_allocator {
	/** allocated items */
	struct m0_co_la_item la_items[M0_MCC_STACK_NR];

	/** next allocated frame to be used */
	uint64_t             la_frame;

	/**
	 * This data is allocated with m0_alloc() to be far-far-away from
	 * user-space stack.
	 */
	void                *la_pool;
	/** totally allocated */
	uint64_t             la_total;
};

/**
 * Coroutine context.
 *
 * Saves current state, including coroutine local variables, control flow data,
 * other state variables w.r.t. restore this state after reentering the stack of
 * functions coroutines are used in.
 *
 * While working with coroutines, user has to use local on-stack variables with
 * understanding, that after yield their values have to be resorted manually.
 * Instead he has to put M0_CO_REENTER() inside the beginning of every function,
 * and inside every function in every function-stack which is being yielded.
 *
 * `Restorable variables' are used like locals and can be declared inside
 * M0_CO_REENTER().  The following code can be inside one of yielded functions:
 * @code
 * static void foo(struct m0_co_context *context, ...)
 * {
 *      M0_CO_REENTER(context,
 *                    int   a;
 *                    char  b[100];
 *                    int   rc;
 *      );
 *      ...
 * }
 * @endcode
 *
 * By design, `restorable variables' can be accessed with M0_CO_FRAME_DATA(),
 * which can be redefined and used like in the following example:
 * @code
 * #define F M0_CO_FRAME_DATA
 * static void foo(struct m0_co_context *context, ...)
 * {
 *      M0_CO_REENTER(context, ...
 *      );
 *      ...
 *      F(rc) = -EINVAL;
 *      F(a)  = 0xb100df100d;
 *      strncpy(F(b), "sillybuff", ARRAY_SIZE(F(b)));
 *      ...
 * }
 * @endcode
 *
 * It can be seen, that user has to maintain `struct m0_co_context context'
 * inside his own structures. For motr code, it's is implied to be related to
 * FOM or FOM objects. m0_co_context_{init,fini}() used for these purposes.
 *
 * Every function, which potentially can be unwinded during yield has to be
 * wrapped with M0_CO_FUN().
 *
 * Every place inside underlying functions wrapped by M0_CO_FUN() can be yielded
 * with M0_CO_YIELD(). After this call, function stack is being unwinded up to
 * the place in code where top-level coroutine call is being performed. Our code
 * assumes this place to be near by FOM tick()-function:
 * @code
 * int foo_tick(struct m0_fom *fom)
 * { ...
 *      M0_CO_START(&context);
 *      top_rc = toplevel_foo(&context); // calls foo()->foo0()->...->foo_n()
 *       co_rc = M0_CO_END(&context);
 *   ...
 * }
 * @endcode
 *
 * User controls execution of coroutine by checking `M0_CO_END(&context)'-value.
 * Coroutine is in progress if -EAGAIN is returned, if 0 -- it is succeeded.
 */
struct m0_co_context {
	/** frame address stack */
	void                         *mc_stack[M0_MCC_STACK_NR];
	/** frame locals stack */
	void                         *mc_locals[M0_MCC_STACK_NR];
	/** current frame pointer */
	uint64_t                      mc_frame;
	/** true if stack is unwinding */
	bool                          mc_yield;
	/** code returned from M0_CO_END() macro, set in M0_CO_YIELD() */
	int                           mc_co_end_ret;
	/** current frame pointer during reentering */
	uint64_t                      mc_yield_frame;
	/** simple pool allocator for locals */
	struct m0_co_locals_allocator mc_alloc;
};

/**
 * M0_CO_START()/M0_CO_END() wrap coroutine call and provide means to control
 * the control flow of it.
 */
#define M0_CO_START(context)                                              \
({                                                                        \
	M0_ASSERT((context)->mc_yield_frame == 0);                        \
})

/**
 * @param context -- @see m0_co_context
 * @return -EAGAIN if coroutine is in progress.
 * @return       0 if coroutine is succeeded.
 */
#define M0_CO_END(context)                                                \
({                                                                        \
	int rc = ((context)->mc_yield ? (context)->mc_co_end_ret : 0);    \
	if (rc == 0) {                                                    \
		M0_ASSERT((context)->mc_frame == 0);                      \
		M0_ASSERT((context)->mc_yield_frame == 0);                \
		m0_co_context_locals_free((context));                     \
	}                                                                 \
	rc;                                                               \
})

/**
 * @param _context -- @see m0_co_context
 * @param function -- function call with or without assignments
 *                    F(rc) = fooX(context, ...)
 */
#define M0_CO_FUN(context, function)                                      \
({                                                                        \
	__label__ save;                                                   \
	M0_LOG(M0_DEBUG, "M0_CO_FUN: context=%p yeild=%d",                 \
	       context, !!context->mc_yield);                             \
	M0_ASSERT(context->mc_frame < M0_MCC_STACK_NR);                   \
	context->mc_stack[context->mc_frame++] = &&save;                  \
save:   (function);                                                       \
	if (context->mc_yield) {                                          \
		return;                                                   \
	} else {                                                          \
		m0_co_context_locals_free(context);                       \
		context->mc_frame--;                                      \
	}                                                                 \
})

#define M0_CO_FRAME_DATA(field) (__frame_data__->field)

/**
 * @param _context -- @see m0_co_context
 * @param __VA_ARGS__ -- `Restorable variables', local context of this function
 */
#define M0_CO_REENTER(context, ...)                                       \
	struct foo_context {                                              \
		__VA_ARGS__                                               \
	};                                                                \
	struct foo_context *__frame_data__;                               \
	M0_CO__REENTER((context), __frame_data__);

/**
 * Mostly for internal usage. @see M0_CO_REENTER()
 * Typical usecase:
 * struct foo_context {
 *      int   a;
 *      ...
 *      int rc;
 * };
 * struct foo_context *data;
 * M0_CO__REENTER(context, data);
 */
#define M0_CO__REENTER(context, frame_data)                               \
({                                                                        \
	uint64_t size = sizeof(*frame_data);                              \
	M0_LOG(M0_DEBUG, "M0_CO_REENTER: context=%p yeild=%d",             \
	       context, !!context->mc_yield);                             \
	if (!context->mc_yield) {                                         \
		m0_co_context_locals_alloc(context, (size));              \
		frame_data = m0_co_context_locals(context);               \
	} else {                                                          \
		M0_ASSERT(context->mc_yield_frame < M0_MCC_STACK_NR);     \
		frame_data = m0_co_context_locals(context);               \
		goto *context->mc_stack[context->mc_yield_frame++];       \
	}                                                                 \
})

/**
 * M0_CO_YIELD() is used like return statement in cases, function needs to wait
 * for some external event, like IO. Typically, user has to arm something, which
 * will generete such an event and call M0_CO_YIELD(). On behalf of FOM, event
 * has to wake up this FOM when it's ready or in any other suitable case.  After
 * this point M0_CO_* machinery will return control flow back into the point
 * right after M0_CO_YIELD().
 *
 * @param _context -- @see m0_co_context
 */
#define M0_CO_YIELD_RC(context, rc)                                       \
({                                                                        \
	__label__ save;                                                   \
	M0_LOG(M0_DEBUG, "M0_CO_YIELD: context=%p yeild=%d",               \
	       context, !!context->mc_yield);                             \
	context->mc_yield = true;                                         \
	context->mc_co_end_ret = (rc);                                    \
	M0_ASSERT(context->mc_frame < M0_MCC_STACK_NR);                   \
	context->mc_stack[context->mc_frame++] = &&save;                  \
	return;                                                           \
save:                                                                     \
	M0_ASSERT(context->mc_yield);                                     \
	M0_ASSERT(context->mc_frame == context->mc_yield_frame);          \
	context->mc_yield = false;                                        \
	context->mc_yield_frame = 0;                                      \
	context->mc_frame--;                                              \
})

#define M0_CO_YIELD(context) M0_CO_YIELD_RC(context, -EAGAIN)


M0_INTERNAL int m0_co_context_init(struct m0_co_context *context);
M0_INTERNAL void m0_co_context_fini(struct m0_co_context *context);

/* internal */
M0_INTERNAL void *m0_co_context_locals(struct m0_co_context *context);
M0_INTERNAL void m0_co_context_locals_alloc(struct m0_co_context *context,
					    uint64_t size);
M0_INTERNAL void m0_co_context_locals_free(struct m0_co_context *context);

#include "sm/sm.h"         /* m0_sm_group */

struct m0_fom;

struct m0_co_op {
	struct m0_sm       co_sm;
	/* MOTR-787: get rid of explicit locking here, use locality lock! */
	struct m0_sm_group co_sm_group;
};

M0_INTERNAL void m0_co_op_init(struct m0_co_op *op);
M0_INTERNAL void m0_co_op_fini(struct m0_co_op *op);

M0_INTERNAL void m0_co_op_reset(struct m0_co_op *op);
M0_INTERNAL void m0_co_op_active(struct m0_co_op *op);
M0_INTERNAL void m0_co_op_done(struct m0_co_op *op);

/* Don't introduce co_op_wait()! co_op intended to be used inside foms. */
M0_INTERNAL int m0_co_op_tick_ret(struct m0_co_op *op,
				  struct m0_fom   *fom,
				  int              next_state);

/** @} end of Coroutine group */
#endif /* __MOTR_LIB_COROUTINE_H__ */

/*
 *  Local variables:
 *  c-indentation-style: "K&R"
 *  c-basic-offset: 8
 *  tab-width: 8
 *  fill-column: 80
 *  scroll-step: 1
 *  End:
 */
/*
 * vim: tabstop=8 shiftwidth=8 noexpandtab textwidth=80 nowrap
 */
