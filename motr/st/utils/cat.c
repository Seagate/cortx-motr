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
#include "lib/trace.h"
#include "lib/getopts.h"
#include "motr/client.h"
#include "motr/idx.h"
#include "motr/st/utils/helper.h"

static struct m0_client          *m0_instance = NULL;
static struct m0_container container;
static struct m0_config    conf;
static struct m0_idx_dix_config   dix_conf;

extern struct m0_addb_ctx m0_addb_ctx;

static void cat_usage(FILE *file, char *prog_name)
{
	fprintf(file, "Usage: %s [OPTION]... DEST_FILE\n"
"Read from MOTR to DEST_FILE.\n"
"If no DEST_FILE is provided, then dump contents in console.\n"
"\n"
"Mandatory arguments to long options are mandatory for short options too.\n"
"  -l, --local          ADDR      Local endpoint address.\n"
"  -H, --ha             ADDR      HA endpoint address.\n"
"  -p, --profile        FID       Profile FID.\n"
"  -P, --process        FID       Process FID.\n"
"  -o, --object         FID       ID of the motr object "
				 "Object id should larger than "
				 "M0_ID_APP.\n%*c The first 0x100000 ids"
				 " are reserved for use by client.\n"
"  -s, --block-size     INT       Block size multiple of 4k in bytes or with "
				 "suffix b/k/m/g.\n%*c Ex: 1k=1024, "
				 "1m=1024*1024. Range: [4k-32m].\n"
"  -c, --block-count    INT       Number of blocks (>0) to copy, can give with "
				 "suffix b/k/m/g/K/M/G.\n%*c Ex: 1k=1024, "
				 "1m=1024*1024, 1K=1000 1M=1000*1000.\n"
"  -L, --layout-id      INT       Layout ID, Range: [1-14].\n"
"  -v, --pver           FID       Pool version fid.\n"
"  -e, --enable-locks             Enables acquiring and releasing RW locks "
				 "before and after performing IO.\n"
"  -b, --blocks-per-io  INT       Number of blocks per IO (>=0). \n%*c "
				 "Default=100 if 0 or nothing is provided.\n"
"  -r, --read-verify              Verify parity after reading the data.\n"
"  -S, --msg_size       INT       Max RPC msg size 64k i.e 65536\n"
                                 "%*c Note: this should match with m0d's current "
                                 "rpc msg size\n"
"  -q, --min_queue      INT       Minimum length of the receive queue i.e 16\n"
"  -O, --offset         INT       Updates the exisiting object from given "
				 "offset.\n%*c Default=0 if not provided. "
				 "Offset should be multiple of 4k.\n"
"  -N, --no-hole                  Report read error on hole in object\n"
"  -h, --help                     Shows this help text and exit.\n"
, prog_name, WIDTH, ' ', WIDTH, ' ', WIDTH, ' ', WIDTH, ' ', WIDTH, ' ',
WIDTH, ' ');
}

int main(int argc, char **argv)
{
	struct m0_utility_param cat_param;
	int                         rc;
	char                       *dest_fname = NULL;

	m0_utility_args_init(argc, argv, &cat_param,
			         &dix_conf, &conf, &cat_usage);

	fprintf(stderr, "pver is : "FID_F "\n", FID_P(&cat_param.cup_pver));

	rc = client_init(&conf, &container, &m0_instance);
	if (rc < 0) {
		fprintf(stderr, "init failed! rc = %d\n", rc);
		exit(EXIT_FAILURE);
	}

	if (argv[optind] != NULL)
		dest_fname = strdup(argv[optind]);

	rc = m0_read(&container, cat_param.cup_id, dest_fname,
		          cat_param.cup_block_size, cat_param.cup_block_count,
			  cat_param.cup_offset,
			  cat_param.cup_blks_per_io, cat_param.cup_take_locks,
			  cat_param.flags, &cat_param.cup_pver);
	if (rc < 0) {
		fprintf(stderr, "m0_read failed! rc = %d\n", rc);
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
