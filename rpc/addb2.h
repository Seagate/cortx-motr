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

#ifndef __MOTR_RPC_ADDB2_H__
#define __MOTR_RPC_ADDB2_H__

/**
 * @defgroup rpc
 *
 * @{
 */

#include "addb2/identifier.h"
#include "xcode/xcode_attr.h"

enum m0_avi_rpc_labels {
	M0_AVI_RPC_LOCK = M0_AVI_RPC_RANGE_START + 1,
	M0_AVI_RPC_REPLIED,
	M0_AVI_RPC_OUT_STATE,
	M0_AVI_RPC_IN_STATE,
	M0_AVI_RPC_ITEM_ID_ASSIGN,
	M0_AVI_RPC_ITEM_ID_FETCH,
	M0_AVI_RPC_BULK_OP,

	M0_AVI_RPC_ATTR_OPCODE,
        M0_AVI_RPC_ATTR_NR_SENT,

        M0_AVI_RPC_BULK_ATTR_OP,
        M0_AVI_RPC_BULK_ATTR_BUF_NR,
        M0_AVI_RPC_BULK_ATTR_BYTES,
        M0_AVI_RPC_BULK_ATTR_SEG_NR,
} M0_XCA_ENUM;

/** @} end of rpc group */
#endif /* __MOTR_RPC_ADDB2_H__ */

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
