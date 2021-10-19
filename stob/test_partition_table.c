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

#include <stdio.h>
#include <stdint.h>
#include <inttypes.h>
#include <math.h>
#include <stdlib.h>
// #include "../be/m0_be_partition_table.h"

#define MAX 1024
#define MAXCOUNT 13
#define M0_DEBUG 1
#define m0_bcount_t uint64_t

enum m0_partition_name {
	/* Partition table type   */
	M0_PARTITION_ENTRY_PARTITION_TABLE = 1,
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

struct m0_partition_allocation_info
{
        enum m0_partition_name partition_id;
        m0_bcount_t user_offset;
};


/*
struct m0_partition_config
{
        struct m0_partition_allocation_info part_alloc_info[MAXCOUNT]; // chunk info table with partition id info;
        
        struct m0_format_header *par_tab_header;
        m0_bcount_t version_info;
        m0_bcount_t total_chunk_count;// Total number chunks
        m0_bcount_t chunk_size_in_bits; // chunk size
        char *device_path_name;
        // struct m0_partition_cfg *par_cfg; // partition config information will get from CDF.yaml

        struct m0_format_footer *par_tab_footer;
};*/

struct m0_partition_config
{
	/* Partition ID and relevant user offset */
	struct m0_partition_allocation_info *part_alloc_info; 
	/* Number of partition types */
	m0_bcount_t no_of_allocation_entries;
	m0_bcount_t chunk_size_in_bits;
	/* Total chunks of all partitions */
	m0_bcount_t total_chunk_count;
	char *device_path_name;
};

struct m0_partition_config pt={0};

/*
* The below tables map the user chunk offset to device chunk offset for log,seg0,seg1 and balloc
* below example shows an example mapping:
*
*    log_table[user chunk offset] = device chunk offset
*
* Also there are index for these different tables defined.
* */
m0_bcount_t log_table[MAX]={0},log_index=0;
m0_bcount_t seg0_table[MAX]={0},seg0_index=0;
m0_bcount_t seg1_table[MAX]={0},seg1_index=0;
m0_bcount_t balloc_table[MAX]={0},balloc_index=0;


void M0_LOG(int, char *str, ...)
{
	printf(str);
}

m0_bcount_t calculate_device_offset(m0_bcount_t user_offset_in_bytes, m0_bcount_t chunk_size_in_bits, m0_bcount_t device_chunk_offset)
{
  m0_bcount_t mask, relative_offset_in_given_chunk, device_offset_in_bytes;

  // chunk_size_in_bits = exponent
  mask = (1 << chunk_size_in_bits) -1 ;
  relative_offset_in_given_chunk = user_offset_in_bytes & mask;
  M0_LOG( M0_DEBUG, "\nDEBUG relative offset in the given chunk: %" PRIu64 , relative_offset_in_given_chunk);
  device_offset_in_bytes = ( device_chunk_offset << chunk_size_in_bits ) + relative_offset_in_given_chunk;

  return(device_offset_in_bytes); 
}

struct m0_partition_config partition_table_get_primary_partition_info()
{
 int user_chunk_offset[MAXCOUNT] = { 0,  0,  0,  0,  1,  2,  0,  1,  2,  3,  4, MAXCOUNT, MAXCOUNT };
 int user_partition_id[MAXCOUNT] = { 1,  2,  4,  3,  3,  3,  5,  5,  5,  3,  5, MAXCOUNT, MAXCOUNT };
 int device_chunk_offset=0;
 
 pt.total_chunk_count=10;
 pt.chunk_size_in_bits=12;

 pt.part_alloc_info = malloc(sizeof(struct m0_partition_allocation_info)*MAXCOUNT);
 if (pt.part_alloc_info == NULL ) {
     M0_LOG(M0_DEBUG, "\nOut of memory\n");
     exit(-1);
 }
 
 for(device_chunk_offset = 0 ; device_chunk_offset < MAXCOUNT; device_chunk_offset++) {
    pt.part_alloc_info[device_chunk_offset].partition_id =  user_partition_id[device_chunk_offset];
    pt.part_alloc_info[device_chunk_offset].user_offset =  user_chunk_offset[device_chunk_offset];
 }
 
 
 M0_LOG(M0_DEBUG, "\nDEBUG:   user_chunk_offset: ");
 for(device_chunk_offset = 0 ; device_chunk_offset < MAXCOUNT; device_chunk_offset++) 
   M0_LOG(M0_DEBUG, "%" PRIu64 ",", user_chunk_offset[device_chunk_offset]);
   
 M0_LOG(M0_DEBUG, "\nDEBUG:        partition id: ");
 for(device_chunk_offset = 0 ; device_chunk_offset < MAXCOUNT; device_chunk_offset++) 
   M0_LOG(M0_DEBUG, "%" PRIu64 ",", user_partition_id[device_chunk_offset]);

 M0_LOG(M0_DEBUG, "\nDEBUG: device_chunk_offset: ");
 for(device_chunk_offset = 0 ; device_chunk_offset < MAXCOUNT; device_chunk_offset++) 
   M0_LOG(M0_DEBUG, "%" PRIu64 ",", device_chunk_offset);
  
