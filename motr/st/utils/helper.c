/* -*- C -*- */
/*
 * Copyright (c) 2019-2020 Seagate Technology LLC and/or its Affiliates
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

#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/time.h>
#include <getopt.h>

#include "lib/trace.h"
#include "conf/obj.h"
#include "fid/fid.h"
#include "motr/client.h"
#include "motr/client_internal.h"
#include "motr/idx.h"
#include "motr/st/utils/helper.h"
#include "lib/getopts.h"
#include "motr/client_internal.h"

extern struct m0_addb_ctx m0_addb_ctx;
enum {
ATTR_SIZE = 16,
};

static int noop_lock_init(struct m0_obj *obj)
{
	/* Do nothing */
	return 0;
}

static void noop_lock_fini(struct m0_obj *obj)
{
	/* Do nothing */
}

static int noop_lock_get(struct m0_obj *obj,
			 struct m0_rm_lock_req *req,
			 struct m0_clink *clink)
{
	/* Do nothing */
	return 0;
}

static int noop_lock_get_sync(struct m0_obj *obj,
			      struct m0_rm_lock_req *req)
{
	/* Do nothing */
	return 0;
}

static void noop_lock_put(struct m0_rm_lock_req *req)
{
	/* Do nothing */
}

const struct m0_obj_lock_ops lock_enabled_ops = {
	.olo_lock_init           = m0_obj_lock_init,
	.olo_lock_fini           = m0_obj_lock_fini,
	.olo_write_lock_get      = m0_obj_write_lock_get,
	.olo_write_lock_get_sync = m0_obj_write_lock_get_sync,
	.olo_read_lock_get       = m0_obj_read_lock_get,
	.olo_read_lock_get_sync  = m0_obj_read_lock_get_sync,
	.olo_lock_put            = m0_obj_lock_put
};

const struct m0_obj_lock_ops lock_disabled_ops = {
	.olo_lock_init           = noop_lock_init,
	.olo_lock_fini           = noop_lock_fini,
	.olo_write_lock_get      = noop_lock_get,
	.olo_write_lock_get_sync = noop_lock_get_sync,
	.olo_read_lock_get       = noop_lock_get,
	.olo_read_lock_get_sync  = noop_lock_get_sync,
	.olo_lock_put            = noop_lock_put
};

static inline uint32_t entity_sm_state(struct m0_obj *obj)
{
	return obj->ob_entity.en_sm.sm_state;
}

static int alloc_vecs(struct m0_indexvec *ext, struct m0_bufvec *data,
		      struct m0_bufvec *attr, uint32_t block_count,
		      uint32_t block_size, uint32_t unit_sz, uint32_t cs_sz)
{
	int      rc;

	rc = m0_indexvec_alloc(ext, block_count);
	if (rc != 0)
		return rc;

	/*
	 * this allocates <block_count> * <block_size>  buffers for data,
	 * and initialises the bufvec for us.
	 */
	rc = m0_bufvec_alloc(data, block_count, block_size);
	if (rc != 0) {
		m0_indexvec_free(ext);
		return rc;
	}
	rc = m0_bufvec_alloc(attr, (block_count * block_size)/unit_sz, cs_sz);
	if (rc != 0) {
		m0_indexvec_free(ext);
		m0_bufvec_free(data);
		return rc;
	}
	return rc;
}

static int write_dummy_hash_data(struct m0_uint128 id, struct m0_bufvec *attr, struct m0_bufvec *data)
{
	int i;
	int nr_unit;
        unsigned char dummy_cksum = 'a';
	M0_ASSERT(data != NULL);
	nr_unit = attr->ov_vec.v_nr;
	for (i = 0; i < nr_unit; ++i) {
		memset(attr->ov_buf[i], dummy_cksum++, ATTR_SIZE);
		attr->ov_vec.v_count[i] = ATTR_SIZE;
	}
	
	return i;
}

static void prepare_ext_vecs(struct m0_indexvec *ext,
			     struct m0_bufvec *attr,
			     uint32_t block_count, uint32_t block_size,
			     uint64_t *last_index)
{
	int      i;

	for (i = 0; i < block_count; ++i) {
		ext->iv_index[i] = *last_index;
		ext->iv_vec.v_count[i] = block_size;
		*last_index += block_size;
	}

	for( i=0; i < attr->ov_vec.v_nr; i++) 
		attr->ov_vec.v_count[i] = ATTR_SIZE;
}

