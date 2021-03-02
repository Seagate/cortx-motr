/* -*- C -*- */
/*
 * Copyright (c) 2012-2020 Seagate Technology LLC and/or its Affiliates
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


#include <linux/kernel.h>  /* printk */
#include <linux/module.h>  /* THIS_MODULE */
#include <linux/init.h>    /* module_init */
#include <linux/version.h>

#include "lib/list.h"
#include "lib/thread.h"
#include "motr/init.h"
#include "motr/version.h"
#include "motr/linux_kernel/module.h"
#include "module/instance.h"  /* m0 */

M0_INTERNAL int __init motr_init(void)
{
	static struct m0     instance;
	const struct module *m;
	M0_THREAD_ENTER;

	m = m0_motr_ko_get_module();
	pr_info("motr: init\n");
	m0_build_info_print();
	pr_info("motr: module address: 0x%p\n", m);
	pr_info("motr: module core address: 0x%p\n", M0_MOTR_KO_BASE(m));
	pr_info("motr: module core size: %u\n", M0_MOTR_KO_SIZE(m));

	return m0_init(&instance);
}

M0_INTERNAL void __exit motr_exit(void)
{
	M0_THREAD_ENTER;
	pr_info("motr: cleanup\n");
	m0_fini();
}

module_init(motr_init);
module_exit(motr_exit);

/*
 * We are using Apache license for complete motr code but for MODULE_LICENSE
 * marker there is no provision to mention Apache for this marker. But as this
 * marker is necessary to remove the warnings, keeping this blank to make
 * compiler happy.
 */

/* Added GPL per suggestions to avoid compilation error, may need to be reviewed */
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
/*
 * vim: tabstop=8 shiftwidth=8 noexpandtab textwidth=80 nowrap
 */
