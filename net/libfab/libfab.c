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
#include "libfab_internal.h"
#include "net/net_internal.h"   /* m0_net__buffer_invariant() */

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

M0_TL_DESCR_DEFINE(fab_buf, "libfab_buf",
		   static, struct m0_fab__buf, fb_linkage, fb_magic,
		   M0_NET_LIBFAB_BUF_MAGIC, M0_NET_LIBFAB_BUF_HEAD_MAGIC);
M0_TL_DEFINE(fab_buf, static, struct m0_fab__buf);

static int libfab_ep_addr_decode(const char *ep_name, char *node, char *port);
static int libfab_ep_res_init(struct m0_fab__ep *ep, struct m0_fab__tm *tm);
static int libfab_pep_res_init(struct m0_fab__ep *ep, struct m0_fab__tm *tm);
static struct m0_fab__ep *libfab_ep_net(struct m0_net_end_point *net);
static bool libfab_ep_eq(struct m0_fab__ep *ep1, struct m0_fab__ep *ep2);
static int libfab_ep_find(struct m0_net_transfer_mc *tm, const char *name,
			  struct m0_net_end_point **epp);
static int libfab_ep_create(struct m0_net_transfer_mc *tm, const char *name, 
			    struct m0_net_end_point **epp);
static int libfab_active_ep_create(struct m0_fab__ep *ep,
				   struct m0_fab__tm *tm,
				   struct fi_info *fi);
static int libfab_passive_ep_create(struct m0_fab__ep *ep,
				    struct m0_fab__tm *tm);
static int libfab_ep_param_free(struct m0_fab__ep *ep, struct m0_fab__tm *tm);
static int libfab_ep_res_free(struct m0_fab__ep_res *ep_res, 
			      struct m0_fab__tm *tm);
static void libfab_poller(struct m0_fab__tm *ma);
static int libfab_waitset_init(struct m0_fab__tm *tm, struct m0_fab__ep *ep);
static int libfab_pollset_init(struct m0_fab__tm *tm, struct m0_fab__ep *ep);
static void libfab_tm_event_post(struct m0_fab__tm *tm, 
				 enum m0_net_tm_state state);
static void libfab_tm_lock(struct m0_fab__tm *tm);
static void libfab_tm_unlock(struct m0_fab__tm *tm);
static bool libfab_tm_is_locked(const struct m0_fab__tm *tm);
static void libfab_buf_complete(struct m0_fab__buf *buf, int32_t status);
static void libfab_buf_done(struct m0_fab__buf *buf, int rc);
static bool libfab_tm_invariant(const struct m0_fab__tm *tm);

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

static void libfab_tm_lock(struct m0_fab__tm *tm)
{
	m0_mutex_lock(&tm->ftm_net_ma->ntm_mutex);
}

static void libfab_tm_unlock(struct m0_fab__tm *tm)
{
	m0_mutex_unlock(&tm->ftm_net_ma->ntm_mutex);
}

static bool libfab_tm_is_locked(const struct m0_fab__tm *tm)
{
	return m0_mutex_is_locked(&tm->ftm_net_ma->ntm_mutex);
}

/**
 * Helper function that posts a tm state change event.
 */
static void libfab_tm_event_post(struct m0_fab__tm *tm,
				 enum m0_net_tm_state state)
{
	struct m0_net_end_point *listen = NULL;

	if (state == M0_NET_TM_STARTED) {
		// Check for LISTENING Passive endpoint
		listen = &tm->ftm_pep->fep_nep;
		M0_ASSERT(listen != NULL);
	}
	
	m0_net_tm_event_post(&(struct m0_net_tm_event) {
			.nte_type       = M0_NET_TEV_STATE_CHANGE,
			.nte_next_state = state,
			.nte_time       = m0_time_now(),
			.nte_ep         = listen,
			.nte_tm         = tm->ftm_net_ma,
	});
}

/**
 * Finds queued buffers that timed out and completes them with a
 * prejudice error.
 */