static int alloc_prepare_vecs(struct m0_indexvec *ext,
			      struct m0_bufvec *data,
			      struct m0_bufvec *attr,
			      uint32_t block_count, uint32_t block_size,
			      uint64_t *last_index, uint32_t unit_sz, uint32_t cs_sz)
{
	int      rc;

	rc = alloc_vecs(ext, data, attr, block_count, block_size, unit_sz, cs_sz);
	if (rc == 0) {
		prepare_ext_vecs(ext, attr, block_count,
				 block_size, last_index);
	}
	return rc;
}

static void cleanup_vecs(struct m0_bufvec *data, struct m0_bufvec *attr,
			        struct m0_indexvec *ext)
{
	/* Free bufvec's and indexvec's */
	m0_indexvec_free(ext);
	m0_bufvec_free(data);
	m0_bufvec_free(attr);
}

int client_init(struct m0_config    *config,
	 struct m0_container *container,
	 struct m0_client   **instance)
{
	int rc;

	if (config->mc_local_addr == NULL || config->mc_ha_addr == NULL ||
	    config->mc_profile == NULL || config->mc_process_fid == NULL) {
		rc = M0_ERR(-EINVAL);
		fprintf(stderr, "config parameters not initialized.\n");
		goto err_exit;
	}

	rc = m0_client_init(instance, config, true);
	if (rc != 0)
		goto err_exit;

	m0_container_init(container, NULL, &M0_UBER_REALM, *instance);
	rc = container->co_realm.re_entity.en_sm.sm_rc;

err_exit:
	return rc;
}

void client_fini(struct m0_client *instance)
{
	m0_client_fini(instance, true);
}

int m0_obj_id_sscanf(char *idstr, struct m0_uint128 *obj_id)
{
	int rc;

	if (strchr(idstr, ':') == NULL) {
		obj_id->u_lo = atoi(idstr);
		return 0;
	}

	rc = m0_fid_sscanf(idstr, (struct m0_fid *)obj_id);
	if (rc != 0)
		fprintf(stderr, "can't m0_fid_sscanf() %s, rc:%d", idstr, rc);

	return rc;
}

static int open_entity(struct m0_entity *entity)
{
	int                  rc;
	struct m0_op        *ops[1] = {NULL};

	rc = m0_entity_open(entity, &ops[0]);
	if (rc != 0)
		return M0_ERR(rc);

	m0_op_launch(ops, 1);
	rc = m0_op_wait(ops[0], M0_BITS(M0_OS_FAILED,
					M0_OS_STABLE), M0_TIME_NEVER);
	if (rc == 0)
		rc = m0_rc(ops[0]);

	m0_op_fini(ops[0]);
	m0_op_free(ops[0]);

	return rc;
}

static int create_object(struct m0_entity *entity)
{
	int                  rc;
	struct m0_op        *ops[1] = {NULL};

	rc = m0_entity_create(NULL, entity, &ops[0]);
	if (rc != 0)
		return M0_ERR(rc);

	m0_op_launch(ops, 1);
	rc = m0_op_wait(ops[0], M0_BITS(M0_OS_FAILED,
					M0_OS_STABLE), M0_TIME_NEVER);
	if (rc == 0)
		rc = m0_rc(ops[0]);

	m0_op_fini(ops[0]);
	m0_op_free(ops[0]);

	return rc;
}

static int read_data_from_file(FILE *fp, struct m0_bufvec *data)
{
	int i;
	int rc;
	int nr_blocks;

	nr_blocks = data->ov_vec.v_nr;
	for (i = 0; i < nr_blocks; ++i) {
		rc = fread(data->ov_buf[i], data->ov_vec.v_count[i], 1, fp);
		if (rc != 1 || feof(fp))
			break;
	}

	return i;
}

static int write_data_to_object(struct m0_obj *obj,
				struct m0_indexvec *ext,
				struct m0_bufvec *data,
				struct m0_bufvec *attr)
{
	int                  rc;
	struct m0_op        *ops[1] = {NULL};

	/* Create write operation */
	rc = m0_obj_op(obj, M0_OC_WRITE, ext, data, attr, 0, 0, &ops[0]);
	if (rc != 0)
		return M0_ERR(rc);

	m0_op_launch(ops, 1);
	rc = m0_op_wait(ops[0], M0_BITS(M0_OS_FAILED,
					M0_OS_STABLE), M0_TIME_NEVER);
	if (rc == 0)
		rc = m0_rc(ops[0]);

	m0_op_fini(ops[0]);
	m0_op_free(ops[0]);

	return rc;
}

int touch(struct m0_container *container,
	  struct m0_uint128 id, bool take_locks)
{
	int                           rc = 0;
	struct m0_obj                 obj;
	struct m0_client             *instance;
	struct m0_rm_lock_req         req;
	const struct m0_obj_lock_ops *lock_ops;
	struct m0_clink               clink;

