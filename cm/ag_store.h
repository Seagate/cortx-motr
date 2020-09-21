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

#ifndef __MOTR_CM_AG_STORE_H__
#define __MOTR_CM_AG_STORE_H__

#include "cm/ag.h"
#include "fop/fom.h"

/**
   @defgroup CMSTORE copy machine store
   @ingroup CM

   @{
 */

enum m0_cm_ag_store_status {
	S_ACTIVE,
	S_COMPLETE,
	S_FINI
};

struct m0_cm_ag_store_data {
	struct m0_cm_ag_id d_in;
	struct m0_cm_ag_id d_out;
	m0_time_t          d_cm_epoch;
};

struct m0_cm_ag_store {
	struct m0_fom              s_fom;
	struct m0_cm_ag_store_data s_data;
	enum m0_cm_ag_store_status s_status;
};

M0_INTERNAL void m0_cm_ag_store_init(struct m0_cm_type *cmtype);
M0_INTERNAL void m0_cm_ag_store_fom_start(struct m0_cm *cm);
M0_INTERNAL void m0_cm_ag_store_complete(struct m0_cm_ag_store *store);
M0_INTERNAL void m0_cm_ag_store_fini(struct m0_cm_ag_store *store);
M0_INTERNAL bool m0_cm_ag_store_is_complete(struct m0_cm_ag_store *store);

/** @} CMSTORE */

#endif /* __MOTR_CM_AG_STORE_H__ */
/*
 *  Local variables:
 *  c-indentation-style: "K&R"
 *  c-basic-offset: 8
 *  tab-width: 8
 *  fill-column: 80
 *  scroll-step: 1
 *  End:
 */
