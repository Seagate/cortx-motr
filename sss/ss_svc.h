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
#ifndef __MOTR_SSS_SS_SVC_H__
#define __MOTR_SSS_SS_SVC_H__

#include "reqh/reqh.h"
#include "reqh/reqh_service.h"
#include "fop/fom_generic.h"

/**
 * @defgroup ss_svc Start_Stop Service
 * @{

  @section DLD_sss-svc-fom Service command FOM

  - @ref DLD-sss-svc-fspec-ds

  @section DLD-sss-svc-fspec Functional Specification
  Service commands provides control of individual service like init, quiesce,
  start, stop, status and health.

   Generic TX fom phases start TX transaction. Its interfere to start some
   services, like IO service. So Start service made a separate stage.
   Both stages can to diagnose error and can go to Failure phase.
   Fom phase SS_FOM_SWITCH switch to next dependence Service command ID and
   current stage.

   First stage - before TX generic fom phases:
   - initiate fom - check service by fid
   - start service if command ID equal Start command

   Second stage - after TX generic fom phases:
   - execute all command except Start command

   State transition diagram for Start Service command

   @verbatim

              Standard fom generic phases
     inits, authenticate, resources, authorization
			 |
			 v
     +<---------SS_FOM_SWITCH
     |			 |
     |			 v
     +<---------SS_FOM_START
     |			 |
     |			 v
     +<--Standard fom generic phases TX context
     |			 |
     |			 v
     +<---------SS_FOM_SWITCH
     |			 |
     |			 v
 FOPH_FAILED        FOPH_SUCCESS
     +------------------>|
			 v
            Standard fom generic phases
               send reply and finish

   @endverbatim

   State transition diagram for Service non-start commands

   @verbatim

              Standard fom generic phases
     inits, authenticate, resources, authorization
			 |
			 v
     +<---------SS_FOM_SWITCH
     |			 |
     |			 v
     +<--Standard fom generic phases TX context
     |			 |
     |			 v
     +<---------SS_FOM_SWITCH
     |			 |
     |			 v
     +<------execute Service command by FOP command ID
     |			 |
     |			 v
 FOPH_FAILED        FOPH_SUCCESS
     +------------------>|
			 v
            Standard fom generic phases
               send reply and finish

   @endverbatim

 */

enum { MAX_SERVICE_NAME_LEN = 128 };

extern struct m0_reqh_service_type m0_ss_svc_type;

enum ss_fom_phases {
	SS_FOM_SWITCH = M0_FOPH_NR + 1,
	SS_FOM_SVC_INIT,
	SS_FOM_START,
	SS_FOM_START_WAIT,
	SS_FOM_QUIESCE,
	SS_FOM_STOP,
	SS_FOM_STATUS,
	SS_FOM_HEALTH,
};

M0_INTERNAL int m0_ss_svc_init(void);
M0_INTERNAL void m0_ss_svc_fini(void);

/** @} end group ss_svc */

#endif /* __MOTR_SSS_SS_SVC_H__ */
/*
 *  Local variables:
 *  c-indentation-style: "K&R"
 *  c-basic-offset: 8
 *  tab-width: 8
 *  fill-column: 80
 *  scroll-step: 1
 *  End:
 */
