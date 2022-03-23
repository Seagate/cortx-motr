/* -*- C -*- */
/*
 * Copyright (c) 2016-2020 Seagate Technology LLC and/or its Affiliates
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

#ifndef __MOTR_M0INDEX_H__
#define __MOTR_M0INDEX_H__
#include "lib/vec.h"
#include "fid/fid.h"
#include "common.h"
/**
 * @defgroup client
 *
 * @{
 */

enum {
	CRT,  /* Create index.      */
	DRP,  /* Drop index.        */
	LST,  /* List index.        */
	LKP,  /* Lookup index.      */
	PUT,  /* Put record.        */
	DEL,  /* Delete record.     */
	GET,  /* Get record.        */
	NXT,  /* Next record.       */
	GENF, /* Generate FID-file. */
	GENV, /* Generate VAL-file. */
	WLF,  /* Wait for a file to appear. */
};

enum {
	INDEX_CMD_COUNT = 10,
	MAX_VAL_SIZE    = 500
};

struct index_cmd {
	int               ic_cmd;
	struct m0_fid_arr ic_fids;
	struct m0_bufvec  ic_keys;
	struct m0_bufvec  ic_vals;
	int               ic_cnt;
	int               ic_len;
	char             *ic_filename;
};

struct index_ctx
{
	struct index_cmd   ictx_cmd[INDEX_CMD_COUNT];
	int                ictx_nr;
};

extern bool is_str;
extern bool is_enf_meta;
extern struct m0_fid dix_pool_ver;

int  index_execute(int argc, char** argv);
int  index_init(struct params *params);
void index_fini(void);
void index_usage(void);

/** @} end of client group */
#endif /* __MOTR_M0INDEX_H__ */

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
