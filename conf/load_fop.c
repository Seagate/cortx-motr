/* -*- C -*- */
/*
 * Copyright (c) 2015-2020 Seagate Technology LLC and/or its Affiliates
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


#define M0_TRACE_SUBSYSTEM M0_TRACE_SUBSYS_CONF

#include "lib/trace.h"
#include "lib/errno.h"
#include "lib/finject.h"   /* M0_FI_ENABLED */
#include "lib/memory.h"
#include "conf/load_fop.h"
#include "net/net.h"       /* m0_net_domain_get_max_buffer_segment_size */

/* tlists and tlist APIs referred from rpc layer. */
M0_TL_DESCR_DECLARE(rpcbulk, M0_EXTERN);

M0_INTERNAL bool m0_is_conf_load_fop(const struct m0_fop *fop)
{
	M0_PRE(fop != NULL);
	return fop->f_type == &m0_fop_conf_load_fopt;
}

M0_INTERNAL bool m0_is_conf_load_fop_rep(const struct m0_fop *fop)
{
	M0_PRE(fop != NULL);
	return fop->f_type == &m0_fop_conf_load_rep_fopt;
}

M0_INTERNAL struct m0_fop_conf_load *m0_conf_fop_to_load_fop(
						const struct m0_fop *fop)
{
	M0_PRE(fop != NULL);
	M0_PRE(m0_is_conf_load_fop(fop));

	return m0_fop_data(fop);
}

M0_INTERNAL struct m0_fop_conf_load_rep *m0_conf_fop_to_load_fop_rep(
						const struct m0_fop *fop)
{
	M0_PRE(fop != NULL);
	M0_PRE(m0_is_conf_load_fop_rep(fop));

	return m0_fop_data(fop);
}

M0_INTERNAL m0_bcount_t m0_conf_segment_size(struct m0_fop *fop)
{
	struct m0_net_domain *dom;

	if (M0_FI_ENABLED("const_size"))
		return 4096;
	dom = m0_fop_domain_get(fop);
	return m0_net_domain_get_max_buffer_segment_size(dom);
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
