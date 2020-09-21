/* -*- C -*- */
/*
 * Copyright (c) 2018-2020 Seagate Technology LLC and/or its Affiliates
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


#define M0_TRACE_SUBSYSTEM M0_TRACE_SUBSYS_CLIENT
#include "lib/trace.h"                /* M0_ERR */
#include <stdlib.h>
#include <stdio.h>
#include <sys/time.h>
#include "motr/client.h"
#include "motr/idx.h"
#include "lib/getopts.h"	/* M0_GETOPTS */
#include "module/instance.h"	/* m0 */


/* Client parameters */
static char *local_addr;
static char *ha_addr;
static char *prof;
static char *proc_fid;

static struct m0_client         *m0_instance = NULL;
static struct m0_container       container;
static struct m0_realm           uber_realm;
static struct m0_config          conf;
static bool                      ls = false;
static struct m0_idx_dix_config  dix_conf;
static struct m0 instance;

extern void st_mt_inst(struct m0_client *client);
void st_lsfid_inst(struct m0_client *client,
		   void (*print)(struct m0_fid*));

static void ls_print(struct m0_fid* fid)
{
	m0_console_printf(FID_F"\n", FID_P(fid));
}

static int init(void)
{
	int rc;

	conf.mc_is_addb_init          = true;
	conf.mc_is_oostore            = true;
	conf.mc_is_read_verify        = false;
	conf.mc_local_addr            = local_addr;
	conf.mc_ha_addr               = ha_addr;
	conf.mc_profile               = prof;
	conf.mc_process_fid           = proc_fid;
	conf.mc_tm_recv_queue_min_len = M0_NET_TM_RECV_QUEUE_DEF_LEN;
	conf.mc_max_rpc_msg_size      = M0_RPC_DEF_MAX_RPC_MSG_SIZE;
	conf.mc_idx_service_id        = M0_IDX_DIX;

	dix_conf.kc_create_meta = false;
	conf.mc_idx_service_conf = &dix_conf;

	/* Client instance */
	rc = m0_client_init(&m0_instance, &conf, true);
	if (rc != 0) {
		m0_console_printf("Failed to initialise Motr client\n");
		goto err_exit;
	}

	/* And finally, client root realm */
	m0_container_init(&container,
			  NULL, &M0_UBER_REALM,
			  m0_instance);
	rc = container.co_realm.re_entity.en_sm.sm_rc;

	if (rc != 0) {
		m0_console_printf("Failed to open uber realm\n");
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

int main(int argc, char **argv)
{
	int rc;

	m0_instance_setup(&instance);
	rc = m0_module_init(&instance.i_self, M0_LEVEL_INST_ONCE);
	if (rc != 0) {
		m0_console_printf("Cannot init module %i\n", rc);
		return rc;
	}

	rc = M0_GETOPTS("m0mt", argc, argv,
			M0_VOIDARG('s', "List all fids client can see",
					LAMBDA(void, (void) {
					ls = true;
					})),
			M0_STRINGARG('l', "Local endpoint address",
					LAMBDA(void, (const char *str) {
					local_addr = (char*)str;
					})),
			M0_STRINGARG('h', "HA address",
					LAMBDA(void, (const char *str) {
					ha_addr = (char*)str;
					})),
			M0_STRINGARG('f', "Process FID",
					LAMBDA(void, (const char *str) {
					proc_fid = (char*)str;
					})),
			M0_STRINGARG('p', "Profile options for Motr client",
					LAMBDA(void, (const char *str) {
					prof = (char*)str;
					})));
	if (rc != 0) {
		m0_console_printf("Usage: m0mt -l laddr -h ha_addr "
				  "-p prof_opt -f proc_fid [-s]\n");
		goto mod;
	}

	/* Initialise motr and Client */
	rc = init();
	if (rc < 0) {
		m0_console_printf("init failed!\n");
		goto mod;
	}

	if (ls) {
		m0_console_printf("FIDs seen with client:\n");
		st_lsfid_inst(m0_instance, ls_print); /* Ls test */
	} else {
		m0_console_printf("Up to 500 client requests pending, ~100s on devvm:\n");
		st_mt_inst(m0_instance); 	         /* Load test */
	}

	/* Clean-up */
	fini();
mod:
	m0_module_fini(&instance.i_self, M0_MODLEV_NONE);

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
