/* -*- C -*- */
/*
 * Copyright (c) 2017-2021 Seagate Technology LLC and/or its Affiliates
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


#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <errno.h>
#include <stdarg.h>

#include "motr/m0crate/parser.h"
#include "motr/m0crate/crate_client.h"

enum cr_kvs_key_len_max {
	MAX_KEY_LEN = 128
};

extern struct workload_type_ops ops;
extern struct workload_type_ops index_ops;
extern struct crate_conf *conf;

enum config_key_val {
	INVALID_OPT = -1,
	LOCAL_ADDR,
	HA_ADDR,
	PROF,
	LAYOUT_ID,
	IS_OOSTORE,
	IS_READ_VERIFY,
	MAX_QUEUE_LEN,
	MAX_RPC_MSG,
	PROCESS_FID,
	IDX_SERVICE_ID,
	CASS_EP,
	CASS_KEYSPACE,
	CASS_COL_FAMILY,
	ADDB_INIT,
	ADDB_SIZE,
	LOG_LEVEL,
	/*
	 * All parameters below are workload-specific,
	 * anything else should be added above this point.
	 * The check for index at copy_value() relies on this.
	 */
	WORKLOAD_TYPE,
	POOL_FID,
	SEED,
	NR_THREADS,
	NUM_OPS,
	NR_OBJS,
	NUM_IDX,
	NUM_KVP,
	PUT,
	GET,
	NEXT,
	DEL,
	NXRECORDS,
	OP_COUNT,
	EXEC_TIME,
	WARMUP_PUT_CNT,
	WARMUP_DEL_RATIO,
	KEY_PREFIX,
	KEY_ORDER,
	KEY_SIZE,
	VALUE_SIZE,
	MAX_KEY_SIZE,
	MAX_VALUE_SIZE,
	INDEX_FID,
	THREAD_OPS,
	BLOCK_SIZE,
	BLOCKS_PER_OP,
	IOSIZE,
	SOURCE_FILE,
	RAND_IO,
	OPCODE,
	START_OBJ_ID,
	MODE,
	MAX_NR_OPS,
	NR_ROUNDS,
	PATTERN,
	INSERT,
	LOOKUP,
	DELETE,
};

struct key_lookup_table {
	char *key;
	enum config_key_val   index;
};

struct key_lookup_table lookuptable[] = {
	{"MOTR_LOCAL_ADDR", LOCAL_ADDR},
	{"MOTR_HA_ADDR", HA_ADDR},
	{"PROF", PROF},
	{"LAYOUT_ID", LAYOUT_ID},
	{"IS_OOSTORE", IS_OOSTORE},
	{"IS_READ_VERIFY", IS_READ_VERIFY},
	{"TM_RECV_QUEUE_MIN_LEN", MAX_QUEUE_LEN},
	{"M0_MAX_RPC_MSG_SIZE", MAX_RPC_MSG},
	{"PROCESS_FID", PROCESS_FID},
	{"IDX_SERVICE_ID", IDX_SERVICE_ID},
	{"CASS_CLUSTER_EP", CASS_EP},
	{"CASS_KEYSPACE", CASS_KEYSPACE},
	{"CASS_MAX_COL_FAMILY_NUM", CASS_COL_FAMILY},
	{"ADDB_INIT", ADDB_INIT},
	{"ADDB_SIZE", ADDB_SIZE},
	{"WORKLOAD_TYPE", WORKLOAD_TYPE},
	{"WORKLOAD_SEED", SEED},
	{"NR_THREADS", NR_THREADS},
	{"OPS", NUM_OPS},
	{"NUM_IDX", NUM_IDX},
	{"NUM_KVP", NUM_KVP},
	{"RECORD_SIZE", VALUE_SIZE},
	{"MAX_RSIZE", MAX_VALUE_SIZE},
	{"GET", GET},
	{"PUT", PUT},
	{"POOL_FID", POOL_FID},
	{"NEXT", NEXT},
	{"DEL", DEL},
	{"NXRECORDS", NXRECORDS},
	{"OP_COUNT", OP_COUNT},
	{"EXEC_TIME", EXEC_TIME},
	{"WARMUP_PUT_CNT", WARMUP_PUT_CNT},
	{"WARMUP_DEL_RATIO", WARMUP_DEL_RATIO},
	{"KEY_PREFIX", KEY_PREFIX},
	{"KEY_ORDER", KEY_ORDER},
	{"KEY_SIZE", KEY_SIZE},
	{"VALUE_SIZE", VALUE_SIZE},
	{"MAX_KEY_SIZE", MAX_KEY_SIZE},
	{"MAX_VALUE_SIZE", MAX_VALUE_SIZE},
	{"INDEX_FID", INDEX_FID},
	{"LOG_LEVEL", LOG_LEVEL},
	{"NR_OBJS", NR_OBJS},
	{"NR_THREADS", NR_THREADS},
	{"THREAD_OPS", THREAD_OPS},
	{"BLOCK_SIZE", BLOCK_SIZE},
	{"BLOCKS_PER_OP", BLOCKS_PER_OP},
	{"IOSIZE", IOSIZE},
	{"SOURCE_FILE", SOURCE_FILE},
	{"RAND_IO", RAND_IO},
	{"OPCODE", OPCODE},
	{"STARTING_OBJ_ID", START_OBJ_ID},
	{"MODE", MODE},
	{"MAX_NR_OPS", MAX_NR_OPS},
	{"NR_ROUNDS", NR_ROUNDS},
	{"PATTERN", PATTERN},
	{"INSERT", INSERT},
	{"LOOKUP", LOOKUP},
	{"DELETE", DELETE}
};

