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

#ifndef __MOTR___DTM0_DTX0_H__
#define __MOTR___DTM0_DTX0_H__

/**
 * @defgroup dtm0
 *
 * @{
 */

/*
 * DTX0 overview
 * -------------
 *   DTX0 is a component that serves as the public API of DTM0.
 * It is used by other "external" Motr components (CAS/DIX)
 * to add DTM0 semantics to their operations.
 *   DTX0 receives REDO message (for example, a serialised CAS request)
 * as incoming data, and notifies when a distributed
 * transaction becomes STABLE (client side). Everything else is hidden
 * from the user.
 *
 * Concurrency
 * -----------
 *    On the client side a dtx belongs to the sm group of the Motr
 *  client operation that uses this dtx. The relation between sm group
 *  of dtx0 and the context of DTM0 log is not defined yet (TODO).
 *
 * Lifetime
 * --------
 *    The lifetime of dtx is related to the lifetime of the corresponding
 *  user operation. dtx cannot be destroyed until it reached STABLE state.
 *
 * States
 * ------
 *    A dtx has only one state transition exposed to the user: transition
 *  to the STABLE state.
 *
 * Failures
 * --------
 *
 *    When one of the participants of a dtx becomes T (TRANSIENT) then
 *  dtx receives this information from the user. dtx itself does not subscribe
 *  to HA events. Once a dtx gets enough P (PERSISTENT) messages, it moves
 *  to STABLE state (enough == as per the durability requirements).
 *
 * Integration
 * -----------
 *
 *  +--------+               +------+                     +----------+
 *  | Client | --- REDO ---> | DTX0 | --- log record ---> | DTM0 log |
 *  | (DIX)  | <--- stable - |      | <--- persistent --- |          |
 *  +--------+               +------+                     +----------+
 *
 *  +--------+               +------+                     +----------+
 *  | Server | --- REDO ---> | DTX0 | --- log record ---> | DTM0 log |
 *  | (CAS)  | --- be_tx --> |      | --- be_tx      ---> |          |
 *  +--------+               +------+                     +----------+
 *
 */

struct m0_dtm0_domain;
struct m0_dtm0_redo;
struct m0_be_tx;
struct m0_be_tx_credit;
struct m0_fid;

M0_INTERNAL void m0_dtx0_redo_add_credit(struct m0_dtm0_domain  *dod,
					 struct m0_dtm0_redo    *redo,
					 struct m0_be_tx_credit *accum);

M0_INTERNAL int m0_dtx0_redo_add(struct m0_dtm0_domain *dod,
				 struct m0_be_tx       *tx,
				 struct m0_dtm0_redo   *redo,
				 const struct m0_fid   *sdev);

/** @} end of dtm0 group */
#endif /* __MOTR___DTM0_DTX0_H__ */

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
