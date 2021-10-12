/* -*- C -*- */
/*
 * Copyright (c) 2017-2021 Seagate Technology LLC and/or its Affiliates
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


#define M0_TRACE_SUBSYSTEM    M0_TRACE_SUBSYS_FDMI

#include "lib/trace.h"
#include "fdmi/module.h"
#include "module/instance.h"

#if 0 /* unused code */
M0_UNUSED static int level_fdmi_enter(struct m0_module *module);
M0_UNUSED static void level_fdmi_leave(struct m0_module *module);

M0_UNUSED static const struct m0_modlev levels_fdmi[] = {
	[M0_LEVEL_FDMI] = {
		.ml_name = "fdmi is initialised",
		.ml_enter = level_fdmi_enter,
		.ml_leave = level_fdmi_leave,
	}
};

M0_UNUSED static int level_fdmi_enter(struct m0_module *module)
{
	return 0;
}

static void level_fdmi_leave(struct m0_module *module)
{
}
#endif

M0_INTERNAL struct m0_fdmi_module *m0_fdmi_module__get(void)
{
	return &m0_get()->i_fdmi_module;
}

#undef M0_TRACE_SUBSYSTEM

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
