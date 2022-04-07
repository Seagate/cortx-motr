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

#include "dtm0/tx_desc.h"       /* m0_dtm0_tx_desc */


#include "fid/fid.h"            /* m0_fid */
#include "lib/mutex.h"          /* m0_mutex */

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
 *
 * @section m0_dtm0_remach interface
 *
 * - m0_dtm0_log_iter_init() - initializes log record iterator for a
 *   sdev participant. It iterates over all records that were in the log during
 *   last local process restart or during last remote process restart for the
 *   process that handles that sdev.
 * - m0_dtm0_log_iter_next() - gives next log record for the sdev participant.
 * - m0_dtm0_log_iter_fini() - finalises the iterator. It MUST be done for every
 *   call of m0_dtm0_log_iter_init().
 * - m0_dtm0_log_participant_restarted() - notifies the log that the participant
 *   has restarted. All iterators for the participant MUST be finalized at the
 *   time of the call. Any record that doesn't have P from the participant at
 *   the time of the call will be returned during the next iteration for the
 *   participant.
 *
 * @section pmach interface
 *
 * - m0_dtm0_log_p_get_local() - returns the next P message that becomes local.
 *   Returns M0_FID0 during m0_dtm0_log_stop() call. After M0_FID0 is returned
 *   new calls to the log MUST NOT be made.
 * - m0_dtm0_log_p_put() - records that P message was received for the sdev
 *   participant.
 *
 * @section pruner interface
 *
 * - m0_dtm0_log_p_get_none_left() - returns dtx0 id for the dtx which has all
 *   participants (except originator) reported P for the dtx0. Also returns all
 *   dtx0 which were cancelled.
 * - m0_dtm0_log_prune() - remove the REDO message about dtx0 from the log
 *
 * dtx0 interface, client & server
 *
 * - m0_dtm0_log_redo_add() - adds a REDO message and, optionally, P message, to
 *   the log.
 *
 * @section dtx0 interface, client only
 *
 * - m0_dtm0_log_redo_p_wait() - returns the number of P messages for the dtx
 *   and waits until either the number increases or m0_dtm0_log_redo_cancel() is
 *   called.
 * - m0_dtm0_log_redo_cancel() - notification that the client doesn't need the
 *   dtx anymore. Before the function returns the op
 * - m0_dtm0_log_redo_end() - notifies dtx0 that the operation dtx0 is a part of
 *   is complete. This function MUST be called for every m0_dtm0_log_redo_add().
 */

struct m0_be_op;
struct m0_be_tx;
struct m0_be_tx_credit;
struct m0_be_domain;

struct m0_dtm0_redo;
struct m0_dtx0_id;
struct dtm0_log_data;


/* TODO s/dlc_/dtlc_/g */
struct m0_dtm0_log_cfg {
	char                 dlc_seg0_suffix[0x100];
	struct m0_be_domain *dlc_be_domain;
	struct m0_be_seg    *dlc_seg;
	struct m0_fid        dlc_btree_fid;
};

struct m0_dtm0_log {
	struct m0_dtm0_log_cfg  dtl_cfg;
	struct dtm0_log_data   *dtl_data;
	struct m0_mutex         dtl_lock;

	struct m0_be_op        *dtl_op;
	struct m0_dtx0_id      *dtl_head;
};

M0_INTERNAL int m0_dtm0_log_open(struct m0_dtm0_log     *dol,
				 struct m0_dtm0_log_cfg *dol_cfg);

M0_INTERNAL void m0_dtm0_log_close(struct m0_dtm0_log *dol);

M0_INTERNAL int m0_dtm0_log_create(struct m0_dtm0_log     *dol,
                                   struct m0_dtm0_log_cfg *dol_cfg);

M0_INTERNAL void m0_dtm0_log_destroy(struct m0_dtm0_log *dol);


M0_INTERNAL int m0_dtm0_log_redo_add(struct m0_dtm0_log        *dol,
                                     struct m0_be_tx           *tx,
                                     const struct m0_dtm0_redo *redo,
                                     const struct m0_fid       *p_sdev_fid);
M0_INTERNAL void m0_dtm0_log_redo_add_credit(struct m0_dtm0_log        *dol,
                                             const struct m0_dtm0_redo *redo,
                                             struct m0_be_tx_credit    *accum);


M0_INTERNAL void m0_dtm0_log_prune(struct m0_dtm0_log *dol,
                                   struct m0_be_tx    *tx,
                                   struct m0_dtx0_id  *dtx0_id);
M0_INTERNAL void m0_dtm0_log_prune_credit(struct m0_dtm0_log     *dol,
                                          struct m0_be_tx_credit *accum);

M0_INTERNAL void m0_dtm0_log_p_get_none_left(struct m0_dtm0_log *dol,
					     struct m0_be_op    *op,
					     struct m0_dtx0_id  *dtx0_id);

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
