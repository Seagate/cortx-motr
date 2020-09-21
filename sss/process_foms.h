/* -*- C -*- */
/*
 * Copyright (c) 2015-2020 Seagate Technology LLC and/or its Affiliates
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
#ifndef __MOTR_SSS_PROCESS_FOMS_H__
#define __MOTR_SSS_PROCESS_FOMS_H__

#include "reqh/reqh.h"
#include "reqh/reqh_service.h"
#include "fop/fom_generic.h"

/**
 * @defgroup ss_process Process command
 * @{
 */

/**
   Process Reconfig command contains the series of steps:

   1. Save core mask and memory limits to instance data (@ref m0).
      Core mask and memory limits applied in between of finalisation and
      initialisation of modules.

   2. Send signal for finalisation of current Motr instance.
      During finalisation of current Motr instance the system correctly
      finalises all modules and all Motr entities (services, REQH, localities,
      FOPs, FOMs, etc).

   3. Apply core mask and memory limits (see setup.c and instance.c).

   4. Restart Motr instance.

      @note For future development, common possible problem in processing
      Reconfig command is missing of a Motr entity finalisation or cleanup.
 */


/** @} end group ss_process */

#endif /* __MOTR_SSS_PROCESS_FOMS_H__ */
/*
 *  Local variables:
 *  c-indentation-style: "K&R"
 *  c-basic-offset: 8
 *  tab-width: 8
 *  fill-column: 80
 *  scroll-step: 1
 *  End:
 */
