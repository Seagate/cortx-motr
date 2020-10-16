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

#if M0_DARWIN

#include <unistd.h>            /* lseek */

#define M0_TRACE_SUBSYSTEM M0_TRACE_SUBSYS_STOB
#include "lib/trace.h"
#include "lib/mutex.h"
#include "lib/memory.h"
#include "lib/errno.h"
#include "stob/aio_emu.h"

typedef struct io_context {
	struct m0_mutex ic_lock;
	struct m0_tl    ic_cb;
} *io_context_t;

M0_TL_DESCR_DEFINE(i, "iocb",
		   static, struct iocb, aio_linkage, aio_magic,
		   M0_AIO_EMU_MAGIC, M0_AIO_EMU_HEAD_MAGIC);
M0_TL_DEFINE(i, static, struct iocb);

static long handle(struct io_context *ctx, const struct iocb *io);

M0_INTERNAL int io_setup(int maxevents, io_context_t *out)
{
	struct io_context *ctx;

	M0_ALLOC_PTR(ctx);
	if (ctx == NULL)
		return M0_ERR(-ENOMEM);
	i_tlist_init(&ctx->ic_cb);
	m0_mutex_init(&ctx->ic_lock);
	*out = ctx;
	return M0_RC(0);
}

M0_INTERNAL int io_destroy(io_context_t ctx)
{
	i_tlist_fini(&ctx->ic_cb);
	return 0;
}

M0_INTERNAL int io_submit(io_context_t ctx, long nr, struct iocb *ios[])
{
	int i;

	m0_mutex_lock(&ctx->ic_lock);
	for (i = 0; i < nr; ++i)
		i_tlink_init_at(ios[i], &ctx->ic_cb);
	m0_mutex_unlock(&ctx->ic_lock);
	return nr;
}

M0_INTERNAL int io_getevents(io_context_t ctx, long min_nr, long nr,
			     struct io_event *events, struct timespec *timeout)
{
	int          i = 0;
	struct iocb *io;

	m0_mutex_lock(&ctx->ic_lock);
	if (i_tlist_length(&ctx->ic_cb) < min_nr) {
		m0_mutex_unlock(&ctx->ic_lock);
		nanosleep(timeout, NULL);
	} else {
		for (; i < nr; ++i) {
			io = i_tlist_pop(&ctx->ic_cb);
			if (io == NULL)
				break;
			events[i].obj  = io;
			events[i].data = (void *)io->aio_data;
			events[i].res  = handle(ctx, io);
			events[i].res2 = 0;
		}
		m0_mutex_unlock(&ctx->ic_lock);
	}
	return i;
}

static long handle(struct io_context *ctx, const struct iocb *io)
{
	long result;
	int  fd = io->aio_fildes;

	result = lseek(fd, io->u.v.offset, SEEK_SET);
	if (result >= 0) {
		switch (io->aio_lio_opcode) {
		case IO_CMD_PREADV:
			result = readv(fd, io->u.v.vec, io->u.v.nr);
			break;
		case IO_CMD_PWRITEV:
			result = writev(fd, io->u.v.vec, io->u.v.nr);
			break;
		default:
			M0_IMPOSSIBLE("Wrong opcode.");
		}
	}
	if (result == -1)
		result = M0_ERR(-errno);
	return result;
}

#undef M0_TRACE_SUBSYSTEM

/* M0_DARWIN */
#endif

/*
 *  Local variables:
 *  c-indentation-style: "K&R"
 *  c-basic-offset: 8
 *  tab-width: 8
 *  fill-column: 80
 *  scroll-step: 1
 *  End:
 */
