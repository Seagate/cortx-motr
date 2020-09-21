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

#ifndef __MOTR_IOSERVICE_IOS_START_SM_H__
#define __MOTR_IOSERVICE_IOS_START_SM_H__

/**
 * @defgroup io_start_sm IO Service State Machines
 *
 * State machine for start IO service
 * @see DLD_bulk_server_fspec_ioservice_operations
 *
 * State machines execute  general steps async:
 * - Create COB domain if it does not exist
 * - Create Create initial files system structures, @see m0_cob_domain_mkfs
 * - Poolmachine init
 *
 *  @{
 */

#include "lib/chan.h"
#include "cob/cob.h"
#include "conf/confc.h"
#include "conf/diter.h"
#include "dtm/dtm.h"
#include "sm/sm.h"

struct m0_fom;
struct m0_reqh;
struct m0_be_seg;
struct m0_be_tx;
struct m0_poolmach;


/**
 * I/O service start SM context.
 */
struct m0_ios_start_sm
{
	/**
	 * ioservice asynchronous start stata machine.
	 *
	 * This machine belongs to locality0 sm group.
	 */
	struct m0_sm                ism_sm;
	/** IO service COB domain */
	struct m0_cob_domain       *ism_dom;
	/** IO service COB domain ID */
	struct m0_cob_domain_id     ism_cdom_id;
	/** Request handler IO service instance */
	struct m0_reqh_service     *ism_service;
	/** Request handler */
	struct m0_reqh             *ism_reqh;
	/** BE TX instance, used three times, @see m0_ios_start_state */
	struct m0_be_tx             ism_tx;
	/** Clink to wait on be_tx and conf_obj channels */
	struct m0_clink             ism_clink;
	/** AST scheduler current states */
	struct m0_sm_ast            ism_ast;
	/** Stores result of last operation between state transitions */
	int                         ism_last_rc;
};

/**
 * IO service start SM states.
 *
 * Note! BE TX create/open/close/destroy up to three times:
 * create COB domain, create MKFS, init poolmachine
 */
enum m0_ios_start_state {
	/**
	 * SM initial state. Next state is determined on whether COB domain is
	 * created or not. Get m0_get()->i_ios_cdom_key, if COB domain doesn't
	 * exist then open BE TX to create it, otherwise initialise COB domain.
	 */
	M0_IOS_START_INIT,
	/**
	 * Create COB domain, close BE TX
	 */
	M0_IOS_START_CDOM_CREATE,
	/**
	 * If COM domain is created then open BE TX for create MKFS else
	 * create poolmachine and start BE TX for fill poolmachine
	 */
	M0_IOS_START_CDOM_CREATE_RES,
	/**
	 * Create MKFS, close BE TX
	 */
	M0_IOS_START_MKFS,
	/**
	 * Create poolmachine
	 */
	M0_IOS_START_MKFS_RESULT,
	/**
	 * Create buffer pool and get filesystem conf object
	 */
	M0_IOS_START_BUFFER_POOL_CREATE,
	/**
	 * Handle errors from other states
	 */
	M0_IOS_START_FAILURE,
	/**
	 * Finish state
	 */
	M0_IOS_START_COMPLETE
};

M0_INTERNAL void m0_ios_start_lock(struct m0_ios_start_sm *ios_sm);
M0_INTERNAL void m0_ios_start_unlock(struct m0_ios_start_sm *ios_sm);

/**
 * Initialize IO service start State machine
 *
 * @param ios_sm - IO service start State machine instance
 * @param service - IO service
 * @param grp - m0_sm_group for initialize ios_sm->ism_sm
 */
M0_INTERNAL int m0_ios_start_sm_init(struct m0_ios_start_sm  *ios_sm,
				     struct m0_reqh_service  *service,
				     struct m0_sm_group      *grp);

/**
 * Execute IO service start SM. User is expected to wait until
 * ios_sm->ism_sm.sm_state is in (M0_IOS_START_COMPLETE, M0_IOS_START_FAILURE).
 */
M0_INTERNAL void m0_ios_start_sm_exec(struct m0_ios_start_sm *ios_sm);

/**
 * Finalize IO service start SM.
 */
M0_INTERNAL void m0_ios_start_sm_fini(struct m0_ios_start_sm *ios_sm);

/**
 *  @}
*/

#endif /* __MOTR_IOSERVICE_IOS_START_SM_H__ */

/*
 *  Local variables:
 *  c-indentation-style: "K&R"
 *  c-basic-offset: 8
 *  tab-width: 8
 *  fill-column: 80
 *  scroll-step: 1
 *  End:
 */
