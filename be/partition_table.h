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

#include "lib/trace.h"
#include "lib/misc.h"    /* M0_SET0 */
#include "lib/arith.h"   /* M0_3WAY */
#include "lib/errno.h"
#include "lib/assert.h"
#include "lib/memory.h"
#include "lib/string.h"  /* m0_strdup */
#include "lib/types.h"   /* PRIu64 */
#include "motr/magic.h"

#include "format/format.h"
#include "be/domain.h"

#define PARTITION_TBL_VERSION               1
#define DEVICE_NAME_MAX_SIZE                128
#define PARTITION_TABLE_DEVICE_CHUNK_OFFSET 0
#define PARTITION_TABLE_USER_OFFSET         0
#define M0_PARTITION_TABLE_HDR_VERSION      1
#define M0_PARTITION_TABLE_TYPE             1

enum m0_partition_name {
	/* Partition table type   */
	M0_PARTITION_ENTRY_PARTITION_TABLE = 0,
	/* Log type */
	M0_PARTITION_ENTRY_LOG,
	/* Segment0 metadata type */
	M0_PARTITION_ENTRY_SEG0,
	/* Segment1 metadata type */
	M0_PARTITION_ENTRY_SEG1,
	/* Balloc type */
	M0_PARTITION_ENTRY_BALLOC,
	/* Free blocks type */
	M0_PARTITION_ENTRY_FREE,
	M0_PARTITION_ENTRY_MAX
};

/**
  Store partition ID and relevant user offset.
 */
struct primary_partition_info
{
	/* Partttion ID to recognize partition name */
	enum m0_partition_name partition_id;
	m0_bcount_t user_offset;

};

/**
  Store partition ID and number of chunks need to allocate for
  partition ID. This info will get from upper layer(hare).
 */
struct m0_partition_allocation_info
{
	enum m0_partition_name partition_id;
	m0_bcount_t initial_user_allocation_chunks;
};

/**
 Partition config will pass the information about partition type
 like number of chunks for each partition, chunk size and total
 chunks of all partitions. This info will get from upper layer(hare).
 */
struct m0_partition_config
{
	/* Partition ID and relevant user offset */
	struct m0_partition_allocation_info *part_alloc_info;
	/* Number of partition types */
	m0_bcount_t no_of_allocation_entries;
	m0_bcount_t chunk_size_in_bits;
	/* Total chunks of all partitions */
	m0_bcount_t total_chunk_count;
	const char *device_path_name;
};

/**
 Partition table which contains the information of device space allocation for
 the different users (e.g. Log, Seg0, Seg1 and Balloc).
 It will be stored at zero offset of the device.
 */
struct m0_be_partition_table
{
	struct m0_format_header par_tbl_header;
	m0_bcount_t version_info;
	/* Total chunks of all partitions */
	m0_bcount_t chunk_count;
	m0_bcount_t chunk_size_in_bits;
	char device_path_name[DEVICE_NAME_MAX_SIZE];
	/* Partition info with ID and user offset */
	struct m0_format_footer par_tbl_footer;
	struct primary_partition_info *pri_part_info;
};

M0_INTERNAL int m0_be_partition_table_create_init(struct m0_be_domain *domain,
						  bool is_mkfs,
						  struct m0_partition_config
						  *part_config);

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
