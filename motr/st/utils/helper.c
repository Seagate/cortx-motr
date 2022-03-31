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
#include "lib/cksum.h"

extern struct m0_addb_ctx m0_addb_ctx;

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
		      uint32_t block_size, uint32_t usz)
{
	int      rc;
	int      num_unit_per_op;

	rc = m0_indexvec_alloc(ext, block_count);
	if (rc != 0)
		return rc;

	/*
	 * this allocates <block_count> * <block_size>  buffers for data,
	 * and initialises the bufvec for us.
	 */

	rc = m0_bufvec_alloc_aligned(data, block_count, block_size,
				     m0_pageshift_get());
	if (rc != 0) {
		m0_indexvec_free(ext);
		return rc;
	}
	num_unit_per_op = (block_count * block_size)/usz;
	M0_LOG(M0_DEBUG,"NU : %d, BC : %d, BS : %d, US : %d", (int)num_unit_per_op,
			(int)block_count,(int)block_size,(int)usz);
	if (num_unit_per_op && attr) {
		rc = m0_bufvec_alloc(attr, num_unit_per_op, max_cksum_size());
		if (rc != 0) {
			m0_indexvec_free(ext);
			m0_bufvec_free(data);
			return rc;
		}
	} else if (attr) {
		memset(attr, 0, sizeof(*attr));
		return M0_RC(-EINVAL);
	}
	return rc;
}

/* This function calculates checksum for data read */
static int calculate_checksum(struct m0_obj *obj, struct m0_indexvec *ext,
					struct m0_bufvec *data,struct m0_bufvec *attr)
{
	struct m0_pi_seed                  seed;
	int                                usz;
	struct m0_bufvec_cursor            datacur;
	struct m0_bufvec_cursor            tmp_datacur;
	struct m0_ivec_cursor              extcur;
	struct m0_bufvec                   user_data = {};
	unsigned char                      *curr_context;
	int                                rc = 0;
	int                                i;
	int                                count;
	struct m0_md5_inc_context_pi       pi={};
	int                                attr_idx = 0;
	uint32_t                           nr_seg;
	m0_bcount_t                        bytes;
	enum m0_pi_calc_flag               flag = M0_PI_CALC_UNIT_ZERO;

	M0_ENTRY();
	if(attr == NULL || !(obj->ob_entity.en_flags & M0_ENF_DI)) {
		return 0;
	}
	usz = m0_obj_layout_id_to_unit_size(
			m0__obj_lid(obj));
	m0_bufvec_cursor_init(&datacur, data);
	m0_bufvec_cursor_init(&tmp_datacur, data);
	m0_ivec_cursor_init(&extcur, ext);
	curr_context = m0_alloc(sizeof(MD5_CTX));
	memset(&pi, 0, sizeof(struct m0_md5_inc_context_pi));
	while (!m0_bufvec_cursor_move(&datacur, 0) &&
		!m0_ivec_cursor_move(&extcur, 0) &&
		attr_idx < attr->ov_vec.v_nr){	
		nr_seg = 0;
		count = usz;
		/* calculate number of segments required for 1 data unit */
		while (count > 0 && !m0_bufvec_cursor_move(&tmp_datacur, 0)) {
			nr_seg++;
			bytes = m0_bufvec_cursor_step(&tmp_datacur);
			if (bytes < count) {
				m0_bufvec_cursor_move(&tmp_datacur, bytes);
				count -= bytes;
			} else {
				m0_bufvec_cursor_move(&tmp_datacur, count);
				count = 0;
			}
		}
		/* allocate an empty buf vec */
		rc = m0_bufvec_empty_alloc(&user_data, nr_seg);
		if (rc != 0) {
			M0_LOG(M0_ERROR, "buffer allocation failed, rc %d", rc);
			return false;
		}
		/* populate the empty buf vec with data pointers
		 * and create 1 data unit worth of buf vec
		 */
		i = 0;
		count = usz;
		while (count > 0 && !m0_bufvec_cursor_move(&datacur, 0)) {
			bytes = m0_bufvec_cursor_step(&datacur);
			if (bytes < count) {
				user_data.ov_vec.v_count[i] = bytes;
				user_data.ov_buf[i] = m0_bufvec_cursor_addr(&datacur);
				m0_bufvec_cursor_move(&datacur, bytes);
				count -= bytes;
			}
			else {
				user_data.ov_vec.v_count[i] = count;
				user_data.ov_buf[i] = m0_bufvec_cursor_addr(&datacur);
				m0_bufvec_cursor_move(&datacur, count);
				count = 0;
			}
			i++;
		}
		M0_ASSERT(attr->ov_vec.v_nr != 0 && attr->ov_vec.v_count[attr_idx] != 0);
		if (attr_idx != 0) {
			flag = M0_PI_NO_FLAG;
			memcpy(pi.pimd5c_prev_context, curr_context, sizeof(MD5_CTX));
		}
		seed.pis_data_unit_offset   = m0_ivec_cursor_index(&extcur);
		seed.pis_obj_id.f_container = obj->ob_entity.en_id.u_hi;
		seed.pis_obj_id.f_key       = obj->ob_entity.en_id.u_lo;
		pi.pimd5c_hdr.pih_type = M0_PI_TYPE_MD5_INC_CONTEXT;
		rc = m0_client_calculate_pi((struct m0_generic_pi *)&pi,
							&seed, &user_data, flag,
							curr_context, NULL);
		memcpy(attr->ov_buf[attr_idx], &pi,
				sizeof(struct m0_md5_inc_context_pi));
		attr_idx++;
		m0_ivec_cursor_move(&extcur, usz);
		m0_bufvec_free2(&user_data);
	}
	return rc;
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

