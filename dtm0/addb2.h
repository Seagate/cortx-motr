/* -*- C -*- */
/*
 * Copyright (c) 2021 Seagate Technology LLC and/or its Affiliates
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

#ifndef __MOTR_DTM0_ADDB2_H__
#define __MOTR_DTM0_ADDB2_H__

/**
 * @defgroup dtm0
 *
 * @{
 */

#include "addb2/identifier.h"

enum m0_avi_dtm0_labels {
	/* dtx0 */
	M0_AVI_DTX0_SM_STATE = M0_AVI_DTM0_RANGE_START + 1,
	M0_AVI_DTX0_SM_COUNTER,
	M0_AVI_DTX0_SM_COUNTER_END = M0_AVI_DTX0_SM_COUNTER + 0x100,

	/* Recovery Machine */
	M0_AVI_DRM_SM_STATE = M0_AVI_DTX0_SM_COUNTER_END + 1,
	M0_AVI_DRM_SM_COUNTER,
	M0_AVI_DRM_SM_COUNTER_END = M0_AVI_DRM_SM_COUNTER + 0x100,
};

/** @} end of dtm0 group */
#endif /* __MOTR_DTM0_ADDB2_H__ */

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
