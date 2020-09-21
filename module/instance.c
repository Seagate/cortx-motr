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


#include "module/instance.h"
#include "module/param.h"     /* m0_param_sources_tlist_init */
#include "lib/thread.h"       /* m0_thread_tls */

/**
 * @addtogroup module
 *
 * @{
 */

M0_INTERNAL int m0_init_once(struct m0 *instance);
M0_INTERNAL void m0_fini_once(void);
M0_INTERNAL int m0_subsystems_init(void);
M0_INTERNAL void m0_subsystems_fini(void);
M0_INTERNAL int m0_quiesce_init(void);
M0_INTERNAL void m0_quiesce_fini(void);

M0_LOCKERS__DEFINE(M0_INTERNAL, m0_inst, m0, i_lockers);

M0_INTERNAL struct m0 *m0_get(void)
{
	struct m0 *result = m0_thread_tls()->tls_m0_instance;
	M0_POST(result != NULL);
	return result;
}

M0_INTERNAL void m0_set(struct m0 *instance)
{
	M0_PRE(instance != NULL);
	m0_thread_tls()->tls_m0_instance = instance;
}

static int level_inst_enter(struct m0_module *module)
{
	switch (module->m_cur + 1) {
	case M0_LEVEL_INST_PREPARE: {
		struct m0 *inst = M0_AMB(inst, module, i_self);

		m0_param_sources_tlist_init(&inst->i_param_sources);
		m0_inst_lockers_init(inst);
		return 0;
	}
	case M0_LEVEL_INST_QUIESCE_SYSTEM:
		return m0_quiesce_init();
	case M0_LEVEL_INST_ONCE:
		return m0_init_once(container_of(module, struct m0, i_self));
	case M0_LEVEL_INST_SUBSYSTEMS:
		return m0_subsystems_init();
	}
	M0_IMPOSSIBLE("Unexpected level: %d", module->m_cur + 1);
}

static void level_inst_leave(struct m0_module *module)
{
	struct m0 *inst = M0_AMB(inst, module, i_self);

	M0_PRE(module->m_cur == M0_LEVEL_INST_PREPARE);

	m0_inst_lockers_fini(inst);
	m0_param_sources_tlist_fini(&inst->i_param_sources);
}

static const struct m0_modlev levels_inst[] = {
	[M0_LEVEL_INST_PREPARE] = {
		.ml_name  = "M0_LEVEL_INST_PREPARE",
		.ml_enter = level_inst_enter,
		.ml_leave = level_inst_leave
	},
	[M0_LEVEL_INST_QUIESCE_SYSTEM] = {
		.ml_name  = "M0_LEVEL_INST_QUIESCE_SYSTEM",
		.ml_enter = level_inst_enter,
		.ml_leave = (void *)m0_quiesce_fini
	},
	[M0_LEVEL_INST_ONCE] = {
		.ml_name  = "M0_LEVEL_INST_ONCE",
		.ml_enter = level_inst_enter,
		.ml_leave = (void *)m0_fini_once
	},
	[M0_LEVEL_INST_SUBSYSTEMS] = {
		.ml_name  = "M0_LEVEL_INST_SUBSYSTEMS",
		.ml_enter = level_inst_enter,
		.ml_leave = (void *)m0_subsystems_fini
	},
	[M0_LEVEL_INST_READY] = {
		.ml_name = "M0_LEVEL_INST_READY"
	}
};

M0_INTERNAL void m0_instance_setup(struct m0 *instance)
{
	m0_module_setup(&instance->i_self, "m0 instance",
			levels_inst, ARRAY_SIZE(levels_inst), instance);
}

/** @} module */
