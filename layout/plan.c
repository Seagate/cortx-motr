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
 * Original author: Andriy Tkachuk <andriy.tkachuk@seagate.com>
 * Original creation date: 26-Apr-2021
 */


/**
 * @addtogroup layout
 * @{
 */

#define M0_TRACE_SUBSYSTEM M0_TRACE_SUBSYS_LAYOUT
#include "lib/trace.h"

/**
 * Layout access plan structure.
 * Links all the plan plops and tracks the dependecies between them.
 * Created by m0_layout_plan_build() and destroyed by m0_layout_plan_fini().
 */
struct m0_layout_plan {
	/** Layout the plan belongs to. */
	struct m0_layout_instance *layout;
	/** All plan plops linked via ::pl_all_link. */
	struct m0_tlist            lp_plops;
};

M0_INTERNAL struct m0_layout_plan * m0_layout_plan_build(struct m0_op *op)
{

}

#undef M0_TRACE_SUBSYSTEM

/** @} end group layout */

/*
 *  Local variables:
 *  c-indentation-style: "K&R"
 *  c-basic-offset: 8
 *  tab-width: 8
 *  fill-column: 80
 *  scroll-step: 1
 *  End:
 */
