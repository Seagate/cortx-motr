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

#ifndef __MOTR_STOB_ADDB2_H__
#define __MOTR_STOB_ADDB2_H__

/**
 * @defgroup stob
 *
 * @{
 */

#include "addb2/identifier.h"
#include "xcode/xcode.h"          /* M0_XCA_ENUM */

enum m0_avi_stob_io_labels {
	M0_AVI_STOB_IO_LAUNCH = M0_AVI_STOB_RANGE_START + 1,
	M0_AVI_STOB_IO_PREPARE,
	M0_AVI_STOB_IO_END,
	M0_AVI_STOB_IOQ,
	M0_AVI_STOB_IOQ_TICK,
	M0_AVI_STOB_IOQ_INFLIGHT,
	M0_AVI_STOB_IOQ_QUEUED,
	M0_AVI_STOB_IOQ_GOT,
	M0_AVI_STOB_IO_REQ,

        M0_AVI_STOB_IO_ATTR_UVEC_NR,
        M0_AVI_STOB_IO_ATTR_UVEC_COUNT,
        M0_AVI_STOB_IO_ATTR_UVEC_BYTES,

	M0_AVI_STOB_AD_TO_LINUX,
	M0_AVI_STIO_TO_Q,
	M0_AVI_STOB_IO_Q,
	M0_AVI_STOB_IOQ_FRAG,
	M0_AVI_STIO_IOQ,
} M0_XCA_ENUM;

enum m0_addb2_stio_req_labels {
	M0_AVI_IO_LAUNCH,
	M0_AVI_AD_PREPARE,
	M0_AVI_AD_WR_PREPARE,
	M0_AVI_AD_BALLOC_START,
	M0_AVI_AD_BALLOC_END,
	M0_AVI_AD_LAUNCH,
	M0_AVI_AD_SORT_START,
	M0_AVI_AD_SORT_END,
	M0_AVI_AD_ENDIO,
	M0_AVI_LIO_LAUNCH,
	M0_AVI_LIO_ENDIO,
} M0_XCA_ENUM;
/*
enum m0_stio_ioq_states {
	M0_AVI_STIO_TO_Q,
	M0_AVI_STOB_IO_Q,
	M0_AVI_STOB_IOQ_FRAG,
	M0_AVI_STIO_IOQ
} M0_XCA_ENUM;
*/

/** @} end of stob group */
#endif /* __MOTR_STOB_ADDB2_H__ */

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
