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
 * Mapping for m0net-->libfabric API
 * xo_dom_init                    =  fi_getinfo, fi_fabric, fi_domain
 * xo_dom_fini                    =  fi_close, // free resources
 * xo_tm_init                     =  // Empty function ? fi_send
 * xo_tm_confine                  =
 * xo_tm_start                    =  // fi_send
 * xo_tm_stop                     =  fi_cancel
 * xo_tm_fini                     =  // Empty function
 * xo_end_point_create            =  fi_endpoint, fi_pep, fi_av, fi_cq, fi_cntr, fi_eq, fi_bind(av/cq/cntr/eq), fi_pep_bind
 * xo_buf_register                =  fi_mr_reg, fi_mr_desc, fi_mr_key, fi_mr_bind, fi_mr_enable
 * xo_buf_deregister              =  fi_close
 * xo_buf_add                     =  fi_send/fi_recv
 * xo_buf_del                     =  fi_cancel 
 * xo_bev_deliver_sync            =  Is it needed?
 * xo_bev_deliver_all             =  Is it needed?
 * xo_bev_pending                 =  Is it needed?
 * xo_bev_notify                  =  Is it needed?
 * xo_get_max_buffer_size         =  // need to define new API
 * xo_get_max_buffer_segment_size =  // need to define new functions
 * xo_get_max_buffer_segments     =  // need to define new functions
 * xo_get_max_buffer_desc_size    =  
 * 
 */

/**
 * @addtogroup netlibfab
 *
 * @{
 */

#define M0_TRACE_SUBSYSTEM M0_TRACE_SUBSYS_NET
#include "lib/trace.h"          /* M0_ENTRY() */
#include "net/net.h"            /* struct m0_net_domain */
#include "lib/memory.h"         /* M0_ALLOC_PTR()*/
#include "libfab_internal.h"    /* struct m0_fab__dom_param */

#define LIBFAB_VERSION FI_VERSION(FI_MAJOR_VERSION,FI_MINOR_VERSION)

static char *providers[] = { "verbs", "tcp", "sockets" };
/* TODO: Remove after merging EOS-15552 */
static char def_node[] = "127.0.0.1";
static char def_port[] = "1000";
/** Parameters required for libfabric configuration */
enum m0_fab__mr_params {
	/** Fabric memory access. */
	FAB_MR_ACCESS  = (FI_READ | FI_WRITE | FI_RECV | FI_SEND | \
			FI_REMOTE_READ | FI_REMOTE_WRITE),
	/** Fabric memory offset. */
	FAB_MR_OFFSET  = 0,
	/** Fabric memory flag. */
	FAB_MR_FLAG    = 0,
	/** Key used for memory registration. */
	FAB_MR_KEY     = 0XABCD,
};

static int libfab_ep_addr_decode(const char *ep_name, char *node, char *port);
static int libfab_ep_res_init(struct m0_fab__ep *ep);
static int libfab_pep_res_init(struct m0_fab__ep *ep);
static struct m0_fab__ep *libfab_ep_net(struct m0_net_end_point *net);
static bool libfab_ep_eq(struct m0_fab__ep *ep1, struct m0_fab__ep *ep2);
static int libfab_ep_find(struct m0_net_transfer_mc *tm, const char *name,
			  struct m0_net_end_point **epp);
static int libfab_ep_create(struct m0_net_transfer_mc *tm, const char *name, 
			    struct m0_net_end_point **epp);
static int libfab_active_ep_create(struct m0_fab__ep *ep);
static int libfab_passive_ep_create(struct m0_fab__ep *ep);
static int libfab_ep_param_free(struct m0_fab__ep *ep);
static int libfab_ep_res_free(struct m0_fab__ep_res *ep_res);
static void libfab_poller(struct m0_fab__tm *ma);

/* libfab init and fini() : initialized in motr init */
M0_INTERNAL int m0_net_libfab_init(void)
{
	int result = 0;

	
	/*  TODO: Uncomment it when all the changes are intergated
	*  commnet to avoid compilation ERROR 
	*  m0_net_xprt_register(&m0_net_libfab_xprt);
	*  m0_net_xprt_default_set(&m0_net_libfab_xprt);
	*/
	return M0_RC(result);
}

