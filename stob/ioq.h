/* -*- C -*- */
/*
 * Copyright (c) 2014-2020 Seagate Technology LLC and/or its Affiliates
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

#ifndef __MOTR_STOB_IOQ_H__
#define __MOTR_STOB_IOQ_H__
#ifndef __KERNEL__
#include <libaio.h>        /* io_context_t */
#endif
#include "lib/types.h"     /* bool */
#include "lib/atomic.h"    /* m0_atomic64 */
#include "lib/thread.h"    /* m0_thread */
#include "lib/mutex.h"     /* m0_mutex */
#include "lib/queue.h"     /* m0_queue */
#include "lib/timer.h"     /* m0_timer */
#include "lib/semaphore.h" /* m0_semaphore */

/**
 * @defgroup stoblinux
 *
 * @{
 */

struct m0_stob;
struct m0_stob_io;

enum {
	/** Default number of threads to create in a storage object domain. */
	M0_STOB_IOQ_NR_THREADS     = 8,
	/** Default size of a ring buffer shared by adieu and the kernel. */
	M0_STOB_IOQ_RING_SIZE      = 1024,
	/** Size of a batch in which requests are moved from the admission queue
	    to the ring buffer. */
	M0_STOB_IOQ_BATCH_IN_SIZE  = 8,
	/** Size of a batch in which completion events are extracted from the
	    ring buffer. */
	M0_STOB_IOQ_BATCH_OUT_SIZE = 8,
};

struct m0_stob_ioq {
	/**
	 *  Controls whether to use O_DIRECT flag for open(2).
	 *  Can be set with m0_stob_ioq_directio_setup().
	 *  Initial value is set to 'false'.
	 */
	bool                     ioq_use_directio;
	/** Set up when domain is being shut down. adieu worker threads
	    (ioq_thread()) check this field on each iteration. */
	/**
	    Ring buffer shared between adieu and the kernel.

	    It contains adieu request fragments currently being executed by the
	    kernel. The kernel delivers AIO completion events through this
	    buffer. */
#ifndef __KERNEL__
	io_context_t             ioq_ctx;
#endif
	/** Free slots in the ring buffer. */
	struct m0_atomic64       ioq_avail;
	/** Used slots in the ring buffer. */
	int                      ioq_queued;
	/** Worker threads. */
	struct m0_thread         ioq_thread[M0_STOB_IOQ_NR_THREADS];

	/** Mutex protecting all ioq_ fields (except for the ring buffer that is
	    updated by the kernel asynchronously). */
	struct m0_mutex          ioq_lock;
	/** Admission queue where adieu request fragments are kept until there
	    is free space in the ring buffer.  */
	struct m0_queue          ioq_queue;
	struct m0_semaphore      ioq_stop_sem[M0_STOB_IOQ_NR_THREADS];
	struct m0_timer          ioq_stop_timer[M0_STOB_IOQ_NR_THREADS];
#ifdef __KERNEL__
	struct m0_timer_locality ioq_stop_timer_loc[M0_STOB_IOQ_NR_THREADS];
#endif
};
#ifndef __KERNEL__
M0_INTERNAL int m0_stob_ioq_init(struct m0_stob_ioq *ioq);
M0_INTERNAL void m0_stob_ioq_fini(struct m0_stob_ioq *ioq);
#endif
M0_INTERNAL void m0_stob_ioq_directio_setup(struct m0_stob_ioq *ioq,
					    bool use_directio);

M0_INTERNAL bool m0_stob_ioq_directio(struct m0_stob_ioq *ioq);
M0_INTERNAL uint32_t m0_stob_ioq_bshift(struct m0_stob_ioq *ioq);
M0_INTERNAL m0_bcount_t m0_stob_ioq_bsize(struct m0_stob_ioq *ioq);
M0_INTERNAL m0_bcount_t m0_stob_ioq_bmask(struct m0_stob_ioq *ioq);

M0_INTERNAL int m0_stob_linux_io_init(struct m0_stob *stob,
				      struct m0_stob_io *io);

/** @} end of stoblinux group */
#endif /* __MOTR_STOB_IOQ_H__ */

/*
 *  Local variables:
 *  c-indentation-style: "K&R"
 *  c-basic-offset: 8
 *  tab-width: 8
 *  fill-column: 80
 *  scroll-step: 1
 *  End:
 */
/*
 * vim: tabstop=8 shiftwidth=8 noexpandtab textwidth=80 nowrap
 */
