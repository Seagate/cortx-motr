/* -*- C -*- */
/*
 * Copyright (c) 2011-2020 Seagate Technology LLC and/or its Affiliates
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



#include <linux/module.h>      /* MODULE_XXX */
#include <linux/init.h>        /* module_init */
#include <linux/debugfs.h>     /* debugfs_create_dir */
#include <linux/kernel.h>      /* pr_err */



#include "lib/thread.h"                           /* M0_THREAD_ENTER */
#include "utils/linux_kernel/m0ctl_internal.h"
#include "utils/linux_kernel/finject_debugfs.h"   /* fi_dfs_init */
#include "utils/linux_kernel/trace_debugfs.h"     /* trc_dfs_init */
#include "utils/linux_kernel/core_debugfs.h"      /* core_dfs_init */

/**
 * @defgroup m0ctl Motr Kernel-space Control
 *
 * @brief m0ctl driver provides a debugfs interface to control m0tr in
 * runtime. All control files are placed under "motr/" directory in the root
 * of debugfs file system. For more details about debugfs please @see
 * Documentation/filesystems/debugfs.txt in the linux kernel's source tree.
 *
 * This file is responsible only for creation and cleanup of motr/ directory
 * in debugfs. Please, put all code related to a particular control interface
 * into a separate *.c file. @see finject_debugfs.c for example.
 */

#if 0
struct dentry  *dfs_root_dir;
const char      dfs_root_name[] = "motr";

int dfs_init(void)
{
	int rc;

	pr_info(KBUILD_MODNAME ": init\n");

	/* create motr's main debugfs directory */
	dfs_root_dir = debugfs_create_dir(dfs_root_name, NULL);
	if (dfs_root_dir == NULL) {
		pr_err(KBUILD_MODNAME ": failed to create debugfs dir '%s'\n",
		       dfs_root_name);
		return -EPERM;
	}

	rc = fi_dfs_init();
	if (rc != 0)
		goto err;

	rc = trc_dfs_init();
	if (rc != 0)
		goto err;

	rc = core_dfs_init();
	if (rc != 0)
		goto err;

	return 0;
err:
	debugfs_remove_recursive(dfs_root_dir);
	dfs_root_dir = 0;
	return rc;
}

void dfs_cleanup(void)
{
	pr_info(KBUILD_MODNAME ": cleanup\n");

	core_dfs_cleanup();
	trc_dfs_cleanup();
	fi_dfs_cleanup();

	/*
	 * remove all orphaned debugfs files (if any) and motr's debugfs root
	 * directroy itself
	 */
	if (dfs_root_dir != 0) {
		debugfs_remove_recursive(dfs_root_dir);
		dfs_root_dir = 0;
	}
}

int __init m0ctl_init(void)
{
	M0_THREAD_ENTER;
	return dfs_init();
}

void __exit m0ctl_exit(void)
{
	M0_THREAD_ENTER;
	dfs_cleanup();
}

#endif

int __init m0ctl_init(void)
{
        return 0;
}

void __exit m0ctl_exit(void)
{
}

module_init(m0ctl_init);
module_exit(m0ctl_exit);
MODULE_LICENSE("GPL");
/*
 *  Local variables:
 *  c-indentation-style: "K&R"
 *  c-basic-offset: 8
 *  tab-width: 8
 *  fill-column: 80
 *  scroll-step: 1
 *  End:
 */
