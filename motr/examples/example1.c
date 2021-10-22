/* -*- C -*- */
/*
 * Copyright (c) 2020 Seagate Technology LLC and/or its Affiliates
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

/*
 * Example Motr application to create object, write to object,
 * read from objet, and then delete the object.
 */

/*
 * Please change the dir according to you development environment.
 *
 * How to build:
 * gcc -I/work/cortx-motr -I/work/cortx-motr/extra-libs/galois/include \
 *     -DM0_EXTERN=extern -DM0_INTERNAL= -Wno-attributes               \
 *     -L/work/cortx-motr/motr/.libs -lmotr                            \
 *     example1.c -o example1
 *
 * Please change the configuration according to you development environment.
 *
 * How to run:
 * LD_LIBRARY_PATH=/work/cortx-motr/motr/.libs/                              \
 * ./example1 172.16.154.179@tcp:12345:34:1 172.16.154.179@tcp:12345:33:1000 \
 *         "<0x7000000000000001:0>" "<0x7200000000000001:64>" 12345670
 */

#include "motr/client.h"
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>

static struct m0_client         *m0_instance = NULL;
static struct m0_container       motr_container;
static struct m0_config          motr_conf;
static struct m0_idx_dix_config  motr_dix_conf;

struct m0_uint128                obj_id = { 0, 0 };

static int object_create(struct m0_container *container)
{
	struct m0_obj     obj;
	struct m0_client *instance;
	struct m0_op     *ops[1] = {NULL};
	int               rc;

	M0_SET0(&obj);
	instance = container->co_realm.re_instance;
	m0_obj_init(&obj, &container->co_realm, &obj_id,
		    m0_client_layout_id(instance));

	rc = m0_entity_create(NULL, &obj.ob_entity, &ops[0]);
	if (rc != 0) {
		printf("Failed to create object: %d\n", rc);
		return rc;
	}

	m0_op_launch(ops, 1);
	rc = m0_op_wait(ops[0],
			M0_BITS(M0_OS_FAILED, M0_OS_STABLE),
			M0_TIME_NEVER);
	if (rc == 0)
		rc = ops[0]->op_rc;

	m0_op_fini(ops[0]);
	m0_op_free(ops[0]);
	ops[0] = NULL;

	m0_entity_fini(&obj.ob_entity);

	printf("Object (id=%lu) creation result: %d\n",
	       (unsigned long)obj_id.u_lo, rc);
	return rc;
}

static int object_open(struct m0_obj *obj)
{
	struct m0_op *ops[1] = {NULL};
	int           rc;

	rc = m0_entity_open(&obj->ob_entity, &ops[0]);
	if (rc != 0) {
		printf("Failed to open object: %d\n", rc);
		return rc;
	}

	m0_op_launch(ops, 1);
	rc = m0_op_wait(ops[0],
			M0_BITS(M0_OS_FAILED, M0_OS_STABLE),
			M0_TIME_NEVER);
	if (rc == 0)
		rc = ops[0]->op_rc;

	m0_op_fini(ops[0]);
	m0_op_free(ops[0]);
	ops[0] = NULL;

	/* obj is valid if open succeeded */
	printf("Object (id=%lu) open result: %d\n",
	       (unsigned long)obj_id.u_lo, rc);
	return rc;
}

static int alloc_vecs(struct m0_indexvec *ext,
		      struct m0_bufvec   *data,
		      struct m0_bufvec   *attr,
		      uint32_t            block_count,
		      uint32_t            block_size)
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
	rc = m0_bufvec_alloc(attr, block_count, 1);
	if (rc != 0) {
		m0_indexvec_free(ext);
		m0_bufvec_free(data);
		return rc;
	}
	return rc;
}

static void prepare_ext_vecs(struct m0_indexvec *ext,
			     struct m0_bufvec   *data,
			     struct m0_bufvec   *attr,
			     uint32_t            block_count,
			     uint32_t            block_size,
			     uint64_t           *last_index,
			     char                c)
{
	int      i;

	for (i = 0; i < block_count; ++i) {
		ext->iv_index[i]       = *last_index;
		ext->iv_vec.v_count[i] = block_size;
		*last_index           += block_size;

		/* Fill the buffer with all `c`. */
		memset(data->ov_buf[i], c, data->ov_vec.v_count[i]);
		/* we don't want any attributes */
		attr->ov_vec.v_count[i] = 0;
	}
}

