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

#ifndef __MOTR_BE_LOG_H__
#define __MOTR_BE_LOG_H__

#include "lib/mutex.h"          /* m0_mutex */
#include "lib/tlist.h"          /* m0_tl */
#include "lib/types.h"          /* m0_bcount_t */

#include "be/fmt.h"
#include "be/log_store.h"       /* m0_be_log_store */
#include "be/log_sched.h"       /* m0_be_log_sched */
#include "be/op.h"              /* m0_be_op */
#include "be/io.h"              /* m0_be_io */
#include "be/recovery.h"        /* m0_be_recovery */

#include "stob/io.h"            /* m0_stob_io_opcode */

/**
 * @defgroup be Meta-data back-end
 *
 * @{
 */

/**
 * BE uses write-ahead logging. Log is used as a place to write information
 * about transactions to make transactions recoverable.
 *
 * Logically, log is an infinite sequence of log records. Physical
 * representation is implemented by log_store.
 *
 * A position in the log is identified by a "log sequence number" (lsn), which
 * is simply an offset in the logical log. Lsn uniquely identifies a point in
 * system history.
 *
 * <b>Log record</b>
 *
 * Log record encapsulates multiple buffers and provides interface for access
 * to specific buffer. Log record is placed continuously in log (but not
 * necessarily in stob).
 *
 * Every buffer is read/written by separated log I/O and as result must be
 * aligned for stob I/O. This is guaranteed by log record implementation.
 *
 * A log record has log record header and footer. They are written as part of
 * the first and the last buffer respectively. Therefore they don't require
 * separated aligned buffers nor log I/Os. Additional memory for header and
 * footer is allocated transparently for user.
 *
 * Log record format:
 *
 * @verbatim
 * +--------+----------------------+-//-+-----------------------+--------+
 * | header | 1st buffer | padding |    | last buffer | padding | footer |
 * +--------+----------------------+-//-+-----------------------+--------+
 * @endverbatim
 *
 * A valid log record is record that fully resides on backing store and is not
 * corrupted.
 *
 * Log record structure can be reused after reset.
 *
 * <b>Reservation</b>
 *
 * User must reserve space in log for every record before operations with the
 * record. Reserved space can be larger than will be actually used, but can't be
 * less. Reservation doesn't allocate space inside log and doesn't make I/O.
 *
 * User must use function m0_be_log_reserved_size() with maximum possible
 * buffers. This function returns required space in log for respective log
 * record and takes into account header, footer and paddings.
 *
 * If m0_be_log_reserve() returns -EAGAIN user should repeat attempt later.
 * Proper condition for this is call of user-defined callback function. This
 * callback function is passed to log as field lc_got_space_cb in configuration.
 *
 * Reservation mechanism guarantees that infinite log can be implemented with
 * finite backing store.
 *
 * <b>Log header</b>
 *
 * Log header contains information required for restoring current position in
 * log after successful closing of the log. If log isn't closed successfully
 * recovery is responsible for setting correct log's pointers after
 * initialisation.
 *
 * Fields of log header:
 *   - Pointer to a valid log record. On successful close this field contains
 *     pointer to the last record.
 *   - Size of the valid log record.
 *
 * When the valid log record from log header needs to be rewritten on backing
 * store the log header must be updated before the record becomes corrupted.
 * It guarantees that log header always points to a valid log record.
 *
 * Also header is updated on close of log. It is done for two reasons:
 *   - Store pointer to the last record and continue writing new records from
 *     this point.
 *   - Keep pointer closer to the last record to reduce number of reads during
 *     recovery.
 *
 * Log uses log_store's redundant buffer to store log header. Log header is self
 * recoverable and m0_be_log_header_read() returns correct header even if it is
 * partially written. As result this function can make I/O.
 *
 * <b>Discarding</b>
 *
 * When user doesn't need log record it should be discarded. Discarding notifies
 * that log record structure isn't accessed and log record in log isn't
 * required. User must not discard a log record before I/O to log is finished.
 *
 * Discarded log record is counted as free space only when all previous log
 * records (with smaller lsn) are discarded too.
 *
 * If a record wasn't discarded before shutdown it should be discarded after
 * reading and re-applying during recovery. User must not discard log record if
 * it is read not for re-applying.
 *
 * <b>Finalisation of non-discarded log records</b>
 *
 * Log allows finalisation of non-discarded log records, but counts them as not
 * placed ones. If such a record exists log sets boolean lg_unplaced_exists to
 * true and all space after the first unplaced record is counted used. This mode
 * is cleared only after closing of the log and must not be used during normal
 * work.
 *
 * This mode can be used during shutdown when user can't complete operations but
 * wants recovery to re-apply them.
 *
 * Recovery is responsible for re-applying all unplaced and following them log
 * records.
 *
 * <b>Log record iterator</b>
 *
 * Log record iterator is a structure that contains only information from log
 * record header and provides interface initial/next/prev for scanning log. This
 * approach allows to keep information about all log records in memory and read
 * the content on-demand.
 *
 * <b>Locking</b>
 *
 * User is responsible for locking of log's fields. This external lock is passed
 * to log through configuration and is used only in preconditions.
 *
 * Log record's field lgr_state is protected by lg_record_state_lock. It needs
 * to be protected only after the record is scheduled.
 *
 * <b>Guarantees</b>
 *
 * User must guarantee:
 *   - Eliminate access to log record after it is scheduled and before log
 *     signals about its completion;
 *   - Recover I/O errors before attempt to write to the log. Log must not
 *     contain corrupted log record among valid ones;
 *
 * Log guarantees:
 *   - Log record is not accessed by log after user is signalled about
 *     completion of operation with record;
 *   - A valid log record which is pointed by log header and following records
 *     are not rewritten or updated.
 *
 * <b>Usecases</b>
 *
 * Write to log:
 *   1. m0_be_log_record_init
 *   2. m0_be_log_record_io_create for every buffer
 *   3. m0_be_log_record_allocate
 *   4. m0_be_log_reserve
 *   5. m0_be_log_record_io_size_set for every buffer
 *   6. m0_be_log_record_io_prepare
 *   7. Fill every buffer with data
 *   8. m0_be_log_record_io_launch
 *   9. m0_be_log_record_discard
 *   10. If log record is reusable then m0_be_log_record_reset and jump to 4.
 *   11. m0_be_log_record_deallocate
 *   12. m0_be_log_record_fini
 *
 * Read from log:
 *   1. m0_be_log_record_iter_init
 *   2. m0_be_log_record_initial/next/prev
 *   3. m0_be_log_record_init
 *   4. m0_be_log_record_io_create for every buffer
 *   5. m0_be_log_record_allocate
 *   6. m0_be_log_record_assign
 *   7. m0_be_log_record_io_prepare
 *   8. m0_be_log_record_io_launch
 *   9. Access to buffers
 *   9. If recovery re-applies record then m0_be_log_record_discard
 *   10. If log record is reusable then m0_be_log_record_reset and jump to 6.
 *   11. m0_be_log_record_deallocate
 *   12. m0_be_log_record_fini
 *
 * @todo BE log's positions grow to infinity, but have uint64_t type.
 *       Better solution will be usage of blocks instead of bytes for the
 *       positions.
 */