static void libfab_tm_buf_timeout(struct m0_fab__tm *ftm)
{
	struct m0_net_transfer_mc *net = ftm->ftm_net_ma;
	int                        i;
	m0_time_t                  now = m0_time_now();

	M0_PRE(libfab_tm_invariant(ftm));
	for (i = 0; i < ARRAY_SIZE(net->ntm_q); ++i) {
		struct m0_net_buffer *nb;

		m0_tl_for(m0_net_tm, &ftm->ftm_net_ma->ntm_q[i], nb) {
			if (nb->nb_timeout < now) {
				nb->nb_flags |= M0_NET_BUF_TIMED_OUT;
				libfab_buf_done(nb->nb_xprt_private,
						-ETIMEDOUT);
			}
		} m0_tl_endfor;
	}
	M0_POST(libfab_tm_invariant(ftm));
}

/**
 * Finds buffers pending completion and completes them.
 *
 * A buffer is placed on ma::t_done queue when its operation is done, but the
 * completion call-back cannot be immediately invoked, for example, because
 * completion happened in a synchronous context.
 */
static void libfab_tm_buf_done(struct m0_fab__tm *ftm)
{
	struct m0_fab__buf *buffer;
	int                 nr = 0;

	M0_PRE(libfab_tm_is_locked(ftm) && libfab_tm_invariant(ftm));
	m0_tl_for(fab_buf, &ftm->ftm_done, buffer) {
		fab_buf_tlist_del(buffer);
		libfab_buf_complete(buffer, 0);
		nr++;
	} m0_tl_endfor;

	if (nr > 0 && ftm->ftm_net_ma->ntm_callback_counter == 0)
		m0_chan_broadcast(&ftm->ftm_net_ma->ntm_chan);
	M0_POST(libfab_tm_invariant(ftm));
}

/**
 * Used to monitor connected events
 */
static uint32_t libfab_handle_connected_events(struct m0_fab__tm *tm)
{
	struct m0_fab__ep       *ep;
	struct m0_net_end_point *net;
	int                      rc;
	struct fi_eq_cm_entry    entry;
	uint32_t                 event;
	uint32_t                 event_cnt = 0;

	/* Check for FI_CONNECTED events in case of active endpoints */
	m0_tl_for(m0_nep, &tm->ftm_net_ma->ntm_end_points, net) {
		ep = libfab_ep_net(net);
		rc = fi_eq_read(ep->fep_ep_res.fer_eq, &event,
				&entry, sizeof(entry), 0);
		if (rc == sizeof(entry)) {
			if (event == FI_CONNECTED) {
				M0_LOG(M0_INFO, "Received  FI_CONNECTED event");
				M0_ASSERT(entry.fid ==
					  &ep->fep_ep_res.fer_eq->fid);
			}
		}
		event_cnt++;
	} m0_tl_endfor;
	return event_cnt;
}

/**
 * Used to monitor connection request events
 */
static uint32_t libfab_handle_connect_request_events(struct m0_fab__tm *tm)
{
	struct m0_fab__ep       *ep = NULL;
	struct m0_net_end_point *net;
	int                      rc;
	struct fid_eq           *eq;
	struct fi_eq_err_entry   eq_err;
	struct fi_eq_cm_entry    entry;
	uint32_t                 event;

	eq = tm->ftm_pep->fep_ep_res.fer_eq;
	rc = fi_eq_read(eq, &event, &entry, sizeof(entry), 0);
	if (rc == sizeof(entry)) {
		if (event == FI_CONNREQ) {
			M0_ALLOC_PTR(ep);
			if (ep != NULL) {
				ep->fep_ep = NULL;
				ep->fep_pep = NULL;
				ep->fep_fi = entry.info;
				rc = libfab_active_ep_create(ep, tm,
							     entry.info);
				if (rc == FI_SUCCESS) {
					tm->ftm_net_ma->ntm_dom->nd_xprt_private
									   = ep;
					net = &ep->fep_nep;
					net->nep_tm = tm->ftm_net_ma;
					m0_nep_tlink_init_at_tail(net,
					&tm->ftm_net_ma->ntm_end_points);
				} else {
					M0_LOG(M0_ERROR, "Failed to create "\
					       "active endpoint = %d", rc);
					libfab_ep_param_free(ep, tm);
				}
			}
		} else
			M0_LOG(M0_ERROR, "Received unwanted event = %d", event);
	} else {
		memset(&eq_err, 0, sizeof(eq_err));
		rc = fi_eq_readerr(eq, &eq_err, 0);
		if (rc != sizeof(eq_err)) {
			M0_LOG(M0_ERROR, "fi_eq_readerr returns error =%s",
			       fi_strerror((int) -(rc)));
		} else {
			M0_LOG(M0_ERROR, "fi_eq_readerr provider err no %d:%s",
				eq_err.prov_errno,
				fi_eq_strerror(eq, eq_err.prov_errno,
					       eq_err.err_data, NULL, 0));
		}
	}
	return 1;
}

