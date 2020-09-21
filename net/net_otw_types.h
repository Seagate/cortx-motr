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

#ifndef __MOTR_NET_NET_OTW_TYPES_H__
#define __MOTR_NET_NET_OTW_TYPES_H__

#include "lib/types.h"
#include "xcode/xcode_attr.h"

/**
 @addtogroup net
 @{
 */

struct m0_net_buf_desc {
	uint32_t  nbd_len;
	uint8_t  *nbd_data;
} M0_XCA_SEQUENCE M0_XCA_DOMAIN(rpc);

/**
 * In order to provide support for partially filled network buffers this
 * structure can be used. bdd_used stores how much data a network buffer
 * contains. rpc bulk fills this value.
 */
struct m0_net_buf_desc_data {
	struct m0_net_buf_desc bdd_desc;
	uint64_t               bdd_used;
} M0_XCA_RECORD M0_XCA_DOMAIN(rpc);

#endif /* __MOTR_NET_NET_OTW_TYPES_H__ */

/** @} end of net group */

/*
 *  Local variables:
 *  c-indentation-style: "K&R"
 *  c-basic-offset: 8
 *  tab-width: 8
 *  fill-column: 80
 *  scroll-step: 1
 *  End:
 */
