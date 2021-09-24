/* -*- C -*- */
/*
 * Copyright (c) 2021 Seagate Technology LLC and/or its Affiliates
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
 */

#include <stdio.h>
#include <unistd.h>               /* getopt */
#include <libgen.h>               /* basename */
#include "motr/client_internal.h" /* m0_client */
#include "util.h"

char *prog;

const char *help_str = "\
\n\
Usage: %s OPTIONS libpath\n\
\n\
   -e <addr>  endpoint address\n\
   -x <addr>  ha-agent (hax) endpoint address\n\
   -f <fid>   process fid\n\
   -p <fid>   profile fid\n\
\n";

static void usage()
{
	fprintf(stderr, help_str, prog);
	exit(1);
}

int main(int argc, char **argv)
{
	int               rc;
	int               opt;
	struct m0_client *cinst = NULL;
	struct m0_config  conf = {};

	prog = basename(strdup(argv[0]));

	while ((opt = getopt(argc, argv, ":he:x:f:p:")) != -1) {
		switch (opt) {
		case 'e':
			conf.mc_local_addr = optarg;
			break;
		case 'x':
			conf.mc_ha_addr = optarg;
			break;
		case 'f':
			conf.mc_process_fid = optarg;
			break;
		case 'p':
			conf.mc_profile = optarg;
			break;
		case 'h':
			usage();
			break;
		default:
			ERR("unknown option: %c\n", optopt);
			usage();
			break;
		}
	}

	if (conf.mc_local_addr == NULL || conf.mc_ha_addr == NULL ||
	    conf.mc_process_fid == NULL || conf.mc_profile == NULL) {
		ERR("mandatory parameter is missing\n");
		usage();
	}
	if (argc - optind < 1)
		usage();

	rc = isc_init(&conf, &cinst);
	if (rc != 0) {
		ERR("isc_init() failed: %d\n", rc);
		return 2;
	}

	rc = m0_isc_lib_register(argv[optind], &cinst->m0c_profile_fid,
				 &cinst->m0c_reqh);
	if (rc != 0)
		ERR("loading of library failed: rc=%d\n", rc);

	isc_fini(cinst);

	if (rc == 0)
		printf("%s success\n", prog);

	return rc != 0 ? 1 : 0;
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
