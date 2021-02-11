#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <motr/client_internal.h>
#include "lib/trace.h"
#include "fdmi/fdmi.h"
#include "fdmi/service.h"
#include "fdmi/plugin_dock.h"
#include "fdmi/plugin_dock_internal.h"
#include "reqh/reqh.h"
#include "motr/client.h"
#include "motr/st/utils/helper.h"
#include "motr/init.h"
#define NOBJS 40

#define SMALL_UNIT_SIZE (4096)	// Default layout.
#define LARGE_UNIT_SIZE (1024*4096)	// Large layout.

struct m0_realm my_realm;
static uint64_t small_layout;
static uint64_t large_layout;

static int large_count;
static int small_count;
static struct m0_client          *m0c = NULL;
static struct m0_pool pool;
static struct m0_config    conf;
static struct m0_container container;

static struct m0_fid CLASSIFY_PLUGIN_FID = {
	.f_container = M0_FDMI_REC_TYPE_FOL,
	.f_key = 0x1
};

static int
m0_write_n_blocks(int nblocks,
		  int unit_size, struct m0_obj *obj, int off, void *data)
{
	//printf("In m0/-write_n_blocks\n");
        int rc;
	struct m0_indexvec ivec;
	struct m0_bufvec bvec;
	struct m0_bufvec attr;
	struct m0_op *ops[1] = { NULL };

	rc = m0_bufvec_alloc(&bvec, 1, nblocks * unit_size);
	if (rc != 0)
		return rc;
	rc = m0_bufvec_alloc(&attr, 1, 1);
	if (rc != 0)
		return rc;
	rc = m0_indexvec_alloc(&ivec, 1);
	if (rc != 0)
		return rc;

	ivec.iv_index[0] = off * unit_size;
	ivec.iv_vec.v_count[0] = unit_size * nblocks;
	attr.ov_vec.v_count[0] = 0;

	for (int i = 0; i < nblocks; i++) {
		memcpy(((char *)bvec.ov_buf[0]) + i * unit_size,
		       (char *)data, unit_size);
	}

	m0_obj_op(obj, M0_OC_WRITE, &ivec, &bvec, &attr, 0,
			 0, &ops[0]);

	m0_op_launch(ops, 1);

	rc = m0_op_wait(ops[0],
			       M0_BITS(M0_OS_FAILED,
				       M0_OS_STABLE), M0_TIME_NEVER);

	rc = m0_rc(ops[0]);

	m0_op_fini(ops[0]);
	m0_op_free(ops[0]);

	return rc;
}

static int m0_open_obj(struct m0_uint128 id,int layout,struct m0_obj *obj)
{
        //printf("In m0_open_obj\n");   
	struct m0_op *open_op[1] = { NULL };
	int rc;
        //struct m0_uint128 id;
        //int layout;

        memset(obj, 0, sizeof(struct m0_obj));

        m0_obj_init(obj, &container.co_realm, &id, layout);

	//m0_clovis_entity_open(&obj->ob_entity, &open_op[0]);
	//m0_clovis_op_launch(open_op, 1);

	//m0_clovis_op_wait(open_op[0], M0_BITS(M0_CLOVIS_OS_FAILED,
	//				      M0_CLOVIS_OS_STABLE),
	//		  M0_TIME_NEVER);
	//rc = m0_clovis_rc(open_op[0]);

	//printf("open rc=%d\n", rc);
	//m0_clovis_op_fini(open_op[0]);
	//m0_clovis_op_free(open_op[0]);

	//return rc;

        rc = m0_entity_open(&obj->ob_entity, &open_op[0]);
        if (rc != 0)
                goto cleanup;

        m0_op_launch(open_op, 1);
        rc =  m0_op_wait(open_op[0], M0_BITS(M0_OS_FAILED,
                                              M0_OS_STABLE),
                          M0_TIME_NEVER);
        if (rc == 0)
                rc = open_op[0]->op_rc;

cleanup:
        m0_op_fini(open_op[0]);
        m0_op_free(open_op[0]);
        open_op[0] = NULL;

        return rc;

}

static int
m0_create_obj(struct m0_uint128 id, int layout, struct m0_obj *obj)
{
        //printf("In m0_create_obj\n");
	int rc;
	struct m0_op *ops[1] = { NULL };

	memset(obj, 0, sizeof(struct m0_obj));

	//m0_clovis_obj_init(obj, &my_realm, &id, layout);
	m0_obj_init(obj, &container.co_realm, &id, layout);

	rc = m0_entity_create(NULL, &obj->ob_entity, &ops[0]);
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

	rc = m0_rc(ops[0]);

	m0_op_fini(ops[0]);
	m0_op_free(ops[0]);

	return rc;
}

