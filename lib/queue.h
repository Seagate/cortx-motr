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

#ifndef __MOTR_LIB_QUEUE_H__
#define __MOTR_LIB_QUEUE_H__

#include "lib/types.h"

/**
   @defgroup queue Queue

   FIFO queue. Should be pretty self-explanatory.

   @{
 */

struct m0_queue_link;

/**
   A queue of elements.
 */
struct m0_queue {
	/** Oldest element in the queue (first to be returned). */
	struct m0_queue_link *q_head;
	/** Youngest (last added) element in the queue. */
	struct m0_queue_link *q_tail;
};

/**
   An element in a queue.
 */
struct m0_queue_link {
	struct m0_queue_link *ql_next;
};

/**
   Static queue initializer. Assign this to a variable of type struct m0_queue
   to initialize empty queue.
 */
extern const struct m0_queue M0_QUEUE_INIT;

M0_INTERNAL void m0_queue_init(struct m0_queue *q);
M0_INTERNAL void m0_queue_fini(struct m0_queue *q);
M0_INTERNAL bool m0_queue_is_empty(const struct m0_queue *q);

M0_INTERNAL void m0_queue_link_init(struct m0_queue_link *ql);
M0_INTERNAL void m0_queue_link_fini(struct m0_queue_link *ql);
M0_INTERNAL bool m0_queue_link_is_in(const struct m0_queue_link *ql);
M0_INTERNAL bool m0_queue_contains(const struct m0_queue *q,
				   const struct m0_queue_link *ql);
M0_INTERNAL size_t m0_queue_length(const struct m0_queue *q);

/**
   Returns queue head or NULL if queue is empty.
 */
M0_INTERNAL struct m0_queue_link *m0_queue_get(struct m0_queue *q);
M0_INTERNAL void m0_queue_put(struct m0_queue *q, struct m0_queue_link *ql);

M0_INTERNAL bool m0_queue_invariant(const struct m0_queue *q);

/** @} end of queue group */
#endif /* __MOTR_LIB_QUEUE_H__ */

/*
 *  Local variables:
 *  c-indentation-style: "K&R"
 *  c-basic-offset: 8
 *  tab-width: 8
 *  fill-column: 80
 *  scroll-step: 1
 *  End:
 */
