/* -*- C -*- */
/*
 * Copyright (c) 2013-2020 Seagate Technology LLC and/or its Affiliates
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


#include "ut/ut.h"		/* M0_UT_ASSERT */
#include "lib/time.h"		/* m0_time_t */
#include "lib/timer.h"		/* m0_timer */
#include "lib/assert.h"		/* M0_ASSERT */
#include "lib/thread.h"		/* M0_THREAD_INIT */
#include "lib/semaphore.h"	/* m0_semaphore */
#include "lib/memory.h"		/* M0_ALLOC_ARR */
#include "lib/atomic.h"		/* m0_atomic64 */
#include "lib/ub.h"		/* m0_ub_set */
#include "lib/trace.h"		/* M0_LOG */

#include <stdlib.h>		/* rand */
#include <unistd.h>		/* syscall */
#include <sys/syscall.h>	/* syscall */

enum {
	NR_TIMERS     =  8,	/* number of timers in tests */
	NR_TG	      =  8,	/* number of thread groups */
	NR_THREADS_TG =  8,	/* number of worker threads per thread group */
	NR_TIMERS_TG  = 50,	/* number of timers per thread group */
};

struct thread_group;

struct tg_worker {
	pid_t                tgs_tid;
	struct m0_semaphore  tgs_sem_init;
	struct m0_semaphore  tgs_sem_resume;
	struct thread_group *tgs_group;
};

struct tg_timer {
	struct m0_timer	     tgt_timer;
	struct thread_group *tgt_group;
	m0_time_t	     tgt_expire;
	struct m0_semaphore  tgt_done;
};

struct thread_group {
	struct m0_thread	 tg_controller;
	struct m0_thread	 tg_threads[NR_THREADS_TG];
	struct tg_worker	 tg_workers[NR_THREADS_TG];
	struct m0_semaphore	 tg_sem_init;
	struct m0_semaphore	 tg_sem_resume;
	struct m0_semaphore	 tg_sem_done;
	struct tg_timer		 tg_timers[NR_TIMERS_TG];
	struct m0_timer_locality tg_loc;
	unsigned int		 tg_seed;
};

static pid_t		   loc_default_tid;
static struct m0_semaphore loc_default_lock;

static pid_t		    test_locality_tid;
static struct m0_semaphore *test_locality_lock;

static struct m0_atomic64 callbacks_executed;

static m0_time_t make_time(int ms)
{
	return m0_time(ms / 1000, ms % 1000 * 1000000);
}

static m0_time_t make_time_abs(int ms)
{
	return m0_time_add(m0_time_now(), make_time(ms));
}

static int time_rand_ms(int min_ms, int max_ms)
{
	return min_ms + (rand() * 1. / RAND_MAX) * (max_ms - min_ms);
}

static void sem_init_zero(struct m0_semaphore *sem)
{
	int rc = m0_semaphore_init(sem, 0);
	M0_UT_ASSERT(rc == 0);
}

static unsigned long timer_callback(unsigned long data)
{
	m0_atomic64_inc(&callbacks_executed);
	return 0;
}

/**
   Test timers.

   @param timer_type timer type
   @param nr_timers number of timers in this test
   @param interval_min_ms minimum value for timer interval
   @param interval_max_ms maximum value for timer interval
	  for every timer it will be chosen with rand()
	  in range [interval_min_ms, interval_max_ms]
   @param wait_time_ms function will wait this time and then m0_time_stop()
	  for all timers
   @param callbacks_min @see callbacks_max
   @param callbacks_max number of executed callbacks should be
	  in the interval [callbacks_min, callbacks_max]
	  (this is checked with M0_UT_ASSERT())
 */
