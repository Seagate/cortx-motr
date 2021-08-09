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

#ifndef __MOTR_LIB_BITMAP_H__
#define __MOTR_LIB_BITMAP_H__

#include "lib/types.h"
#include "lib/assert.h"
#include "xcode/xcode_attr.h"

/**
   @defgroup bitmap Bitmap
   @{
 */

/**
   An array of bits (Booleans).

   The bitmap is stored as an array of 64-bit "words"
 */
struct m0_bitmap {
	/** Number of bits in this map. */
	size_t    b_nr;
	/** Words with bits. */
	uint64_t *b_words;
} M0_XCA_SEQUENCE M0_XCA_DOMAIN(be);;

struct m0_bitmap_onwire {
	/** size of bo_words. */
	size_t    bo_size;
	/** Words with bits. */
	uint64_t *bo_words;
} M0_XCA_SEQUENCE M0_XCA_DOMAIN(conf|rpc);

/**
   Initialise a bitmap to hold nr bits. The array to store bits is
   allocated internally.

   On success, the bitmap is initialised with all bits initially
   set to false.

   @param map bitmap object to initialize
   @param nr  size of the bitmap, in bits
   @retval 0 success
   @retval !0 failure, -errno
 */
M0_INTERNAL int m0_bitmap_init(struct m0_bitmap *map, size_t nr);

/**
   Finalise the bitmap.
   All memory associated with the bitmap is released.

   @param map bitmap to finalise
 */
M0_INTERNAL void m0_bitmap_fini(struct m0_bitmap *map);

/**
   Get a bit value from a bitmap.

   @pre idx < map->b_br

   @param map bitmap to query
   @param idx bit offset in the bitmap to query
   @return the bit value, true or false.
 */
M0_INTERNAL bool m0_bitmap_get(const struct m0_bitmap *map, size_t idx);

/**
   Find first zero (a.k.a unset, false) bit from a bitmap.

   @param map bitmap to query
   @return index of the first zero bit. If no zero bit found, -1 is returned.
 */
M0_INTERNAL size_t m0_bitmap_ffz(const struct m0_bitmap *map);

/**
   Set a bit value in a bitmap.

   @param map bitmap to modify
   @param idx bit offset to modify.  Attempting to set a bit beyond the size
   of the bitmap results is not allowed (causes and assert to fail).
   @param val new bit value, true or false
 */
M0_INTERNAL void m0_bitmap_set(struct m0_bitmap *map, size_t idx, bool val);

/**
   Reset a bitmap.
 */
M0_INTERNAL void m0_bitmap_reset(struct m0_bitmap *map);

/**
   Copies the bit values from one bitmap to another.
   @param dst destination bitmap, must already be initialised.  If dst
   is larger than src, bits beyond src->b_nr are cleared in dst.
   @param src source bitmap
   @pre dst->b_nr >= src->b_nr
 */
M0_INTERNAL void m0_bitmap_copy(struct m0_bitmap *dst,
				const struct m0_bitmap *src);

/**
 * Returns the number of bits that are 'true'.
 */
M0_INTERNAL size_t m0_bitmap_set_nr(const struct m0_bitmap *map);

/**
   Initialise an on-wire bitmap to hold nr bits. The array to store bits is
   allocated internally.

   On success, the bitmap is initialised with all bits initially set to false.

   @param ow_map on-wire bitmap object to initialise
   @param nr  size of the bitmap, in bits
   @retval 0 success
   @retval !0 failure, -errno
 */
M0_INTERNAL int m0_bitmap_onwire_init(struct m0_bitmap_onwire *ow_map,
				      size_t nr);
/**
   Finalise the on-wire bitmap.
   All memory associated with the on-wire bitmap is released.

   @param ow_map on-wire bitmap to finalise
 */
M0_INTERNAL void m0_bitmap_onwire_fini(struct m0_bitmap_onwire *ow_map);

/**
   Converts in-memory struct m0_bitmap to on-wire struct m0_bitmap_onwire.

   @param im_map in-memory bitmap object to be converted into on-wire bitmap
		 object.
   @param ow_map pre-initialised on-wire bitmap object
 */
M0_INTERNAL void m0_bitmap_store(const struct m0_bitmap *im_map,
			         struct m0_bitmap_onwire *ow_map);

/**
   Converts on-wire bitmap to in-memory bitmap.

   @param ow_map on-wire bitmap object to be converted into in-memory bitmap.
   @param im_map pre-initialised in-memory bitmap object
 */
M0_INTERNAL void m0_bitmap_load(const struct m0_bitmap_onwire *ow_map,
				struct m0_bitmap *im_map);

M0_BASSERT(8 == sizeof ((struct m0_bitmap *)0)->b_words[0]);

/** @} end of bitmap group */
#endif /* __MOTR_LIB_BITMAP_H__ */

/*
 *  Local variables:
 *  c-indentation-style: "K&R"
 *  c-basic-offset: 8
 *  tab-width: 8
 *  fill-column: 80
 *  scroll-step: 1
 *  End:
 */
