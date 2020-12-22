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
 * Example Motr application to create index, put key&val into index,
 * get val from index, and then delete the index.
 */

/*
 * Please change the dir according to you development environment.
 *
 * How to build:
 * gcc -I/work/cortx-motr -I/work/cortx-motr/extra-libs/galois/include \
 *     -DM0_EXTERN=extern -DM0_INTERNAL= -Wno-attributes               \
 *     -L/work/cortx-motr/motr/.libs -lmotr                            \
 *     example2.c -o example2
 *
 * Please change the configuration according to you development environment.
 *
 * How to run:
 * LD_LIBRARY_PATH=/work/cortx-motr/motr/.libs/                              \
 * ./example2 172.16.154.179@tcp:12345:34:1 172.16.154.179@tcp:12345:33:1000 \
 *         "<0x7000000000000001:0>" "<0x7200000000000001:64>" 12345670
 */

#include "motr/client.h"
#include "lib/assert.h"
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>

enum {
	KV_COUNT = 10,
	KV_LEN   = 32,
};

static void op_entity_fini(struct m0_entity *e, struct m0_op **ops)
{
	m0_op_fini(ops[0]);
	m0_op_free(ops[0]);
	ops[0] = NULL;
	m0_entity_fini(e);
}

static int op_launch_wait_fini(struct m0_entity *e,
			       struct m0_op    **ops,
			       int              *sm_rc)
{
	int rc;

	m0_op_launch(ops, 1);
	rc = m0_op_wait(ops[0], M0_BITS(M0_OS_FAILED, M0_OS_STABLE),
			M0_TIME_NEVER);
	printf("rc=%d op_rc=%d\n", rc, ops[0]->op_rc);
	if (sm_rc != NULL) {
		/* Save retcodes. */
		*sm_rc = ops[0]->op_rc;
	}
	op_entity_fini(e, ops);
	return rc;
}

int index_create(struct m0_container *container, struct m0_uint128 *fid)
{
	struct m0_op   *ops[1] = { NULL };
	struct m0_idx   idx;
	int             rc;

	m0_idx_init(&idx, &container->co_realm, fid);

	rc = m0_entity_create(NULL, &idx.in_entity, &ops[0]);
	if (rc == 0)
		rc = op_launch_wait_fini(&idx.in_entity, ops, NULL);

	printf("index create rc: %i\n", rc);
	return rc;
}

int index_delete(struct m0_container *container, struct m0_uint128 *fid)
{
	struct m0_op   *ops[1] = { NULL };
	struct m0_idx   idx;
	int             rc;

	m0_idx_init(&idx, &container->co_realm, fid);

	rc = m0_entity_open(&idx.in_entity, &ops[0]);
	if (rc == 0) {
		rc = m0_entity_delete(&idx.in_entity, &ops[0]);
		if (rc == 0)
			rc = op_launch_wait_fini(&idx.in_entity, ops, NULL);
		else
			op_entity_fini(&idx.in_entity, ops);
	}

	printf("index delete rc: %i\n", rc);
	return rc;
}

int index_put(struct m0_container *container, struct m0_uint128 *fid)
{
	struct m0_op    *ops[1] = { NULL };
	struct m0_idx    idx;
	struct m0_bufvec keys;
	struct m0_bufvec vals;
	int              i;
	int32_t          rcs[KV_COUNT];
	int              rc;

	rc = m0_bufvec_alloc(&keys, KV_COUNT, KV_LEN);
	M0_ASSERT(rc == 0);

	rc= m0_bufvec_alloc(&vals, KV_COUNT, KV_LEN);
	M0_ASSERT(rc == 0);

	for (i = 0; i < KV_COUNT; i++) {
		memset(keys.ov_buf[i], 'A' + i, KV_LEN - 1);
		memset(vals.ov_buf[i], 'a' + i, KV_LEN - 1);
	}

	m0_idx_init(&idx, &container->co_realm, fid);
	rc = m0_idx_op(&idx, M0_IC_PUT, &keys, &vals, rcs, 0, &ops[0]);
	if (rc == 0)
		rc = op_launch_wait_fini(&idx.in_entity, ops, NULL);
	for (i = 0; rc == 0 && i < KV_COUNT; i++) {
		printf("PUT %d: key=%s val=%s\n",
			i, (char*)keys.ov_buf[i], (char*)vals.ov_buf[i]);
	}
	m0_bufvec_free(&keys);
	m0_bufvec_free(&vals);

	printf("index put rc: %i\n", rc);
	return rc;
}

int index_get(struct m0_container *container, struct m0_uint128 *fid)
{
	struct m0_op    *ops[1] = { NULL };
	struct m0_idx    idx;
	struct m0_bufvec keys;
	struct m0_bufvec vals;
	int              i;
	int32_t          rcs[KV_COUNT];
	int              rc;

	rc = m0_bufvec_alloc(&keys, KV_COUNT, KV_LEN);
	M0_ASSERT(rc == 0);
	/* For GET operation, we don't alloc actual buf */
	rc = m0_bufvec_empty_alloc(&vals, KV_COUNT);
	M0_ASSERT(rc == 0);

	for (i = 0; i < KV_COUNT; i++) {
		memset(keys.ov_buf[i], 'A' + i, KV_LEN - 1);
	}

	m0_idx_init(&idx, &container->co_realm, fid);
	rc = m0_idx_op(&idx, M0_IC_GET, &keys, &vals, rcs, 0, &ops[0]);
	if (rc == 0)
		rc = op_launch_wait_fini(&idx.in_entity, ops, NULL);
	for (i = 0; rc == 0 && i < KV_COUNT; i++) {
		printf("GOT %d: key=%s val=%s\n",
			i, (char*)keys.ov_buf[i], (char*)vals.ov_buf[i]);
	}

	m0_bufvec_free(&keys);
	m0_bufvec_free(&vals);

	printf("index get rc: %i\n", rc);
	return rc;
}

int main(int argc, char *argv[])
{
	static struct m0_client         *m0_instance = NULL;
	static struct m0_container       motr_container;
	static struct m0_config          motr_conf;
	static struct m0_idx_dix_config  motr_dix_conf;
	struct m0_uint128                index_id = { 0, 0 };
	int                              rc;

	if (argc != 6) {
		printf("%s HA_ADDR LOCAL_ADDR Profile_fid Process_fid obj_id\n",
		       argv[0]);
		exit(-1);
	}
	index_id.u_lo = atoll(argv[5]);
	if (index_id.u_lo < M0_ID_APP.u_lo) {
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

	/* index FID must be m0_dix_fid_type */
	m0_fid_tassume((struct m0_fid*)&index_id, &m0_dix_fid_type);

	rc = index_create(&motr_container, &index_id);
	if (rc == 0) {
		rc = index_put(&motr_container, &index_id);
		if (rc == 0) {
			printf("index put succeeded\n");
			rc = index_get(&motr_container, &index_id);
			if (rc == 0)
				printf("index get succeeded\n");
		}
		rc = index_delete(&motr_container, &index_id);
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
