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

#ifndef __MOTR_DESIM_CHS_H__
#define __MOTR_DESIM_CHS_H__

/**
   @addtogroup desim desim
   @{
 */

/*
 * CHS: Cylinder-head-sector rotational storage.
 */
#include "desim/sim.h"
#include "desim/storage.h"

struct chs_dev;

struct chs_conf {
	struct storage_conf cc_storage;

	unsigned   cc_heads;
	unsigned   cc_cylinders;
	unsigned   cc_track_skew;
	unsigned   cc_cylinder_skew;
	unsigned   cc_sectors_min;
	unsigned   cc_sectors_max;
	unsigned   cc_cyl_in_zone;

	sim_time_t cc_seek_avg;
	sim_time_t cc_seek_track_to_track;
	sim_time_t cc_seek_full_stroke;
	sim_time_t cc_write_settle;
	sim_time_t cc_head_switch;
	sim_time_t cc_command_latency;

	unsigned   cc_rps; /* revolutions per second */

	long long  cc_alpha;
	long long  cc_beta;

	struct {
		sector_t track_sectors;
		sector_t cyl_sectors;
		sector_t cyl_first;
	} *cc_zone;
};

enum chs_dev_state {
	CDS_XFER,
	CDS_IDLE
};

struct chs_dev {
	struct storage_dev  cd_storage;
	struct chs_conf    *cd_conf;
	enum chs_dev_state  cd_state;
	unsigned            cd_head;
	unsigned            cd_cylinder;
	struct sim_chan     cd_wait;
	struct sim_callout  cd_todo;

	struct cnt          cd_seek_time;
	struct cnt          cd_rotation_time;
	struct cnt          cd_xfer_time;
	struct cnt          cd_read_size;
	struct cnt          cd_write_size;
};

M0_INTERNAL void chs_conf_init(struct chs_conf *conf);
M0_INTERNAL void chs_conf_fini(struct chs_conf *conf);

M0_INTERNAL void chs_dev_init(struct chs_dev *dev, struct sim *sim,
			      struct chs_conf *conf);
M0_INTERNAL void chs_dev_fini(struct chs_dev *dev);

#endif /* __MOTR_DESIM_CHS_H__ */

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
