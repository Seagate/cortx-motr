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


#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#include "lib/assert.h"
#include "motr/magic.h"
#include "desim/elevator.h"

/**
   @addtogroup desim desim
   @{
 */

/*
 * Simple FIFO elevator.
 */

struct io_req {
	struct m0_tlink ir_linkage;
	uint64_t        ir_magic;
};

M0_TL_DESCR_DEFINE(req, "io requests", static, struct io_req,
		   ir_linkage, ir_magic, M0_DESIM_IO_REQ_MAGIC,
		   M0_DESIM_IO_REQ_HEAD_MAGIC);
M0_TL_DEFINE(req, static, struct io_req);

static void elevator_submit(struct elevator *el,
			    enum storage_req_type type,
			    sector_t sector, unsigned long count)
{
	el->e_idle = 0;
	el->e_dev->sd_submit(el->e_dev, type, sector, count);
}

/*
 * submit asynchronous requests.
 */
static void elevator_go(struct elevator *el)
{
	/* impossible for now */
	M0_IMPOSSIBLE("Elevator is not yet implemented");
}

M0_INTERNAL void el_end_io(struct storage_dev *dev)
{
	struct elevator *el;

	el = dev->sd_el;
	el->e_idle = 1;
	if (!req_tlist_is_empty(&el->e_queue))
		elevator_go(el);
	sim_chan_broadcast(&el->e_wait);
}

M0_INTERNAL void elevator_init(struct elevator *el, struct storage_dev *dev)
{
	el->e_dev  = dev;
	el->e_idle = 1;
	dev->sd_end_io = el_end_io;
	dev->sd_el     = el;
	req_tlist_init(&el->e_queue);
	sim_chan_init(&el->e_wait, "xfer-queue@%s", dev->sd_name);
}

M0_INTERNAL void elevator_fini(struct elevator *el)
{
	req_tlist_fini(&el->e_queue);
	sim_chan_fini(&el->e_wait);
}

M0_INTERNAL void elevator_io(struct elevator *el, enum storage_req_type type,
			     sector_t sector, unsigned long count)
{
	while (!el->e_idle)
		sim_chan_wait(&el->e_wait, sim_thread_current());
	elevator_submit(el, type, sector, count);
}

/** @} end of desim group */

/*
 *  Local variables:
 *  c-indentation-style: "K&R"
 *  c-basic-offset: 8
 *  tab-width: 8
 *  fill-column: 80
 *  scroll-step: 1
 *  End:
 */
