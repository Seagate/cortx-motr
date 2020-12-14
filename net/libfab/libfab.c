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
#include "lib/trace.h"

#include "net/net.h"
#include "lib/bitmap.h"
#include "lib/chan.h"
#include "lib/memory.h"
#include "libfab_internal.h"
#include <errno.h>

#define LIBFAB_VERSION FI_VERSION(FI_MAJOR_VERSION,FI_MINOR_VERSION)

static char *providers[] = { "verbs", "tcp", "sockets" };

static int libfab_dom_find(const char *name, struct fi_info *fi_list);
static int libfab_ep_res_init(struct m0_fab__dom_param *fab,
			      struct m0_fab__ep_param *ep);
static struct m0_fab__ep_param *libfab_ep_net(struct m0_net_end_point *net);
static int libfab_ep_find(const char *name, struct m0_net_transfer_mc *tm, 
			  struct m0_net_end_point **epp);
static void libfab_ep_param_free(struct m0_fab__ep_param *ep);
static void libfab_ep_res_free(struct m0_fab__ep_res *ep_res);

/* Find and select the domain from the list */
static int libfab_dom_find(const char *name, struct fi_info *fi_list)
{
	struct fi_info *fab = fi_list;
	int             rc = 1;

	M0_ENTRY();

	if (name == NULL)
		return M0_RC(0);

	while ((fab != NULL) && (rc != 0)) {
		rc = strcmp(name, fab->domain_attr->name);
		if (rc == 0)
			fi_list = fab;
		fab = fab->next;
	}
	
	return M0_RC(rc);

}

/** Used as m0_net_xprt_ops::xo_dom_init(). */
static int libfab_dom_init(struct m0_net_xprt *xprt, struct m0_net_domain *dom)
{
	struct m0_fab__dom_param *fab;
	int                       rc;
	int                       i;
	char                     *node = 0;
	char                     *port = 0;

	M0_ENTRY();
	
	M0_ALLOC_PTR(fab);
	if (fab == NULL)
		return M0_ERR(-ENOMEM);

	fab->fdp_hints = fi_allocinfo();
	if (fab->fdp_hints != NULL) {
		/*
		* TODO: Added for future use
		* fab->hints->ep_attr->type = FI_EP_RDM;
		* fab->hints->caps = FI_MSG;
		*/
		
		for (i = 0; i < ARRAY_SIZE(providers); i++) {
			fab->fdp_hints->fabric_attr->prov_name = providers[i];
			rc = fi_getinfo(LIBFAB_VERSION, node, port, 0, 
					fab->fdp_hints, &fab->fdp_fi);
			if ((rc == FI_SUCCESS) && 
			    libfab_dom_find(dom->nd_name, fab->fdp_fi) == 0) {
				break;
			}
		}

		M0_ASSERT(i < ARRAY_SIZE(providers));

		rc = fi_fabric(fab->fdp_fi->fabric_attr, &fab->fdp_fabric, 
			       NULL);
		if (rc == FI_SUCCESS) {
			rc = fi_domain(fab->fdp_fabric, fab->fdp_fi,
				       &fab->fdp_domain, NULL);
			if (rc == FI_SUCCESS)
				dom->nd_xprt_private = fab;
		}
		fi_freeinfo(fab->fdp_hints);
	} else 
		rc = M0_ERR(-ENOMEM); 

	return M0_RC(rc);
}

static void libfab_fab_param_free(struct m0_fab__dom_param *fab_dom)
{
	int    ret;

	M0_ENTRY();

	if (fab_dom->fdp_domain != NULL) {
		ret = fi_close(&(fab_dom->fdp_domain)->fid);
		if (ret != FI_SUCCESS)
			M0_LOG(M0_ERROR, "fdp_domain fi_close ret=%d fid=%d",
			       ret, (int)(fab_dom->fdp_domain)->fid.fclass);
	}

	if (fab_dom->fdp_fabric != NULL) {
		ret = fi_close(&(fab_dom->fdp_fabric)->fid);
		if (ret != FI_SUCCESS)
			M0_LOG(M0_ERROR, "fdp_fabric fi_close ret=%d fid=%d",
			       ret, (int)(fab_dom->fdp_fabric)->fid.fclass);
		fab_dom->fdp_fabric = NULL;
	}

	if (fab_dom->fdp_fi != NULL) {
		fi_freeinfo(fab_dom->fdp_fi);
		fab_dom->fdp_fi = NULL;
	}

	if (fab_dom->fdp_hints != NULL) {
		fi_freeinfo(fab_dom->fdp_hints);
		fab_dom->fdp_hints = NULL;
	}

	M0_LEAVE();

}

/** Used as m0_net_xprt_ops::xo_dom_fini(). */
static void libfab_dom_fini(struct m0_net_domain *dom)
{
	struct m0_fab__dom_param *fab_dom = dom->nd_xprt_private;

	M0_ENTRY();

	libfab_fab_param_free(fab_dom);
	dom->nd_xprt_private = NULL;

	M0_LEAVE();
}

