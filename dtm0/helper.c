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
#define M0_TRACE_SUBSYSTEM M0_TRACE_SUBSYS_DTM0
#include "lib/trace.h"
#include "lib/assert.h"
#include "reqh/reqh_service.h"
#include "reqh/reqh.h"               /* m0_reqh */

M0_INTERNAL struct m0_reqh_service *
m0_dtm__client_service_start(struct m0_reqh *reqh, struct m0_fid *cli_srv_fid)
{
       struct m0_reqh_service_type *svct;
       struct m0_reqh_service      *reqh_svc;
       int rc;

       svct = m0_reqh_service_type_find("M0_CST_DTM0");
       M0_ASSERT(svct != NULL);

       rc = m0_reqh_service_allocate(&reqh_svc, svct, NULL);
       M0_ASSERT(rc == 0);

       m0_reqh_service_init(reqh_svc, reqh, cli_srv_fid);

       rc = m0_reqh_service_start(reqh_svc);
       M0_ASSERT(rc == 0);

       return reqh_svc;
}

M0_INTERNAL void m0_dtm__client_service_stop(struct m0_reqh_service *svc)
{
       m0_reqh_service_prepare_to_stop(svc);
       m0_reqh_idle_wait_for(svc->rs_reqh, svc);
       m0_reqh_service_stop(svc);
       m0_reqh_service_fini(svc);
}

/*
 *  Local variables:
 *  c-indentation-style: "K&R"
 *  c-basic-offset: 8
 *  tab-width: 8
 *  fill-column: 80
 *  scroll-step: 1
 *  End:
 */
