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

#ifndef __MOTR___DTM0_LOG_H__
#define __MOTR___DTM0_LOG_H__

#include "dtm0/tx_desc.h" /* m0_dtm0_tx_desc */
#include "lib/buf.h"       /* m0_buf */


/**
 * @defgroup dtm0
 *
 * @{
 */

/*
 * DTM0 log overview
 * -----------------
 *
 *   DTM0 log is a persistent or volatile storage for log records.
 * Functionally, log provides a way to add records and receive notifications
 * about their persistence. Logically, DTM0 log is formed as a threaded
 * B tree on the server side or as a threaded hashtable on the client side
 * (TODO). Each "thread" of such a structure forms a list. Server-side log
 * has N+K "threads" (lists). One list for originator/pruner, and N+K-1 lists
 * for the remote participants of a dtx.
 *   DTM0 log inputs are REDO messages and P (PERSISTENT) messages. When
 * a REDO message is inserted into the tree/htable, it is also gets linked
 * to the originator's list and to each remote participant's list. When
 * a P message is received, then the log entry gets removed from the
 * corresponding participant's list. Once the record becomes STABLE,
 * it is removed from the originator's list and inserted into the log
 * pruner list (re-using the link).
 *   DTM0 log has no FOMs. It reacts to the incoming events re-using
 * their sm groups. The log has one single lock that protects container
 * from modification. Individual log records has no lock protection.
 */

struct m0_be_op;
struct m0_be_tx;
struct m0_be_tx_credit;

struct m0_dtm0_log_cfg {
};

struct m0_dtm0_log_create_cfg {
};

struct m0_dtm0_log {
};

struct m0_dtm0_log_record {
	struct m0_dtm0_tx_desc lr_desc;
	struct m0_buf          lr_data;
};


M0_INTERNAL int m0_dtm0_log_init(struct m0_dtm0_log     *dol,
				 struct m0_dtm0_log_cfg *dol_cfg);

M0_INTERNAL void m0_dtm0_log_fini(struct m0_dtm0_log *dol);

M0_INTERNAL int m0_dtm0_log__create(struct m0_dtm0_log            *dol,
				    struct m0_dtm0_log_create_cfg *dlc_cfg);

M0_INTERNAL void m0_dtm0_log_destroy(struct m0_dtm0_log *dol);

M0_INTERNAL void m0_dtm0_log_update_credit(struct m0_dtm0_log        *dol,
					   struct m0_dtm0_log_record *rec,
					   struct m0_be_tx_credit    *accum);

M0_INTERNAL int m0_dtm0_log_update(struct m0_dtm0_log              *dol,
				   struct m0_be_tx                 *tx,
				   struct m0_be_op                 *op,
				   const struct m0_dtm0_log_record *rec,
				   bool                             is_redo);


/** @} end of dtm0 group */
#endif /* __MOTR___DTM0_LOG_H__ */

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