/**
 * Initialises transport-specific part of the transfer machine.
 *
 * Used as m0_net_xprt_ops::xo_tm_init().
 */
static int libfab_ma_init(struct m0_net_transfer_mc *net)
{
	int result = 0;
   /* 
      approach 2 - Recommended
      Poller thread should be started here
      completion queue and counters can be initialised
      here based on list of endpoints
   */
	return M0_RC(result);
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
	return 0;
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
 * Used as m0_net_xprt_ops::xo_ma_fini().
 */
static void libfab_ma_fini(struct m0_net_transfer_mc *tm)
{
	/* TODO: 
	 * Reverse the actions of libfab_ma_init() 
	 * */

	struct m0_net_end_point *net;
	struct m0_fab__ep_param *ep;

	M0_ENTRY();

	m0_tl_for(m0_nep, &tm->ntm_end_points, net) {
		ep = libfab_ep_net(net);
		libfab_ep_res_free(ep->fep_ep_res);
		libfab_ep_param_free(ep);
		/* TODO: Remove entry from tm->ntm_end_points list */
	} m0_tl_endfor;

	M0_LEAVE();
}

/** Converts generic end-point to its libfabric structure. */
static struct m0_fab__ep_param *libfab_ep_net(struct m0_net_end_point *net)
{
	return container_of(net, struct m0_fab__ep_param, fep_nep);
}

static int libfab_ep_find(const char *name, struct m0_net_transfer_mc *tm, 
			  struct m0_net_end_point **epp)
{
	struct m0_net_end_point *net;
	struct m0_fab__ep_param *xep;

	M0_ENTRY();

	m0_tl_for(m0_nep, &tm->ntm_end_points, net) {
		xep = libfab_ep_net(net);
		if (strcmp(name, xep->fep_name.fen_addr) == 0) {
			*epp = &xep->fep_nep;
			return M0_RC(0);
		}
	} m0_tl_endfor;

	return M0_RC(1);
}

/**
 * Init resources and bind it to the endpoint.
 */
static int libfab_ep_res_init(struct m0_fab__dom_param *fab,
			      struct m0_fab__ep_param *ep)
{
	int rc = 0;
	M0_ENTRY();

	if (ep->fep_ep_res->fer_cq_attr != NULL) {
		ep->fep_ep_res->fer_cq_attr->wait_obj = FI_WAIT_NONE;
		ep->fep_ep_res->fer_cq_attr->format = FI_CQ_FORMAT_UNSPEC;
		
		ep->fep_ep_res->fer_cq_attr->size = fab->fdp_fi->tx_attr->size;
		rc = fi_cq_open(fab->fdp_domain, ep->fep_ep_res->fer_cq_attr, 
				&ep->fep_tx_cq, NULL);
		if (rc != FI_SUCCESS)
			return M0_RC(rc);
		ep->fep_ep_res->fer_cq_attr->size = fab->fdp_fi->rx_attr->size;
		rc = fi_cq_open(fab->fdp_domain, ep->fep_ep_res->fer_cq_attr, 
				&ep->fep_rx_cq, NULL);
		if (rc != FI_SUCCESS)
			return M0_RC(rc);
		
		rc = fi_ep_bind(ep->fep_ep, &ep->fep_tx_cq->fid, FI_SEND);
		if (rc != FI_SUCCESS)
			return M0_RC(rc);
		rc = fi_ep_bind(ep->fep_ep, &ep->fep_rx_cq->fid, FI_RECV);
		if (rc != FI_SUCCESS)
			return M0_RC(rc);
	}

	if (ep->fep_ep_res->fer_eq_attr != NULL) {
		ep->fep_ep_res->fer_eq_attr->wait_obj = FI_WAIT_UNSPEC;
		rc = fi_eq_open(fab->fdp_fabric, ep->fep_ep_res->fer_eq_attr, 
				&ep->fep_eq, NULL);
		if (rc != FI_SUCCESS)
			return M0_RC(rc);
		
		rc = fi_ep_bind(ep->fep_ep, &ep->fep_eq->fid, 0);
		if (rc != FI_SUCCESS)
			return M0_RC(rc);
	}

	if (ep->fep_ep_res->fer_av_attr != NULL) {
		ep->fep_ep_res->fer_av_attr->type = FI_AV_UNSPEC;
		rc = fi_av_open(fab->fdp_domain, ep->fep_ep_res->fer_av_attr, 
				&ep->fep_av, NULL);
		if (rc != FI_SUCCESS)
			return M0_RC(rc);

		rc = fi_ep_bind(ep->fep_ep, &ep->fep_av->fid, 0);
		if (rc != FI_SUCCESS)
			return M0_RC(rc);
	}

	if (ep->fep_ep_res->fer_cntr_attr != NULL) {
		ep->fep_ep_res->fer_cntr_attr->wait_obj = FI_WAIT_NONE;
		rc = fi_cntr_open(fab->fdp_domain, ep->fep_ep_res->fer_cntr_attr, 
				  &ep->fep_tx_cntr, NULL);
		if (rc != FI_SUCCESS)
			return M0_RC(rc);
	
		rc = fi_cntr_open(fab->fdp_domain, ep->fep_ep_res->fer_cntr_attr, 
				  &ep->fep_rx_cntr, NULL);
		if (rc != FI_SUCCESS)
			return M0_RC(rc);

		rc = fi_ep_bind(ep->fep_ep, &ep->fep_tx_cntr->fid, 0);
		if (rc != FI_SUCCESS)
			return M0_RC(rc);
		rc = fi_ep_bind(ep->fep_ep, &ep->fep_rx_cntr->fid, 0);
		if (rc != FI_SUCCESS)
			return M0_RC(rc);
	}

	return M0_RC(rc);
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
	struct m0_net_end_point  *net;
	struct m0_fab__dom_param *fab;
	struct m0_fab__ep_param  *ep;
	int                       rc = 0;

	M0_ENTRY();

	rc = libfab_ep_find(name, tm, epp);
	if (rc == 0)
		return M0_RC(rc);

	fab = tm->ntm_dom->nd_xprt_private;

	M0_ALLOC_PTR(ep);
	M0_ALLOC_PTR(ep->fep_ep_res);
	
	M0_ALLOC_PTR(ep->fep_ep_res->fer_cq_attr);
	if (ep->fep_ep_res->fer_cq_attr == NULL)
		return M0_ERR(-ENOMEM);
	
	M0_ALLOC_PTR(ep->fep_ep_res->fer_eq_attr);
	if (ep->fep_ep_res->fer_eq_attr == NULL)
		return M0_ERR(-ENOMEM);
	
	ep->fep_ep_res->fer_av_attr = NULL;
	ep->fep_ep_res->fer_cntr_attr = NULL;
	
	rc = fi_endpoint(fab->fdp_domain, fab->fdp_fi, &ep->fep_ep, NULL);
	if (rc == FI_SUCCESS) {
		rc = libfab_ep_res_init(fab, ep);
		if (rc == FI_SUCCESS) {
			rc = fi_enable(ep->fep_ep);
			strcpy(ep->fep_name.fen_addr, name);
			net = &ep->fep_nep;
			m0_nep_tlink_init_at_tail(net, &tm->ntm_end_points);
		}
	} else {
		libfab_ep_res_free(ep->fep_ep_res);
		libfab_ep_param_free(ep);
	}


	return M0_RC(rc);
}

/**
 * Initialises a network buffer.
 *
 * Used as m0_net_xprt_ops::xo_buf_register().
 *
 * @see m0_net_buffer_register().
 */
static int libfab_buf_register(struct m0_net_buffer *nb)
{
	int        		ret = 0;

	return M0_RC(ret);

	/*
	* TODO:
	* fi_mr_reg, fi_mr_desc, fi_mr_key, fi_mr_bind, fi_mr_enable
	*/
}	

/**
 * Finalises a network buffer.
 *
 * Used as m0_net_xprt_ops::xo_buf_deregister().
 *
 * @see m0_net_buffer_deregister().
 */
static void libfab_buf_deregister(struct m0_net_buffer *nb)
{
	/*
 	* TODO:
 	* fi_close() */
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

static int libfab_ma_confine(struct m0_net_transfer_mc *ma,
		      const struct m0_bitmap *processors)
{
	return -ENOSYS;
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

static void libfab_ep_param_free(struct m0_fab__ep_param *ep)
{
	M0_ENTRY();

	if (ep->fep_ep != NULL)
		fi_close(&ep->fep_ep->fid);

	if (ep->fep_pep != NULL)
		fi_close(&ep->fep_pep->fid);

	if (ep->fep_av != NULL)
		fi_close(&ep->fep_av->fid);

	if (ep->fep_eq != NULL)
		fi_close(&ep->fep_eq->fid);
	
	if (ep->fep_tx_cq != NULL)
		fi_close(&ep->fep_tx_cq->fid);
	
	if (ep->fep_rx_cq != NULL)
		fi_close(&ep->fep_rx_cq->fid);
	
	if (ep->fep_tx_cntr != NULL)
		fi_close(&ep->fep_tx_cntr->fid);
	
	if (ep->fep_rx_cntr != NULL)
		fi_close(&ep->fep_rx_cntr->fid);

	m0_free(ep);

	M0_LEAVE();
}

static void libfab_ep_res_free(struct m0_fab__ep_res *ep_res)
{
	M0_ENTRY();

	if (ep_res->fer_cq_attr != NULL)
		m0_free(ep_res->fer_cq_attr);

	if (ep_res->fer_eq_attr != NULL)
		m0_free(ep_res->fer_eq_attr);

	if (ep_res->fer_av_attr != NULL)
		m0_free(ep_res->fer_av_attr);

	if (ep_res->fer_cntr_attr != NULL)
		m0_free(ep_res->fer_cntr_attr);

	m0_free(ep_res);

	M0_LEAVE();
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
