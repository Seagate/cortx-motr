/*
 * Copyright (c) 2018-2020 Seagate Technology LLC and/or its Affiliates
 * COPYRIGHT 2017-2018 CEA[1] and SAGE partners
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
 * [1]Commissariat a l'energie atomique et aux energies alternatives
 *
 * Original author: Thomas Leibovici <thomas.leibovici@cea.fr>
 */

#include <errno.h>
#include <stdlib.h>
#include <unistd.h>
#include <getopt.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdio.h>
#include <editline/readline.h>

#include "lib/string.h"
#include "m0hsm_api.h"
#include "motr/idx.h"

static const char *RCFILE = ".hsm/config";

/* Client parameters */
static char *local_addr;
static char *ha_addr;
static char *profile;
static char *proc_fid;

static struct m0_client    *instance = NULL;
static struct m0_container container;
static struct m0_realm     uber_realm;
static struct m0_config    client_conf;
static struct m0_idx_dix_config   dix_conf;

struct m0hsm_options hsm_options = {
	.trace_level = LOG_INFO,
	.op_timeout = 10,
};

static void client_fini(void)
{
	m0_client_fini(instance, true);
}

static int client_init(void)
{
	int rc;

	client_conf.mc_is_oostore            = true;
	client_conf.mc_is_read_verify        = false;
	client_conf.mc_local_addr            = local_addr;
	client_conf.mc_ha_addr               = ha_addr;
	client_conf.mc_profile               = profile;
	client_conf.mc_process_fid           = proc_fid;

	/*
	 * Note: max_rpc_msg_size should match server side configuration for
	 * optimal performance.
	 */
	/* TODO Implement runtime configuration to override the defaults:
	 * (M0_NET_TM_RECV_QUEUE_DEF_LEN and M0_RPC_DEF_MAX_RPC_MSG_SIZE).
	 * The following values are tuned for the SAGE prototype cluster: */
	client_conf.mc_tm_recv_queue_min_len = 64;
	client_conf.mc_max_rpc_msg_size      = 65536;

	client_conf.mc_layout_id	     = 0;
	/* using DIX index */
	client_conf.mc_idx_service_id        = M0_IDX_DIX;
	dix_conf.kc_create_meta              = false;
	client_conf.mc_idx_service_conf      = &dix_conf;

	rc = m0_client_init(&instance, &client_conf, true);
	if (rc != 0) {
		fprintf(stderr, "m0_client_init() failed: %d\n", rc);
		return rc;
	}

	/* Initialize root realm */
	m0_container_init(&container, NULL, &M0_UBER_REALM, instance);
	rc = container.co_realm.re_entity.en_sm.sm_rc;
	if (rc != 0) {
		fprintf(stderr, "m0_container_init() failed: %d\n", rc);
		goto out;
	}

	uber_realm = container.co_realm;
out:
	if (rc)
		client_fini();
	return rc;
}

/** Helper to open an object entity */
static int open_entity(struct m0_entity *entity)
{
	struct m0_op *ops[1] = {NULL};
	int rc;

	m0_entity_open(entity, &ops[0]);
	m0_op_launch(ops, 1);
	rc = m0_op_wait(ops[0], M0_BITS(M0_OS_FAILED,
					M0_OS_STABLE),
			m0_time_from_now(hsm_options.op_timeout, 0));
	m0_op_fini(ops[0]);
	m0_op_free(ops[0]);
	ops[0] = NULL;

	return rc;
}

