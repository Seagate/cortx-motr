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

#ifndef __MOTR_LIB_USER_SPACE_THREAD_H__
#define __MOTR_LIB_USER_SPACE_THREAD_H__

#include <pthread.h>
#include <setjmp.h>     /* jmp_buf */
#include <signal.h>
#include "lib/types.h"  /* bool */

/**
   @addtogroup thread Thread

   <b>User space m0_thread implementation</b>

   User space implementation is straight-forwardly based on POSIX thread
   interface.

   @see m0_thread

   @{
 */

enum { M0_THREAD_NAME_LEN = 16 };

struct m0_thread_handle {
	pthread_t h_id;
};

/** User space thread-local storage. */
struct m0_thread_arch_tls {
	/** Non-zero iff the thread is in awkward context. */
	uint32_t    tat_awkward;
	/** Stack context/environment, saved with setjmp(3). */
	sigjmp_buf *tat_jmp;
};

#if defined(M0_LINUX)
/**
   Helper macro creating an anonymous function with a given body.

   For example:

   @code
   int x;

   result = M0_THREAD_INIT(&tcb, int, NULL,
                           LAMBDA(void, (int y) { printf("%i", x + y); } ), 1,
                           "add_%d", 1);
   @endcode

   LAMBDA is useful to create an "ad-hoc" function that can be passed as a
   call-back function pointer.

   @note resulting anonymous function can be called only while the block where
   LAMBDA macro was called is active. For example, in the code fragment above,
   the tcb thread must finish before the block where M0_THREAD_INIT() was called
   is left.

   @note Be careful if using LAMBDA in kernel code, as the code could be
   generated on the stack and would fault in the kernel as it is execution
   protected in the kernel.

   The long story. Consider an arrangement like

   @code
   int foo(int x)
   {
           int y = x + 1;

	   bar(LAMBDA(int, (int z) { return z + y; }));
	   ...
   }
   @endcode

   Let's call the lambda function defined above L. 'y' is called a "free
   variable" of L, because it is neither declared in L's body nor passed as a
   parameter. When bar() or some of the functions (transitively) called by bar()
   calls L, L needs access to its parameters (z), supplied by its caller, and
   also to its free variables (y). y is stored in foo's stack frame and y's
   location on the stack, relative to L's frame is, in general, impossible to
   determine statically. For example, bar() can call baz(), which can store L in
   some data-structure, call quux() that will extract L from the data-structure
   and call it.

   This means that to call L, in addition, to the address of L's executable
   code, one also has to supply references to all its free variables. A
   structure, containing address of a function, together with references to the
   function's free variables is called a "closure"
   (http://en.wikipedia.org/wiki/Closure_(computer_science)). A closure is
   sufficient to call a function, but it is incompatible with the format of
   other function pointers. For example, given a value of type int (*func)(int),
   it is impossible to tell whether func is a pointer to a closure or a "normal"
   function pointer to a function without free variable. To work around this, gcc
   dynamically creates for each closure a small "trampoline function" that
   effectively builds closure and calls it. For example, the trampoline for L
   would look like

   @code
   int __L_trampoline(int z)
   {
           int *y = (int *)LITERAL_ADDRESS_OF_y;
	   return L(z, *y);
   }
   @endcode

   The trampoline is built when foo() is called and y's address is known. Once
   it is built, __L_trampoline()'s address can be passed to other functions
   safely. Note that __L_trampoline code is generated dynamically at
   run-time. Moreover gcc stores trampoline's code at the stack. And in kernel
   stack pages has no execution bit, to prevent exploits.

   Summary:

       - if your LAMBDA() has no free variables, you are safe;

       - if your LAMBDA() has only free variables with statically known
         addresses (e.g., global variables) you are safe, because gcc doesn't
         generate trampoline in this case;

       - otherwise

           - you are unsafe in kernel;

           - you are safe in user space, provided a lambda function is never
             called after the function in which the lambda function is defined
             returns (because when foo() returns, __L_trampoline() is
             destroyed).

   @see http://en.wikipedia.org/wiki/Lambda_calculus
 */
#define LAMBDA(T, ...) ({ T __lambda __VA_ARGS__; &__lambda; })
#define CAPTURED
#define LAMBDA_T *
#endif

#if defined(M0_DARWIN)
#define LAMBDA(T, ...) (^ T __VA_ARGS__)
#define CAPTURED __block
#define LAMBDA_T ^
#endif

/** @} end of thread group */

/* __MOTR_LIB_USER_SPACE_THREAD_H__ */
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
