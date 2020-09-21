/* -*- C -*- */
/*
 * Copyright (c) 2014-2020 Seagate Technology LLC and/or its Affiliates
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

#ifndef __MOTR_SSS_SS_FOPS_H__
#define __MOTR_SSS_SS_FOPS_H__

#include "conf/onwire.h"
#include "lib/types_xc.h"
#include "lib/buf_xc.h"
#include "fid/fid.h"
#include "fid/fid_xc.h"

/**
 * @defgroup ss_fop Start stop FOP
 * @{
 */

struct m0_ref;
extern struct m0_fop_type m0_fop_ss_fopt;
extern struct m0_fop_type m0_fop_ss_rep_fopt;
extern struct m0_fop_type m0_fop_ss_svc_list_fopt;
extern struct m0_fop_type m0_fop_ss_svc_list_rep_fopt;

/** Service commands. */
enum m0_sss_req_cmd {
	M0_SERVICE_START,
	M0_SERVICE_STOP,
	M0_SERVICE_STATUS,
	M0_SERVICE_QUIESCE,
	M0_SERVICE_INIT,
	M0_SERVICE_HEALTH,
};

/** Request to start/stop a service. */
struct m0_sss_req {
	/**
	 * Command to execute.
	 * @see enum m0_sss_req_cmd
	 */
	uint32_t      ss_cmd;
	/**
	 * Name of service type. Mandatory only for M0_SERVICE_INIT command.
	 * @see m0_reqh_service_type::rst_name
	 */
	struct m0_buf ss_name;
	/**
	 * Identifier of the service being started.
	 * fid type should set to M0_CONF_SERVICE_TYPE.cot_ftype
	 */
	struct m0_fid ss_id;
	/** Opaque parameter. */
	struct m0_buf ss_param;
} M0_XCA_RECORD M0_XCA_DOMAIN(rpc);

/** Response to m0_sss_req. */
struct m0_sss_rep {
	/**
	 * Result of service operation
	 */
	int32_t  ssr_rc;
	/**
	 * Service status. Undefined if ssr_rc < 0.
	 * @see enum m0_reqh_service_state
	 */
	uint32_t ssr_state;
} M0_XCA_RECORD M0_XCA_DOMAIN(rpc);

M0_INTERNAL int m0_ss_fops_init(void);
M0_INTERNAL void m0_ss_fops_fini(void);
M0_INTERNAL void m0_ss_fop_release(struct m0_ref *ref);

/** @} ss_fop */
#endif /* __MOTR_SSS_SS_FOPS_H__ */

/*
 *  Local variables:
 *  c-indentation-style: "K&R"
 *  c-basic-offset: 8
 *  tab-width: 8
 *  fill-column: 80
 *  scroll-step: 1
 *  End:
 */