M0_INTERNAL void m0_net_libfab_fini(void)
{
	/*  TODO: Uncomment it when all the changes are intergated
	*  commnet to avoid compilation ERROR 
	*	m0_net_xprt_deregister(&m0_net_libfab_xprt);
	*/
}

/**
 * Used to poll for connection and completion events
 */
static void libfab_poller(struct m0_fab__tm *ma)
{
}

/**
 * This function will extract the ip addr and port from the given ep str
 */
static int libfab_ep_addr_decode(const char *ep_name, char *node, char *port)
{
	M0_PRE(ep_name != NULL);
	node = def_node;
	port = def_port;
	return M0_RC(0);
}

/** 
 * Converts generic end-point to its libfabric structure. 
 */
static struct m0_fab__ep *libfab_ep_net(struct m0_net_end_point *net)
{
	return container_of(net, struct m0_fab__ep, fep_nep);
}

/**
 * Compares thw two endpoints and returns true if equal, or else returs false
*/
static bool libfab_ep_eq(struct m0_fab__ep *ep1, struct m0_fab__ep *ep2)
{
	bool ret = false;

	if (strcmp(ep1->fep_name.fen_addr, ep2->fep_name.fen_addr) == 0 &&
	    strcmp(ep1->fep_name.fen_port, ep2->fep_name.fen_port) == 0)
	    	ret = true;

	return ret;
}

/**
 * Search for the ep in the existing ep list.
 * If found then return the ep structure, or else create a new endpoint 
 * with the name
 */
static int libfab_ep_find(struct m0_net_transfer_mc *tm, const char *name, 
			  struct m0_net_end_point **epp)
{
	struct m0_net_end_point *net;
	struct m0_fab__ep       *xep;
	struct m0_fab__ep        ep;
	bool                     found = false;
	int                      rc;

	M0_ENTRY();

	M0_PRE(name != NULL);
	rc = libfab_ep_addr_decode(name, ep.fep_name.fen_addr, 
				   ep.fep_name.fen_port);
	if (rc != FI_SUCCESS)
		return M0_ERR(-EINVAL);

	m0_tl_for(m0_nep, &tm->ntm_end_points, net) {
		xep = libfab_ep_net(net);
		/* TODO: libfab_ep_eq() */
		if (libfab_ep_eq(xep, &ep) == true) {
			*epp = &xep->fep_nep;
			found = true;
			break;
		}
	} m0_tl_endfor;

	if (found == false)
		rc = libfab_ep_create(tm, name, epp);

	return M0_RC(rc);
}

/**
 * Used to create an endpoint
 */
static int libfab_ep_create(struct m0_net_transfer_mc *tm, const char *name, 
			    struct m0_net_end_point **epp)
{
	struct m0_net_end_point *net;
	struct m0_fab__ep       *ep = NULL;
	int                      rc;

	M0_ENTRY();
	M0_PRE(name != NULL);

	M0_ALLOC_PTR(ep);
	if (ep == NULL)
		return M0_ERR(-ENOMEM);

	ep->fep_ep = NULL;
	ep->fep_pep = NULL;

	rc = libfab_ep_addr_decode(name, ep->fep_name.fen_addr,
				   ep->fep_name.fen_port);
	if (rc != FI_SUCCESS) {
		libfab_ep_param_free(ep);
		return M0_RC(rc);
	}

	rc = libfab_active_ep_create(ep);
	if (rc != FI_SUCCESS) {
		libfab_ep_param_free(ep);
		return M0_RC(rc);
	}

	net = &ep->fep_nep;
	net->nep_tm = tm;
	m0_nep_tlink_init_at_tail(net, &tm->ntm_end_points);
	net->nep_addr = (const char *)(&ep->fep_name);
	*epp = &ep->fep_nep;
	return M0_RC(rc);
}

/**
 * Init resources and bind it to the active endpoint.
 */
