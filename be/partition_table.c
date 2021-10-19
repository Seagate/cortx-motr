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

static int verify_partition_table(struct m0_be_partition_table *partition_table)
{
	int rc = -1;

	if (partition_table != NULL) {
		rc = m0_format_footer_verify(&partition_table->par_tbl_header,
					     true);
	}
	return M0_RC(rc);
}

#if 0 /** unused for now */
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

#endif

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
	struct m0_be_partition_table *partition_table = NULL;
	m0_bcount_t		      f_offset;

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

	partition_table = m0_alloc(primary_partition_size +
				   sizeof(struct m0_be_partition_table));
	if (partition_table == NULL)
	{
		M0_LOG(M0_ERROR, "Failed allocate memory for partition_table");
		rc = -ENOMEM;
		goto out;
	}
	partition_table->pri_part_info = (struct primary_partition_info *)
					((char *)partition_table +
					 offsetof(struct m0_be_partition_table,
						  par_tbl_footer));

#if 0
	M0_ALLOC_ARR(partition_table->pri_part_info,
		     part_config->total_chunk_count);
	if (partition_table->pri_part_info == NULL)
	{
		M0_LOG(M0_ERROR, "Failed to allocate memory for pri_part_info");
		rc = -ENOMEM;
		goto out;
	}
#endif
	if (is_mkfs == true) {

		m0_format_header_pack(&partition_table->par_tbl_header,
				      &(struct m0_format_tag){
			.ot_version = M0_PARTITION_TABLE_HDR_VERSION,
			.ot_type    = M0_PARTITION_TABLE_TYPE,
			.ot_footer_offset = f_offset
		});

		partition_table->version_info = PARTITION_TBL_VERSION;
		partition_table->chunk_size_in_bits =
			part_config->chunk_size_in_bits;

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

		for (p_index = 0;
		     p_index <
		     part_config->no_of_allocation_entries;
		     p_index++) {

			if (part_config->part_alloc_info[p_index].partition_id
				>= M0_PARTITION_ENTRY_MAX) {
				M0_LOG(M0_ERROR, "Invalid partition type");
				rc = -EINVAL;
				goto out;
			}

			/* Always expected part_alloc_info first entry is PT
			 * Is it expected or not? will check with Arc team
			 * if uncommented below block, need to change
 			 * curr_dev_chunk_off value to 0 */
#if 0
			if ((curr_dev_chunk_off == 0) &&
				(part_config->part_alloc_info[p_index].partition_id !=
				 M0_PARTITION_ENTRY_PARTITION_TABLE)) {
					M0_LOG(M0_ERROR, "Invalid partition config");
					rc = -EINVAL;
					goto out;
			}
#endif

			offset_count = 0;
			user_allocation_chunks = part_config->part_alloc_info[p_index].initial_user_allocation_chunks;
			if ((curr_dev_chunk_off + user_allocation_chunks) >
				part_config->total_chunk_count) {
				M0_LOG(M0_ERROR, "Invalid current device chunk offset");
				rc = -EINVAL;
				goto out;
			}
			for (i = 0; i < user_allocation_chunks; i++) {
				partition_table->pri_part_info[curr_dev_chunk_off].partition_id =
					part_config->part_alloc_info[p_index].partition_id;
				partition_table->pri_part_info[curr_dev_chunk_off].user_offset =
					offset_count;
				offset_count++;
				curr_dev_chunk_off++;
			}
		}

		offset_count = 0;
		while (curr_dev_chunk_off <
			part_config->total_chunk_count) {

			partition_table->pri_part_info[curr_dev_chunk_off].partition_id =
				M0_PARTITION_ENTRY_FREE;
			partition_table->pri_part_info[curr_dev_chunk_off].user_offset =
				offset_count;
			offset_count++;
			curr_dev_chunk_off++;
		}

		m0_format_footer_update(partition_table);

		rc = m0_be_io_single(stob, SIO_WRITE, partition_table, 0,
				     offsetof(struct m0_be_partition_table,
					      pri_part_info)
				     + primary_partition_size);
		if (rc != 0) {
			M0_LOG(M0_ERROR, "Failed write partition table "
			       "on disk");
			goto out;
		}
	}
	rc = m0_be_io_single(stob, SIO_READ, partition_table, 0,
			     sizeof(struct m0_be_partition_table) +
			     primary_partition_size);
	if (rc != 0) {
		M0_LOG(M0_ERROR, "Failed read partition table from disk");
		goto out;
	}
	partition_table->pri_part_info = (struct primary_partition_info *)
					((char *)partition_table +
					 offsetof(struct m0_be_partition_table,
						  par_tbl_footer));

	rc = verify_partition_table(partition_table);
	if (rc != 0) {
		M0_LOG(M0_ERROR, "Failed partition table verification");
		goto out;
	}
	return M0_RC(rc);
out:
	if (partition_table)
		m0_free(partition_table);
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
