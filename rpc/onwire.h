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


#pragma once

#ifndef __MOTR_RPC_ONWIRE_H__
#define __MOTR_RPC_ONWIRE_H__

#include "lib/types.h"        /* uint64_t */
#include "lib/types_xc.h"     /* m0_uint128_xc */
#include "lib/cookie.h"
#include "lib/cookie_xc.h"
#include "xcode/xcode_attr.h" /* M0_XCA_RECORD */
#include "format/format.h"    /* struct m0_format_header */
#include "format/format_xc.h"

/**
 * @addtogroup rpc
 * @{
 */

enum {
	M0_RPC_VERSION_1 = 1,
};

enum {
	/*
	 * Version of RPC packet
	 */

	M0_RPC_PACKET_FORMAT_VERSION_1 = 1,

	/* future versions, uncomment and update M0_RPC_PACKET_FORMAT_VERSION */
	/*M0_RPC_PACKET_FORMAT_VERSION_2,*/
	/*M0_RPC_PACKET_FORMAT_VERSION_3,*/

	/**
	 * Current version, should point to the latest
	 * M0_RPC_PACKET_FORMAT_VERSION_*
	 */
	M0_RPC_PACKET_FORMAT_VERSION = M0_RPC_PACKET_FORMAT_VERSION_1,

	/*
	 * Version of RPC item
	 */

	M0_RPC_ITEM_FORMAT_VERSION_1 = 1,
	M0_RPC_ITEM_FORMAT_VERSION   = M0_RPC_ITEM_FORMAT_VERSION_1,
};

struct m0_rpc_packet_onwire_header {
	struct m0_format_header poh_header;
	/* Version */
	uint32_t                poh_version;
	/** Number of RPC items in packet */
	uint32_t                poh_nr_items;
	uint64_t                poh_magic;
} M0_XCA_RECORD M0_XCA_DOMAIN(rpc);

struct m0_rpc_packet_onwire_footer {
	struct m0_format_footer pof_footer;
} M0_XCA_RECORD M0_XCA_DOMAIN(rpc);

struct m0_rpc_item_header1 {
	struct m0_format_header ioh_header;
	/** Item opcode */
	uint32_t                ioh_opcode;
	/** Item flags, taken from enum m0_rpc_item_flags. */
	uint32_t                ioh_flags;
	/** HA epoch transferred by the item. */
	uint64_t                ioh_ha_epoch;
	uint64_t                ioh_magic;
} M0_XCA_RECORD M0_XCA_DOMAIN(rpc);

struct m0_rpc_item_header2 {
	struct m0_uint128 osr_uuid;
	uint64_t          osr_sender_id;
	uint64_t          osr_session_id;
	/**
	 * The sender will never send rpc items in this session with osr_xid
	 * which is less than this value.
	 *
	 * @see m0_rpc_session::s_xid_list, m0_rpc_item_xid_min_update().
	 */
	uint64_t          osr_session_xid_min;
	uint64_t          osr_xid;
	struct m0_cookie  osr_cookie;
} M0_XCA_RECORD M0_XCA_DOMAIN(rpc);

struct m0_rpc_item_footer {
	struct m0_format_footer iof_footer;
} M0_XCA_RECORD M0_XCA_DOMAIN(rpc);

M0_INTERNAL int m0_rpc_item_header1_encdec(struct m0_rpc_item_header1 *ioh,
					   struct m0_bufvec_cursor *cur,
					   enum m0_xcode_what what);
M0_INTERNAL int m0_rpc_item_footer_encdec (struct m0_rpc_item_footer *iof,
					   struct m0_bufvec_cursor *cur,
					   enum m0_xcode_what what);

/** @}  End of rpc group */

#endif /* __MOTR_RPC_ONWIRE_H__ */
