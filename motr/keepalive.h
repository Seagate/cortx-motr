/* -*- C -*- */
/*
 * Copyright (c) 2016-2020 Seagate Technology LLC and/or its Affiliates
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

#ifndef __MOTR_MOTR_KEEPALIVE_H__
#define __MOTR_MOTR_KEEPALIVE_H__

/**
 * @defgroup motr-keepalive
 *
 * @{
 */

#include "lib/types.h"          /* m0_uint128 */
#include "lib/atomic.h"         /* m0_atomic64 */
#include "ha/dispatcher.h"      /* m0_ha_handler */
#include "xcode/xcode_attr.h"   /* M0_XCA_RECORD */

#include "lib/types_xc.h"       /* m0_uint128_xc */

struct m0_ha_msg_keepalive_req {
	struct m0_uint128 kaq_id;
} M0_XCA_RECORD M0_XCA_DOMAIN(rpc);

struct m0_ha_msg_keepalive_rep {
	struct m0_uint128 kap_id;
	uint64_t          kap_counter;
} M0_XCA_RECORD M0_XCA_DOMAIN(rpc);

struct m0_ha_keepalive_handler {
	struct m0_ha_dispatcher *kah_dispatcher;
	struct m0_ha_handler     kah_handler;
	struct m0_atomic64       kah_counter;
};

M0_INTERNAL int
m0_ha_keepalive_handler_init(struct m0_ha_keepalive_handler *ka,
                             struct m0_ha_dispatcher        *hd);
M0_INTERNAL void
m0_ha_keepalive_handler_fini(struct m0_ha_keepalive_handler *ka);

/** @} end of motr-keepalive group */
#endif /* __MOTR_MOTR_KEEPALIVE_H__ */

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