static void test_timers(enum m0_timer_type timer_type, int nr_timers,
		int interval_min_ms, int interval_max_ms,
		int wait_time_ms, int callbacks_min, int callbacks_max)
{
	struct m0_timer *timers;
	int		 i;
	int		 time;
	int		 rc;
	m0_time_t	 wait;
	m0_time_t	 rem;

	m0_atomic64_set(&callbacks_executed, 0);
	srand(0);
	M0_ALLOC_ARR(timers, nr_timers);
	M0_UT_ASSERT(timers != NULL);
	if (timers == NULL)
		return;

	/* m0_timer_init() */
	for (i = 0; i < nr_timers; ++i) {
		time = time_rand_ms(interval_min_ms, interval_max_ms);
		rc = m0_timer_init(&timers[i], timer_type, NULL,
				   timer_callback, i);
		M0_UT_ASSERT(rc == 0);
	}
	/* m0_timer_start() */
	for (i = 0; i < nr_timers; ++i)
		m0_timer_start(&timers[i], make_time_abs(time));
	/* wait some time */
	wait = make_time(wait_time_ms);
	do {
		rc = m0_nanosleep(wait, &rem);
		wait = rem;
	} while (rc != 0);
	/* m0_timer_stop() */
	for (i = 0; i < nr_timers; ++i)
		m0_timer_stop(&timers[i]);
	/* m0_timer_fini() */
	for (i = 0; i < nr_timers; ++i)
		m0_timer_fini(&timers[i]);

	M0_UT_ASSERT(m0_atomic64_get(&callbacks_executed) >= callbacks_min);
	M0_UT_ASSERT(m0_atomic64_get(&callbacks_executed) <= callbacks_max);

	m0_free(timers);
}

static unsigned long locality_default_callback(unsigned long data)
{
<<<<<<< HEAD
	M0_UT_ASSERT(_gettid() == loc_default_tid);
=======
	M0_UT_ASSERT(m0_tid() == loc_default_tid);
>>>>>>> Initial commit of Darwin (aka XNU aka macos) port.
	m0_semaphore_up(&loc_default_lock);
	return 0;
}

/*
 * This test will be successful iff it is called from process main thread.
 */
static void timer_locality_default_test()
{
	int		rc;
	struct m0_timer timer;

	rc = m0_timer_init(&timer, M0_TIMER_HARD, NULL,
			   &locality_default_callback, 0);
	M0_UT_ASSERT(rc == 0);

	sem_init_zero(&loc_default_lock);

<<<<<<< HEAD
	loc_default_tid = _gettid();
=======
	loc_default_tid = m0_tid();
>>>>>>> Initial commit of Darwin (aka XNU aka macos) port.
	m0_timer_start(&timer, make_time_abs(100));
	m0_semaphore_down(&loc_default_lock);

	m0_timer_stop(&timer);

	m0_semaphore_fini(&loc_default_lock);
	m0_timer_fini(&timer);
}

static unsigned long locality_test_callback(unsigned long data)
{
	M0_ASSERT(data >= 0);
<<<<<<< HEAD
	M0_ASSERT(test_locality_tid == _gettid());
=======
	M0_ASSERT(test_locality_tid == m0_tid());
>>>>>>> Initial commit of Darwin (aka XNU aka macos) port.
	m0_semaphore_up(&test_locality_lock[data]);
	return 0;
}

static void timer_locality_test(int nr_timers,
		int interval_min_ms, int interval_max_ms)
{
	int                      i;
	int                      rc;
	struct m0_timer         *timers;
	struct m0_timer_locality loc = {};
	int                      time;

	M0_ALLOC_ARR(timers, nr_timers);
	M0_UT_ASSERT(timers != NULL);
	if (timers == NULL)
		return;

	M0_ALLOC_ARR(test_locality_lock, nr_timers);
	M0_UT_ASSERT(test_locality_lock != NULL);
	if (test_locality_lock == NULL)
		goto free_timers;

<<<<<<< HEAD
	test_locality_tid = _gettid();
=======
	test_locality_tid = m0_tid();
>>>>>>> Initial commit of Darwin (aka XNU aka macos) port.
	for (i = 0; i < nr_timers; ++i)
		sem_init_zero(&test_locality_lock[i]);

	m0_timer_locality_init(&loc);
	rc = m0_timer_thread_attach(&loc);
	M0_UT_ASSERT(rc == 0);

	/* m0_timer_init() */
	for (i = 0; i < nr_timers; ++i) {
		time = time_rand_ms(interval_min_ms, interval_max_ms);
		rc = m0_timer_init(&timers[i], M0_TIMER_HARD, &loc,
				   &locality_test_callback, i);
		M0_UT_ASSERT(rc == 0);
	}

	/* m0_timer_start() */
	for (i = 0; i < nr_timers; ++i)
		m0_timer_start(&timers[i], make_time_abs(time));

	for (i = 0; i < nr_timers; ++i)
		m0_semaphore_down(&test_locality_lock[i]);

	/* m0_timer_stop(), m0_timer_fini() */
	for (i = 0; i < nr_timers; ++i) {
		m0_timer_stop(&timers[i]);
		m0_timer_fini(&timers[i]);
		m0_semaphore_fini(&test_locality_lock[i]);
	}

	m0_timer_thread_detach(&loc);
	m0_timer_locality_fini(&loc);

	m0_free(test_locality_lock);
free_timers:
	m0_free(timers);
}

