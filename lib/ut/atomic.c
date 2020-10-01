/* -*- C -*- */
/*
 * Copyright (c) 2011-2020 Seagate Technology LLC and/or its Affiliates
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


#include <time.h>    /* nanosleep */
#include <stdlib.h>
#include <string.h>
#include <pthread.h> /* barrier */

#include "lib/misc.h"   /* M0_SET0 */
#include "ut/ut.h"
#include "lib/ub.h"
#include "lib/thread.h"
#include "lib/atomic.h"
#include "lib/assert.h"

#ifdef HAVE_PTHREAD_BARRIER_T
enum {
	NR = 64
};

static struct m0_atomic64 atom;
static pthread_barrier_t bar[NR];
static pthread_barrier_t let[NR];

static void wait(pthread_barrier_t *b)
{
	int result;

	result = pthread_barrier_wait(b);
	M0_ASSERT(result == 0 || result == PTHREAD_BARRIER_SERIAL_THREAD);
}

static void worker(int id)
{
	int i;
	int j;
	int k;

	struct timespec delay;

	for (i = 0; i < NR; ++i) {
		wait(&let[i]);
		for (j = 0; j < 2; ++j) {
			for (k = 0; k < NR; ++k) {
				if ((id + i + j) % 2 == 0)
					m0_atomic64_sub(&atom, id);
				else
					m0_atomic64_add(&atom, id);
			}
		}
		delay.tv_sec = 0;
		delay.tv_nsec = (((id + i) % 4) + 1) * 1000;
		nanosleep(&delay, NULL);
		wait(&bar[i]);
		M0_ASSERT(m0_atomic64_get(&atom) == 0);
	}
}

struct el {
	struct el *next;
	int        datum;
};

static struct el *list;

static void cas_insert(struct el *e)
{
	do
		e->next = list;
	while (!M0_ATOMIC64_CAS(&list, e->next, e));
}

static void cas_delete(void)
{
	struct el *e;

	do
		e = list;
	while (!M0_ATOMIC64_CAS(&list, e, e->next));
}

static void breset(pthread_barrier_t *b, int n)
{
	int result;

	result = pthread_barrier_destroy(b);
	M0_ASSERT(result == 0);
	result = pthread_barrier_init(b, NULL, n);
	M0_ASSERT(result == 0);
}

static void cas(int id)
{
	int i;
	int j;

	struct el e = {
		.next  = NULL,
		.datum = id
	};
	struct el d[NR];

	wait(&bar[0]);
	/* and all together now: non-blocking list insertion. */
	for (i = 0; i < NR; ++i) {
		cas_insert(&e);
		wait(&bar[1]);
		wait(&bar[2]);
	}
	for (i = 0; i < NR; ++i) {
		for (j = 0; j < NR; ++j)
			cas_insert(&d[j]);
		for (j = 0; j < NR; ++j)
			cas_delete();
		wait(&bar[3]);
		wait(&bar[4]);
	}
}
#endif

