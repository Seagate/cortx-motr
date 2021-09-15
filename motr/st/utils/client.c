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
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/time.h>
#include <getopt.h>
#include <libgen.h>

#include "lib/string.h"
#include "conf/obj.h"
#include "fid/fid.h"
#include "lib/memory.h"
#include "lib/trace.h"
#include "motr/client.h"
#include "motr/idx.h"
#include "motr/st/utils/helper.h"
#include "motr/client_internal.h"
#include "lib/getopts.h"

enum { CMD_SIZE = 128 };

/* Client parameters */

static struct m0_client          *m0_instance = NULL;
static struct m0_container container;
static struct m0_config    conf;
static struct m0_idx_dix_config   dix_conf;

extern struct m0_addb_ctx m0_addb_ctx;

static void help(void)
{
	m0_console_printf("Help:\n");
	m0_console_printf("touch   OBJ_ID\n");
	m0_console_printf("write   OBJ_ID SRC_FILE BLOCK_SIZE BLOCK_COUNT "
			  "BLOCKS_PER_IO UPDATE_FLAG OFFSET\n");
	m0_console_printf("read    OBJ_ID DEST_FILE BLOCK_SIZE BLOCK_COUNT "
			  "BLOCKS_PER_IO\n");
	m0_console_printf("delete  OBJ_ID\n");
	m0_console_printf("help\n");
	m0_console_printf("quit\n");
}

#define GET_ARG(arg, cmd, saveptr)       \
	arg = strtok_r(cmd, "\n ", saveptr);   \
	if (arg == NULL) {                      \
		help();                         \
		continue;                       \
	}

#define GET_COMMON_ARG(arg, fname, saveptr, id,                  \
		       block_size, block_count, blocks_per_io)   \
	GET_ARG(arg, NULL, &saveptr);                            \
	m0_obj_id_sscanf(arg, &id);                              \
	GET_ARG(arg, NULL, &saveptr);                            \
	fname = strdup(arg);                                     \
	GET_ARG(arg, NULL, &saveptr);                            \
	block_size = atoi(arg);                                  \
	GET_ARG(arg, NULL, &saveptr);                            \
	block_count = atoi(arg);                                 \
	GET_ARG(arg, NULL, &saveptr);                            \
	blocks_per_io = atoi(arg);

static void c0client_usage(FILE *file, char *prog_name)
{
	fprintf(file, "Usage: %s [OPTION]...\n"
"Launches Client client.\n"
"\n"
"Mandatory arguments to long options are mandatory for short options too.\n"
"  -l, --local          ADDR        Local endpoint address\n"
"  -H, --ha             ADDR        HA endpoint address\n"
"  -p, --profile        FID         Profile FID\n"
"  -P, --process        FID         Process FID\n"
"  -L, --layout-id      INT         Layout ID, range: [1-14]\n"
"  -e, --enable-locks               Enables acquiring and releasing RW locks "
				   "before and after performing IO.\n"
"  -b  --blocks-per-io  INT         Number of blocks (>=0) per IO. Default=100 "
				   "if 0 or nothing is provided.\n"
"  -r, --read-verify                Verify parity after reading the data\n"
"  -S, --msg_size       INT         Max RPC msg size 64k i.e 65536\n"
                                   "%*c Note: this should match with m0d's current "
                                   "rpc msg size\n"
"  -q, --min_queue      INT         Minimum length of the receive queue i.e 16\n"
"  -h, --help                       Shows this help text and exit\n"
, prog_name, WIDTH, ' ');
}

int main(int argc, char **argv)
{
	struct m0_utility_param  params;
	struct m0_fid            fid;
	int                      rc;
	char                    *arg;
	char                    *saveptr;
	char                    *cmd;
	char                    *fname = NULL;
	struct m0_uint128        id = M0_ID_APP;
	int                      block_size;
	int                      block_count;
	int                      blocks_per_io;
	uint64_t                 offset;
	bool                     update_flag;

	m0_utility_args_init(argc, argv, &params,
			     &dix_conf, &conf, &c0client_usage);

	rc = client_init(&conf, &container, &m0_instance);
	if (rc < 0) {
		fprintf(stderr, "init failed! rc = %d\n", rc);
		exit(EXIT_FAILURE);
	}

	memset(&fid, 0, sizeof fid);

	cmd = m0_alloc(CMD_SIZE);
	if (cmd == NULL) {
		M0_ERR(-ENOMEM);
		goto cleanup;
	}

	do {
		fflush(stdin);
		offset = 0;
		update_flag = false;
		m0_console_printf("m0client >>");
		arg = fgets(cmd, CMD_SIZE, stdin);
		if (arg == NULL)
			continue;
		GET_ARG(arg, cmd, &saveptr);
		if (strcmp(arg, "read") == 0) {
			GET_COMMON_ARG(arg, fname, saveptr, id,
				       block_size, block_count,
				       blocks_per_io);
			if (*saveptr != 0) {
				GET_ARG(arg, NULL, &saveptr);
				if (m0_bcount_get(arg, &offset) == 0) {
					if (offset % BLK_SIZE_4k != 0) {
						continue;
					}
				}
			}
			if (!entity_id_is_valid(&id))
				continue;
			rc = m0_read(&container, id, fname,
				     block_size, block_count, offset,
				     blocks_per_io,
				     params.cup_take_locks,
				     0);
		} else if (strcmp(arg, "write") == 0) {
			GET_COMMON_ARG(arg, fname, saveptr, id,
				       block_size, block_count,
				       blocks_per_io);
			if (blocks_per_io > M0_MAX_BLOCK_COUNT) {
				fprintf(stderr, "Blocks per IO (%d) is out of "
						"range. Max is %d for write "
						"operation!\n", blocks_per_io,
						 M0_MAX_BLOCK_COUNT);
				continue;
			}
			if (*saveptr != 0) {
				GET_ARG(arg, NULL, &saveptr);
				update_flag = atoi(arg) ? true : false;
			}
			if (*saveptr != 0) {
				GET_ARG(arg, NULL, &saveptr);
				if (m0_bcount_get(arg, &offset) == 0) {
					if (offset % BLK_SIZE_4k != 0) {
						continue;
					}
				}
			}
			if (!entity_id_is_valid(&id))
				continue;
			rc = m0_write(&container, fname, id,
				      block_size, block_count, offset,
				      blocks_per_io, params.cup_take_locks,
				      update_flag);
		} else if (strcmp(arg, "touch") == 0) {
			GET_ARG(arg, NULL, &saveptr);
			m0_obj_id_sscanf(arg, &id);
			if (!entity_id_is_valid(&id))
				continue;
			rc = touch(&container, id,
				   params.cup_take_locks);
		} else if (strcmp(arg, "delete") == 0) {
			GET_ARG(arg, NULL, &saveptr);
			m0_obj_id_sscanf(arg, &id);
			if (!entity_id_is_valid(&id))
				continue;
			rc = m0_unlink(&container, id,
				       params.cup_take_locks);
		} else if (strcmp(arg, "help") == 0)
			help();
		else
			help();
		if (rc < 0) {
			fprintf(stderr, "IO failed! rc = %d\n", rc);
		}
	} while (arg == NULL || strcmp(arg, "quit") != 0);

	m0_free(cmd);
	if (fname != NULL)
		free(fname);

cleanup:
	/* Clean-up */
	client_fini(m0_instance);

	return 0;
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
