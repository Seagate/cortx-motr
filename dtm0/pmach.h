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

#ifndef __MOTR___DTM0_PMACH_H__
#define __MOTR___DTM0_PMACH_H__

/**
 * @defgroup dtm0
 *
 * @{
 */

/*
 * DTM0 Persistentcy machine (pmach)
 * ---------------------------------
 *
 *   Pmach is a component responsible for handling of P (PERSISTENT) messages.
 * P messages are produced and consumed by DTM0 log and DTM0 network components.
 * The machine serves as "converter" that turns incoming P messages produced
 * by DTM0 network into calls to the corresponding log API, and vice-versa --
 * it turns notifications received from the log into a call or a series of
 * calls to DTM0 network component.
 *   Pmach may group and buffer incoming Pmsgs. For example, a set of Pmsgs
 * from DTM0 log (set of participants lists) could be transposed into a set of
 * Pmsgs for network (set of vectors of dtx ids).
 *   Pmach uses internal FOMs to buffer Pmsgs. For example, when a local BE
 * transaction gets logged, the state transition may cause the FOM to post
 * a message into DTM0 network (transport). If DTM0 network queue is full,
 * then the FOM goes to still until a free buffer appears. Pmach cannot push
 * back DTM0 log (be transactions get logged without any restrictions) but
 * it may be "pressed" by another Pmach though DTM0 network.
 *   When a remote participant goes TRANSIENT, Pmach gets a notification from
 * the HA. In this case, the machine removes any pending P messages that would
 * be sent to the participant if it was not TRANSIENT. The same principle
 * applies to FAILED (permanent failure) notification.
 *
 *   Interaction with other DTM0 components (net, HA, log):
 *
 *   +----+                  +--------+              +------+
 *   | HA | ----- F/T -----> | Pmach  | <-- Pmsg --> | Net  |
 *   +----+                  +--------+              +------+
 *                              /|\  |
 *   +-----+                     |  [tid]
 *   | Log | -- [tx_desc] -------+   |
 *   +-----+ <-----------------------+
 */


struct m0_dtm0_pmach {
};

struct m0_dtm0_pmach_cfg {
};

M0_INTERNAL int m0_dtm0_pmach_init(struct m0_dtm0_pmach     *drm,
                                   struct m0_dtm0_pmach_cfg *drm_cfg);
M0_INTERNAL void m0_dtm0_pmach_fini(struct m0_dtm0_pmach  *drm);
M0_INTERNAL void m0_dtm0_pmach_start(struct m0_dtm0_pmach *drm);
M0_INTERNAL void m0_dtm0_pmach_stop(struct m0_dtm0_pmach  *drm);

/** @} end of dtm0 group */
#endif /* __MOTR___DTM0_PMACH_H__ */

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