	M0_SET0(&obj);
	M0_SET0(&req);
	lock_ops = take_locks ? &lock_enabled_ops : &lock_disabled_ops;
	instance = container->co_realm.re_instance;
	m0_obj_init(&obj, &container->co_realm, &id,
		    m0_client_layout_id(instance));
	rc = lock_ops->olo_lock_init(&obj);
	if (rc != 0)
		goto init_error;

	m0_clink_init(&clink, NULL);
	clink.cl_is_oneshot = true;
	lock_ops->olo_write_lock_get(&obj, &req, &clink);
	if (take_locks)
		m0_chan_wait(&clink);
	m0_clink_fini(&clink);
	rc = req.rlr_rc;
	if (rc != 0)
		goto get_error;

	rc = create_object(&obj.ob_entity);

	/* fini and release */
	lock_ops->olo_lock_put(&req);
get_error:
	lock_ops->olo_lock_fini(&obj);
init_error:
	m0_entity_fini(&obj.ob_entity);

	return rc;
}

int m0_write(struct m0_container *container, char *src,
	     struct m0_uint128 id, uint32_t block_size,
	     uint32_t block_count, uint64_t update_offset,
	     int blks_per_io, bool take_locks, bool update_mode)
{
	int                           rc;
	struct m0_indexvec            ext;
	struct m0_bufvec              data;
	struct m0_bufvec              attr;
	uint32_t                      bcount;
	uint64_t                      last_index;
	FILE                         *fp;
	struct m0_obj                 obj;
	struct m0_client             *instance;
	struct m0_rm_lock_req         req;
	const struct m0_obj_lock_ops *lock_ops;
	/* Open source file */
	fp = fopen(src, "r");
	if (fp == NULL)
		return -EPERM;
	M0_SET0(&obj);
	lock_ops = take_locks ? &lock_enabled_ops : &lock_disabled_ops;
	instance = container->co_realm.re_instance;
	m0_obj_init(&obj, &container->co_realm, &id,
		    m0_client_layout_id(instance));
	rc = lock_ops->olo_lock_init(&obj);
	if (rc != 0)
		goto init_error;
	rc = lock_ops->olo_write_lock_get_sync(&obj, &req);
	if (rc != 0)
		goto get_error;

	if (update_mode)
		rc = open_entity(&obj.ob_entity);
	else {
		rc = create_object(&obj.ob_entity);
		update_offset = 0;
	}
	if (entity_sm_state(&obj) != M0_ES_OPEN || rc != 0)
		goto cleanup;

	last_index = update_offset;

	if (blks_per_io == 0)
		blks_per_io = M0_MAX_BLOCK_COUNT;

	rc = alloc_vecs(&ext, &data, &attr, blks_per_io, block_size,
					m0_obj_layout_id_to_unit_size(obj.ob_attr.oa_layout_id),
					ATTR_SIZE );
	if (rc != 0)
		goto cleanup;

	while (block_count > 0) {
		bcount = (block_count > blks_per_io)?
			  blks_per_io:block_count;
		if (bcount < blks_per_io) {
			cleanup_vecs(&data, &attr, &ext);
			rc = alloc_vecs(&ext, &data, &attr, bcount,
					block_size, m0_obj_layout_id_to_unit_size(obj.ob_attr.oa_layout_id),
					ATTR_SIZE );
			if (rc != 0)
				goto cleanup;
		}

		prepare_ext_vecs(&ext, &attr, bcount,
				 block_size, &last_index);

		/* Read data from source file. */
		rc = read_data_from_file(fp, &data);
		M0_ASSERT(rc == bcount);
		write_dummy_hash_data(id, &attr, &data);

		/* Copy data to the object*/
		rc = write_data_to_object(&obj, &ext, &data, &attr);
		if (rc != 0) {
			fprintf(stderr, "Writing to object failed!\n");
			break;
		}
		block_count -= bcount;
	}
	cleanup_vecs(&data, &attr, &ext);
	/* fini and release */
cleanup:
	lock_ops->olo_lock_put(&req);
get_error:
	lock_ops->olo_lock_fini(&obj);
init_error:
	m0_entity_fini(&obj.ob_entity);
	fclose(fp);
	return rc;
}

