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


#include "ut/ut.h"		/* M0_UT_ASSERT */

#include "net/test/ringbuf.h"

enum {
	NET_TEST_RB_SIZE    = 0x1000,
	NET_TEST_RB_LOOP_NR = 0x10,
};

static void ringbuf_push_pop(struct m0_net_test_ringbuf *rb, size_t nr)
{
	size_t i;
	size_t value;
	size_t len;

	M0_PRE(rb != NULL);

	for (i = 0; i < nr; ++i) {
		m0_net_test_ringbuf_push(rb, i);
		len = m0_net_test_ringbuf_nr(rb);
		M0_UT_ASSERT(len == i + 1);
	}
	for (i = 0; i < nr; ++i) {
		value = m0_net_test_ringbuf_pop(rb);
		M0_UT_ASSERT(value == i);
		len = m0_net_test_ringbuf_nr(rb);
		M0_UT_ASSERT(len == nr - i - 1);
	}
}

void m0_net_test_ringbuf_ut(void)
{
	struct m0_net_test_ringbuf rb;
	int			   rc;
	int			   i;
	size_t			   value;
	size_t			   nr;

	/* init */
	rc = m0_net_test_ringbuf_init(&rb, NET_TEST_RB_SIZE);
	M0_UT_ASSERT(rc == 0);
	nr = m0_net_test_ringbuf_nr(&rb);
	M0_UT_ASSERT(nr == 0);
	/* test #1: single value push, single value pop */
	m0_net_test_ringbuf_push(&rb, 42);
	nr = m0_net_test_ringbuf_nr(&rb);
	M0_UT_ASSERT(nr == 1);
	value = m0_net_test_ringbuf_pop(&rb);
	M0_UT_ASSERT(value == 42);
	nr = m0_net_test_ringbuf_nr(&rb);
	M0_UT_ASSERT(nr == 0);
	/* test #2: multiple values push, multiple values pop */
	ringbuf_push_pop(&rb, NET_TEST_RB_SIZE);
	/*
	 * test #3: push and pop (NET_TEST_RB_SIZE - 1) items
	 * NET_TEST_RB_LOOP_NR times
	 */
	for (i = 0; i < NET_TEST_RB_LOOP_NR; ++i)
		ringbuf_push_pop(&rb, NET_TEST_RB_SIZE - 1);
	/* fini */
	m0_net_test_ringbuf_fini(&rb);
}

/*
 *  Local variables:
 *  c-indentation-style: "K&R"
 *  c-basic-offset: 8
 *  tab-width: 8
 *  fill-column: 79
 *  scroll-step: 1
 *  End:
 */
