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

#include <getopt.h>

#if defined(M0_DARWIN)
#include <libgen.h>        /* basename */
#endif

#include "lib/memory.h"    /* m0_free() */
#include "lib/trace.h"
#include "motr/client_internal.h"
#include "motr/st/utils/helper.h"

static struct m0_client        *instance = NULL;
static struct m0_container      container;
static struct m0_config         conf;
static struct m0_idx_dix_config dix_conf;
static struct m0_cc_io_args        writer_args;
static struct m0_cc_io_args        reader_args;

extern struct m0_addb_ctx m0_addb_ctx;

static void writer_thread_launch(struct m0_cc_io_args *args)
{
	m0_write_cc(args->cia_container, args->cia_files,
		    args->cia_id, &args->cia_index, args->cia_block_size,
		    args->cia_block_count, args->entity_flags);
}

static void reader_thread_launch(struct m0_cc_io_args *args)
{
	m0_read_cc(args->cia_container, args->cia_id,
		   args->cia_files, &args->cia_index, args->cia_block_size,
		   args->cia_block_count, args->entity_flags);
}

static void mt_io(struct m0_thread *writer_t,
		  struct m0_cc_io_args writer_args,
		  struct m0_thread *reader_t,
		  struct m0_cc_io_args reader_args,
		  int writer_numb, int reader_numb)
{
	int i;

	for (i = 0; i < writer_numb; ++i) {
		M0_THREAD_INIT(&writer_t[i], struct m0_cc_io_args *, NULL,
			       &writer_thread_launch, &writer_args,
			       "Writer: %d", i);
	}

	for (i = 0; i < reader_numb; ++i) {
		M0_THREAD_INIT(&reader_t[i], struct m0_cc_io_args *, NULL,
			       &reader_thread_launch, &reader_args,
			       "Reader: %d", i);
	}

	for (i = 0; i < writer_numb; ++i) {
		m0_thread_join(&writer_t[i]);
	}

	for (i = 0; i < reader_numb; ++i) {
		m0_thread_join(&reader_t[i]);
	}
}

static void usage(FILE *file, char *prog_name)
{
	fprintf(file, "Usage: %s [OPTION]... SOURCE... DESTINATION...\n"
"Launch multithreaded concurrent Read/Write\n"
"\n"
"Mandatory arguments to long options are mandatory for short options too.\n"
"  -l, --local            ADDR      local endpoint address\n"
"  -H, --ha               ADDR      HA endpoint address\n"
"  -p, --profile          FID       profile FID\n"
"  -P, --process          FID       process FID\n"
"  -o, --object           FID       ID of the motr object\n"
"  -W, --writer_numb      INT       number of writer threads\n"
"  -R, --reader_numb      INT       number of reader threads\n"
"  -s, --block-size       INT       block size multiple of 4k in bytes or " \
				   "with suffix b/k/m/g Ex: 1k=1024, " \
				   "1m=1024*1024\n"
"  -c, --block-count      INT       number of blocks to copy, can give with " \
				   "suffix b/k/m/g/K/M/G. Ex: 1k=1024, " \
				   "1m=1024*1024, 1K=1000 1M=1000*1000\n"
"  -r, --read-verify                verify parity after reading the data\n"
"  -G, --DI-generate                Flag to generate Data Integrity\n"
"  -I, --DI-user-input              Flag to pass checksum by user\n"
"  -h, --help                       shows this help text and exit\n"
, prog_name);
}