static int read_data_from_object(struct m0_obj *obj,
				 struct m0_indexvec *ext,
				 struct m0_bufvec *data,
				 struct m0_bufvec *attr,
				 uint32_t flags)
{
	int                  rc;
	struct m0_op        *ops[1] = {NULL};

	/* Create read operation */
	rc = m0_obj_op(obj, M0_OC_READ, ext, data, attr, 0, flags, &ops[0]);
	if (rc != 0)
		return M0_ERR(rc);

	m0_op_launch(ops, 1);
	rc = m0_op_wait(ops[0], M0_BITS(M0_OS_FAILED,
					M0_OS_STABLE), M0_TIME_NEVER);
	if (rc == 0)
		rc = m0_rc(ops[0]);

	m0_op_fini(ops[0]);
	m0_op_free(ops[0]);

	return rc;
}

int m0_read(struct m0_container *container,
	    struct m0_uint128 id, char *dest,
	    uint32_t block_size, uint32_t block_count,
	    uint64_t offset, int blks_per_io, bool take_locks,
	    uint32_t flags)
{
	int                           i;
	int                           j;
	int                           rc;
	uint64_t                      last_index = 0;
	struct m0_obj                 obj;
	struct m0_indexvec            ext;
	struct m0_bufvec              data;
	struct m0_bufvec              attr;
	FILE                         *fp = NULL;
	struct m0_client             *instance;
	struct m0_rm_lock_req         req;
	uint32_t                      bcount;
	const struct m0_obj_lock_ops *lock_ops;
	uint64_t                      bytes_read;
	
	lock_ops = take_locks ? &lock_enabled_ops : &lock_disabled_ops;

	/* If input file is not given, write to stdout */
	if (dest != NULL) {
		fp = fopen(dest, "w");
		if (fp == NULL)
			return -EPERM;
	}
	instance = container->co_realm.re_instance;

	/* Read the requisite number of blocks from the entity */
	M0_SET0(&obj);
	m0_obj_init(&obj, &container->co_realm, &id,
		    m0_client_layout_id(instance));
	rc = lock_ops->olo_lock_init(&obj);
	if (rc != 0)
		goto init_error;
	rc = lock_ops->olo_read_lock_get_sync(&obj, &req);
	if (rc != 0)
		goto get_error;

	rc = open_entity(&obj.ob_entity);
	if (entity_sm_state(&obj) != M0_ES_OPEN || rc != 0)
		goto cleanup;

	last_index = offset;

	if (blks_per_io == 0)
		blks_per_io = M0_MAX_BLOCK_COUNT;
	
	rc = alloc_vecs(&ext, &data, &attr, blks_per_io, block_size,
					m0_obj_layout_id_to_unit_size(obj.ob_attr.oa_layout_id), ATTR_SIZE );
	if (rc != 0)
		goto cleanup;
	
	while (block_count > 0) {
		bytes_read = 0;
		bcount = (block_count > blks_per_io) ?
			  blks_per_io : block_count;
		if (bcount < blks_per_io) {
			cleanup_vecs(&data, &attr, &ext);
			rc = alloc_vecs(&ext, &data, &attr, bcount,
					block_size, m0_obj_layout_id_to_unit_size(obj.ob_attr.oa_layout_id), ATTR_SIZE );			
			if (rc != 0)
				goto cleanup;
		}
		prepare_ext_vecs(&ext, &attr, bcount,
				 block_size, &last_index);

		rc = read_data_from_object(&obj, &ext, &data, &attr, flags);
		if (rc != 0) {
			fprintf(stderr, "Reading from object failed!\n");
			break;
		}

		if (fp != NULL) {
			for (i = 0; i < bcount; ++i) {
				bytes_read += fwrite(data.ov_buf[i], sizeof(char),
					    data.ov_vec.v_count[i], fp);
			}
			if (bytes_read != bcount * block_size) {
				rc = -EIO;
				fprintf(stderr, "Writing to destination "
					"file failed!\n");
				break;
			}
		} else {
			/* putchar the output */
			for (i = 0; i < bcount; ++i) {
				for (j = 0; j < data.ov_vec.v_count[i]; ++j)
					putchar(((char *)data.ov_buf[i])[j]);
			}
		}
		block_count -= bcount;
	}

	cleanup_vecs(&data, &attr, &ext);

cleanup:
	if (fp != NULL) {
		fclose(fp);
	}
	lock_ops->olo_lock_put(&req);
get_error:
	lock_ops->olo_lock_fini(&obj);
init_error:
	m0_entity_fini(&obj.ob_entity);
	return rc;
}

static int punch_data_from_object(struct m0_obj *obj,
				  struct m0_indexvec *ext)
{
	int                  rc;
	struct m0_op        *ops[1] = {NULL};

