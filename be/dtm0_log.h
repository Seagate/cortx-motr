/* -*- C -*- */
/*
 * Copyright (c) 2021 Seagate Technology LLC and/or its Affiliates
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
#ifndef __MOTR_BE_DTM0_LOG_H__
#define __MOTR_BE_DTM0_LOG_H__

#include "be/list.h"            /* m0_be_list */
#include "dtm0/tx_desc.h"       /* m0_dtm0_tx_desc */
#include "fid/fid.h"            /* m0_fid */
#include "lib/buf.h"            /* m0_buf */
#include "dtm0/dtx.h"           /* struct m0_dtm0_dtx */

struct m0_be_tx;
struct m0_be_tx_credit;
struct m0_dtm0_clk_src;

/**
 * @page dtm0 log implementation
 *
 * @section Overview
 * DTM0 log module will be working on incoming message request, the goal of
 * this module is to track journaling of incoming message either on persistent
 * or volatile memory based on whether logging is happening on participant or
 * on originator, so that in the phase failure consistency of failed
 * participant can be restored by iterating over logged journal(DTM0 log
 * record) to decide which of the logged record needs to be sent as a redo
 * request.
 *
 * A distributed transaction(dtx) has a group of states of each individual
 * participant of the distributed transaction(dtx) (note: originator is not a
 * part of this group). Each entry of such a group may have many different
 * states, but this set of states must include the following states:
 *
 * 1) InProgress - the node (owner of the log) has a request.
 * 2) Executed - one or more operations have been executed in volatile
 *               memory of the participant, the result of such execution
 *               is known and returned to the dtx user.
 * 3) Persistent - locally persisted in transactional context.
 * 4) Stable - sufficient number of sent operations have been “persisted”
 *             on the remote end which guarantees survivial of persistent
 *             failures.
 * 5) DONE - all sent operations have been “persisted” on sufficient
 *           number of non-failed participants.
 *
 * Every participant maintains the journal record(DTM0 Log record) that
 * corresponds to each distributed transaction(dtx) and it can be described by
 * "struct m0_dtm0_log_rec" which will be stored in volatile/persistent
 * storage. Basically this log record will maintain txr id, group of state of
 * each individual participant and payload.
 *
 * When a distributed transaction(dtx) is executed by participant, participant
 * modifies its state in group of state as M0_DTPS_EXECUTED and will add/modify
 * DTM0 log record, for rest of the participant it will keep the state as it is
 * in distributed transaction(dtx).
 *
 * When a distributed transaction(dtx) becomes persistent on participant,
 * participant will modify its state in group of state as M0_DTPS_PERSISTENT
 * and will modify DTM0 log record. For rest of the participants it will keep
 * the state as it is in DTM0 log record.
 *
 * Originator also maintains the same distributed transaction(dtx) record in
 * volatile memory. Originator is expecting to get replies from participant
 * when distributed transaction(dtx) on participant is M0_DTPS_EXECUTED or
 * M0_DTPS_PERSISTENT. On originator the state of the distributed transaction
 * (dtx) can be M0_DTPS_INPROGRESS to M0_DTPS_PERSISTENT for each participant.
 *
 * During recovery operation of any of the participant, rest of the participant
 * and originator will iterate over the logged journal and extract the state
 * of each transation for participant under recovery from logged information
 * and will send redo request for those distributed transaction(dtx) which  are
 * not M0_DTPS_PERSISTENT on participant being recovered.
 *
 * Upon receiving redo request participant under recovery will log the state
 * of same distributed transaction(dtx) of remote participant in persistent
 * store.
 *
 * @section Usecases
 *
 * 1. There is a dedicated function m0_be_dtm0_log_create which will create
 *    m0_be_dtm0_log during mkfs procedure.
 *
 * 2. When a distributed transaction(dtx) is successfully executed on a participant,
 *    the participant will add a new log record to DTM0 log. The state of the
 *    participant will be set to M0_DTPS_EXECUTED in the record. The states of the
 *    other participants will be set to M0_DTPS_INPROGRESS in the record.
 *
 * 3. When a distributed transaction(dtx) becomes persistent on a particular
 *    participant, the state of the distributed transaction(dtx) for this
 *    participant will be updated to M0_DTPS_PERSISTENT.
 *
 * 4. When distributed transaction(dtx) becomes persistent on a participant,
 *    this participant sends a persistent notice to rest of the participants
 *    and to the originator indicating that the given distributed transaction(dtx)
 *    is now persistent on its store. Upon receiving a persistent notice, the
 *    recipient will update the state of the distributed transaction(dtx) for the
 *    sender as M0_DTPS_PERSISTENT in its own DTM0 log.
 *
 *    There is one special case where a participant gets a persistent notice from
 *    another participant but the corresponding request and reply haven't been
 *    received/processed yet. In this case, the DTM service should add a special
 *    log record: a log record with empty payload (NULL). The users of DTM0 log
 *    should be ready to encounter such a log entry, and handle it appropriately.
 */

