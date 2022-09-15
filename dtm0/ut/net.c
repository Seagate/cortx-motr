/* -*- C -*- */
/*
 * Copyright (c) 2013-2020 Seagate Technology LLC and/or its Affiliates
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
 * @addtogroup dtm0
 *
 * @{
 */

#define M0_TRACE_SUBSYSTEM M0_TRACE_SUBSYS_UT
#include "lib/trace.h"
#include "lib/fs.h"         /* m0_file_read */

#include "dtm0/net.h"
#include "ut/ut.h"              /* M0_UT_ASSERT */
#include "fid/fid.h"            /* M0_FID_INIT */
#include "conf/helpers.h"      /* struct m0_confc_args */

#include "dtm0/service.h"       /* m0_dtm0_service */
#include "dtm0/ut/helper.h"     /* m0_ut_dtm0_helper_init */

#include "module/instance.h"   /* m0_set() */

enum {
	MSG_NR = 0x100,
	EP_STR_LEN = 255,

	CLUSTER_SIZE = 5,
};

struct endpoint {
	char as_string[EP_STR_LEN + 1];
};


struct ut_motr_net {
	struct m0_net_domain      domain;
	struct m0_reqh            reqh;
	struct m0_net_buffer_pool buf_pool;
	struct m0_rpc_machine     rpc_machine;
	struct m0_reqh_service    service;
	struct endpoint           endpoint;
	struct m0_dtm0_net        dnet;
	struct m0_motr_ha         motr_ha;

	struct m0                 mn_motr;
	struct m0_thread          mn_thr;
	struct m0_clink           mn_clink;

};

struct ut_motr_net_cs {
	struct ut_motr_net server;
	struct ut_motr_net client;
};

struct m0_ut_dtm0_net_helper {
	struct m0_ut_dtm0_helper base;
	struct m0_dtm0_net       dnet_client;
	struct m0_dtm0_net       dnet_server;
};

void ut_motr_net_init(struct ut_motr_net    *net,
		      const struct m0_fid   *fid,
		      const struct endpoint *endpoint)
{
	enum { NR_TMS = 1 };
	int      rc;
	uint32_t bufs_nr;
	const char *ep_addr = &endpoint->as_string[0];

	/*M0_UT_ASSERT(M0_IS0(net));*/
	rc = m0_net_domain_init(&net->domain, m0_net_xprt_default_get());
	M0_UT_ASSERT(rc == 0);
	bufs_nr = m0_rpc_bufs_nr(M0_NET_TM_RECV_QUEUE_DEF_LEN, NR_TMS);
	rc = m0_rpc_net_buffer_pool_setup(&net->domain, &net->buf_pool,
					  bufs_nr, NR_TMS);
	M0_UT_ASSERT(rc == 0);
	rc = M0_REQH_INIT(&net->reqh,
			  .rhia_dtm     = (void *)1,
			  .rhia_db      = NULL,
			  .rhia_mdstore = (void *)1,
			  .rhia_fid     = fid);
	M0_UT_ASSERT(rc == 0);

	rc = m0_rpc_machine_init(&net->rpc_machine, &net->domain,
				 ep_addr, &net->reqh, &net->buf_pool,
				 M0_BUFFER_ANY_COLOUR,
				 M0_RPC_DEF_MAX_RPC_MSG_SIZE,
				 M0_NET_TM_RECV_QUEUE_DEF_LEN);
	net->endpoint = *endpoint;
	M0_UT_ASSERT(rc == 0);
}

extern struct m0_reqh_service_type dtm0_service_type;

void ut_motr_net_start(struct ut_motr_net *net)
{
	struct m0_reqh_service_type *type = &dtm0_service_type;
	struct m0_reqh_service      *svc = &net->service;
	struct m0_fid                service_fid =
		M0_FID_TINIT('s', 0, net->reqh.rh_fid.f_key);
	int                          rc;

	rc = m0_reqh_service_setup(&svc, type, &net->reqh, NULL, &service_fid);
	M0_UT_ASSERT(rc == 0);
}

void ut_motr_net_stop(struct ut_motr_net *net)
{
	m0_reqh_service_quit(&net->service);
}

