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
#ifndef __MOTR_BE_HA_H__
#define __MOTR_BE_HA_H__

#include "lib/types.h"  /* uint8_t */

/**
 * @defgroup be-ha
 *
 * @{
 */

enum m0_be_location {
	M0_BE_LOC_NONE,
	M0_BE_LOC_LOG,
	M0_BE_LOC_SEGMENT_1,
	M0_BE_LOC_SEGMENT_2
};

/**
 * BE I/O error.
 *
 * Payload of m0_ha_msg, which Motr sends to HA in case of BE I/O error.
 */
struct m0_be_io_err {
	uint32_t ber_errcode; /* `int' is not xcodeable */
	uint8_t  ber_location;   /**< @see m0_be_location for values */
	uint8_t  ber_io_opcode;  /**< @see m0_stob_io_opcode for values */
} M0_XCA_RECORD M0_XCA_DOMAIN(be|rpc);

/**
 * Sends HA notification about BE I/O error.
 *
 * @note The function never returns.
 */
M0_INTERNAL void m0_be_io_err_send(uint32_t errcode, uint8_t location,
				   uint8_t io_opcode);

/** @} be-ha */
#endif /* __MOTR_BE_HA_H__ */