/*
   It isn't safe to use M0_UT_ASSERT() in signal handler code,
   therefore instead of M0_UT_ASSERT() was used M0_ASSERT().
 */
static unsigned long test_timer_callback_mt(unsigned long data)
{
	struct tg_timer *tgt = (struct tg_timer *)data;
	bool		 found = false;
<<<<<<< HEAD
	pid_t		 tid = _gettid();
=======
	pid_t		 tid = m0_tid();
>>>>>>> Initial commit of Darwin (aka XNU aka macos) port.
	int		 i;

	M0_ASSERT(tgt != NULL);
	/* check thread ID */
	for (i = 0; i < NR_THREADS_TG; ++i)
		if (tgt->tgt_group->tg_workers[i].tgs_tid == tid) {
			found = true;
			break;
		}
	M0_ASSERT(found);
	/* callback is done */
	m0_semaphore_up(&tgt->tgt_done);
	return 0;
}

static void test_timer_worker_mt(struct tg_worker *worker)
{
	int rc;

<<<<<<< HEAD
	worker->tgs_tid = _gettid();
=======
	worker->tgs_tid = m0_tid();
>>>>>>> Initial commit of Darwin (aka XNU aka macos) port.
	/* add worker thread to locality */
	rc = m0_timer_thread_attach(&worker->tgs_group->tg_loc);
	M0_UT_ASSERT(rc == 0);
	/* signal to controller thread about init */
	m0_semaphore_up(&worker->tgs_sem_init);

	/* now m0_timer callback can be executed in this thread context */

	/* wait for controller thread */
	m0_semaphore_down(&worker->tgs_sem_resume);
	/* remove current thread from thread group locality */
	m0_timer_thread_detach(&worker->tgs_group->tg_loc);
}

static void test_timer_controller_mt(struct thread_group *tg)
{
	int		 i;
	int		 rc;
	struct tg_timer *tgt;

	/* init() all worker semaphores */
	for (i = 0; i < NR_THREADS_TG; ++i) {
		sem_init_zero(&tg->tg_workers[i].tgs_sem_init);
		sem_init_zero(&tg->tg_workers[i].tgs_sem_resume);
	}
	/* init() timer locality */
	m0_timer_locality_init(&tg->tg_loc);
	/* init() and start all worker threads */
	for (i = 0; i < NR_THREADS_TG; ++i) {
		tg->tg_workers[i].tgs_group = tg;
		rc = M0_THREAD_INIT(&tg->tg_threads[i], struct tg_worker *,
				    NULL, &test_timer_worker_mt,
				    &tg->tg_workers[i], "timer worker #%d", i);
		M0_UT_ASSERT(rc == 0);
	}
	/* wait until all workers initialized */
	for (i = 0; i < NR_THREADS_TG; ++i)
		m0_semaphore_down(&tg->tg_workers[i].tgs_sem_init);
	/* init() all timers */
	for (i = 0; i < NR_TIMERS_TG; ++i) {
		tgt = &tg->tg_timers[i];
		tgt->tgt_group = tg;
		/* expiration time is in [now + 1, now + 100] ms range */
		tgt->tgt_expire = m0_time_from_now(0,
					(1 + rand_r(&tg->tg_seed) % 100) *
					 1000000);
		/* init timer semaphore */
		sem_init_zero(&tgt->tgt_done);
		/* `unsigned long' must have enough space to contain `void*' */
		M0_CASSERT(sizeof(unsigned long) >= sizeof(void *));
		/*
		 * create timer.
		 * parameter for callback is pointer to corresponding
		 * `struct tg_timer'
		 */
		rc = m0_timer_init(&tg->tg_timers[i].tgt_timer, M0_TIMER_HARD,
				   &tg->tg_loc,
				   test_timer_callback_mt, (unsigned long) tgt);
		M0_UT_ASSERT(rc == 0);
	}
	/* synchronize with all controller threads */
	m0_semaphore_up(&tg->tg_sem_init);
	m0_semaphore_down(&tg->tg_sem_resume);
	/* start() all timers */
	for (i = 0; i < NR_TIMERS_TG; ++i)
		m0_timer_start(&tg->tg_timers[i].tgt_timer, tgt->tgt_expire);
	/* wait for all timers */
	for (i = 0; i < NR_TIMERS_TG; ++i)
		m0_semaphore_down(&tg->tg_timers[i].tgt_done);
	/* stop() all timers */
	for (i = 0; i < NR_TIMERS_TG; ++i)
		m0_timer_stop(&tg->tg_timers[i].tgt_timer);
	/* fini() all timers */
	for (i = 0; i < NR_TIMERS_TG; ++i) {
		m0_timer_fini(&tg->tg_timers[i].tgt_timer);
		m0_semaphore_fini(&tg->tg_timers[i].tgt_done);
	}
	/* resume all workers */
	for (i = 0; i < NR_THREADS_TG; ++i)
		m0_semaphore_up(&tg->tg_workers[i].tgs_sem_resume);
	/* fini() all worker threads */
	for (i = 0; i < NR_THREADS_TG; ++i) {
		rc = m0_thread_join(&tg->tg_threads[i]);
		M0_UT_ASSERT(rc == 0);
		m0_thread_fini(&tg->tg_threads[i]);
	}
	/* fini() thread group locality */
	m0_timer_locality_fini(&tg->tg_loc);
	/* fini() all worker semaphores */
	for (i = 0; i < NR_THREADS_TG; ++i) {
		m0_semaphore_fini(&tg->tg_workers[i].tgs_sem_init);
		m0_semaphore_fini(&tg->tg_workers[i].tgs_sem_resume);
	}
	/* signal to main thread */
	m0_semaphore_up(&tg->tg_sem_done);
}

