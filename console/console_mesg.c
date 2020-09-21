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


#include <stdio.h>	/* fprintf */

#include "lib/errno.h" /* ENOTSUP */
#include "lib/misc.h"  /* M0_BITS */

#include "console/console_mesg.h"
#include "console/console_it.h" /* m0_cons_fop_fields_show */

#define M0_TRACE_SUBSYSTEM M0_TRACE_SUBSYS_CONSOLE
#include "lib/trace.h"

M0_INTERNAL void m0_cons_fop_name_print(const struct m0_fop_type *ftype)
{
	printf("%.2d %s\n", ftype->ft_rpc_item_type.rit_opcode, ftype->ft_name);
}

M0_INTERNAL int m0_cons_fop_send(struct m0_fop *fop,
				 struct m0_rpc_session *session,
				 m0_time_t resend_interval,
				 uint64_t nr_sent_max)
{
	struct m0_rpc_item *item;
	int		    rc;

	M0_PRE(fop != NULL && session != NULL);

	item = &fop->f_item;
	item->ri_deadline        = 0;
	item->ri_prio            = M0_RPC_ITEM_PRIO_MID;
	item->ri_session         = session;
	item->ri_nr_sent_max     = nr_sent_max;
	item->ri_resend_interval = resend_interval;

        rc = m0_rpc_post(item);
	if (rc == 0) {
		rc = m0_rpc_item_wait_for_reply(item, M0_TIME_NEVER);
		if (rc != 0)
			fprintf(stderr, "Error while waiting for reply: %d\n",
				rc);
	} else {
		fprintf(stderr, "m0_rpc_post failed!\n");
	}
	return M0_RC(rc);
}

M0_INTERNAL int m0_cons_fop_show(struct m0_fop_type *fopt)
{
	struct m0_fop         *fop;
	int                    rc;
	struct m0_rpc_machine  mach;

	m0_sm_group_init(&mach.rm_sm_grp);
	fop = m0_fop_alloc(fopt, NULL, &mach);
	if (fop == NULL) {
		fprintf(stderr, "FOP allocation failed\n");
		return M0_ERR(-ENOMEM);
	}

	rc = m0_cons_fop_fields_show(fop);

	m0_fop_put_lock(fop);
	m0_sm_group_fini(&mach.rm_sm_grp);
	return M0_RC(rc);
}

M0_INTERNAL void m0_cons_fop_list_show(void)
{
        struct m0_fop_type *ftype;

	fprintf(stdout, "List of FOP's: \n");
	ftype = NULL;
	while ((ftype = m0_fop_type_next(ftype)) != NULL)
		m0_cons_fop_name_print(ftype);
}

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
