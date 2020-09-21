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
#include "lib/queue.h"
#include "lib/assert.h"

struct qt {
	struct m0_queue_link t_linkage;
	int                  t_val;
};

enum {
	NR = 255
};

void test_queue(void)
{
	int i;
	int sum0;
	int sum1;

	struct m0_queue  q;
	static struct qt t[NR]; /* static to reduce kernel stack consumption. */

	for (sum0 = i = 0; i < ARRAY_SIZE(t); ++i) {
		t[i].t_val = i;
		sum0 += i;
	};

	m0_queue_init(&q);
	M0_UT_ASSERT(m0_queue_is_empty(&q));
	M0_UT_ASSERT(m0_queue_get(&q) == NULL);
	M0_UT_ASSERT(m0_queue_length(&q) == 0);

	for (i = 0; i < ARRAY_SIZE(t); ++i) {
		m0_queue_put(&q, &t[i].t_linkage);
		M0_UT_ASSERT(!m0_queue_is_empty(&q));
		M0_UT_ASSERT(m0_queue_link_is_in(&t[i].t_linkage));
		M0_UT_ASSERT(m0_queue_length(&q) == i + 1);
	}
	M0_UT_ASSERT(m0_queue_length(&q) == ARRAY_SIZE(t));

	for (i = 0; i < ARRAY_SIZE(t); ++i)
		M0_UT_ASSERT(m0_queue_contains(&q, &t[i].t_linkage));

	for (sum1 = i = 0; i < ARRAY_SIZE(t); ++i) {
		struct m0_queue_link *ql;
		struct qt            *qt;

		ql = m0_queue_get(&q);
		M0_UT_ASSERT(ql != NULL);
		qt = container_of(ql, struct qt, t_linkage);
		M0_UT_ASSERT(&t[0] <= qt && qt < &t[NR]);
		M0_UT_ASSERT(qt->t_val == i);
		sum1 += qt->t_val;
	}
	M0_UT_ASSERT(sum0 == sum1);
	M0_UT_ASSERT(m0_queue_get(&q) == NULL);
	M0_UT_ASSERT(m0_queue_is_empty(&q));
	M0_UT_ASSERT(m0_queue_length(&q) == 0);
	for (i = 0; i < ARRAY_SIZE(t); ++i)
		M0_UT_ASSERT(!m0_queue_link_is_in(&t[i].t_linkage));

	m0_queue_fini(&q);
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
