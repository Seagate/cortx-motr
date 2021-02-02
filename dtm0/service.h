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


#pragma once

#ifndef __MOTR_DTM0_SERVICE_H__
#define __MOTR_DTM0_SERVICE_H__

#include "reqh/reqh_service.h"

extern struct m0_reqh_service_type dtm0_service_type;

M0_INTERNAL int m0_dtm0_stype_init(void);
M0_INTERNAL void m0_dtm0_stype_fini(void);


M0_INTERNAL int m0_dtm0_service_process_connect(struct m0_reqh_service *s,
						struct m0_fid *remote_srv,
						const char    *remote_ep);
M0_INTERNAL int m0_dtm0_service_process_disconnect(struct m0_reqh_service *s,
						   struct m0_fid *remote_srv);
M0_INTERNAL struct m0_rpc_session *
m0_dtm0_service_process_session_get(struct m0_reqh_service *s,
				    struct m0_fid *remote_srv);

M0_INTERNAL bool m0_dtm0_is_a_volatile_dtm(struct m0_reqh_service *service);
M0_INTERNAL bool m0_dtm0_is_a_persistent_dtm(struct m0_reqh_service *service);

#endif /* __MOTR_DTM0_SERVICE_H__ */
