/* -*- C -*- */
/*
 * Copyright (c) 2016-2020 Seagate Technology LLC and/or its Affiliates
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
 * @addtogroup client
 *
 * @{
 */

#define M0_TRACE_SUBSYSTEM M0_TRACE_SUBSYS_CLIENT
#include "lib/trace.h"

#include <stdio.h>              /* vprintf */
#include <stdlib.h>             /* exit */
#include <stdarg.h>             /* va_list */
#include "lib/errno.h"
#include "index.h"
#include "common.h"
#include "lib/getopts.h"	/* M0_GETOPTS */
#include "lib/thread.h"		/* LAMBDA */
#include "module/instance.h"	/* m0 */

struct c_subsystem {
	const char  *s_name;
	int        (*s_init)(struct params *par);
	void       (*s_fini)(void);
	void       (*s_usage)(void);
	int        (*s_execute)(int argc, char** argv);
};

static struct c_subsystem subsystems[] = {
	{ "index", index_init, index_fini, index_usage, index_execute }
};

enum {
	LOCAL,
	HA,
	CONFD,
	PROF,
	HELP
};

static struct m0 instance;
bool is_str;
bool is_enf_meta;
bool is_skip_layout;
bool is_crow_disable;
struct m0_fid dix_pool_ver;

static int subsystem_id(char *name)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(subsystems); ++i) {
		if (!strcmp(name, subsystems[i].s_name))
			return i;
	}
	return M0_ERR(-EPROTONOSUPPORT);
}

static void usage(void)
{
	int i;

	m0_console_printf(
		"Client Command Line tool: m0kv\n"
		"Usage: ./m0kv "
		"-l local_addr -h ha_addr -p profile -f proc_fid "
		"[-s] [subsystem] [subsystem commands]\n"
		"\n"
		"Use -? for more verbose help on common arguments.\n"
		"Usage example for common arguments: \n"
		"./m0kv -l 10.0.2.15@tcp:12345:33:100 "
		"-h 10.0.2.15@tcp:12345:34:1 "
		"-p '<0x7000000000000001:0>' -f '<0x7200000000000000:0>'"
		" [subsystem] [subsystem commands]\n"
		"\n"
		"-s  Enable string format and it is optional.\n"
		"-e  Enable M0_ENF_META flag and it is optional.\n"
		"-v 7600000000000001:30 it is mandatory with -e for "
		"PUT, GET, DEL and NEXT operations.\n"
		"-L  Enable M0_OIF_SKIP_LAYOUT flag and it is optional.\n"
		"-v 7600000000000001:30 it is mandatory with -L for "
		"other operations except CREATE.\n"
		"-C  Disable M0_OIF_CROW flag and it is optional.\n"
		"Available subsystems and subsystem-specific commands are "
		"listed below.\n");
	for (i = 0; i < ARRAY_SIZE(subsystems); i++)
		subsystems[i].s_usage();
}

static int opts_get(struct params *par, int *argc, char ***argv)
{
	int    rc = 0;
	char **arg = *(argv);
	int    common_args = 9;
	CAPTURED char *pv = NULL;

	par->cp_local_addr = NULL;
	par->cp_ha_addr    = NULL;
	par->cp_prof       = NULL;
	par->cp_proc_fid   = NULL;

	rc = M0_GETOPTS("m0kv", *argc, *argv,
			M0_HELPARG('?'),
			M0_VOIDARG('i', "more verbose help",
					LAMBDA(void, (void) {
						usage();
						exit(0);
					})),
			M0_STRINGARG('l', "Local endpoint address",
					LAMBDA(void, (const char *string) {
					par->cp_local_addr = (char*)string;
					})),
			M0_STRINGARG('h', "HA address",
					LAMBDA(void, (const char *str) {
						par->cp_ha_addr = (char*)str;
					})),
			M0_STRINGARG('f', "Process FID",
					LAMBDA(void, (const char *str) {
						par->cp_proc_fid = (char*)str;
					})),
			M0_STRINGARG('p', "Profile options for Client",
					LAMBDA(void, (const char *str) {
						par->cp_prof = (char*)str;
					})),
			M0_FLAGARG('s', "Enable string format",
					&is_str),
			M0_FLAGARG('e', "Enable M0_ENF_META flag",
					&is_enf_meta),
			M0_FLAGARG('L', "Enable M0_OIF_SKIP_LAYOUT flag",
					&is_skip_layout),
			M0_FLAGARG('C', "Disable M0_OIF_CROW flag",
					&is_crow_disable),
			M0_STRINGARG('v', "DIX pool version information",
					LAMBDA(void, (const char *str) {
						pv = (char *)str;
					})));
	if (rc != 0)
		return M0_ERR(rc);
	/* All mandatory params must be defined. */
	if (rc == 0 &&
	    (par->cp_local_addr == NULL || par->cp_ha_addr == NULL ||
	     par->cp_prof == NULL || par->cp_proc_fid == NULL)) {
		usage();
		rc = M0_ERR(-EINVAL);
	}

	if (is_str)
		common_args++;

	if (is_enf_meta)
		common_args++;

	if (is_skip_layout)
		common_args++;

	if (is_crow_disable)
		common_args++;

	if (pv != NULL) {
		common_args += 2;
		rc = m0_fid_sscanf(pv, &dix_pool_ver);
	}

	*argc -= common_args;
	*(argv) = arg + common_args;
	return rc;
}

int main(int argc, char **argv)
{
	int           rc;
	int           subid;
	struct params params;

	m0_instance_setup(&instance);
	rc = m0_module_init(&instance.i_self, M0_LEVEL_INST_ONCE);
	if (rc != 0) {
		m0_console_printf("Cannot init module %i\n", rc);
		return M0_ERR(rc);
	}
	rc = opts_get(&params, &argc, &argv);

	if (rc == 0)
		rc = subsystem_id(argv[0]);
	if (rc == 0) {
		subid = rc;
		rc = subsystems[subid].s_init(&params);
		if (rc == 0) {
			rc = subsystems[subid].s_execute(argc - 1, argv + 1);
			if (rc != 0)
				m0_console_printf("Execution result %i\n", rc);
			else
				rc = 0;
			subsystems[subid].s_fini();
		}
		else
			m0_console_printf("Initialization error %i\n", rc);
	}
	if (rc < 0) {
		m0_console_printf("Got error %i\n", rc);
		/* main() should return positive values. */
		rc = -rc;
	}
	m0_console_printf("Done, rc:  %i\n", rc);
	return rc;
}

#undef M0_TRACE_SUBSYSTEM

/** @} end of client group */

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
