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

#ifndef __MOTR_FDMI_FDMI_PLUGIN_DOCK_INTERNAL_H__
#define __MOTR_FDMI_FDMI_PLUGIN_DOCK_INTERNAL_H__

#include "fop/fom.h"
#include "fdmi/plugin_dock.h"

/**
   @defgroup fdmi_pd_int FDMI Plugin Dock internals
   @ingroup fdmi_main

   @see @ref FDMI-DLD-fspec "FDMI Functional Specification"
   @{
*/

/**
   Plugin dock initialisation.
   - Should be called once during FDMI service start
 */

M0_INTERNAL int  m0_fdmi__plugin_dock_init(void);

/**
 * Start plugin dock
 */
M0_INTERNAL int m0_fdmi__plugin_dock_start(struct m0_reqh *reqh);

/**
 * Stop plugin dock
 */
M0_INTERNAL void m0_fdmi__plugin_dock_stop(void);

/**
   Plugin dock de-initialisation.
   - Should be called once during FDMI service shutdown
 */

M0_INTERNAL void m0_fdmi__plugin_dock_fini(void);

/**
   Plugin dock FOM registration.
   - Should be called once during FDMI service start
 */

M0_INTERNAL int  m0_fdmi__plugin_dock_fom_init(void);

/**
   Incoming FDMI record registration in plugin dock communication context.
 */

M0_INTERNAL struct
m0_fdmi_record_reg *m0_fdmi__pdock_fdmi_record_register(struct m0_fop *fop);

/**
   Plugin dock FOM context
 */
struct pdock_fom {
	/** FOM based on record notification FOP */
	struct m0_fom              pf_fom;
	/** FDMI record notification body */
	struct m0_fop_fdmi_record *pf_rec;
	/** Current position in filter ids array the FOM iterates on */
	uint32_t                   pf_pos;
	/** custom FOM finalisation routine, currently intended for use in UT */
	void (*pf_custom_fom_fini)(struct m0_fom *fom);
};

/**
   Helper function, lookup for FDMI filter registration by filter id
   - NOTE: used in UT as well
 */
struct m0_fdmi_filter_reg *
m0_fdmi__pdock_filter_reg_find(const struct m0_fid *fid);

/**
   Helper function, lookup for FDMI record registration by record id
   - NOTE: used in UT as well
 */
struct m0_fdmi_record_reg *
m0_fdmi__pdock_record_reg_find(const struct m0_uint128 *rid);

const struct m0_fom_type_ops *m0_fdmi__pdock_fom_type_ops_get(void);

struct m0_rpc_machine *m0_fdmi__pdock_conn_pool_rpc_machine(void);

/** @} end of fdmi_pd_int group */

#endif

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
