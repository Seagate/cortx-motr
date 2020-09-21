/* -*- C -*- */
/*
 * Copyright (c) 2011-2020 Seagate Technology LLC and/or its Affiliates
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

#ifndef __MOTR_MDSERVICE_MD_SERVICE_H__
#define __MOTR_MDSERVICE_MD_SERVICE_H__

#include "reqh/reqh_service.h"   /* m0_reqh_service */

/**
 * @defgroup mdservice MD Service Operations
 * @see @ref reqh
 *
 * MD Service defines service operation vector -
 * - MD Service operation @ref m0_mds_start()<br>
 *   Initiate buffer_pool and register I/O FOP with service
 * - MD Service operation @ref m0_mds_stop()<br>
 *   Free buffer_pool and unregister I/O FOP with service
 * - MD Service operation @ref m0_mds_fini()<br>
 *   Free MD Service instance.
 *
 *  @{
 */

/**
 * Structure contains generic service structure and
 * service specific information.
 */
struct m0_reqh_md_service {
	/** Generic reqh service object */
	struct m0_reqh_service	rmds_gen;
	/** Magic to check io service object */
	uint64_t		rmds_magic;
};

M0_INTERNAL void m0_mds_unregister(void);
M0_INTERNAL int m0_mds_register(void);

/** @} end of mdservice */

#endif /* __MOTR_MDSERVICE_MD_SERVICE_H__ */
/*
 *  Local variables:
 *  c-indentation-style: "K&R"
 *  c-basic-offset: 8
 *  tab-width: 8
 *  fill-column: 80
 *  scroll-step: 1
 *  End:
 */
