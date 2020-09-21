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

#ifndef __MOTR_MOTR_INIT_H__
#define __MOTR_MOTR_INIT_H__

struct m0;

/**
   @defgroup init Motr initialisation calls.

   @{
 */

#if 1 /* XXX OBSOLETE */
/**
   Performs all global initializations of M0 sub-systems. The nomenclature of
   sub-systems to be initialised depends on the build configuration.

   @see m0_fini().
 */
int m0_init(struct m0 *instance);

/**
   Finalizes all sub-systems initialised by m0_init().
 */
void m0_fini(void);

/**
   Performs part global initializations of M0 sub-systems, when stopped before
   reconfigure Motr.

   @see m0_init(), @see m0_fini(), @see m0_quiesce().
 */
int m0_resume(struct m0 *instance);

/**
   Finalizes part global initializations of M0 sub-systems, for starting
   reconfigure Motr. Sub-systems finalize from high level to quiese level.
   @see M0_LEVEL_INST_QUIESCE_SYSTEM.

   @see m0_init(), @see m0_fini(), @see m0_resume().
 */
void m0_quiesce(void);

#endif /* XXX OBSOLETE */

/** @} end of init group */
#endif /* __MOTR_MOTR_LIST_H__ */

/*
 *  Local variables:
 *  c-indentation-style: "K&R"
 *  c-basic-offset: 8
 *  tab-width: 8
 *  fill-column: 80
 *  scroll-step: 1
 *  End:
 */