/**
   @verbatim
   this (main) thread		controller threads		worker threads
   start all controllers
   wait for controllers init
				init workers
				wait for all workers
				barrier with workers	barrier with controller
				sync with main
				wait for all controllers
   barrier with all controllers	barrier with main
				init all timers
				run all timers
				fini() all timers
				barrier with workers	barrier with controller
							detach from locality
							exit from thread
				wait for workers
					termination
   barrier with all controllers	barrier with main
				exit from thread
   wait for controllers termination
   @endverbatim
 */
static void test_timer_many_timers_mt()
{
	int			   i;
	int			   rc;
	static struct thread_group tg[NR_TG];

	/* init() all semaphores */
	for (i = 0; i < NR_TG; ++i) {
		sem_init_zero(&tg[i].tg_sem_init);
		sem_init_zero(&tg[i].tg_sem_resume);
		sem_init_zero(&tg[i].tg_sem_done);
	}

	/* init RNG seeds for thread groups */
	for (i = 0; i < NR_TG; ++i)
		tg[i].tg_seed = i;

	/* start all controllers from every thread group */
	for (i = 0; i < NR_TG; ++i) {
		rc = M0_THREAD_INIT(&tg[i].tg_controller, struct thread_group *,
				    NULL, &test_timer_controller_mt,
				    &tg[i], "timer ctrller");
		M0_UT_ASSERT(rc == 0);
	}

	/* wait for controllers initializing */
	for (i = 0; i < NR_TG; ++i)
		m0_semaphore_down(&tg[i].tg_sem_init);

	/* resume all controllers */
	for (i = 0; i < NR_TG; ++i)
		m0_semaphore_up(&tg[i].tg_sem_resume);

	/* wait for finishing */
	for (i = 0; i < NR_TG; ++i)
		m0_semaphore_down(&tg[i].tg_sem_done);

	/* fini() all semaphores and controller threads */
	for (i = 0; i < NR_TG; ++i) {
		m0_semaphore_fini(&tg[i].tg_sem_init);
		m0_semaphore_fini(&tg[i].tg_sem_resume);
		m0_semaphore_fini(&tg[i].tg_sem_done);

		rc = m0_thread_join(&tg[i].tg_controller);
		M0_UT_ASSERT(rc == 0);
		m0_thread_fini(&tg[i].tg_controller);
	}
}