 return(pt);
}

void init_parition_table()
{
  struct m0_partition_config pt={0};   // TODO : check venky code line 80
  m0_bcount_t primary_partition_count; 
  
  // populate the partition table 
  pt = partition_table_get_primary_partition_info();

  // populate other tables like M0_PARTITION_ENTRY_LOG, M0_PARTITION_ENTRY_SEG0, M0_PARTITION_ENTRY_SEG1, Balloc.
  for (primary_partition_count = 0; primary_partition_count <= pt.total_chunk_count; primary_partition_count++) {
    if (pt.part_alloc_info[primary_partition_count].partition_id == M0_PARTITION_ENTRY_LOG)
      log_table[log_index++] = primary_partition_count;
    if (pt.part_alloc_info[primary_partition_count].partition_id == M0_PARTITION_ENTRY_SEG0)
      seg0_table[seg0_index++] = primary_partition_count;
    if (pt.part_alloc_info[primary_partition_count].partition_id == M0_PARTITION_ENTRY_SEG1)
      seg1_table[seg1_index++] = primary_partition_count;
    if (pt.part_alloc_info[primary_partition_count].partition_id == M0_PARTITION_ENTRY_BALLOC)
      balloc_table[balloc_index++] = primary_partition_count;
  }

 // chunk_size_in_bits = exponent 
 // chunk_size_in_bits  = (int) log2(pt.chunk_size_in_bits);

 
 // Display Table 
 M0_LOG(M0_DEBUG, "\n\nDEBUG: balloc table:");  // TODO : replace printf with M0_LOG line 86 venky code
 M0_LOG(M0_DEBUG, "\nDEBUG: device_chunk_offset: ");
 for(primary_partition_count= 0 ; primary_partition_count < MAXCOUNT; primary_partition_count++) 
   M0_LOG(M0_DEBUG, "%" PRIu64 ",", balloc_table[primary_partition_count]);

 M0_LOG(M0_DEBUG, "\nDEBUG:   user_chunk_offset: ");
 for(primary_partition_count= 0 ; primary_partition_count < MAXCOUNT; primary_partition_count++) 
   M0_LOG(M0_DEBUG, "%" PRIu64 ",",primary_partition_count);
 
}

m0_bcount_t get_device_chunk_offset(m0_bcount_t user_chunk_offset_index, m0_bcount_t partition_id)
{
  m0_bcount_t device_chunk_offset;
  
  if (partition_id == M0_PARTITION_ENTRY_LOG)
    device_chunk_offset = log_table[user_chunk_offset_index];
  if (partition_id == M0_PARTITION_ENTRY_SEG0)
    device_chunk_offset = seg0_table[user_chunk_offset_index];
  if (partition_id == M0_PARTITION_ENTRY_SEG1)
    device_chunk_offset = seg1_table[user_chunk_offset_index];
  if (partition_id == M0_PARTITION_ENTRY_BALLOC)
    device_chunk_offset = balloc_table[user_chunk_offset_index];
  
  return(device_chunk_offset);
}

m0_bcount_t get_partition_offset(m0_bcount_t user_offset_in_bytes, m0_bcount_t partition_id)
{
  m0_bcount_t actual_address=0,device_chunk_offset,device_offset_in_bytes;
  m0_bcount_t mask, user_chunk_offset_index, relative_offset_in_given_chunk;

  user_chunk_offset_index = (user_offset_in_bytes >> pt.chunk_size_in_bits);
  M0_LOG(M0_DEBUG, "\n\nDEBUG: user_chunk_offset_index :%" PRIu64, user_chunk_offset_index);

  device_chunk_offset = get_device_chunk_offset(user_chunk_offset_index, partition_id);
  
  M0_LOG(M0_DEBUG, "\nDEBUG: device_chunk_offset: %" PRIu64, device_chunk_offset);
  device_offset_in_bytes = calculate_device_offset(user_offset_in_bytes, pt.chunk_size_in_bits, device_chunk_offset);

  return(device_offset_in_bytes);
}


/*
 * Below test program assumes that we have a 16 TB hard disk and
 * We have created N slots of 4 KB each.
 *  
 * 1.M0_PARTITION_ENTRY_PARTITION_TABLE
 * 2.M0_PARTITION_ENTRY_SEG0
 * 3.M0_PARTITION_ENTRY_SEG1
 * 4.M0_PARTITION_ENTRY_LOG
 * 5.M0_PARTITION_ENTRY_BALLOC
 * 6.M0_PARTITION_ENTRY_FREE
 *
 * Primary Partition Info : 
 *
 *      user chunk offset:  0  0  0  0  1  2  0  1  2  3  4  X  ... X
 *      user partition id:  1  2  4  3  3  3  5  5  5  3  5  6  ... 6 
 *    device chunk offset:  0  1  2  3  4  5  6  7  8  9  10 11 ... N-1
 *
 * Partition Stob - Balloc 
 *
 *      device chunk offset:  6  7  8  10  X  ...  K-1
 *        user chunk offset:  0  1  2   3  4  ...  K-1
 *
 * Example inputs and output of the API : 
 *
 *  physical_address = get_partition_offset( 13000, 5)
 *
 *  Then physical_address will be 
 *
 * */


int main(int argc, const char *argv[])
{
  m0_bcount_t device_offset_in_bytes=0;
  
  init_parition_table();
  device_offset_in_bytes = get_partition_offset( 13000, 5); 

  M0_LOG(M0_DEBUG, "\n\nPhysical Address:%" PRIu64 "\n", device_offset_in_bytes);

  return 0;
}