struct m0_ext;
struct m0_stob_id;
struct m0_be_io;
struct m0_be_log_io;
struct m0_be_log_record_iter;
struct m0_be_tx_group;

struct m0_be_log;
struct m0_be_log_record;
typedef void (*m0_be_log_got_space_cb_t)(struct m0_be_log *log);
typedef void (*m0_be_log_record_cb_t)(struct m0_be_log_record *record);
typedef void (*m0_be_log_full_cb_t)(struct m0_be_log *log);

enum {
	M0_BE_LOG_RECORD_IO_NR_MAX = 2,
};

enum {
	M0_BE_LOG_LEVEL_INIT,
	M0_BE_LOG_LEVEL_LOG_SCHED,
	M0_BE_LOG_LEVEL_LOG_STORE,
	M0_BE_LOG_LEVEL_HEADER_PREINIT,
	M0_BE_LOG_LEVEL_HEADER,
	M0_BE_LOG_LEVEL_RECOVERY,
	M0_BE_LOG_LEVEL_ASSIGNS,
	M0_BE_LOG_LEVEL_READY,
};

struct m0_be_log_cfg {
	struct m0_be_log_store_cfg  lc_store_cfg;
	struct m0_be_log_sched_cfg  lc_sched_cfg;
	struct m0_be_recovery_cfg   lc_recovery_cfg;
	m0_be_log_got_space_cb_t    lc_got_space_cb;
	m0_be_log_full_cb_t         lc_full_cb;
	m0_bcount_t                 lc_full_threshold;
	struct m0_mutex            *lc_lock;
	bool                        lc_skip_recovery;
};