static int libfab_ep_res_init(struct m0_fab__ep *ep)
{
	struct fi_cq_attr   cq_attr;
	struct fi_eq_attr   eq_attr;
	struct fi_av_attr   av_attr;
	struct fi_cntr_attr cntr_attr;
	int                 rc = 0;
	
	M0_ENTRY();

	memset(&cq_attr, 0, sizeof(cq_attr));
	/* Initialise and bind completion queues for tx and rx */
	cq_attr.wait_obj = FI_WAIT_NONE;
	cq_attr.format = FI_CQ_FORMAT_UNSPEC;
	cq_attr.size = ep->fep_fi->tx_attr->size;
	
	rc = fi_cq_open(ep->fep_domain, &cq_attr, &ep->fep_ep_res.fer_tx_cq, 
			NULL);
	if (rc != FI_SUCCESS)
		return M0_RC(rc);
	
	cq_attr.size = ep->fep_fi->rx_attr->size;
	rc = fi_cq_open(ep->fep_domain, &cq_attr, &ep->fep_ep_res.fer_rx_cq, 
			NULL);
	if (rc != FI_SUCCESS)
		return M0_RC(rc);
	
	rc = fi_ep_bind(ep->fep_ep, &ep->fep_ep_res.fer_tx_cq->fid, FI_SEND);
	if (rc != FI_SUCCESS)
		return M0_RC(rc);
	rc = fi_ep_bind(ep->fep_ep, &ep->fep_ep_res.fer_rx_cq->fid, FI_RECV);
	if (rc != FI_SUCCESS)
		return M0_RC(rc);

	/* Initailise and bind event queue */
	memset(&eq_attr, 0, sizeof(eq_attr));
	eq_attr.wait_obj = FI_WAIT_UNSPEC;
	rc = fi_eq_open(ep->fep_fabric, &eq_attr, &ep->fep_ep_res.fer_eq, NULL);
	if (rc != FI_SUCCESS)
		return M0_RC(rc);
	
	rc = fi_ep_bind(ep->fep_ep, &ep->fep_ep_res.fer_eq->fid, 0);
	if (rc != FI_SUCCESS)
		return M0_RC(rc);

	/* Initailise and bind address vector */
	memset(&av_attr, 0, sizeof(av_attr));
	av_attr.type = FI_AV_UNSPEC;
	rc = fi_av_open(ep->fep_domain, &av_attr, &ep->fep_ep_res.fer_av, NULL);
	if (rc != FI_SUCCESS)
		return M0_RC(rc);

	rc = fi_ep_bind(ep->fep_ep, &ep->fep_ep_res.fer_av->fid, 0);
	if (rc != FI_SUCCESS)
		return M0_RC(rc);

	/* Initialise and bind counters */
	memset(&cntr_attr, 0, sizeof(cntr_attr));
	cntr_attr.wait_obj = FI_WAIT_NONE;
	rc = fi_cntr_open(ep->fep_domain, &cntr_attr, 
			  &ep->fep_ep_res.fer_tx_cntr, NULL);
	if (rc != FI_SUCCESS)
		return M0_RC(rc);

	rc = fi_cntr_open(ep->fep_domain, &cntr_attr, 
			  &ep->fep_ep_res.fer_rx_cntr, NULL);
	if (rc != FI_SUCCESS)
		return M0_RC(rc);

	rc = fi_ep_bind(ep->fep_ep, &ep->fep_ep_res.fer_tx_cntr->fid, 0);
	if (rc != FI_SUCCESS)
		return M0_RC(rc);
	rc = fi_ep_bind(ep->fep_ep, &ep->fep_ep_res.fer_rx_cntr->fid, 0);
	if (rc != FI_SUCCESS)
		return M0_RC(rc);

	return M0_RC(rc);
}

/**
 * Init resources and bind it to the passive endpoint.
 */
static int libfab_pep_res_init(struct m0_fab__ep *ep)
{
	struct fi_eq_attr eq_attr;
	int               rc = 0;
	
	M0_ENTRY();

	/* Initailise and bind event queue */
	memset(&eq_attr, 0, sizeof(eq_attr));
	eq_attr.wait_obj = FI_WAIT_UNSPEC;
	rc = fi_eq_open(ep->fep_fabric, &eq_attr, &ep->fep_ep_res.fer_eq, NULL);
	if (rc != FI_SUCCESS)
		return M0_RC(rc);
	
	rc = fi_pep_bind(ep->fep_pep, &ep->fep_ep_res.fer_eq->fid, 0);
	if (rc != FI_SUCCESS)
		return M0_RC(rc);

	ep->fep_ep_res.fer_av      = NULL;
	ep->fep_ep_res.fer_tx_cntr = NULL;
	ep->fep_ep_res.fer_rx_cntr = NULL;
	ep->fep_ep_res.fer_tx_cq   = NULL;
	ep->fep_ep_res.fer_rx_cq   = NULL;

	return M0_RC(rc);
}

