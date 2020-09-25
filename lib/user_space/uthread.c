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


#include <stdlib.h>      /* getenv */
#include <unistd.h>      /* getpid */
#include <errno.h>       /* program_invocation_name */
#include <stdio.h>       /* snprinf */

#if defined(M0_LINUX)
#include <linux/limits.h>       /* PATH_MAX */
#elif defined(M0_DARWIN)
#include <sys/syslimits.h>      /* PATH_MAX */
#include <mach/mach_init.h>
#include <mach/thread_policy.h>
#include <mach/mach.h>
#include <pthread.h>
#include <mach-o/dyld.h>        /* _NSGetExecutablePath */
#endif

#define M0_TRACE_SUBSYSTEM M0_TRACE_SUBSYS_MEMORY
#include "lib/trace.h"
#include "lib/misc.h"    /* M0_SET0 */
#include "lib/string.h"  /* m0_strdup */
#include "lib/memory.h"
#include "lib/errno.h"
#include "lib/thread.h"
#include "lib/arith.h"
#include "lib/bitmap.h"
#include "lib/assert.h"
#include "module/instance.h"  /* m0_set */
#include "addb2/global.h"     /* m0_addb2_global_thread_enter */

/**
   @addtogroup thread Thread

   Implementation of m0_thread on top of pthread_t.

   <b>Implementation notes</b>

   Instead of creating a new POSIX thread executing user-supplied function, all
   threads start executing the same trampoline function m0_thread_trampoline()
   that performs some generic book-keeping.

   Threads are created with a PTHREAD_CREATE_JOINABLE attribute.

   @{
 */

/**
 * Default pthread creation attribute.
 *
 * @todo move this in m0 instance.
 */
static pthread_attr_t pthread_attr_default;

static __thread struct m0_thread thread = {};
static __thread struct m0_thread_tls *tls = NULL;

static struct m0_thread main_thread = {
	.t_state = TS_RUNNING,
	.t_tls   = {
		.tls_self = &main_thread
	},
	.t_namebuf = "main()"
};

M0_INTERNAL struct m0_thread_tls *m0_thread_tls(void)
{
	if (tls == NULL) {
		thread = (struct m0_thread) {
			.t_state = TS_RUNNING,
			.t_tls   = {
				.tls_m0_instance =
					main_thread.t_tls.tls_m0_instance,
				.tls_self = &thread,
			},
			.t_namebuf = "anon"
		};
		tls = &thread.t_tls;
	}
	return tls;
}

static void *uthread_trampoline(void *arg)
{
	struct m0_thread *t = arg;

	tls = &t->t_tls;
	m0_thread_trampoline(arg);
	tls = NULL;
	return NULL;
}

M0_INTERNAL int m0_thread_init_impl(struct m0_thread *q, const char *_)
{
	M0_PRE(q->t_state == TS_RUNNING);

	return -pthread_create(&q->t_h.h_id, &pthread_attr_default,
			       uthread_trampoline, q);
}

int m0_thread_join(struct m0_thread *q)
{
	int result;

	M0_PRE(q->t_state == TS_RUNNING);
	M0_PRE(!pthread_equal(q->t_h.h_id, pthread_self()));

	result = -pthread_join(q->t_h.h_id, NULL);
	if (result == 0)
		q->t_state = TS_PARKED;
	return result;
}

M0_INTERNAL int m0_thread_signal(struct m0_thread *q, int sig)
{
	return -pthread_kill(q->t_h.h_id, sig);
}

M0_INTERNAL int m0_thread_confine(struct m0_thread *q,
				  const struct m0_bitmap *processors)
{
	size_t idx;
#if defined(M0_DARWIN)
	thread_port_t                 mthread;
	thread_affinity_policy_data_t policy = {};

	for (idx = 0; idx < processors->b_nr; ++idx) {
		if (m0_bitmap_get(processors, idx))
			break;
	}
	M0_ASSERT(idx < processors->b_nr);
	policy.affinity_tag = idx;
	mthread = pthread_mach_thread_np(q->t_h.h_id);
	thread_policy_set(mthread, THREAD_AFFINITY_POLICY,
			  (thread_policy_t)&policy, 1);
	return 0;
#else
	size_t    nr_bits = min64u(processors->b_nr, CPU_SETSIZE);
	cpu_set_t cpuset;

	CPU_ZERO(&cpuset);
	for (idx = 0; idx < nr_bits; ++idx) {
		if (m0_bitmap_get(processors, idx))
			CPU_SET(idx, &cpuset);
	}
	return -pthread_setaffinity_np(q->t_h.h_id, sizeof cpuset, &cpuset);
#endif
}