	/* Create free operation */
	rc = m0_obj_op(obj, M0_OC_FREE, ext, NULL, NULL, 0, 0, &ops[0]);
	if (rc != 0)
		return M0_ERR(rc);

	m0_op_launch(ops, 1);
	rc = m0_op_wait(ops[0], M0_BITS(M0_OS_FAILED,
					M0_OS_STABLE), M0_TIME_NEVER);
	if (rc == 0)
		rc = m0_rc(ops[0]);

	m0_op_fini(ops[0]);
	m0_op_free(ops[0]);

	return rc;
}

int m0_truncate(struct m0_container *container,
		struct m0_uint128 id, uint32_t block_size,
		uint32_t trunc_count, uint32_t trunc_len, int blks_per_io,
		bool take_locks)
{
	int                           i;
	int                           rc;
	struct m0_obj                 obj;
	uint64_t                      last_index;
	struct m0_indexvec            ext;
	struct m0_client             *instance;
	struct m0_rm_lock_req         req;
	uint32_t                      bcount;
	const struct m0_obj_lock_ops *lock_ops;

	lock_ops = take_locks ? &lock_enabled_ops : &lock_disabled_ops;

	instance = container->co_realm.re_instance;
	/* Read the requisite number of blocks from the entity */
	M0_SET0(&obj);
	m0_obj_init(&obj, &container->co_realm, &id,
		    m0_client_layout_id(instance));

	rc = lock_ops->olo_lock_init(&obj);
	if (rc != 0)
		goto init_error;
	rc = lock_ops->olo_write_lock_get_sync(&obj, &req);
	if (rc != 0)
		goto get_error;
	rc = open_entity(&obj.ob_entity);
	if (entity_sm_state(&obj) != M0_ES_OPEN || rc != 0)
		goto open_entity_error;

	if (blks_per_io == 0)
		blks_per_io = M0_MAX_BLOCK_COUNT;

	rc = m0_indexvec_alloc(&ext, blks_per_io);
	if (rc != 0)
		goto open_entity_error;

	last_index = trunc_count * block_size;
	while (trunc_len > 0) {
		bcount = (trunc_len > blks_per_io) ?
			  blks_per_io : trunc_len;

		if (bcount < blks_per_io) {
			m0_indexvec_free(&ext);
			rc = m0_indexvec_alloc(&ext, bcount);
			if (rc != 0)
				goto open_entity_error;
		}
		for (i = 0; i < bcount; ++i) {
			ext.iv_index[i] = last_index;
			ext.iv_vec.v_count[i] = block_size;
			last_index += block_size;
		}
		rc = punch_data_from_object(&obj, &ext);
		if (rc != 0) {
			fprintf(stderr, "Truncate failed!\n");
			break;
		}
		trunc_len -= bcount;
	}

	/*To free last allocated buff*/
	m0_indexvec_free(&ext);

open_entity_error:
	lock_ops->olo_lock_put(&req);
get_error:
	lock_ops->olo_lock_fini(&obj);
init_error:
	m0_entity_fini(&obj.ob_entity);
	return rc;
}

int m0_unlink(struct m0_container *container,
	      struct m0_uint128 id, bool take_locks)
{
	int                           rc;
	struct m0_op                 *ops[1] = {NULL};
	struct m0_obj                 obj;
	struct m0_client             *instance;
	struct m0_rm_lock_req         req;
	const struct m0_obj_lock_ops *lock_ops;

	instance = container->co_realm.re_instance;
	lock_ops = take_locks ? &lock_enabled_ops : &lock_disabled_ops;

	/* Delete an entity */
	M0_SET0(&obj);
	m0_obj_init(&obj, &container->co_realm, &id,
		    m0_client_layout_id(instance));

	rc = lock_ops->olo_lock_init(&obj);
	if (rc != 0)
		goto init_error;
	rc = lock_ops->olo_write_lock_get_sync(&obj, &req);
	if (rc != 0)
		goto get_error;

	rc = open_entity(&obj.ob_entity);
	if (entity_sm_state(&obj) != M0_ES_OPEN || rc != 0)
		goto open_entity_error;

	m0_entity_delete(&obj.ob_entity, &ops[0]);

	m0_op_launch(ops, 1);
	rc = m0_op_wait(ops[0], M0_BITS(M0_OS_FAILED,
					M0_OS_STABLE), M0_TIME_NEVER);
	if (rc == 0)
		rc = m0_rc(ops[0]);

	m0_op_fini(ops[0]);
	m0_op_free(ops[0]);
open_entity_error:
	lock_ops->olo_lock_put(&req);
get_error:
	lock_ops->olo_lock_fini(&obj);
init_error:
	m0_entity_fini(&obj.ob_entity);