#define NKEYS (sizeof(lookuptable)/sizeof(struct key_lookup_table))

enum config_key_val get_index_from_key(char *key)
{
	int   i;
	char *s1;

	for(i = 0; i < NKEYS; i++) {
		s1 = strstr(key, lookuptable[i].key);
		if (s1 != NULL && !strcmp(key, lookuptable[i].key)) {
			return lookuptable[i].index;
		}
	}
	return INVALID_OPT;
}

const char *get_key_from_index(const enum config_key_val key)
{
	int   i;
	const char *result = NULL;

	for(i = 0; i < NKEYS; i++) {
		if (key == lookuptable[i].index) {
			result = lookuptable[i].key;
			break;
		}
	}

	return result;
}

void parser_emit_error(const char *fmt, ...)
	__attribute__((no_exit))
	__attribute__((format(printf, 1,2)));

void parser_emit_error(const char *fmt, ...)
{
	va_list args;
	va_start(args, fmt);
	vfprintf(stderr, fmt, args);
	fprintf(stderr, "\n");
	va_end(args);
	exit(EXIT_FAILURE);
}

static int parse_int_with_units(const char *value, enum config_key_val tag)
{
	unsigned long long v = getnum(value, get_key_from_index(tag));

	if (v > INT_MAX)
		parser_emit_error("Value overflow detected (value=%s, tag=%s", value, get_key_from_index(tag));

	return v;
}

static int parse_int(const char *value, enum config_key_val tag)
{
	char *endptr;
	long val = 0;

	val = strtol(value, &endptr, 10);

	if ((val == LONG_MAX || val == LONG_MIN) && errno == ERANGE) {
		parser_emit_error("Invalid int value (value '%s'='%s', err: %s).\n", get_key_from_index(tag), value, strerror(errno));
	}

	if (endptr == value) {
		parser_emit_error("Value '%s' is not a number\n", value);
	}

	return val;
}

#define SIZEOF_CWIDX sizeof(struct m0_workload_index)
#define SIZEOF_CWIO sizeof(struct m0_workload_io)
#define SIZEOF_CWBTREE sizeof(struct cr_workload_btree)

#define workload_index(t) (t->u.cw_index)
#define workload_io(t) (t->u.cw_io)
#define workload_btree(t) (t->u.cw_btree)

const char conf_section_name[] = "MOTR_CONFIG";

