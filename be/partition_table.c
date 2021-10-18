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

#include "be/m0_be_partition_table.h"

int verify_partition_table(struct m0_be_partition_table *partition_table)
{
	int rc = -1;

	if (partition_table != NULL) {
		rc = m0_format_footer_verify(partition_table->par_tbl_header,
					     true);
	}
	return M0_RC(rc);
}

int get_partition_id(const char *partition_name)
{
	int rc = M0_PARTITION_ENTRY_MAX + 1;
	if (!strcmp("free", partition_name))
		rc = M0_PARTITION_ENTRY_FREE;
	else if (!strcmp("log", partition_name))
		rc = M0_PARTITION_ENTRY_LOG;
	else if (!strcmp("seg0", partition_name))
		rc = M0_PARTITION_ENTRY_SEG0;
	else if (!strcmp("seg1", partition_name))
		rc = M0_PARTITION_ENTRY_SEG1;
	else if (!strcmp("balloc", partition_name))
		rc = M0_PARTITION_ENTRY_BALLOC;
	else if (!strcmp("partition_table", partition_name))
		rc = M0_PARTITION_ENTRY_PARTITION_TABLE;

	return M0_RC(rc);
}

/**
 * Name: m0_be_partition_table_create_init 
 * Description:
 *   This function creates partition table and write partition table 
 *   on the disk. Again it will read the partitition table from the
 *   disk and validate partiton table.
 * Parameters:
 *   domain    	      - (struct) Domain information to open the STOB     
 *   is_mkfs          - (bool) To recognise mkfs
 *   partition_config - (struct) Information about partition type and 
 *   			 number of chunks for each partition
 * Return Value:
 *   SUCCESS  - Zero
 *   FAILED   - Negative value 
 */
int m0_be_partition_table_create_init(struct m0_be_domain *domain, bool is_mkfs,
				      struct m0_partition_config 
				      *partition_config)
{
	int			      rc;
	int 			      partition_id_index;
	int 			      offset_count;
	int			      user_allocation_chunks;
	int			      primary_partition_size;
	int 			      current_device_chunk_offset = 1;
	struct m0_format_tag          partition_tag;
	struct m0_stob		     *stob;
	struct m0_be_partition_table *partition_table = NULL;

	rc = be_domain_stob_open(domain, M0_PARTITION_ENTRY_PARTITION_TABLE,
				 partition_config->device_path_name, &stob,
				 true);
	if (rc != 0) {
		M0_LOG(M0_ERROR, "Failed domain stob open");
		goto out;
	}

	M0_ALLOC_PTR(partition_table);
	if (partition_table == NULL)
	{
		M0_LOG(M0_ERROR, "Failed allocate memory for partition_table");
		rc = -ENOMEM;
		goto out;
	}

	primary_partition_size = (sizeof(struct primary_partition_info) * 
				  partition_config->total_chunk_count);
	M0_ALLOC_ARR(partition_table->pri_part_info,
		     partition_config->total_chunk_count);
	if (partition_table->pri_part_info == NULL)
	{
		M0_LOG(M0_ERROR, "Failed to allocate memory for pri_part_info");
		rc = -ENOMEM;
		goto out;
	}

	if (is_mkfs == true) {
		m0_format_header_pack(&partition_table->partition_table_header,
				      &(struct m0_format_tag){
			.ot_version = M0_PARTITION_TABLE_HDR_VERSION,
			.ot_type    = M0_PARTITION_TABLE_TYPE,
			.ot_footer_offset = offsetof(struct m0_be_partition_table,
					             partition_table_footer)
					             + primary_partition_size
					             - sizeof(struct primary_partition_info) 
		});

		partition_table->version_info = PARTITION_TBL_VERSION;
		partition_table->chunk_size_in_bits = 
			partition_config->chunk_size_in_bits;

		if(strlen(partition_config->device_path_name) > 
			DEVICE_NAME_MAX_SIZE) {
			M0_LOG(M0_ERROR, "Invalid device path name size");
			rc = -EINVAL;
			goto out;
		}
		
		strncpy(partition_table->device_path_name, 
			partition_config->device_path_name,
			DEVICE_NAME_MAX_SIZE);
	
		for (partition_id_index = 0; 
		     partition_id_index < 
		     partition_config->no_of_allocation_entries;
		     partition_id_index++) {
			
			if (partition_config->part_alloc_info[partition_id_index].partition_id 
				>= M0_PARTITION_ENTRY_MAX) {
				M0_LOG(M0_ERROR, "Invalid partition type");
				rc = -EINVAL;
				goto out;
			}

			/* Always expected part_alloc_info first entry is PT
			 * Is it expected or not? will check with Arc team
			 * if uncommented below block, need to change 
 			 * current_device_chunk_offset value to 0 */ 
#if 0			
			if ((current_device_chunk_offset == 0) && 
				(partition_config->part_alloc_info[partition_id_index].partition_id != 
				 M0_PARTITION_ENTRY_PARTITION_TABLE)) {
					M0_LOG(M0_ERROR, "Invalid partition config");
					rc = -EINVAL;
					goto out;
			}
#endif

			offset_count = 0;
			user_allocation_chunks = partition_config->part_alloc_info[partition_id_index].user_allocation_chunks;
			for (int i = 0; i < user_allocation_chunks; i++) {
				partition_table->pri_part_info[current_device_chunk_offset].partition_id = 
					partition_config->part_alloc_info[partition_id_index].partition_id;
				partition_table->pri_part_info[current_device_chunk_offset].user_offset =
					offset_count;
				offset_count++;
				current_device_chunk_offset++; 
			}	
		}
		if (current_device_chunk_offset >
			partition_config->total_chunk_count) {
			M0_LOG(M0_ERROR, "Invalid current device chunk offset");
			rc = -EINVAL;
			goto out;
		}

		offset_count = 0;
		while (current_device_chunk_offset <= 
			partition_config->total_chunk_count) {

			partition_table->pri_part_info[current_device_chunk_offset].partition_id = 
				M0_PARTITION_ENTRY_FREE;
			partition_table->pri_part_info[current_device_chunk_offset].user_offset =
				offset_count;
			offset_count++;
			current_device_chunk_offset++; 
		}

		/* TODO: Allocate fresh memory and copy all data */
		m0_format_footer_update(partition_table);

		rc = m0_be_io_single(stob, SIO_WRITE, partition_table, 0,
				     sizeof(m0_be_partition_table)
				     + primary_partition_size);
		if (rc != 0) {
			M0_LOG(M0_ERROR, "Failed write partition table "
			       "on disk");
			goto out;
		}
	}
	rc = m0_be_io_single(stob, SIO_READ, partition_table, 0,
			     sizeof(m0_be_partition_table) +
			     primary_partition_size);
	if (rc != 0) {
		M0_LOG(M0_ERROR, "Failed read partition table from disk");
		goto out;
	}

	rc = verify_partition_table(partition_table);
	if (rc != 0) {
		M0_LOG(M0_ERROR, "Failed partition table verification");
		goto out;
	}
out:
	if (partition_table)
		m0_free0(partition_table);
	return M0_RC(rc);
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
