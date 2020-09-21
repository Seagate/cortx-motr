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

#ifndef __MOTR_DESIM_STORAGE_H__
#define __MOTR_DESIM_STORAGE_H__

#include "desim/sim.h"

/**
   @addtogroup desim desim
   @{
 */

typedef unsigned long long sector_t;

struct storage_dev;

struct storage_conf {
	unsigned   sc_sector_size;
};

enum storage_req_type {
	SRT_READ,
	SRT_WRITE
};

typedef void (*storage_end_io_t)(struct storage_dev *dev);
typedef void (*storage_submit_t)(struct storage_dev *dev,
				 enum storage_req_type type,
				 sector_t sector, unsigned long count);

struct elevator;
struct storage_dev {
	struct sim          *sd_sim;
	struct storage_conf *sd_conf;
	storage_end_io_t     sd_end_io;
	storage_submit_t     sd_submit;
	struct elevator     *sd_el;
	char                *sd_name;
};

#endif /* __MOTR_DESIM_STORAGE_H__ */

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