static void ut_motr_net_fini(struct ut_motr_net *net)
{
	//m0_reqh_services_terminate(&net->reqh);
	m0_rpc_machine_fini(&net->rpc_machine);
	if (m0_reqh_state_get(&net->reqh) != M0_REQH_ST_STOPPED)
		m0_reqh_services_terminate(&net->reqh);
	m0_reqh_fini(&net->reqh);
	m0_rpc_net_buffer_pool_cleanup(&net->buf_pool);
	m0_net_domain_fini(&net->domain);
	/*M0_SET0(net);*/
}



static void m0_ut_dtm0_net_helper_init(struct m0_ut_dtm0_net_helper *h)
{
	struct m0_ut_dtm0_helper *udh = &h->base;

	m0_ut_dtm0_helper_init(udh);
	m0_dtm0_net_init(&h->dnet_server,
			 &(struct m0_dtm0_net_cfg) {
				.dnc_reqh = udh->udh_server_reqh, });
	m0_dtm0_net_init(&h->dnet_client,
			 &(struct m0_dtm0_net_cfg) {
				.dnc_reqh = udh->udh_client_reqh, });
}

static void m0_ut_dtm0_net_helper_fini(struct m0_ut_dtm0_net_helper *h)
{
	m0_dtm0_net_fini(&h->dnet_client);
	m0_dtm0_net_fini(&h->dnet_server);
	m0_ut_dtm0_helper_fini(&h->base);
}

void m0_dtm0_ut_net_init_fini(void)
{
	struct m0_reqh     *reqh;
	struct m0_dtm0_net *dnet;
	int                 rc;

	M0_ALLOC_PTR(reqh);
	M0_UT_ASSERT(reqh != NULL);
	M0_ALLOC_PTR(dnet);
	M0_UT_ASSERT(dnet != NULL);

	rc = M0_REQH_INIT(reqh,
			  .rhia_dtm     = (void*)1,
			  .rhia_mdstore = (void*)1,
			  .rhia_fid     = &g_process_fid);
	M0_UT_ASSERT(rc == 0);
	m0_reqh_start(reqh);
	m0_dtm0_net_init(dnet,
			 &(struct m0_dtm0_net_cfg) { .dnc_reqh = reqh, });
	m0_dtm0_net_fini(dnet);
	m0_reqh_services_terminate(reqh);
	m0_reqh_fini(reqh);

	m0_free(dnet);
	m0_free(reqh);
}

void m0_dtm0_ut_net_tranceive(void)
{
	struct m0_ut_dtm0_net_helper *h;
	struct m0_dtm0_msg           *fop;
	struct m0_dtm0_msg            msg_out;
	struct m0_be_op              *op;
	struct m0_be_op               op_out = {};
	struct m0_fid                *fid;
	struct m0_fid                 fid_out;
	bool                          successful;
	int                           i;
	int                           j;
	struct m0_fid                *target;
	uint32_t                      msg_nr = 2;

	M0_ALLOC_PTR(h);
	M0_UT_ASSERT(h != NULL);

	m0_ut_dtm0_net_helper_init(h);
	target = &h->base.udh_client_dtm0_fid;

	M0_ALLOC_ARR(fid, msg_nr);
	M0_UT_ASSERT(fid != NULL);
	for (i = 0; i < msg_nr; ++i) {
		fid[i] = M0_FID_INIT(0, i+1);  /* TODO set fid type */
	}
	M0_ALLOC_ARR(fop, msg_nr);
	M0_UT_ASSERT(fop != NULL);
	for (i = 0; i < msg_nr; ++i) {
		fop[i] = M0_DTM0_MSG_EOL_INIT(&fid[i]);
	}
	M0_ALLOC_ARR(op, msg_nr);
	M0_UT_ASSERT(op != NULL);
	for (i = 0; i < msg_nr; ++i)
		m0_be_op_init(&op[i]);

	for (i = 0; i < msg_nr; ++i)
		m0_dtm0_net_send(&h->dnet_server,
				 &op[i], target, &fop[i], NULL);

	m0_be_op_init(&op_out);
	for (i = 0; i < msg_nr; ++i) {
		successful = false;
		msg_out = (struct m0_dtm0_msg) {};
		m0_dtm0_net_recv(&h->dnet_client,
				 &op_out, &successful, &msg_out, M0_DMT_EOL);
		m0_be_op_wait(&op_out);
		M0_UT_ASSERT(successful);
		M0_UT_ASSERT(memcmp(&msg_out, &((struct m0_dtm0_msg) {}),
				    sizeof(msg_out)) != 0);
		M0_UT_ASSERT(msg_out.dm_type == M0_DMT_EOL);
		fid_out = msg_out.dm_msg.eol.dme_initiator;
		m0_be_op_reset(&op_out);
		/* TODO: it would be nice to have m0_find() */
		j = m0_fold(m, a, msg_nr, -1,
			    a  == -1 ? m0_fid_eq(&fid_out, &fid[m]) ? m :
			    a : a);
		M0_UT_ASSERT(j != -1);
		M0_UT_ASSERT(fid[j].f_key == j + 1);
		fid[j] = M0_FID0;
	}
	m0_be_op_fini(&op_out);
	for (i = 0; i < msg_nr; ++i)
		M0_UT_ASSERT(m0_fid_eq(&fid[i], &M0_FID0));
	m0_free(fid);
	for (i = 0; i < msg_nr; ++i) {
		m0_be_op_wait(&op[i]);
		m0_be_op_fini(&op[i]);
	}
	m0_free(op);
	m0_free(fop);

	m0_ut_dtm0_net_helper_fini(h);
	m0_free(h);
}

