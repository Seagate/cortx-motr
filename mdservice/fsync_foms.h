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

#ifndef __MOTR_MDSERVICE_FSYNC_FOMS_H__
#define __MOTR_MDSERVICE_FSYNC_FOMS_H__

#include "fop/fop.h" /* m0_fom */

/**
 * Configuration of the fsync state machine. Defines the sm's phases
 * and additional information.
 */
M0_EXTERN struct m0_sm_conf m0_fsync_fom_conf;
extern struct m0_sm_state_descr m0_fsync_fom_phases[];

/**
 * Creates a FOM that can process fsync fop requests.
 * @param fop fsync fop request to be processed.
 * @param out output parameter pointing to the created fom.
 * @param reqh pointer to the request handler that will run the fom.
 * @return 0 if the fom was correctly created or an error code otherwise.
 */
M0_INTERNAL int m0_fsync_req_fom_create(struct m0_fop  *fop,
					struct m0_fom **out,
					struct m0_reqh *reqh);


#endif /* __MOTR_MDSERVICE_FSYNC_FOMS_H__ */
/*
 *  Local variables:
 *  c-indentation-style: "K&R"
 *  c-basic-offset: 8
 *  tab-width: 8
 *  fill-column: 80
 *  scroll-step: 1
 *  End:
 */
