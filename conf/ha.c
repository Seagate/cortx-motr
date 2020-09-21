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
 * @addtogroup conf-ha
 *
 * @{
 */

#define M0_TRACE_SUBSYSTEM M0_TRACE_SUBSYS_CONF
#include "lib/trace.h"

#include "conf/ha.h"

#include "lib/memory.h"         /* M0_ALLOC_PTR */
#include "lib/time.h"           /* m0_time_now */
#include "lib/types.h"          /* PRIu64 */

#include "fid/fid.h"            /* FID_P */
#include "module/instance.h"    /* m0_get */
#include "ha/msg.h"             /* m0_ha_msg */
#include "ha/ha.h"              /* m0_ha_send */

M0_INTERNAL void
m0_conf_ha_process_event_post(struct m0_ha                  *ha,
                              struct m0_ha_link             *hl,
                              const struct m0_fid           *process_fid,
                              uint64_t                       pid,
                              enum m0_conf_ha_process_event  event,
                              enum m0_conf_ha_process_type   type)
{
	struct m0_ha_msg *msg;
	uint64_t          tag;

	M0_ENTRY("process_fid="FID_F" event=%d", FID_P(process_fid), event);
	M0_ALLOC_PTR(msg);
	if (msg == NULL) {
		M0_LOG(M0_ERROR, "can't allocate memory for msg");
		M0_LEAVE();
		return;
	}
	*msg = (struct m0_ha_msg){
		.hm_fid  = *process_fid,
		.hm_time = m0_time_now(),
		.hm_data = {
			.hed_type            = M0_HA_MSG_EVENT_PROCESS,
			.u.hed_event_process = {
				.chp_event = event,
				.chp_type  = type,
				.chp_pid   = pid,
			},
		},
	};
	m0_ha_send(ha, hl, msg, &tag);
	m0_free(msg);
	M0_LEAVE("tag=%"PRIu64, tag);
}

M0_INTERNAL void
m0_conf_ha_service_event_post(struct m0_ha                  *ha,
                              struct m0_ha_link             *hl,
                              const struct m0_fid           *source_process_fid,
                              const struct m0_fid           *source_service_fid,
                              const struct m0_fid           *service_fid,
                              uint64_t                       pid,
                              enum m0_conf_ha_service_event  event,
                              enum m0_conf_service_type      service_type)
{
	struct m0_ha_msg *msg;
	uint64_t          tag;

	M0_ENTRY("ha=%p hl=%p service_fid="FID_F" source_process_fid="FID_F" "
	         "source_service_fid="FID_F" event=%d",
	         ha, hl, FID_P(service_fid),
		 FID_P(source_process_fid), FID_P(source_service_fid), event);
	M0_LOG(M0_DEBUG, "service_type=%d", service_type);
	M0_ALLOC_PTR(msg);
	if (msg == NULL) {
		M0_LOG(M0_ERROR, "can't allocate memory for msg");
		M0_LEAVE();
		return;
	}
	*msg = (struct m0_ha_msg){
		.hm_fid            = *service_fid,
		.hm_source_process = *source_process_fid,
		.hm_source_service = *source_service_fid,
		.hm_time           =  m0_time_now(),
		.hm_data           = {
			.hed_type            = M0_HA_MSG_EVENT_SERVICE,
			.u.hed_event_service = {
				.chs_event = event,
				.chs_type  = service_type,
				.chs_pid   = pid,
			},
		},
	};
	m0_ha_send(ha, hl, msg, &tag);
	m0_free(msg);
	M0_LEAVE("tag=%"PRIu64, tag);
}

#undef M0_TRACE_SUBSYSTEM

/** @} end of conf-ha group */

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