/** This structure encapsulates internals of transactional log. */
struct m0_be_log {
	struct m0_be_log_cfg     lg_cfg;
	bool                     lg_create_mode;
	bool                     lg_destroy_mode;
	struct m0_module         lg_module;
	/**
	 * Underlying storage.
	 *
	 * @todo this might be changed to something more complicated to support
	 * flexible deployment and grow-able logs. E.g., a log can be stored in
	 * a sequence of regions in segments, linked to each other through
	 * header blocks.
	 */
	struct m0_be_log_store   lg_store;
	/** Scheduler */
	struct m0_be_log_sched   lg_sched;
	struct m0_be_recovery    lg_recovery;
	struct m0_be_fmt_log_header lg_header;
	/** List of all non-discarded log_records. TODO remove it as unneeded */
	struct m0_tl             lg_records;
	/** Protects m0_be_log_record.lgr_state after the record is scheduled */
	struct m0_mutex          lg_record_state_lock;
	/** Logical offset of the reserved space within log */
	m0_bindex_t              lg_current;
	/** Logical offset of the first non-discarded log record */
	m0_bindex_t              lg_discarded;
	/** Free space in the log */
	m0_bcount_t              lg_free;
	/** Reserved space in the log */
	m0_bcount_t              lg_reserved;
	/** Position of the last allocated record */
	m0_bindex_t              lg_prev_record;
	m0_bcount_t              lg_prev_record_size;
	/**
	 * Indicates that there is a finalised/reset but not discarded log
	 * record. Log keeps pointer to such record with the least lsn.
	 */
	bool                     lg_unplaced_exists;
	m0_bindex_t              lg_unplaced_pos;
	m0_bcount_t              lg_unplaced_size;
	/**
	 * Callback for notification about free space availability, so user can
	 * retry failed reservation.
	 */
	m0_be_log_got_space_cb_t lg_got_space_cb;
	/**
	 * Pointer to an external lock that synchronizes access to log's fields.
	 * Used only for the invariant.
	 */
	struct m0_mutex         *lg_external_lock;
	/* op for log header read */
	struct m0_be_op          lg_header_read_op;
	/* op for log header write */
	struct m0_be_op          lg_header_write_op;
};

/* m0_be_log */
#define BL_F "(lg_current=%"PRIu64" lg_discarded=%"PRIu64" lg_free=%"PRIu64")"
#define BL_P(log) (log)->lg_current, (log)->lg_discarded, (log)->lg_free

/** This structure represents minimal unit for log operations. */
struct m0_be_log_record {
	struct m0_be_log    *lgr_log;
	int                  lgr_state;
	/** Specifies if record is discarded after read operation. */
	bool                 lgr_need_discard;
	/**
	 * Last known value of the Discarded pointer. User can use it for
	 * generation of tx_group's commit block.
	 */
	m0_bindex_t          lgr_last_discarded;
	/** Logical offset within log. Is set during space allocation. */
	m0_bindex_t          lgr_position;
	m0_bcount_t          lgr_size;
	/** Pointer to the previous record inside log */
	m0_bindex_t          lgr_prev_pos;
	m0_bcount_t          lgr_prev_size;
	/**
	 * BE log header flh_discarded pointer during
	 * m0_be_log_record_io_prepare(). It should be set to a value that has
	 * already became persistent.
	 *
	 * Use case: discard FOL records (sent as FDMI records) at FDMI plugin.
	 * This value allows to filter out FDMI records that are never going to
	 * be resent because BE log records before this pointer are never going
	 * to be recovered (they are already fully written to BE segments and BE
	 * log header is updated to reflect that).
	 */
	m0_bindex_t          lgr_log_header_discarded;

	uint64_t             lgr_magic;
	struct m0_tlink      lgr_linkage;
	struct m0_tlink      lgr_sched_linkage;

