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


#include "stob/module.h"
#include "module/instance.h"

#if 0 /* unused code */
static int level_stob_enter(struct m0_module *module);
static void level_stob_leave(struct m0_module *module);

static const struct m0_modlev levels_stob[] = {
	[M0_LEVEL_STOB] = {
		.ml_name = "stob is initialised",
		.ml_enter = level_stob_enter,
		.ml_leave = level_stob_leave,
	}
};

static int level_stob_enter(struct m0_module *module)
{
	return m0_stob_types_init();
}

static void level_stob_leave(struct m0_module *module)
{
	m0_stob_types_fini();
}
#endif

M0_INTERNAL struct m0_stob_module *m0_stob_module__get(void)
{
	return &m0_get()->i_stob_module;
}

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
