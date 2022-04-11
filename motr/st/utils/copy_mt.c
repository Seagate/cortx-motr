/* -*- C -*- */
/*
 * Copyright (c) 2019-2020 Seagate Technology LLC and/or its Affiliates
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
#include <getopt.h>

#include "lib/memory.h"    /* m0_free() */
#include "lib/trace.h"
#include "motr/client.h"
#include "motr/client_internal.h"
#include "motr/st/utils/helper.h"

static struct m0_client        *m0_instance = NULL;
static struct m0_container      container;
static struct m0_config         conf;
static struct m0_idx_dix_config dix_conf;

static void copy_thread_launch(struct m0_copy_mt_args *args)
{
	int index;

	/* lock ensures that each thread writes on different object id */
	m0_mutex_lock(&args->cma_mutex);
	index = args->cma_index;
	args->cma_utility->cup_id = args->cma_ids[index];
	args->cma_index++;
	m0_mutex_unlock(&args->cma_mutex);
	args->cma_rc[index] = m0_write(&container,
				       args->cma_utility->cup_file,
				       args->cma_utility->cup_id,
				       args->cma_utility->cup_block_size,
				       args->cma_utility->cup_block_count,
				       args->cma_utility->cup_offset,
				       args->cma_utility->cup_blks_per_io,
				       false,
				       args->cma_utility->cup_update_mode,
					   args->cma_utility->entity_flags);
}

static void copy_mt_usage(FILE *file, char *prog_name)
{
	fprintf(file, "Usage: %s [OPTION]... SOURCE\n"
"Copy SOURCE to MOTR (Multithreaded: One thread per object).\n"
"Designed to dump large amount of data into Motr"
"\n"
"Mandatory arguments to long options are mandatory for short options too.\n"
"  -l, --local          ADDR      Local endpoint address.\n"
"  -H, --ha             ADDR      HA endpoint address.\n"
"  -p, --profile        FID       Profile FID.\n"
"  -P, --process        FID       Process FID.\n"
"  -o, --object         FID       ID of the first motr object.\n"
"  -n, --n_obj          INT       No of objects to write.\n"
"  -s, --block-size     INT       Block size multiple of 4k in bytes or with "
				 "suffix b/k/m/g.\n%*c Ex: 1k=1024, "
				 "1m=1024*1024.  Range: [4k-32m]\n"
"  -c, --block-count    INT       Number of blocks (>0) to copy, can give with "
				 "suffix b/k/m/g/K/M/G.\n%*c Ex: 1k=1024, "
				 "1m=1024*1024, 1K=1000 1M=1000*1000.\n"
"  -L, --layout-id      INT       Layout ID, Range: [1-14].\n"
"  -b, --blocks-per-io  INT       Number of blocks (>=0) per IO. Default=100 "
				 "if 0 or nothing is provided.\n"
"  -O, --offset  INT              Updates the exisiting object from given "
				 "offset. \n%*c Default=0 if not provided. "
				 "Offset should be multiple of 4k.\n"
"  -r, --read-verify              Verify parity after reading the data.\n"
"  -G, --DI-generate                Flag to generate Data Integrity\n"
"  -I, --DI-user-input              Flag to pass checksum by user\n"
"  -S, --msg_size       INT       Max RPC msg size 64k i.e 65536\n"
                                 "%*c Note: this should match with m0d's current "
                                 "rpc msg size\n"
"  -q, --min_queue      INT       Minimum length of the receive queue i.e 16\n"
"  -u, --update_mode    INT       Object update mode\n"
"  -h, --help                     Shows this help text and exit.\n"
, prog_name, WIDTH, ' ', WIDTH, ' ', WIDTH, ' ', WIDTH, ' ');
}


int main(int argc, char **argv)
{
	struct m0_utility_param copy_mt_params;
	struct m0_copy_mt_args  copy_mt_args;
	struct m0_thread       *copy_th = NULL;
	int                     i = 0;
	int                     rc;
	int                     obj_nr;

	m0_utility_args_init(argc, argv, &copy_mt_params,
			     &dix_conf, &conf, &copy_mt_usage);

	/* Read Verify is only for m0cat */
	conf.mc_is_read_verify = false;

	if (copy_mt_params.cup_blks_per_io > M0_MAX_BLOCK_COUNT) {
		fprintf(stderr, "Blocks per IO (%d) is out of range. "
				"Max is %d for write operation!\n",
				 copy_mt_params.cup_blks_per_io,
				 M0_MAX_BLOCK_COUNT);
		return -EINVAL;
	}

	rc = client_init(&conf, &container, &m0_instance);
	if (rc < 0) {
		fprintf(stderr, "init failed! rc = %d\n", rc);
		exit(EXIT_FAILURE);
	}

	if (argv[optind] != NULL)
		copy_mt_params.cup_file = strdup(argv[optind]);

	copy_mt_args.cma_index = 0;
	copy_mt_args.cma_utility = &copy_mt_params;
	m0_mutex_init(&copy_mt_args.cma_mutex);
	obj_nr = copy_mt_params.cup_n_obj;

	M0_ALLOC_ARR(copy_th, obj_nr);
	M0_ALLOC_ARR(copy_mt_args.cma_ids, obj_nr);
	M0_ALLOC_ARR(copy_mt_args.cma_rc, obj_nr);

	for (i = 0; i < obj_nr; ++i) {
		copy_mt_args.cma_ids[i] = copy_mt_params.cup_id;
		copy_mt_args.cma_ids[i].u_lo = copy_mt_params.cup_id.u_lo + i;
	}

	/* launch one thread per object */
	for (i = 0; i < obj_nr; ++i)
		M0_THREAD_INIT(&copy_th[i], struct m0_copy_mt_args *,
		               NULL, &copy_thread_launch, &copy_mt_args,
		               "Writer");

	for (i = 0; i < obj_nr; ++i) {
		m0_thread_join(&copy_th[i]);
		m0_thread_fini(&copy_th[i]);
		if (copy_mt_args.cma_rc[i] != 0) {
			if (copy_mt_args.cma_rc[i] == -EEXIST) {
				fprintf(stderr, "Object exists for Id: " U128X_F
						". To update an existing "
						"object use -u=start index! "
						"rc = %d\n",
						 U128_P(&copy_mt_args.
							cma_ids[i]),
						 copy_mt_args.cma_rc[i]);
				rc = -EEXIST;
			} else {
				fprintf(stderr, "copy failed for "
						"object Id: " U128X_F
						" rc = %d\n",
						 U128_P(&copy_mt_args.
							cma_ids[i]),
						 copy_mt_args.cma_rc[i]);
				/*
				 * If any write fails, m0cp_mt operation is
				 * considered as unsuccessful.
				 */
				rc = -EIO;
			}
		}
	}

	/* Clean-up */
	m0_mutex_fini(&copy_mt_args.cma_mutex);
	client_fini(m0_instance);
	m0_free(copy_mt_args.cma_rc);
	m0_free(copy_mt_args.cma_ids);
	m0_free(copy_th);
	return M0_RC(rc);
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
