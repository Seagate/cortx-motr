/* -*- C -*- */
/*
 * Copyright (c) 2012-2020 Seagate Technology LLC and/or its Affiliates
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
   @addtogroup LNetXODFS
   @{
 */

/**
   End point release subroutine invoked when the reference count goes to 0.
   Unlinks the end point from the TM, and releases the memory.
   Must be called holding the TM mutex.
 */
static void nlx_ep_release(struct m0_ref *ref)
{
	struct m0_net_end_point *ep;
	struct nlx_xo_ep *xep;

	ep = container_of(ref, struct m0_net_end_point, nep_ref);
	M0_PRE(m0_mutex_is_locked(&ep->nep_tm->ntm_mutex));
	M0_PRE(nlx_ep_invariant(ep));
	xep = container_of(ep, struct nlx_xo_ep, xe_ep);

	m0_nep_tlist_del(ep);
	ep->nep_tm = NULL;
	m0_free(xep);
}

static int nlx_ep_create(struct m0_net_end_point **epp,
			 struct m0_net_transfer_mc *tm,
			 const struct nlx_core_ep_addr *cepa)
{
	struct m0_net_end_point *ep;
	struct m0_net_end_point *ep1;
	struct nlx_xo_domain *dp;
	struct nlx_xo_ep *xep;

	NLXDBG((struct nlx_xo_transfer_mc *)tm->ntm_xprt_private, 2,
	       nlx_print_core_ep_addr("nlx_ep_create", cepa));
	M0_PRE(m0_mutex_is_locked(&tm->ntm_mutex));
	M0_PRE(nlx_tm_invariant(tm));
	dp = tm->ntm_dom->nd_xprt_private;

	/* check if its already on the TM end point list */
	m0_tl_for(m0_nep, &tm->ntm_end_points, ep1) {
		M0_ASSERT(nlx_ep_invariant(ep1));
		xep = container_of(ep1, struct nlx_xo_ep, xe_ep);
		if (nlx_core_ep_eq(&xep->xe_core, cepa)) {
			m0_net_end_point_get(ep1);
			*epp = ep1;
			return 0;
		}
	} m0_tl_endfor;

	NLX_ALLOC_PTR(xep);
	if (xep == NULL)
		return M0_ERR(-ENOMEM);
	xep->xe_magic = M0_NET_LNET_XE_MAGIC;
	xep->xe_core = *cepa;
	nlx_core_ep_addr_encode(&dp->xd_core, cepa, xep->xe_addr);
	ep = &xep->xe_ep;
	m0_ref_init(&ep->nep_ref, 1, nlx_ep_release);
	ep->nep_tm = tm;
	m0_nep_tlink_init_at_tail(ep, &tm->ntm_end_points);
	ep->nep_addr = &xep->xe_addr[0];
	M0_POST(nlx_ep_invariant(ep));
	*epp = ep;
	return 0;
}

/** @} */ /* LNetXODFS */

/*
 *  Local variables:
 *  c-indentation-style: "K&R"
 *  c-basic-offset: 8
 *  tab-width: 8
 *  fill-column: 79
 *  scroll-step: 1
 *  End:
 */
