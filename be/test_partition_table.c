/*
 * Copyright (c) 2011-2020 Seagate Technology LLC and/or its Affiliates
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

#define M0_TRACE_SUBSYSTEM M0_TRACE_SUBSYS_BE
#include "lib/trace.h"

#include <stdio.h>
#include <stdint.h>
#include <inttypes.h>
#include <math.h>
#include <stdlib.h>
#include "be/partition_table.h"

#define MAX 1024
#define MAXCOUNT 13

static bool is_test_table_initilized = false;

/*
* The below tables map the user chunk offset to device chunk offset for
* log,seg0,seg1 and balloc
* below example shows an example mapping:
*
*    log_table[user chunk offset] = device chunk offset
*
* Also there are index for these different tables defined.
* */
m0_bcount_t log_table[MAX]={0};
m0_bcount_t log_index=0;
m0_bcount_t seg0_table[MAX]={0};
m0_bcount_t seg0_index=0;
m0_bcount_t seg1_table[MAX]={0};
m0_bcount_t seg1_index=0;
m0_bcount_t balloc_table[MAX]={0};
m0_bcount_t balloc_index=0;
static struct m0_be_primary_part_info pt={0};


static m0_bcount_t calculate_device_offset(m0_bcount_t user_offset_in_bytes,
					   m0_bcount_t chunk_size_in_bits,
					   m0_bcount_t device_chunk_offset)
{
	m0_bcount_t	mask;
	m0_bcount_t	relative_offset_in_given_chunk;
	m0_bcount_t	device_offset_in_bytes;

	/** chunk_size_in_bits = exponent */
	mask = (1 << chunk_size_in_bits) - 1;
	relative_offset_in_given_chunk = user_offset_in_bytes & mask;
	M0_LOG( M0_DEBUG, "\nDEBUG relative offset in the given chunk: %" PRIu64,
		relative_offset_in_given_chunk);
	device_offset_in_bytes = ( device_chunk_offset << chunk_size_in_bits ) +
				relative_offset_in_given_chunk;

	return(device_offset_in_bytes);
}

static int init_parition_table()
{
	m0_bcount_t primary_part_index;
	M0_ENTRY("");


	/**  populate the partition table */
	if ( m0_be_partition_get_part_info(&pt))
		M0_ASSERT(0);

	/**  populate other tables like M0_PARTITION_ENTRY_LOG,
	 * M0_PARTITION_ENTRY_SEG0, M0_PARTITION_ENTRY_SEG1, Balloc*/
	for (primary_part_index = 0;
	     primary_part_index <= pt.chunk_count;
	     primary_part_index++) {
		if (pt.pri_part_info[primary_part_index].partition_id ==
		    M0_PARTITION_ENTRY_LOG)
			log_table[log_index++] = primary_part_index;
		else if (pt.pri_part_info[primary_part_index].partition_id ==
			 M0_PARTITION_ENTRY_SEG0)
			seg0_table[seg0_index++] = primary_part_index;
		else if (pt.pri_part_info[primary_part_index].partition_id ==
			 M0_PARTITION_ENTRY_SEG1)
			seg1_table[seg1_index++] = primary_part_index;
		else if (pt.pri_part_info[primary_part_index].partition_id ==
			 M0_PARTITION_ENTRY_BALLOC)
			balloc_table[balloc_index++] = primary_part_index;
	}
	M0_LEAVE();
	return 0;
}


static m0_bcount_t get_device_chunk_offset(m0_bcount_t user_chunk_offset_index,
				    m0_bcount_t partition_id)
{
	m0_bcount_t device_chunk_offset;
	M0_ENTRY();
	if (partition_id == M0_PARTITION_ENTRY_LOG)
		device_chunk_offset = log_table[user_chunk_offset_index];
	else if (partition_id == M0_PARTITION_ENTRY_SEG0)
		device_chunk_offset = seg0_table[user_chunk_offset_index];
	else if (partition_id == M0_PARTITION_ENTRY_SEG1)
		device_chunk_offset = seg1_table[user_chunk_offset_index];
	else if (partition_id == M0_PARTITION_ENTRY_BALLOC)
		device_chunk_offset = balloc_table[user_chunk_offset_index];
	else
		device_chunk_offset = user_chunk_offset_index;
	M0_ASSERT(device_chunk_offset);
	return(device_chunk_offset);
}

M0_INTERNAL m0_bcount_t get_partition_offset(m0_bcount_t user_offset_in_bytes,
					     m0_bcount_t partition_id)
{
	m0_bcount_t device_chunk_offset;
	m0_bcount_t device_offset_in_bytes;
	m0_bcount_t user_chunk_offset_index;
	if(partition_id ==  M0_PARTITION_ENTRY_PARTITION_TABLE)
		return user_offset_in_bytes;
	if(!is_test_table_initilized){
		if (init_parition_table())
			M0_ASSERT(0);
		is_test_table_initilized = true;
	}

	user_chunk_offset_index =
		(user_offset_in_bytes >> pt.chunk_size_in_bits);
	M0_LOG( M0_DEBUG, "\n\nDEBUG: user_chunk_offset_index :%" PRIu64,
		user_chunk_offset_index);

	device_chunk_offset = get_device_chunk_offset( user_chunk_offset_index,
						       partition_id);

	M0_LOG( M0_DEBUG, "\nDEBUG: device_chunk_offset: %" PRIu64,
		device_chunk_offset);
	device_offset_in_bytes = calculate_device_offset( user_offset_in_bytes,
							  pt.chunk_size_in_bits,
							  device_chunk_offset);

	M0_LOG( M0_DEBUG, "\nDEBUG: device_chunk in bytes: %" PRIu64,
		device_offset_in_bytes);
	return(device_offset_in_bytes);
}