struct ut_motr_cluster {
	struct ut_motr_net items[2];
};

static struct ut_motr_cluster cluster;
static struct m0_mutex        ut_net_tests_chan_mutex;
static struct m0_chan         ut_net_tests_chan;

static int ut_motr_thin_ha_init(struct ut_motr_net *net)
{
	struct m0_motr_ha_cfg  motr_ha_cfg;
	int                    rc;
	struct endpoint       *endpoint = &net->endpoint;
	const char            *ep = &endpoint->as_string[0];
	char                  *cc_ha_addr = m0_strdup(ep);

	M0_ENTRY();

	//if (net != NULL)
		//return 0;
#if 0
	if (cctx->cc_ha_addr == NULL && cctx->cc_reqh_ctx.rc_confdb != NULL) {
		ep = cs_eps_tlist_head(&cctx->cc_reqh_ctx.rc_eps)->ex_endpoint;
		cctx->cc_ha_addr = m0_strdup(ep);
		cctx->cc_no_all2all_connections = true;
	}
#endif

	motr_ha_cfg = (struct m0_motr_ha_cfg){
		.mhc_dispatcher_cfg = {
			.hdc_enable_note      = true,
			.hdc_enable_keepalive = true,
			.hdc_enable_fvec      = true,
		},
		.mhc_addr           = cc_ha_addr,
		.mhc_rpc_machine    = 
		   	m0_reqh_rpc_mach_tlist_head(&net->reqh.rh_rpc_machines),
		.mhc_reqh           = &net->reqh,
		.mhc_process_fid    = g_process_fid,
	};
	rc = m0_motr_ha_init(&net->motr_ha, &motr_ha_cfg);
	M0_ASSERT(rc == 0);
	rc = m0_motr_ha_start(&net->motr_ha);
	M0_ASSERT(rc == 0);
	return 0;
}

static void ut_motr_thin_ha_fini(struct ut_motr_net *net)
{
	//if (net != NULL)
		//return;
	m0_motr_ha_stop(&net->motr_ha);
	m0_motr_ha_fini(&net->motr_ha);
}

static void ut_motr_confc_init(struct ut_motr_net *net)
{
	char   *confdb = M0_SRC_PATH("dtm0/conf.xc");
	char   *confstr;
	int     rc = 0;
	struct m0_confc_args conf_args = {
		.ca_profile = "0:0",
		.ca_group   = m0_locality0_get()->lo_grp
	};

	//if (net != NULL)
		//return;
	m0_file_read(confdb, &confstr);
	conf_args.ca_rmach = 
		   m0_reqh_rpc_mach_tlist_head(&net->reqh.rh_rpc_machines);
	rc = m0_reqh_conf_setup(&net->reqh, &conf_args);
	M0_ASSERT(rc == 0);
	m0_free(confstr);
}

