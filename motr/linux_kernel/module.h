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


#pragma once

#ifndef __MOTR_MOTR_LINUX_KERNEL_MODULE_H__
#define __MOTR_MOTR_LINUX_KERNEL_MODULE_H__

#include <linux/version.h> /* LINUX_VERSION_CODE */

M0_INTERNAL const struct module *m0_motr_ko_get_module(void);

#if LINUX_VERSION_CODE >= KERNEL_VERSION(4,5,0)
#define M0_MOTR_KO_BASE(module) ((module)->core_layout.base)
#define M0_MOTR_KO_SIZE(module) ((module)->core_layout.size)
#else
#define M0_MOTR_KO_BASE(module) ((module)->module_core)
#define M0_MOTR_KO_SIZE(module) ((module)->core_size)
#endif

#endif /* __MOTR_MOTR_LINUX_KERNEL_MODULE_H__ */

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
