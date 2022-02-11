
#include "motr/client.h"
#include "lib/assert.h"
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>

enum {
	KV_COUNT = 10,
	KV_LEN   = 32,
};

static struct m0_client         *m0_instance = NULL;
static struct m0_container       motr_container;
static struct m0_config          motr_conf;
static struct m0_idx_dix_config  motr_dix_conf;

static struct m0_uint128         index_id = { 0, 0 };

static void op_entity_fini(struct m0_entity *e, struct m0_op **ops)
{
	m0_op_fini(ops[0]);
	m0_op_free(ops[0]);
	ops[0] = NULL;
	m0_entity_fini(e);
}

static int op_launch_wait_fini(struct m0_entity *e, struct m0_op **ops, int *sm_rc)
{
	int rc;

	m0_op_launch(ops, 1);
	rc = m0_op_wait(ops[0], M0_BITS(M0_OS_FAILED, M0_OS_STABLE), M0_TIME_NEVER);

	printf("rc=%d op_rc=%d\n", rc, ops[0]->op_rc);

	if (sm_rc != NULL) {
		/* Save retcodes. */
		*sm_rc = ops[0]->op_rc;
	}
	op_entity_fini(e, ops);
	return rc;
}

static struct m0_uint128 create_index(long long id) {

	if (id < M0_ID_APP.u_lo) {
		printf("obj_id invalid. Please refer to M0_ID_APP in motr/client.c\n");
		exit(-EINVAL);
	}
	
	struct m0_uint128 index_id = { 0, 0 };
	index_id.u_lo = id;
	
	m0_fid_tassume((struct m0_fid*)&index_id, &m0_dix_fid_type);

	return index_id;
}

// should accept an index id
int index_create(long long id)
{
	struct m0_op   *ops[1] = { NULL };
	struct m0_idx   idx;
	int             rc;
	struct m0_uint128 index_id = create_index(id);
	
	// set the idx's entity to index_id, using container realm ( as the parent )
	m0_idx_init(&idx, &motr_container.co_realm, &index_id);

	// create the index
	rc = m0_entity_create(NULL, &idx.in_entity, &ops[0]);

	if (rc == 0)
		rc = op_launch_wait_fini(&idx.in_entity, ops, NULL);

	printf("index create rc: %i\n", rc);
	
	return rc;
}

// should accept an index id
int index_delete(long long id)
{
	struct m0_op   *ops[1] = { NULL };
	struct m0_idx   idx;
	int             rc;
	struct m0_uint128 index_id = create_index(id);

	// create an index entity by set index_id to idx, using container realm ( as the parent )
	m0_idx_init(&idx, &motr_container.co_realm, &index_id);

	// set the entity state to open
	rc = m0_entity_open(&idx.in_entity, &ops[0]);
	
	if (rc == 0) {

		// delete the index
		rc = m0_entity_delete(&idx.in_entity, &ops[0]);

		if (rc == 0)
			rc = op_launch_wait_fini(&idx.in_entity, ops, NULL);
		else
			op_entity_fini(&idx.in_entity, ops);
	}

	printf("index delete rc: %i\n", rc);
	return rc;
}

// should accept an index id, key, value
int index_put(void)
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

	m0_idx_init(&idx, &motr_container.co_realm, &index_id);
	rc = m0_idx_op(&idx, M0_IC_PUT, &keys, &vals, rcs, 0, &ops[0]);

	if (rc == 0)
		rc = op_launch_wait_fini(&idx.in_entity, ops, NULL);

	for (i = 0; rc == 0 && i < KV_COUNT; i++) {
		printf("PUT %d: key=%s val=%s\n", i, (char*)keys.ov_buf[i], (char*)vals.ov_buf[i]);
	}

	m0_bufvec_free(&keys);
	m0_bufvec_free(&vals);

	printf("index put rc: %i\n", rc);
	return rc;
}

