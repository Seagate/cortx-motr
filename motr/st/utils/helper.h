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

#pragma once

#ifndef __MOTR_ST_UTILS_HELPER_H__
#define __MOTR_ST_UTILS_HELPER_H__

#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/time.h>

#include "conf/obj.h"
#include "fid/fid.h"
#include "motr/client.h"
#include "motr/idx.h"

/** Max number of blocks in concurrent IO per thread.
  * Currently Client can write at max 100 blocks in
  * a single request. This will change in future.
  */
enum { M0_MAX_BLOCK_COUNT = 256 };

enum {
	/** Min block size */
	BLK_SIZE_4k = 4096,
	/** Max block size */
	BLK_SIZE_32m = 32 * 1024 * 1024
};

/* It is used for allignment in help messages */
enum { WIDTH = 32 };

struct m0_cc_io_args {
	struct m0_container *cia_container;
	struct m0_uint128    cia_id;
	uint32_t             cia_block_count;
	uint32_t             cia_block_size;
	char               **cia_files;
	int                  cia_index;
};

struct m0_utility_param {
	struct m0_uint128 cup_id;
	uint64_t          cup_block_size;
	uint64_t          cup_block_count;
	int               cup_n_obj;
	uint64_t          cup_offset;
	bool              cup_take_locks;
	uint64_t          cup_trunc_len;
	char             *cup_file;
	int               cup_blks_per_io;
	bool              cup_update_mode;
	struct m0_fid     cup_pver;
	uint32_t          flags;
};

struct m0_copy_mt_args {
	struct m0_utility_param *cma_utility;
	struct m0_uint128       *cma_ids;
	struct m0_mutex          cma_mutex;
	int                     *cma_rc;
	int                      cma_index;
};

struct m0_obj_lock_ops {
	int (*olo_lock_init)(struct m0_obj *obj);

	void (*olo_lock_fini)(struct m0_obj *obj);

	int (*olo_write_lock_get)(struct m0_obj *obj,
				  struct m0_rm_lock_req *req,
				  struct m0_clink *clink);

	int (*olo_write_lock_get_sync)(struct m0_obj *obj,
				       struct m0_rm_lock_req *req);

	int (*olo_read_lock_get)(struct m0_obj *obj,
				 struct m0_rm_lock_req *req,
				 struct m0_clink *clink);

	int (*olo_read_lock_get_sync)(struct m0_obj *obj,
				      struct m0_rm_lock_req *req);

	void (*olo_lock_put)(struct m0_rm_lock_req *req);
};

int client_init(struct m0_config    *config,
	        struct m0_container *container,
	        struct m0_client   **m0_instance);

void client_fini(struct m0_client *m0_instance);

int touch(struct m0_container *container,
	  struct m0_uint128 id, bool take_locks);

int m0_write(struct m0_container *container,
	     char *src, struct m0_uint128 id, uint32_t block_size,
	     uint32_t block_count, uint64_t update_offset, int blks_per_io,
	     bool take_locks, bool update_mode);

int m0_read(struct m0_container *container,
	    struct m0_uint128 id, char *dest, uint32_t block_size,
	    uint32_t block_count, uint64_t offset, int blks_per_io,
	    bool take_locks, uint32_t flags, struct m0_fid *read_pver);

int m0_truncate(struct m0_container *container,
		struct m0_uint128 id, uint32_t block_size,
		uint32_t trunc_count, uint32_t trunc_len, int blks_per_io,
		bool take_locks);

int m0_unlink(struct m0_container *container,
	      struct m0_uint128 id, bool take_locks);
int m0_write_cc(struct m0_container *container,
		char **src, struct m0_uint128 id, int *index,
		uint32_t block_size, uint32_t block_count);

int m0_read_cc(struct m0_container *container,
	       struct m0_uint128 id, char **dest, int *index,
	       uint32_t block_size, uint32_t block_count);


int m0_obj_id_sscanf(char *idstr, struct m0_uint128 *obj_id);

int m0_utility_args_init(int argc, char **argv,
			 struct m0_utility_param *params,
			 struct m0_idx_dix_config *dix_conf,
			 struct m0_config *conf,
			 void (*utility_usage) (FILE*, char*));
#endif /* __MOTR_ST_UTILS_HELPER_H__ */

/*
 *  Local variables:
 *  c-indentation-style: "K&R"
 *  c-basic-offset: 8
 *  tab-width: 8
 *  fill-column: 80
 *  scroll-step: 1
 *  End:
 */