int copy_value(struct workload *load, int max_workload, int *index,
		char *key, char *value)
{
	int                       value_len = strlen(value);
	struct workload          *w = NULL;
	struct m0_fid            *obj_fid;
	struct m0_workload_io    *cw;
	struct m0_workload_index *ciw;
	struct cr_workload_btree *cbw;
	int			 *key_size;
	int			 *val_size;

	if (m0_streq(value, conf_section_name)) {
		if (conf != NULL) {
			cr_log(CLL_ERROR, "YAML file error: "
			       "more than one config sections\n");
			return -EINVAL;
		}

		conf = m0_alloc(sizeof(struct crate_conf));
		if (conf == NULL)
			return -ENOMEM;
	}
	if (conf == NULL) {
		cr_log(CLL_ERROR, "YAML file error: %s section is missing\n",
		       conf_section_name);
		return -EINVAL;
	}

	if (get_index_from_key(key) > WORKLOAD_TYPE && *index < 0) {
		cr_log(CLL_ERROR, "YAML file error: WORKLOAD_TYPE is missing "
		       "or is not going first in the workload section\n");
		return -EINVAL;
	}

	switch(get_index_from_key(key)) {
		case LOCAL_ADDR:
			conf->local_addr = m0_alloc(value_len + 1);
			if (conf->local_addr == NULL)
				return -ENOMEM;
			strcpy(conf->local_addr, value);
			break;
		case HA_ADDR:
			conf->ha_addr = m0_alloc(value_len + 1);
			if (conf->ha_addr == NULL)
				return -ENOMEM;
			strcpy(conf->ha_addr, value);
			break;
		case PROF:
			conf->prof = m0_alloc(value_len + 1);
			if (conf->prof == NULL)
				return -ENOMEM;
			strcpy(conf->prof, value);
			break;
		case MAX_QUEUE_LEN:
			conf->tm_recv_queue_min_len = atoi(value);
			break;
		case MAX_RPC_MSG:
			conf->max_rpc_msg_size = atoi(value);
			break;
		case PROCESS_FID:
			conf->process_fid = m0_alloc(value_len + 1);
			if (conf->process_fid == NULL)
				return -ENOMEM;
			strcpy(conf->process_fid, value);
			break;
		case LAYOUT_ID:
			conf->layout_id = atoi(value);
			break;
		case IS_OOSTORE:
			conf->is_oostrore = atoi(value);
			break;
		case IS_READ_VERIFY:
			conf->is_read_verify = atoi(value);
			break;
		case IDX_SERVICE_ID:
			conf->index_service_id = atoi(value);
			break;
		case CASS_EP:
			conf->cass_cluster_ep = m0_alloc(value_len + 1);
			if (conf->cass_cluster_ep == NULL)
				return -ENOMEM;
			strcpy(conf->cass_cluster_ep, value);
			break;
		case CASS_KEYSPACE:
			conf->cass_keyspace = m0_alloc(value_len + 1);
			if ( conf->cass_keyspace == NULL)
				return -ENOMEM;

			strcpy(conf->cass_keyspace, value);
			break;
		case CASS_COL_FAMILY:
			conf->col_family = atoi(value);
			break;
		case ADDB_INIT:
			conf->is_addb_init = atoi(value);
			break;
		case ADDB_SIZE:
			conf->addb_size = getnum(value, "addb size");
			break;
		case LOG_LEVEL:
			conf->log_level = parse_int(value, LOG_LEVEL);
			break;
		case WORKLOAD_TYPE:
			(*index)++;
			w = &load[*index];
			if (atoi(value) == OT_INDEX) {
				w->cw_type = CWT_INDEX;
				w->u.cw_index = m0_alloc(SIZEOF_CWIDX);
				if (w->u.cw_io == NULL)
					return -ENOMEM;
			} else if (atoi(value) == OT_IO) {
				w->cw_type = CWT_IO;
				w->u.cw_io = m0_alloc(SIZEOF_CWIO);
				if (w->u.cw_io == NULL)
					return -ENOMEM;
			} else {
				w->cw_type = CWT_BTREE;
				w->u.cw_btree = m0_alloc(SIZEOF_CWBTREE);
				if (w->u.cw_btree == NULL)
					return -ENOMEM;
			}
                        return workload_init(w, w->cw_type);
		case SEED:
			w = &load[*index];
			if (w->cw_type == CWT_IO || w->cw_type == CWT_BTREE) {
				if (strcmp(value, "tstamp"))
					w->cw_rstate = atoi(value);
			} else {
				ciw = workload_index(w);
				if (!strcmp(value, "tstamp"))
					ciw->seed = time(NULL);
				else
					ciw->seed = parse_int(value, SEED);
			}
			break;
		case NR_THREADS:
			w = &load[*index];
			w->cw_nr_thread = atoi(value);
			break;
		case NUM_OPS:
			w = &load[*index];
			w->cw_ops = atoi(value);
			break;
		case NUM_IDX:
			w = &load[*index];
			ciw = workload_index(w);
			ciw->num_index = atoi(value);
			break;
		case NUM_KVP:
			w = &load[*index];
			ciw = workload_index(w);
			ciw->num_kvs = atoi(value);
			break;
		case PUT:
			w = &load[*index];
			ciw = workload_index(w);
			ciw->opcode_prcnt[CRATE_OP_PUT] = parse_int(value, PUT);
			break;
		case GET:
			w = &load[*index];
			ciw = workload_index(w);
			ciw->opcode_prcnt[CRATE_OP_GET] = parse_int(value, GET);
			break;
		case NEXT:
			w = &load[*index];
			ciw = workload_index(w);
			ciw->opcode_prcnt[CRATE_OP_NEXT] = parse_int(value, NEXT);
			break;
		case DEL:
			w = &load[*index];
			ciw = workload_index(w);
			ciw->opcode_prcnt[CRATE_OP_DEL] = parse_int(value, DEL);
			break;
		case NXRECORDS:
			w = &load[*index];
			ciw = workload_index(w);
			if (!strcmp(value, "default"))
				ciw->next_records = -1;
			else
				ciw->next_records = parse_int_with_units(value, NXRECORDS);
			break;
		case OP_COUNT:
			w = &load[*index];
			ciw = workload_index(w);
			if (!strcmp(value, "unlimited"))
				ciw->op_count = -1;
			else
				ciw->op_count = parse_int_with_units(value, OP_COUNT);
			break;
		case EXEC_TIME:
			w = &load[*index];
			if (w->cw_type == CWT_INDEX) {
				ciw = workload_index(w);
				if (!strcmp(value, "unlimited"))
					ciw->exec_time = -1;
				else
					ciw->exec_time = parse_int(value,
							           EXEC_TIME);
			} else {
				cw = workload_io(w);
				if (!strcmp(value, "unlimited"))
					cw->cwi_execution_time = M0_TIME_NEVER;
				else
					cw->cwi_execution_time = m0_time((uint64_t)parse_int(value, EXEC_TIME),
									 (long)0);
			}
			break;
		case KEY_PREFIX:
			w = &load[*index];
			ciw = workload_index(w);
			if (!strcmp(value, "random"))
				ciw->key_prefix.f_container = -1;
			else
			    ciw->key_prefix.f_container = parse_int(value, KEY_PREFIX);
			break;
		case KEY_ORDER:
			w = &load[*index];
			if (w->cw_type == CWT_INDEX) {
				ciw = workload_index(w);
				if (!strcmp(value, "ordered"))
					ciw->keys_ordered = true;
				else if (!strcmp(value, "random"))
					ciw->keys_ordered = false;
				else
					parser_emit_error("Unkown key ordering:"
							  "'%s'", value);
			} else {
				cbw = workload_btree(w);
				if (!strcmp(value, "ordered"))
					cbw->cwb_keys_ordered = true;
				else if (!strcmp(value, "random"))
					cbw->cwb_keys_ordered = false;
				else
					parser_emit_error("Unkown key ordering:"
							  "'%s'", value);
			}
			break;
		case KEY_SIZE:
			w = &load[*index];
			if (w->cw_type == CWT_INDEX) {
				ciw = workload_index(w);
				key_size = &ciw->key_size;
			} else {
				cbw = workload_btree(w);
				key_size = &cbw->cwb_key_size;
			}
			if (strcmp(value, "random") == 0)
				*key_size = -1;
			else
				*key_size = parse_int(value, KEY_SIZE);
			break;
		case VALUE_SIZE:
			w = &load[*index];
			if (w->cw_type == CWT_INDEX) {
				ciw = workload_index(w);
				val_size = &ciw->value_size;
				key_size = &ciw->key_size;
			} else {
				cbw = workload_btree(w);
				val_size = &cbw->cwb_val_size;
				key_size = &cbw->cwb_key_size;
			}
			if (strcmp(value, "random") == 0)
				*val_size = -1;
			else {
				*val_size = parse_int(value, VALUE_SIZE);
				if (strcmp(key, "RECORD_SIZE") == 0) {
					cr_log(CLL_WARN, "RECORD_SIZE is being deprecated, use KEY_SIZE and VALUE_SIZE.\n");
					*val_size = *val_size - *key_size;
				}
			}
			break;
		case MAX_KEY_SIZE:
			w = &load[*index];
			if (w->cw_type == CWT_INDEX) {
				ciw = workload_index(w);
				ciw->max_key_size = parse_int(value,
							      MAX_KEY_SIZE);
			} else {
				cbw = workload_btree(w);
				cbw->cwb_max_key_size = parse_int(value,
								  MAX_KEY_SIZE);
			}
			break;
		case MAX_VALUE_SIZE:
			w = &load[*index];
			if (w->cw_type == CWT_INDEX) {
				ciw = workload_index(w);
				ciw->max_value_size = parse_int(value,
								MAX_VALUE_SIZE);
				if (strcmp(key, "MAX_RSIZE") == 0) {
					cr_log(CLL_WARN, "MAX_RSIZE is being deprecated, use MAX_KEY_SIZE and MAX_VALUE_SIZE.\n");
					ciw->max_value_size = ciw->max_value_size - ciw->max_key_size;
				}
			} else {
				cbw = workload_btree(w);
				cbw->cwb_max_val_size = parse_int(value,
								  MAX_VALUE_SIZE);
			}
			break;
		case INDEX_FID:
			w = &load[*index];
			ciw = workload_index(w);
			if (0 != m0_fid_sscanf(value, &ciw->index_fid)) {
				parser_emit_error("Unable to parse fid: %s", value);
			}
			break;
		case WARMUP_PUT_CNT:
			w = &load[*index];
			ciw = workload_index(w);
			if (!strcmp(value, "all"))
				ciw->warmup_put_cnt = -1;
			else
				ciw->warmup_put_cnt = parse_int(value, WARMUP_PUT_CNT);
			break;
		case WARMUP_DEL_RATIO:
			w = &load[*index];
			ciw = workload_index(w);
			ciw->warmup_del_ratio = parse_int(value, WARMUP_DEL_RATIO);
			break;
		case THREAD_OPS:
			w = &load[*index];
			cw = workload_io(w);
			cw->cwi_share_object = atoi(value);
			break;
		case BLOCK_SIZE:
			w = &load[*index];
			cw = workload_io(w);
			cw->cwi_bs = getnum(value, "block size");
			break;
		case BLOCKS_PER_OP:
			w  = &load[*index];
			cw = workload_io(w);
			cw->cwi_bcount_per_op = atol(value);
			break;
		case NR_OBJS:
			w = &load[*index];
			cw = workload_io(w);
			cw->cwi_nr_objs = atoi(value);
			break;
		case MAX_NR_OPS:
			w = &load[*index];
			cw = workload_io(w);
			cw->cwi_max_nr_ops = atoi(value);
			break;
		case IOSIZE:
			w = &load[*index];
			cw = workload_io(w);
			cw->cwi_io_size = getnum(value, "io size");
			break;
		case SOURCE_FILE:
			w = &load[*index];
			cw = workload_io(w);
			cw->cwi_filename = m0_alloc(value_len + 1);
			strcpy(cw->cwi_filename, value);
			break;
		case RAND_IO:
			w = &load[*index];
			cw = workload_io(w);
			cw->cwi_random_io = atoi(value);
			break;
		case POOL_FID:
			w = &load[*index];
			cw = workload_io(w);
			if (0 != m0_fid_sscanf(value, &cw->cwi_pool_id)) {
				parser_emit_error("Unable to parse fid: %s", value);
			}
			break;
		case OPCODE:
			w = &load[*index];
			cw = workload_io(w);
			cw->cwi_opcode = atoi(value);
			if (conf->layout_id <= 0) {
				cr_log(CLL_ERROR, "LAYOUT_ID is not set\n");
				return -EINVAL;
			}
			cw->cwi_layout_id = conf->layout_id;
			break;
		case START_OBJ_ID:
			w = &load[*index];
			cw = workload_io(w);
			cw->cwi_start_obj_id = M0_ID_APP;
			if (strchr(value, ':') == NULL) {
				cw->cwi_start_obj_id.u_lo = atoi(value);
				break;
			}
			obj_fid = (struct m0_fid *)&cw->cwi_start_obj_id;
			m0_fid_sscanf(value, obj_fid);
			break;
		case MODE:
			w = &load[*index];
			cw = workload_io(w);
			cw->cwi_mode = atoi(value);
			break;
		case NR_ROUNDS:
			w = &load[*index];
			cw = workload_io(w);
			cw->cwi_rounds = atoi(value);
			break;
		case PATTERN:
			w = &load[*index];
			cbw = workload_btree(w);
			cbw->cwb_pattern = parse_int(value, PATTERN);
			break;
		case INSERT:
			w = &load[*index];
			cbw = workload_btree(w);
			cbw->cwb_bo[BOT_INSERT].prcnt = parse_int(value,
								  INSERT);
			break;
		case LOOKUP:
			w = &load[*index];
			cbw = workload_btree(w);
			cbw->cwb_bo[BOT_LOOKUP].prcnt = parse_int(value,
								  LOOKUP);
			break;
		case DELETE:
			w = &load[*index];
			cbw = workload_btree(w);
			cbw->cwb_bo[BOT_DELETE].prcnt = parse_int(value,
								  DELETE);
			break;

		default:
			break;
	}
	return 0;
}