static void cleanup_vecs(struct m0_indexvec *ext,
			 struct m0_bufvec   *data,
			 struct m0_bufvec   *attr)
{
	/* Free bufvec's and indexvec's */
	m0_indexvec_free(ext);
	m0_bufvec_free(data);
	m0_bufvec_free(attr);
}

static int write_data_to_object(struct m0_obj      *obj,
				struct m0_indexvec *ext,
				struct m0_bufvec   *data,
				struct m0_bufvec   *attr)
{
	int          rc;
	struct m0_op *ops[1] = { NULL };

	/* Create the write request */
	m0_obj_op(obj, M0_OC_WRITE, ext, data, attr, 0, 0, &ops[0]);
	if (ops[0] == NULL) {
		printf("Failed to init a write op\n");
		return -EINVAL;
	}

	/* Launch the write request*/
	m0_op_launch(ops, 1);

	/* wait */
	rc = m0_op_wait(ops[0],
			M0_BITS(M0_OS_FAILED,
				M0_OS_STABLE),
			M0_TIME_NEVER);
	rc = rc ? : ops[0]->op_sm.sm_rc;

	/* fini and release the ops */
	m0_op_fini(ops[0]);
	m0_op_free(ops[0]);
	printf("Object write result: %d\n", rc);
	return rc;
}

static int object_write(struct m0_container *container)
{
	struct m0_obj      obj;
	struct m0_client  *instance;
	int                rc;

        struct m0_indexvec ext;
        struct m0_bufvec   data;
        struct m0_bufvec   attr;

	uint64_t           last_offset = 0;

	M0_SET0(&obj);
	instance = container->co_realm.re_instance;
	m0_obj_init(&obj, &container->co_realm, &obj_id,
		    m0_client_layout_id(instance));

	rc = object_open(&obj);
	if (rc != 0) {
		printf("Failed to open object: rc=%d\n", rc);
		return rc;
	}

	/*
	 * alloc & prepare ext, data and attr. We will write 4k * 2.
	 */
	rc = alloc_vecs(&ext, &data, &attr, 2, 4096);
	if (rc != 0) {
		printf("Failed to alloc ext & data & attr: %d\n", rc);
		goto out;
	}
	prepare_ext_vecs(&ext, &data, &attr, 2, 4096, &last_offset, 'A');

	/* Start to write data to object */
	rc = write_data_to_object(&obj, &ext, &data, &attr);
	cleanup_vecs(&ext, &data, &attr);

out:
	/* Similar to close() */
	m0_entity_fini(&obj.ob_entity);

	printf("Object write: %d\n", rc);
	return rc;
}

static int read_data_from_object(struct m0_obj      *obj,
				 struct m0_indexvec *ext,
				 struct m0_bufvec   *data,
				 struct m0_bufvec   *attr)
{
	int          rc;
	struct m0_op *ops[1] = { NULL };

	/* Create the read request */
	m0_obj_op(obj, M0_OC_READ, ext, data, attr, 0, 0, &ops[0]);
	if (ops[0] == NULL) {
		printf("Failed to init a read op\n");
		return -EINVAL;
	}

	/* Launch the read request*/
	m0_op_launch(ops, 1);

	/* wait */
	rc = m0_op_wait(ops[0],
			M0_BITS(M0_OS_FAILED,
				M0_OS_STABLE),
			M0_TIME_NEVER);
	rc = rc ? : ops[0]->op_sm.sm_rc;

	/* fini and release the ops */
	m0_op_fini(ops[0]);
	m0_op_free(ops[0]);
	printf("Object read result: %d\n", rc);
	return rc;
}

static void verify_show_data(struct m0_bufvec *data,
			     char c)
{
	int i, j;

	for (i = 0; i < data->ov_vec.v_nr; ++i) {
		printf("Block %6d:\n", i);
		printf("%.*s", (int)data->ov_vec.v_count[i],
			       (char *)data->ov_buf[i]);
		printf("\n");
		for (j = 0; j < data->ov_vec.v_count[i]; j++)
			if (((char*) data->ov_buf[i])[j] != c) {
				printf("verification failed at: %d:%d\n"
				       "Expected %c result %c\n",
					i, j, c, ((char*)data->ov_buf[i])[j]);
			}
	}
}

