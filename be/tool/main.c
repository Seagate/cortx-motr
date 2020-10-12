/* -*- C -*- */
/*
 * Copyright (c) 2015-2020 Seagate Technology LLC and/or its Affiliates
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



/**
 * @addtogroup be
 *
 * @{
 */

#define M0_TRACE_SUBSYSTEM M0_TRACE_SUBSYS_BE
#include "lib/trace.h"

#include <stdio.h>              /* printf */
#include <stdlib.h>             /* EXIT_FAILURE */

#include "lib/string.h"         /* m0_streq */

#include "be/tool/st.h"         /* m0_betool_st_mkfs */
#include "be/tool/common.h"     /* m0_betool_m0_init */
#include "be/ut/helper.h"       /* m0_be_ut_backend */


static const char *betool_help = ""
"Usage: m0betool [cmd] [path] [size]\n"
"where [cmd] is one from (without quotes):\n"
"- 'st mkfs'\n"
"- 'st run'\n"
"- 'be_recovery_run'\n"
"- 'create_be_log'\n"
"\n"
"Use case for 'st mkfs' and 'st run': run 'st mkfs' once to initialise \n"
"BE data structures, then run 'st run' and kill m0betool process during \n"
"'st run' execution. When 'st run' is called next time BE recovery will \n"
"replay BE log and 'st run' should be able to continue without any issues. \n"
"create_be_log will create BE log file with custom size. \n"
"This is an ST for BE recovery.\n"
"\n"
"'be_recovery_run' runs BE recovery.\n"
"\n"
"'path' parameter is an optional path to BE domain stob domain location.\n"
"Default BE domain stob domain location is used when this parameter is absent:"
"\n"
"'create_be_log' create BE log file without mkfs.\n"
"\n"
"'path' and 'size' mandatory arguments for create_be_log. 'size' in bytes. \n"
"Usage: m0betool create_be_log <path> <size> \n"
"\n";

static void be_recovery_run(char *path)
{
	struct m0_be_ut_backend ut_be = {};
	struct m0_be_domain_cfg cfg = {};
	char                    location[0x100] = {};
	int                     rc;

	m0_betool_m0_init();
	m0_be_ut_backend_cfg_default(&cfg);
	if (path != NULL) {
		snprintf((char *)&location, ARRAY_SIZE(location),
			 "linuxstob:%s", path);
		cfg.bc_stob_domain_location = (char *)&location;
	}
	rc = m0_be_ut_backend_init_cfg(&ut_be, &cfg, false);
	M0_ASSERT_INFO(rc == 0, "rc=%d", rc);

	m0_be_ut_backend_fini(&ut_be);
	m0_betool_m0_fini();
}

static void create_be_log(char *path, uint64_t size)
{
	struct m0_be_ut_backend ut_be = {};
	struct m0_be_domain_cfg cfg = {};
	char                    location[0x100] = {};
	int                     rc;

	m0_betool_m0_init();
	m0_be_ut_backend_cfg_default(&cfg);

	if (path != NULL) {
		snprintf((char *)&location, ARRAY_SIZE(location),
			 "linuxstob:%s", path);
		cfg.bc_stob_domain_location = (char *)&location;
	}
	cfg.bc_log.lc_store_cfg.lsc_size = size;
	cfg.bc_log.lc_store_cfg.lsc_stob_dont_zero = false;

	rc = m0_be_ut_backend_init_cfg_create(&ut_be, &cfg, true);
	M0_ASSERT_INFO(rc == 0, "rc=%d", rc);

	m0_betool_m0_fini();
}

int main(int argc, char *argv[])
{
	struct m0_be_domain_cfg  cfg = {};
	char                    *path;
	int                      rc;
	uint64_t                 size;

	if (argc > 1 && m0_streq(argv[1], "be_recovery_run")) {
		path = argc > 2 ? argv[2] : NULL;
		be_recovery_run(path);
		return EXIT_SUCCESS;
	}

	if (argc > 1 && m0_streq(argv[1], "create_be_log")) {
		if (argc > 3) {
			path = argv[2];
			size = atoi(argv[3]);
		} else {
			printf("%s", betool_help);
			return EXIT_FAILURE;
		}
		create_be_log(path, size);
		return EXIT_SUCCESS;
	}

	if (argc == 3 && m0_streq(argv[1], "st") &&
	    (m0_streq(argv[2], "mkfs") || m0_streq(argv[2], "run"))) {
		if (m0_streq(argv[2], "mkfs") == 0)
			rc = m0_betool_st_mkfs();
		else
			rc = m0_betool_st_run();
	} else {
		m0_betool_m0_init();
		m0_be_ut_backend_cfg_default(&cfg);
		printf("%s", betool_help);
		printf("bc_stob_domain_location=%s\n",
		       cfg.bc_stob_domain_location);
		rc = EXIT_FAILURE;
		m0_betool_m0_fini();
	}
	return rc;
}

#undef M0_TRACE_SUBSYSTEM

/** @} end of be group */

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