void test_atomic(void)
{
#ifdef HAVE_PTHREAD_BARRIER_T
	int               i;
	int               j;
	int               result;
	uint64_t          sum;
	uint64_t          sum1;
	bool              zero;
	struct m0_thread  t[NR];
	struct el        *e;

	M0_SET_ARR0(t);
	m0_atomic64_set(&atom, 0);
	sum = 0;
	for (i = 0; i < NR; ++i) {
		m0_atomic64_add(&atom, i);
		sum += i;
		M0_ASSERT(m0_atomic64_get(&atom) == sum);
	}

	for (i = sum; i > 0; --i) {
		zero = m0_atomic64_dec_and_test(&atom);
		M0_ASSERT(zero == (i == 1));
	}

	for (i = 0; i < ARRAY_SIZE(bar); ++i) {
		result = pthread_barrier_init(&bar[i], NULL, NR + 1);
		M0_ASSERT(result == 0);
		result = pthread_barrier_init(&let[i], NULL, NR + 1);
		M0_ASSERT(result == 0);
	}

	m0_atomic64_set(&atom, 0);

	for (i = 0; i < ARRAY_SIZE(t); ++i) {
		result = M0_THREAD_INIT(&t[i], int, NULL, &worker, i, "worker");
		M0_ASSERT(result == 0);
	}

	for (i = 0; i < NR; ++i) {
		wait(&let[i]);
		wait(&bar[i]);
		M0_ASSERT(m0_atomic64_get(&atom) == 0);
	}

	for (i = 0; i < ARRAY_SIZE(t); ++i) {
		m0_thread_join(&t[i]);
		m0_thread_fini(&t[i]);
	}

	/*
	 * m0_atomic64_cas() test.
	 */

	M0_CASSERT(ARRAY_SIZE(bar) > 5);

	/* bar[0] is reached when all cas() threads are created. */
	breset(&bar[0], NR);
	/* bar[1] is reached when every cas() thread insert its element in the
	   global lock-free linked list. */
	breset(&bar[1], NR + 1);
	/* bar[2] is reached when main thread checked that the list is correct
	   after the previous concurrent step and reset the list to NULL. */
	breset(&bar[2], NR + 1);
	/* bar[3] is reached after every cas() step inserted NR elements in the
	   global lock-free list and then removed NR entries from it. */
	breset(&bar[3], NR + 1);
	/* bar[4] is reached after main thread checked that the list is empty
	   after the previous step. */
	breset(&bar[4], NR + 1);

	for (i = 0; i < ARRAY_SIZE(t); ++i) {
		result = M0_THREAD_INIT(&t[i], int, NULL, &cas, i, "cas");
		M0_ASSERT(result == 0);
	}

	for (j = 0; j < NR; ++j) {
		wait(&bar[1]);
		breset(&bar[1], NR + 1);

		/* all threads inserted their identifiers in the list, check. */
		for (i = 0, sum1 = 0, e = list; i < NR; ++i, e = e->next) {
			M0_ASSERT(e != NULL);
			M0_ASSERT(0 <= e->datum && e->datum < NR);
			sum1 += e->datum;
		}
		M0_ASSERT(sum == sum1);
		list = NULL;
		wait(&bar[2]);
		breset(&bar[2], NR + 1);
	}
	for (j = 0; j < NR; ++j) {
		wait(&bar[3]);
		breset(&bar[3], NR + 1);
		M0_ASSERT(list == NULL);
		wait(&bar[4]);
		breset(&bar[4], NR + 1);
	}

	for (i = 0; i < ARRAY_SIZE(bar); ++i) {
		result = pthread_barrier_destroy(&bar[i]);
		M0_ASSERT(result == 0);
		result = pthread_barrier_destroy(&let[i]);
		M0_ASSERT(result == 0);
	}

	for (i = 0; i < ARRAY_SIZE(t); ++i) {
		m0_thread_join(&t[i]);
		m0_thread_fini(&t[i]);
	}
#elif defined(M0_DARWIN)
	/*
	 * XXX @todo no barriers on Darwin, skip the test.
	 */
#else
	M0_IMPOSSIBLE("pthread barriers are not supported!");
#endif

}

enum {
	UB_ITER = 1000
};

static void atomic_ub(int i)
{
	test_atomic();
}

struct m0_ub_set m0_atomic_ub = {
	.us_name = "atomic-ub",
	.us_init = NULL,
	.us_fini = NULL,
	.us_run  = {
		{ .ub_name  = "atomic",
		  .ub_iter  = UB_ITER,
		  .ub_round = atomic_ub },

		{ .ub_name  = NULL }
	}
};

/*
 *  Local variables:
 *  c-indentation-style: "K&R"
 *  c-basic-offset: 8
 *  tab-width: 8
 *  fill-column: 80
 *  scroll-step: 1
 *  End:
 */
