/* -*- C -*- */
/*
 * Copyright (c) 2020 Seagate Technology LLC and/or its Affiliates
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

#ifndef __MOTR_STOB_AIO_EMU_H__
#define __MOTR_STOB_AIO_EMU_H__

#include "lib/tlist.h"

struct io_contextж;
typedef struct io_context *io_context_t;

typedef enum io_iocb_cmd {
	IO_CMD_PREADV,
	IO_CMD_PWRITEV
} io_iocb_cmd_t;

struct iocb {
	uint64_t aio_magic;
	uint64_t aio_data;
	uint64_t aio_lio_opcode;
	uint32_t aio_fildes;
	union {
		struct {
			struct iovec *vec;
			int           nr;
			long long     offset;
		} v;
	} u;
	struct m0_tlink aio_linkage;
};

struct io_event {
	void         *data;
	struct iocb  *obj;
	unsigned long res;
	unsigned long res2;
};

struct timespec;

M0_INTERNAL int io_setup(int maxevents, io_context_t *ctxp);
M0_INTERNAL int io_destroy(io_context_t ctx);
M0_INTERNAL int io_submit(io_context_t ctx, long nr, struct iocb *ios[]);
M0_INTERNAL int io_getevents(io_context_t ctx_id, long min_nr, long nr,
			     struct io_event *events, struct timespec *timeout);

/** @} end of stoblinux group */
#endif /* __MOTR_STOB_AIO_EMU_H__ */

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