static void ut_motr_confc_fini(struct ut_motr_net *net)
{
	//if (net != NULL)
		//return;
	m0_rconfc_fini(&net->reqh.rh_rconfc);
}

static void ut_motr_cluster_init(int i)
{
	struct m0_fid process_fid = g_process_fid;
	struct endpoint endpoint = {};
	char  *ep_addr = &endpoint.as_string[0];

	(void) sprintf(ep_addr, "inet:tcp:127.0.0.1@220%d0", i);
	process_fid.f_key = i;
	ut_motr_net_init(&cluster.items[i], &process_fid, &endpoint);
	//if (i == 0)
		ut_motr_thin_ha_init(&cluster.items[i]);
	ut_motr_confc_init(&cluster.items[i]);
	m0_dtm0_net_init(&cluster.items[i].dnet, 
			 &(struct m0_dtm0_net_cfg) {
			 .dnc_reqh = &cluster.items[i].reqh, });
}

static void ut_motr_cluster_fini(int i)
{
	m0_dtm0_net_fini(&cluster.items[i].dnet);
	ut_motr_confc_fini(&cluster.items[i]);
	//if (i == 0)
		ut_motr_thin_ha_fini(&cluster.items[i]);
	ut_motr_net_fini(&cluster.items[i]);
}

static void ut_motr_cluster_start(int i)
{

	ut_motr_net_start(&cluster.items[i]);
}

static void ut_motr_cluster_stop(int i)
{
	ut_motr_net_stop(&cluster.items[i]);
}

void m0_dtm0_ut_net_thin_init_fini(void)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(cluster.items); i++)
		ut_motr_cluster_init(i);

	for (i = 0; i < ARRAY_SIZE(cluster.items); i++)
		ut_motr_cluster_fini(i);
}

static struct m0_semaphore startup_sem;

int m0_dtm0_ut_net_thread_init(int i)
{
	/* Set the module instance to the varaible present in TLS. */
	cluster.items[i].mn_motr = *m0_get();
	m0_set(&cluster.items[i].mn_motr);
	cluster.items[i].mn_thr.t_tls.tls_m0_instance = m0_get();
	
	return 0;
}

void m0_dtm0_ut_net_thin_start_stop_thr(int i)
{
	/* Announce the thread is ready to run */
	m0_semaphore_up(&startup_sem);

	/* Wait for signal from main thread to start INIT */
	m0_chan_wait(&cluster.items[i].mn_clink);

	ut_motr_cluster_init(i);
	ut_motr_cluster_start(i);

	/* Sync with main thread before starting the FINI process */
	m0_semaphore_up(&startup_sem);
	m0_chan_wait(&cluster.items[i].mn_clink);

	ut_motr_cluster_stop(i);
	ut_motr_cluster_fini(i);


}

void m0_dtm0_ut_init(void)
{
	int i;

	m0_mutex_init(&ut_net_tests_chan_mutex);
	m0_chan_init(&ut_net_tests_chan, &ut_net_tests_chan_mutex);

	for (i = 0; i < ARRAY_SIZE(cluster.items); i++) {
		m0_clink_init(&cluster.items[i].mn_clink, NULL);
		m0_clink_add_lock(&ut_net_tests_chan, &cluster.items[i].mn_clink);
	}
		
	m0_semaphore_init(&startup_sem, 0);
}

void m0_dtm0_ut_fini(void)
{
	int i;

	m0_semaphore_fini(&startup_sem);

	for (i = 0; i < ARRAY_SIZE(cluster.items); i++) {
		m0_clink_del_lock(&cluster.items[i].mn_clink);
		m0_clink_fini(&cluster.items[i].mn_clink);
	}

	m0_chan_fini_lock(&ut_net_tests_chan);
	m0_mutex_fini(&ut_net_tests_chan_mutex);
}

