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



/**
 * @addtogroup crate_utils
 *
 * @{
 */

#include <errno.h>
#include "lib/trace.h"
#include "motr/m0crate/logger.h"
#include "motr/client_internal.h" /* m0_instance */
#include "motr/m0crate/crate_client_utils.h"

#define LOG_PREFIX "utils:"

struct crate_conf	       *conf = NULL;
struct m0_config		m0_conf = {};
struct m0_idx_cass_config	cass_conf = {};
static int			num_m0_workloads = 0;
struct m0_client	       *m0_instance = NULL;
struct m0_container	        container = {};
static struct m0_realm	        uber_realm = {};
static struct m0_idx_dix_config dix_conf = {};

struct m0_realm *crate_uber_realm()
{
	M0_PRE(uber_realm.re_instance != NULL);
	return &uber_realm;
}

int adopt_motr_thread(struct m0_workload_task *task)
{
	int		  rc = 0;

	M0_PRE(crate_uber_realm() != NULL);
	M0_PRE(crate_uber_realm()->re_instance != NULL);

	if (m0_thread_tls() != NULL)
		return M0_RC(rc);

	rc = m0_thread_adopt(&task->mthread, m0_instance->m0c_motr);
	if (rc < 0) {
		crlog(CLL_ERROR, "Motr adoptation failed");
	}

	return M0_RC(rc);
}

void release_motr_thread(struct m0_workload_task *task)
{
	m0_thread_shun();
}

static void dix_config_init(struct m0_idx_dix_config *conf)
{
	conf->kc_create_meta = false;
}

int init(struct workload *w)
{
	int rc;

	num_m0_workloads++;

	if (num_m0_workloads != 1) {
		rc = 0;
		goto do_exit;
	}

	m0_conf.mc_is_addb_init          = conf->is_addb_init;
	m0_conf.mc_addb_size             = conf->addb_size;
	m0_conf.mc_is_oostore            = conf->is_oostrore;
	m0_conf.mc_is_read_verify        = conf->is_read_verify;
	m0_conf.mc_local_addr            = conf->local_addr;
	m0_conf.mc_ha_addr               = conf->ha_addr;
	m0_conf.mc_profile               = conf->prof;
	m0_conf.mc_process_fid           = conf->process_fid;
	m0_conf.mc_tm_recv_queue_min_len = conf->tm_recv_queue_min_len ?:
	                                       M0_NET_TM_RECV_QUEUE_DEF_LEN;
	m0_conf.mc_max_rpc_msg_size      = conf->max_rpc_msg_size ?:
	                                       M0_RPC_DEF_MAX_RPC_MSG_SIZE;
	m0_conf.mc_layout_id             = conf->layout_id;
	m0_conf.mc_idx_service_id        = conf->index_service_id;

	if (m0_conf.mc_idx_service_id == M0_IDX_CASS) {
		cass_conf.cc_cluster_ep              = conf->cass_cluster_ep;
		cass_conf.cc_keyspace                = conf->cass_keyspace;
		cass_conf.cc_max_column_family_num   = conf->col_family;
		m0_conf.mc_idx_service_conf = &cass_conf;
        } else if (m0_conf.mc_idx_service_id == M0_IDX_DIX ||
		   m0_conf.mc_idx_service_id == M0_IDX_MOCK) {
                dix_config_init(&dix_conf);
                m0_conf.mc_idx_service_conf = &dix_conf;
        } else {
		rc = -EINVAL;
		cr_log(CLL_ERROR, "Unknown index service id: %d!\n",
		       m0_conf.mc_idx_service_id);
		goto do_exit;
	}

	/* Client instance */
	rc = m0_client_init(&m0_instance, &m0_conf, true);
	if (rc != 0) {
		cr_log(CLL_ERROR, "Failed to initialise Client: %d\n", rc);
		goto do_exit;
	}

	M0_POST(m0_instance != NULL);

	/* And finally, client root realm */
	m0_container_init(&container,
				 NULL, &M0_UBER_REALM,
				 m0_instance);

	rc = container.co_realm.re_entity.en_sm.sm_rc;
	if (rc != 0) {
		cr_log(CLL_ERROR, "Failed to open uber realm\n");
		goto do_exit;
	}

	M0_POST(container.co_realm.re_instance != NULL);
	uber_realm = container.co_realm;

do_exit:
	return rc;
}

void free_m0_conf()
{
	m0_free(conf->local_addr);
	m0_free(conf->ha_addr);
	m0_free(conf->prof);
	m0_free(conf->process_fid);
	m0_free(conf->cass_cluster_ep);
	m0_free(conf->cass_keyspace);
	m0_free(conf);
}

int fini(struct workload *w)
{
	num_m0_workloads--;
	if(num_m0_workloads == 0) {
		m0_client_fini(m0_instance, true);
		free_m0_conf();
	}
	return 0;
}

void check(struct workload *w)
{
}

/** @} end of crate_utils group */

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
