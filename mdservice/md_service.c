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
   @addtogroup mdservice
   @{
 */

#define M0_TRACE_SUBSYSTEM M0_TRACE_SUBSYS_MDS
#include "lib/trace.h"

#include "lib/errno.h"
#include "lib/memory.h"
#include "lib/locality.h"
#include "motr/magic.h"
#include "motr/setup.h"
#include "reqh/reqh_service.h"
#include "reqh/reqh.h"
#include "layout/layout.h"
#include "layout/linear_enum.h"
#include "layout/pdclust.h"
#include "conf/confc.h"
#include "conf/helpers.h"
#include "mdservice/md_fops.h"
#include "mdservice/md_service.h"
#include "mdservice/fsync_fops.h"
#include "module/instance.h"	/* m0_get */


static int mds_allocate(struct m0_reqh_service **service,
			const struct m0_reqh_service_type *stype);
static void mds_fini(struct m0_reqh_service *service);

static int mds_start(struct m0_reqh_service *service);
static void mds_stop(struct m0_reqh_service *service);

/**
 * MD Service type operations.
 */
static const struct m0_reqh_service_type_ops mds_type_ops = {
        .rsto_service_allocate = mds_allocate
};

/**
 * MD Service operations.
 */
static const struct m0_reqh_service_ops mds_ops = {
        .rso_start       = mds_start,
	.rso_start_async = m0_reqh_service_async_start_simple,
        .rso_stop        = mds_stop,
        .rso_fini        = mds_fini
};

struct m0_reqh_service_type m0_mds_type = {
	.rst_name     = "M0_CST_MDS",
	.rst_ops      = &mds_type_ops,
	.rst_level    = M0_MD_SVC_LEVEL,
	.rst_typecode = M0_CST_MDS,
};

M0_INTERNAL int m0_mds_register(void)
{
	int rc;

	m0_get()->i_mds_cdom_key = m0_reqh_lockers_allot();

	/* XXX: find better place */
	m0_get()->i_actrec_dom_key = m0_reqh_lockers_allot();
	m0_get()->i_dtm0_log_key = m0_reqh_lockers_allot();

	m0_reqh_service_type_register(&m0_mds_type);

	rc = m0_mdservice_fsync_fop_init(&m0_mds_type);
	if (rc != 0) {
		return M0_ERR_INFO(rc,
				   "Unable to initialize mdservice fsync fop");
	}
	rc = m0_mdservice_fop_init();
	if (rc != 0) {
		m0_mdservice_fsync_fop_fini();
		return M0_ERR_INFO(rc, "Unable to initialize mdservice fop");
	}
	return M0_RC(rc);
}

M0_INTERNAL void m0_mds_unregister(void)
{
	m0_reqh_service_type_unregister(&m0_mds_type);
	m0_mdservice_fop_fini();
	m0_mdservice_fsync_fop_fini();

	m0_reqh_lockers_free(m0_get()->i_mds_cdom_key);

	/* XXX: find better place */
	m0_reqh_lockers_free(m0_get()->i_actrec_dom_key);
	m0_reqh_lockers_free(m0_get()->i_dtm0_log_key);
}

/**
 * Allocates and initiates MD Service instance.
 * This operation allocates & initiates service instance with its operation
 * vector.
 */
static int mds_allocate(struct m0_reqh_service **service,
			const struct m0_reqh_service_type *stype)
{
	struct m0_reqh_md_service *mds;

	M0_PRE(service != NULL && stype != NULL);

	M0_ALLOC_PTR(mds);
	if (mds == NULL)
		return M0_ERR(-ENOMEM);

	mds->rmds_magic = M0_MDS_REQH_SVC_MAGIC;

	*service = &mds->rmds_gen;
	(*service)->rs_ops = &mds_ops;
	return 0;
}

/**
 * Finalise MD Service instance.
 * This operation finalises service instance and de-allocate it.
 *
 * @param service pointer to service instance.
 *
 * @pre service != NULL
 */
static void mds_fini(struct m0_reqh_service *service)
{
        struct m0_reqh_md_service *serv_obj;

        M0_PRE(service != NULL);

        serv_obj = container_of(service, struct m0_reqh_md_service, rmds_gen);
        m0_free(serv_obj);
}

/**
 * Start MD Service.
 * @param service pointer to service instance.
 *
 * @pre service != NULL
 */
static int mds_start(struct m0_reqh_service *service)
{
	struct m0_motr            *cctx;
	/*struct m0_reqh            *reqh;*/
	struct m0_reqh_md_service *mds;
	/*struct m0_sm_group        *grp;*/

	M0_PRE(service != NULL);

	mds = container_of(service, struct m0_reqh_md_service, rmds_gen);

	/* in UT we don't init layouts */
	if (mds->rmds_gen.rs_reqh_ctx == NULL)
		return 0;

	cctx = mds->rmds_gen.rs_reqh_ctx->rc_motr;
	if (cctx->cc_ha_addr == NULL)
		return 0;
	/*reqh = mds->rmds_gen.rs_reqh;*/
	/* grp  = &reqh->rh_sm_grp; - no, reqh is not started yet */
	/** @todo XXX change this when reqh will be started before services,
	 *        see MOTR-317
	 */
	/*grp  = m0_locality0_get()->lo_grp;*/
	return 0;
}

/**
 * Stops MD Service.
 * @param service pointer to service instance.
 *
 * @pre service != NULL
 */
static void mds_stop(struct m0_reqh_service *service)
{
	m0_reqh_lockers_clear(service->rs_reqh, m0_get()->i_mds_cdom_key);

	/* XXX: find better place */
	m0_reqh_lockers_clear(service->rs_reqh, m0_get()->i_actrec_dom_key);
	m0_reqh_lockers_clear(service->rs_reqh, m0_get()->i_dtm0_log_key);
}

#undef M0_TRACE_SUBSYSTEM

/** @} endgroup mdservice */

#undef M0_TRACE_SUBSYSTEM
/*
 *  Local variables:
 *  c-indentation-style: "K&R"
 *  c-basic-offset: 8
 *  tab-width: 8
 *  fill-column: 80
 *  scroll-step: 1
 *  End:
 */
