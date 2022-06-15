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

#ifndef __MOTR_FDMI_FDMI_SOURCE_DOCK_H__
#define __MOTR_FDMI_FDMI_SOURCE_DOCK_H__

#include "lib/buf.h"
#include "lib/tlist.h"
#include "lib/mutex.h"
#include "fop/fom.h"
#include "fdmi/fdmi.h"
#include "fdmi/filter.h"
#include "fdmi/src_rec.h"

/* This file describes FDMI source dock public API */

/**
   @defgroup fdmi_sd FDMI Source Dock public interface
   @ingroup fdmi_main

   @see @ref FDMI-DLD-fspec "FDMI Functional Specification"
   @{
*/

/** FDMI source  */
struct m0_fdmi_src {
	/** Source/record type identifier */
	enum m0_fdmi_rec_type_id fs_type_id;

	/* Data owned and populated by source dock. */

	/** Function to post FDMI data */
	void (*fs_record_post)(struct m0_fdmi_src_rec *src_rec);

	/* Data owned and populated by source. */

	/**
	 * Function to return specific source field value according to its
	 * description. Mandatory.
	 */
	int (*fs_node_eval)(struct m0_fdmi_src_rec 	*src_rec,
			    struct m0_fdmi_flt_var_node *value_desc,
			    struct m0_fdmi_flt_operand 	*value);
	/** Function to increment source specific ref counter. Optional. */
	void (*fs_get)(struct m0_fdmi_src_rec *src_rec);
	/** Function to decrement source specific ref counter. Optional. */
	void (*fs_put)(struct m0_fdmi_src_rec *src_rec);
	/** Processing of FDMI record is about to start. Optional. */
	void (*fs_begin)(struct m0_fdmi_src_rec *src_rec);
	/** Processing of FDMI record is complete and can be freed. Optional. */
	void (*fs_end)(struct m0_fdmi_src_rec *src_rec);

	/**
	 * Xcode function: encoding at given location.
	 *
	 * @param buf - encoding result will be saved into this
	 * buffer.  Memory will be allocated by the encode method.
	 * 'buf' must not be preallocated (expected buf->b_addr ==
	 * NULL).
	 */
	int (*fs_encode)(struct m0_fdmi_src_rec *src_rec, struct m0_buf *buf);

	/**
	 * Xcode function: decoding from buf.
	 *
	 * @param handle - OUT param to return the object which is a
	 * result of decoding.  Memory is allocated inside 'decode'.
	 * Caller is responsible for freeing it.  It is also assumed
	 * that caller knows the exact type of the result.
	 */
	int (*fs_decode)(struct m0_buf *buf, void **handle);

};

/** Source structure allocation, along with wrapping context */

M0_INTERNAL int m0_fdmi_source_alloc(enum m0_fdmi_rec_type_id type_id,
				     struct m0_fdmi_src     **source);

/** Source structure release, along with wrapping context */

M0_INTERNAL void m0_fdmi_source_free(struct m0_fdmi_src *source);

/**
   Register FDMI source type.
   Should be called only once for each FDMI source type.
   This call provides source dock private API for posting
   record and obtaining new FDMI record id.
 */
M0_INTERNAL int m0_fdmi_source_register(struct m0_fdmi_src *src);

/** Deregister FDMI source type.
 *
 * Must ONLY be called during motr fini phase (from the _fini method of the
 * corresponding source).
 *
 * Marks the source as not registered; dock will no longer apply any filters
 * to it, and will issue no more callbacks (e.g. on 'release record' coming
 * from plugin dock).
 *
 * Iteratest over all records from this source that are waiting for 'release'
 * from plugins.  Removes these records from internal dock's list, calls
 * deinit for whatever runtime data it has stored in src_rec part.  But it
 * does NOT call fs_put or fs_end, so it's up to plugin to release these
 * locks (which must be done after this deregister call).
 * */
M0_INTERNAL void m0_fdmi_source_deregister(struct m0_fdmi_src *src);


/** Initialize FDMI source dock internals */
M0_INTERNAL void
m0_fdmi_source_dock_init(struct m0_fdmi_src_dock *src_dock);

/** Deinitialize FDMI source dock instance */
M0_INTERNAL void
m0_fdmi_source_dock_fini(struct m0_fdmi_src_dock *src_dock);

/**
   Posts source data to FDMI
   @param _src_reg   Pointer to registered FDMI source instance

   @see m0_fdmi_source_register()

   @note Important! Source implementation should increment reference counter (if
   it is supported by the source) for the posted FDMI record.
*/
#define M0_FDMI_SOURCE_POST_RECORD(_src_rec_ptr) \
	((_src_rec_ptr)->fsr_src)->fs_record_post(_src_rec_ptr)

/** @} end of fdmi_sd group */

#endif /* __MOTR_FDMI_FDMI_SOURCE_DOCK_H__ */

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
