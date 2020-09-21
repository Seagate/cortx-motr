/* -*- C -*- */
/*
 * Copyright (c) 2017-2020 Seagate Technology LLC and/or its Affiliates
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
#include <assert.h>

#include "motr/client.h"
#include "motr/client_internal.h"
#include "motr/idx.h"
#include "motr/layout.h"


/* Client parameters */
static char *local_addr;
static char *ha_addr;
static char *prof;
static char *proc_fid;

static struct m0_client          *m0_instance = NULL;
static struct m0_container        container;
static struct m0_realm            uber_realm;
static struct m0_config           conf;
static struct m0_idx_dix_config   dix_conf;

extern struct m0_addb_ctx m0_addb_ctx;

static int init(void)
{
	int rc;

	conf.mc_is_oostore            = true;
	conf.mc_is_read_verify        = false;
	conf.mc_local_addr            = local_addr;
	conf.mc_ha_addr               = ha_addr;
	conf.mc_profile               = prof;
	conf.mc_process_fid           = proc_fid;
	conf.mc_tm_recv_queue_min_len = M0_NET_TM_RECV_QUEUE_DEF_LEN;
	conf.mc_max_rpc_msg_size      = M0_RPC_DEF_MAX_RPC_MSG_SIZE;
	conf.mc_layout_id	      = 0;

	/* Use dix index services. */
	conf.mc_idx_service_id        = M0_IDX_DIX;
	dix_conf.kc_create_meta       = false;
	conf.mc_idx_service_conf      = &dix_conf;

	/* Client instance */
	rc = m0_client_init(&m0_instance, &conf, true);
	if (rc != 0) {
		fprintf(stderr, "Failed to initialise Client\n");
		goto err_exit;
	}

	/* And finally, client root realm */
	m0_container_init(&container,
			  NULL, &M0_UBER_REALM,
			  m0_instance);
	rc = container.co_realm.re_entity.en_sm.sm_rc;
	if (rc != 0) {
		fprintf(stderr, "Failed to open uber realm\n");
		goto err_exit;
	}
	uber_realm = container.co_realm;
	return 0;

err_exit:
	return rc;
}

static void fini(void)
{
	m0_client_fini(m0_instance, true);
}

static int create_index(struct m0_fid fid)
{
	int                     rc;
	struct m0_op           *ops[1] = {NULL};
	struct m0_idx           idx;

	memset(&idx, 0, sizeof idx);
	ops[0] = NULL;

	/* Set an index creation operation. */
	m0_idx_init(&idx,
		&container.co_realm, (struct m0_uint128 *)&fid);
	m0_entity_create(NULL, &idx.in_entity, &ops[0]);

	/* Launch and wait for op to complete */
	m0_op_launch(ops, 1);
	rc = m0_op_wait(ops[0],
		    M0_BITS(M0_OS_FAILED,
			    M0_OS_STABLE),
		    M0_TIME_NEVER);
	if (rc < 0) return rc;

	rc = ops[0]->op_sm.sm_rc;
	if (rc < 0) return rc;

	/* fini and release */
	m0_op_fini(ops[0]);
	m0_op_free(ops[0]);
	m0_entity_fini(&idx.in_entity);

	return rc;
}

static int delete_index(struct m0_fid fid)
{
	int                     rc;
	struct m0_op           *ops[1] = {NULL};
	struct m0_idx           idx;

	memset(&idx, 0, sizeof idx);
	ops[0] = NULL;

	/* Set an index creation operation. */
	m0_idx_init(&idx,
		&container.co_realm, (struct m0_uint128 *)&fid);
	m0_entity_delete(&idx.in_entity, &ops[0]);

	/* Launch and wait for op to complete */
	m0_op_launch(ops, 1);
	rc = m0_op_wait(ops[0],
		    M0_BITS(M0_OS_FAILED,
			    M0_OS_STABLE),
		    M0_TIME_NEVER);
	rc = (rc != 0)?rc:ops[0]->op_sm.sm_rc;

	/* fini and release */
	m0_op_fini(ops[0]);
	m0_op_free(ops[0]);
	m0_entity_fini(&idx.in_entity);

	return rc;
}

int main(int argc, char **argv)
{
	int rc;

	/* Get input parameters */
	if (argc < 5) {
		fprintf(stderr,
			"Usage: m0composite laddr ha_addr prof_opt proc_fid\n");
		return -1;
	}
	local_addr = argv[1];
	ha_addr = argv[2];
	prof = argv[3];
	proc_fid = argv[4];

	/* Initialise motr and Client */
	rc = init();
	if (rc < 0) {
		fprintf(stderr, "init failed!\n");
		return rc;
	}

	/* Create global extent indices for composite layouts. */
	rc = create_index(composite_extent_rd_idx_fid);
	if (rc != 0) {
		fprintf(stderr,
			"Can't create composite RD extent index, rc=%d!\n", rc);
		return rc;
	}
	rc = create_index(composite_extent_wr_idx_fid);
	if (rc != 0) {
		fprintf(stderr, "Can't create composite RD extent index!\n");
		delete_index(composite_extent_rd_idx_fid);
		return rc;
	}

	/* Clean-up */
	fini();

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
