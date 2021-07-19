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
#include "util.h"

char *prog;

/* main */
int main(int argc, char **argv)
{
	int rc = 0;

	prog = basename(strdup(argv[0]));

	/* check input */
	if (argc != 2) {
		fprintf(stderr,"Usage:\n");
		fprintf(stderr,"%s libpath\n", prog);
		return -1;
	}

	m0util_setrc(prog);

	/* initialize resources */
	rc = m0util_init(0);
	if (rc != 0) {
		fprintf(stderr,"error! m0util_init() failed: %d\n", rc);
		return -2;
	}
	rc = m0util_isc_api_register(argv[1]);
	if ( rc != 0)
		fprintf(stderr, "error! loading of library from %s failed. \n",
			argv[1]);
	/* free resources*/
	m0util_free();

	/* success */
	if (rc == 0)
		fprintf(stderr,"%s success\n", prog);
	else
		fprintf(stderr,"%s fail\n", prog);
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
