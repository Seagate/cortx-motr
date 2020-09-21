/*
 * Copyright (c) 2017-2020 Seagate Technology LLC and/or its Affiliates
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
   @addtogroup fdmi_main
   @{
 */

#define M0_TRACE_SUBSYSTEM M0_TRACE_SUBSYS_FDMI
#include "lib/trace.h"

#include "lib/errno.h"
#include "lib/memory.h"
#include "lib/locality.h"
#include "motr/magic.h"
#include "motr/setup.h"
#include "rpc/rpclib.h"
#include "reqh/reqh_service.h"
#include "reqh/reqh.h"
#include "layout/layout.h"
#include "layout/linear_enum.h"
#include "layout/pdclust.h"
#include "conf/confc.h"
#include "fdmi/fops.h"
#include "fdmi/service.h"

static int fdms_allocate(struct m0_reqh_service **service,
			 const struct m0_reqh_service_type *stype);
static void fdms_fini(struct m0_reqh_service *service);

static int fdms_start(struct m0_reqh_service *service);
static void fdms_prepare_to_stop(struct m0_reqh_service *service);
static void fdms_stop(struct m0_reqh_service *service);

/**
 * FDMI Service type operations.
 */
static const struct m0_reqh_service_type_ops fdms_type_ops = {
	.rsto_service_allocate = fdms_allocate
};

/**
 * FDMI Service operations.
 */
static const struct m0_reqh_service_ops fdms_ops = {
	.rso_start           = fdms_start,
	.rso_start_async     = m0_reqh_service_async_start_simple,
	.rso_stop            = fdms_stop,
	.rso_prepare_to_stop = fdms_prepare_to_stop,
	.rso_fini            = fdms_fini
};

#ifndef __KERNEL__
M0_INTERNAL struct m0_reqh_service_type m0_fdmi_service_type = {
	.rst_name       = "M0_CST_FDMI",
	.rst_ops        = &fdms_type_ops,
	.rst_level      = M0_FDMI_SVC_LEVEL,
	.rst_typecode   = M0_CST_FDMI,
	.rst_keep_alive = true
};
#endif

M0_INTERNAL int m0_fdms_register(void)
{
	M0_ENTRY();
	m0_fdmi__src_dock_fom_init();
#ifndef __KERNEL__
	m0_fdmi__plugin_dock_fom_init();
	m0_reqh_service_type_register(&m0_fdmi_service_type);
#endif

	return M0_RC(m0_fdms_fop_init());
}

M0_INTERNAL void m0_fdms_unregister(void)
{
	M0_ENTRY();
	m0_reqh_service_type_unregister(&m0_fdmi_service_type);
	m0_fdms_fop_fini();
	M0_LEAVE();
}

static int fdms_allocate(struct m0_reqh_service **service,
                         const struct m0_reqh_service_type *stype)
{
	struct m0_reqh_fdmi_service *fdms;

	M0_ENTRY();

	M0_PRE(service != NULL && stype != NULL);

	M0_ALLOC_PTR(fdms);
	if (fdms == NULL)
		return M0_RC(-ENOMEM);

	fdms->rfdms_magic = M0_FDMS_REQH_SVC_MAGIC;

	*service = &fdms->rfdms_gen;
	(*service)->rs_ops = &fdms_ops;
	return M0_RC(0);
}

/**
 * Finalise FDMI Service instance.
 * This operation finalises service instance and de-allocate it.
 *
 * @param service pointer to service instance.
 *
 * @pre service != NULL
 */
static void fdms_fini(struct m0_reqh_service *service)
{
	struct m0_reqh_fdmi_service *serv_obj;

	M0_ENTRY();

	M0_PRE(service != NULL);

	serv_obj = container_of(service,
				struct m0_reqh_fdmi_service, rfdms_gen);
	m0_free(serv_obj);

	M0_LEAVE();
}

/**
 * Start FDMI Service.
 * @param service pointer to service instance.
 *
 * @pre service != NULL
 */
static int fdms_start(struct m0_reqh_service *service)
{
	const struct m0_filterc_ops    *filterc_ops = &filterc_def_ops;
	struct m0_reqh_fdmi_service    *fdms;
	struct m0_reqh_fdmi_svc_params *start_params;
	int                             rc;

	M0_ENTRY();

	M0_PRE(service != NULL);

	fdms = container_of(service, struct m0_reqh_fdmi_service, rfdms_gen);

	/* UT can patch filterC operations */
	if (m0_buf_is_set(&service->rs_ss_param)) {
		start_params =
			(struct m0_reqh_fdmi_svc_params *)
			service->rs_ss_param.b_addr;
		filterc_ops = start_params->filterc_ops;
	}

	rc = m0_fdmi__plugin_dock_start(service->rs_reqh);

	if (rc == 0) {
		rc = m0_fdmi__src_dock_fom_start(m0_fdmi_src_dock_get(),
		       	filterc_ops, service->rs_reqh);

		if (rc != 0) {
			/* FIXME: Temporary workaround. Don't want the whole service
			 * startup failure (phase 2) */

			M0_LOG(M0_WARN,
			       "can't start FDMI source dock fom %d", rc);
			/*m0_fdmi__plugin_dock_stop();*/
			rc = 0;
		} else {
			fdms->rfdms_src_dock_inited = true;
		}
	}

	return M0_RC(rc);
}

/**
 * Preparing FDMI Service to stop
 * @param service pointer to service instance.
 *
 * @pre service != NULL
 */
static void fdms_prepare_to_stop(struct m0_reqh_service *service)
{
	struct m0_reqh_fdmi_service *fdms;

	M0_PRE(service != NULL);

	fdms = container_of(service, struct m0_reqh_fdmi_service, rfdms_gen);

	m0_fdmi__plugin_dock_stop();

	if (fdms->rfdms_src_dock_inited)
		m0_fdmi__src_dock_fom_stop(m0_fdmi_src_dock_get());
}

/**
 * Stops FDMI Service.
 * @param service pointer to service instance.
 *
 * @pre service != NULL
 */
static void fdms_stop(struct m0_reqh_service *service)
{;}

#undef M0_TRACE_SUBSYSTEM

/** @} endgroup fdmi_main */

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
