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

#include "be/partition_table.h"

static bool is_part_table_initilized = false;
static struct m0_be_partition_table *partition_table = NULL;

static struct m0_be_partition_table rd_partition_table;
static int verify_partition_table(struct m0_be_partition_table *partition_table)
{
	int rc = -1;

	if (partition_table != NULL) {
		rc = m0_format_footer_verify(partition_table, true);
	}
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
 *   part_config - (struct) Information about partition type and
 *   			 number of chunks for each partition
 * Return Value:
 *   SUCCESS  - Zero
 *   FAILED   - Negative value
 */
M0_INTERNAL int m0_be_partition_table_create_init(struct m0_be_domain *domain,
						  bool is_mkfs,
						  struct m0_partition_config
						  *part_config)
{
	int			      rc;
	int			      i;
	int			      p_index;
	int			      offset_count;
	int			      user_allocation_chunks;
	int			      primary_partition_size;
	int			      curr_dev_chunk_off = 1;
	struct m0_stob		     *stob;
	m0_bcount_t		      f_offset;
	m0_bcount_t                   pri_part_offset;
	m0_bcount_t		      part_tabl_sz;
	struct primary_partition_info *ptr_primary_part;
	void * temp_partition_table = NULL;

	M0_ENTRY("mkfs = %d", is_mkfs);
	rc = m0_be_domain_stob_open(domain, M0_PARTITION_ENTRY_PARTITION_TABLE,
				 part_config->device_path_name, &stob,
				 true);
	if (rc != 0) {
		M0_LOG(M0_ERROR, "Failed domain stob open");
		goto out;
	}

	primary_partition_size = (sizeof(struct primary_partition_info) *
				  part_config->total_chunk_count);

	f_offset = offsetof(struct m0_be_partition_table,
			    par_tbl_footer);
	f_offset += primary_partition_size;
	part_tabl_sz = f_offset + sizeof(struct m0_format_footer);
	partition_table = m0_alloc(part_tabl_sz);
	if (partition_table == NULL)
	{
		M0_LOG(M0_ERROR, "Failed allocate memory for partition_table");
		rc = -ENOMEM;
		goto out;
	}
	pri_part_offset = offsetof(struct m0_be_partition_table,
				   par_tbl_footer);

	if (is_mkfs == true) {


		partition_table->version_info = PARTITION_TBL_VERSION;
		partition_table->chunk_size_in_bits =
			part_config->chunk_size_in_bits;
		partition_table->chunk_count = part_config->total_chunk_count;
		if(strnlen(part_config->device_path_name,
			   DEVICE_NAME_MAX_SIZE) >=
			DEVICE_NAME_MAX_SIZE) {
			M0_LOG(M0_ERROR, "Invalid device path name size");
			rc = -EINVAL;
			goto out;
		}

		strncpy(partition_table->device_path_name,
			part_config->device_path_name,
			DEVICE_NAME_MAX_SIZE);

		partition_table->pri_part_info =
			(struct primary_partition_info *)
			((char *)partition_table + pri_part_offset);
		ptr_primary_part = partition_table->pri_part_info;
		ptr_primary_part->user_offset = 0;
		ptr_primary_part->partition_id =
			M0_PARTITION_ENTRY_PARTITION_TABLE;
		ptr_primary_part++;
		for (p_index = 0;
		     p_index < part_config->no_of_allocation_entries;
		     p_index++) {
			struct m0_partition_allocation_info *part_info;

			part_info = &part_config->part_alloc_info[p_index];

			offset_count = 0;
			user_allocation_chunks =
				part_info->initial_user_allocation_chunks;
			if ((curr_dev_chunk_off + user_allocation_chunks) >
				part_config->total_chunk_count) {
				M0_LOG(M0_ERROR,
				       "Invalid current device chunk offset");
				rc = -EINVAL;
				goto out;
			}
			for (i = 0; i < user_allocation_chunks; i++) {
				ptr_primary_part->partition_id =
					part_info->partition_id;
				ptr_primary_part->user_offset = offset_count;
				offset_count++;
				curr_dev_chunk_off++;
				ptr_primary_part++;
			}
		}

		offset_count = 0;
		while (curr_dev_chunk_off <
			part_config->total_chunk_count ) {
			ptr_primary_part->partition_id = M0_PARTITION_ENTRY_FREE;
			ptr_primary_part->user_offset = offset_count;
			offset_count++;
			curr_dev_chunk_off++;
		}

		m0_format_header_pack(&partition_table->par_tbl_header,
				      &(struct m0_format_tag){
			.ot_version = M0_PARTITION_TABLE_HDR_VERSION,
			.ot_type    = M0_FORMAT_TYPE_PARTITION_TABLE,
			.ot_footer_offset = f_offset
		});
		m0_format_footer_update(partition_table);

		rc = m0_be_io_single(stob, SIO_WRITE, partition_table, 0,
				     part_tabl_sz);
		if (rc != 0) {
			M0_LOG(M0_ERROR, "Failed write partition table "
			       "on disk");
			goto out;
		}
	}

	rc = m0_be_io_single(stob, SIO_READ, partition_table, 0, part_tabl_sz);
	if (rc != 0) {
		M0_LOG(M0_ERROR, "Failed read partition table from disk");
		goto out;
	}

	rc = verify_partition_table(partition_table);
	if (rc != 0) {
		M0_LOG(M0_ERROR, "Failed partition table verification");
		M0_ERR(rc);
	}
	temp_partition_table = partition_table;
	if(partition_table->chunk_size_in_bits != 30)
		M0_ASSERT(0);
	// copy static info
	memcpy ((void *)&rd_partition_table, temp_partition_table,
		offsetof(struct m0_be_partition_table, par_tbl_footer));
	//copy partition info
	temp_partition_table += offsetof(struct m0_be_partition_table,
					 par_tbl_footer);

	rd_partition_table.pri_part_info = temp_partition_table;
	// copy footer
	temp_partition_table +=  primary_partition_size;
        memcpy((void *)&rd_partition_table.par_tbl_footer,
	       temp_partition_table,
	       sizeof(struct m0_format_footer));
	is_part_table_initilized = true;
	m0_stob_put(stob);
	domain->bd_partition_table = &rd_partition_table;
	M0_LEAVE();
	return M0_RC(rc);
out:
	if (partition_table)
		m0_free(partition_table);
	return M0_RC(rc);
}


M0_INTERNAL int m0_be_partition_get_part_info(struct m0_be_primary_part_info
					      *primary_part_info)
{
	M0_ENTRY();
	if ( is_part_table_initilized &&
	     primary_part_info != NULL) {
		primary_part_info->chunk_count = rd_partition_table.chunk_count;
		primary_part_info->chunk_size_in_bits =
			rd_partition_table.chunk_size_in_bits;
		primary_part_info->pri_part_info =
			rd_partition_table.pri_part_info;
		M0_LOG(M0_ALWAYS,"chunk info : %d, %d, ",
		       (int) primary_part_info->chunk_count,
		       (int)primary_part_info->chunk_size_in_bits);
		return 0;
	}
	else
		return -EAGAIN;

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