/** wraps m0hsm_pwrite */
static int m0hsm_write_file(struct m0_uint128 id, const char *path)
{
#define IO_SIZE (1024 * 1024)
	struct m0_obj obj;
	void *io_buff;
	ssize_t read_nr;
	off_t off = 0;
	int fd;
	int rc;

	/* Open source file */
	fd = open(path, O_RDONLY);
	if (fd < 0) {
		fprintf(stderr, "open '%s' failed: %s\n", path,
			strerror(errno));
		return -1;
	}

	/* allocate I/O buffer */
	io_buff = malloc(IO_SIZE);
	if (!io_buff) {
		fprintf(stderr, "failed to allocate IO buffer\n");
		return -ENOMEM;
	}

	memset(&obj, 0, sizeof(struct m0_obj));
	m0_obj_init(&obj, &uber_realm, &id, m0_client_layout_id(instance));

	/* open the entity */
	rc = open_entity(&obj.ob_entity);
	if (rc) {
		fprintf(stderr,
			"open object %#"PRIx64":%#"PRIx64" failed: %d\n",
			id.u_hi, id.u_lo, rc);
		goto err;
	}

	while ((read_nr = read(fd, io_buff, IO_SIZE)) > 0) {
		rc = m0hsm_pwrite(&obj, io_buff, read_nr, off);
		if (rc) {
			fprintf(stderr, "error on writting at %zu to "
				"%#"PRIx64":%#"PRIx64": %d\n", (size_t)off,
				id.u_hi, id.u_lo, rc);
			break;
		}

		/* increase offset */
		off += read_nr;
	}

	if (read_nr < 0) {
		fprintf(stderr, "error after reading %zu bytes from '%s': %s\n",
			(size_t)off, path, strerror(errno));
		rc = -1;
	}

	m0_entity_fini(&obj.ob_entity);
err:
	free(io_buff);
	return rc;
}


static int load_env(char **tgt, const char *env)
{
	*tgt = getenv(env);
	if (*tgt == NULL) {
		fprintf(stderr, "%s environment variable is not set\n", env);
		return -1;
	}
	return 0;
}

static int load_client_env()
{
	if (load_env(&local_addr, "CLIENT_LADDR"))
		return -1;
	if (load_env(&ha_addr, "CLIENT_HA_ADDR"))
		return -1;
	if (load_env(&profile, "CLIENT_PROF_OPT"))
		return -1;
	if (load_env(&proc_fid, "CLIENT_PROC_FID"))
		return -1;
	return 0;
}

static void usage()
{
	printf("Usage: m0hsm <action> <fid> [...]\n");
	printf("  actions:\n");
	printf("    create <fid> <tier>\n");
	printf("    show <fid>\n");
	printf("    dump <fid>\n");
	printf("    write <fid> <offset> <len> <seed>\n");
	printf("    write_file <fid> <path>\n");
	printf("    read <fid> <offset> <len>\n");
	printf("    copy <fid> <offset> <len> <src_tier> "
			"<tgt_tier> [options: mv,keep_prev,w2dest]\n");
	printf("    move <fid> <offset> <len> <src_tier> "
			"<tgt_tier> [options: keep_prev,w2dest]\n");
	printf("    stage <fid> <offset> <len> <tgt_tier> "
			"[options: mv,w2dest]\n");
	printf("    archive <fid> <offset> <len> <tgt_tier> "
			"[options: mv,keep_prev,w2dest]\n");
	printf("    release <fid> <offset> <len> <tier> "
			"[options: keep_latest]\n");
	printf("    multi_release <fid> <offset> <len> <max_tier> "
			"[options: keep_latest]\n");
	printf("    set_write_tier <fid> <tier>\n\n");
	printf("  <fid> parameter format is [hi:]lo. "
	                  "(hi == 0 if not specified.)\n");
	printf("  The numbers are read in decimal, hexadecimal "
	                              "(when prefixed with `0x')\n"
	       "  or octal (when prefixed with `0') formats.\n");
}


/* read a 64 bits argument. Interprets 0x as base16, 0 as base 8,
 * others as base 10 */
int64_t read_arg64(const char *s)
{
	long long ll = -1LL;

	if (sscanf(s, "%lli", &ll) != 1)
		return -1LL;

	return ll;
}

/**
 * Read FID in the format [hi:]lo.
 * Return how many numbers are read.
 */
int read_fid(const char *s, struct m0_uint128 *fid)
{
	int res;
	long long hi, lo;

	res = sscanf(s, "%lli:%lli", &hi, &lo);
	if (res == 1) {
		fid->u_hi = 0;
		fid->u_lo = hi;
	} else if (res == 2) {
		fid->u_hi = hi;
		fid->u_lo = lo;
	}

	return res;
}

