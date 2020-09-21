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


#include "lib/queue.h"
#include "lib/assert.h"
#include "lib/misc.h"    /* NULL */

/**
   @addtogroup queue Queue

   When a queue is not empty, last element's m0_queue_link::ql_next is set to
   "end-of-queue" marker (EOQ). This guarantees that an element is in a queue
   iff ql_next is not NULL (see m0_queue_link_is_in()).

   When a queue is empty, its head and tail are set to EOQ. This allows
   iteration over queue elements via loop of the form

   @code
   for (scan = q->q_head; scan != EOQ; scan = scan->ql_next) { ... }
   @endcode

   independently of whether the queue is empty.

   @{
 */

#define EOQ ((struct m0_queue_link *)8)

const struct m0_queue M0_QUEUE_INIT = {
	.q_head = EOQ,
	.q_tail = EOQ
};

M0_INTERNAL void m0_queue_init(struct m0_queue *q)
{
	q->q_head = q->q_tail = EOQ;
	M0_ASSERT(m0_queue_invariant(q));
}

M0_INTERNAL void m0_queue_fini(struct m0_queue *q)
{
	M0_ASSERT(m0_queue_invariant(q));
	M0_ASSERT(m0_queue_is_empty(q));
}

M0_INTERNAL bool m0_queue_is_empty(const struct m0_queue *q)
{
	M0_ASSERT_EX(m0_queue_invariant(q));
	return q->q_head == EOQ;
}

M0_INTERNAL void m0_queue_link_init(struct m0_queue_link *ql)
{
	ql->ql_next = NULL;
}

M0_INTERNAL void m0_queue_link_fini(struct m0_queue_link *ql)
{
	M0_ASSERT(!m0_queue_link_is_in(ql));
}

M0_INTERNAL bool m0_queue_link_is_in(const struct m0_queue_link *ql)
{
	return ql->ql_next != NULL;
}

M0_INTERNAL bool m0_queue_contains(const struct m0_queue *q,
				   const struct m0_queue_link *ql)
{
	struct m0_queue_link *scan;

	M0_ASSERT(m0_queue_invariant(q));
	for (scan = q->q_head; scan != EOQ; scan = scan->ql_next) {
		M0_ASSERT(scan != NULL);
		if (scan == ql)
			return true;
	}
	return false;
}

M0_INTERNAL size_t m0_queue_length(const struct m0_queue *q)
{
	size_t length;
	struct m0_queue_link *scan;

	M0_ASSERT_EX(m0_queue_invariant(q));

	for (length = 0, scan = q->q_head; scan != EOQ; scan = scan->ql_next)
		++length;
	return length;
}

M0_INTERNAL struct m0_queue_link *m0_queue_get(struct m0_queue *q)
{
	struct m0_queue_link *head;

	/* invariant is checked on entry to m0_queue_is_empty() */
	if (m0_queue_is_empty(q))
		head = NULL;
	else {
		head = q->q_head;
		q->q_head = head->ql_next;
		if (q->q_head == EOQ)
			q->q_tail = EOQ;
		head->ql_next = NULL;
	}
	M0_ASSERT_EX(m0_queue_invariant(q));
	return head;

}

M0_INTERNAL void m0_queue_put(struct m0_queue *q, struct m0_queue_link *ql)
{
	/* invariant is checked on entry to m0_queue_is_empty() */
	if (m0_queue_is_empty(q))
		q->q_head = ql;
	else
		q->q_tail->ql_next = ql;
	q->q_tail = ql;
	ql->ql_next = EOQ;
	M0_ASSERT_EX(m0_queue_invariant(q));
}

M0_INTERNAL bool m0_queue_invariant(const struct m0_queue *q)
{
	struct m0_queue_link *scan;

	if ((q->q_head == EOQ) != (q->q_tail == EOQ))
		return false;
	if (q->q_head == NULL || q->q_tail == NULL)
		return false;

	for (scan = q->q_head; scan != EOQ; scan = scan->ql_next) {
		if (scan == NULL || scan == EOQ)
			return false;
	}
	if (q->q_head != EOQ && q->q_tail->ql_next != EOQ)
		return false;
	return true;
}

/** @} end of queue group */

/*
 *  Local variables:
 *  c-indentation-style: "K&R"
 *  c-basic-offset: 8
 *  tab-width: 8
 *  fill-column: 80
 *  scroll-step: 1
 *  End:
 */