/**
 * @defgroup DTM0Internals DTM0 implementation Internals
 * @ingroup DTM0
 *
 * @section This section describes the dtm0 log related enumerations and structures.
 *
 * @{
 */

/**
 * @b  m0_be_dtm0_log_credit_op enum
 *
 * DTM0 persistent log is implemented on top of be modules and uses be_list for
 * storing the log records. m0_be_dtm0_log_credit_op is an enumeration of the
 * operation codes that can be specified to obtain credits for performing the
 * corresponding operation related to dtm0_log.
 */
enum m0_be_dtm0_log_credit_op {
	M0_DTML_CREATE,         /**< m0_be_dtm0_log_create() */
	M0_DTML_SENT,           /**< m0_be_dtm0_log_update() */
	M0_DTML_EXECUTED,       /**< m0_be_dtm0_log_update() */
	M0_DTML_PERSISTENT,     /**< m0_be_dtm0_log_update() */
	M0_DTML_PRUNE,          /**< m0_be_dtm0_log_prune()  */
	M0_DTML_REDO            /**< m0_be_dtm0_log_update() */
};

/**
 * @b  m0_be_dtm0_log_rec structure
 *
 * A DTM0 log record is represented by m0_dtm0_log_rec structure. The important
 * fields in this structure are:
 * - dlr_dtx: This stores dtx information related to dtm0 client.
 * - dlr_txd: This stores the states of the participants.
 * - dlr_payload: This stores the original request.
 */

struct m0_dtm0_log_rec {
	struct m0_dtm0_dtx     dlr_dtx;
	struct m0_dtm0_tx_desc dlr_txd;
	uint64_t               dlr_magic;
	union  {
		struct m0_be_list_link dlr_link;  /*
						   * used when this record is
						   * stored within a persistent
						   * log that is going to stored
						   * in BE.
						   */
		struct m0_tlink        dlr_tlink; /*
						   * used when this record is
						   * stored within a volatile
						   * log that is going to stored
						   * in memory.
						   */
	} u;
	struct m0_buf          dlr_payload;
};

/**
 * @b  m0_be_dtm0_log_rec structure
 *
 * A DTM0 log is represented by m0_be_dtm0_log structure. The important
 * fields in this structure are:
 * - dl_is_persistent: This is a flag to distinguish between a volatile
 * (client-side) log and a persistent (server-side) log.
 * - dl_cs: A pointer to the type of clock used to generate the timestamps
 * for the log records.
 */

struct m0_be_dtm0_log {
	/** Indicates if the structure is a persistent or volatile */
	bool                       dl_is_persistent;
	/** dl_lock protects access to the dl_list/dl_tlist. This lock
	 *  should be held before performing any operations on the log
	 *  involving a log record search or insert. */
	struct m0_mutex            dl_lock;
	struct m0_be_seg          *dl_seg;
	/** DTM0 clock source */
	struct m0_dtm0_clk_src    *dl_cs;
	union {
		/** Persistent list, used if dl_is_persistent */
		struct m0_be_list *dl_persist;
		/** Volatile list, used if !dl_is_persistent */
		struct m0_tl      *dl_inmem;
	} u;
};

