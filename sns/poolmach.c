/*
 * Copyright (c) 2011-2020 Seagate Technology LLC and/or its Affiliates
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


#include "sns/repair.h"

static const struct m0_persistent_sm_ops;

/**
   Register pool state machine as a persistent state machine with the given
   transaction manager instance. This allows poolmachine to update its persisten
   state transactionally and to be call-back on node restart.
 */
M0_INTERNAL int m0_poolmach_init(struct m0_poolmach *pm, struct m0_dtm *dtm)
{
	m0_persistent_sm_register(&pm->pm_mach, dtm,
				  &poolmach_persistent_sm_ops);
}

M0_INTERNAL void m0_poolmach_fini(struct m0_poolmach *pm)
{
	m0_persistent_sm_unregister(&pm->pm_mach);
}

M0_INTERNAL int m0_poolmach_device_join(struct m0_poolmach *pm,
					struct m0_pooldev *dev)
{
}

M0_INTERNAL int m0_poolmach_device_leave(struct m0_poolmach *pm,
					 struct m0_pooldev *dev)
{
}

M0_INTERNAL int m0_poolmach_node_join(struct m0_poolmach *pm,
				      struct m0_poolnode *node)
{
}

M0_INTERNAL int m0_poolmach_node_leave(struct m0_poolmach *pm,
				       struct m0_poolnode *node)
{
}

/**
   Pool machine recovery call-back.

   This function is installed as m0_persistent_sm_ops::pso_recover method of a
   persistent state machine. It is called when a node hosting a pool machine
   replica reboots and starts local recovery.
 */
static int poolmach_recover(struct m0_persistent_sm *pmach)
{
	struct m0_poolmach *pm;

	pm = container_of(pmach, struct m0_poolmach, pm_mach);
}

static const struct m0_persistent_sm_ops poolmach_persistent_sm_ops = {
	.pso_recover = poolmach_recover
};

/*
 *  Local variables:
 *  c-indentation-style: "K&R"
 *  c-basic-offset: 8
 *  tab-width: 8
 *  fill-column: 80
 *  scroll-step: 1
 *  End:
 */
