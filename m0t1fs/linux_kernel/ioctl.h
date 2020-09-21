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

#ifndef __MOTR_M0T1FS_IOCTL_H__
#define __MOTR_M0T1FS_IOCTL_H__

#include <linux/version.h>    /* LINUX_VERSION_CODE */
#include <linux/ioctl.h>      /* include before m0t1fs/m0t1fs_ioctl.h */

#include "m0t1fs/m0t1fs_ioctl.h"

#ifdef __KERNEL__
/**
 * Handles the ioctl targeted to m0t1fs.
 * @param filp File associated to the file descriptor used when creating
 * the ioctl.
 * @param cmd Code of the operation requested by the ioctl.
 * @param arg Argument passed from user space. It can be a pointer.
 * @return -ENOTTY if m0t1fs does not support the requested operation.
 * Otherwise, the value returned by the operation.
 */
#if LINUX_VERSION_CODE >= KERNEL_VERSION(3,10,0)
M0_INTERNAL long m0t1fs_ioctl(struct file *filp, unsigned int cmd,
			      unsigned long arg);
#else
M0_INTERNAL int m0t1fs_ioctl(struct inode *inode, struct file *filp,
			     unsigned int cmd, unsigned long arg);
#endif
#endif /* __KERNEL__ */

#endif /* __MOTR_M0T1FS_IOCTL_H__ */

/*
 *  Local variables:
 *  c-indentation-style: "K&R"
 *  c-basic-offset: 8
 *  tab-width: 8
 *  fill-column: 80
 *  scroll-step: 1
 *  End:
 */
