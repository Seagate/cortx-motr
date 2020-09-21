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


#include <linux/fs.h>	     /* struct file, struct inode */
#include <linux/version.h>   /* LINUX_VERSION_CODE */

#define M0_TRACE_SUBSYSTEM M0_TRACE_SUBSYS_M0T1FS
#include "lib/trace.h"

#include "lib/assert.h"
#include "m0t1fs/linux_kernel/ioctl.h"
#include "m0t1fs/linux_kernel/fsync.h"
#include "m0t1fs/linux_kernel/m0t1fs.h"
#include "m0t1fs/linux_kernel/file_internal.h"

#if LINUX_VERSION_CODE >= KERNEL_VERSION(3,10,0)
M0_INTERNAL long m0t1fs_ioctl(struct file *filp, unsigned int cmd,
			      unsigned long arg)
#else
M0_INTERNAL int m0t1fs_ioctl(struct inode                              *inode,
			     __attribute__((unused)) struct file       *filp,
			     unsigned int                               cmd,
			     __attribute__((unused)) unsigned long      arg)
#endif
{
#if LINUX_VERSION_CODE >= KERNEL_VERSION(3,10,0)
	struct inode        *inode;
#endif
	struct m0t1fs_inode *m0inode;
	int                  rc;

	M0_THREAD_ENTER;
	M0_ENTRY();

#if LINUX_VERSION_CODE >= KERNEL_VERSION(3,10,0)
	inode = file_inode(filp);
	M0_ASSERT(inode != NULL);
#else
	M0_PRE(inode != NULL);
#endif

	m0inode = m0t1fs_inode_to_m0inode(inode);
	M0_ASSERT(m0inode != NULL);

	switch(cmd) {
	case M0_M0T1FS_FWAIT:
		rc = m0t1fs_fsync_core(m0inode, M0_FSYNC_MODE_PASSIVE);
		break;
	default:
		return M0_ERR_INFO(-ENOTTY, "Unknown IOCTL: %d", cmd);
	}

	return M0_RC(rc);
}
