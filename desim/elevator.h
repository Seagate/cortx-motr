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

#ifndef __MOTR_DESIM_ELEVATOR_H__
#define __MOTR_DESIM_ELEVATOR_H__

#include "desim/sim.h"
#include "desim/storage.h"
#include "lib/tlist.h"

/**
   @addtogroup desim desim
   @{
 */

struct elevator {
	struct storage_dev *e_dev;
	int                 e_idle;
	struct m0_tl        e_queue;
	struct sim_chan     e_wait;
};

M0_INTERNAL void elevator_init(struct elevator *el, struct storage_dev *dev);
M0_INTERNAL void elevator_fini(struct elevator *el);

M0_INTERNAL void elevator_io(struct elevator *el, enum storage_req_type type,
			     sector_t sector, unsigned long count);

#endif /* __MOTR_DESIM_ELEVATOR_H__ */

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
