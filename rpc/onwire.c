/* -*- C -*- */
/*
 * Copyright (c) 2012-2020 Seagate Technology LLC and/or its Affiliates
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


#define M0_TRACE_SUBSYSTEM M0_TRACE_SUBSYS_RPC
#include "lib/trace.h"

#include "lib/errno.h"
#include "rpc/onwire.h"
#include "rpc/onwire_xc.h"
#include "rpc/item.h"          /* m0_rpc_item_header2 */
#include "rpc/rpc_helpers.h"
#include "xcode/xcode.h"       /* M0_XCODE_OBJ */

/**
 * @addtogroup rpc
 * @{
 */

#define ITEM_HEAD1_XCODE_OBJ(ptr) M0_XCODE_OBJ(m0_rpc_item_header1_xc, ptr)
#define ITEM_HEAD2_XCODE_OBJ(ptr) M0_XCODE_OBJ(m0_rpc_item_header2_xc, ptr)
#define ITEM_FOOTER_XCODE_OBJ(ptr) M0_XCODE_OBJ(m0_rpc_item_footer_xc, ptr)

M0_INTERNAL int m0_rpc_item_header1_encdec(struct m0_rpc_item_header1 *ioh,
					   struct m0_bufvec_cursor    *cur,
					   enum m0_xcode_what          what)
{
	M0_ENTRY("item header1: %p", ioh);
	return M0_RC(m0_xcode_encdec(&ITEM_HEAD1_XCODE_OBJ(ioh), cur, what));
}

M0_INTERNAL int m0_rpc_item_header2_encdec(struct m0_rpc_item_header2 *ioh,
					   struct m0_bufvec_cursor    *cur,
					   enum m0_xcode_what          what)
{
	M0_ENTRY("item header2: %p", ioh);
	return M0_RC(m0_xcode_encdec(&ITEM_HEAD2_XCODE_OBJ(ioh), cur, what));
}

M0_INTERNAL int m0_rpc_item_footer_encdec(struct m0_rpc_item_footer *iof,
					  struct m0_bufvec_cursor   *cur,
					  enum m0_xcode_what         what)
{
	M0_ENTRY("item footer: %p", iof);
	return M0_RC(m0_xcode_encdec(&ITEM_FOOTER_XCODE_OBJ(iof), cur, what));
}

#undef M0_TRACE_SUBSYSTEM

/** @} */

/*
 *  Local variables:
 *  c-indentation-style: "K&R"
 *  c-basic-offset: 8
 *  tab-width: 8
 *  fill-column: 79
 *  scroll-step: 1
 *  End:
 */
