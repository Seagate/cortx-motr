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

#ifndef __MOTR_CM_REPREB_SW_ONWIRE_FOP_H__
#define __MOTR_CM_REPREB_SW_ONWIRE_FOP_H__

#include "xcode/xcode_attr.h"
#include "rpc/rpc_opcodes.h"

#include "cm/cm.h"
#include "cm/sw.h"
#include "cm/sw_xc.h"

/**
   @defgroup XXX Repair/re-balance sliding window
   @ingroup XXX

   @{
 */

struct m0_cm_repreb_sw {
	struct m0_cm_sw_onwire swo_base;
} M0_XCA_RECORD M0_XCA_DOMAIN(rpc);

/** Initialises sliding window FOP type. */
M0_INTERNAL
void m0_cm_repreb_sw_onwire_fop_init(struct m0_fop_type *ft,
				     const struct m0_fom_type_ops *fomt_ops,
				     enum M0_RPC_OPCODES op,
				     const char *name,
				     const struct m0_xcode_type *xt,
				     uint64_t rpc_flags,
				     struct m0_cm_type *cmt);

/** Finalises sliding window FOP type. */
M0_INTERNAL void m0_cm_repreb_sw_onwire_fop_fini(struct m0_fop_type *ft);

/**
 * Allocates sliding window FOP data and initialises sliding window FOP.
 *
 * @see m0_cm_sw_onwire_init()
 */
M0_INTERNAL int
m0_cm_repreb_sw_onwire_fop_setup(struct m0_cm *cm, struct m0_fop_type *ft,
				 struct m0_fop *fop,
				 void (*fop_release)(struct m0_ref *),
				 uint64_t proxy_id, const char *local_ep,
				 const struct m0_cm_sw *sw,
				 const struct m0_cm_sw *out_interval);

extern struct m0_fop_type m0_cm_repreb_sw_fopt;

/** @} XXX */

#endif /* __MOTR_CM_REPREB_SW_ONWIRE_FOP_H__ */
/*
 *  Local variables:
 *  c-indentation-style: "K&R"
 *  c-basic-offset: 8
 *  tab-width: 8
 *  fill-column: 80
 *  scroll-step: 1
 *  End:
 */