static int parse_copy_subopt(char *opts, enum hsm_cp_flags *flags)
{
	enum copy_opt {
		OPT_MOVE = 0,
		OPT_KEEP_PREV_VERS = 1,
		OPT_WRITE_TO_DEST = 2,
	};
	char *const options[] = {
		[OPT_MOVE]	     = "mv",
		[OPT_KEEP_PREV_VERS] = "keep_prev",
		[OPT_WRITE_TO_DEST]  = "w2dest",
		NULL,
	};
	char *subopts = opts;
	char *value;

	while (*subopts != '\0') {
		switch (getsubopt(&subopts, options, &value)) {
		case OPT_MOVE:
			*flags |= HSM_MOVE;
			break;

		case OPT_KEEP_PREV_VERS:
			*flags |= HSM_KEEP_OLD_VERS;
			break;

		case OPT_WRITE_TO_DEST:
			*flags |= HSM_WRITE_TO_DEST;
			break;
		default:
			fprintf(stderr, "Unexpected option: %s\n", value);
			return -EINVAL;
		}
	}
	return 0;
}

static int parse_release_subopt(char *opts, enum hsm_rls_flags *flags)
{
	enum release_opt {
		OPT_KEEP_LATEST = 0,
	};
	char *const options[] = {
		[OPT_KEEP_LATEST] = "keep_latest",
		NULL,
	};
	char *subopts = opts;
	char *value;

	while (*subopts != '\0') {
		switch (getsubopt(&subopts, options, &value)) {
		case OPT_KEEP_LATEST:
			*flags |= HSM_KEEP_LATEST;
			break;

		default:
			fprintf(stderr, "Unexpected option: %s\n", value);
			return -EINVAL;
		}
	}
	return 0;
}

static const struct option option_tab[] = {
	{"quiet", no_argument, NULL, 'q'},
	{"verbose", required_argument, NULL, 'v'},
};
#define SHORT_OPT "qv"

static int parse_cmd_options(int argc, char **argv)
{
	int c, option_index = 0;

	while ((c = getopt_long(argc, argv, SHORT_OPT, option_tab,
				&option_index)) != -1) {
		switch (c) {
		case 'q':
			if (hsm_options.trace_level > LOG_NONE)
				hsm_options.trace_level--;
			break;
		case 'v':
			if (hsm_options.trace_level < LOG_DEBUG)
				hsm_options.trace_level++;
			break;
		case ':':
		case '?':
		default:
			return -EINVAL;
		}
	}
	return 0;
}

/* test functions hidden in m0hsm_api */
int m0hsm_test_write(struct m0_uint128 id, off_t offset, size_t len, int seed);
int m0hsm_test_read(struct m0_uint128 id, off_t offset, size_t len);

