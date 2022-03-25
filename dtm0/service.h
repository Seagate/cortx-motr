/* -*- C -*- */
/*
 * Copyright (c) 2021 Seagate Technology LLC and/or its Affiliates
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
#include "dtm0/clk_src.h"

struct m0_be_dtm0_log;
struct dtm0_req_fop;
struct m0_be_queue;

enum m0_dtm0_service_origin {
	DTM0_UNKNOWN = 0,
	DTM0_ON_VOLATILE,
	DTM0_ON_PERSISTENT,
};

/**
 * DTM0 service structure
 */
struct m0_dtm0_service {
	struct m0_reqh_service       dos_generic;
	struct m0_tl                 dos_processes;
	enum m0_dtm0_service_origin  dos_origin;
	uint64_t                     dos_magix;
	struct m0_dtm0_clk_src       dos_clk_src;
	struct m0_be_dtm0_log       *dos_log;
	/*
	 * A queue for DTM_TEST message for drlink UTs.
	 * The UTs are fully responsible for the queue init/fini/get.
	 * DTM_TEST fom puts dtm0_req_fop::dtr_txr::dtd_id::dti_fid to the
	 * queue.
	 */
	struct m0_be_queue          *dos_ut_queue;
};

extern struct m0_reqh_service_type dtm0_service_type;

M0_INTERNAL int m0_dtm0_stype_init(void);
M0_INTERNAL void m0_dtm0_stype_fini(void);

M0_INTERNAL int
m0_dtm_client_service_start(struct m0_reqh *reqh, struct m0_fid *cli_srv_fid,
			    struct m0_reqh_service **out);
M0_INTERNAL void m0_dtm_client_service_stop(struct m0_reqh_service *svc);

M0_INTERNAL int m0_dtm0_service_process_connect(struct m0_reqh_service *s,
						struct m0_fid *remote_srv,
						const char    *remote_ep,
						bool           async);
M0_INTERNAL int
m0_dtm0_service_process_disconnect(struct m0_reqh_service *s,
				   struct m0_fid          *remote_srv);

M0_INTERNAL struct m0_rpc_session *
m0_dtm0_service_process_session_get(struct m0_reqh_service *s,
				    const struct m0_fid    *remote_srv);

M0_INTERNAL bool m0_dtm0_is_a_volatile_dtm(struct m0_reqh_service *service);
M0_INTERNAL bool m0_dtm0_is_a_persistent_dtm(struct m0_reqh_service *service);

M0_INTERNAL struct m0_dtm0_service *
m0_dtm0_service_find(const struct m0_reqh *reqh);

/** Get the DTM0 service this FOM belongs to. */
M0_INTERNAL struct m0_dtm0_service *m0_dtm0_fom2service(struct m0_fom *fom);

M0_INTERNAL bool m0_dtm0_in_ut(void);

#endif /* __MOTR_DTM0_SERVICE_H__ */

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
