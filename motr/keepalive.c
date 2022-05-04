/* -*- C -*- */
/*
 * Copyright (c) 2016-2020 Seagate Technology LLC and/or its Affiliates
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


/**
 * @addtogroup motr-keepalive
 *
 * TODO s/M0_AMB/bob_of/g
 *
 * @{
 */

#define M0_TRACE_SUBSYSTEM M0_TRACE_SUBSYS_HA
#include "lib/trace.h"

#include "motr/keepalive.h"

#include "lib/memory.h" /* M0_ALLOC_PTR */
#include "lib/time.h"   /* m0_time_now */
#include "ha/msg.h"     /* m0_ha_msg */
#include "ha/ha.h"      /* m0_ha_send */

static void ha_keepalive_msg_received_cb(struct m0_ha_handler *hh,
                                         struct m0_ha         *ha,
                                         struct m0_ha_link    *hl,
                                         struct m0_ha_msg     *msg,
                                         uint64_t              tag,
                                         void                 *data)
{
	struct m0_ha_keepalive_handler *ka;
	struct m0_ha_msg               *rep;
	uint64_t                        tag_rep;

	ka = M0_AMB(ka, hh, kah_handler);
	M0_ASSERT(ka == data);

	if (msg->hm_data.hed_type != M0_HA_MSG_KEEPALIVE_REQ)
		return;
	M0_ALLOC_PTR(rep);
	if (rep == NULL)
		return;
	*rep = (struct m0_ha_msg){
		.hm_time = m0_time_now(),
		.hm_data = {
			.hed_type = M0_HA_MSG_KEEPALIVE_REP,
			.u.hed_keepalive_rep = {
				.kap_id =
				    msg->hm_data.u.hed_keepalive_req.kaq_id,
				.kap_counter =
				    m0_atomic64_add_return(&ka->kah_counter, 1),
			},
		},
	};
	m0_ha_send(ha, hl, rep, &tag_rep);
	M0_LOG(M0_DEBUG, "kap_id="U128X_F" kap_counter=%" PRIu64 " tag=%"PRIu64,
	       U128_P(&rep->hm_data.u.hed_keepalive_rep.kap_id),
	       rep->hm_data.u.hed_keepalive_rep.kap_counter, tag_rep);
	m0_free(rep);
}

M0_INTERNAL int
m0_ha_keepalive_handler_init(struct m0_ha_keepalive_handler *ka,
                             struct m0_ha_dispatcher        *hd)
{
	M0_PRE(M0_IS0(ka));

	ka->kah_dispatcher = hd;
	ka->kah_handler = (struct m0_ha_handler){
		.hh_data            = ka,
		.hh_msg_received_cb = &ha_keepalive_msg_received_cb,
	};
	m0_atomic64_set(&ka->kah_counter, 0);
	m0_ha_dispatcher_attach(ka->kah_dispatcher, &ka->kah_handler);
	return 0;
}

M0_INTERNAL void
m0_ha_keepalive_handler_fini(struct m0_ha_keepalive_handler *ka)
{
	m0_ha_dispatcher_detach(ka->kah_dispatcher, &ka->kah_handler);
}

#undef M0_TRACE_SUBSYSTEM

/** @} end of motr-keepalive group */

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