int index_get(void)
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

	m0_idx_init(&idx, &motr_container.co_realm, &index_id);
	rc = m0_idx_op(&idx, M0_IC_GET, &keys, &vals, rcs, 0, &ops[0]);

	if (rc == 0)
		rc = op_launch_wait_fini(&idx.in_entity, ops, NULL);

	for (i = 0; rc == 0 && i < KV_COUNT; i++) {
		printf("GOT %d: key=%s val=%s\n", i, (char*)keys.ov_buf[i], (char*)vals.ov_buf[i]);
	}

	m0_bufvec_free(&keys);
	m0_bufvec_free(&vals);

	printf("index get rc: %i\n", rc);
	return rc;
}

/*
int main(int argc, char *argv[])
{	
	if (argc != 6) {
		printf("%s HA_ADDR LOCAL_ADDR Profile_fid Process_fid obj_id\n",
		       argv[0]);
		exit(-1);
	}
	
	long long id = atoll(argv[5]);

	if (id < M0_ID_APP.u_lo) {
		printf("obj_id invalid. Please refer to M0_ID_APP in motr/client.c\n");
		exit(-EINVAL);
	}

	run(argv[1], argv[2], argv[3], argv[4], id);
}
*/

int init(char *ha_addr, char *local_addr, char *profile, char *process_fid) {

	motr_dix_conf.kc_create_meta = false;

	motr_conf.mc_is_oostore            = true;
	motr_conf.mc_is_read_verify        = false;
	motr_conf.mc_ha_addr               = ha_addr;
	motr_conf.mc_local_addr            = local_addr;
	motr_conf.mc_profile               = profile;
	motr_conf.mc_process_fid           = process_fid;
	motr_conf.mc_tm_recv_queue_min_len = M0_NET_TM_RECV_QUEUE_DEF_LEN;
	motr_conf.mc_max_rpc_msg_size      = M0_RPC_DEF_MAX_RPC_MSG_SIZE;
	motr_conf.mc_idx_service_id        = M0_IDX_DIX;
	motr_conf.mc_idx_service_conf      = (void *)&motr_dix_conf;

	int rc = m0_client_init(&m0_instance, &motr_conf, true);

	if (rc != 0) {
		printf("error in m0_client_init: %d\n", rc);
		return -1;
	}

	m0_container_init(&motr_container, NULL, &M0_UBER_REALM, m0_instance);

	rc = motr_container.co_realm.re_entity.en_sm.sm_rc;

	if (rc != 0) {
		printf("error in m0_container_init: %d\n", rc);
		return -2;
	}

}

int run(char *ha_addr, char *local_addr, char *profile, char *process_fid, long long id) {
	
	if (id < M0_ID_APP.u_lo) {
		printf("obj_id invalid. Please refer to M0_ID_APP in motr/client.c\n");
		exit(-EINVAL);
	}

	index_id.u_lo = id;

	motr_dix_conf.kc_create_meta = false;

	motr_conf.mc_is_oostore            = true;
	motr_conf.mc_is_read_verify        = false;
	motr_conf.mc_ha_addr               = ha_addr;
	motr_conf.mc_local_addr            = local_addr;
	motr_conf.mc_profile               = profile;
	motr_conf.mc_process_fid           = process_fid;
	motr_conf.mc_tm_recv_queue_min_len = M0_NET_TM_RECV_QUEUE_DEF_LEN;
	motr_conf.mc_max_rpc_msg_size      = M0_RPC_DEF_MAX_RPC_MSG_SIZE;
	motr_conf.mc_idx_service_id        = M0_IDX_DIX;
	motr_conf.mc_idx_service_conf      = (void *)&motr_dix_conf;

	int rc = m0_client_init(&m0_instance, &motr_conf, true);

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

	// rc = index_create(&index_id);

	// if (rc == 0) {

	// 	rc = index_put();

	// 	if (rc == 0) {
	// 		printf("index put succeeded\n");

	// 		rc = index_get();

	// 		if (rc == 0)
	// 			printf("index get succeeded\n");

	// 	}

	// 	rc = index_delete(&index_id);
	// }

out:
	m0_client_fini(m0_instance, true);
	printf("app completed: %d\n", rc);
	return rc;
}
