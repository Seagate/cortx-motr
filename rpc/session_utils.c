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


#ifndef __KERNEL__
#include <sys/stat.h>    /* S_ISDIR */
#endif

#define M0_TRACE_SUBSYSTEM M0_TRACE_SUBSYS_RPC
#include "lib/trace.h"
#include "lib/errno.h"
#include "lib/memory.h"
#include "lib/misc.h"
#include "lib/arith.h"
#include "lib/bitstring.h"
#include "lib/finject.h"       /* M0_FI_ENABLED */
#include "lib/uuid.h"
#include "lib/hash.h"          /* m0_hash() */
#include "fop/fop.h"
#include "reqh/reqh.h"
#include "rpc/rpc_internal.h"

/**
   @addtogroup rpc_session

   @{
 */

M0_INTERNAL int m0_rpc_session_module_init(void)
{
        return m0_rpc_session_fop_init();
}

M0_INTERNAL void m0_rpc_session_module_fini(void)
{
        m0_rpc_session_fop_fini();
}

M0_INTERNAL int m0_rpc__post_locked(struct m0_rpc_item *item);

/**
   Initialises rpc item and posts it to rpc-layer
 */
M0_INTERNAL int m0_rpc__fop_post(struct m0_fop *fop,
				 struct m0_rpc_session *session,
				 const struct m0_rpc_item_ops *ops,
				 m0_time_t abs_timeout)
{
	struct m0_rpc_item *item;
	m0_time_t           now = m0_time_now();

	M0_ENTRY("fop: %p, session: %p, item %p", fop, session, &fop->f_item);

	item                = &fop->f_item;
	item->ri_session    = session;
	item->ri_prio       = M0_RPC_ITEM_PRIO_MAX;
	item->ri_deadline   = 0;
	item->ri_ops        = ops;
	item->ri_rmachine   = session_machine(session);

	if (M0_FI_ENABLED("fake_error"))
		return M0_ERR(-EINVAL);

	if (M0_FI_ENABLED("do_nothing"))
		return 0;

	if (abs_timeout != M0_TIME_NEVER) {
		item->ri_nr_sent_max = 0;
		if (abs_timeout > now)
			item->ri_nr_sent_max = m0_time_sub(abs_timeout, now) /
						item->ri_resend_interval;
		if (item->ri_nr_sent_max == 0)
			item->ri_nr_sent_max = 1;
	}
	return M0_RC(m0_rpc__post_locked(item));
}

M0_INTERNAL uint64_t m0_rpc_id_generate(const struct m0_fid *uniq_fid)
{
	static struct m0_atomic64 cnt;
	uint64_t                  id;
	uint64_t                  millisec;

	do {
		m0_atomic64_inc(&cnt);
		millisec = m0_time_nanoseconds(m0_time_now()) * 1000000;
		id = (millisec << 10) | (m0_atomic64_get(&cnt) & 0x3FF);
		id = m0_hash(m0_fid_hash(uniq_fid) + id);
	} while (id == 0 || id == UINT64_MAX);

	return id;
}

M0_INTERNAL int m0_rpc_item_dispatch(struct m0_rpc_item *item)
{
	int                                rc;
	const struct m0_rpc_item_type_ops *itops = item->ri_type->rit_ops;

	M0_ENTRY("item %p[%u], xid=%"PRIu64,
		 item, item->ri_type->rit_opcode, item->ri_header.osr_xid);

	rc = m0_ha_epoch_check(item);
	if (rc != 0)
		return M0_RC(0);

	if (itops != NULL && itops->rito_deliver != NULL)
		rc = itops->rito_deliver(item->ri_rmachine, item);
	else
		/**
		 * @todo this assumes that the item is a fop.
		 */
		rc = m0_reqh_fop_handle(item->ri_rmachine->rm_reqh,
					m0_rpc_item_to_fop(item));
	return M0_RC(rc);
}

#undef M0_TRACE_SUBSYSTEM
/** @} */
