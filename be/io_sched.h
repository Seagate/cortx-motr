/* -*- C -*- */
/*
 * Copyright (c) 2015-2020 Seagate Technology LLC and/or its Affiliates
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

#ifndef __MOTR_BE_IO_SCHED_H__
#define __MOTR_BE_IO_SCHED_H__

/**
 * @defgroup be
 *
 * @{
 */

#include "lib/types.h"          /* bool */
#include "lib/tlist.h"          /* m0_tl */
#include "lib/mutex.h"          /* m0_mutex */

struct m0_be_op;
struct m0_be_io;
struct m0_ext;

struct m0_be_io_sched_cfg {
	/** start position for m0_be_io_sched::bis_pos */
	m0_bcount_t bisc_pos_start;
};

/*
 * IO scheduler.
 *
 * It launches m0_be_io in m0_ext-based ordered queue.
 *
 * Highlighs:
 * - write I/O:
 *   - each one should have m0_ext;
 *   - m0_ext for one I/O should not intersect with m0_ext for another I/O;
 *   - I/Os are launched in the m0_ext increasing order, without gaps. If there
 *     is no such I/O in the queue at the scheduler's current position then I/O
 *     after the gap is not launched until another I/O is added to fill the gap;
 * - read I/O:
 *   - doesn't have m0_ext assigned (subject to change);
 *   - is launched after the last write I/O (at the time the read I/O is added
 *     to the scheduler's queue) from the queue is finished.
 */
struct m0_be_io_sched {
	struct m0_be_io_sched_cfg bis_cfg;
	/** list of m0_be_io-s under scheduler's control */
	struct m0_tl              bis_ios;
	struct m0_mutex           bis_lock;
	bool                      bis_io_in_progress;
	/** position for the next I/O */
	m0_bcount_t               bis_pos;
};

M0_INTERNAL int m0_be_io_sched_init(struct m0_be_io_sched     *sched,
				    struct m0_be_io_sched_cfg *cfg);
M0_INTERNAL void m0_be_io_sched_fini(struct m0_be_io_sched *sched);
M0_INTERNAL void m0_be_io_sched_lock(struct m0_be_io_sched *sched);
M0_INTERNAL void m0_be_io_sched_unlock(struct m0_be_io_sched *sched);
M0_INTERNAL bool m0_be_io_sched_is_locked(struct m0_be_io_sched *sched);
M0_INTERNAL void m0_be_io_sched_add(struct m0_be_io_sched *sched,
                                    struct m0_be_io       *io,
                                    struct m0_ext         *ext,
                                    struct m0_be_op       *op);

/** @} end of be group */
#endif /* __MOTR_BE_IO_SCHED_H__ */

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
