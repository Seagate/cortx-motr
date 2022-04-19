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

#ifndef __MOTR___DTM0_PRUNER_H__
#define __MOTR___DTM0_PRUNER_H__

#include "fop/fom.h"

/**
 * @defgroup dtm0
 *
 * @{
 */

/*
 * DTM0 log pruner overview
 * ------------------------
 *
 *   Pruner is a component responsible for removal of DTM0 log records that
 * are no longer need (as per requirements for durability). There are two
 * procedures in the cluster that makes log records redundant: stabilisation
 * and eviction. In both cases, DTM0 log "unlinks" log records from originator's
 * list and moves the record into the pruner list. Once a record linked to that
 * list, it shall be eventually removed. The only difference between the cases
 * is that eviction is forced from the pruner side. See the DTM0 log
 *  description for the details about these lists.
 *
 *   Interaction with other components:
 *
 *   +--------+                   +-----+
 *   | Pruner | <--- STABLE/P --- | Log |
 *   |        | ---- PRUNE -----> |     |
 *   +--------+                   +-----+
 *       /|\             +----+
 *        +------ F -----| HA |
 *                       +----+
 */

struct m0_dtm0_pruner_fom;

struct m0_dtm0_pruner_cfg {
	struct m0_co_fom_service *dpc_cfs;
	struct m0_dtm0_log       *dpc_dol;
};

struct m0_dtm0_pruner {
	struct m0_dtm0_pruner_fom *dp_pruner_fom;
	struct m0_dtm0_pruner_cfg  dp_cfg;
};

M0_INTERNAL int m0_dtm0_pruner_init(struct m0_dtm0_pruner     *dpn,
				    struct m0_dtm0_pruner_cfg *dpn_cfg);
M0_INTERNAL void m0_dtm0_pruner_fini(struct m0_dtm0_pruner *dpn);
M0_INTERNAL void m0_dtm0_pruner_start(struct m0_dtm0_pruner *dpn);
M0_INTERNAL void m0_dtm0_pruner_stop(struct m0_dtm0_pruner *dpn);

M0_INTERNAL void m0_dtm0_pruner_mod_init(void);
M0_INTERNAL void m0_dtm0_pruner_mod_fini(void);

/** @} end of dtm0 group */
#endif /* __MOTR___DTM0_PRUNER_H__ */

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
