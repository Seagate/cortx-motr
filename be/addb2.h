/* -*- C -*- */
/*
 * Copyright (c) 2015-2020 Seagate Technology LLC and/or its Affiliates
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

#ifndef __MOTR_BE_ADDB2_H__
#define __MOTR_BE_ADDB2_H__

/**
 * @defgroup be
 *
 * @{
 */

#include "addb2/identifier.h"
#include "xcode/xcode_attr.h"

enum m0_avi_be_labels {
	M0_AVI_BE_TX_STATE = M0_AVI_BE_RANGE_START + 1,
	M0_AVI_BE_TX_COUNTER,
	M0_AVI_BE_TX_COUNTER_END = M0_AVI_BE_TX_COUNTER + 0x100,
	M0_AVI_BE_OP_COUNTER,
	M0_AVI_BE_OP_COUNTER_END = M0_AVI_BE_OP_COUNTER + 0x100,
	M0_AVI_BE_TX_TO_GROUP,

	M0_AVI_BE_TX_ATTR_PAYLOAD_NOB,
	M0_AVI_BE_TX_ATTR_PAYLOAD_PREP,
	M0_AVI_BE_TX_ATTR_LOG_RESERVED_SZ,
	M0_AVI_BE_TX_ATTR_LOG_USED,

	M0_AVI_BE_TX_ATTR_RA_AREA_USED,
	M0_AVI_BE_TX_ATTR_RA_PREP_TC_REG_NR,
	M0_AVI_BE_TX_ATTR_RA_PREP_TC_REG_SIZE,
	M0_AVI_BE_TX_ATTR_RA_CAPT_TC_REG_NR,
	M0_AVI_BE_TX_ATTR_RA_CAPT_TC_REG_SIZE,

	M0_AVI_BE_TX_CAPTURE
} M0_XCA_ENUM;

/** @} end of be group */
#endif /* __MOTR_BE_ADDB2_H__ */

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
