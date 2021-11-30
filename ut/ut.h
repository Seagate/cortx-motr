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

#ifndef __MOTR_UT_UT_H__
#define __MOTR_UT_UT_H__

#include "module/module.h"
#include "lib/types.h"
#include "lib/list.h"     /* m0_list_link, m0_list */
#include "fid/fid.h"
#include "ut/ep.h"

struct m0_ut_module;

/**
   @defgroup ut Motr UT library
   @brief Common unit test library

   The intent of this library is to include all code, which could be potentially
   useful for several UTs and thus can be shared, avoiding duplication of
   similar code.

   @{
*/

# define M0_UT_ASSERT(a) m0_ut_assertimpl((a), #a, __FILE__, __LINE__, __func__)

#if defined M0_UT_TRACE && M0_UT_TRACE > 0
#  define M0_UT_ENTER(FMT, ...) \
	m0_console_printf("> %s: " FMT "\n", __func__, ## __VA_ARGS__)
#  define M0_UT_LOG(FMT, ...) \
	m0_console_printf("* %s: " FMT "\n", __func__, ## __VA_ARGS__)
#  define M0_UT_RETURN(FMT, ...) \
	m0_console_printf("< %s: " FMT "\n", __func__, ## __VA_ARGS__)
#else
#  define M0_UT_ENTER(...)
#  define M0_UT_LOG(...)
#  define M0_UT_RETURN(...)
#endif

/**
   structure to define test in test suite.
 */
struct m0_ut {
	/** name of the test, must be unique */
	const char *t_name;
	/** pointer to testing procedure */
	void      (*t_proc)(void);
	/** test's owner name */
	const char *t_owner;
	/** indicates whether test is enabled for execution */
	bool        t_enabled;
};

enum { M0_UT_SUITE_TESTS_MAX = 128 };

struct m0_ut_suite {
	struct m0_module           ts_module;
	/**
	 * modular dependencies of this suite
	 *
	 * XXX FIXME: How do we know the value of m0_ut_moddep::ud_module
	 * at compile time? We don't.
	 */
	const struct m0_ut_moddep *ts_deps;
	/** length of ->ts_deps array */
	unsigned                   ts_deps_nr;
	struct m0_list_link        ts_linkage;
	/** indicates whether this suite should be executed */
	bool                       ts_enabled;
	/**
	 * test can be masked at compile time, this overrides ts_enabled;
	 * this field can be set directly at m0_ut_suite initialization or by
	 * m0_ut_add(), it can be used to disable broken tests, in this case
	 * a message will be printed on console
	 */
	bool                       ts_masked;
	/** name of the suite */
	const char                *ts_name;
	/** suite owners names */
	const char                *ts_owners;
	/**
	 * optional configuration string in YAML format, it's interpreted by
	 * m0gentestds utility, check it's documentation regarding format of
	 * YAML data
	 */
	const char                *ts_yaml_config_string;
	/**
	 * This function is run after ->ts_deps are satisfied, but
	 * before the tests are executed. It is optional.
	 */
	int                      (*ts_init)(void);
	/**
	 * The function to run after the tests of this suite are executed.
	 * Optional.
	 */
	int                      (*ts_fini)(void);
	/** tests in the suite */
	struct m0_ut               ts_tests[M0_UT_SUITE_TESTS_MAX];
	/** Suite order for re-ordered runs. */
	int                        ts_order;
};

int m0_ut_init(struct m0 *instance);
void m0_ut_fini(void);

/**
 add test suite into global pool.
 if adding test suite failed application is aborted.

 @param ts pointer to test suite

 */
M0_INTERNAL void m0_ut_add(struct m0_ut_module *m, struct m0_ut_suite *ts,
			   bool enable);

/**
 * Shuffles added suites.
 */
M0_INTERNAL void m0_ut_shuffle(unsigned seed);

/**
 * Re-orders the suites to start from a given one.
 */
M0_INTERNAL void m0_ut_start_from(const char *suite);

/**
   run tests
 */
M0_INTERNAL int m0_ut_run(void);

/**
 * Return "small transaction credits" flag command line parameter
 */
M0_INTERNAL bool m0_ut_small_credits(void);

/**
 print all available test suites in YAML format to STDOUT

 @param with_tests - if true, then all tests of each suite are printed in
                     addition
 */
M0_INTERNAL void m0_ut_list(bool with_tests, bool yaml_output);

/**
 * Print owners of all UTs on STDOUT
 */
M0_INTERNAL void m0_ut_list_owners(void);

/**
 * Implements UT assert logic in the kernel.
 *
 * @param c the result of the boolean condition, evaluated by caller
 * @param lno line number of the assertion, eg __LINE__
 * @param str_c string representation of the condition, c
 * @param file path of the file, eg __FILE__
 * @param func name of the function which triggered assertion, eg __func__
 * @param panic flag, which controls whether this function should call
 *              m0_panic() or just print error message and continue
 */
M0_INTERNAL bool m0_ut_assertimpl(bool c, const char *str_c, const char *file,
				  int lno, const char *func);

/**
 * Parses fault point definitions from command line argument and enables them.
 *
 * The input string should be in format:
 *
 * func:tag:type[:integer[:integer]][,func:tag:type[:integer[:integer]]]
 */
M0_INTERNAL int m0_ut_enable_fault_point(const char *str);

/**
 * Parses fault point definitions from a yaml file and enables them.
 *
 * Each FP is described by a yaml mapping with the following keys:
 *
 *   func  - a name of the target function, which contains fault point
 *   tag   - a fault point tag
 *   type  - a fault point type, possible values are: always, oneshot, random,
 *           off_n_on_m
 *   p     - data for 'random' fault point
 *   n     - data for 'off_n_on_m' fault point
 *   m     - data for 'off_n_on_m' fault point
 *
 * An example of yaml file:
 *
 * @verbatim
 * ---
 *
 * - func: test_func1
 *   tag:  test_tag1
 *   type: random
 *   p:    50
 *
 * - func: test_func2
 *   tag:  test_tag2
 *   type: oneshot
 *
 * # yaml mappings could be specified in a short form as well
 * - { func: test_func3, tag:  test_tag3, type: off_n_on_m, n: 3, m: 1 }
 *
 * @endverbatim
 */
M0_INTERNAL int m0_ut_enable_fault_points_from_file(const char *file_name);

#ifndef __KERNEL__
#include <stdio.h>       /* FILE, fpos_t */

struct m0_ut_redirect {
	FILE  *ur_stream;
	int    ur_oldfd;
	int    ur_fd;
	fpos_t ur_pos;
};

/**
 * Associates one of the standard streams (stdin, stdout, stderr) with a file
 * pointed by 'path' argument.
 */
M0_INTERNAL void m0_stream_redirect(FILE * stream, const char *path,
				    struct m0_ut_redirect *redir);
/**
 * Restores standard stream from file descriptor and stream position, which were
 * saved earlier by m0_stream_redirect().
 */
M0_INTERNAL void m0_stream_restore(const struct m0_ut_redirect *redir);

/**
 * Checks if a text file contains the specified string.
 *
 * @param fp   - a file, which is searched for a string
 * @param mesg - a string to search for
 */
M0_INTERNAL bool m0_error_mesg_match(FILE * fp, const char *mesg);
#endif

M0_EXTERN struct m0_fid g_process_fid;

/** @} ut end group */
#endif /* __MOTR_UT_UT_H__ */
