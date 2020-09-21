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

#ifndef __MOTR_MOTR_HA_H__
#define __MOTR_MOTR_HA_H__

/**
 * @defgroup motr
 *
 * @{
 */

#include "lib/tlist.h"          /* m0_tl */
#include "lib/types.h"          /* uint64_t */
#include "lib/mutex.h"          /* m0_mutex */
#include "fid/fid.h"            /* m0_fid */
#include "module/module.h"      /* m0_module */
#include "ha/ha.h"              /* m0_ha */
#include "ha/dispatcher.h"      /* m0_ha_dispatcher */

struct m0_rpc_machine;
struct m0_reqh;
struct m0_ha_link;

struct m0_motr_ha_cfg {
	struct m0_ha_dispatcher_cfg  mhc_dispatcher_cfg;
	const char                  *mhc_addr;
	struct m0_rpc_machine       *mhc_rpc_machine;
	struct m0_reqh              *mhc_reqh;
	struct m0_fid                mhc_process_fid;
};

struct m0_motr_ha {
	struct m0_motr_ha_cfg    mh_cfg;
	struct m0_module         mh_module;
	struct m0_ha             mh_ha;
	struct m0_ha_link       *mh_link;
	struct m0_ha_dispatcher  mh_dispatcher;
};

M0_INTERNAL void m0_motr_ha_cfg_make(struct m0_motr_ha_cfg *mha_cfg,
				     struct m0_reqh        *reqh,
				     struct m0_rpc_machine *rmach,
				     const char            *addr);

M0_INTERNAL int m0_motr_ha_init(struct m0_motr_ha     *mha,
                                struct m0_motr_ha_cfg *mha_cfg);
M0_INTERNAL int m0_motr_ha_start(struct m0_motr_ha *mha);
M0_INTERNAL void m0_motr_ha_stop(struct m0_motr_ha *mha);
M0_INTERNAL void m0_motr_ha_fini(struct m0_motr_ha *mha);

M0_INTERNAL void m0_motr_ha_connect(struct m0_motr_ha *mha);
M0_INTERNAL void m0_motr_ha_disconnect(struct m0_motr_ha *mha);

extern const struct m0_ha_ops m0_motr_ha_ops;

/** @} end of motr group */
#endif /* __MOTR_MOTR_HA_H__ */

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
