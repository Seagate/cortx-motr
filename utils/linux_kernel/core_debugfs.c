/* -*- C -*- */
/*
 * Copyright (c) 2013-2020 Seagate Technology LLC and/or its Affiliates
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

#include <linux/kernel.h>    /* pr_info */
#include <linux/debugfs.h>   /* debugfs_create_dir */
#include <linux/module.h>    /* THIS_MODULE */
#include <linux/version.h>

#include "lib/misc.h"        /* bool */
#include "motr/linux_kernel/module.h"
#include "utils/linux_kernel/m0ctl_internal.h"
#include "utils/linux_kernel/trace_debugfs.h"


/**
 * @addtogroup m0ctl
 *
 * @{
 */

static struct dentry  *core_file;
static const char      core_name[] = "core";
static bool            core_file_is_opened = false;


static int core_open(struct inode *i, struct file *f)
{
	if (core_file_is_opened)
		return -EBUSY;

	core_file_is_opened = true;

	return 0;
}

static int core_release(struct inode *i, struct file *f)
{
	core_file_is_opened = false;
	return 0;
}

static ssize_t core_read(struct file *file, char __user *ubuf,
			 size_t ubuf_size, loff_t *ppos)
{
	const struct module *m = m0_motr_ko_get_module();

	return simple_read_from_buffer(ubuf, ubuf_size, ppos,
				       M0_MOTR_KO_BASE(m),
				       M0_MOTR_KO_SIZE(m));
}

static const struct file_operations core_fops = {
	.owner    = THIS_MODULE,
	.open     = core_open,
	.release  = core_release,
	.read     = core_read,
};

/******************************* init/fini ************************************/

int core_dfs_init(void)
{
	core_file = debugfs_create_file(core_name, S_IRUSR, dfs_root_dir, NULL,
					&core_fops);
	if (core_file == NULL) {
		pr_err(KBUILD_MODNAME ": failed to create debugfs file"
		       " '%s/%s'\n", dfs_root_name, core_name);
		return -EPERM;
	}

	return 0;
}

void core_dfs_cleanup(void)
{
	debugfs_remove(core_file);
}

/** @} end of m0ctl group */

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
