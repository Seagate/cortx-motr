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


#define M0_TRACE_SUBSYSTEM M0_TRACE_SUBSYS_SNSCM
#include "lib/memory.h"
#include "lib/assert.h"
#include "lib/errno.h"
#include "lib/trace.h"
#include "lib/misc.h"
#include "lib/finject.h"

#include "reqh/reqh_service.h"
#include "sns/cm/cm.h"
#include "motr/setup.h"
#include "sns/cm/sns_cp_onwire.h"

/**
  @addtogroup SNSCMSVC

  @{
*/

M0_INTERNAL int
m0_sns_cm_svc_allocate(struct m0_reqh_service **service,
		       const struct m0_reqh_service_type *stype,
		       const struct m0_reqh_service_ops *svc_ops,
		       const struct m0_cm_ops *cm_ops)
{
	struct m0_sns_cm *sns_cm;
	struct m0_cm     *cm;
	int               rc;

	M0_ENTRY("stype: %p", stype);
	M0_PRE(service != NULL && stype != NULL);

	M0_ALLOC_PTR(sns_cm);
	if (sns_cm == NULL)
		return M0_RC(-ENOMEM);

	cm = &sns_cm->sc_base;
	*service = &cm->cm_service;
	(*service)->rs_ops = svc_ops;
	sns_cm->sc_magic = M0_SNS_CM_MAGIC;

	rc = m0_cm_init(cm, container_of(stype, struct m0_cm_type, ct_stype),
			cm_ops);
	if (rc != 0)
		m0_free(sns_cm);

	M0_LOG(M0_DEBUG, "sns_cm: %p service: %p", sns_cm, *service);
	return M0_RC(rc);
}

M0_INTERNAL int m0_sns_cm_svc_start(struct m0_reqh_service *service)
{
	struct m0_cm                *cm;
	int                          rc;
	struct cs_endpoint_and_xprt *ep;
	struct m0_reqh_context      *rctx;
	struct m0_motr              *motr;

	M0_ENTRY("service: %p", service);
	M0_PRE(service != NULL);

	cm = container_of(service, struct m0_cm, cm_service);
	rc = m0_cm_setup(cm);
	if (rc != 0)
		return M0_RC(rc);

	/* The following shows how to retrieve ioservice endpoints list.
	 * Copy machine can establish connections to all ioservices,
	 * and build a map for "cob_id" -> "sesssion of ioservice" with
	 * the same algorithm on m0t1fs client.
	 */
	rctx = service->rs_reqh_ctx;
	motr = rctx->rc_motr;
	m0_tl_for(cs_eps, &motr->cc_ios_eps, ep) {
		M0_ASSERT(cs_endpoint_and_xprt_bob_check(ep));
		M0_LOG(M0_DEBUG, "pool_width=%d, "
				 "ioservice xprt:endpoints: %s:%s",
				 motr->cc_pool_width,
				 ep->ex_xprt, ep->ex_endpoint);
	} m0_tl_endfor;

	M0_LEAVE();
	return M0_RC(rc);
}

M0_INTERNAL void m0_sns_cm_svc_stop(struct m0_reqh_service *service)
{
	struct m0_cm *cm;

	M0_ENTRY("service: %p", service);
	M0_PRE(service != NULL);

	cm = container_of(service, struct m0_cm, cm_service);
	/*
	 * Finalise the copy machine as the copy machine as the service is
	 * stopped.
	 */
	m0_cm_fini(cm);

	M0_LEAVE();
}

M0_INTERNAL void m0_sns_cm_svc_fini(struct m0_reqh_service *service)
{
	struct m0_cm     *cm;
	struct m0_sns_cm *sns_cm;

	M0_ENTRY("service: %p", service);
	M0_PRE(service != NULL);

	cm = container_of(service, struct m0_cm, cm_service);
	sns_cm = cm2sns(cm);
	m0_free(sns_cm);

	M0_LEAVE();
}

#undef M0_TRACE_SUBSYSTEM

/** @} SNSCMSVC */
/*
 *  Local variables:
 *  c-indentation-style: "K&R"
 *  c-basic-offset: 8
 *  tab-width: 8
 *  fill-column: 80
 *  scroll-step: 1
 *  End:
 */
