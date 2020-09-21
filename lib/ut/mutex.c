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


#include "ut/ut.h"
#include "lib/thread.h"
#include "lib/mutex.h"
#include "lib/assert.h"

enum {
	NR = 255
};

static int counter;
static struct m0_thread t[NR];
static struct m0_mutex  m[NR];
static struct m0_mutex  static_m = M0_MUTEX_SINIT(&static_m);

static void t0(int n)
{
	int i;

	for (i = 0; i < NR; ++i) {
		m0_mutex_lock(&m[0]);
		counter += n;
		m0_mutex_unlock(&m[0]);
	}
}

static void t1(int n)
{
	int i;
	int j;

	for (i = 0; i < NR; ++i) {
		for (j = 0; j < NR; ++j)
			m0_mutex_lock(&m[j]);
		counter += n;
		for (j = 0; j < NR; ++j)
			m0_mutex_unlock(&m[j]);
	}
}

static void static_mutex_test(void)
{
	m0_mutex_lock(&static_m);
	m0_mutex_unlock(&static_m);
}

void test_mutex(void)
{
	int i;
	int sum;
	int result;

	counter = 0;

	for (sum = i = 0; i < NR; ++i) {
		m0_mutex_init(&m[i]);
		result = M0_THREAD_INIT(&t[i], int, NULL, &t0, i, "t0");
		M0_UT_ASSERT(result == 0);
		sum += i;
	}

	for (i = 0; i < NR; ++i) {
		m0_thread_join(&t[i]);
		m0_thread_fini(&t[i]);
	}

	M0_UT_ASSERT(counter == sum * NR);

	counter = 0;

	for (sum = i = 0; i < NR; ++i) {
		result = M0_THREAD_INIT(&t[i], int, NULL, &t1, i, "t1");
		M0_UT_ASSERT(result == 0);
		sum += i;
	}

	for (i = 0; i < NR; ++i) {
		m0_thread_join(&t[i]);
		m0_thread_fini(&t[i]);
	}

	for (i = 0; i < NR; ++i)
		m0_mutex_fini(&m[i]);

	M0_UT_ASSERT(counter == sum * NR);

	static_mutex_test();
}


/*
 *  Local variables:
 *  c-indentation-style: "K&R"
 *  c-basic-offset: 8
 *  tab-width: 8
 *  fill-column: 80
 *  scroll-step: 1
 *  End:
 */