void m0_dtm0_ut_net_thin_start_stop(void)
{
	int  rc;
	int  i;
	bool ok;

	m0_dtm0_ut_init();
	for (i = 0; i < ARRAY_SIZE(cluster.items); i++) {
		rc = M0_THREAD_INIT(&cluster.items[i].mn_thr, int,
				    &m0_dtm0_ut_net_thread_init,
				    &m0_dtm0_ut_net_thin_start_stop_thr,
				    i, "net_server_%d", i);
		M0_UT_ASSERT(rc == 0);
	}

	/* Wait for all threads to get READY to run */
	for (i = 0; i < ARRAY_SIZE(cluster.items); ++i) {
		ok = m0_semaphore_timeddown(&startup_sem,
				            m0_time_from_now(43200, 0));
		M0_UT_ASSERT(ok);
	}

	/* Start cluster INIT */
	m0_chan_broadcast_lock(&ut_net_tests_chan);

	/* Wait for all threads to complete the INIT */
	for (i = 0; i < ARRAY_SIZE(cluster.items); ++i) {
		ok = m0_semaphore_timeddown(&startup_sem,
				            m0_time_from_now(43200, 0));
		M0_UT_ASSERT(ok);
	}

	/* Start cluster FINI */
	m0_chan_broadcast_lock(&ut_net_tests_chan);

	/* Wait for all threads to complete */
	for (i = 0; i < ARRAY_SIZE(cluster.items); ++i) {
		m0_thread_join(&cluster.items[i].mn_thr);
	}

	m0_dtm0_ut_fini();
}

static void ut_blob_init(struct m0_dtm0_msg *msg)
{
	static char message[] = "Hello";

	msg->dm_type = M0_DMT_EOL;
	msg->dm_msg.blob.datum.b_addr = message;
	msg->dm_msg.blob.datum.b_nob  = sizeof(message);
}

static void ut_blob_fini(struct m0_dtm0_msg *msg)
{
}

static void ut_motr_net_send(struct ut_motr_net *client,
			     struct m0_be_op *op,
			     struct ut_motr_net *server,
			     const struct m0_dtm0_msg *msg)
{
	/* TODO: won't work unless confc is here. */
	//M0_UT_ASSERT(false);
	m0_dtm0_net_send(&client->dnet, op, &server->reqh.rh_fid, msg, NULL);
}

static void ut_motr_net_recv(struct ut_motr_net *client,
			     struct m0_be_op *op,
			     bool *successful,
			     struct m0_dtm0_msg *msg)
{
	m0_dtm0_net_recv(&client->dnet, op, successful, msg, M0_DMT_EOL);
}

#if 0
static void ut_motr_cluster_tranceive_simple(struct ut_motr_cluster *cluster,
					     int client_id, int server_id)
{
	struct ut_motr_net *client = &cluster->items[client_id];
	struct ut_motr_net *server = &cluster->items[server_id];
	struct m0_be_op snd_op = {};
	struct m0_be_op rcv_op = {};
	struct m0_dtm0_msg snd_msg;
	struct m0_dtm0_msg rcv_msg;
	bool               rcv_successful;
	int                rc;
	struct m0_fid      fid;

	m0_be_op_init(&snd_op);
	m0_be_op_init(&rcv_op);

	ut_blob_init(&snd_msg); /* To be removed */
	fid = M0_FID_INIT(0, 1);  /* TODO set fid type */
	snd_msg = M0_DTM0_MSG_EOL_INIT(&fid);

	ut_motr_net_send(client, &snd_op, server, &snd_msg);
	ut_motr_net_recv(server, &rcv_op, &rcv_successful, &rcv_msg);

	m0_be_op_wait(&rcv_op);
	M0_UT_ASSERT(rcv_successful);
	rc = memcmp(&snd_msg, &rcv_msg, sizeof(snd_msg));
	M0_UT_ASSERT(rc == 0);

	m0_be_op_wait(&snd_op);

	ut_blob_fini(&rcv_msg);
	ut_blob_fini(&snd_msg);

	m0_be_op_fini(&snd_op);
	m0_be_op_fini(&rcv_op);
}
#endif