/**
 * Used to create an active endpoint
 */
static int libfab_active_ep_create(struct m0_fab__ep *ep)
{
	struct fi_info *hints;
	int             i;
	int             rc;

	M0_ENTRY();

	hints = fi_allocinfo();
	if (hints == NULL) {
		libfab_ep_param_free(ep);
		return M0_ERR(-ENOMEM);
	}

	/* TODO: Set appropriate flags and hints
	 * fab->hints->ep_attr->type = FI_EP_MSG;
	 * fab->hints->caps = FI_MSG;
	 */
	
	for (i = 0; i < ARRAY_SIZE(providers); i++) {
		hints->fabric_attr->prov_name = providers[i];
		rc = fi_getinfo(LIBFAB_VERSION, ep->fep_name.fen_addr, 
				ep->fep_name.fen_port, 0, hints, &ep->fep_fi);
		if (rc == FI_SUCCESS)
			break;
	}
	M0_ASSERT(i < ARRAY_SIZE(providers));

	fi_freeinfo(hints);
	
	rc = fi_fabric(ep->fep_fi->fabric_attr, &ep->fep_fabric, NULL);
	if (rc != FI_SUCCESS) {
		libfab_ep_param_free(ep);
		return M0_RC(rc);
	}
	
	rc = fi_domain(ep->fep_fabric, ep->fep_fi, &ep->fep_domain, NULL);
	if (rc != FI_SUCCESS) {
		libfab_ep_param_free(ep);
		return M0_RC(rc);
	}

	rc = fi_endpoint(ep->fep_domain, ep->fep_fi, &ep->fep_ep, NULL);
	if (rc != FI_SUCCESS)
		libfab_ep_param_free(ep);
		return M0_RC(rc);

	rc = libfab_ep_res_init(ep);
	if (rc != FI_SUCCESS)
		libfab_ep_param_free(ep);
		return M0_RC(rc);

	/* TODO: Enable fi_connect call with remote addr 
	rc = fi_connect();
	*/

	return M0_RC(rc);
}

/**
 * Used to create a passive endpoint which will listen for incoming connection
 * requests
 */
static int libfab_passive_ep_create(struct m0_fab__ep *ep)
{
	struct fi_info *hints;
	int             i;
	int             rc;

	M0_ENTRY();

	ep->fep_ep = NULL;

	hints = fi_allocinfo();
	if (hints == NULL)
		return M0_ERR(-ENOMEM);

	/* TODO: Set appropriate flags and hints
	 * flags |= FI_SOURCE;
	 * fab->hints->ep_attr->type = FI_EP_MSG;
	 * fab->hints->caps = FI_MSG;
	 */
	
	for (i = 0; i < ARRAY_SIZE(providers); i++) {
		hints->fabric_attr->prov_name = providers[i];
		rc = fi_getinfo(LIBFAB_VERSION, NULL, NULL, FI_SOURCE, hints,
				&ep->fep_fi);
		if (rc == FI_SUCCESS)
			break;
	}
	M0_ASSERT(i < ARRAY_SIZE(providers));

	fi_freeinfo(hints);
	
	rc = fi_fabric(ep->fep_fi->fabric_attr, &ep->fep_fabric, NULL);
	if (rc != FI_SUCCESS) {
		libfab_ep_param_free(ep);
		return M0_RC(rc);
	}

	rc = fi_passive_ep(ep->fep_fabric, ep->fep_fi, &ep->fep_pep, NULL);
	if (rc != FI_SUCCESS) {
		libfab_ep_param_free(ep);
		return M0_RC(rc);
	}

	rc = libfab_pep_res_init(ep);
	if (rc != FI_SUCCESS) {
		libfab_ep_param_free(ep);
		return M0_RC(rc);
	}

	rc = fi_listen(ep->fep_pep);
	if (rc != FI_SUCCESS) {
		libfab_ep_param_free(ep);
		return M0_RC(rc);
	}

	return M0_RC(rc);
}

/**
 * Used to free the resources attached to an active ep
 */
