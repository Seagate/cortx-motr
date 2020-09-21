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

#ifndef __MOTR_LIB_REFS_H__
#define __MOTR_LIB_REFS_H__

#include "lib/atomic.h"

/**
 routines for handling generic reference counted objects
*/

struct m0_ref {
	/**
	 number references to object
	 */
	struct m0_atomic64	ref_cnt;
	/**
	  ponter to destructor
	  @param ref pointer to reference object
	*/
	void (*release) (struct m0_ref *ref);
};

/**
 constructor for init reference counted protection

 @param ref pointer to m0_ref object
 @param init_num initial references on object
 @param release destructor function for the object
*/
void m0_ref_init(struct m0_ref *ref, int init_num,
		void (*release) (struct m0_ref *ref));

/**
 take one reference to the object

 @param ref pointer to m0_ref object

 @return none
 */
M0_INTERNAL void m0_ref_get(struct m0_ref *ref);

/**
 Release one reference from the object.
 If function will release last reference, destructor will called.

 @param ref pointer to m0_ref object

 @return none
*/
M0_INTERNAL void m0_ref_put(struct m0_ref *ref);

/**
 Read the current value of the reference count from the m0_ref object

 @param ref pointer to m0_ref object

 @return current value of the reference count
 */
M0_INTERNAL int64_t m0_ref_read(const struct m0_ref *ref);
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