static int run_cmd(int argc, char **argv)
{
	struct m0_uint128 id;
	const char *action;
	int rc = 0;

	if (m0_streq(argv[optind], "help")) {
		usage();
		return 0;
	}

	/* expect at least cmd <action> <fid> */
	if (optind > argc - 2) {
		usage();
		return -1;
	}

	action = argv[optind];

	optind++;
	id = M0_ID_APP;
	rc = read_fid(argv[optind], &id);
	if (rc <= 0) {
		usage();
		return -1;
	}
	optind++;

	if (m0_streq(action, "create")) {
		int tier;
		struct m0_obj obj;

		if (optind > argc - 1) {
			usage();
			return -1;
		}
		tier = atoi(argv[optind]);
		optind++;

		M0_SET0(&obj);
		/* Create the object */
		rc = m0hsm_create(id, &obj, tier, false);

	} else if (m0_streq(action, "show")) {
		/* get and display the composite layout */
		rc = m0hsm_dump(stdout, id, false);
	} else if (m0_streq(action, "dump")) {
		/* get and display the composite layout */
		rc = m0hsm_dump(stdout, id, true);
	} else if (m0_streq(action, "write")) {
		off_t offset;
		size_t len;
		int seed;

		if (optind > argc - 3) {
			usage();
			return -1;
		}
		offset = read_arg64(argv[optind++]);
		len = read_arg64(argv[optind++]);
		seed = atoi(argv[optind]);

		rc = m0hsm_test_write(id, offset, len, seed);

	} else if (m0_streq(action, "write_file")) {
		const char *path;

		if (optind > argc - 1) {
			usage();
			return -1;
		}
		path = argv[optind];
		optind++;

		rc = m0hsm_write_file(id, path);

	} else if (m0_streq(action, "read")) {
		off_t offset;
		size_t len;

		if (optind > argc - 2) {
			usage();
			return -1;
		}
		offset = read_arg64(argv[optind]);
		optind++;
		len = read_arg64(argv[optind]);
		optind++;

		rc = m0hsm_test_read(id, offset, len);

	} else if (m0_streq(action, "copy") ||
		   m0_streq(action, "move")) {
		off_t offset;
		size_t len;
		int src_tier;
		int tgt_tier;
		enum hsm_cp_flags flags = 0;

		/* at least 4 arguments */
		if (optind > argc - 4) {
			usage();
			return -1;
		}
		offset = read_arg64(argv[optind]);
		optind++;
		len = read_arg64(argv[optind]);
		optind++;
		src_tier = atoi(argv[optind]);
		optind++;
		tgt_tier = atoi(argv[optind]);
		optind++;
		if (src_tier > HSM_TIER_MAX || tgt_tier > HSM_TIER_MAX) {
			fprintf(stderr, "Max tier index: %u\n", HSM_TIER_MAX);
			return -1;
		}
		if (optind < argc)
			if (parse_copy_subopt(argv[optind], &flags))
				 return -1;

		/* force move flag for 'move' action */
		if (m0_streq(action, "move"))
			flags |= HSM_MOVE;

		rc = m0hsm_copy(id, src_tier, tgt_tier, offset, len, flags);

	} else if (m0_streq(action, "stage")) {
		off_t offset;
		size_t len;
		int tgt_tier;
		enum hsm_cp_flags flags = 0;

		/* at least 3 arguments */
		if (optind > argc - 3) {
			usage();
			return -1;
		}
		offset = read_arg64(argv[optind]);
		optind++;
		len = read_arg64(argv[optind]);
		optind++;
		tgt_tier = atoi(argv[optind]);
		optind++;
		if (tgt_tier > HSM_TIER_MAX) {
			fprintf(stderr, "Max tier index: %u\n", HSM_TIER_MAX);
			return -1;
		}
		if (optind < argc)
			if (parse_copy_subopt(argv[optind], &flags))
				 return -1;

		rc = m0hsm_stage(id, tgt_tier, offset, len, flags);

	} else if (m0_streq(action, "archive")) {
		off_t offset;
		size_t len;
		int tgt_tier;
		enum hsm_cp_flags flags = 0;

		/* at least 3 arguments */
		if (optind > argc - 3) {
			usage();
			return -1;
		}
		offset = read_arg64(argv[optind]);
		optind++;
		len = read_arg64(argv[optind]);
		optind++;
		tgt_tier = atoi(argv[optind]);
		optind++;
		if (tgt_tier > HSM_TIER_MAX) {
			fprintf(stderr, "Max tier index: %u\n", HSM_TIER_MAX);
			return -1;
		}
		if (optind < argc)
			if (parse_copy_subopt(argv[optind], &flags))
				 return -1;

		rc = m0hsm_archive(id, tgt_tier, offset, len, flags);

	} else if (m0_streq(action, "release") ||
		   m0_streq(action, "multi_release")) {
		off_t offset;
		size_t len;
		int tier;
		enum hsm_rls_flags flags = 0;

		if (optind > argc - 3) {
			usage();
			return -1;
		}
		offset = read_arg64(argv[optind]);
		optind++;
		len = read_arg64(argv[optind]);
		optind++;
		tier = atoi(argv[optind]);
		optind++;
		if (tier > HSM_TIER_MAX) {
			fprintf(stderr, "Max tier index: %u\n", HSM_TIER_MAX);
			return -1;
		}
		if (optind < argc)
			if (parse_release_subopt(argv[optind], &flags))
				 return -1;

		if (m0_streq(action, "release"))
			/* XXX only handle the case of an extent copied to a lower tier */
			rc = m0hsm_release(id, tier, offset, len, flags);
		else if (m0_streq(action, "multi_release"))
			rc = m0hsm_multi_release(id, tier, offset, len, flags);

	} else if (m0_streq(action, "set_write_tier")) {
		int tier;

		if (optind > argc - 1) {
			usage();
			return -1;
		}
		tier = atoi(argv[optind]);
		optind++;

		rc = m0hsm_set_write_tier(id, tier);

	} else {
		usage();
		return -1;
	}

	return rc;
}