/**
 * @b Typical call flow for calling DTM0 log interface routines:
 *
 * ** Initialisation/Finalisation for volatile fields of the log
 * ** Create a persistent log or just allocate a volatile (in-memory) log
 *   m0_be_dtm0_log_create / m0_be_dtm0_log_alloc
 * - m0_be_dtm0_log_init()
 * ** Preparation phase
 * - m0_be_dtm0_log_credit()
 * ** Normal operation phase
 * - m0_be_dtm0_log_update()
 * - m0_be_dtm0_log_prune()
 * - m0_be_dtm0_log_find()
 */

/**
 * Allocate an m0_be_dtm0_log. This call will allocate a dtm0 volatile log.
 * We need to call this routine once before we can perform any operations on .
 *
 * @pre log != NULL.
 * @post *log is allocated and ready to be initialized.
 *
 * @param log Pointer to variable in which to return the allocate memory
 *        address.
 *
 * @return 0 on success. Anything else is a failure.
 */
M0_INTERNAL int m0_be_dtm0_log_alloc(struct m0_be_dtm0_log **log);

/**
 * Initialize a dtm0 log. We need to call this routine once before we can
 * perform any operations on it.
 *
 * @pre log != NULL;
 * @pre cs != NULL;
 * @post *log is initialized and ready to be used
 *
 * @param log Pointer to a m0_be_dtm0_log that has to be initialized.
 * @param cs Pointer to a clock source. This will be used for comparisons
 *        involving log record timestamps.
 * @param is_plog A flag to indicate whether the dtm0 log that we are trying
 *        to init is a volatile of persistent store.
 *
 * @return 0 on success. Anything else is a failure.
 */
M0_INTERNAL int m0_be_dtm0_log_init(struct m0_be_dtm0_log  *log,
				    struct m0_be_seg       *seg,
				    struct m0_dtm0_clk_src *cs,
				    bool                    is_plog);

/**
 * Finalize a dtm0 log. We need to call this routine once after we have
 * finished performing all operations on it.
 *
 * @pre m0_be_dtm0_log__invariant needs to be satisfied.
 * @post All fields of *log are finalized. For volatile log the memory
 *       allocated by m0_be_dtm0_log_alloc is freed.
 *
 * @param log Pointer to a valid log.
 *
 * @return None
 */
M0_INTERNAL void m0_be_dtm0_log_fini(struct m0_be_dtm0_log *log);

/**
 * Free the memory allocated by m0_be_dtm0_log_alloc
 *
 * @pre m0_be_dtm0_log->dl_is_persistent needs to be false.
 * @post *log is set to NULL.
 *
 * @param log Pointer to a log structure that has been previously allocated.
 *
 * @return None
 */
M0_INTERNAL void m0_be_dtm0_log_free(struct m0_be_dtm0_log **log);

/**
 * For performing an operation on a persistent log, we need to take the
 * appropriate number of be transaction credit.
 *
 * @pre op in  enum m0_be_dtm0_log_credit_op
 *      seg != NULL
 *      accum != NULL
 * @post None
 *
 * @param op Operation code for the type of dtm0 log operation that we want
 *        to perform. @see @ref m0_be_dtm0_log_credit_op.
 * @param txd A valid dtm0 tx descriptor that we want to store in the log
 *        record.
 * @param payload The payload is an opaque structure for dtm0 log to store
 *        in the log record.
 * @param seg The be segment on which the log resides.
 * @param A pointer to a valid m0_dtm0_log_rec.
 * @param accum This contains the number of credits required for performing
 *        the operation.
 * @return None
 */
M0_INTERNAL void m0_be_dtm0_log_credit(enum m0_be_dtm0_log_credit_op op,
				       struct m0_dtm0_tx_desc       *txd,
				       struct m0_buf                *payload,
				       struct m0_be_seg             *seg,
				       struct m0_dtm0_log_rec       *rec,
				       struct m0_be_tx_credit       *accum);