static int libfab_ep_res_free(struct m0_fab__ep_res *ep_res)
{
	int rc = 0;
	
	M0_ENTRY();

	if (ep_res->fer_av != NULL){
		rc = fi_close(&(ep_res->fer_av)->fid);
		if (rc != FI_SUCCESS)
			M0_LOG(M0_ERROR, "fer_av fi_close ret=%d fid=%d",
			       rc, (int)(ep_res->fer_av)->fid.fclass);
	}

	if (ep_res->fer_eq != NULL){
		rc = fi_close(&(ep_res->fer_eq)->fid);
		if (rc != FI_SUCCESS)
			M0_LOG(M0_ERROR, "fer_eq fi_close ret=%d fid=%d",
			       rc, (int)(ep_res->fer_eq)->fid.fclass);
	}

	if (ep_res->fer_tx_cq != NULL){
		rc = fi_close(&(ep_res->fer_tx_cq)->fid);
		if (rc != FI_SUCCESS)
			M0_LOG(M0_ERROR, "fer_tx_cq fi_close ret=%d fid=%d",
			       rc, (int)(ep_res->fer_tx_cq)->fid.fclass);
	}
	
	if (ep_res->fer_rx_cq != NULL){
		rc = fi_close(&(ep_res->fer_rx_cq)->fid);
		if (rc != FI_SUCCESS)
			M0_LOG(M0_ERROR, "fer_rx_cq fi_close ret=%d fid=%d",
			       rc, (int)(ep_res->fer_rx_cq)->fid.fclass);
	}

	if (ep_res->fer_tx_cntr != NULL){
		rc = fi_close(&(ep_res->fer_tx_cntr)->fid);
		if (rc != FI_SUCCESS)
			M0_LOG(M0_ERROR, "fer_tx_cntr fi_close ret=%d fid=%d",
			       rc, (int)(ep_res->fer_tx_cntr)->fid.fclass);
	}

	if (ep_res->fer_rx_cntr != NULL){
		rc = fi_close(&(ep_res->fer_rx_cntr)->fid);
		if (rc != FI_SUCCESS)
			M0_LOG(M0_ERROR, "fer_rx_cntr fi_close ret=%d fid=%d",
			       rc, (int)(ep_res->fer_rx_cntr)->fid.fclass);
	}

	m0_free(ep_res);

	return M0_RC(rc);
}

/**
 * Used to free the active ep
 */
static int libfab_ep_param_free(struct m0_fab__ep *ep)
{
	int rc = 0;

	M0_ENTRY();

	rc = libfab_ep_res_free(&ep->fep_ep_res);
	
	if (ep->fep_pep != NULL) {
		rc = fi_close(&(ep->fep_pep)->fid);
		if (rc != FI_SUCCESS)
			M0_LOG(M0_ERROR, "fep_pep fi_close ret=%d fid=%d",
			       rc, (int)(ep->fep_pep)->fid.fclass);
	}

	if (ep->fep_ep != NULL) {
		rc = fi_close(&(ep->fep_ep)->fid);
		if (rc != FI_SUCCESS)
			M0_LOG(M0_ERROR, "fep_ep fi_close ret=%d fid=%d",
			       rc, (int)(ep->fep_ep)->fid.fclass);
	}
	
	if (ep->fep_domain != NULL) {
		rc = fi_close(&(ep->fep_domain)->fid);
		if (rc != FI_SUCCESS)
			M0_LOG(M0_ERROR, "fep_domain fi_close ret=%d fid=%d",
			       rc, (int)(ep->fep_domain)->fid.fclass);
	}

	if (ep->fep_fabric != NULL) {
		rc = fi_close(&(ep->fep_fabric)->fid);
		if (rc != FI_SUCCESS)
			M0_LOG(M0_ERROR, "fep_fabric fi_close ret=%d fid=%d",
			       rc, (int)(ep->fep_fabric)->fid.fclass);
	}

	if (ep->fep_fi != NULL) {
		fi_freeinfo(ep->fep_fi);
		ep->fep_fi = NULL;
	}

	memset(&ep->fep_name, 0, sizeof(ep->fep_name));

	m0_free(ep);

	return M0_RC(rc);

}

/**
 * Used to free the transfer machine params
 */
