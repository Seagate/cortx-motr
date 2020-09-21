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

#ifndef __MOTR_MOTR_PROCESS_ATTR_H__
#define __MOTR_MOTR_PROCESS_ATTR_H__

#include "lib/bitmap.h"        /* m0_bitmap */

struct m0;

/**
   @addtogroup m0d

   For reconfigure (see @ref m0_ss_process_req) some process attributes
   save into @ref m0 instance and apply on initialize module phase.

   @{
 */

enum {
	/**
	   Default value of Process attribute. Attribute not reply if set to
	   this value.
	*/
	M0_PROCESS_ATTRIBUTE_NO_MEMLIMIT = 0
};

/**
   Define Process attribute structure, which contains information on
   core mask and memory limits. This attribute reply on reconfigure Motr.
*/
struct m0_proc_attr {
	/** Available cores mask */
	struct m0_bitmap pca_core_mask;
	/** Memory limits */
	uint64_t         pca_memlimit_as;
	uint64_t         pca_memlimit_rss;
	uint64_t         pca_memlimit_stack;
	uint64_t         pca_memlimit_memlock;
};

/**
 * Set memory limits.
 *
 * For each value of memory limit with non-default value from m0_proc_attr
 * use pair getrlimit and setrlimit.
 *
 * @return error code
 */
int m0_cs_memory_limits_setup(struct m0 *instance);

/** @}  */

#endif /* __MOTR_MOTR_PROCESS_ATTR_H__ */

/*
 *  Local variables:
 *  c-indentation-style: "K&R"
 *  c-basic-offset: 8
 *  tab-width: 8
 *  fill-column: 80
 *  scroll-step: 1
 *  End:
 */