int main(int argc, char **argv)
{
	int                rc;
	struct m0_uint128  id = M0_ID_APP;
	struct m0_thread  *writer_t;
	struct m0_thread  *reader_t;
	char             **dest_fnames = NULL;
	char             **src_fnames = NULL;
	uint32_t           block_size = 0;
	uint32_t           block_count = 0;
	int                c;
	int                i;
	int                option_index = 0;
	int                writer_numb = 0;
	int                reader_numb = 0;
	uint32_t           entity_flags = 0;

	static struct option l_opts[] = {
				{"local",        required_argument, NULL, 'l'},
				{"ha",           required_argument, NULL, 'H'},
				{"profile",      required_argument, NULL, 'p'},
				{"process",      required_argument, NULL, 'P'},
				{"object",       required_argument, NULL, 'o'},
				{"writer-numb",  required_argument, NULL, 'W'},
				{"reader-numb",  required_argument, NULL, 'R'},
				{"block-size",   required_argument, NULL, 's'},
				{"block-count",  required_argument, NULL, 'c'},
				{"read-verify",  no_argument,       NULL, 'r'},
				{"DI-generate",  no_argument,       NULL, 'G'},
				{"DI-user-input",no_argument,       NULL, 'I'},
				{"help",         no_argument,       NULL, 'h'},
				{0,              0,                 0,     0 }};

	while ((c = getopt_long(argc, argv, ":l:H:p:P:o:W:R:s:c:rh", l_opts,
		       &option_index)) != -1) {
		switch (c) {
			case 'l': conf.mc_local_addr = optarg;
				  continue;
			case 'H': conf.mc_ha_addr = optarg;
				  continue;
			case 'p': conf.mc_profile = optarg;
				  continue;
			case 'P': conf.mc_process_fid = optarg;
				  continue;
			case 'o': id.u_lo = atoi(optarg);
				  continue;
			case 'W': writer_numb = atoi(optarg);
				  continue;
			case 'R': reader_numb  = atoi(optarg);
				  continue;
			case 's': block_size = atoi(optarg);
				  continue;
			case 'c': block_count = atoi(optarg);
				  continue;
			case 'r': conf.mc_is_read_verify = true;
				  continue;
			case 'G': entity_flags |= M0_ENF_GEN_DI;
				  continue;
			case 'I': entity_flags |= M0_ENF_DI;
				  continue;
			case 'h': usage(stderr, basename(argv[0]));
				  exit(EXIT_FAILURE);
			case '?': fprintf(stderr, "Unsupported option '%c'\n",
					  optopt);
				  usage(stderr, basename(argv[0]));
				  exit(EXIT_FAILURE);
			case ':': fprintf(stderr, "No argument given for '%c'\n",
				          optopt);
				  usage(stderr, basename(argv[0]));
				  exit(EXIT_FAILURE);
			default:  fprintf(stderr, "Unsupported option '%c'\n", c);
		}
	}

	conf.mc_is_oostore            = true;
	conf.mc_tm_recv_queue_min_len = M0_NET_TM_RECV_QUEUE_DEF_LEN;
	conf.mc_max_rpc_msg_size      = M0_RPC_DEF_MAX_RPC_MSG_SIZE;
	conf.mc_idx_service_conf      = &dix_conf;
	dix_conf.kc_create_meta       = false;
	conf.mc_idx_service_id        = M0_IDX_DIX;

	rc = client_init(&conf, &container,
			 &instance);
	if (rc < 0) {
		fprintf(stderr, "init failed! rc = %d\n", rc);
		exit(EXIT_FAILURE);
	}

	M0_ALLOC_ARR(src_fnames, writer_numb);
	M0_ALLOC_ARR(dest_fnames, reader_numb);
	M0_ALLOC_ARR(writer_t, writer_numb);
	M0_ALLOC_ARR(reader_t, reader_numb);

	for (i = 0; i < writer_numb; ++i, ++optind) {
		src_fnames[i] = strdup(argv[optind]);
	}

	for (i = 0; i < reader_numb; ++i, ++optind) {
		dest_fnames[i] = strdup(argv[optind]);
	}

	writer_args.cia_container        = &container;
	writer_args.cia_id               = id;
	writer_args.cia_block_count      = block_count;
	writer_args.cia_block_size       = block_size;
	writer_args.cia_files            = src_fnames;
	writer_args.cia_index            = 0;
	writer_args.entity_flags         = entity_flags;

	reader_args.cia_container = &container;
	reader_args.cia_id               = id;
	reader_args.cia_block_count      = block_count;
	reader_args.cia_block_size       = block_size;
	reader_args.cia_files            = dest_fnames;
	reader_args.cia_index            = 0;
	reader_args.entity_flags         = entity_flags;

	mt_io(writer_t, writer_args, reader_t, reader_args,
	      writer_numb, reader_numb);
	client_fini(instance);
	for (i = 0; i < writer_numb; ++i) {
		m0_free(src_fnames[i]);
		m0_thread_fini(&writer_t[i]);
	}

	for (i = 0; i < reader_numb; ++i) {
		m0_free(dest_fnames[i]);
		m0_thread_fini(&reader_t[i]);
	}

	m0_free(writer_t);
	m0_free(reader_t);
	m0_free(src_fnames);
	m0_free(dest_fnames);
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