	return rc;
}

/*
 * XXX:The following functions are used by cc_cp_cat.c
 *     to perform concurrent IO.
 *     An array of files are passed and the threads use the files in the
 *     order in which they get the locks.
 * TODO:Reduce code duplication caused by the following functions.
 *      Find a way to incorporate the functionalities of the following
 *      functions, into existing helper functions.
 */
int m0_write_cc(struct m0_container *container,
		char **src, struct m0_uint128 id, int *index,
		uint32_t block_size, uint32_t block_count)
{
	int                    rc;
	struct m0_indexvec     ext;
	struct m0_bufvec       data;
	struct m0_bufvec       attr;
	uint32_t               bcount;
	uint64_t               last_index;
	FILE                  *fp;
	struct m0_obj          obj;
	struct m0_client      *instance;
	struct m0_rm_lock_req  req;

	M0_SET0(&obj);
	instance = container->co_realm.re_instance;
	m0_obj_init(&obj, &container->co_realm, &id,
		    m0_client_layout_id(instance));

	rc = m0_obj_lock_init(&obj);
	if (rc != 0)
		goto init_error;

	rc = m0_obj_write_lock_get_sync(&obj, &req);
	if (rc != 0)
		goto get_error;

	fp = fopen(src[(*index)++], "r");
	if (fp == NULL) {
		rc = -EPERM;
		goto file_error;
	}

	rc = create_object(&obj.ob_entity);
	if (rc != 0)
		goto cleanup;

	last_index = 0;
	while (block_count > 0) {
		bcount = (block_count > M0_MAX_BLOCK_COUNT) ?
			  M0_MAX_BLOCK_COUNT : block_count;
		rc = alloc_prepare_vecs(&ext, &data, &attr, bcount,
					       block_size, &last_index, m0_obj_layout_id_to_unit_size(obj.ob_attr.oa_layout_id),
					       ATTR_SIZE );
		if (rc != 0)
			goto cleanup;

		/* Read data from source file. */
		rc = read_data_from_file(fp, &data);
		M0_ASSERT(rc == bcount);

		/* Copy data to the object*/
		rc = write_data_to_object(&obj, &ext, &data, &attr);
		if (rc != 0) {
			fprintf(stderr, "Writing to object failed!\n");
			cleanup_vecs(&data, &attr, &ext);
			goto cleanup;
		}
		cleanup_vecs(&data, &attr, &ext);
		block_count -= bcount;
	}
cleanup:
	fclose(fp);
file_error:
	m0_obj_lock_put(&req);
get_error:
	m0_obj_lock_fini(&obj);
init_error:
	m0_entity_fini(&obj.ob_entity);

	return rc;
}

int m0_read_cc(struct m0_container *container,
	       struct m0_uint128 id, char **dest, int *index,
	       uint32_t block_size, uint32_t block_count)
{
	int                           i;
	int                           j;
	int                           rc;
	uint64_t                      last_index = 0;
	struct m0_op                 *ops[1] = {NULL};
	struct m0_obj                 obj;
	struct m0_indexvec            ext;
	struct m0_bufvec              data;
	struct m0_bufvec              attr;
	FILE                         *fp = NULL;
	struct m0_client             *instance;
	struct m0_rm_lock_req  req;

	rc = alloc_prepare_vecs(&ext, &data, &attr, block_count,
				       block_size, &last_index, m0_obj_layout_id_to_unit_size(obj.ob_attr.oa_layout_id),
					   ATTR_SIZE );
	if (rc != 0)
		return rc;
	instance = container->co_realm.re_instance;

	/* Read the requisite number of blocks from the entity */
	M0_SET0(&obj);
	m0_obj_init(&obj, &container->co_realm, &id,
			   m0_client_layout_id(instance));

	rc = m0_obj_lock_init(&obj);
	if (rc != 0)
		goto init_error;

	rc = m0_obj_read_lock_get_sync(&obj, &req);
	if (rc != 0)
		goto get_error;

	rc = open_entity(&obj.ob_entity);
	if (entity_sm_state(&obj) != M0_ES_OPEN || rc != 0) {
		m0_obj_lock_put(&req);
		goto get_error;
	}

	/* Create read operation */
	rc = m0_obj_op(&obj, M0_OC_READ, &ext, &data, &attr, 0, 0, &ops[0]);
	if (rc != 0) {
		m0_obj_lock_put(&req);
		goto get_error;
	}

	m0_op_launch(ops, 1);
	rc = m0_op_wait(ops[0], M0_BITS(M0_OS_FAILED,
					M0_OS_STABLE), M0_TIME_NEVER);
	if (rc == 0)
		rc = m0_rc(ops[0]);

	m0_obj_lock_put(&req);

	if (rc != 0)
		goto cleanup;

	if (dest != NULL) {
		fp = fopen(dest[(*index)++], "w");
		if (fp == NULL) {
			rc = -EPERM;
			goto cleanup;
		}
		for (i = 0; i < block_count; ++i) {
			rc = fwrite(data.ov_buf[i], sizeof(char),
				    data.ov_vec.v_count[i], fp);
		}
		fclose(fp);
		if (rc != block_count) {
			rc = -1;
			goto cleanup;
		}
	} else {
		/* putchar the output */
		for (i = 0; i < block_count; ++i) {
			for (j = 0; j < data.ov_vec.v_count[i]; ++j)
				putchar(((char *)data.ov_buf[i])[j]);
		}
	}

cleanup:
	m0_op_fini(ops[0]);
	m0_op_free(ops[0]);
get_error:
	m0_obj_lock_fini(&obj);
init_error:
	m0_entity_fini(&obj.ob_entity);
	cleanup_vecs(&data, &attr, &ext);

	return rc;
}

