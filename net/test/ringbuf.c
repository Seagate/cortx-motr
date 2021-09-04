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


#include "lib/errno.h"		/* ENOMEM */
#include "lib/misc.h"		/* M0_SET0 */
#include "lib/memory.h"		/* M0_ALLOC_ARR */

#include "net/test/ringbuf.h"

/**
   @defgroup NetTestRingbufInternals Ringbuf
   @ingroup NetTestInternals

   @{
 */

int m0_net_test_ringbuf_init(struct m0_net_test_ringbuf *rb, size_t size)
{
	M0_PRE(rb != NULL);
	M0_PRE(size != 0);

	rb->ntr_size = size;
	m0_atomic64_set(&rb->ntr_start, 0);
	m0_atomic64_set(&rb->ntr_end, 0);
	M0_ALLOC_ARR(rb->ntr_buf, rb->ntr_size);

	if (rb->ntr_buf != NULL)
		M0_ASSERT(m0_net_test_ringbuf_invariant(rb));

	return rb->ntr_buf == NULL ? -ENOMEM : 0;
}

void m0_net_test_ringbuf_fini(struct m0_net_test_ringbuf *rb)
{
	M0_PRE(m0_net_test_ringbuf_invariant(rb));

	m0_free(rb->ntr_buf);
	M0_SET0(rb);
}

bool m0_net_test_ringbuf_invariant(const struct m0_net_test_ringbuf *rb)
{
	int64_t start = m0_atomic64_get(&rb->ntr_start);
	int64_t end   = m0_atomic64_get(&rb->ntr_end);

	return  _0C(rb != NULL) && _0C(rb->ntr_buf != NULL) &&
		_0C(start <= end) && _0C(end - start <= rb->ntr_size);
}

void m0_net_test_ringbuf_push(struct m0_net_test_ringbuf *rb, size_t value)
{
	int64_t index;

	M0_PRE(m0_net_test_ringbuf_invariant(rb));
	index = m0_atomic64_add_return(&rb->ntr_end, 1) - 1;
	M0_ASSERT(m0_net_test_ringbuf_invariant(rb));

	rb->ntr_buf[index % rb->ntr_size] = value;
}

size_t m0_net_test_ringbuf_pop(struct m0_net_test_ringbuf *rb)
{
	int64_t index;

	M0_PRE(m0_net_test_ringbuf_invariant(rb));
	index = m0_atomic64_add_return(&rb->ntr_start, 1) - 1;
	M0_ASSERT(m0_net_test_ringbuf_invariant(rb));

	return rb->ntr_buf[index % rb->ntr_size];
}

bool m0_net_test_ringbuf_is_empty(struct m0_net_test_ringbuf *rb)
{
	M0_PRE(m0_net_test_ringbuf_invariant(rb));

	return m0_atomic64_get(&rb->ntr_end) == m0_atomic64_get(&rb->ntr_start);
}

size_t m0_net_test_ringbuf_nr(struct m0_net_test_ringbuf *rb)
{
	M0_PRE(m0_net_test_ringbuf_invariant(rb));

	return m0_atomic64_get(&rb->ntr_end) - m0_atomic64_get(&rb->ntr_start);
}

/**
   @} end of NetTestRingbufInternals group
 */

/*
 *  Local variables:
 *  c-indentation-style: "K&R"
 *  c-basic-offset: 8
 *  tab-width: 8
 *  fill-column: 79
 *  scroll-step: 1
 *  End:
 */
