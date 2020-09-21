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


#include "ut/module.h"
#include "ut/ut.h"            /* m0_ut_suite */
#include "ut/ut_internal.h"   /* m0_ut_sandbox_init */
#include "ut/cs_service.h"    /* m0_cs_default_stypes_init */
#include "module/instance.h"  /* m0 */
#include "lib/memory.h"       /* M0_ALLOC_PTR */

/**
 * @addtogroup ut
 *
 * @{
 */

static struct m0_module *ut_module_create(struct m0 *instance);

const struct m0_module_type m0_ut_module_type = {
	.mt_name   = "m0_ut_module",
	.mt_create = ut_module_create
};

static int level_ut_enter(struct m0_module *module)
{
	switch (module->m_cur + 1) {
	case M0_LEVEL_UT_PREPARE: {
		struct m0_ut_module *m = M0_AMB(m, module, ut_module);

		m0_atomic64_set(&m->ut_asserts, 0);
		return m0_ut_sandbox_init(m->ut_sandbox);
	}
	case M0_LEVEL_UT_KLUDGE:
		return m0_cs_default_stypes_init();
	}
	M0_IMPOSSIBLE("Unexpected level: %d", module->m_cur + 1);
}

static void level_ut_leave(struct m0_module *module)
{
	struct m0_ut_module *m = M0_AMB(m, module, ut_module);

	M0_PRE(module->m_cur == M0_LEVEL_UT_PREPARE);

	m0_ut_sandbox_fini(m->ut_sandbox, m->ut_keep_sandbox);
}

static const struct m0_modlev levels_ut[] = {
	[M0_LEVEL_UT_PREPARE] = {
		.ml_name  = "M0_LEVEL_UT_PREPARE",
		.ml_enter = level_ut_enter,
		.ml_leave = level_ut_leave
	},
	[M0_LEVEL_UT_KLUDGE] = {
		.ml_name  = "M0_LEVEL_UT_KLUDGE",
		.ml_enter = level_ut_enter,
		.ml_leave = (void *)m0_cs_default_stypes_fini
	},
	[M0_LEVEL_UT_READY] = {
		.ml_name  = "M0_LEVEL_UT_READY"
	}
};

static const struct m0_modlev levels_ut_suite[] = {
	[M0_LEVEL_UT_SUITE_READY] = {
		.ml_name = "M0_LEVEL_UT_SUITE_READY"
	}
};

/* XXX TODO: Move this logic to some level of m0_ut_module. */
M0_INTERNAL void
m0_ut_suite_module_setup(struct m0_ut_suite *ts, struct m0 *instance)
{
	int i;

	m0_module_setup(&ts->ts_module, "m0_ut_suite module",
			levels_ut_suite, ARRAY_SIZE(levels_ut_suite), instance);
	for (i = 0; i < ts->ts_deps_nr; ++i) {
		M0_IMPOSSIBLE("XXX FIXME: This won't work, because we"
			      " don't know the address of a module"
			      " at compile time.");
		m0_module_dep_add(&ts->ts_module, M0_LEVEL_UT_SUITE_READY,
				  ts->ts_deps[i].ud_module,
				  ts->ts_deps[i].ud_level);
	}
}

static struct m0_module *ut_module_create(struct m0 *instance)
{
	static struct m0_ut_module ut;

	m0_module_setup(&ut.ut_module, m0_ut_module_type.mt_name,
			levels_ut, ARRAY_SIZE(levels_ut), instance);
#if 1 /* XXX FIXME
       *
       * m0_ut_stob_init(), called when M0_LEVEL_INST_SUBSYSTEMS is entered,
       * requires a sandbox directory, which is created by
       * M0_LEVEL_UT_PREPARE's ->ml_enter().
       *
       * This is a temporary solution. It should go away together with
       * M0_LEVEL_INST_SUBSYSTEMS.
       */
	m0_module_dep_add(&instance->i_self, M0_LEVEL_INST_SUBSYSTEMS,
			  &ut.ut_module, M0_LEVEL_UT_PREPARE);
#endif
	m0_module_dep_add(&instance->i_self, M0_LEVEL_INST_READY,
			  &ut.ut_module, M0_LEVEL_UT_READY);
	instance->i_moddata[M0_MODULE_UT] = &ut;
	return &ut.ut_module;
}

/** @} ut */
