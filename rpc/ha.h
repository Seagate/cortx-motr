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

#ifndef __MOTR_RPC_HA_H__
#define __MOTR_RPC_HA_H__

/**
 * @defgroup rpc-ha
 *
 * @{
 */
#include "lib/types.h"          /* uint64_t */
#include "xcode/xcode_attr.h"   /* M0_XCA_RECORD */

struct m0_ha_msg_rpc {
	/** Indicates how many attempts to notify HA were made */
	uint64_t hmr_attempts;
	/** @see m0_ha_obj_state for values */
	uint64_t hmr_state;
} M0_XCA_RECORD M0_XCA_DOMAIN(rpc);

#endif /* __MOTR_RPC_HA_H__ */