int parse_yaml_file(struct workload *load, int max_workload, int *index,
		    char *config_file)
{
	FILE *fh;
	int   is_key = 0;
	char  key[MAX_KEY_LEN];
	char *scalar_value;
	int   rc;

	yaml_parser_t parser;
	yaml_token_t  token;

	if (!yaml_parser_initialize(&parser)) {
		cr_log(CLL_ERROR, "Failed to initialize parser!\n");
		return -1;
	}

	fh = fopen(config_file, "r");
	if (fh == NULL) {
		cr_log(CLL_ERROR, "Failed to open file!\n");
		yaml_parser_delete(&parser);
		return -1;
	}

	yaml_parser_set_input_file(&parser, fh);

	do {
		rc = 0;
		yaml_parser_scan(&parser, &token);
		switch (token.type) {
			case YAML_KEY_TOKEN:
				is_key = 1;
				break;
			case YAML_VALUE_TOKEN:
				is_key = 0;
				break;
			case YAML_SCALAR_TOKEN:
				scalar_value = (char *)token.data.scalar.value;
				if (is_key) {
					strcpy(key, scalar_value);
				} else {
					rc = copy_value(load, max_workload, index,
							key, scalar_value);
				}
				break;
			case YAML_NO_TOKEN:
				rc = -EINVAL;
				break;
			default:
				break;
		}

		if (rc != 0) {
			cr_log(CLL_ERROR, "Failed to parse %s\n", key);
			yaml_token_delete(&token);
			yaml_parser_delete(&parser);
			fclose(fh);
			return rc;
		}

		if (token.type != YAML_STREAM_END_TOKEN)
			yaml_token_delete(&token);

	} while (token.type != YAML_STREAM_END_TOKEN);

	yaml_token_delete(&token);
	yaml_parser_delete(&parser);
	fclose(fh);
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
/*
 * vim: tabstop=8 shiftwidth=8 noexpandtab textwidth=80 nowrap
 */