	/* Fields for I/O launch */
	int                  lgr_io_nr;
	struct m0_be_log_io *lgr_io[M0_BE_LOG_RECORD_IO_NR_MAX];
	struct m0_be_op     *lgr_op[M0_BE_LOG_RECORD_IO_NR_MAX];
	struct m0_be_op      lgr_record_op;

	/** Notification about log record completion */
	struct m0_be_op     *lgr_user_op;

	struct m0_be_fmt_log_record_header lgr_header;
	struct m0_be_fmt_log_record_footer lgr_footer;

	/*
	 * Log header is updated just before the record is written
	 * to the log if the flag is set.
	 */
	bool                 lgr_write_header;
};

/* m0_be_log_record */
#define BLR_F "(lgr_last_discarded=%"PRIu64" lgr_position=%"PRIu64" " \
	       "lgr_size=%"PRIu64" " \
	       "lgr_prev_pos=%"PRIu64" lgr_prev_size=%"PRIu64")"
#define BLR_P(record) (record)->lgr_last_discarded, (record)->lgr_position, \
		      (record)->lgr_size, \
		      (record)->lgr_prev_pos, (record)->lgr_prev_size

M0_INTERNAL void m0_be_log_module_setup(struct m0_be_log     *log,
					struct m0_be_log_cfg *lg_cfg,
					bool                  create_mode);

M0_INTERNAL bool m0_be_log__invariant(struct m0_be_log *log);

M0_INTERNAL int  m0_be_log_open(struct m0_be_log     *log,
				struct m0_be_log_cfg *log_cfg);
M0_INTERNAL void m0_be_log_close(struct m0_be_log *log);

M0_INTERNAL int  m0_be_log_create(struct m0_be_log     *log,
				  struct m0_be_log_cfg *log_cfg);
M0_INTERNAL void m0_be_log_destroy(struct m0_be_log *log);

M0_INTERNAL m0_bcount_t m0_be_log_reserved_size(struct m0_be_log *log,
						m0_bcount_t      *lio_size,
						int               lio_nr);

/**
 * Initialises log record.
 *
 * @param cb callback is called when all operations for the log record are
 *           finished.
 */
M0_INTERNAL void m0_be_log_record_init(struct m0_be_log_record *record,
				       struct m0_be_log *log);
M0_INTERNAL void m0_be_log_record_fini(struct m0_be_log_record *record);
/** Resets log record. */
M0_INTERNAL void m0_be_log_record_reset(struct m0_be_log_record *record);

/**
 * Assigns log record iterator to log record. Copies information from iter to
 * record for further reading of the record's buffers.
 *
 * @param record Initialised and allocated log record. After execution
 *               represents log record which is specified by iter. Record's
 *               buffers have uninitialised content.
 * @param need_discard Specifies if the record will be discarded. Record should
 *                     be discarded only if user reads it for re-applying.
 */
M0_INTERNAL void
m0_be_log_record_assign(struct m0_be_log_record      *record,
			struct m0_be_log_record_iter *iter,
			bool                          need_discard);

M0_INTERNAL void m0_be_log_record_ext(struct m0_be_log_record *record,
                                      struct m0_ext           *ext);

/** Skips discarding of a record. The record will remain non-discarded. */
M0_INTERNAL void m0_be_log_record_skip_discard(struct m0_be_log_record *record);
/** Discards used space withing log. */
M0_INTERNAL void m0_be_log_record_discard(struct m0_be_log *log,
					  m0_bcount_t       size);

M0_INTERNAL int m0_be_log_record_io_create(struct m0_be_log_record *record,
					   m0_bcount_t              size_max);
/**
 * Allocates buffers for log i/o that were created through
 * m0_be_log_record_io_create().
 *
 * @post m0_be_log_record_io_create() isn't called for this record anymore
 */
M0_INTERNAL int m0_be_log_record_allocate(struct m0_be_log_record *record);
M0_INTERNAL void m0_be_log_record_deallocate(struct m0_be_log_record *record);
M0_INTERNAL void m0_be_log_record_io_size_set(struct m0_be_log_record *record,
					      int                      index,
					      m0_bcount_t              size);
M0_INTERNAL void
m0_be_log_record_io_prepare(struct m0_be_log_record *record,
			    enum m0_stob_io_opcode   opcode,
			    m0_bcount_t              size_reserved);
