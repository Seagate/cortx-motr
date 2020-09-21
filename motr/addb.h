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

#ifndef __MOTR_ADDB_H__
#define __MOTR_ADDB_H__

#include "addb2/addb2.h"
#include "addb2/identifier.h"
#include "motr/client_internal_xc.h"
#include "xcode/xcode_attr.h"

/**
   @addtogroup client
   @{
 */

enum m0_avi_labels {
	M0_AVI_CLIENT_SM_OP = M0_AVI_CLIENT_RANGE_START + 1,
	M0_AVI_CLIENT_SM_OP_COUNTER,
	M0_AVI_CLIENT_SM_OP_COUNTER_END = M0_AVI_CLIENT_SM_OP_COUNTER + 0x100,

	M0_AVI_CLIENT_TO_DIX,

	M0_AVI_CLIENT_COB_REQ,
	M0_AVI_CLIENT_TO_COB_REQ,
	M0_AVI_CLIENT_COB_REQ_TO_RPC,
	M0_AVI_CLIENT_TO_IOO,
	M0_AVI_IOO_TO_RPC,
	M0_AVI_CLIENT_BULK_TO_RPC,

	M0_AVI_OP_ATTR_ENTITY_ID,
	M0_AVI_OP_ATTR_CODE,

	M0_AVI_IOO_ATTR_BUFS_NR,
	M0_AVI_IOO_ATTR_BUF_SIZE,
	M0_AVI_IOO_ATTR_PAGE_SIZE,
	M0_AVI_IOO_ATTR_BUFS_ALIGNED,
	M0_AVI_IOO_ATTR_RMW,

	M0_AVI_IOO_REQ,
	M0_AVI_IOO_REQ_COUNTER,
	M0_AVI_IOO_REQ_COUNTER_END = M0_AVI_IOO_REQ_COUNTER + 0x100,
} M0_XCA_ENUM;

/** @} */ /* end of client group */

#endif /* __MOTR_ADDB_H__ */

/*
 *  Local variables:
 *  c-indentation-style: "K&R"
 *  c-basic-offset: 8
 *  tab-width: 8
 *  fill-column: 80
 *  scroll-step: 1
 *  End:
 */