/**
 * Used to poll for connection and completion events
 */
static void libfab_poller(struct m0_fab__tm *tm)
{
	struct fi_cq_entry       comp;
	int                      wait_cnt;
	int                      poll_cnt;
	void                    *ctx[8];
	int                      i;
	int                      rc = 0;
	struct fi_cq_err_entry   cq_err;
	uint32_t                 cnt;

	while (tm->ftm_shutdown == false) {

		while(1) {
			if (m0_mutex_trylock(&tm->ftm_net_ma->ntm_mutex) != 0) {
				libfab_tm_lock(tm);
				libfab_tm_unlock(tm);
			} else
				break;
		}
		M0_ASSERT(libfab_tm_is_locked(tm) && libfab_tm_invariant(tm));

		memset(ctx, 0, sizeof(ctx));
		wait_cnt = fi_wait(tm->ftm_waitset, -1);
		if (wait_cnt) {
			poll_cnt = fi_poll(tm->ftm_pollset, ctx,
					   ARRAY_SIZE(ctx));
			for (i = 0; i < poll_cnt; i++) {
				rc = fi_cq_read(ctx[i], &comp, 1);
				if (rc > 0)
					libfab_buf_done(comp.op_context, 0);
				else {
					/* In case of FI_EAGAIN wait for
					* completion and do not cancel
					*/
					if (rc != -FI_EAGAIN) {
						rc = fi_cq_readerr(ctx[i],
								   &cq_err, 0);
						if ( rc >= 0) {
							M0_LOG(M0_ERROR,
							"fi_cq_readerr err:%d",
							cq_err.err);
						}
						libfab_buf_done(comp.op_context,
								-ECANCELED);
					}
				}
			}

			M0_ASSERT(wait_cnt >= poll_cnt);
			if (wait_cnt != poll_cnt) {
				cnt = 0;
				/* Check for connection request events*/
				cnt = libfab_handle_connect_request_events(tm);
				/* Monitor connection established events*/
				cnt += libfab_handle_connected_events(tm);
				M0_ASSERT((wait_cnt - poll_cnt) <= cnt );
			}
		}
		libfab_tm_buf_timeout(tm);
		libfab_tm_buf_done(tm);

		M0_ASSERT(libfab_tm_invariant(tm));
		libfab_tm_unlock(tm);
	}
}

/**
 * This function will extract the ip addr and port from the given ep str
 */
