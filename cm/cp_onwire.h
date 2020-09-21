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

#ifndef __MOTR_CM_CP_ONWIRE_H__
#define __MOTR_CM_CP_ONWIRE_H__

#include "lib/bitmap.h"            /* m0_bitmap */
#include "lib/bitmap_xc.h"         /* m0_bitmap_xc */
#include "ioservice/io_fops.h"     /* m0_io_descs */
#include "ioservice/io_fops_xc.h"  /* m0_io_descs_xc */
#include "cm/ag.h"                 /* m0_cm_ag_id */
#include "cm/ag_xc.h"              /* m0_cm_ag_id_xc */

/** Onwire copy packet structure. */
struct m0_cpx {
	/** Copy packet priority. */
	uint32_t                  cpx_prio;

	/**
	 * Aggregation group id corresponding to an aggregation group,
	 * to which the copy packet belongs.
	 */
	struct m0_cm_ag_id        cpx_ag_id;

	/** Global index of this copy packet in aggregation group. */
	uint64_t                  cpx_ag_cp_idx;

	/** Bitmap representing the accumulator information. */
	struct m0_bitmap_onwire   cpx_bm;

	/** Network buffer descriptors corresponding to copy packet data. */
	struct m0_io_descs        cpx_desc;

	/**
	 * Epoch of the copy packet, the same as that from the copy machine
	 * which this copy packet belongs to.
	 */
	m0_time_t                 cpx_epoch;
} M0_XCA_RECORD M0_XCA_DOMAIN(rpc);

/** Onwire copy packet reply. */
struct m0_cpx_reply {
	int32_t           cr_rc;
} M0_XCA_RECORD M0_XCA_DOMAIN(rpc);

#endif /*  __MOTR_CM_CP_ONWIRE_H__ */
/*
 *  Local variables:
 *  c-indentation-style: "K&R"
 *  c-basic-offset: 8
 *  tab-width: 8
 *  fill-column: 80
 *  scroll-step: 1
 *  End:
 */
