/* -*- C -*- */
/*
 * Copyright (c) 2015-2020 Seagate Technology LLC and/or its Affiliates
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


#pragma once

#ifndef __MOTR_RM_RM_SERVICE_H__
#define __MOTR_RM_RM_SERVICE_H__

#include "reqh/reqh_service.h"
#include "rm/rm.h"

/**
   @defgroup rm_service Resource manager service (RMS)
   @{

   - @ref rmsvc-ds
   - @ref rmsvc-sub

   @brief RMS provides service management API for resource manager. RMS will
   be registered with Motr request handler and the service will provide
   interfaces to resource manager, like identification of owner, borrow or
   revoke requests &c.

   @section rmsvc-ds Data Structures

   RMS will be represented by an instance of m0_reqh_rm_service, which will be a
   wrapper to contain m0_reqh_service. RMS will be registered during Motr
   server initialisation and request handler service specific routines will be
   provided. RMS has following components associated with it:

   - m0_reqh_service: Representation of a service on node

   - m0_rm_domain: Resource manager domain

   It will be simply assumed that the request handler that runs confd also runs
   resource manager service. Note that there will be multiple resource manager
   services running in the system, but we need one for boot-strapping to access
   confd. The locations of other resource manager services can be obtained from
   confd.

   @section rmsvc-sub Subroutines
   Following new API's are introduced:

   - m0_rms_register(): Register RMS. This will initialise the resource manager
   domain.

   - m0_rms_unregister(): Unregister RMS.

   @todo All the creditors for the resources reside inside RM service;
   i.e. RM service will be the creditor for all currently available resource
   types. When specific resource types are implemented, this can be changed.
*/

struct m0_reqh_rm_service {
	/** Request handler service representation */
	struct m0_reqh_service     rms_svc;

	/** Resource manager domain */
	struct m0_rm_domain        rms_dom;

	/** Supported type: file lock */
	struct m0_rm_resource_type rms_flock_rt;

	/** Supported type: RW lockable resources */
	struct m0_rm_resource_type rms_rwlockable_rt;

	/** rms_magic == M0_RM_SERVICE_MAGIC */
	uint64_t                   rms_magic;
};

M0_INTERNAL int m0_rms_register(void);
M0_INTERNAL void m0_rms_unregister(void);

/**
 * Creates an owner for resource type description from resbuf.
 * Adds the resource to the domain maintained by m0_reqh_rm_service.
 *
 * @pre service != NULL
 * @pre resbuf != NULL
 */
M0_INTERNAL int m0_rm_svc_owner_create(struct m0_reqh_service *service,
				       struct m0_rm_owner    **owner,
				       struct m0_buf          *resbuf);

M0_INTERNAL struct m0_rm_domain *
m0_rm_svc_domain_get(const struct m0_reqh_service *svc);

extern struct m0_reqh_service_type m0_rms_type;

/** @} end of rm_service group */
#endif /* __MOTR_RM_RM_SERVICE_H__ */

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
