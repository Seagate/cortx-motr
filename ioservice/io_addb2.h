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

#ifndef __MOTR_IOSERVICE_IO_ADDB2_H__
#define __MOTR_IOSERVICE_IO_ADDB2_H__

/**
 * @defgroup io_foms
 *
 * @{
 */

#include "addb2/identifier.h"    /* M0_AVI_IOS_RANGE_START */
#include "xcode/xcode_attr.h"


enum m0_avi_ios_io_labels {
	M0_AVI_IOS_IO_DESCR = M0_AVI_IOS_RANGE_START + 1,

	M0_AVI_IOS_IO_ATTR_FOMCRW_NDESC,
	M0_AVI_IOS_IO_ATTR_FOMCRW_TOTAL_IOIVEC_CNT,
	M0_AVI_IOS_IO_ATTR_FOMCRW_COUNT,
	M0_AVI_IOS_IO_ATTR_FOMCRW_BYTES,

	M0_AVI_IOS_IO_ATTR_FOMCOB_FOP_TYPE,
	M0_AVI_IOS_IO_ATTR_FOMCOB_GFID_CONT,
	M0_AVI_IOS_IO_ATTR_FOMCOB_GFID_KEY,
	M0_AVI_IOS_IO_ATTR_FOMCOB_CFID_CONT,
	M0_AVI_IOS_IO_ATTR_FOMCOB_CFID_KEY,
} M0_XCA_ENUM;

/** @} end of io_foms group */
#endif /* __MOTR_IOSERVICE_IO_ADDB2_H__ */

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
