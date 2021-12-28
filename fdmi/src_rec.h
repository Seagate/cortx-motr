/* -*- C -*- */
/*
 * Copyright (c) 2017-2020 Seagate Technology LLC and/or its Affiliates
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

#ifndef __MOTR_FDMI_FDMI_SRC_REC_H__
#define __MOTR_FDMI_FDMI_SRC_REC_H__

#include "lib/tlist.h"
#include "lib/refs.h"

/* This file contains definition of FDMI Source Record. */

struct m0_fdmi_src;
struct m0_fdmi_src_ctx;

/**
   @addtogroup  fdmi_sd
   @{
 */

/** FDMI source record struct. Used to keep data, associated to FDMI src record.
 *
 * Is also used to pass the record over between source and source dock.  On
 * this communication, a pointer to this struct is uniquely identifying the
 * record being processed.
 *
 * FDMI source is responsible for allocating it, and filling in the following
 * fields:
 * - fsr_src
 * - fsr_data
 *
 * The rest of fields are populated by source dock.  The key ones are filled
 * in at post_record call, the rest -- during record processing.  FDMI source
 * dock will keeps here all the information on FDMI record that is being
 * processed (or being sent to plugin).
 *
 * FDMI source is also responsible for de-allocating the struct, but only
 * after source dock calls fs_put for the last time.
 *
 * One recommended way of using this structure is to incorporate it into
 * whatever struct that keeps actual data/descriptor of the event/object which
 * has caused this FDMI record to come into existence.  Later on, whenever src
 * dock will require actions on the record, the source can easily obtain the
 * ptr to ambient structure and use it, no need for any look-ups.
 */
struct m0_fdmi_src_rec {
	uint64_t                    fsr_magic;

	/* Source owns fields: */

	/** Link to owner FDMI source. */
	struct m0_fdmi_src         *fsr_src;

	/** Source can save record-specifric data handle here. */
	void                       *fsr_data;

	/* FDMI dock owns fields: */

	/** FDMI record ID. Should be unique within FDMI source dock instance */
	struct m0_uint128           fsr_rec_id;

	/** FDMI record internal reference counter, used by FDMI source dock.
	 *
	 * Is only needed during sending of record to plugins.  Once all
	 * plugins confirmed they got the record, this ref is decremented down
	 * to zero and calls its callback (which in turn calls fs_end). */
	struct m0_ref               fsr_ref;

	/** Matched filters list.
	 *
	 * Links using fdmi_src_matched_filter_item.fsmfi_linkage */
	struct m0_tl                fsr_filter_list;

	/** Service field for linked list */
	struct m0_tlink             fsr_linkage;

	/** Source that posted this record */
	struct m0_fdmi_src_ctx     *fsr_src_ctx;

	/* Used for UT. Meaning: true if any filter matched this record. */
	bool			    fsr_matched;
	/**
	 * Used for UT. Meaning: set to true if this records should not be
	 * set to remote ep after matching.
	 */
	bool			    fsr_dryrun;

	/**
	 * The first time this record is initialized for post.
	 */
	m0_time_t                   fsr_init_time;
};

/** Validates that the record is valid.  Should be used in M0_ASSERT in every
 * function that receives src_rec as input parameter. */
M0_INTERNAL bool m0_fdmi__record_is_valid(struct m0_fdmi_src_rec *src_rec);

/** @} addtogroup fdmi_sd */

#endif /* __MOTR_FDMI_FDMI_SRC_REC_H__ */

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
