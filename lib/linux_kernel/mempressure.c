/* -*- C -*- */
/*
 * Copyright (c) 2020 Seagate Technology LLC and/or its Affiliates
 * 
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Affero General Public License as published
 * by the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 * 
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Affero General Public License for more details.
 * 
 * You should have received a copy of the GNU Affero General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 * 
 * For any questions about this software or licensing, please email opensource@seagate.com
 * or cortx-questions@seagate.com.
 *
 */

#define M0_TRACE_SUBSYSTEM M0_TRACE_SUBSYS_MEMORY
#if !defined(__KERNEL__)
#endif

#include "lib/trace.h"

#include "lib/mutex.h"
#include "lib/mempressure.h"
#include "lib/memory.h"
#include "module/instance.h"
#include "lib/string.h"
#include "config.h"     /* HAVE_MEMPRESSURE */

/**
 *  Get method for current event if exist?
 */
M0_INTERNAL enum m0_mempressure_level m0_mempressure_get(void)
{
	M0_LOG(M0_ERROR, "m0_mempressure_get not supported.");
	return M0_ERR(-ENOSYS);
}
M0_EXPORTED(m0_mempressure_get);

/**
 * subscribe interface for registering event_th.
 */
M0_INTERNAL int m0_mempressure_cb_add(struct m0_mempressure_cb *cb)
{
	M0_LOG(M0_ERROR, "m0_mempressure_cb_add not supported.");
	return M0_ERR(-ENOSYS);;
}
M0_EXPORTED(m0_mempressure_cb_add);

/**
 * unsubscribe interface for event_th.
 */
M0_INTERNAL void m0_mempressure_cb_del(struct m0_mempressure_cb *cb)
{
	M0_LOG(M0_ERROR, "m0_mempressure_cb_del not supported.");
}
M0_EXPORTED(m0_mempressure_cb_del);

/**
 * mod init method.
 *  initialize group lock.
 *  initialize event queue.
 *  create/run three level [ low, medium, critical ] non-blocking threads. 
 */
M0_INTERNAL int m0_mempressure_mod_init()
{
	return 0;
}
M0_EXPORTED(m0_mempressure_mod_init);


/**
 * finialize the mempressure mod.
 */
M0_INTERNAL void m0_mempressure_mod_fini()
{
}
M0_EXPORTED(m0_mempressure_mod_fini);

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
