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


#include "lib/assert.h"
#include "lib/errno.h"
#include "lib/finject.h"
#include "net/net_internal.h"
#include "motr/magic.h"

#define M0_TRACE_SUBSYSTEM M0_TRACE_SUBSYS_NET
#include "lib/trace.h"

/**
 * @addtogroup net
 * @{
 */

M0_TL_DESCR_DEFINE(m0_nep, "net end points", M0_INTERNAL,
		   struct m0_net_end_point, nep_tm_linkage, nep_magix,
		   M0_NET_NEP_MAGIC, M0_NET_NEP_HEAD_MAGIC);
M0_TL_DEFINE(m0_nep, M0_INTERNAL, struct m0_net_end_point);

M0_INTERNAL bool m0_net__ep_invariant(struct m0_net_end_point *ep,
				      struct m0_net_transfer_mc *tm,
				      bool under_tm_mutex)
{
	return
		_0C(ep != NULL) &&
		_0C(m0_atomic64_get(&ep->nep_ref.ref_cnt) > 0) &&
		_0C(ep->nep_ref.release != NULL) &&
		_0C(ep->nep_tm == tm) &&
		_0C(ep->nep_addr != NULL) &&
		_0C(ergo(under_tm_mutex,
			 m0_nep_tlist_contains(&tm->ntm_end_points, ep)));
}

M0_INTERNAL int m0_net_end_point_create(struct m0_net_end_point **epp,
					struct m0_net_transfer_mc *tm,
					const char *addr)
{
	int rc;
	struct m0_net_domain *dom;

	M0_PRE(tm != NULL && tm->ntm_state == M0_NET_TM_STARTED);
	M0_PRE(epp != NULL);

	if (M0_FI_ENABLED("fake_error"))
		return M0_ERR(-EINVAL);

	dom = tm->ntm_dom;
	M0_PRE(dom->nd_xprt != NULL);

	m0_mutex_lock(&tm->ntm_mutex);

	*epp = NULL;

	rc = dom->nd_xprt->nx_ops->xo_end_point_create(epp, tm, addr);

	/*
	 * Either we failed or we got back a properly initialized end point
	 * with reference count of at least 1.
	 */
	M0_POST(ergo(rc == 0, m0_net__ep_invariant(*epp, tm, true)));
	m0_mutex_unlock(&tm->ntm_mutex);
	return M0_RC(rc);
}
M0_EXPORTED(m0_net_end_point_create);

M0_INTERNAL void m0_net_end_point_get(struct m0_net_end_point *ep)
{
	struct m0_ref *ref = &ep->nep_ref;
	M0_PRE(ep != NULL);
	M0_PRE(m0_atomic64_get(&ref->ref_cnt) >= 1);
	m0_ref_get(ref);
	return;
}
M0_EXPORTED(m0_net_end_point_get);

void m0_net_end_point_put(struct m0_net_end_point *ep)
{
	struct m0_ref *ref = &ep->nep_ref;
	struct m0_net_transfer_mc *tm;
	M0_PRE(ep != NULL);
	M0_PRE(m0_atomic64_get(&ref->ref_cnt) >= 1);
	tm = ep->nep_tm;
	M0_PRE(tm != NULL);
	/* hold the transfer machine lock to synchronize release(), if called */
	m0_mutex_lock(&tm->ntm_mutex);
	m0_ref_put(ref);
	m0_mutex_unlock(&tm->ntm_mutex);
	return;
}
M0_EXPORTED(m0_net_end_point_put);

M0_INTERNAL int m0_net_end_point_lookup(struct m0_net_transfer_mc *tm, const char *addr,
			   struct m0_net_end_point **epp)
{
	struct m0_net_domain *dom;
	struct m0_net_end_point *net;
	struct m0_net_ip_addr ip_addr;
	struct m0_net_ip_addr net_addr;
	bool                  found = false;

	M0_PRE(tm != NULL && tm->ntm_state == M0_NET_TM_STARTED);
	M0_PRE(epp != NULL);
	dom = tm->ntm_dom;
	M0_PRE(dom->nd_xprt != NULL);

	m0_mutex_lock(&tm->ntm_mutex);
	if (m0_net_ip_parse(addr, &ip_addr) != 0)
		return -EINVAL;

	m0_tl_for(m0_nep, &tm->ntm_end_points, net) {
		if (m0_net_ip_parse(net->nep_addr, &net_addr) != 0)
			return -EINVAL;
		if (m0_net_ip_addr_eq(&ip_addr, &net_addr, true)) {
			found = true;
			break;
		}
	} m0_tl_endfor;

	*epp = found ? net : NULL;
	m0_mutex_unlock(&tm->ntm_mutex);
	return 0;
}

#undef M0_TRACE_SUBSYSTEM

/** @} end of net group */

/*
 *  Local variables:
 *  c-indentation-style: "K&R"
 *  c-basic-offset: 8
 *  tab-width: 8
 *  fill-column: 79
 *  scroll-step: 1
 *  End:
 */
