/* -*- C -*- */
/*
 * Copyright (c) 2020 Seagate Technology LLC and/or its Affiliates
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

#include <stdlib.h>
#include <stdio.h>
#include <sys/time.h>
#include <getopt.h>
#include <libgen.h>

#include "lib/string.h"
#include "motr/client.h"
#include "motr/idx.h"
#include "motr/st/utils/helper.h"

static struct m0_client        *m0_instance = NULL;
static struct m0_container      container;
static struct m0_config         conf;
static struct m0_idx_dix_config dix_conf;

static void touch_usage(FILE *file, char *prog_name)
{
	fprintf(file, "Usage: %s [OPTION]...\n"
"Create empty MOTR object.\n"
"\n"
"Mandatory arguments to long options are mandatory for short options too.\n"
"  -l, --local          ADDR      Local endpoint address.\n"
"  -H, --ha             ADDR      HA endpoint address.\n"
"  -p, --profile        FID       Profile FID.\n"
"  -P, --process        FID       Process FID.\n"
"  -o, --object         FID       ID of the motr object. "
				 "Object id should larger than "
				 "M0_ID_APP.\n%*c The first 0x100000 "
				 "ids are reserved for use by client.\n"
"  -L, --layout-id      INT       Layout ID, Range:[1-14].\n"
"  -n, --n_obj          INT       Number of objects to touch.\n"
"  -S, --msg_size       INT       Max RPC msg size 64k i.e 65536\n"
				 "%*c Note: this should match with m0d's "
				 "current rpc msg size\n"
"  -q, --min_queue      INT       Minimum length of the receive queue i.e 16\n"

"  -e, --enable-locks             Enables acquiring and releasing RW locks "
				 "before and after performing IO.\n"
"  -h, --help                     Shows this help text and exit.\n"
, prog_name, WIDTH, ' ', WIDTH, ' ');
}


int main(int argc, char **argv)
{
	struct m0_utility_param touch_params;
	struct m0_uint128           b_id = M0_ID_APP;
	int                         i = 0;
	int                         rc;

	m0_utility_args_init(argc, argv, &touch_params,
			     &dix_conf,&conf, &touch_usage);

	/* Read Verify is only for m0cat */
	conf.mc_is_read_verify = false;

	rc = client_init(&conf, &container, &m0_instance);
	if (rc < 0) {
		fprintf(stderr, "init failed! rc = %d\n", rc);
		exit(EXIT_FAILURE);
	}

	/* Setting up base object id to object id received */
	b_id.u_lo = touch_params.cup_id.u_lo;
	for (i = 0; i < touch_params.cup_n_obj; ++i) {
		touch_params.cup_id.u_lo = b_id.u_lo + i;
		rc = touch(&container, touch_params.cup_id,
			   touch_params.cup_take_locks);
		if (rc != 0) {
			if (rc == -EEXIST) {
				fprintf(stderr, "Object id: " U128X_F "exists. "
						"rc = %d\n",
						 U128_P(&touch_params.cup_id),
						 rc);
			} else {
				fprintf(stderr, "Failed to touch Object id: "
						 U128X_F "rc = %d\n",
						 U128_P(&touch_params.cup_id),
						 rc);
			}

		}
	}

	client_fini(m0_instance);

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