static int object_read(struct m0_container *container)
{
	struct m0_obj      obj;
	struct m0_client  *instance;
	int                rc;

        struct m0_indexvec ext;
        struct m0_bufvec   data;
        struct m0_bufvec   attr;

	uint64_t           last_offset = 0;

	M0_SET0(&obj);
	instance = container->co_realm.re_instance;
	m0_obj_init(&obj, &container->co_realm, &obj_id,
		    m0_client_layout_id(instance));

	rc = object_open(&obj);
	if (rc != 0) {
		printf("Failed to open object: rc=%d\n", rc);
		return rc;
	}

	/*
	 * alloc & prepare ext, data and attr. We will write 4k * 2.
	 */
	rc = alloc_vecs(&ext, &data, &attr, 2, 4096);
	if (rc != 0) {
		printf("Failed to alloc ext & data & attr: %d\n", rc);
		goto out;
	}
	prepare_ext_vecs(&ext, &data, &attr, 2, 4096, &last_offset, '\0');

	/* Start to read data to object */
	rc = read_data_from_object(&obj, &ext, &data, &attr);
	if (rc == 0) {
		verify_show_data(&data, 'A');
	}
	cleanup_vecs(&ext, &data, &attr);

out:
	/* Similar to close() */
	m0_entity_fini(&obj.ob_entity);

	printf("Object read: %d\n", rc);
	return rc;
}

static int object_delete(struct m0_container *container)
{
	struct m0_obj      obj;
	struct m0_client  *instance;
	struct m0_op      *ops[1] = { NULL };
	int                rc;

	M0_SET0(&obj);
	instance = container->co_realm.re_instance;
	m0_obj_init(&obj, &container->co_realm, &obj_id,
		    m0_client_layout_id(instance));

	rc = object_open(&obj);
	if (rc != 0) {
		printf("Failed to open object: rc=%d\n", rc);
		return rc;
	}

	m0_entity_delete(&obj.ob_entity, &ops[0]);
	m0_op_launch(ops, 1);
	rc = m0_op_wait(ops[0],
			M0_BITS(M0_OS_FAILED, M0_OS_STABLE),
			M0_TIME_NEVER);

	/* fini and release */
	m0_op_fini(ops[0]);
	m0_op_free(ops[0]);

	/* Similar to close() */
	m0_entity_fini(&obj.ob_entity);

	printf("Object deletion: %d\n", rc);
	return rc;
}


int main(int argc, char *argv[])
{
	int rc;

	if (argc != 6) {
		printf("%s HA_ADDR LOCAL_ADDR Profile_fid Process_fid obj_id\n",
		       argv[0]);
		exit(-1);
	}
	obj_id.u_lo = atoll(argv[5]);
	if (obj_id.u_lo < M0_ID_APP.u_lo) {
		printf("obj_id invalid. Please refer to M0_ID_APP "
		       "in motr/client.c\n");
		exit(-EINVAL);
	}

	motr_dix_conf.kc_create_meta = false;

	motr_conf.mc_is_oostore            = true;
	motr_conf.mc_is_read_verify        = false;
	motr_conf.mc_ha_addr               = argv[1];
	motr_conf.mc_local_addr            = argv[2];
	motr_conf.mc_profile               = argv[3];
	motr_conf.mc_process_fid           = argv[4];
	motr_conf.mc_tm_recv_queue_min_len = M0_NET_TM_RECV_QUEUE_DEF_LEN;
	motr_conf.mc_max_rpc_msg_size      = M0_RPC_DEF_MAX_RPC_MSG_SIZE;
	motr_conf.mc_idx_service_id        = M0_IDX_DIX;
	motr_conf.mc_idx_service_conf      = (void *)&motr_dix_conf;

	rc = m0_client_init(&m0_instance, &motr_conf, true);
	if (rc != 0) {
		printf("error in m0_client_init: %d\n", rc);
		exit(rc);
	}

	m0_container_init(&motr_container, NULL, &M0_UBER_REALM, m0_instance);
	rc = motr_container.co_realm.re_entity.en_sm.sm_rc;
	if (rc != 0) {
		printf("error in m0_container_init: %d\n", rc);
		goto out;
	}

	rc = object_create(&motr_container);
	if (rc == 0) {
		rc = object_write(&motr_container);
		if (rc == 0) {
			rc = object_read(&motr_container);
		}
		object_delete(&motr_container);
	}

out:
	m0_client_fini(m0_instance, true);
	printf("app completed: %d\n", rc);
	return rc;
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
/*
 * vim: tabstop=8 shiftwidth=8 noexpandtab textwidth=80 nowrap
 */
