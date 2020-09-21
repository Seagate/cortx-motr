/* -*- C -*- */
/*
 * Copyright (c) 2019-2020 Seagate Technology LLC and/or its Affiliates
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

#ifndef __MOTR_DIX_DIX_ADDB_H__
#define __MOTR_DIX_DIX_ADDB_H__

/**
 * @defgroup DIX
 *
 * @{
 */

#include "addb2/identifier.h"  /* M0_AVI_DIX_RANGE_START */
#include "xcode/xcode_attr.h"

enum m0_avi_dix_labels {
	M0_AVI_DIX_SM_REQ = M0_AVI_DIX_RANGE_START + 1,
	M0_AVI_DIX_SM_REQ_COUNTER,
	M0_AVI_DIX_SM_REQ_COUNTER_END = M0_AVI_DIX_SM_REQ_COUNTER + 0x100,

	M0_AVI_DIX_TO_MDIX,
	M0_AVI_DIX_TO_CAS,

	M0_AVI_DIX_REQ_ATTR_IS_META,
	M0_AVI_DIX_REQ_ATTR_REQ_TYPE,
	M0_AVI_DIX_REQ_ATTR_ITEMS_NR,
	M0_AVI_DIX_REQ_ATTR_INDICES_NR,
	M0_AVI_DIX_REQ_ATTR_KEYS_NR,
	M0_AVI_DIX_REQ_ATTR_VALS_NR,
} M0_XCA_ENUM;


/** @} end of DIX group */
#endif /* __MOTR_DIX_DIX_ADDB_H__ */

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
