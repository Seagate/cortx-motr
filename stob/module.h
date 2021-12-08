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
#ifndef __MOTR_STOB_MODULE_H__
#define __MOTR_STOB_MODULE_H__

#include "module/module.h"
#include "stob/type.h"

/**
 * @defgroup stob Storage object
 *
 * @{
 */

/** Levels of m0_stob_module::stm_module. */
enum {
	/** m0_stob_types_init() has been called. */
	M0_LEVEL_STOB
};

struct m0_stob_module {
	struct m0_module     stm_module;
	struct m0_stob_types stm_types;
};

M0_INTERNAL struct m0_stob_module *m0_stob_module__get(void);

struct m0_stob_ad_module {
	struct m0_tl    sam_domains;
	struct m0_mutex sam_lock;
};

struct m0_stob_part_module {
	struct m0_tl    spm_domains;
	struct m0_mutex spm_lock;
};

/** @} end of stob group */
#endif /* __MOTR_STOB_MODULE_H__ */

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