void test_timer(void)
{
	int		   i;
	int		   j;
	enum m0_timer_type timer_types[2] = {M0_TIMER_SOFT, M0_TIMER_HARD};

	/* soft and hard timers tests */
	for (j = 0; j < 2; ++j)
		for (i = 1; i < NR_TIMERS; ++i) {
			/* simple test */
			test_timers(timer_types[j], i, 0, 10, 5, 0, i);
			/* zero-time test */
			test_timers(timer_types[j], i, 0, 0, 50, i, i);
			/* cancel-timer-before-callback-executed test */
			test_timers(timer_types[j], i, 10000, 10000, 50, 0, 0);
		}
	/* simple hard timer default locality test */
	timer_locality_default_test();
	/* test many hard timers in locality */
	for (i = 1; i < NR_TIMERS; ++i)
		timer_locality_test(i, 10, 30);
	/* hard timer test in multithreaded environment */
	test_timer_many_timers_mt();
}

enum { UB_TIMER_NR = 0x1 };

static struct m0_timer	   timer_ub_timers[UB_TIMER_NR];
static struct m0_semaphore timer_ub_semaphores[UB_TIMER_NR];
static enum m0_timer_type  timer_ub_type;
static m0_time_t	   timer_ub_expiration;
static m0_timer_callback_t timer_ub_cb;

unsigned long timer_ub_callback_dummy(unsigned long unused)
{
	return 0;
}

static int timers_ub_init(const char *opts)
{
	return 0;
}

static void timers_ub_fini(void)
{
}

static void timer_ub__init(int index)
{
	int rc = m0_timer_init(&timer_ub_timers[index], timer_ub_type, NULL,
			       timer_ub_cb, index);
	M0_UB_ASSERT(rc == 0);
}

static void timer_ub__fini(int index)
{
	m0_timer_fini(&timer_ub_timers[index]);
}

static void timer_ub__start(int index)
{
	m0_timer_start(&timer_ub_timers[index], timer_ub_expiration);
}

static void timer_ub__stop(int index)
{
	m0_timer_stop(&timer_ub_timers[index]);
}

static void timer_ub_for_each(void (*func)(int index))
{
	int i;

	for (i = 0; i < UB_TIMER_NR; ++i)
		func(i);
}

static void timer_ub_init_dummy(void)
{
	timer_ub_expiration = M0_TIME_NEVER;
	timer_ub_cb	    = &timer_ub_callback_dummy;
}

static void timer_ub_soft_init_dummy(void)
{
	timer_ub_init_dummy();
	timer_ub_type = M0_TIMER_SOFT;
}

static void timer_ub_hard_init_dummy(void)
{
	timer_ub_init_dummy();
	timer_ub_type = M0_TIMER_HARD;
}

static void timer_ub_soft_init_all(void)
{
	timer_ub_soft_init_dummy();
	timer_ub_for_each(&timer_ub__init);
}

static void timer_ub_hard_init_all(void)
{
	timer_ub_hard_init_dummy();
	timer_ub_for_each(&timer_ub__init);
}

static void timer_ub_soft_init_start_all(void)
{
	timer_ub_soft_init_all();
	timer_ub_for_each(&timer_ub__start);
}

static void timer_ub_hard_init_start_all(void)
{
	timer_ub_hard_init_all();
	timer_ub_for_each(&timer_ub__start);
}

static void timer_ub_fini_dummy(void)
{
}

static void timer_ub_fini_all(void)
{
	timer_ub_for_each(&timer_ub__fini);
}

static void timer_ub_stop_fini_all(void)
{
	timer_ub_for_each(&timer_ub__stop);
	timer_ub_fini_all();
}

static void timer_ub_start_stop(int index)
{
	timer_ub__start(index);
	timer_ub__stop(index);
}

static void timer_ub_init_start_stop(int index)
{
	timer_ub__init(index);
	timer_ub__start(index);
	timer_ub__stop(index);
}

static void timer_ub_init_start_stop_fini(int index)
{
	timer_ub__init(index);
	timer_ub__start(index);
	timer_ub__stop(index);
	timer_ub__fini(index);
}

unsigned long timer_ub_callback(unsigned long index)
{
	m0_semaphore_up(&timer_ub_semaphores[index]);
	return 0;
}

static void timer_ub__init_cb(int index)
{
	int rc;

	timer_ub__init(index);
	rc = m0_semaphore_init(&timer_ub_semaphores[index], 0);
	M0_UB_ASSERT(rc == 0);
}

static void timer_ub__fini_cb(int index)
{
	m0_semaphore_fini(&timer_ub_semaphores[index]);
	timer_ub__fini(index);
}

