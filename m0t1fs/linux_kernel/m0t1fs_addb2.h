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

#ifndef __MOTR_M0T1FS_LINUX_KERNEL_M0T1FS_ADDB2_H__
#define __MOTR_M0T1FS_LINUX_KERNEL_M0T1FS_ADDB2_H__

/**
 * @defgroup m0t1fs
 *
 * @{
 */

#include "addb2/identifier.h"    /* M0_AVI_M0T1FS_RANGE_START */

enum {
	M0_AVI_FS_OPEN = M0_AVI_M0T1FS_RANGE_START + 1,
	M0_AVI_FS_LOOKUP,
	M0_AVI_FS_CREATE,
	M0_AVI_FS_READ,
	M0_AVI_FS_WRITE,
	M0_AVI_FS_IO_DESCR,
	M0_AVI_FS_IO_MAP
};

/** @} end of m0t1fs group */
#endif /* __MOTR_M0T1FS_LINUX_KERNEL_M0T1FS_ADDB2_H__ */

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
