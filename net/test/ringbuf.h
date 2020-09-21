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

#ifndef __MOTR_NET_TEST_RINGBUF_H__
#define __MOTR_NET_TEST_RINGBUF_H__

#include "lib/types.h"	/* size_t */
#include "lib/atomic.h"	/* m0_atomic64 */

/**
   @defgroup NetTestRingbufDFS Ringbuf
   @ingroup NetTestDFS

   @{
 */

/**
   Circular FIFO buffer with size_t elements.
   @note m0_net_test_ringbuf.ntr_start and m0_net_test_ringbuf.ntr_end are
   absolute indices.
 */
struct m0_net_test_ringbuf {
	size_t		    ntr_size;	/**< Maximum number of elements */
	size_t		   *ntr_buf;	/**< Ringbuf array */
	struct m0_atomic64  ntr_start;	/**< Start pointer */
	struct m0_atomic64  ntr_end;	/**< End pointer */
};

/**
   Initialize ring buffer.
   @param rb ring buffer
   @param size maximum number of elements.
 */
int m0_net_test_ringbuf_init(struct m0_net_test_ringbuf *rb, size_t size);

/**
   Finalize ring buffer.
   @pre m0_net_test_ringbuf_invariant(rb)
 */
void m0_net_test_ringbuf_fini(struct m0_net_test_ringbuf *rb);
/** Ring buffer invariant. */
bool m0_net_test_ringbuf_invariant(const struct m0_net_test_ringbuf *rb);

/**
   Push item to the ring buffer.
   @pre m0_net_test_ringbuf_invariant(rb)
   @post m0_net_test_ringbuf_invariant(rb)
 */
void m0_net_test_ringbuf_push(struct m0_net_test_ringbuf *rb, size_t value);

/**
   Pop item from the ring buffer.
   @pre m0_net_test_ringbuf_invariant(rb)
   @post m0_net_test_ringbuf_invariant(rb)
 */
size_t m0_net_test_ringbuf_pop(struct m0_net_test_ringbuf *rb);

/**
   Is ring buffer empty?
   Useful with MPSC/SPSC access pattern.
   @pre m0_net_test_ringbuf_invariant(rb)
 */
bool m0_net_test_ringbuf_is_empty(struct m0_net_test_ringbuf *rb);

/**
   Get current number of elements in the ring buffer.
   @note This function is not thread-safe.
   @pre m0_net_test_ringbuf_invariant(rb)
 */
size_t m0_net_test_ringbuf_nr(struct m0_net_test_ringbuf *rb);

/**
   @} end of NetTestRingbufDFS group
 */

#endif /*  __MOTR_NET_TEST_RINGBUF_H__ */

/*
 *  Local variables:
 *  c-indentation-style: "K&R"
 *  c-basic-offset: 8
 *  tab-width: 8
 *  fill-column: 79
 *  scroll-step: 1
 *  End:
 */