static int libfab_tm_param_free(struct m0_fab__tm *tm)
{
	struct m0_net_end_point *net;
	struct m0_fab__ep       *xep;
	int                      rc = 0;

	M0_ENTRY();

	if (tm->ftm_waitset != NULL) {
		rc = fi_close(&(tm->ftm_waitset)->fid);
		if (rc != FI_SUCCESS)
			M0_LOG(M0_ERROR, "ftm_waitset fi_close ret=%d fid=%d",
			       rc, (int)(tm->ftm_waitset)->fid.fclass);
	}
	
	if (tm->ftm_pollset != NULL) {
		rc = fi_close(&(tm->ftm_pollset)->fid);
		if (rc != FI_SUCCESS)
			M0_LOG(M0_ERROR, "ftm_pollset fi_close ret=%d fid=%d",
			       rc, (int)(tm->ftm_pollset)->fid.fclass);
	}

	if (tm->ftm_pep != NULL) {
		rc = libfab_ep_param_free(tm->ftm_pep);
		if (rc != FI_SUCCESS)
			M0_LOG(M0_ERROR, "ftm_pep fi_close ret=%d fid=%d",
			       rc, (int)(tm->ftm_pep->fep_pep)->fid.fclass);
	}

	if (tm->ftm_poller.t_func != NULL) {
		m0_thread_join(&tm->ftm_poller);
		m0_thread_fini(&tm->ftm_poller);
	}

	m0_tl_for(m0_nep, &tm->ftm_net_ma->ntm_end_points, net) {
		xep = libfab_ep_net(net);
		rc = libfab_ep_param_free(xep);
	} m0_tl_endfor;
	
	m0_free(tm);

	return M0_RC(rc);
}

/*============================================================================*/

/** 
 * Used as m0_net_xprt_ops::xo_dom_init(). 
 */
static int libfab_dom_init(struct m0_net_xprt *xprt, struct m0_net_domain *dom)
{
	M0_ENTRY();
	return M0_RC(0);
}

/** 
 * Used as m0_net_xprt_ops::xo_dom_fini(). 
 */
static void libfab_dom_fini(struct m0_net_domain *dom)
{
	M0_ENTRY();
	M0_LEAVE();
}

/**
 * Used as m0_net_xprt_ops::xo_ma_fini().
 */
static void libfab_ma_fini(struct m0_net_transfer_mc *tm)
{
	struct m0_fab__tm *ma = tm->ntm_xprt_private;
	int                rc = 0;

	M0_ENTRY();
	
	rc = libfab_tm_param_free(ma);
	if (rc != FI_SUCCESS)
		M0_LOG(M0_ERROR, "libfab_tm_param_free ret=%d",	rc);

	tm->ntm_xprt_private = NULL;
	M0_LEAVE();
}

/**
 * Initialises transport-specific part of the transfer machine.
 *
 * Used as m0_net_xprt_ops::xo_tm_init().
 */
static int libfab_ma_init(struct m0_net_transfer_mc *tm)
{
	struct m0_fab__tm *ma;
	int                rc = 0;

	/* TODO: open passive ep, start poller thread 
	*  net->ntm_dom->nd_xprt_private = (m0_fab__ep *)ep
	*/

	M0_ASSERT(tm->ntm_xprt_private == NULL);
	M0_ALLOC_PTR(ma);
	if (ma != NULL) {
		ma->ftm_shutdown = false;
		tm->ntm_xprt_private = ma;
		ma->ftm_net_ma = tm;
		M0_ALLOC_PTR(ma->ftm_pep);
		if (ma->ftm_pep != NULL)
			rc = libfab_passive_ep_create(ma->ftm_pep);
			tm->ntm_dom->nd_xprt_private = ma->ftm_pep;
			if (rc == FI_SUCCESS)
				rc = M0_THREAD_INIT(&ma->ftm_poller,
						    struct m0_fab__tm *, NULL, 
						    &libfab_poller, ma, 
						    "libfab_tm");
		else
			rc = M0_ERR(-ENOMEM);
	} else
		rc = M0_ERR(-ENOMEM);

	if (rc != 0)
		libfab_ma_fini(tm);
	return M0_RC(rc);
}

/**
 * Starts initialised ma.
 *
 * Initialises everything that libfab_ma_init() didn't. Note that ma is in
 * M0_NET_TM_STARTING state after this returns. Switch to M0_NET_TM_STARTED
 * happens when the poller thread posts special event.
 *
 * Used as m0_net_xprt_ops::xo_tm_start().
 */
