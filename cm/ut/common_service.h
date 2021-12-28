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
#ifndef __MOTR_CM_UT_COMMON_SERVICE_H__
#define __MOTR_CM_UT_COMMON_SERVICE_H__

#include "cm/cm.h"
#include "cm/cp.h"
#include "cm/ag.h"
#include "lib/misc.h"
#include "lib/finject.h"
#include "lib/memory.h"
#include "ut/ut.h"
#include "lib/chan.h"
#include "reqh/reqh.h"
#include "reqh/reqh_service.h"
#include "motr/setup.h"
#include "ut/ut_rpc_machine.h"

#define DUMMY_DBNAME      "dummy-db"
#define DUMMY_COB_ID      20
#define DUMMY_SERVER_ADDR M0_UT_DUMMY_EP_ADDR

extern struct m0_cm_cp            cm_ut_cp;
extern struct m0_reqh_service    *cm_ut_service;
extern struct m0_ut_rpc_mach_ctx  cmut_rmach_ctx;

enum {
	AG_ID_NR = 100,
	CM_UT_LOCAL_CP_NR = 4,
	MAX_CM_NR = 2
};

struct m0_ut_cm {
	uint64_t        ut_cm_id;
	struct m0_cm    ut_cm;
	struct m0_chan  ut_cm_wait;
	struct m0_mutex ut_cm_wait_mutex;
};

extern struct m0_reqh           cm_ut_reqh;
extern struct m0_cm_cp          cm_ut_cp;
extern struct m0_ut_cm          cm_ut[MAX_CM_NR];
extern struct m0_reqh_service  *cm_ut_service;
extern struct m0_mutex          cm_wait_mutex;
extern struct m0_chan           cm_wait;

extern struct m0_cm_type                 cm_ut_cmt;
extern const struct m0_cm_aggr_group_ops cm_ag_ut_ops;
extern uint64_t                          ut_cm_id;
extern bool                              test_ready_fop;

void cm_ut_service_alloc_init(struct m0_reqh *reqh);
void cm_ut_service_cleanup();

#endif /** __MOTR_CM_UT_COMMON_SERVICE_H__ */
/*
 *  Local variables:
 *  c-indentation-style: "K&R"
 *  c-basic-offset: 8
 *  tab-width: 8
 *  fill-column: 80
 *  scroll-step: 1
 *  End:
 */
