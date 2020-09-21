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

#ifndef __MOTR_SNS_CM_SERVICE_H__
#define __MOTR_SNS_CM_SERVICE_H__

#include "cm/cm.h"
/**
  @defgroup SNSCMSVC SNS copy machine service
  @ingroup SNSCM

  @{
*/

/**
   State of current running repair or rebalance process. It is used for
   indicating. It is used to show the status of the operation to the Spiel
   %client.

   @dot
   digraph sns_service_states {
       node [shape=ellipse, fontsize=12];
       INIT [shape=point];
       IDLE [label="IDLE"];
       STARTED [label="STARTED"];
       PAUSED [label="PAUSED"];
       FAILED [label="FAILED"];
       INIT -> IDLE;
       IDLE -> STARTED [label="Start"];
       STARTED -> IDLE [label="Finish"];
       STARTED -> PAUSED [label="Pause"];
       PAUSED -> STARTED [label="Resume"];
       STARTED -> FAILED [label="Fail"];
       FAILED -> STARTED [label="Start again"];
   }
   @enddot

   @see m0_spiel_sns_repair_status
   @see m0_spiel_sns_rebalance_status
 */

/**
 * Allocates and initialises SNS copy machine.
 * This allocates struct m0_sns_cm and invokes m0_cm_init() to initialise
 * m0_sns_cm::rc_base.
 */
M0_INTERNAL int
m0_sns_cm_svc_allocate(struct m0_reqh_service **service,
		       const struct m0_reqh_service_type *stype,
		       const struct m0_reqh_service_ops *svc_ops,
		       const struct m0_cm_ops *cm_ops);

/**
 * Sets up copy machine corresponding to the given service.
 * Invokes m0_cm_setup().
 */
M0_INTERNAL int m0_sns_cm_svc_start(struct m0_reqh_service *service);

/**
 * Finalises copy machine corresponding to the given service.
 * Invokes m0_cm_fini().
 */
M0_INTERNAL void m0_sns_cm_svc_stop(struct m0_reqh_service *service);

/**
 * Destorys SNS copy machine (struct m0_sns_cm) correponding to the
 * given service.
 */
M0_INTERNAL void m0_sns_cm_svc_fini(struct m0_reqh_service *service);

/** @} SNSCMSVC */
#endif /* __MOTR_SNS_CM_SERVICE_H__ */

/*
 *  Local variables:
 *  c-indentation-style: "K&R"
 *  c-basic-offset: 8
 *  tab-width: 8
 *  fill-column: 80
 *  scroll-step: 1
 *  End:
 */