static int libfab_ma_start(struct m0_net_transfer_mc *net, const char *name)
{
	/*
	* TODO:
	* poller thread needs to be added to check completion queue, 
	* refer nlx_xo_tm_start() LNet 
	*/
	return M0_RC(0);
}

/**
 * Stops a ma that has been started or is being started.
 *
 *
 * Used as m0_net_xprt_ops::xo_tm_stop().
 */
static int libfab_ma_stop(struct m0_net_transfer_mc *net, bool cancel)
{
	/* TODO: fi_cancel () */

	return 0;
}

/**
 * Used as m0_net_xprt_ops::xo_ma_confine().
 */
static int libfab_ma_confine(struct m0_net_transfer_mc *ma,
		      const struct m0_bitmap *processors)
{
	return -ENOSYS;
}

/**
 * Returns an end-point with the given name.
 *
 * Used as m0_net_xprt_ops::xo_end_point_create().
 *
 * @see m0_net_end_point_create().
 */
static int libfab_end_point_create(struct m0_net_end_point **epp,
				   struct m0_net_transfer_mc *tm,
				   const char *name)
{
	int rc = 0;

	M0_ENTRY();

	rc = libfab_ep_find(tm, name, epp);
	if (rc != 0)
		return M0_RC(rc);

	return M0_RC(rc);
}

/**
 * Deregister a network buffer.
 *
 * Used as m0_net_xprt_ops::xo_buf_deregister().
 *
 * @see m0_net_buffer_deregister().
 */
static void libfab_buf_deregister(struct m0_net_buffer *nb)
{
	struct m0_fab__buf *fbp = nb->nb_xprt_private;
	int                 ret;

	ret = fi_close(&fbp->fbp_mr->fid);
	M0_PRE(ret == FI_SUCCESS);
	m0_free(fbp);
	nb->nb_xprt_private = NULL;
}

/**
 * Register a network buffer.
 *
 * Used as m0_net_xprt_ops::xo_buf_register().
 *
 * @see m0_net_buffer_register().
 */
static int libfab_buf_register(struct m0_net_buffer *nb)
{
	/* TODO: Generate and save desc in m0_fab__buf struct*/
	struct m0_fab__ep  *ep = nb->nb_dom->nd_xprt_private;
	struct fid_domain  *dp = ep->fep_domain;
	struct m0_fab__buf *fbp;
	int                 ret;

	M0_PRE(nb->nb_xprt_private == NULL);

	M0_ALLOC_PTR(fbp);
	if (fbp == NULL)
		return M0_ERR(-ENOMEM);

	nb->nb_xprt_private = fbp;
	fbp->fbp_nb = nb;
	/* Registers buffer that can be used for send/recv and local/remote RMA. */
	ret = fi_mr_reg(dp, nb->nb_buffer.ov_buf[0], nb->nb_length,
			FAB_MR_ACCESS, FAB_MR_OFFSET, FAB_MR_KEY,
			FAB_MR_FLAG, &fbp->fbp_mr, NULL);	
	if (ret != FI_SUCCESS)
	{
		nb->nb_xprt_private = NULL;
		m0_free(fbp);
		return M0_ERR(ret);
	}
	
	ret = fi_mr_enable(fbp->fbp_mr);
	if (ret != FI_SUCCESS)
		libfab_buf_deregister(nb); /* Failed to enable memory region */

	return M0_RC(ret);
}

/**
 * Adds a network buffer to a ma queue.
 *
 * Used as m0_net_xprt_ops::xo_buf_add().
 *
 * @see m0_net_buffer_add().
 */
static int libfab_buf_add(struct m0_net_buffer *nb)
{
	int        result = 0;
	/*
 	* TODO:
 	*   fi_send/fi_recv
 	* */
	return M0_RC(result);
}

/**
 * Cancels a buffer operation..
 *
 * Used as m0_net_xprt_ops::xo_buf_del().
 *
 * @see m0_net_buffer_del().
 */
static void libfab_buf_del(struct m0_net_buffer *nb)
{

	/*
 	* TODO:
 	*   fi_cancel
 	* */
}

static int libfab_bev_deliver_sync(struct m0_net_transfer_mc *ma)
{
	/*
 	* TODO:
 	* Check if it is required ?
 	* */
	return 0;
}

static void libfab_bev_deliver_all(struct m0_net_transfer_mc *ma)
{
	/*
 	* TODO:
 	* Check if it is required ?
 	* */
}

