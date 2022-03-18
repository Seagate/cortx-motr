/* -*- C -*- */
/*
 * Copyright (c) 2014-2020 Seagate Technology LLC and/or its Affiliates
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
#ifndef __MOTR_STOB_PARTITION_TABLE_H__
#define __MOTR_STOB_PARTITION_TABLE_H__

#include <stdint.h>
#include <string.h>

#include "lib/misc.h"    /* M0_SET0 */
#include "lib/arith.h"   /* M0_3WAY */
#include "lib/errno.h"
#include "lib/assert.h"
#include "lib/memory.h"
#include "lib/string.h"  /* m0_strdup */
#include "lib/types.h"   /* PRIu64 */
#include "motr/magic.h"

#include "format/format.h"  /* m0_format_header*/
#include "ut/helper.h"     /* BE_UT_LOG_ID */
#include "be/domain.h"    /*M0_BE_MAX_PARTITION_USERS */

#define M0_BE_PTABLE_VERSION               (1)
#define M0_BE_PTABLE_DEV_NAME_MAX_SIZE     (128)
#define M0_BE_PTABLE_HDR_VERSION           (1)


enum m0_be_ptable_id {
	/* Free blocks type */
	M0_BE_PTABLE_ENTRY_FREE = (BE_UT_SEG_START_ID - 5),
	/* Partition table type   */
	M0_BE_PTABLE_PARTITION_TABLE = (BE_UT_SEG_START_ID - 4),
	/* Balloc type */
	M0_BE_PTABLE_ENTRY_BALLOC = (BE_UT_SEG_START_ID - 3),
	/* Log type */
	M0_BE_PTABLE_ENTRY_LOG       = (BE_UT_SEG_START_ID - 2),
	/* Segment0 metadata type */
	M0_BE_PTABLE_ENTRY_SEG0      = (BE_UT_SEG_START_ID - 1),
	/* Segment1 metadata type */
	M0_BE_PTABLE_ENTRY_SEG1      = BE_UT_SEG_START_ID
};

/**
  Store partition ID and relevant user offset.
 */
struct m0_be_ptable_pri_part_info
{
	/* Partttion ID to recognize partition name */
	m0_bcount_t ppi_part_id;
	m0_bcount_t ppi_user_off;

};

/**
  Store partition ID and number of chunks need to allocate for
  partition ID. This info will get from upper layer(hare).
 */
struct m0_be_ptable_alloc_info
{
	m0_bcount_t ai_part_id;
	m0_bcount_t ai_def_size_in_chunks;
};

/**
 Partition config will pass the information about partition type
 like number of chunks for each partition, chunk size and total
 chunks of all partitions. This info will get from upper layer(hare).
 */
struct m0_be_ptable_part_config
{
	/* Partition ID and default chunk size */
	struct m0_be_ptable_alloc_info *pc_part_alloc_info;
	/* Number of partition types */
	m0_bcount_t                     pc_num_of_alloc_entries;
	m0_bcount_t                     pc_chunk_size_in_bits;
	/* Total chunks of all partitions */
	m0_bcount_t                     pc_total_chunk_count;
	char                           *pc_dev_path_name;
	m0_bcount_t                     pc_dev_size_in_bytes;
	m0_bcount_t                     pc_key;
};

/**
 Partition table which contains the information of device space allocation for
 the different users (e.g. Log, Seg0, Seg1 and Balloc).
 It will be stored at zero offset of the device.
 */
struct m0_be_ptable_part_table
{
	struct m0_format_header             pt_par_tbl_header;
	m0_bcount_t                         pt_version_info;
	struct m0_be_ptable_alloc_info
		pt_part_alloc_info[M0_BE_MAX_PARTITION_USERS];
	/* Total chunks of all partitions */
	m0_bcount_t                         pt_dev_size_in_chunks;
	m0_bcount_t                         pt_chunk_size_in_bits;
	m0_bcount_t                         pt_key;
	char
		pt_device_path_name[M0_BE_PTABLE_DEV_NAME_MAX_SIZE];
	/* Partition info with ID and user offset */
	struct m0_format_footer             pt_par_tbl_footer;
	struct m0_be_ptable_pri_part_info  *pt_pri_part_info;
};

struct m0_be_ptable_part_tbl_info
{
	/* Total chunks of all partitions */
	m0_bcount_t                              pti_dev_size_in_chunks;
	m0_bcount_t                              pti_chunk_size_in_bits;
	m0_bcount_t                              pti_key;
	char					*pti_dev_pathname;
	struct m0_be_ptable_alloc_info          *pti_part_alloc_info;
	const struct m0_be_ptable_pri_part_info *pti_pri_part_info;

};

M0_INTERNAL int m0_be_ptable_create_init(void *be_domain,
					 bool is_mkfs,
					 struct m0_be_ptable_part_config
					 *part_config);

M0_INTERNAL int m0_be_ptable_get_part_info(struct m0_be_ptable_part_tbl_info
					   *primary_part_info);

#endif /* __MOTR_STOB_PARTITION_TABLE_H__ */

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
