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


#include "lib/assert.h"
#include "lib/misc.h"
#include "lib/buf.h"
#include "fop/fop.h"

#include "fop/fom_interpose.h"

enum { INTERPOSE_CONT = M0_FSO_NR + 1 };

static int thrall_finish(struct m0_fom *fom, struct m0_fom_interpose *proxy,
			 int result);

static const struct m0_fom_interpose_ops thrall_ops = {
	.io_post = {
		[0 ... ARRAY_SIZE(thrall_ops.io_post) - 1] = &thrall_finish
	}
};

static int interpose_tick(struct m0_fom *fom)
{
	struct m0_fom_interpose *proxy = M0_AMB(proxy, fom->fo_ops, fi_shim);
	int                      phase = m0_fom_phase(fom);
	int                      result;

	M0_PRE(IS_IN_ARRAY(phase, proxy->fi_ops->io_pre));
	M0_PRE(IS_IN_ARRAY(phase, proxy->fi_ops->io_post));
	if (proxy->fi_ops->io_pre[phase] != NULL) {
		result = (proxy->fi_ops->io_pre[phase])(fom, proxy);
		if (result != INTERPOSE_CONT)
			return result;
	}
	/*
	 * Perhaps restore original fom->fo_ops around this call?
	 */
	result = proxy->fi_orig->fo_tick(fom);
	M0_ASSERT(result != INTERPOSE_CONT);
	if (proxy->fi_ops->io_post[phase] != NULL)
		result = (proxy->fi_ops->io_post[phase])(fom, proxy, result);
	M0_POST(M0_IN(result, (M0_FSO_WAIT, M0_FSO_AGAIN)));
	return result;
}

static int thrall_finish(struct m0_fom *fom, struct m0_fom_interpose *proxy,
			 int result)
{
	struct m0_fom_thralldom *thrall = M0_AMB(thrall, proxy, ft_fief);
	int                      phase  = m0_fom_phase(fom);

	if (phase == M0_FOM_PHASE_FINISH) {
		/* Mors liberat. */
		m0_fom_interpose_leave(fom, proxy);
		if (thrall->ft_end != NULL)
			thrall->ft_end(thrall, fom);
		m0_fom_wakeup(thrall->ft_leader);
	}
	return result;
}

M0_INTERNAL void m0_fom_interpose_enter(struct m0_fom           *fom,
					struct m0_fom_interpose *proxy)
{
	M0_PRE(proxy->fi_ops != NULL);
	/*
	 * Activate the interposition. Substitute shim fom operation vector for
	 * the original one after saving the original in proxy->fi_orig.
	 */
	proxy->fi_orig =  fom->fo_ops;
	proxy->fi_shim = *fom->fo_ops;
	proxy->fi_shim.fo_tick = &interpose_tick;
	fom->fo_ops = &proxy->fi_shim;
}

M0_INTERNAL void m0_fom_interpose_leave(struct m0_fom           *fom,
					struct m0_fom_interpose *proxy)
{
	M0_PRE(fom->fo_ops == &proxy->fi_shim);
	fom->fo_ops = proxy->fi_orig;
}

M0_INTERNAL void m0_fom_enthrall(struct m0_fom *leader, struct m0_fom *serf,
				 struct m0_fom_thralldom *thrall,
				 void (*end)(struct m0_fom_thralldom *thrall,
					     struct m0_fom           *serf))
{
	thrall->ft_leader      = leader;
	thrall->ft_fief.fi_ops = &thrall_ops;
	thrall->ft_end         = end;
	m0_fom_interpose_enter(serf, &thrall->ft_fief);
}

/*
 *  Local variables:
 *  c-indentation-style: "K&R"
 *  c-basic-offset: 8
 *  tab-width: 8
 *  fill-column: 80
 *  scroll-step: 1
 *  End:
 */
