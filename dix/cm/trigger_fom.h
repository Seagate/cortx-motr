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

#ifndef __MOTR_DIX_CM_TRIGGER_FOM_H__
#define __MOTR_DIX_CM_TRIGGER_FOM_H__

/**
 * @defgroup DIXCM
 *
 * @{
 */

extern const struct m0_fom_type_ops m0_dix_trigger_fom_type_ops;

/** Finalises repair trigger fops. */
M0_INTERNAL void m0_dix_cm_repair_trigger_fop_fini(void);
/** Initialises repair trigger fops. */
M0_INTERNAL void m0_dix_cm_repair_trigger_fop_init(void);
/** Finalises re-balance trigger fops. */
M0_INTERNAL void m0_dix_cm_rebalance_trigger_fop_fini(void);
/** Initialises re-balance trigger fops. */
M0_INTERNAL void m0_dix_cm_rebalance_trigger_fop_init(void);

/** @} end of DIXCM group */
#endif /* __MOTR_DIX_CM_TRIGGER_FOM_H__ */

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
