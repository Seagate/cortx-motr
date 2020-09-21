/* -*- C -*- */
/*
 * Copyright (c) 2013-2020 Seagate Technology LLC and/or its Affiliates
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

#ifndef __MOTR_RM_FOMS_H__
#define __MOTR_RM_FOMS_H__

#include "lib/chan.h"
#include "fop/fop.h"
#include "rm/rm.h"
#include "rm/rm_internal.h"
#include "rm/rm_ha.h"        /* m0_rm_ha_subscriber */

/**
 * @addtogroup rm
 *
 * This file includes data structures used by RM:fop layer.
 *
 * @{
 *
 */
enum m0_rm_fom_phases {
	FOPH_RM_REQ_START = M0_FOM_PHASE_INIT,
	FOPH_RM_REQ_FINISH = M0_FOM_PHASE_FINISH,
	FOPH_RM_REQ_CREDIT_GET,
	FOPH_RM_REQ_WAIT,
	/*
	 * Custom step required for asynchronous subscription to debtor death.
	 * So far, only borrow request needs this while subscribing debtor
	 * object to HA notifications.
	 */
	FOPH_RM_REQ_DEBTOR_SUBSCRIBE,
};

struct rm_request_fom {
	/** Generic m0_fom object */
	struct m0_fom                rf_fom;
	/** Incoming request */
	struct m0_rm_remote_incoming rf_in;
	/**
	 * Subscriber for remote failure notifications.
	 * Used if new remote is created to handle borrow request.
	 */
	struct m0_rm_ha_subscriber   rf_sbscr;
};

/** @} */

/* __MOTR_RM_FOMS_H__ */
#endif

/**
 *  Local variables:
 *  c-indentation-style: "K&R"
 *  c-basic-offset: 8
 *  tab-width: 8
 *  fill-column: 80
 *  scroll-step: 1
 *  End:
 */
