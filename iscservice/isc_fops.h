/* -*- C -*- */
/*
 * Copyright (c) 2020 Seagate Technology LLC and/or its Affiliates
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

#ifndef __MOTR_RPC_ISC_FOPS_H__
#define __MOTR_RPC_ISC_FOPS_H__

#include "fop/fop.h"
#include "rpc/rpc_opcodes.h"
#include "lib/types.h"
#include "xcode/xcode_attr.h"
#include "rpc/at.h"          /* m0_rpc_at_buf */
#include "rpc/at_xc.h"       /* m0_rpc_at_buf_xc */


M0_INTERNAL int m0_iscservice_fop_init(void);
M0_INTERNAL void m0_iscservice_fop_fini(void);

/** Registers fom types of fop-less foms. */
M0_INTERNAL void m0_isc_fom_type_init(void);

/**
 * FOP definitions and corresponding fop type formats
 */
extern struct m0_fop_type m0_fop_isc_fopt;
extern struct m0_fop_type m0_fop_isc_rep_fopt;

extern const struct m0_fop_type_ops m0_fop_isc_ops;
extern const struct m0_fop_type_ops m0_fop_isc_rep_ops;

extern const struct m0_rpc_item_type m0_rpc_item_type_isc;
extern const struct m0_rpc_item_type m0_rpc_item_type_isc_rep;

/** A fop for the ISC service */
struct m0_fop_isc {
	/** An identifier of the computation registered with the service. */
	struct m0_fid        fi_comp_id;
	/**
	 * An array holding the relevant arguments for the computation.
	 * This might involve gfid, cob fid, and few other parameters
	 * relevant to the required computation.
	 */
	struct m0_rpc_at_buf fi_args;
	/**
	 * An rpc AT buffer requesting the output of computation.
	 */
	struct m0_rpc_at_buf fi_ret;
	/** A cookie for fast searching of a computation. */
	struct m0_cookie     fi_comp_cookie;
} M0_XCA_RECORD M0_XCA_DOMAIN(rpc);

/** Reply FOPs for the ISC service */
struct m0_fop_isc_rep {
	/** Return code of the execution of the computation. */
	int32_t              fir_rc;
	/** Cookie associated with the computation. */
	struct m0_cookie     fir_comp_cookie;
	/** RPC adaptive buffers associated with the reply  */
	struct m0_rpc_at_buf fir_ret;
} M0_XCA_RECORD M0_XCA_DOMAIN(rpc);

/* __MOTR_RPC_ISC_FOPS_H__ */
#endif

/*
 *  Local variables:
 *  c-indentation-style: "K&R"
 *  c-basic-offset: 8
 *  tab-width: 8
 *  fill-column: 80
 *  scroll-step: 1
 *  End:
 */