M0_INTERNAL struct m0_bufvec *
m0_be_log_record_io_bufvec(struct m0_be_log_record *record,
			   int                      index);
/**
 * Schedule log record. Log owns the log record until user is signalled about
 * completion.
 */
M0_INTERNAL void m0_be_log_record_io_launch(struct m0_be_log_record *record,
					    struct m0_be_op         *op);


/** Returns absolute position of this log record in BE log */
M0_INTERNAL m0_bindex_t
m0_be_log_record_position(const struct m0_be_log_record *record);

/**
 * Returns BE log discarded pointer before or at the time the record was
 * written to the log.
 */
M0_INTERNAL m0_bindex_t
m0_be_log_record_discarded(const struct m0_be_log_record *record);

/**
 * Reserves space for a log record. Reserved size can be bigger than actual
 * size of the log record.
 */
M0_INTERNAL int m0_be_log_reserve(struct m0_be_log *log, m0_bcount_t size);
/**
 * Unreserves space that was previously reserved with m0_be_log_reserve().
 * Used if tx failed, but log space is already reserved.
 */
M0_INTERNAL void m0_be_log_unreserve(struct m0_be_log *log, m0_bcount_t size);

/** Returns optimal block shift for the underlying storage. */
M0_INTERNAL uint32_t m0_be_log_bshift(struct m0_be_log *log);

M0_INTERNAL void m0_be_log_header__set(struct m0_be_fmt_log_header *hdr,
				       m0_bindex_t                  discarded,
				       m0_bindex_t                  lsn,
				       m0_bcount_t                  size);
M0_INTERNAL bool m0_be_log_header__is_eq(struct m0_be_fmt_log_header *hdr1,
					 struct m0_be_fmt_log_header *hdr2);
M0_INTERNAL bool m0_be_log_header__repair(struct m0_be_fmt_log_header **hdrs,
					  int                           nr,
					  struct m0_be_fmt_log_header  *out);
/**
 * Reads log header. Returned log_hdr structure is valid even if one log header
 * is corrupted. It is achieved by storing redundant log headers.
 */
M0_INTERNAL int m0_be_log_header_read(struct m0_be_log            *log,
				      struct m0_be_fmt_log_header *log_hdr);

struct m0_be_log_record_iter {
	struct m0_be_fmt_log_record_header lri_header;
	/** @see lgr_log_header_discarded */
	m0_bindex_t                        lri_log_header_discarded;
	struct m0_tlink                    lri_linkage;
	uint64_t                           lri_magic;
};

M0_INTERNAL int m0_be_log_record_iter_init(struct m0_be_log_record_iter *iter);
M0_INTERNAL void m0_be_log_record_iter_fini(struct m0_be_log_record_iter *iter);
M0_INTERNAL void m0_be_log_record_iter_copy(struct m0_be_log_record_iter *dest,
					    struct m0_be_log_record_iter *src);
M0_INTERNAL int m0_be_log_record_initial(struct m0_be_log             *log,
					 struct m0_be_log_record_iter *curr);
M0_INTERNAL int m0_be_log_record_next(struct m0_be_log                   *log,
				      const struct m0_be_log_record_iter *curr,
				      struct m0_be_log_record_iter       *next);
M0_INTERNAL int m0_be_log_record_prev(struct m0_be_log *log,
				      const struct m0_be_log_record_iter *curr,
				      struct m0_be_log_record_iter       *prev);
M0_INTERNAL bool m0_be_fmt_log_record_header__invariant(
				struct m0_be_fmt_log_record_header *header,
				struct m0_be_log                   *log);

M0_INTERNAL bool
m0_be_log_recovery_record_available(struct m0_be_log *log);
M0_INTERNAL void
m0_be_log_recovery_record_get(struct m0_be_log             *log,
			      struct m0_be_log_record_iter *iter);
M0_INTERNAL m0_bindex_t
m0_be_log_recovery_discarded(struct m0_be_log *log);

M0_INTERNAL bool m0_be_log_contains_stob(struct m0_be_log        *log,
                                         const struct m0_stob_id *stob_id);
/** @} end of be group */

#endif /* __MOTR_BE_LOG_H__ */


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
