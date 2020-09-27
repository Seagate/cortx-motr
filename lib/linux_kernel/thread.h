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

#ifndef __MOTR_LIB_LINUX_KERNEL_THREAD_H__
#define __MOTR_LIB_LINUX_KERNEL_THREAD_H__

#include <linux/kthread.h>
#include <linux/hardirq.h>

/**
   @addtogroup thread Thread

   <b>Linux kernel m0_thread implementation</b>

   Kernel space implementation is based <linux/kthread.h>

   @see m0_thread

   @{
 */

enum { M0_THREAD_NAME_LEN = TASK_COMM_LEN };

struct m0_thread_handle {
	struct task_struct *h_tsk;
	unsigned long       h_pid;
};

/** Kernel thread-local storage. */
struct m0_thread_arch_tls {
	void *tat_prev;
};

struct m0_thread;
M0_INTERNAL void m0_thread_enter(struct m0_thread *thread, bool full);
M0_INTERNAL void m0_thread_leave(void);
M0_INTERNAL void m0_thread__cleanup(struct m0_thread *bye);

#define M0_THREAD_ENTER						\
	struct m0_thread __th						\
		__attribute__((cleanup(m0_thread__cleanup))) = { 0, };	\
	m0_thread_enter(&__th, true)

M0_INTERNAL struct m0_thread_tls *m0_thread_tls_pop(void);
M0_INTERNAL void m0_thread_tls_back(struct m0_thread_tls *tls);

#define LAMBDA(T, ...) DO-NOT-USE-LAMBDA-IN-KERNEL!@$%^&* 
#define CAPTURED 
#define LAMBDA_T *

/** @} end of thread group */
#endif /* __MOTR_LIB_LINUX_KERNEL_THREAD_H__ */

/*
 *  Local variables:
 *  c-indentation-style: "K&R"
 *  c-basic-offset: 8
 *  tab-width: 8
 *  fill-column: 80
 *  scroll-step: 1
 *  End:
 */