static bool bsize_valid(uint64_t blk_size)
{
	return ((blk_size >= BLK_SIZE_4k && blk_size <= BLK_SIZE_32m) &&
		 !(blk_size % BLK_SIZE_4k));
}

int m0_utility_args_init(int argc, char **argv,
			 struct m0_utility_param *params,
			 struct m0_idx_dix_config *dix_conf,
			 struct m0_config *conf,
			 void (*utility_usage) (FILE*, char*))
{
        int      option_index = 0;
        uint32_t temp;
        int      c;

	M0_SET0(params);
	params->cup_id = M0_ID_APP;
	params->cup_n_obj = 1;
	params->cup_take_locks = false;
	params->cup_update_mode = false;
	params->cup_offset = 0;
	params->flags = 0;
	conf->mc_is_read_verify = false;
	conf->mc_tm_recv_queue_min_len = M0_NET_TM_RECV_QUEUE_DEF_LEN;
	conf->mc_max_rpc_msg_size      = M0_RPC_DEF_MAX_RPC_MSG_SIZE;

	/*
	 * TODO This arguments parsing is common for all the client utilities.
	 * Every option here is not supported by every utility, for ex-
	 * block_count and block_size are not supported by m0unlink and
	 * m0touch. So if someone uses '-c' or '-s' with either of those
	 * without referring to help, (s)he won't get any error regarding
	 * 'unsupported option'.
	 * This need to be handle.
	 */
	static struct option l_opts[] = {
				{"local",         required_argument, NULL, 'l'},
				{"ha",            required_argument, NULL, 'H'},
				{"profile",       required_argument, NULL, 'p'},
				{"process",       required_argument, NULL, 'P'},
				{"object",        required_argument, NULL, 'o'},
				{"block-size",    required_argument, NULL, 's'},
				{"block-count",   required_argument, NULL, 'c'},
				{"trunc-len",     required_argument, NULL, 't'},
				{"layout-id",     required_argument, NULL, 'L'},
				{"n_obj",         required_argument, NULL, 'n'},
				{"msg_size",      required_argument, NULL, 'S'},
				{"min_queue",     required_argument, NULL, 'q'},
				{"blks-per-io",   required_argument, NULL, 'b'},
				{"offset",        required_argument, NULL, 'O'},
				{"update_mode",   no_argument,       NULL, 'u'},
				{"enable-locks",  no_argument,       NULL, 'e'},
				{"read-verify",   no_argument,       NULL, 'r'},
				{"help",          no_argument,       NULL, 'h'},
				{"no-hole",       no_argument,       NULL, 'N'},
				{0,               0,                 0,     0 }};

        while ((c = getopt_long(argc, argv, ":l:H:p:P:o:s:c:t:L:n:S:q:b:O:uerhN",
				l_opts, &option_index)) != -1)
	{
		switch (c) {
			case 'l': conf->mc_local_addr = optarg;
				  continue;
			case 'H': conf->mc_ha_addr = optarg;
				  continue;
			case 'p': conf->mc_profile = optarg;
				  continue;
			case 'P': conf->mc_process_fid = optarg;
				  continue;
			case 'o': if (m0_obj_id_sscanf(optarg,
							   &params->cup_id) < 0) {
					utility_usage(stderr, basename(argv[0]));
					exit(EXIT_FAILURE);
				  } else if (!entity_id_is_valid(
					     &params->cup_id)) {
					utility_usage(stderr, basename(argv[0]));
					exit(EXIT_FAILURE);
				  }
				  continue;
			case 's': if (m0_bcount_get(optarg,
						    &params->cup_block_size) ==
						    0) {
					if (bsize_valid(params->cup_block_size))
						continue;
					fprintf(stderr, "Invalid value for -%c."
							" Block size "
							"should be multiple of "
							"4k and in the range of"
							" (4k,32m)\n", c);
					utility_usage(stderr,
						      basename(argv[0]));
					exit(EXIT_FAILURE);
				  }
				  utility_usage(stderr, basename(argv[0]));
				  exit(EXIT_FAILURE);
			case 'b': if ((params->cup_blks_per_io = atoi(optarg)) < 0)
				  {
					fprintf(stderr, "Invalid value "
							"for option -%c. "
							"Blocks per io should "
							"be (>= 0)\n", c);
					utility_usage(stderr,
						      basename(argv[0]));
					exit(EXIT_FAILURE);
				  }
				  continue;
			case 'c': if (m0_bcount_get(optarg,
						    &params->cup_block_count) ==
						    0) {
					if (params->cup_block_count < 0) {
						fprintf(stderr, "Invalid value "
							"%lu for -%c. Block "
							"count should be > 0\n",
							params->cup_block_count,
							c);
						utility_usage(stderr,
							      basename(argv[0])
							     );
						exit(EXIT_FAILURE);
					}
					continue;
				  }
				  utility_usage(stderr, basename(argv[0]));
				  exit(EXIT_FAILURE);
			case 't': if (m0_bcount_get(optarg,
						    &params->cup_trunc_len) ==
						    0) {
					if (params->cup_trunc_len <= 0) {
						fprintf(stderr, "Invalid value "
							"%lu for -%c. Truncate "
							"length should be "
							"> 0\n",
							params->cup_trunc_len,
							c);
						utility_usage(stderr,
							      basename(argv[0])
							     );
						exit(EXIT_FAILURE);
					}
					continue;
				  }
				  utility_usage(stderr, basename(argv[0]));
				  exit(EXIT_FAILURE);
			case 'L': conf->mc_layout_id = atoi(optarg);
				  if (conf->mc_layout_id <= 0 ||
					conf->mc_layout_id >= 15) {
					fprintf(stderr, "Invalid layout id"
							" for -%c. Valid "
							"range: [1-14]\n", c);
					utility_usage(stderr,
						      basename(argv[0]));
					exit(EXIT_FAILURE);
				  }
				  continue;
			case 'n': params->cup_n_obj = atoi(optarg);
				  continue;
			case 'e': params->cup_take_locks = true;
				  continue;
			/* Update offset should be in multiple of 4k. */
			case 'O': if (!m0_bcount_get(optarg,
						     &params->cup_offset))
				  {
					if (params->cup_offset %
					    BLK_SIZE_4k == 0)
						continue;
					fprintf(stderr, "Invalid value for "
							"-%c. offset should be "
							"multiple of 4k\n", c);
					utility_usage(stderr,
						      basename(argv[0]));
					exit(EXIT_FAILURE);
				  }
				  utility_usage(stderr, basename(argv[0]));
				  exit(EXIT_FAILURE);
			case 'r': conf->mc_is_read_verify = true;
				  continue;
			case 'S': temp = atoi(optarg);
				  conf->mc_max_rpc_msg_size = temp;
				  continue;
			case 'q': temp = atoi(optarg);
				  conf->mc_tm_recv_queue_min_len = temp;
				  continue;
			case 'u': params->cup_update_mode = true;
				  continue;
			case 'h': utility_usage(stderr, basename(argv[0]));
				  exit(EXIT_FAILURE);
			case 'N': params->flags |= M0_OOF_NOHOLE;
				  continue;
			case '?': fprintf(stderr, "Unsupported option '%c'\n",
					  optopt);
				  utility_usage(stderr, basename(argv[0]));
				  exit(EXIT_FAILURE);
			case ':': fprintf(stderr, "No argument given for '%c'\n",
				          optopt);
				  utility_usage(stderr, basename(argv[0]));
				  exit(EXIT_FAILURE);
			default:  fprintf(stderr, "Unsupported option '%c'\n", c);
		}
	}
	conf->mc_is_oostore            = true;
	conf->mc_idx_service_conf      = dix_conf;
	dix_conf->kc_create_meta       = false;
	conf->mc_idx_service_id        = M0_IDX_DIX;

	return 0;
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
