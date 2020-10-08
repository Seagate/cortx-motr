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
#ifndef __MOTR_UT_MODULE_H__
#define __MOTR_UT_MODULE_H__

#include "module/module.h"
#include "lib/atomic.h"     /* m0_atomic64 */



/**
 * @addtogroup ut
 *
 * @{
 */

/** Represents a (module, level) pair, which a UT suite depends on. */
struct m0_ut_moddep {
	struct m0_module *ud_module; /* XXX FIXME: Use enum m0_module_id. */
	int               ud_level;
};

enum { M0_UT_SUITES_MAX = 1024 };

struct m0_ut_module {
	struct m0_module    ut_module;
	/**
	 * Specifies the list of tests to run (ut_exclude is false)
	 * or to be excluded from testing (ut_exclude is true).
	 *
	 * Format: suite[:test][,suite[:test]]
	 */
	const char         *ut_tests;
	/**
	 * Whether ->ut_tests should be excluded from testing.
	 * @pre ergo(m->ut_exclude, m->ut_tests != NULL)
	 */
	bool                ut_exclude;
	/** Name of UT sandbox directory. */
	const char         *ut_sandbox;
	/** Whether to keep sandbox directory after UT execution. */
	bool                ut_keep_sandbox;
	bool                ut_small_credits;
	struct m0_ut_suite *ut_suites[M0_UT_SUITES_MAX];
	unsigned            ut_suites_nr;
	struct m0_atomic64  ut_asserts;
};

/** Levels of m0_ut_module. */
enum {
	/** Creates sandbox directory. */
	M0_LEVEL_UT_PREPARE,
	/**
	 * XXX DELETEME
	 * Registers dummy service types that are used by some UTs.
	 */
	M0_LEVEL_UT_KLUDGE,
	/** Depends on M0_LEVEL_UT_SUITE_READY of the used test suites. */
	M0_LEVEL_UT_READY
};

/** Levels of m0_ut_suite module. */
enum { M0_LEVEL_UT_SUITE_READY };

/*
 *          m0_ut_suite                      m0_ut_module
 *         +-------------------------+ *  1 +---------------------+
 * [<-----]| M0_LEVEL_UT_SUITE_READY |<-----| M0_LEVEL_UT_READY   |
 *         +-------------------------+      +---------------------+
 *                                          | M0_LEVEL_UT_PREPARE |
 *                                          +---------------------+
 */
/* XXX DELETEME */
M0_INTERNAL void m0_ut_suite_module_setup(struct m0_ut_suite *ts,
					  struct m0 *instance);

/*
 *  m0_ut_module                 m0
 * +---------------------+      +--------------------------+
 * | M0_LEVEL_UT_READY   |<-----| M0_LEVEL_INST_READY      |
 * +---------------------+      +--------------------------+
 * | M0_LEVEL_UT_KLUDGE  |   ,--| M0_LEVEL_INST_SUBSYSTEMS |
 * +---------------------+   |  +--------------------------+
 * | M0_LEVEL_UT_PREPARE |<--'  | M0_LEVEL_INST_ONCE       |
 * +---------------------+      +--------------------------+
 *                              | M0_LEVEL_INST_PREPARE    |
 *                              +--------------------------+
 */
extern const struct m0_module_type m0_ut_module_type;

/** @} ut */
#endif /* __MOTR_UT_MODULE_H__ */
