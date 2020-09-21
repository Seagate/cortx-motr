/* -*- C -*- */
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


#define M0_TRACE_SUBSYSTEM M0_TRACE_SUBSYS_OTHER

#include "lib/trace.h"
#include "lib/errno.h"
#include "lib/memory.h"           /* M0_ALLOC_PTR */
#include "fis/fi_command_fops.h"
#include "fis/fi_service.h"

/**
 * @page fis-lspec Fault Injection Service.
 *
 * - @subpage fis-lspec-command
 * - @subpage fis-lspec-command-fops
 * - @subpage fis-lspec-command-fom
 *
 * <b>Fault Injection Service: objectives and use</b>
 *
 * Fault Injection Service is intended to provide fault injection functionality
 * at run time. To be enabled the service must appear in configuration database
 * with service type M0_CST_FIS under the process which is the subject for fault
 * injection. Additionally, command line parameter '-j' must be specified for
 * the process to unlock the service start. This "double-lock" mechanism is
 * intended to protect cluster from accidental FIS start.
 *
 * @see motr/st/m0d-fatal
 *
 * - @ref fis-dlspec "Detailed Logical Specification"
 *
 */

/**
 * @defgroup fis-dlspec FIS Internals
 *
 * @{
 */

static int fis_allocate(struct m0_reqh_service **service,
			const struct m0_reqh_service_type *stype);

static int fis_start(struct m0_reqh_service *service)
{
	M0_ENTRY();
	/**
	 * @note FI command fops become acceptable only in case fault injection
	 * service appears in configuration database and started normal way.
	 */
	m0_fi_command_fop_init();
	return M0_RC(0);
}

static void fis_stop(struct m0_reqh_service *service)
{
	M0_ENTRY();
	m0_fi_command_fop_fini();
	M0_LEAVE();
}

static void fis_fini(struct m0_reqh_service *service)
{
	/* Nothing to finalise here. */
}

static const struct m0_reqh_service_type_ops fis_type_ops = {
	.rsto_service_allocate = fis_allocate
};

static const struct m0_reqh_service_ops fis_ops = {
	.rso_start = fis_start,
	.rso_stop  = fis_stop,
	.rso_fini  = fis_fini,
};

struct m0_reqh_service_type m0_fis_type = {
	.rst_name     = FI_SERVICE_NAME,
	.rst_ops      = &fis_type_ops,
	.rst_level    = M0_RPC_SVC_LEVEL,
	.rst_typecode = M0_CST_FIS,
};

static const struct m0_bob_type fis_bob = {
	.bt_name = FI_SERVICE_NAME,
	.bt_magix_offset = offsetof(struct m0_reqh_fi_service, fis_magic),
	.bt_magix = M0_FI_SERVICE_MAGIC,
	.bt_check = NULL
};

M0_BOB_DEFINE(M0_INTERNAL, &fis_bob, m0_reqh_fi_service);

/**
 * Allocates @ref m0_reqh_fi_service instance and initialises it as BOB. Exposes
 * standard @ref m0_reqh_service interface outside for registration with REQH.
 */
static int fis_allocate(struct m0_reqh_service **service,
			const struct m0_reqh_service_type *stype)
{
	struct m0_reqh_fi_service *fis;

	M0_ENTRY();
	M0_PRE(service != NULL && stype != NULL);

	M0_ALLOC_PTR(fis);
	if (fis == NULL)
		return M0_ERR(-ENOMEM);
	m0_reqh_fi_service_bob_init(fis);
	*service = &fis->fis_svc;
	(*service)->rs_ops = &fis_ops;
	return M0_RC(0);
}

M0_INTERNAL int m0_fis_register(void)
{
	M0_ENTRY();
	m0_reqh_service_type_register(&m0_fis_type);
	return M0_RC(0);
}

M0_INTERNAL void m0_fis_unregister(void)
{
	M0_ENTRY();
	m0_reqh_service_type_unregister(&m0_fis_type);
	M0_LEAVE();
}

/** @} end fis-dlspec */
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