	for ( i = 0; i < attr->ov_vec.v_nr; i++) {
		attr->ov_vec.v_count[i] = max_cksum_size();
	}
}

static int alloc_prepare_vecs(struct m0_indexvec *ext,
			      struct m0_bufvec *data,
			      struct m0_bufvec *attr,
			      uint32_t block_count, uint32_t block_size,
			      uint64_t *last_index, uint32_t usz)
{
	int      rc;

	rc = alloc_vecs(ext, data, attr, block_count, block_size, usz);
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
	struct m0_obj       *obj;
	struct m0_op_common *oc;
	struct m0_op_obj    *oo;

	rc = m0_entity_create(NULL, entity, &ops[0]);
	if (rc != 0)
		return M0_ERR(rc);

	m0_op_launch(ops, 1);
	rc = m0_op_wait(ops[0], M0_BITS(M0_OS_FAILED,
					M0_OS_STABLE), M0_TIME_NEVER);
	if (rc == 0)
		rc = ops[0]->op_rc;

	oc = M0_AMB(oc, ops[0], oc_op);
	oo = M0_AMB(oo, oc, oo_oc);
	obj = m0__obj_entity(oo->oo_oc.oc_op.op_entity);
	M0_LOG(M0_DEBUG, "post create object, obj->ob_attr.oa_pver :"FID_F,
	       FID_P(&obj->ob_attr.oa_pver));

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

	/** Create write operation
	 *  CKSUM_TODO: calculate cksum and pass in
        *  attr instead of NULL
        */
	rc = m0_obj_op(obj, M0_OC_WRITE, ext, data,
				attr->ov_vec.v_nr ? attr : NULL, 0, 0, &ops[0]);
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
	     int blks_per_io, bool take_locks, bool update_mode, bool di_flag)
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
	uint32_t                      usz;

	/* Open source file */
	fp = fopen(src, "r");
	if (fp == NULL)
		return -EPERM;
	M0_SET0(&obj);
	lock_ops = take_locks ? &lock_enabled_ops : &lock_disabled_ops;
	instance = container->co_realm.re_instance;
	m0_obj_init(&obj, &container->co_realm, &id,
		    m0_client_layout_id(instance));
	if (di_flag)
		obj.ob_entity.en_flags |= M0_ENF_DI;
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

	usz = m0_obj_layout_id_to_unit_size(obj.ob_attr.oa_layout_id);
	rc = alloc_vecs(&ext, &data, &attr, blks_per_io, block_size, usz);
	if (rc == -EINVAL) {
		obj.ob_entity.en_flags &= ~M0_ENF_DI;
		rc = 0;
	}
	if (rc != 0)
		goto cleanup;

	while (block_count > 0) {
		bcount = (block_count > blks_per_io)?
			  blks_per_io:block_count;
		if (bcount < blks_per_io) {
			cleanup_vecs(&data, &attr, &ext);
			rc = alloc_vecs(&ext, &data, &attr, bcount,
					block_size, usz);
			if (rc == -EINVAL) {
				obj.ob_entity.en_flags &= ~M0_ENF_DI;
				rc = 0;
			}
			if (rc != 0)
				goto cleanup;
		}
		prepare_ext_vecs(&ext, &attr, bcount,
				 block_size, &last_index);

		/* Read data from source file. */
		rc = read_data_from_file(fp, &data);
		M0_ASSERT(rc == bcount);
		calculate_checksum(&obj, &ext, &data, &attr);
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
	rc = m0_obj_op(obj, M0_OC_READ, ext, data, attr->ov_vec.v_nr ? attr : NULL,
				0, flags, &ops[0]);
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
	    uint32_t flags, struct m0_fid *read_pver, bool di_flag)
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
	uint32_t                      usz;

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
	if (di_flag)
		obj.ob_entity.en_flags |= M0_ENF_DI;
	rc = lock_ops->olo_lock_init(&obj);
	if (rc != 0)
		goto init_error;
	rc = lock_ops->olo_read_lock_get_sync(&obj, &req);
	if (rc != 0)
		goto get_error;

	/* Setting pver here to read_pver received as parameter to this func.
	 * Caller of this function is expected to pass pver of object to be
	 * read, if he knows pver of object.
	 * */
	if (read_pver != NULL &&  m0_fid_is_set(read_pver)) {
		obj.ob_attr.oa_pver = *read_pver;
		M0_LOG(M0_DEBUG, "obj->ob_attr.oa_pver is set to:"FID_F,
	               FID_P(&obj.ob_attr.oa_pver));
	}

	rc = open_entity(&obj.ob_entity);
	if (entity_sm_state(&obj) != M0_ES_OPEN || rc != 0)
		goto cleanup;

	last_index = offset;

	if (blks_per_io == 0)
		blks_per_io = M0_MAX_BLOCK_COUNT;
	usz = m0_obj_layout_id_to_unit_size(obj.ob_attr.oa_layout_id);
	rc = alloc_vecs(&ext, &data, &attr, blks_per_io, block_size, usz);
	if (rc == -EINVAL) {
		obj.ob_entity.en_flags &= ~M0_ENF_DI;
		rc = 0;
	}
	if (rc != 0)
		goto cleanup;
	while (block_count > 0) {
		bytes_read = 0;
		bcount = (block_count > blks_per_io) ?
			  blks_per_io : block_count;
		if (bcount < blks_per_io) {
			cleanup_vecs(&data, &attr, &ext);
			rc = alloc_vecs(&ext, &data, &attr, bcount,
					block_size, usz);
			if (rc == -EINVAL) {
				obj.ob_entity.en_flags &= ~M0_ENF_DI;
				rc = 0;
			}
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
		uint32_t block_size, uint32_t block_count, bool di_flag)
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
	uint32_t               usz;

	M0_SET0(&obj);
	instance = container->co_realm.re_instance;
	m0_obj_init(&obj, &container->co_realm, &id,
		    m0_client_layout_id(instance));
	if (di_flag)
		obj.ob_entity.en_flags |= M0_ENF_DI;
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
	usz = m0_obj_layout_id_to_unit_size(obj.ob_attr.oa_layout_id);
	while (block_count > 0) {
		bcount = (block_count > M0_MAX_BLOCK_COUNT) ?
			  M0_MAX_BLOCK_COUNT : block_count;
		rc = alloc_prepare_vecs(&ext, &data, &attr, bcount,
					       block_size, &last_index, usz);
		if (rc == -EINVAL) {
			obj.ob_entity.en_flags &= ~M0_ENF_DI;
			rc = 0;
		}
		if (rc != 0)
			goto cleanup;

		/* Read data from source file. */
		rc = read_data_from_file(fp, &data);
		M0_ASSERT(rc == bcount);
		rc = calculate_checksum(&obj, &ext, &data, &attr);
		M0_ASSERT(rc == 0);

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
	       uint32_t block_size, uint32_t block_count, bool di_flag)
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
	uint32_t                      usz;

	instance = container->co_realm.re_instance;

	/* Read the requisite number of blocks from the entity */
	M0_SET0(&obj);
	m0_obj_init(&obj, &container->co_realm, &id,
			   m0_client_layout_id(instance));
	usz = m0_obj_layout_id_to_unit_size(obj.ob_attr.oa_layout_id);
	rc = alloc_prepare_vecs(&ext, &data, &attr, block_count,
				       block_size, &last_index, usz);
	if (rc == -EINVAL) {
		obj.ob_entity.en_flags &= ~M0_ENF_DI;
		rc = 0;
	}
	if (rc != 0)
		return rc;
	if (di_flag)
		obj.ob_entity.en_flags |= M0_ENF_DI;
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
	rc = m0_obj_op(&obj, M0_OC_READ, &ext, &data,
				attr.ov_vec.v_nr ? &attr : NULL, 0, 0, &ops[0]);
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
	params->di_flag = false;
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
				{"pver",          required_argument, NULL, 'v'},
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
				{"data-integrity",no_argument,       NULL, 'd'},
				{0,               0,                 0,     0 }};

        while ((c = getopt_long(argc, argv, ":l:H:p:P:o:s:c:t:L:v:n:S:q:b:O:uerhNd",
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
			case 'v': if (m0_fid_sscanf(optarg,
						&params->cup_pver) < 0) {
					utility_usage(stderr, basename(argv[0]));
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
			case 'd': params->di_flag = true;
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
