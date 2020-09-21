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

#ifndef __MOTR_RPC_IT_PING_FOM_H__
#define __MOTR_RPC_IT_PING_FOM_H__

#include "rpc/it/ping_fop.h"

/**
 * Object encompassing FOM for ping
 * operation and necessary context data
 */
struct m0_fom_ping {
	/** Generic m0_fom object. */
        struct m0_fom                    fp_gen;
	/** FOP associated with this FOM. */
        struct m0_fop			*fp_fop;
};

/**
 * <b> State Transition function for "ping" operation
 *     that executes on data server. </b>
 *  - Send reply FOP to client.
 */
M0_INTERNAL int m0_fom_ping_state(struct m0_fom *fom);
M0_INTERNAL size_t m0_fom_ping_home_locality(const struct m0_fom *fom);
M0_INTERNAL void m0_fop_ping_fom_fini(struct m0_fom *fom);

/* __MOTR_RPC_IT_PING_FOM_H__ */
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
