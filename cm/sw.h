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

#ifndef __MOTR_CM_SW_H__
#define __MOTR_CM_SW_H__

#include "lib/types.h"

#include "cm/ag.h"
#include "cm/ag_xc.h"
#include "xcode/xcode_attr.h"
#include "fop/fop.h"

/**
   @defgroup CMSW copy machine sliding window
   @ingroup CM

   @{
 */

struct m0_rpc_conn;
struct m0_cm_type;

struct m0_cm_sw {
	struct m0_cm_ag_id     sw_lo;
	struct m0_cm_ag_id     sw_hi;
	uint64_t               sw_nr_bufs;
} M0_XCA_RECORD M0_XCA_DOMAIN(rpc);

/** Copy machine replica's local endpoint. */
struct m0_cm_local_ep {
	uint32_t  ep_size;
	char     *ep;
} M0_XCA_SEQUENCE M0_XCA_DOMAIN(rpc);

/**
 * Copy machine's sliding window update to be sent to a
 * remote copy machine replica.
 */
struct m0_cm_sw_onwire {
	/** Beginning of copy machine operation. */
	m0_time_t             swo_cm_epoch;

	uint64_t              swo_sender_id;

	/** Replica's local endpoint. */
	struct m0_cm_local_ep swo_cm_ep;

	/** Sender copy machine's [hi, lo] from incoming aggregation groups. */
	struct m0_cm_sw       swo_in_interval;

	/** Sender copy machine's [hi, lo] from outgoing aggregation groups. */
	struct m0_cm_sw       swo_out_interval;

	uint32_t              swo_cm_status;
} M0_XCA_RECORD M0_XCA_DOMAIN(rpc);

struct m0_cm_sw_onwire_rep {
	int swr_rc;
} M0_XCA_RECORD M0_XCA_DOMAIN(rpc);

struct m0_cm_sw_update {
	struct m0_fom    swu_fom;
	struct m0_cm_sw  swu_sw;
	bool             swu_is_complete;
};

M0_INTERNAL int m0_cm_sw_onwire_init(struct m0_cm *cm, struct m0_cm_sw_onwire *sw_onwire,
				     uint64_t proxy_id, const char *ep,
				     const struct m0_cm_sw *sw,
				     const struct m0_cm_sw *out_interval);

M0_INTERNAL void m0_cm_sw_set(struct m0_cm_sw *dst,
			      const struct m0_cm_ag_id *lo,
			      const struct m0_cm_ag_id *hi);

M0_INTERNAL void m0_cm_sw_copy(struct m0_cm_sw *dst,
			       const struct m0_cm_sw *src);

M0_INTERNAL bool m0_cm_sw_is_set(const struct m0_cm_sw *sw);
M0_INTERNAL bool m0_cm_sw_cmp(const struct m0_cm_sw *sw0,
			      const struct m0_cm_sw *sw1);
/**
 * Updates local sliding window, creates new aggregation groups as many as
 * possible and adds them to the sliding window.
 * This is invoked from sliding window update fom.
 * See @ref CMSWFOM "sliding window update fom" for more details.
 */
M0_INTERNAL int m0_cm_sw_local_update(struct m0_cm *cm);

/**
 * Sends local sliding window updates to remote replicas.
 * This is invoked from m0_cm_ready(), once the local copy machine sliding
 * window is updated.
 * See @ref CMPROXY "copy machine proxy" for more details.
 */
M0_INTERNAL int m0_cm_sw_remote_update(struct m0_cm *cm);

M0_INTERNAL void m0_cm_sw_update_init(struct m0_cm_type *cmtype);

/**
 * Starts sliding window update FOM by submitting the corresponding FOM to
 * request handler.
 */
M0_INTERNAL void m0_cm_sw_update_start(struct m0_cm *cm);
M0_INTERNAL void m0_cm_sw_update_complete(struct m0_cm *cm);

M0_INTERNAL void m0_cm_sw_update_fom_wakeup(struct m0_cm_sw_update *swu);

/** @} CMSW */

#endif /* __MOTR_CM_SW_H__ */
/*
 *  Local variables:
 *  c-indentation-style: "K&R"
 *  c-basic-offset: 8
 *  tab-width: 8
 *  fill-column: 80
 *  scroll-step: 1
 *  End:
 */
