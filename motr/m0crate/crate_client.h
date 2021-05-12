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


#pragma once

#ifndef __MOTR_M0CRATE_CRATE_H__
#define __MOTR_M0CRATE_CRATE_H__

/**
 * @defgroup crate
 *
 * @{
 */

#include "fid/fid.h"
#include "motr/client.h"
#include "motr/m0crate/workload.h"
#include "motr/m0crate/crate_utils.h"

struct crate_conf {
        /* Client parameters */
        bool is_addb_init;
        bool is_oostrore;
        bool is_read_verify;
        char *local_addr;
        char *ha_addr;
        char *prof;
        char *process_fid;
        int layout_id;
        char *index_dir;
        int index_service_id;
        char *cass_cluster_ep;
        char *cass_keyspace;
	int tm_recv_queue_min_len;
	int max_rpc_msg_size;
	int col_family;
	int log_level;
	uint64_t addb_size;
};

enum m0_operation_type {
	OT_INDEX,
	OT_IO,
	OT_BTREE
};

enum cr_opcode {
	CRATE_OP_PUT,
	CRATE_OP_GET,
	CRATE_OP_NEXT,
	CRATE_OP_DEL,
	CRATE_OP_TYPES,
	CRATE_OP_NR = CRATE_OP_TYPES,
	CRATE_OP_START = CRATE_OP_PUT,
	CRATE_OP_INVALID = CRATE_OP_TYPES,
};

struct m0_workload_index {
	struct m0_uint128      *ids;
	int		       *op_status;
	int			num_index;
	int			num_kvs;
	int			mode;
	int			opcode;
	int			opcode_prcnt[CRATE_OP_TYPES];
	int			next_records;

	/** Total count for all operaions.
	 * If op_count == -1, then operations count is unlimited.
	 **/
	int			op_count;

	/** Maximum number of seconds to execute test.
	 * exec_time == -1 means "unlimited".
	 **/
	int			exec_time;

	/** Insert 'warmup_put_cnt' records before test. */
	int			warmup_put_cnt;
	/** Delete every 'warmup_del_ratio' records before test. */
	int			warmup_del_ratio;

	struct m0_fid		key_prefix;
	int			keys_count;

	/** Added to set key_size and value_size parameter from .yaml file. */
	int			key_size;
	int			value_size;
	int			max_key_size;
	int			max_value_size;
	int			min_key_size;

	bool			keys_ordered;

	struct m0_fid		index_fid;

	uint64_t		seed;
};

struct m0_workload_task {
	int                    task_idx;
	int		      *op_status;
	struct m0_obj         *objs;
	struct m0_op         **ops;
	struct timeval        *op_list_time;
	struct m0_thread       mthread;
};

enum m0_operations {
	CR_CREATE,
	CR_OPEN,
	CR_WRITE,
	CR_READ,
	CR_DELETE,
	CR_POPULATE,
	CR_CLEANUP,
	CR_READ_ONLY,
	CR_OPS_NR
};

enum m0_operation_status {
	CR_OP_NEW,
	CR_OP_EXECUTING,
	CR_OP_COMPLETE
};

enum thread_operation {
	CR_WRITE_TO_SAME = 0,
	CR_WRITE_TO_DIFF
};

struct cwi_global {
	struct m0_uint128 cg_oid;
	bool              cg_created;
	int               cg_nr_tasks;
	m0_time_t         cg_cwi_acc_time[CR_OPS_NR];
	struct m0_mutex   cg_mutex;
};

struct m0_workload_io {
	/** Client Workload global context. */
	struct cwi_global cwi_g;
	uint32_t          cwi_layout_id;
	/** IO Block Size */
	uint64_t          cwi_bs;
	/**
	 * Number of blocks per IO operation. (Each thread
	 * can run several IO operations concurrently.)
	 */
	uint32_t          cwi_bcount_per_op;
	struct m0_fid     cwi_pool_id;
	uint64_t          cwi_io_size;
	uint64_t          cwi_ops_done[CR_OPS_NR];
	uint32_t          cwi_max_nr_ops;
	int32_t           cwi_mode;
	int32_t           cwi_nr_objs;
	uint32_t          cwi_rounds;
	bool              cwi_random_io;
	bool              cwi_share_object;
	int32_t	          cwi_opcode;
	struct m0_uint128 cwi_start_obj_id;
	m0_time_t         cwi_start_time;
	m0_time_t         cwi_finish_time;
	m0_time_t         cwi_execution_time;
	m0_time_t         cwi_time[CR_OPS_NR];
	char             *cwi_filename;
};

struct cti_global {
	struct m0_obj obj;
};

struct m0_task_io {
	struct m0_workload_io     *cti_cwi;
	int                        cti_task_idx;
	uint32_t		  *cti_op_status;
	uint32_t		  *cti_op_rcs;
	int32_t                    cti_progress;
	uint64_t                   cti_start_offset;
	struct m0_obj             *cti_objs;
	struct m0_op             **cti_ops;
	uint64_t                   cti_nr_ops;
	uint64_t                   cti_nr_ops_done;
	struct timeval            *cti_op_list_time;
	struct m0_thread          *cti_mthread;
	struct m0_bufvec          *cti_bufvec;
	struct m0_bufvec          *cti_rd_bufvec;
	struct m0_uint128         *cti_ids;
	m0_time_t                  cti_op_acc_time;
	struct cti_global          cti_g;
	/** Limit op_launch to max_nr_ops */
	struct m0_semaphore        cti_max_ops_sem;
};

int parse_crate(int argc, char **argv, struct workload *w);
void run(struct workload *w, struct workload_task *task);
void m0_op_run(struct workload *w, struct workload_task *task,
		   const struct workload_op *op);
void run_index(struct workload *w, struct workload_task *tasks);
void m0_op_run_index(struct workload *w, struct workload_task *task,
			 const struct workload_op *op);


/** @} end of crate group */
#endif /* __MOTR_M0CRATE_CRATE_H__ */

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
