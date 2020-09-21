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

#ifndef __MOTR_BE_PD_H__
#define __MOTR_BE_PD_H__

#include "lib/tlist.h"          /* m0_tl */
#include "lib/types.h"          /* uint32_t */

#include "be/io_sched.h"        /* m0_be_io_sched */
#include "be/io.h"              /* m0_be_io_credit */
#include "be/pool.h"            /* m0_be_pool */


/**
 * @defgroup be
 *
 * @{
 */

struct m0_ext;
struct m0_be_io;
struct m0_be_op;
struct m0_be_pd_io;

enum m0_be_pd_io_state {
	/* io is ready to be taken using m0_be_pd_io_get() */
	M0_BPD_IO_IDLE,
	/* io is owned by m0_be_pd::bpd_sched */
	M0_BPD_IO_IN_PROGRESS,
	/* m0_be_pd::bpd_sched had reported about io completion. */
	M0_BPD_IO_DONE,
	M0_BPD_IO_STATE_NR,
};

struct m0_be_pd_cfg {
	struct m0_be_io_sched_cfg bpdc_sched;
	uint32_t                  bpdc_seg_io_nr;
	uint32_t                  bpdc_seg_io_pending_max;
	struct m0_be_io_credit    bpdc_io_credit;
};

struct m0_be_pd {
	struct m0_be_pd_cfg    bpd_cfg;
	struct m0_be_io_sched  bpd_sched;
	struct m0_be_pd_io    *bpd_io;
	struct m0_be_pool      bpd_io_pool;

	struct m0_be_op       *bpd_sync_op;
	m0_time_t              bpd_sync_delay;
	m0_time_t              bpd_sync_runtime;
	m0_time_t              bpd_sync_prev;
	struct m0_be_io        bpd_sync_io;
	bool                   bpd_sync_in_progress;
	char                   bpd_sync_read_to[2];
	struct m0_sm_ast       bpd_sync_ast;
};

M0_INTERNAL int m0_be_pd_init(struct m0_be_pd *pd, struct m0_be_pd_cfg *pd_cfg);
M0_INTERNAL void m0_be_pd_fini(struct m0_be_pd *pd);

M0_INTERNAL void m0_be_pd_io_add(struct m0_be_pd    *pd,
                                 struct m0_be_pd_io *pdio,
                                 struct m0_ext      *ext,
                                 struct m0_be_op    *op);

M0_INTERNAL void m0_be_pd_io_get(struct m0_be_pd     *pd,
				 struct m0_be_pd_io **pdio,
				 struct m0_be_op     *op);
M0_INTERNAL void m0_be_pd_io_put(struct m0_be_pd    *pd,
				 struct m0_be_pd_io *pdio);

M0_INTERNAL struct m0_be_io *m0_be_pd_io_be_io(struct m0_be_pd_io *pdio);

/**
 * Run fdatasync() for the given set of stobs.
 *
 * @note pos parameter is ignored for now.
 */
M0_INTERNAL void m0_be_pd_sync(struct m0_be_pd  *pd,
                               m0_bindex_t       pos,
                               struct m0_stob  **stobs,
                               int               nr,
                               struct m0_be_op  *op);

/** @} end of be group */
#endif /* __MOTR_BE_PD_H__ */

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
