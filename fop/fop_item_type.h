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

#ifndef __MOTR_FOP_FOP_ONWIRE_H__
#define __MOTR_FOP_FOP_ONWIRE_H__

#include "rpc/rpc_helpers.h"

struct m0_rpc_item;
struct m0_rpc_item_type;

/**
   @addtogroup fop

   This file contains definitions for encoding/decoding a fop type rpc item
   onto a bufvec.
	@see rpc/onwire.h
   @{
 */
/**
   Generic bufvec serialization routine for a fop rpc item type.
   @param item_type Pointer to the item type struct for the item.
   @param item  pointer to the item which is to be serialized.
   @param cur current position of the cursor in the bufvec.
   @retval 0 On success.
   @retval -errno On failure.
*/
M0_INTERNAL int
m0_fop_item_type_default_encode(const struct m0_rpc_item_type *item_type,
				struct m0_rpc_item *item,
				struct m0_bufvec_cursor *cur);

/**
   Generic deserialization routine for a fop rpc item type. Allocates a new rpc
   item and decodes the header and the payload into this item.
   @param item_type Pointer to the item type struct for the item.
   @param item Pointer to the item containing deserialized rpc onwire data and
   payload.
   @param cur current position of the cursor in the bufvec.
   @retval 0 On success.
   @retval -errno if failure.
*/
M0_INTERNAL int
m0_fop_item_type_default_decode(const struct m0_rpc_item_type *item_type,
				struct m0_rpc_item **item_out,
				struct m0_bufvec_cursor *cur);

/**
   Return the onwire size of the item type which is a fop in bytes.
   The onwire size of an item equals = size of (header + payload).
   @param item The rpc item for which the on wire size is to be calculated
   @retval Size of the item in bytes.
*/
M0_INTERNAL m0_bcount_t m0_fop_payload_size(const struct m0_rpc_item *item);

M0_INTERNAL int m0_fop_item_encdec(struct m0_rpc_item *item,
				   struct m0_bufvec_cursor *cur,
				   enum m0_xcode_what  what);

void m0_fop_item_get(struct m0_rpc_item *item);
void m0_fop_item_put(struct m0_rpc_item *item);

/** @} end of fop group */

/* __MOTR_FOP_FOP_ONWIRE_H__ */
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