static int libfab_ep_addr_decode(const char *ep_name, char *node, char *port)
{
	M0_PRE(ep_name != NULL);
	strcpy(node, def_node);
	strcpy(port, def_port);
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
	struct m0_fab__tm       *ma = tm->ntm_xprt_private;
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
		libfab_ep_param_free(ep, ma);
		return M0_RC(rc);
	}

	rc = libfab_active_ep_create(ep, ma, NULL);
	if (rc != FI_SUCCESS) {
		libfab_ep_param_free(ep, ma);
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
static int libfab_ep_res_init(struct m0_fab__ep *ep, struct m0_fab__tm *tm)
{
	struct fi_cq_attr   cq_attr;
	struct fi_eq_attr   eq_attr;
	struct fi_av_attr   av_attr;
	struct fi_cntr_attr cntr_attr;
	int                 rc = 0;
	
	M0_ENTRY();
	
	M0_PRE(tm->ftm_waitset != NULL);
	M0_PRE(tm->ftm_pollset != NULL);

	memset(&cq_attr, 0, sizeof(cq_attr));
	/* Initialise and bind completion queues for tx and rx */
	cq_attr.format = FI_CQ_FORMAT_CONTEXT;
	cq_attr.wait_obj = FI_WAIT_SET;
	cq_attr.wait_cond = FI_CQ_COND_NONE;
	cq_attr.wait_set = tm->ftm_waitset;
	
	cq_attr.size = ep->fep_fi->tx_attr->size;
	rc = fi_cq_open(ep->fep_domain, &cq_attr, &ep->fep_ep_res.fer_tx_cq, 
			ep->fep_ep_res.fer_tx_cq);
	if (rc != FI_SUCCESS)
		return M0_RC(rc);
	
	cq_attr.size = ep->fep_fi->rx_attr->size;
	rc = fi_cq_open(ep->fep_domain, &cq_attr, &ep->fep_ep_res.fer_rx_cq, 
			ep->fep_ep_res.fer_rx_cq);
	if (rc != FI_SUCCESS)
		return M0_RC(rc);
	
	rc = fi_ep_bind(ep->fep_ep, &ep->fep_ep_res.fer_tx_cq->fid, FI_SEND);
	if (rc != FI_SUCCESS)
		return M0_RC(rc);
	rc = fi_ep_bind(ep->fep_ep, &ep->fep_ep_res.fer_rx_cq->fid, FI_RECV);
	if (rc != FI_SUCCESS)
		return M0_RC(rc);

	rc = fi_poll_add(tm->ftm_pollset, &ep->fep_ep_res.fer_tx_cq->fid, 0);
	if (rc != FI_SUCCESS)
		return M0_RC(rc);
	rc = fi_poll_add(tm->ftm_pollset, &ep->fep_ep_res.fer_rx_cq->fid, 0);
	if (rc != FI_SUCCESS)
		return M0_RC(rc);
	

	/* Initailise and bind event queue */
	memset(&eq_attr, 0, sizeof(eq_attr));
	eq_attr.wait_obj = FI_WAIT_SET;
	eq_attr.wait_set = tm->ftm_waitset;
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
static int libfab_pep_res_init(struct m0_fab__ep *ep, struct m0_fab__tm *tm)
{
	struct fi_eq_attr eq_attr;
	int               rc = 0;
	
	M0_ENTRY();

	M0_PRE(tm->ftm_waitset != NULL);

	/* Initailise and bind event queue */
	memset(&eq_attr, 0, sizeof(eq_attr));
	eq_attr.wait_obj = FI_WAIT_SET;
	eq_attr.wait_set = tm->ftm_waitset;
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
static int libfab_active_ep_create(struct m0_fab__ep *ep, struct m0_fab__tm *tm,
				   struct fi_info *fi)
{
	struct fi_info *hints;
	int             i;
	int             rc;

	M0_ENTRY();

	if (fi == NULL) {
		hints = fi_allocinfo();
		if (hints == NULL) {
			libfab_ep_param_free(ep, tm);
			return M0_ERR(-ENOMEM);
		}

		/* TODO: Set appropriate flags and hints
		 * fab->hints->ep_attr->type = FI_EP_MSG;
		 * fab->hints->caps = FI_MSG;
		 */

		for (i = 0; i < ARRAY_SIZE(providers); i++) {
			hints->fabric_attr->prov_name = providers[i];
			rc = fi_getinfo(LIBFAB_VERSION, ep->fep_name.fen_addr,
					ep->fep_name.fen_port, 0,
					hints, &ep->fep_fi);
			if (rc == FI_SUCCESS)
				break;
		}
		M0_ASSERT(i < ARRAY_SIZE(providers));

		fi_freeinfo(hints);

		rc = fi_fabric(ep->fep_fi->fabric_attr, &ep->fep_fabric, NULL);
		if (rc != FI_SUCCESS) {
			libfab_ep_param_free(ep, tm);
			return M0_RC(rc);
		}
	}
	rc = libfab_waitset_init(tm, ep);
	if (rc != FI_SUCCESS) {
		libfab_ep_param_free(ep, tm);
		return M0_RC(rc);
	}
	
	rc = fi_domain(ep->fep_fabric, ep->fep_fi, &ep->fep_domain, NULL);
	if (rc != FI_SUCCESS) {
		libfab_ep_param_free(ep, tm);
		return M0_RC(rc);
	}

	rc = libfab_pollset_init(tm, ep);
	if (rc != FI_SUCCESS) {
		libfab_ep_param_free(ep, tm);
		return M0_RC(rc);
	}
	
	rc = fi_endpoint(ep->fep_domain, ep->fep_fi, &ep->fep_ep, NULL);
	if (rc != FI_SUCCESS) {
		libfab_ep_param_free(ep, tm);
		return M0_RC(rc);
	}

	rc = libfab_ep_res_init(ep, tm);
	if (rc != FI_SUCCESS) {
		libfab_ep_param_free(ep, tm);
		return M0_RC(rc);
	}

	if (fi != NULL) {
		/* Accept incoming request */
		rc = fi_accept(ep->fep_ep, NULL, 0);
		if (rc != FI_SUCCESS) {
			libfab_ep_param_free(ep, tm);
			return M0_RC(rc);
		}
	} else {
		/* Initiate outgoing connection request */
		rc = fi_connect(ep->fep_ep, fi->dest_addr, NULL, 0);
		if (rc != FI_SUCCESS) {
			libfab_ep_param_free(ep, tm);
			return M0_RC(rc);
		}
	}

	return M0_RC(rc);
}

/**
 * Used to create a passive endpoint which will listen for incoming connection
 * requests
 */
static int libfab_passive_ep_create(struct m0_fab__ep *ep, 
				    struct m0_fab__tm *tm)
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
		rc = fi_getinfo(LIBFAB_VERSION, NULL, NULL, 0, hints,
				&ep->fep_fi);
		if (rc == FI_SUCCESS)
			break;
	}
	M0_ASSERT(i < ARRAY_SIZE(providers));

	fi_freeinfo(hints);
	
	rc = fi_fabric(ep->fep_fi->fabric_attr, &ep->fep_fabric, NULL);
	if (rc != FI_SUCCESS) {
		libfab_ep_param_free(ep, tm);
		return M0_RC(rc);
	}

	rc = libfab_waitset_init(tm, ep);
	if (rc != FI_SUCCESS) {
		libfab_ep_param_free(ep, tm);
		return M0_RC(rc);
	}

	rc = fi_passive_ep(ep->fep_fabric, ep->fep_fi, &ep->fep_pep, NULL);
	if (rc != FI_SUCCESS) {
		libfab_ep_param_free(ep, tm);
		return M0_RC(rc);
	}

	rc = libfab_pep_res_init(ep, tm);
	if (rc != FI_SUCCESS) {
		libfab_ep_param_free(ep, tm);
		return M0_RC(rc);
	}

	rc = fi_listen(ep->fep_pep);
	if (rc != FI_SUCCESS) {
		libfab_ep_param_free(ep, tm);
		return M0_RC(rc);
	}

	return M0_RC(rc);
}

/**
 * Used to free the resources attached to an active ep
 */
static int libfab_ep_res_free(struct m0_fab__ep_res *ep_res,
			      struct m0_fab__tm *tm)
{
	int rc = 0;
	
	M0_ENTRY();

	if (ep_res == NULL)
		return M0_RC(0);

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
		M0_PRE(tm->ftm_pollset != NULL);
		rc = fi_poll_del(tm->ftm_pollset, &ep_res->fer_tx_cq->fid, 0);
		if (rc != FI_SUCCESS)
			M0_LOG(M0_ERROR, "fer_tx_cq fi_poll_del ret=%d fid=%d",
			       rc, (int)(ep_res->fer_tx_cq)->fid.fclass);
		
		rc = fi_close(&(ep_res->fer_tx_cq)->fid);
		if (rc != FI_SUCCESS)
			M0_LOG(M0_ERROR, "fer_tx_cq fi_close ret=%d fid=%d",
			       rc, (int)(ep_res->fer_tx_cq)->fid.fclass);
	}
	
	if (ep_res->fer_rx_cq != NULL){
		M0_PRE(tm->ftm_pollset != NULL);
		rc = fi_poll_del(tm->ftm_pollset, &ep_res->fer_rx_cq->fid, 0);
		if (rc != FI_SUCCESS)
			M0_LOG(M0_ERROR, "fer_rx_cq fi_poll_del ret=%d fid=%d",
			       rc, (int)(ep_res->fer_rx_cq)->fid.fclass);
		
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
static int libfab_ep_param_free(struct m0_fab__ep *ep, struct m0_fab__tm *tm)
{
	int rc = 0;

	M0_ENTRY();

	if (ep == NULL)
		return M0_RC(0);
	
	rc = libfab_ep_res_free(&ep->fep_ep_res, tm);
	
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

	if (tm == NULL)
		return M0_RC(0);

	if (tm->ftm_pep != NULL) {
		rc = libfab_ep_param_free(tm->ftm_pep, tm);
		if (rc != FI_SUCCESS)
			M0_LOG(M0_ERROR, "ftm_pep fi_close ret=%d fid=%d",
			       rc, (int)(tm->ftm_pep->fep_pep)->fid.fclass);
	}

	m0_tl_for(m0_nep, &tm->ftm_net_ma->ntm_end_points, net) {
		xep = libfab_ep_net(net);
		rc = libfab_ep_param_free(xep, tm);
	} m0_tl_endfor;
	
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

	if (tm->ftm_poller.t_func != NULL) {
		m0_thread_join(&tm->ftm_poller);
		m0_thread_fini(&tm->ftm_poller);
	}

	return M0_RC(rc);
}

/**
 * Used to init the waitset for the transfer machine 
 */
static int libfab_waitset_init(struct m0_fab__tm *tm, struct m0_fab__ep *ep)
{
	struct fi_wait_attr wait_attr;
	int                 rc = 0;

	M0_ENTRY();
	if (tm->ftm_waitset != NULL)
		return M0_RC(rc);

	memset(&wait_attr, 0, sizeof(wait_attr));
	wait_attr.wait_obj = FI_WAIT_UNSPEC;
	rc = fi_wait_open(ep->fep_fabric, &wait_attr, &tm->ftm_waitset);

	return M0_RC(rc);
}

/**
 * Used to init the waitset for the transfer machine 
 */
static int libfab_pollset_init(struct m0_fab__tm *tm, struct m0_fab__ep *ep)
{
	struct fi_poll_attr poll_attr;
	int                 rc = 0;

	M0_ENTRY();
	if (tm->ftm_pollset != NULL)
		return M0_RC(rc);

	memset(&poll_attr, 0, sizeof(poll_attr));
	rc = fi_poll_open(ep->fep_domain, &poll_attr, &tm->ftm_pollset);

	return M0_RC(rc);
}

static struct m0_fab__tm *libfab_buf_tm(struct m0_fab__buf *buf)
{
	return buf->fb_nb->nb_tm->ntm_xprt_private;
}

static void libfab_buf_fini(struct m0_fab__buf *buf)
{
	fab_buf_tlink_fini(buf);
	buf->fb_length = 0;
}

static bool libfab_dom_invariant(const struct m0_net_domain *dom)
{
	const struct m0_fab__ep *ep = dom->nd_xprt_private;
	return _0C(ep != NULL) &&
		_0C(dom->nd_xprt == &m0_net_libfab_xprt);
}

static bool libfab_tm_invariant(const struct m0_fab__tm *fab_tm)
{
	return fab_tm != NULL &&
	       fab_tm->ftm_net_ma->ntm_xprt_private == fab_tm &&
	       libfab_dom_invariant(fab_tm->ftm_net_ma->ntm_dom);
}

static bool libfab_buf_invariant(const struct m0_fab__buf *buf)
{
	const struct m0_net_buffer *nb = buf->fb_nb;

	return  (nb->nb_flags == M0_NET_BUF_REGISTERED &&
		 nb->nb_tm == NULL) ^ /* or (exclusively) ... */
		/* it is queued to a machine. */
		(_0C(nb->nb_flags & (M0_NET_BUF_REGISTERED|M0_NET_BUF_QUEUED))&&
		 _0C(nb->nb_tm != NULL) &&
		 _0C(m0_net__buffer_invariant(nb)));
}

/** Invokes completion call-back (releasing tm lock). */
static void libfab_buf_complete(struct m0_fab__buf *buf, int32_t status)
{
	struct m0_fab__tm *ma  = libfab_buf_tm(buf);
	struct m0_net_buffer *nb = buf->fb_nb;
	struct m0_net_buffer_event ev = {
		.nbe_buffer = nb,
		.nbe_status = status,
		.nbe_time   = m0_time_now()
	};

	if (M0_IN(nb->nb_qtype, (M0_NET_QT_MSG_RECV,
				 M0_NET_QT_PASSIVE_BULK_RECV,
				 M0_NET_QT_ACTIVE_BULK_RECV))) {
		ev.nbe_length = buf->fb_length;
	}
#if 0
	/* TODO: check it in recv case */
	if (nb->nb_qtype == M0_NET_QT_MSG_RECV) {
		if (ev.nbe_status == 0 && buf->b_other != NULL) {
			ev.nbe_ep = &buf->b_other->e_ep;
			EP_GET(buf->b_other, find);
		}
	}
#endif
	ma->ftm_net_ma->ntm_callback_counter++;

	libfab_buf_fini(buf);
	M0_ASSERT(libfab_tm_invariant(ma));
	libfab_tm_unlock(ma);
	m0_net_buffer_event_post(&ev);
	libfab_tm_lock(ma);
	M0_ASSERT(libfab_tm_invariant(ma));
	M0_ASSERT(M0_IN(ma->ftm_net_ma->ntm_state, (M0_NET_TM_STARTED,
					      M0_NET_TM_STOPPING)));
	ma->ftm_net_ma->ntm_callback_counter--;
}


/** Completes the buffer operation. */
static void libfab_buf_done(struct m0_fab__buf *buf, int rc)
{
	struct m0_fab__tm *ma = libfab_buf_tm(buf);

	M0_PRE(libfab_tm_is_locked(ma));
	/*
	 * Multiple libfab_buf_done() calls on the same buffer are possible if
	 * the buffer is cancelled.
	 */
	if (!fab_buf_tlink_is_in(buf)) {
		/* Try to finalise. */
		if (m0_thread_self() == &ma->ftm_poller)
			libfab_buf_complete(buf, rc);
		else
			/* Otherwise, postpone finalisation to
			* libfab_tm_buf_done(). */
			fab_buf_tlist_add_tail(&ma->ftm_done, buf);
	}
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

	libfab_tm_lock(ma);
	ma->ftm_shutdown = true;	
	rc = libfab_tm_param_free(ma);
	if (rc != FI_SUCCESS)
		M0_LOG(M0_ERROR, "libfab_tm_param_free ret=%d",	rc);

	tm->ntm_xprt_private = NULL;
	libfab_tm_unlock(ma);

	m0_free(ma);

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

	M0_ASSERT(tm->ntm_xprt_private == NULL);
	M0_ALLOC_PTR(ma);
	if (ma != NULL) {
		ma->ftm_shutdown = false;
		tm->ntm_xprt_private = ma;
		ma->ftm_net_ma = tm;
		fab_buf_tlist_init(&ma->ftm_done);
		M0_ALLOC_PTR(ma->ftm_pep);
		if (ma->ftm_pep != NULL)
			rc = libfab_passive_ep_create(ma->ftm_pep, ma);
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
	struct m0_fab__tm *tm = net->ntm_xprt_private;

	libfab_tm_unlock(tm);
	libfab_tm_event_post(tm, M0_NET_TM_STARTED);
	libfab_tm_lock(tm);

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
	struct m0_fab__tm *tm = net->ntm_xprt_private;

	M0_PRE(net->ntm_state == M0_NET_TM_STOPPING);

	if (cancel)
		m0_net__tm_cancel(net);
	
	libfab_tm_unlock(tm);
	libfab_tm_event_post(tm, M0_NET_TM_STOPPED);
	libfab_tm_lock(tm);

	return M0_RC(0);
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
	struct m0_fab__buf *fb = nb->nb_xprt_private;
	int                 ret;
	M0_PRE(nb->nb_flags == M0_NET_BUF_REGISTERED &&
	       libfab_buf_invariant(fb));

	ret = fi_close(&fb->fb_mr->fid);
	M0_ASSERT(ret == FI_SUCCESS);

	libfab_buf_fini(fb);
	m0_free(fb);
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
	struct m0_fab__buf *fb;
	int                 ret;

	M0_PRE(nb->nb_xprt_private == NULL);

	M0_ALLOC_PTR(fb);
	if (fb == NULL)
		return M0_ERR(-ENOMEM);

	fab_buf_tlink_init(fb);
	nb->nb_xprt_private = fb;
	fb->fb_nb = nb;
	/* Registers buff that can be used for send/recv and local/remote RMA*/
	ret = fi_mr_reg(dp, nb->nb_buffer.ov_buf[0], nb->nb_length,
			FAB_MR_ACCESS, FAB_MR_OFFSET, FAB_MR_KEY,
			FAB_MR_FLAG, &fb->fb_mr, NULL);
	if (ret != FI_SUCCESS)
	{
		nb->nb_xprt_private = NULL;
		m0_free(fb);
		return M0_ERR(ret);
	}
	
	ret = fi_mr_enable(fb->fb_mr);
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
	struct m0_fab__buf *buf = nb->nb_xprt_private;
	struct m0_fab__ep  *fep = nb->nb_dom->nd_xprt_private;
	int                 ret;

	nb->nb_flags |= M0_NET_BUF_CANCELLED;
	ret = fi_cancel(&fep->fep_ep->fid, buf);
	if (ret != FI_SUCCESS)
		libfab_buf_done(buf, -ECANCELED);

}

static int libfab_bev_deliver_sync(struct m0_net_transfer_mc *ma)
{
	return 0;
}

static void libfab_bev_deliver_all(struct m0_net_transfer_mc *ma)
{

}

static bool libfab_bev_pending(struct m0_net_transfer_mc *ma)
{
	return false;
}

static void libfab_bev_notify(struct m0_net_transfer_mc *ma,
			      struct m0_chan *chan)
{

}

/**
 * Maximal number of bytes in a buffer.
 *
 * Used as m0_net_xprt_ops::xo_get_max_buffer_size()
 *
 * @see m0_net_domain_get_max_buffer_size()
 */
static m0_bcount_t libfab_get_max_buf_size(const struct m0_net_domain *dom)
{
	return M0_BCOUNT_MAX / 2;
}

/**
 * Maximal number of bytes in a buffer segment.
 *
 * Used as m0_net_xprt_ops::xo_get_max_buffer_segment_size()
 *
 * @see m0_net_domain_get_max_buffer_segment_size()
 */
static m0_bcount_t libfab_get_max_buf_seg_size(const struct m0_net_domain *dom)
{
	return M0_BCOUNT_MAX / 2;
}

/**
 * Maximal number of segments in a buffer
 *
 * Used as m0_net_xprt_ops::xo_get_max_buffer_segments()
 *
 * @see m0_net_domain_get_max_buffer_segments()
 */
static int32_t libfab_get_max_buf_segments(const struct m0_net_domain *dom)
{
	return INT32_MAX / 2;
}

/**
 * Maximal number of bytes in a buffer descriptor.
 *
 * Used as m0_net_xprt_ops::xo_get_max_buffer_desc_size()
 *
 * @see m0_net_domain_get_max_buffer_desc_size()
 */
static m0_bcount_t libfab_get_max_buf_desc_size(const struct m0_net_domain *dom)
{
	return sizeof(uint64_t);
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
	.xo_get_max_buffer_size         = &libfab_get_max_buf_size,
	.xo_get_max_buffer_segment_size = &libfab_get_max_buf_seg_size,
	.xo_get_max_buffer_segments     = &libfab_get_max_buf_segments,
	.xo_get_max_buffer_desc_size    = &libfab_get_max_buf_desc_size
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
