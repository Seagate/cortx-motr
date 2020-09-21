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

#ifndef __MOTR_DESIM_CLIENT_H__
#define __MOTR_DESIM_CLIENT_H__

/**
   @addtogroup desim desim
   @{
 */

#include "lib/tlist.h"

struct net_conf;
struct net_srv;

struct client {
	struct sim_thread  *cl_thread;
	struct sim_thread  *cl_pageout;
	unsigned long       cl_cached;
	unsigned long       cl_dirty;
	unsigned long       cl_io;
	unsigned long       cl_fid;
	unsigned            cl_id;
	unsigned            cl_inflight;
	struct sim_chan     cl_cache_free;
	struct sim_chan     cl_cache_busy;
	struct client_conf *cl_conf;
	struct m0_tl        cl_write_ext;
};

struct client_conf {
	unsigned           cc_nr_clients;
	unsigned           cc_nr_threads;
	unsigned long      cc_total;
	unsigned long      cc_count;
	unsigned long      cc_opt_count;
	unsigned           cc_inflight_max;
	sim_time_t         cc_delay_min;
	sim_time_t         cc_delay_max;
	unsigned long      cc_cache_max;
	unsigned long      cc_dirty_max;
	struct net_conf   *cc_net;
	struct net_srv    *cc_srv;
	struct client     *cc_client;
	struct cnt         cc_cache_free;
	struct cnt         cc_cache_busy;
	int                cc_shutdown;
};

M0_INTERNAL void client_init(struct sim *s, struct client_conf *conf);
M0_INTERNAL void client_fini(struct client_conf *conf);

#endif /* __MOTR_DESIM_CLIENT_H__ */

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
