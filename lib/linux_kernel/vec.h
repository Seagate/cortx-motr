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

#ifndef __MOTR_LIB_LINUX_KERNEL_VEC_H__
#define __MOTR_LIB_LINUX_KERNEL_VEC_H__

#include "lib/types.h"

/**
   @addtogroup vec
   @{
*/

struct page;
struct m0_0vec;

/**
   Add a struct page to contents of m0_0vec structure.
   Struct page is kernel representation of a buffer.
   @note The m0_0vec struct should be allocated by user.

   @param zvec The m0_0vec struct to be initialized.
   @param pages Array of kernel pages.
   @param index The target object offset for page.
   @post ++zvec->z_cursor.bc_vc.vc_seg
 */
M0_INTERNAL int m0_0vec_page_add(struct m0_0vec *zvec, struct page *pg,
				 m0_bindex_t index);

/** @} end of vec group */

/* __MOTR_LIB_LINUX_KERNEL_VEC_H__ */
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