/**
 * This routine is used to create a dtm0 persistent log.
 *
 * @pre tx != NULL
 * @pre seg != NULL
 * @post None
 *
 * @param tx handle to an already opened transaction.
 * @param seg be segment on which to create the log.
 * @param out variable in which to store the pointer to the newly created
 *        log.
 * @return 0 on success, anything else is a failure.
 */
M0_INTERNAL int m0_be_dtm0_log_create(struct m0_be_tx        *tx,
				      struct m0_be_seg       *seg,
				      struct m0_be_dtm0_log **out);

/**
 * This routine is used to destroy a previously created persistent dtm0 log
 *
 * @pre tx != NULL
 * @pre *log != NULL
 * @post None
 *
 * @param tx handle to an already opened transaction.
 * @param log Pointer to the dtm0 log which we wish to destroy
 * @return None
 */
M0_INTERNAL void m0_be_dtm0_log_destroy(struct m0_be_tx        *tx,
					struct m0_be_dtm0_log **log);

/**
 * This routine is used to update a record in the dtm0 log. If the record
 * does not exist it will be inserted.
 *
 * @pre payload != NULL
 * @pre m0_be_dtm0_log__invariant(log)
 * @pre m0_dtm0_tx_desc__invariant(txd)
 * @pre m0_mutex_is_locked(&log->dl_lock)
 * @post None
 *
 * @param log Pointer to the dtm0 log which we wish to update.
 * @param tx handle to an already opened transaction.
 * @param txd A valid dtm0 tx descriptor that we want to store in the log
 *        record.
 * @param payload The payload is an opaque structure for dtm0 log to store
 *        in the log record.
 * @return 0 on success, anything else is a failure.
 */
M0_INTERNAL int m0_be_dtm0_log_update(struct m0_be_dtm0_log  *log,
				      struct m0_be_tx        *tx,
				      struct m0_dtm0_tx_desc *txd,
				      struct m0_buf          *payload);

/**
 * Given a pointer to a dtm0 transaction id, this routine searches the
 * log for the log record matching this tx id and returns it.
 *
 * @pre m0_be_dtm0_log__invariant(log)
 * @pre m0_dtm0_tid__invariant(id))
 * @pre m0_mutex_is_locked(&log->dl_lock)
 *
 * @post None
 *
 * @param log Pointer to the dtm0 log in which we wish to search.
 * @param tid Pointer to a dtm0 transaction id corresponding to the record
 *        that we wish to fetch from the log.
 * @return m0_dtm0_log_rec corresponding to the input tid or NULL if no
 *        such record is found.
 */
M0_INTERNAL
struct m0_dtm0_log_rec *m0_be_dtm0_log_find(struct m0_be_dtm0_log    *log,
					    const struct m0_dtm0_tid *id);

/**
 * Given a pointer to a dtm0 transaction id, this routine searches the
 * log for the log record matching this tx id and removes all the records
 * having id lower or equal to the specified id.
 *
 * @pre *log->dl_is_persistent == false (This is a volatile log)
 * @pre m0_be_dtm0_log__invariant(log)
 * @pre m0_dtm0_tid__invariant(id)
 * @pre m0_mutex_is_locked(&log->dl_lock)
 * @post None
 *
 * @param log Pointer to the log from which we want to delete the record.
 * @param tx  Pointer to an open be transaction.
 * @param id  Pointer to a dtm0 transaction id corresponding to the record
 *        that we wish to delete from the log.
 * @return -ENOENT if no record exists for this id,
 *         0       if the record is found and successfully deleted,
 *         Anything else is considered a failure.
 */
M0_INTERNAL int m0_be_dtm0_log_prune(struct m0_be_dtm0_log    *log,
				     struct m0_be_tx          *tx,
				     const struct m0_dtm0_tid *id);

