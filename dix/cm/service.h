/* -*- C -*- */
/*
 * Copyright (c) 2016-2020 Seagate Technology LLC and/or its Affiliates
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

#ifndef __MOTR_DIX_CM_SERVICE_H__
#define __MOTR_DIX_CM_SERVICE_H__

/**
 * @defgroup dixcm
 *
 * @{
 */

/**
 * Allocates DIX copy machine (service context is embedded into copy machine
 * context), initialises embedded base copy machine, sets up embedded service
 * context and return its pointer.
 *
 * @param[out] service DIX CM service.
 * @param[in]  stype   DIX CM service type.
 * @param[in]  svc_ops DIX CM service operation list.
 * @param[in]  cm_ops  Copy machine operation list.
 * @param[in]  dcmt    DIX copy machine type.
 *
 * @ret 0 on succes or negative error code.
 */
M0_INTERNAL int
m0_dix_cm_svc_allocate(struct m0_reqh_service **service,
		       const struct m0_reqh_service_type *stype,
		       const struct m0_reqh_service_ops *svc_ops,
		       const struct m0_cm_ops *cm_ops,
		       struct m0_dix_cm_type  *dcmt);

/**
 * Sets up a DIX copy machine served by @service.
 *
 * @param service DIX CM service.
 *
 * @ret 0 on succes or negative error code.
 */
M0_INTERNAL int m0_dix_cm_svc_start(struct m0_reqh_service *service);

/**
 * Finalises DIX copy machine and embedded base copy machine.
 *
 * @param service DIX CM service.
 */
M0_INTERNAL void m0_dix_cm_svc_stop(struct m0_reqh_service *service);

/** @} end of dixcm group */
#endif /* __MOTR_DIX_CM_SERVICE_H__ */

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
