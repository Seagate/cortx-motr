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

#include "dtm0/net.h"
#include "ut/ut.h"              /* M0_UT_ASSERT */
#include "fid/fid.h"            /* M0_FID_INIT */

#include "dtm0/service.h"       /* m0_dtm0_service */
#include "dtm0/ut/helper.h"     /* m0_ut_dtm0_helper_init */


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
	m0_reqh_services_terminate(&net->reqh);
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

	M0_ALLOC_PTR(h);
	M0_UT_ASSERT(h != NULL);

	m0_ut_dtm0_net_helper_init(h);
	target = &h->base.udh_client_dtm0_fid;

	M0_ALLOC_ARR(fid, MSG_NR);
	M0_UT_ASSERT(fid != NULL);
	for (i = 0; i < MSG_NR; ++i) {
		fid[i] = M0_FID_INIT(0, i+1);  /* TODO set fid type */
	}
	M0_ALLOC_ARR(fop, MSG_NR);
	M0_UT_ASSERT(fop != NULL);
	for (i = 0; i < MSG_NR; ++i) {
		fop[i] = M0_DTM0_MSG_EOL_INIT(&fid[i]);
	}
	M0_ALLOC_ARR(op, MSG_NR);
	M0_UT_ASSERT(op != NULL);
	for (i = 0; i < MSG_NR; ++i)
		m0_be_op_init(&op[i]);

	for (i = 0; i < MSG_NR; ++i)
		m0_dtm0_net_send(&h->dnet_server,
				 &op[i], target, &fop[i], NULL);

	m0_be_op_init(&op_out);
	for (i = 0; i < MSG_NR; ++i) {
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
		j = m0_fold(m, a, MSG_NR, -1,
			    a  == -1 ? m0_fid_eq(&fid_out, &fid[m]) ? m :
			    a : a);
		M0_UT_ASSERT(j != -1);
		M0_UT_ASSERT(fid[j].f_key == j + 1);
		fid[j] = M0_FID0;
	}
	m0_be_op_fini(&op_out);
	for (i = 0; i < MSG_NR; ++i)
		M0_UT_ASSERT(m0_fid_eq(&fid[i], &M0_FID0));
	m0_free(fid);
	for (i = 0; i < MSG_NR; ++i) {
		m0_be_op_wait(&op[i]);
		m0_be_op_fini(&op[i]);
	}
	m0_free(op);
	m0_free(fop);

	m0_ut_dtm0_net_helper_fini(h);
	m0_free(h);
}

struct ut_motr_cluster {
	struct ut_motr_net items[CLUSTER_SIZE];
};

static void ut_motr_cluster_init(struct ut_motr_cluster *cluster)
{
	struct m0_fid process_fid = g_process_fid;
	struct endpoint endpoint = {};
	char *ep_addr = &endpoint.as_string[0];
	int i;

	for (i = 0; i < ARRAY_SIZE(cluster->items); ++i) {
		(void) sprintf(ep_addr, "inet:tcp:127.0.0.1@220%d0", i);
		process_fid.f_key = i;
		ut_motr_net_init(&cluster->items[i], &process_fid, &endpoint);
	}
}

static void ut_motr_cluster_fini(struct ut_motr_cluster *cluster)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(cluster->items); ++i) {
		ut_motr_net_fini(&cluster->items[i]);
	}
}

static void ut_motr_cluster_start(struct ut_motr_cluster *cluster)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(cluster->items); ++i) {
		ut_motr_net_start(&cluster->items[i]);
	}
}

static void ut_motr_cluster_stop(struct ut_motr_cluster *cluster)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(cluster->items); ++i) {
		ut_motr_net_stop(&cluster->items[i]);
	}
}

void m0_dtm0_ut_net_thin_init_fini(void)
{
	struct ut_motr_cluster cluster = {};
	ut_motr_cluster_init(&cluster);
	ut_motr_cluster_fini(&cluster);
}

void m0_dtm0_ut_net_thin_start_stop(void)
{
	struct ut_motr_cluster cluster = {};
	ut_motr_cluster_init(&cluster);
	ut_motr_cluster_start(&cluster);
	ut_motr_cluster_stop(&cluster);
	ut_motr_cluster_fini(&cluster);
}

static void ut_blob_init(struct m0_dtm0_msg *msg)
{
	static char message[] = "Hello";

	msg->dm_type = M0_DMT_BLOB;
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
	M0_UT_ASSERT(false);
	m0_dtm0_net_send(&client->dnet, op, &server->reqh.rh_fid, msg, NULL);
}

static void ut_motr_net_recv(struct ut_motr_net *client,
			     struct m0_be_op *op,
			     bool *successful,
			     struct m0_dtm0_msg *msg)
{
	m0_dtm0_net_recv(&client->dnet, op, successful, msg, M0_DMT_BLOB);
}

static void ut_motr_cluster_tranceive_simple(struct ut_motr_cluster *cluster,
					     int client_id, int server_id)
{
	struct ut_motr_net *client = &cluster->items[client_id];
	struct ut_motr_net *server = &cluster->items[server_id];
	struct m0_be_op snd_op;
	struct m0_be_op rcv_op;
	struct m0_dtm0_msg snd_msg;
	struct m0_dtm0_msg rcv_msg;
	bool               rcv_successful;
	int                rc;

	m0_be_op_init(&snd_op);
	m0_be_op_init(&rcv_op);

	ut_blob_init(&snd_msg);

	ut_motr_net_send(client,  &snd_op, server, &snd_msg);
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

void m0_dtm0_ut_net_thin_tranceive(void)
{
	enum { CLIENT = 0, SERVER = 1 };
	struct ut_motr_cluster cluster = {};
	ut_motr_cluster_init(&cluster);
	ut_motr_cluster_start(&cluster);
	ut_motr_cluster_tranceive_simple(&cluster, CLIENT, SERVER);
	ut_motr_cluster_stop(&cluster);
	ut_motr_cluster_fini(&cluster);
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