static void ut_motr_cluster_tranceive_simple(int i)
{
#if 1
	//struct ut_motr_net *client = &cluster->items[client_id];
	//struct ut_motr_net *server = &cluster->items[server_id];
	struct m0_be_op snd_op[ARRAY_SIZE(cluster.items)] = {};
	struct m0_be_op rcv_op[ARRAY_SIZE(cluster.items)] = {};
	struct m0_dtm0_msg snd_msg = {};
	struct m0_dtm0_msg rcv_msg[ARRAY_SIZE(cluster.items)] = {};
	bool               rcv_successful[ARRAY_SIZE(cluster.items)] = {};
	int                rc;
	struct m0_fid      fid;
	int                off;

	for (off = 0; off < ARRAY_SIZE(cluster.items); off++) {
		if (off == i)
			continue;
		m0_be_op_init(&snd_op[off]);
		m0_be_op_init(&rcv_op[off]);
	}

	ut_blob_init(&snd_msg); /* To be removed */
	fid = M0_FID_INIT(0, 1);  /* TODO set fid type */
	snd_msg = M0_DTM0_MSG_EOL_INIT(&fid);

	for (off= 0; off < ARRAY_SIZE(cluster.items); off++) {
		if (off == i)
			continue;
		ut_motr_net_send(&cluster.items[i], &snd_op[off],
				 &cluster.items[off], &snd_msg);
	}

	for (off= 0; off < ARRAY_SIZE(cluster.items); off++) {
		if (off == i)
			continue;
		m0_be_op_wait(&snd_op[off]);
	}

	for (off= 0; off < ARRAY_SIZE(cluster.items); off++) {
		if (off == i)
			continue;
		ut_motr_net_recv(&cluster.items[off], &rcv_op[off], 
				 &rcv_successful[off], &rcv_msg[off]);
	}

	for (off= 0; off < ARRAY_SIZE(cluster.items); off++) {
		if (off == i)
			continue;
		m0_be_op_wait(&rcv_op[off]);
		M0_UT_ASSERT(rcv_successful[off]);
		rc = memcmp(&snd_msg, &rcv_msg[off], sizeof(snd_msg));
		M0_UT_ASSERT(rc == 0);
	}


	ut_blob_fini(&snd_msg);
	for (off= 0; off < ARRAY_SIZE(cluster.items); off++) {
		if (off == i)
			continue;
		ut_blob_fini(&rcv_msg[off]);
		m0_be_op_fini(&snd_op[off]);
		m0_be_op_fini(&rcv_op[off]);
	}

#endif
	cluster = cluster;
}

void m0_dtm0_ut_net_thin_server(int i)
{
	ut_motr_cluster_init(i);
	ut_motr_cluster_start(i);

	m0_semaphore_up(&startup_sem);
	m0_chan_wait(&cluster.items[i].mn_clink);

	ut_motr_cluster_tranceive_simple(i);

	m0_semaphore_up(&startup_sem);
	m0_chan_wait(&cluster.items[i].mn_clink);

	ut_motr_cluster_stop(i);
	ut_motr_cluster_fini(i);
}

void m0_dtm0_ut_net_thin_tranceive(void)
{
	enum { CLIENT = 0, SERVER = 1 };
	int  rc;
	int  i;
	bool ok;

	m0_dtm0_ut_init();

	for (i = 0; i < ARRAY_SIZE(cluster.items); i++) {
		rc = M0_THREAD_INIT(&cluster.items[i].mn_thr, int,
				    &m0_dtm0_ut_net_thread_init,
				    &m0_dtm0_ut_net_thin_server,
				    i, "net_server_%d", i);
		M0_UT_ASSERT(rc == 0);
	}

	/* Wait till all NET servers are started */
	for (i = 0; i < ARRAY_SIZE(cluster.items); ++i) {
		ok = m0_semaphore_timeddown(&startup_sem,
				            m0_time_from_now(43200, 0));
		M0_UT_ASSERT(ok);
	}

	/* Start the tranceive test. */
	m0_chan_broadcast_lock(&ut_net_tests_chan);

	/* Wait till the NET server test is done */
	for (i = 0; i < ARRAY_SIZE(cluster.items); ++i) {
		ok = m0_semaphore_timeddown(&startup_sem,
				            m0_time_from_now(43200, 0));
		M0_UT_ASSERT(ok);
	}

	/* Start the cluster down process. */
	m0_chan_broadcast_lock(&ut_net_tests_chan);

	for (i = 0; i < ARRAY_SIZE(cluster.items); ++i)
		m0_thread_join(&cluster.items[i].mn_thr);

	m0_dtm0_ut_fini();
}
#undef M0_TRACE_SUBSYSTEM

/** @} end of dtm0 group */

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