void list_container(struct m0_uint128 container_id)
{
        //printf("in list_container\n"); 
	struct m0_bufvec keys;
	struct m0_container cont;
	struct m0_op *op = NULL;
	int rc, rcs[1];

	rc = m0_bufvec_alloc(&keys, 41, sizeof(struct m0_fid));

	m0_container_init(&cont, &my_realm, &container_id, m0c);
	//rc = m0_clovis_container_op(&cont, M0_CLOVIS_IC_LIST, &keys, NULL, rcs,
	//			    0, &op);
	
        m0_op_launch(&op, 1);
	rc = m0_op_wait(op, M0_BITS(M0_OS_STABLE), M0_TIME_NEVER);

	if (rc < 0) {
		fprintf(stderr, "Failed to list container items.\n");
	} else {
		for (int i = 0; i < keys.ov_vec.v_nr; i++) {
			printf("%s\n", (char *)keys.ov_buf[i]);
		}
	}

	m0_bufvec_free(&keys);
}
int clients_init(struct m0_config    *config,
                struct m0_container *container,
                struct m0_client          **instance)
   {
        int rc;



        //printf("In clovis_init\n");
        if (config->mc_local_addr == NULL || config->mc_ha_addr == NULL ||
            config->mc_profile == NULL || config->mc_process_fid == NULL) {
                rc = M0_ERR(-EINVAL);
                fprintf(stderr, "config parameters not initialized.\n");
                goto err_exit;
        }


        //rc = m0_init();
        rc = m0_client_init(instance, config, true);
        if (rc != 0)
                goto err_exit;


        m0_container_init(container,
                                 NULL, &M0_UBER_REALM,
                                 *instance);
        rc = container->co_realm.re_entity.en_sm.sm_rc;


   err_exit:
        return rc;
 }

int main(int argc, char **argv)
{
        int rc;

        struct m0_idx_dix_config   idx_service_conf = {
                .kc_create_meta = false
        };


        struct m0_config    conf = {
                .mc_is_oostore            = true,
                .mc_is_read_verify        = false,
                .mc_ha_addr               = argv[1],  
                .mc_local_addr            = argv[2],  
                .mc_profile               = argv[3],  
                .mc_process_fid           = argv[4],      
                .mc_tm_recv_queue_min_len = M0_NET_TM_RECV_QUEUE_DEF_LEN,
                .mc_max_rpc_msg_size      = M0_RPC_DEF_MAX_RPC_MSG_SIZE,
                .mc_idx_service_id        = M0_IDX_DIX, 
                .mc_idx_service_conf      = (void *)&idx_service_conf
        };

        //printf("In main\n");
        if (argc < 5) {
		printf("%s HA_ADDR LOCAL_ADDR PROFILE_ID PROCESS_ID\n", argv[0]);
                return -1;
	}

        rc = clients_init(&conf, &container, &m0c);
        if (rc < 0) {
                fprintf(stderr, "client_init failed! rc = %d\n", rc);
		return -2;
        }
        else {
                printf("Clovis init OK. Creating %d objects...\n", NOBJS);
        }

        struct m0_fid pver = M0_FID_TINIT('v', 1, 100);
        struct m0_ext range[] = { {.e_start = 0,.e_end = IMASK_INF} };
        //rc = m0_dix_ldesc_init(&idx_service_conf.kc_layout_ldesc, range,
        //                       ARRAY_SIZE(range), HASH_FNC_FNV1, &pver);
        rc = m0_dix_ldesc_init(&idx_service_conf.kc_ldescr_ldesc, range,
                               ARRAY_SIZE(range), HASH_FNC_FNV1, &pver);


        //small_layout = layout_id(m0c);
        small_layout = m0_client_layout_id(m0c);
        large_layout = 11;


        //m0_clovis_realm_init(&my_realm,
        //                   NULL, &M0_CLOVIS_UBER_REALM,
        //                   M0_CLOVIS_ST_CONTAINER, m0c);
        
        //Block of 4MiB
        char *data = (char *)malloc(LARGE_UNIT_SIZE);
        for (int i = 0; i < LARGE_UNIT_SIZE; i++)
                data[i] = i % 128;

        for (int i = 0; i < NOBJS; i++) {
                bool large = rand() > RAND_MAX / 2;
                int layout = large ? large_layout : small_layout;


                struct m0_obj obj;
                struct m0_uint128 obj_id = M0_ID_APP;


                obj_id.u_lo += i + 1;
                printf("Creating %s object #%d... ", large ? "large" : "small",
                       i + 1);


                rc = m0_create_obj(obj_id, layout, &obj);
                if (rc) {
                        continue;
                }


                m0_open_obj(obj_id, layout, &obj);
                rc = m0_write_n_blocks(1,
                                       large ? LARGE_UNIT_SIZE : SMALL_UNIT_SIZE, &obj, 0,
                                       data);

                if (rc) {
                        fprintf(stderr,
                                "Failed writing object %d (rc=%d). Skipping...\n",
                                i + 1, rc);
                        continue;
                }
        }

        printf("Listing containers.\n ");
        struct m0_container small_cont;
        struct m0_container large_cont;
        struct m0_uint128 small_cont_id = M0_ID_APP;
        struct m0_uint128 large_cont_id = M0_ID_APP;
        small_cont_id.u_hi = small_cont_id.u_hi + 1;
        large_cont_id.u_hi = large_cont_id.u_hi + 2;


        getchar();
        printf("==================\n");
        printf("SMALL OBJECTS: \n");
        printf("==================\n");
        list_container(small_cont_id);


        getchar();
        printf("==================\n");
        printf("LARGE OBJECTS: \n");
        printf("==================\n");
        list_container(large_cont_id);


        free(data);


        m0_fini();
	//client_fini(m0_instance);
        printf("End of program\n");


        return rc;
}
