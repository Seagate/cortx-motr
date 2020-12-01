/* -*- C -*- */
/*
 * Copyright (c) 2019-2020 Seagate Technology LLC and/or its Affiliates
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


#define M0_TRACE_SUBSYSTEM M0_TRACE_SUBSYS_M0D

#include "motr/iem.h"
#include "lib/trace.h"

// For documentation links, please refer to this file :
// doc/motr-design-doc-list.rst
// Slide 4
const char M0_MOTR_IEM_SOURCE_ID = 'S';

// Slide 12
const int M0_MOTR_IEM_COMPONENT_ID_MOTR = 2;

const char *m0_motr_iem_severity = "TAXEWNCIDB";

/**
 * Variable is not protected for concurrent modification.
 * In rare cases we could miss some IEM alerts but that is not serious because
 * we might miss counting an IEM but we would still have delivered the event.
 */
struct m0_iem_retrigger {
	m0_time_t ir_stamp;
	uint64_t  ir_niems; /** Reset after every interval. */
	uint64_t  ir_total; /** Reset on restart. */
};

static struct m0_iem_retrigger iem_re[M0_IEM_EVENT_NR];

enum {
      /** Time interval (seconds) to throttle messages. */
      INTERVAL  = 60,
      /** How many messages during INTERVAL. */
      THRESHOLD = 10
};

void m0_iem(const char* file, const char* function, int line,
	    const enum m0_motr_iem_severity sev_id,
	    const enum m0_motr_iem_module mod_id,
	    const enum m0_motr_iem_event evt_id,
	    const bool report_evt_count,
	    const char *msg, ...)
{
	char     description[512] = {0x0};
	va_list  aptr;

	M0_PRE(IS_IN_ARRAY(evt_id, iem_re));

	iem_re[evt_id].ir_total++;
	/* Simple throttling scheme. */
	if (m0_time_is_in_past(iem_re[evt_id].ir_stamp)) {
		iem_re[evt_id].ir_stamp = m0_time_add(m0_time_now(),
						      M0_MKTIME(INTERVAL, 0));
		iem_re[evt_id].ir_niems = 0;
	}

	if (msg != NULL && msg[0] != 0) {
		va_start(aptr, msg);
		vsnprintf(description, sizeof(description)-1, msg, aptr);
		va_end(aptr);
	}
	if (++iem_re[evt_id].ir_niems < THRESHOLD) {
		if (report_evt_count)
			m0_console_printf("IEC: %c%c%03x%03x%04x: "
					  "Event count: %"PRIx64", %s\n",
					  m0_motr_iem_severity[sev_id],
					  M0_MOTR_IEM_SOURCE_ID,
					  M0_MOTR_IEM_COMPONENT_ID_MOTR, mod_id,
					  evt_id, iem_re[evt_id].ir_total,
					  description);
		else
			m0_console_printf("IEC: %c%c%03x%03x%04x: %s\n",
					  m0_motr_iem_severity[sev_id],
					  M0_MOTR_IEM_SOURCE_ID,
					  M0_MOTR_IEM_COMPONENT_ID_MOTR,
					  mod_id, evt_id, description);

		m0_console_flush();
	}
	/* Do not throttle trace messages. */
	if (report_evt_count)
		M0_LOG(M0_INFO, "IEC: %c%c%3x%3x%4x: Event count %"PRIx64", %s",
		       m0_motr_iem_severity[sev_id],
		       M0_MOTR_IEM_SOURCE_ID, M0_MOTR_IEM_COMPONENT_ID_MOTR,
		       mod_id, evt_id, iem_re[evt_id].ir_total,
		       (const char *)description);
	else
		M0_LOG(M0_INFO, "IEC: %c%c%3x%3x%4x: %s",
		       m0_motr_iem_severity[sev_id],
		       M0_MOTR_IEM_SOURCE_ID, M0_MOTR_IEM_COMPONENT_ID_MOTR,
		       mod_id, evt_id, (const char *)description);

	M0_LOG(M0_INFO, "from %s:%s:%d",
	       (const char*)file, (const char*)function, line);
}

#undef M0_TRACE_SUBSYSTEM