#define SH_TOK_BUFSIZE 64
#define SH_TOK_DELIM " \t\r\n\a"
char **sh_split_line(char *line, int *argc)
{
	int bufsize = SH_TOK_BUFSIZE, position = 0;
	char **tokens = malloc(bufsize * sizeof(char*)), **tt;
	char *token;

	if (!tokens) {
		fprintf(stderr, "m0hsm: allocation error\n");
		return NULL;
	}

	/* set first arg add command name to be getopt compliant */
	tokens[0] = "m0hsm";
	position++;

	token = strtok(line, SH_TOK_DELIM);
	while (token != NULL) {
		tokens[position] = token;
		position++;

		if (position >= bufsize) {
			bufsize += SH_TOK_BUFSIZE;
			tt = realloc(tokens, bufsize * sizeof(char*));
			if (!tt) {
				fprintf(stderr, "m0hsm: allocation error\n");
				goto err;
			}
			tokens = tt;
		}

		token = strtok(NULL, SH_TOK_DELIM);
	}
	tokens[position] = NULL;
	*argc = position;
	return tokens;
 err:
	free(tokens);
	return NULL;
}



static int shell_loop()
{
	char *line;
	int argc;
	char **argv;

	using_history();
	while (1) {
		optind = 1;
		line = readline("m0hsm> ");
		if (!line)
			break;
		if (strlen(line) > 0)
			add_history(line);
		argv = sh_split_line(line, &argc);

		if (argc > 1) {
			if (m0_streq(argv[1], "quit"))
				break;
			run_cmd(argc, argv);
		}

		free(argv);
		free(line);
	}
	return 0;
}

int main(int argc, char **argv)
{
	int rc = -1;
	FILE *rcfile;
	char rcpath[256 + ARRAY_SIZE(RCFILE)];

	snprintf(rcpath, ARRAY_SIZE(rcpath), "%s/%s", getenv("HOME"), RCFILE);
	rcfile = fopen(rcpath, "r");
	if (rcfile == NULL) {
		fprintf(stderr, "m0hsm: error on opening %s file: %s\n",
			rcpath, strerror(errno));
		exit(EXIT_FAILURE);
	}

	if (load_client_env())
		goto out;

	if (parse_cmd_options(argc, argv))
		goto out;

	/* expect at least m0hsm <action|shell> */
	if (optind > argc - 1) {
		usage();
		goto out;
	}

	/* Initialize cloivis */
	rc = client_init();
	if (rc < 0) {
		fprintf(stderr, "m0hsm: error: client_init() failed!\n");
		goto out;
	}

	/* Initialize HSM API */
	hsm_options.log_stream = stderr;
	hsm_options.rcfile     = rcfile;
	rc = m0hsm_init(instance, &uber_realm, &hsm_options);
	if (rc < 0) {
		fprintf(stderr, "m0hsm: error: m0hsm_init() failed!\n");
		goto fini;
	}

	if (m0_streq(argv[optind], "shell")) {
		/* run shell */
		rc = shell_loop();
	} else {
		/* run command */
		rc = run_cmd(argc, argv);
	}
 fini:
	/* terminate */
	client_fini();
 out:
	fclose(rcfile);
	if (rc != 0)
		exit(EXIT_FAILURE);
	exit(EXIT_SUCCESS);
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
