/* -*- C -*- */
/*
 * Copyright (c) 2014-2020 Seagate Technology LLC and/or its Affiliates
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

#ifndef __MOTR_M0T1FS_M0T1FS_IOCTL_H__
#define __MOTR_M0T1FS_M0T1FS_IOCTL_H__

#include <linux/ioctl.h>

#define M0_M0T1FS_IOC_MAGIC (0xAA)

/* ioctl(fs, M0_M0T1FS_FWAIT) */
#define M0_M0T1FS_FWAIT _IO(M0_M0T1FS_IOC_MAGIC, 'w')

#endif /* __MOTR_M0T1FS_M0T1FS_IOCTL_H__ */

/*
 *  Local variables:
 *  c-indentation-style: "K&R"
 *  c-basic-offset: 8
 *  tab-width: 8
 *  fill-column: 80
 *  scroll-step: 1
 *  End:
 */
