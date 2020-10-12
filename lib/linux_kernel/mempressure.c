/* -*- C -*- */
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

#define M0_TRACE_SUBSYSTEM M0_TRACE_SUBSYS_LIB
#include "lib/trace.h"
#include "lib/mempressure.h"

/**
 *  Get method for current event if exist?
 */
M0_INTERNAL enum m0_mempressure_level m0_arch_mempressure_get(void)
{
	M0_LOG(M0_ERROR, "m0_mempressure_get not supported.");
	return M0_ERR(-ENOSYS);
}

/**
 * subscribe interface for registering event_th.
 */
M0_INTERNAL int m0_arch_mempressure_cb_add(struct m0_mempressure_cb *cb)
{
	M0_LOG(M0_ERROR, "m0_mempressure_cb_add not supported.");
	return M0_ERR(-ENOSYS);;
}

/**
 * unsubscribe interface for event_th.
 */
M0_INTERNAL void m0_arch_mempressure_cb_del(struct m0_mempressure_cb *cb)
{
	M0_LOG(M0_ERROR, "m0_mempressure_cb_del not supported.");
}

/**
 * mod init method.
 *  initialize group lock.
 *  initialize event queue.
 *  create/run three level [ low, medium, critical ] non-blocking threads. 
 */
M0_INTERNAL int m0_arch_mempressure_mod_init(void)
{
	return 0;
}

/**
 * finialize the mempressure mod.
 */
M0_INTERNAL void m0_arch_mempressure_mod_fini(void)
{
}

/** @} end of mempressure */
#undef M0_TRACE_SUBSYSTEM

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