static bool libfab_bev_pending(struct m0_net_transfer_mc *ma)
{
	/*
 	* TODO:
 	* Check if it is required ?
 	* */
	return false;
}

static void libfab_bev_notify(struct m0_net_transfer_mc *ma, struct m0_chan *chan)
{
	/*
 	* TODO:
 	* Check if it is required ?
 	* */
}

/**
 * Maximal number of bytes in a buffer.
 *
 * Used as m0_net_xprt_ops::xo_get_max_buffer_size()
 *
 * @see m0_net_domain_get_max_buffer_size()
 */
static m0_bcount_t libfab_get_max_buffer_size(const struct m0_net_domain *dom)
{
	/*
 	* TODO:
 	* Explore libfab code and return approriate value based on
 	* underlying protocol used i.e. tcp/udp/verbs
 	* Might have to add switch case based on protocol used 
 	* */
	return M0_BCOUNT_MAX / 2;
}

/**
 * Maximal number of bytes in a buffer segment.
 *
 * Used as m0_net_xprt_ops::xo_get_max_buffer_segment_size()
 *
 * @see m0_net_domain_get_max_buffer_segment_size()
 */
static m0_bcount_t libfab_get_max_buffer_segment_size(const struct m0_net_domain *dom)
{
	/*
 	* TODO:
 	* same as get_max_buffer_size()
	* This is maximum size of buffer segment size
 	* */
	return M0_BCOUNT_MAX / 2;
}

/**
 * Maximal number of segments in a buffer
 *
 * Used as m0_net_xprt_ops::xo_get_max_buffer_segments()
 *
 * @see m0_net_domain_get_max_buffer_segments()
 */
static int32_t libfab_get_max_buffer_segments(const struct m0_net_domain *dom)
{
	/*
 	* TODO:
 	* same as libfab_get_max_buffer_size()
	* This is maximum number of segments supported 
	* */
	return INT32_MAX / 2; /* Beat this, LNet! */
}

/**
 * Maximal number of bytes in a buffer descriptor.
 *
 * Used as m0_net_xprt_ops::xo_get_max_buffer_desc_size()
 *
 * @see m0_net_domain_get_max_buffer_desc_size()
 */
static m0_bcount_t libfab_get_max_buffer_desc_size(const struct m0_net_domain *dom)
{
	/*
 	* TODO:
 	* same as libfab_get_max_buffer_size()
	* This is size of buffer descriptor structure size, refer fi_mr_desc() 
	* */
	//return sizeof(struct bdesc);
	return 0;
}

static const struct m0_net_xprt_ops libfab_xprt_ops = {
	.xo_dom_init                    = &libfab_dom_init,
	.xo_dom_fini                    = &libfab_dom_fini,
	.xo_tm_init                     = &libfab_ma_init,
	.xo_tm_confine                  = &libfab_ma_confine,
	.xo_tm_start                    = &libfab_ma_start,
	.xo_tm_stop                     = &libfab_ma_stop,
	.xo_tm_fini                     = &libfab_ma_fini,
	.xo_end_point_create            = &libfab_end_point_create,
	.xo_buf_register                = &libfab_buf_register,
	.xo_buf_deregister              = &libfab_buf_deregister,
	.xo_buf_add                     = &libfab_buf_add,
	.xo_buf_del                     = &libfab_buf_del,
	.xo_bev_deliver_sync            = &libfab_bev_deliver_sync,
	.xo_bev_deliver_all             = &libfab_bev_deliver_all,
	.xo_bev_pending                 = &libfab_bev_pending,
	.xo_bev_notify                  = &libfab_bev_notify,
	.xo_get_max_buffer_size         = &libfab_get_max_buffer_size,
	.xo_get_max_buffer_segment_size = &libfab_get_max_buffer_segment_size,
	.xo_get_max_buffer_segments     = &libfab_get_max_buffer_segments,
	.xo_get_max_buffer_desc_size    = &libfab_get_max_buffer_desc_size
};

struct m0_net_xprt m0_net_libfab_xprt = {
	.nx_name = "libfab",
	.nx_ops  = &libfab_xprt_ops
};
M0_EXPORTED(m0_net_libfab_xprt);

#undef M0_TRACE_SUBSYSTEM

/** @} end of netlibfab group */

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