/**
 * Given a pointer to a dtm0 persistent log and a transaction id, this routine
 * searches the log for the log record matching this tx id and checks whether
 * all log records with id lower or equal to the specified id are persistent
 * and if they can be pruned.
 *
 * @pre m0_be_dtm0_log__invariant(log)
 * @pre m0_dtm0_tid__invariant(id)
 * @pre m0_mutex_is_locked(&log->dl_lock)
 * @post None
 *
 * @param log Pointer to the log from which we want to delete the record.
 * @param tx  Pointer to an open be transaction.
 * @param id  Pointer to a dtm0 transaction id corresponding to the record
 *        that we wish to delete from the log.
 * @return -ENOENT if no record exists for this tid,
 *         0       if the record is found and all previous records in the log
 *                 including this record can be successfully deleted,
 *         -EPROTO if the record is found but NOT all previous records in
 *                 the log can be deleted,
 *         Anything else is considered a failure.
 */
M0_INTERNAL bool m0_be_dtm0_plog_can_prune(struct m0_be_dtm0_log    *log,
					   const struct m0_dtm0_tid *id,
					   struct m0_be_tx_credit   *cred);

/**
 * Given a pointer to a dtm0 persistent log and a transaction id, this routine
 * searches the log for the log record matching this tx id and removes all the
 * log records with id lower or equal to the specified id that are persistent.
 *
 * @pre *log->dl_is_persistent == true
 * @pre m0_be_dtm0_log__invariant(log)
 * @pre m0_dtm0_tid__invariant(id)
 * @pre m0_mutex_is_locked(&log->dl_lock)
 * @post None
 *
 * @param log Pointer to the log from which we want to delete the record.
 * @param tx  Pointer to an open be transaction.
 * @param id  Pointer to a dtm0 transaction id corresponding to the record
 *        that we wish to delete from the log.
 * @return -ENOENT if no record exists for this id,
 *          0      if the record is found and successfully deleted,
 *          Anything else is considered a failure.
 */
M0_INTERNAL int m0_be_dtm0_plog_prune(struct m0_be_dtm0_log    *log,
				      struct m0_be_tx          *tx,
				      const struct m0_dtm0_tid *id);

/**
 * Given a pointer to a dtm0 volatile log clear the log and finalize it.
 *
 * @pre log is not a persistent log
 * @pre m0_dtm0_tx_desc_state_eq == M0_DTPS_PERSISTENT for all records in the
 *      log.
 * @post None
 *
 * @param log Pointer to the volatile log that we want to clear.
 * @return None.
 *
 * TODO: rename this to indicate that it's used for volatile usecase only.
 * TODO: remove later.
 * Removes all records from the volatile log.
 */
M0_INTERNAL void m0_be_dtm0_log_clear(struct m0_be_dtm0_log *log);

/**
 * Given a pointer to a dtm0 volatile log and a log record, insert the record
 * in the log.
 *
 * @pre log is a volatile log
 * @pre rec is initialized and valid
 * @post None
 *
 * @param log Pointer to the volatile log into which that we want to insert the
 *        record.
 * @return None.
 */
M0_INTERNAL int m0_be_dtm0_volatile_log_insert(struct m0_be_dtm0_log  *log,
					       struct m0_dtm0_log_rec *rec);

/**
 * This routine is used to update a record in a dtm0 volatile log.
 *
 * @pre log is a volatile log
 * @pre rec is initialized and valid
 * @post None
 *
 * @param log Pointer to the dtm0 log which we wish to destroy
 * @param rec Pointer to a valid dtm0 log record that we want to update.
 * @return None
 */
M0_INTERNAL void m0_be_dtm0_volatile_log_update(struct m0_be_dtm0_log  *log,
						struct m0_dtm0_log_rec *rec);

/**
 * Deliver a persistent message to the log.
 *
 * @pre log is a volatile log
 * @pre fop->f_type == &dtm0_req_fop_fopt;
 * @post None
 *
 * @param log Pointer to the dtm0 log which we wish to destroy
 * @param fop This is a fop for the request carrying a PERSISTENT notice
 * @return None
 *
 * TODO: Only volatile log is supported so far.
 */
M0_INTERNAL void m0_be_dtm0_log_pmsg_post(struct m0_be_dtm0_log *log,
					  struct m0_fop         *fop);

/** @} */ /* end DTM0Internals */

#endif /* __MOTR_BE_DTM0_LOG_H__ */

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
