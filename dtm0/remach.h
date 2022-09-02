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

#pragma once

#ifndef __MOTR___DTM0_REMACH_H__
#define __MOTR___DTM0_REMACH_H__

/**
 * @defgroup dtm0
 *
 * @{
 */

/*
 * DTM0 recovery machine (remach) overview
 * ---------------------------------------
 *
 *   Recovery machine is a component responsible for handling of REDO messages
 * in case of failures reported by DTM0 HA component. REDO messages are read
 * from DTM0 log and applied on the recepient side to the user service (for
 * example, CAS) and, though the user service, to the local repceient's log.
 * There are other messages that contol recovery process: End-of-Log (EOL)
 * and PERSISTENT (P). EOL is sent when the sender reached the end of its
 * DTM0 log. P message usually accompanies a REDO message when the REDO message
 * is formed by a persistent log record.
 *   Recovery machine reacts to TRANSIENT, FAILED, ONLINE and RECOVERING
 * notifications sent from the HA subsystem. When a participant enters the
 * T state, it causes the machine to halt any interaction with it.
 * The F state causes eviction of the failed participant. The R state
 * causes an attempt to recover the participant after transient failure.
 * The O state indicates that no recovery procedures required for the
 * participant.
 *
 *   Interactions with other components:
 *
 *   +--------+                    +----------+
 *   | Remach | --- tx_desc+op --> | User svc | -------+
 *   |        |                    | (CAS/DIX)|     record
 *   |        |                    +----------+       \|/
 *   |        |                                     +-----+
 *   |        | <------ record, P ------------------| Log |
 *   |        |                                     +-----+
 *   |        |                      +-----+
 *   |        | -- REDO,P,EOL msg -> | Net |
 *   |        | <-- REDO,EOL msg --- |     |
 *   |        |                      +-----+                +----+
 *   |        | <------------------------------- F/O/R/T -> | HA |
 *   +--------+                                             +----+
 */

struct m0_dtx0_payload;

struct m0_dtm0_remach {
};

struct m0_dtm0_remach_cfg {
	void (*dtrc_blob_handler)(struct m0_dtx0_payload *payload,
	                          void                   *datum);
	void  *dtrc_blob_handler_datum;
};

M0_INTERNAL int m0_dtm0_remach_init(struct m0_dtm0_remach     *drm,
				    struct m0_dtm0_remach_cfg *drm_cfg);
M0_INTERNAL void m0_dtm0_remach_fini(struct m0_dtm0_remach  *drm);
M0_INTERNAL void m0_dtm0_remach_start(struct m0_dtm0_remach *drm);
M0_INTERNAL void m0_dtm0_remach_stop(struct m0_dtm0_remach  *drm);

/** @} end of dtm0 group */
#endif /* __MOTR___DTM0_REMACH_H__ */

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