static void timer_ub_init_cb_dummy(void)
{
	timer_ub_expiration = m0_time_now();
	timer_ub_cb	    = &timer_ub_callback;
}

static void timer_ub_soft_init_cb_all(void)
{
	timer_ub_init_cb_dummy();
	timer_ub_type = M0_TIMER_SOFT;
	timer_ub_for_each(&timer_ub__init_cb);
}

static void timer_ub_hard_init_cb_all(void)
{
	timer_ub_init_cb_dummy();
	timer_ub_type = M0_TIMER_HARD;
	timer_ub_for_each(&timer_ub__init_cb);
}

static void timer_ub_soft_init_start_cb_all(void)
{
	timer_ub_soft_init_cb_all();
	timer_ub_for_each(&timer_ub__start);
}

static void timer_ub_hard_init_start_cb_all(void)
{
	timer_ub_hard_init_cb_all();
	timer_ub_for_each(&timer_ub__start);
}

static void timer_ub_start_callback(int index)
{
	timer_ub__start(index);
	m0_semaphore_down(&timer_ub_semaphores[index]);
}

static void timer_ub_fini_cb_all(void)
{
	timer_ub_for_each(&timer_ub__fini_cb);
}

static void timer_ub_stop_fini_cb_all(void)
{
	timer_ub_for_each(&timer_ub__stop);
	timer_ub_fini_cb_all();
}

static void timer_ub_callback_stop(int index)
{
	m0_semaphore_down(&timer_ub_semaphores[index]);
	timer_ub__stop(index);
}

static void timer_ub_init_start_callback_stop_fini(int index)
{
	timer_ub_expiration = m0_time_now();
	timer_ub_cb	    = &timer_ub_callback;
	timer_ub__init_cb(index);
	timer_ub__start(index);
	m0_semaphore_down(&timer_ub_semaphores[index]);
	timer_ub__stop(index);
	timer_ub__fini_cb(index);
}

#define TIMER_UB(name, init, round, fini) (struct m0_ub_bench) {	\
	.ub_name = name,						\
	.ub_iter = UB_TIMER_NR,						\
	.ub_init = timer_ub_##init,					\
	.ub_fini = timer_ub_##fini,					\
	.ub_round = timer_ub_##round,					\
}

#define TIMER_UB2(name, init, round, fini)				\
	TIMER_UB("S-"name, soft_##init, round, fini),		\
	TIMER_UB("H-"name, hard_##init, round, fini)

/*
 * <init>-fini
 * init-<fini>
 * init-<start>-stop-fini
 * init-start-<stop>-fini
 * init-<start-stop>-fini
 * <init-start-stop>-fini
 * <init-start-stop-fini>
 * init-<start-callback>-stop-fini
 * init-start-<callback-stop>-fini
 * <init-start-callback-stop-fini>
 */
struct m0_ub_set m0_timer_ub = {
	.us_name = "timer-ub",
	.us_init = timers_ub_init,
	.us_fini = timers_ub_fini,
	.us_run  = {
		TIMER_UB2("<init>-fini", init_dummy, _init, fini_all),
		TIMER_UB2("init-<fini>", init_all, _fini, fini_dummy),
		TIMER_UB2("init-<start>-stop-fini",
			  init_all, _start, stop_fini_all),
		TIMER_UB2("init-start-<stop>-fini",
			  init_start_all, _stop, fini_all),
		TIMER_UB2("init-<start-stop>-fini",
			  init_all, start_stop, fini_all),
		TIMER_UB2("<init-start-stop>-fini",
			  init_dummy, init_start_stop, fini_all),
		TIMER_UB2("<init-start-stop-fini>",
			  init_dummy, init_start_stop_fini, fini_dummy),
		TIMER_UB2("init-<start-callback>-stop-fini",
			  init_cb_all, start_callback, stop_fini_cb_all),
		TIMER_UB2("init-start-<callback-stop>-fini",
			  init_start_cb_all, callback_stop, fini_cb_all),
		TIMER_UB2("<init-start-callback-stop-fini>",
			  init_dummy, init_start_callback_stop_fini,
			  fini_dummy),
		{ .ub_name = NULL }
	}
};

#undef TIMER_UB
#undef TIMER_UB2

/*
 *  Local variables:
 *  c-indentation-style: "K&R"
 *  c-basic-offset: 8
 *  tab-width: 8
 *  fill-column: 80
 *  scroll-step: 1
 *  End:
 */
