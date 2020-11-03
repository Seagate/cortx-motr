/* -*- C -*- */
/*
 * Copyright (c) 2019-2020 Seagate Technology LLC and/or its Affiliates
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
 * xo_buf_del                     =  fi_cancel // Why del buffer? What is the action in sock/lnet
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

#define M0_TRACE_SUBSYSTEM M0_TRACE_SUBSYS_NET



/** Network transfer machine */
struct trasfer_ma {
	/** Generic transfer machine with buffer queues, etc. */
	struct m0_net_transfer_mc *t_ma;

	// TODO: Is poller thread required ?
}


/** Returns (find or create) the end-point with the given name. */
static int ep_find(struct ma *ma, const char *name, struct ep **out)
{
	struct addr addr = {};
	/*
	* TODO: Check if addr_resolve() and ep_create() needs to be implemented or 
	* is there support in libfabric already
	*/
	return addr_resolve(&addr, name) ?: ep_create(ma, &addr, name, out);
}

/** Used as m0_net_xprt_ops::xo_dom_init(). */
static int dom_init(struct m0_net_xprt *xprt, struct m0_net_domain *dom)
{
	M0_ENTRY();
	/*
	*  TODO:
	* fi_fabric()
	* fi_eq_open()
	* fi_domain()
	* fi_open_ops() for domain 
	* fi_set_ops()  for domain
	*/
	return M0_RC(0);
}

/** Used as m0_net_xprt_ops::xo_dom_fini(). */
static void dom_fini(struct m0_net_domain *dom)
{
	M0_ENTRY();
	/*
	* TODO
	* fi_close()
	* Check if other are required refer pp_free_res()
	*/
	M0_LEAVE();
}

/**
 * Initialises transport-specific part of the transfer machine.
 *
 * Used as m0_net_xprt_ops::xo_tm_init().
 */
static int ma_init(struct m0_net_transfer_mc *net)
{
   /*
   approach 1:   
	 Looks like this would be empty function, 
   * as completion queue and counters are initialised during endpoint creation 
   */
   
   /* 
    approach 2 - Recommended
     completion queue and counters can be initialised  here based on list of endpoints
	 
   */
	return M0_RC(result);
}


/**
 * Starts initialised ma.
 *
 * Initialises everything that ma_init() didn't. Note that ma is in
 * M0_NET_TM_STARTING state after this returns. Switch to M0_NET_TM_STARTED
 * happens when the poller thread posts special event.
 *
 * Used as m0_net_xprt_ops::xo_tm_start().
 */
static int ma_start(struct m0_net_transfer_mc *net, const char *name)
{
	/*
	*  check if worker thread needs to be added to check completion queue, refer nlx_xo_tm_start() LNet 
	*/
}

/**
 * Stops a ma that has been started or is being started.
 *
 *
 * Used as m0_net_xprt_ops::xo_tm_stop().
 */
static int ma_stop(struct m0_net_transfer_mc *net, bool cancel)
{
	/* TODO: fi_cancel () */

	return 0;
}

/**
 * Used as m0_net_xprt_ops::xo_ma_fini().
 */
static void ma_fini(struct m0_net_transfer_mc *net)
{
	/* TODO: Reverses the actions of ma_init() */

}


/**
 * Returns an end-point with the given name.
 *
 * Used as m0_net_xprt_ops::xo_end_point_create().
 *
 * @see m0_net_end_point_create().
 */
static int end_point_create(struct m0_net_end_point **epp,
			    struct m0_net_transfer_mc *net,
			    const char *name)
{
	struct ep *ep;
	struct ma *ma = net->ntm_xprt_private;
	int        result;

	M0_PRE(ma_is_locked(ma) && ma_invariant(ma));
	result = ep_find(ma, name, &ep);
	*epp = result == 0 ? &ep->e_ep : NULL;
	return M0_RC(result);
}

/**
 * Initialises a network buffer.
 *
 * Used as m0_net_xprt_ops::xo_buf_register().
 *
 * @see m0_net_buffer_register().
 */
static int buf_register(struct m0_net_buffer *nb)
{
	/*
	* TODO
	* fi_mr_reg()
	*/
	
}	

/**
 * Finalises a network buffer.
 *
 * Used as m0_net_xprt_ops::xo_buf_deregister().
 *
 * @see m0_net_buffer_deregister().
 */
static void buf_deregister(struct m0_net_buffer *nb)
{
	/*fi_close() */
}

/**
 * Adds a network buffer to a ma queue.
 *
 * Used as m0_net_xprt_ops::xo_buf_add().
 *
 * @see m0_net_buffer_add().
 */
static int buf_add(struct m0_net_buffer *nb)
{
	/* Needs to be explored, refer nlx_xo_buf_add() from LNet */
}



/**
 * Cancels a buffer operation..
 *
 * Used as m0_net_xprt_ops::xo_buf_del().
 *
 * @see m0_net_buffer_del().
 */
static void buf_del(struct m0_net_buffer *nb)
{
	struct buf *buf = nb->nb_xprt_private;
	struct ma  *ma  = buf_ma(buf);

	M0_PRE(ma_is_locked(ma) && ma_invariant(ma) && buf_invariant(buf));
	nb->nb_flags |= M0_NET_BUF_CANCELLED;
	buf_done(buf, -ECANCELED);
}

static int ma_confine(struct m0_net_transfer_mc *ma,
		      const struct m0_bitmap *processors)
{
	return -ENOSYS;
}

#undef M0_TRACE_SUBSYSTEM

/** @} end of netsock group */

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
