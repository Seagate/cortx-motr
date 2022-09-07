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


#pragma once

#ifndef __MOTR_FIS_FI_SERVICE_H__
#define __MOTR_FIS_FI_SERVICE_H__

#include "reqh/reqh_service.h"

/**
 * @page fis-dld Fault Injection at run time DLD
 * - @subpage fis-fspec "Functional Specification"
 * - @subpage fis-lspec "Logical Specification"
 *
 * Typically used in UT suites, the fault injection functionality can be used in
 * system tests as well. In order to control fault injections a service of a
 * special M0_CST_FIS type is enabled and started in motr instance allowing it
 * accept respective FOPs. From now on any cluster participant is able to post a
 * particular FOP to the motr instance that is to control its fault injection
 * point states based on received information.
 *
 * @note Fault injection mechanisms take effect only in debug builds when
 * appropriate build configuration parameters were applied. And in release
 * builds FI appears disabled, so even with FI service up and running the posted
 * commands are going to cause no effect despite reported success code.
 */

/**
 * @page fis-fspec Fault Injection Service functions.
 * - @subpage fis-fspec-command
 *
 * FI service is registered with REQH by calling m0_fis_register(). The
 * registration occurs during motr instance initialisation. The registration
 * makes FIS service type be available globally in motr instance.
 *
 * During motr finalisation FIS service type is unregistered by calling
 * m0_fis_unregister().
 *
 * @note FIS related FOPs are not registered along with the service type (see
 * fis_start() for the details of FOPs registration).
 */

/**
 * @defgroup fis-dfspec Fault Injection Service (FIS)
 * @brief Detailed Functional Specification.
 *
 * @{
 */
#define FI_SERVICE_NAME "M0_CST_FIS"

/** Service structure to be registered with REQH. */
struct m0_reqh_fi_service {
	/** Request handler service representation */
	struct m0_reqh_service fis_svc;

	/** fis_magic == M0_FI_SERVICE_MAGIC */
	uint64_t               fis_magic;
};

M0_INTERNAL int m0_fis_register(void);
M0_INTERNAL void m0_fis_unregister(void);

extern struct m0_reqh_service_type m0_fis_type;
/** @} fis-dfspec */
#endif /* __MOTR_FIS_FI_SERVICE_H__ */

/*
 *  Local variables:
 *  c-indentation-style: "K&R"
 *  c-basic-offset: 8
 *  tab-width: 8
 *  fill-column: 80
 *  scroll-step: 1
 *  End:
 */
