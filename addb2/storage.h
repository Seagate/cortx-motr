/* -*- C -*- */
/*
 * Copyright (c) 2015-2020 Seagate Technology LLC and/or its Affiliates
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

#ifndef __MOTR_ADDB2_STORAGE_H__
#define __MOTR_ADDB2_STORAGE_H__

/**
 * @defgroup addb2
 *
 * Addb2 storage
 * -------------
 *
 * Addb2 storage (m0_addb2_storage) is responsible for storing addb2 records on
 * storage.
 *
 * Records are submitted to the storage in the form of traces (m0_addb2_trace),
 * wrapped in trace objects (m0_addb2_trace_obj). When a trace has been stored,
 * m0_addb2_trace_obj::o_done() is invoked. When an entire trace has been stored
 * m0_addb2_storage_ops::sto_done() call-back is invoked. In addition,
 * m0_addb2_storage_ops::sto_commit() is periodically called to inform SYSTEM
 * about the position of the last stored trace. The SYSTEM is supposed to store
 * this information persistently and pass it to m0_addb2_storage_init() on the
 * next initialisation.
 *
 * @{
 */

/* import */
#include "lib/types.h"
#include "fid/fid.h"
#include "fid/fid_xc.h"
#include "xcode/xcode_attr.h"
#include "format/format.h"
#include "format/format_xc.h"
struct m0_stob;
struct m0_addb2_trace_obj;
struct m0_addb2_record;

/* export */
struct m0_addb2_storage;
struct m0_addb2_storage_ops;
struct m0_addb2_frame_header;
struct m0_addb2_sit;

/**
 * Operation vector passed to m0_addb2_storgae_init() by the SYSTEM.
 */
struct m0_addb2_storage_ops {
	/**
	 * This is invoked by the storage component after stopped
	 * m0_addb2_storage() completed processing the last trace.
	 */
	void (*sto_idle)(struct m0_addb2_storage *stor);
	/**
	 * This is called after processing a trace object.
	 */
	void (*sto_done)(struct m0_addb2_storage *stor,
			 struct m0_addb2_trace_obj *obj);
	/**
	 * This is called inform the SYSTEM about the position of the last
	 * written trace.
	 */
	void (*sto_commit)(struct m0_addb2_storage *stor,
			   const struct m0_addb2_frame_header *anchor);
};

/**
 * Allocates and initialises the storage machine.
 *
 * @param location - the stob domain's location.
 * @param key - the stob domain's key.
 * @param mkfs - if true, creates the stob domain.
 * @param force - if true, overwrites the existing stob domain.
 * @param size - the size of the storage object.
 * @param cookie - an arbitrary cookie returned by m0_addb2_storage_cookie().
 *
 * @pre size is a multiple of stob block size.
 */
M0_INTERNAL struct m0_addb2_storage *
m0_addb2_storage_init(const char *location, uint64_t key, bool mkfs, bool force,
		      const struct m0_addb2_storage_ops *ops, m0_bcount_t size,
		      void *cookie);

/**
 * Returns the cookie passed to m0_addb2_storage_init().
 */
M0_INTERNAL void *m0_addb2_storage_cookie(const struct m0_addb2_storage *stor);

/**
 * Initiates storage machine stopping.
 *
 * New traces are not accepted any more. Once the last trace is processed,
 * m0_addb2_storage_ops::sto_idle() is invoked.
 */
M0_INTERNAL void m0_addb2_storage_stop(struct m0_addb2_storage *stor);

/**
 * Finalises an idle storage machine.
 */
M0_INTERNAL void m0_addb2_storage_fini(struct m0_addb2_storage *stor);

/**
 * Submits a trace to storage.
 */
M0_INTERNAL int m0_addb2_storage_submit(struct m0_addb2_storage *stor,
					struct m0_addb2_trace_obj *obj);
/**
 * Header used by storage machine to locate traces on storage.
 */
struct m0_addb2_frame_header {
	struct m0_format_header he_header;
	uint64_t                he_seqno;
	uint64_t                he_offset;
	uint64_t                he_prev_offset;
	uint32_t                he_trace_nr;
	uint32_t                he_size;
	uint64_t                he_time;
	uint64_t                he_stob_size;
	uint64_t                he_magix;
	struct m0_fid           he_fid;
	struct m0_format_footer he_footer;
} M0_XCA_RECORD;

enum m0_addb2_frame_header_format_version {
	M0_ADDB2_FRAME_HEADER_FORMAT_VERSION_1 = 1,

	/*
	 * future versions, uncomment and update
	 * M0_ADDB2_FRAME_HEADER_FORMAT_VERSION
	 */
	/*M0_ADDB2_FRAME_HEADER_FORMAT_VERSION_2,*/
	/*M0_ADDB2_FRAME_HEADER_FORMAT_VERSION_3,*/

	/** Current version, should point to the latest version present */
	M0_ADDB2_FRAME_HEADER_FORMAT_VERSION = M0_ADDB2_FRAME_HEADER_FORMAT_VERSION_1
};

/**
 * Returns the header of the latest frame recorded on the stob.
 */
M0_INTERNAL int m0_addb2_storage_header(struct m0_stob *stob,
					struct m0_addb2_frame_header *h);
/*
 * Storage offline CONSUMER interface.
 */

/**
 * Allocates and initialises storage trace iterator.
 */
int  m0_addb2_sit_init(struct m0_addb2_sit **out, struct m0_stob *stob,
		       m0_bindex_t start);
/**
 * Returns the next record from the storage iterator.
 *
 * Returns +ve value on success, 0 for "no more records", -ve on error.
 */
int  m0_addb2_sit_next(struct m0_addb2_sit *it, struct m0_addb2_record **out);
void m0_addb2_sit_fini(struct m0_addb2_sit *it);

/**
 * Returns the record source embedded in the iterator.
 */
struct m0_addb2_source *m0_addb2_sit_source(struct m0_addb2_sit *it);

/** @} end of addb2 group */
#endif /* __MOTR_ADDB2_STORAGE_H__ */

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
