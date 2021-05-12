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

#ifndef __MOTR_M0CRATE_WORKLOAD_H__
#define __MOTR_M0CRATE_WORKLOAD_H__

/**
 * @defgroup crate_workload
 *
 * @{
 */

#include <sys/param.h>    /* MAXPATHLEN */
#include "lib/memory.h"
#include "lib/time.h"
#include "lib/bitstring.h"
#include "motr/m0crate/crate_utils.h"

/* used for both file offsets and file sizes. */


/*
 * Fundamental data-types.
 */

/*
 * Workloads.
 */

enum {
        CR_WORKLOAD_MAX = 32,
        CR_CSUM_DEV_MAX = 8
};

enum cr_workload_type {
        CWT_HPCS,   /* HPCS type file creation workload */
        CWT_CSUM,   /* checksumming workload */
	CWT_IO,
	CWT_INDEX,
	CWT_BTREE,  /* Btree Operations */
        CWT_NR
};


enum swab_type {
        ST_NONE = 0,
        ST_32   = 1,
        ST_32W  = 2,
        ST_64   = 3,
        ST_64W  = 4,
        ST_NR
};

/* description of the whole workload */
struct workload {
        enum cr_workload_type  cw_type;
        const char            *cw_name;
        unsigned               cw_ops;
        unsigned               cw_done;
        unsigned               cw_nr_thread;
        unsigned               cw_rstate;
        bcnt_t                 cw_avg;
        bcnt_t                 cw_max;
        bcnt_t                 cw_block;
        char                  *cw_buf;
        int                    cw_progress;
        int                    cw_header;
        int                    cw_oflag;
        int                    cw_usage;
        int                    cw_directio;
        int                    cw_bound;
	int                    cw_log_level;
        char                  *cw_fpattern; /* "/mnt/m0/dir%d/f%d.%d" */
        unsigned               cw_nr_dir;
	short                  cw_read_frac;
        struct timeval         cw_rate;
        pthread_mutex_t        cw_lock;

        union {
		void *cw_io;
		void *cw_index;
		void *cw_btree;
                struct cr_hpcs {
                } cw_hpcs;
                struct cr_csum {
                        int      c_nr_devs;
                        struct csum_dev {
                                char *d_name;
                                char *d_csum_name;
                                int   d_fd;
                                int   d_csum_fd;
                        } c_dev[CR_CSUM_DEV_MAX];
                        int      c_async;
                        unsigned c_blocksize;
                        bcnt_t   c_dev_size;
                        int      c_csum;
                        bcnt_t   c_csum_size;
                        int      c_swab;
                } cw_csum;

        } u;
};

enum csum_op_type {
        COT_READ,
        COT_WRITE,
        COT_NR
};

enum btree_op_type {
	BOT_INSERT,
	BOT_LOOKUP,
	BOT_DELETE,
	BOT_OPS_NR
};

struct cr_btree_key {
	struct m0_bitstring pattern;
	uint64_t	    bkey;
};

struct btree_ops {
	const char *opname;
	int	    prcnt;
	int	    nr_ops;
	uint64_t    key;
	m0_time_t   exec_time;
};

struct cr_workload_btree {
	int	         cwb_key_size;
	int	         cwb_val_size;
	int	         cwb_max_key_size;
	int		 cwb_max_val_size;
	bool	         cwb_keys_ordered; /* Sequential or random workload */
	char	         cwb_pattern; /* Fixed pattern */
	struct btree_ops cwb_bo[BOT_OPS_NR];
	m0_time_t        cwb_start_time;
	m0_time_t        cwb_finish_time;
};

/* description of a single task (thread) executing workload */
struct workload_task {
        struct workload *wt_load;
        unsigned         wt_thread;
        pthread_t        wt_pid;
        bcnt_t           wt_total;
        unsigned         wt_ops;
        union {
                struct task_hpcs {
                        struct timeval   th_open;
                        struct timeval   th_write;
                        int              th_bind;
                } wt_hpcs;
                struct task_csum {
                        struct aiocb    *tc_cb;
                        struct aiocb   **tc_rag;
                        char            *tc_csum_buf;
                        char            *tc_buf;
                } wt_csum;
		void *m0_task;
		// void *index_task;
        } u;
};

/* particular operation from a workload */
struct workload_op {
        bcnt_t                wo_size;
        struct workload_task *wo_task;
        union {
                struct op_hpcs {
                        unsigned oh_dirno;
                        unsigned oh_opno;
                        char     oh_fname[MAXPATHLEN];
                } wo_hpcs;
                struct op_csum {
                        enum csum_op_type  oc_type;
                        bcnt_t             oc_offset;
                } wo_csum;
                struct op_btree {
                        enum btree_op_type  ob_type;
                } wo_btree;
        } u;
};

struct workload_type_ops {
        int (*wto_init)(struct workload *w);
        int (*wto_fini)(struct workload *w);

        void (*wto_run)(struct workload *w, struct workload_task *task);
        void (*wto_op_get)(struct workload *w, struct workload_op *op);
        void (*wto_op_run)(struct workload *w, struct workload_task *task,
			   const struct workload_op *op);
        int  (*wto_parse)(struct workload *w, char ch, const char *optarg);
        void (*wto_check)(struct workload *w);
};
int workload_init(struct workload *w, enum cr_workload_type wtype);
void workload_start(struct workload *w, struct workload_task *task);
void workload_join(struct workload *w, struct workload_task *task);

/** @} end of crate_workload group */
#endif /* __MOTR_M0CRATE_WORKLOAD_H__ */

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
