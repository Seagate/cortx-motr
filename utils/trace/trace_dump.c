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



#include <stdio.h>      /* printf */
#include <err.h>        /* err */
#include <errno.h>      /* errno */
#include <string.h>     /* strcpy, basename */
#include <sysexits.h>   /* EX_* exit codes (EX_OSERR, EX_SOFTWARE) */

#if defined(M0_DARWIN)
#include <libgen.h>                    /* basename */
#endif

#include "module/instance.h"       /* m0 */
#include "motr/init.h"             /* m0_init */
#include "lib/uuid.h"              /* m0_node_uuid_string_set */
#include "lib/getopts.h"           /* M0_GETOPTS */
#include "lib/thread.h"            /* LAMBDA */
#include "lib/string.h"            /* m0_strdup */
#include "lib/user_space/types.h"  /* bool */
#include "lib/user_space/trace.h"  /* m0_trace_parse */
#include "lib/misc.h"              /* ARRAY_SIZE */


#define DEFAULT_MOTR_KO_IMG_PATH  "/var/log/motr/m0tr_ko.img"

int main(int argc, char *argv[])
{
	static struct m0 instance;

	const char  std_inout_file_name[] = "-";
	CAPTURED const char *input_file_name       = std_inout_file_name;
	CAPTURED const char *output_file_name      = std_inout_file_name;
	CAPTURED const char *m0tr_ko_path          = DEFAULT_MOTR_KO_IMG_PATH;
	CAPTURED enum m0_trace_parse_flags flags = M0_TRACE_PARSE_DEFAULT_FLAGS;
	FILE       *input_file;
	FILE       *output_file;
	int         rc;

	/* prevent creation of trace file for ourselves */
	m0_trace_set_mmapped_buffer(false);

	/* we don't need a real node uuid, so we force a default one to be useed
	 * instead */
	m0_node_uuid_string_set(NULL);

	rc = m0_init(&instance);
	if (rc != 0)
		return EX_SOFTWARE;

	/* process CLI options */
	rc = M0_GETOPTS(basename(argv[0]), argc, argv,
	  M0_HELPARG('h'),
	  M0_STRINGARG('i',
		"input file name, if none is provided, then STDIN is used by"
		" default",
		LAMBDA(void, (const char *str) {
			input_file_name = m0_strdup(str);
		})
	  ),
	  M0_STRINGARG('o',
		"output file name, if none is provided, then STDOUT is used by"
		" default",
		LAMBDA(void, (const char *str) {
			output_file_name = m0_strdup(str);
		})
	  ),
	  M0_VOIDARG('s',
		  "stream mode, each trace record is formatted as a"
		  " separate YAML document, so they can be fetched from"
		  " YAML stream one by one (this option has no effect as it's"
		  " 'on' by default, it has been kept for backward"
		  " compatibility, it's superseded by '-S' option)",
		  LAMBDA(void, (void) { })
	  ),
	  M0_VOIDARG('S',
		  "disable stream mode (discards action of '-s' option)",
		  LAMBDA(void, (void) {
		    flags |= M0_TRACE_PARSE_YAML_SINGLE_DOC_OUTPUT;
		  })
	  ),
	  M0_VOIDARG('H',
		  "dump only trace header information",
		  LAMBDA(void, (void) {
		    flags |= M0_TRACE_PARSE_HEADER_ONLY;
		  })
	  ),
	  M0_STRINGARG('k',
		"path to m0tr.ko modules's core image (only required for"
		" parsing kernel mode trace files), by default it is '"
		DEFAULT_MOTR_KO_IMG_PATH "'",
		LAMBDA(void, (const char *str) {
			m0tr_ko_path = m0_strdup(str);
		})
	  ),
	);

	if (rc != 0)
		return EX_USAGE;

	/* open input file */
	if (strcmp(input_file_name, std_inout_file_name) == 0) {
		input_file = stdin;
	} else {
		input_file = fopen(input_file_name, "r");
		if (input_file == NULL)
			err(EX_NOINPUT, "Failed to open input file '%s'",
					input_file_name);
	}

	/* open output file */
	if (strcmp(output_file_name, std_inout_file_name) == 0) {
		output_file = stdout;
	} else {
		output_file = fopen(output_file_name, "w");
		if (output_file == NULL)
			err(EX_CANTCREAT, "Failed to open output file '%s'",
					  output_file_name);
	}

	rc = m0_trace_parse(input_file, output_file, m0tr_ko_path, flags,
			    0, 0);
	if (rc != 0) {
		warnx("Error occurred while parsing input trace data");
		rc = EX_SOFTWARE;
	}

	m0_fini();

	fclose(output_file);
	fclose(input_file);

	return rc;
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