M0_INTERNAL char m0_argv0[PATH_MAX] = {};

M0_INTERNAL char *m0_debugger_args[4] = {
	NULL, /* debugger name */
	NULL, /* our binary name */
	NULL, /* our process id */
	NULL
};

M0_INTERNAL int m0_threads_init(struct m0 *instance)
{
	m0_set(instance);
	return -pthread_attr_init(&pthread_attr_default) ?:
		-pthread_attr_setdetachstate(&pthread_attr_default,
					     PTHREAD_CREATE_JOINABLE);
}

M0_INTERNAL void m0_threads_fini(void)
{
	(void)pthread_attr_destroy(&pthread_attr_default);
}

M0_INTERNAL int m0_threads_once_init(void)
{
	static char pidbuf[20];
	char       *env_ptr;
#if defined(M0_LINUX)

	if (readlink("/proc/self/exe", m0_argv0, sizeof m0_argv0) == -1)
		return M0_ERR_INFO(errno, "%s", strerror(errno));
	/*
	 * Note: program_invocation_name requires _GNU_SOURCE.
	 */
	m0_debugger_args[1] = program_invocation_name;
#elif defined(M0_DARWIN)
	int      result;
	uint32_t bufsize = ARRAY_SIZE(m0_argv0);

	result = _NSGetExecutablePath(m0_argv0, &bufsize);
	if (result != 0)
		return M0_ERR(-E2BIG);
	m0_debugger_args[1] = m0_argv0;
#endif
	env_ptr = getenv("M0_DEBUGGER");
	if (env_ptr != NULL)
		m0_debugger_args[0] = m0_strdup(env_ptr);
	m0_debugger_args[2] = pidbuf;
	(void)snprintf(pidbuf, ARRAY_SIZE(pidbuf), "%i", getpid());
	tls = &main_thread.t_tls;
	return 0;
}

M0_INTERNAL void m0_threads_once_fini(void)
{
	m0_free0(&m0_debugger_args[0]);
}

M0_INTERNAL void m0_enter_awkward(void)
{
	struct m0_thread_tls *tls = m0_thread_tls();

	/*
	 * m0_enter_awkward() can be called at arbitrary moment. It is possible
	 * that TLS is not yet set (or already released) at this time (this
	 * happens, for example, when a timer signal) arrives to a thread
	 * executing glibc code, creating or destroying a thread.
	 */
	if (tls != NULL)
		M0_CNT_INC(tls->tls_arch.tat_awkward);
}

M0_INTERNAL void m0_exit_awkward(void)
{
	struct m0_thread_tls *tls = m0_thread_tls();

	if (tls != NULL)
		M0_CNT_DEC(tls->tls_arch.tat_awkward);
}

M0_INTERNAL bool m0_is_awkward(void)
{
	return m0_thread_tls()->tls_arch.tat_awkward != 0;
}

M0_INTERNAL uint64_t m0_pid(void)
{
	return getpid();
}

M0_INTERNAL uint64_t m0_tid(void)
{
#if M0_DARWIN
	uint64_t tid;
	pthread_threadid_np(NULL, &tid);
	return tid;
#elif M0_LINUX
	return syscall(SYS_gettid);
#else
#error Platform not supported.
#endif
}

M0_INTERNAL uint64_t m0_process(void)
{
	return m0_pid();
}

M0_INTERNAL int m0_thread_arch_adopt(struct m0_thread *thread,
				     struct m0 *instance, bool full)
{
	M0_PRE(M0_IS0(thread));

	thread->t_tls.tls_self = thread;
	tls = &thread->t_tls;
	m0_set(instance);
	if (full)
		m0_addb2_global_thread_enter();
	return 0;
}

M0_INTERNAL void m0_thread_arch_shun(void)
{
	m0_addb2_global_thread_leave();
	tls = NULL;
}

#undef M0_TRACE_SUBSYSTEM

/** @} end of thread group */

/*
 *  Local variables:
 *  c-indentation-style: "K&R"
 *  c-basic-offset: 8
 *  tab-width: 8
 *  fill-column: 80
 *  scroll-step: 1
 *  End:
 */
