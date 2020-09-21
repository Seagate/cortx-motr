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

#ifndef __MOTR_CAS_CAS_ADDB2_H__
#define __MOTR_CAS_CAS_ADDB2_H__

#include "addb2/identifier.h"  /* M0_AVI_CAS_RANGE_START */
#include "xcode/xcode_attr.h"

/**
 * @defgroup cas-service
 *
 * @{
 */
enum m0_avi_cas_labels {
	M0_AVI_CAS_KV_SIZES = M0_AVI_CAS_RANGE_START + 1,

	M0_AVI_CAS_SM_REQ,
	M0_AVI_CAS_SM_REQ_COUNTER,
	M0_AVI_CAS_SM_REQ_COUNTER_END = M0_AVI_CAS_SM_REQ_COUNTER + 0x100,

	M0_AVI_CAS_TO_RPC,
	M0_AVI_CAS_FOM_TO_CROW_FOM,

	M0_AVI_CAS_REQ_ATTR_IS_META,
	M0_AVI_CAS_REQ_ATTR_REC_NR,

	M0_AVI_CAS_FOM_ATTR_IKV_NR,
	M0_AVI_CAS_FOM_ATTR_IN_INLINE_KEYS_NR,
	M0_AVI_CAS_FOM_ATTR_IN_BULK_KEYS_NR,
	M0_AVI_CAS_FOM_ATTR_IN_KEYS_SIZE,
	M0_AVI_CAS_FOM_ATTR_IN_INLINE_VALS_NR,
	M0_AVI_CAS_FOM_ATTR_IN_BULK_VALS_NR,
	M0_AVI_CAS_FOM_ATTR_IN_VALS_SIZE,

	M0_AVI_CAS_FOM_ATTR_OUT_INLINE_KEYS_NR,
	M0_AVI_CAS_FOM_ATTR_OUT_BULK_KEYS_NR,
	M0_AVI_CAS_FOM_ATTR_OUT_KEYS_SIZE,
	M0_AVI_CAS_FOM_ATTR_OUT_INLINE_VALS_NR,
	M0_AVI_CAS_FOM_ATTR_OUT_BULK_VALS_NR,
	M0_AVI_CAS_FOM_ATTR_OUT_VALS_SIZE,
} M0_XCA_ENUM;


/** @} end of cas-service group */
#endif /* __MOTR_CAS_CAS_ADDB2_H__ */

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
