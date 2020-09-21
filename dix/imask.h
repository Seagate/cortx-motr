/* -*- C -*- */
/*
 * Copyright (c) 2016-2020 Seagate Technology LLC and/or its Affiliates
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

#ifndef __MOTR_DIX_IMASK_H__
#define __MOTR_DIX_IMASK_H__

#include "lib/types.h"  /* m0_bcount_t */
#include "lib/ext_xc.h"
/**
 * @addtogroup dix
 *
 * @{
 *
 * Identity mask is a sequence of ranges of bit positions in bit-string:
 * [S0, E0], [S1, E1], ..., [Sm, Em], where Si and Ei are bit-offsets counted
 * from 0. The ranges can be overlapping and are not necessarily monotone
 * offset-wise. Range of identity mask can be infinite [X, inf] meaning
 * that range includes bit positions from X to the end of a bit-string. Empty
 * identity mask is a mask with 0 ranges defined.
 *
 * The mask can be applied to a bit-string. Applying identity mask to a
 * bit-string X produces a new bit-string Y:
 * Y = X[S0, E0] :: X[S1, E1] :: ... :: X[Sm, Em],
 * where :: is is bit-string concatenation.
 *
 * For example, given identity mask I = [0, 3], [7, 8] and X = 0xf0, result of
 * mask application is:
 * Y = 0xf0[0, 3] :: 0xf0[7, 8] = 0b0000 :: 0b11 = 0b000011 = 0x03
 *
 * It's totally fine to apply mask with ranges extending the bit-string's
 * length. For example, application of identity mask with a single range
 * [50,100] to a bit-string 0b11 is valid and produces empty bit-string.
 */

enum {
	/** Infinite value as the end of the mask's range. */
	IMASK_INF = ~0ULL
};

/** Identity mask. */
struct m0_dix_imask {
	/** Number of ranges. */
	uint64_t       im_nr;
	/** Array of ranges. */
	struct m0_ext *im_range;
} M0_XCA_SEQUENCE M0_XCA_DOMAIN(rpc);

/**
 * Initialises identity mask. Array of ranges for the mask is allocated
 * internally, so user can free 'range' array after initialisation.
 */
M0_INTERNAL int m0_dix_imask_init(struct m0_dix_imask *mask,
				  struct m0_ext       *range,
				  uint64_t             nr);

/** Finalises identity mask. */
M0_INTERNAL void m0_dix_imask_fini(struct m0_dix_imask *mask);

/**
 * Applies identity mask to a user-provided bit-string.
 * Value res should be deallocated by user with m0_free().
 */
M0_INTERNAL int m0_dix_imask_apply(void                 *buffer,
				   m0_bcount_t           buf_len_bytes,
				   struct m0_dix_imask  *mask,
				   void                **res,
				   m0_bcount_t          *res_len_bits);

/** Checks whether identity mask is empty (has 0 ranges defined). */
M0_INTERNAL bool m0_dix_imask_is_empty(const struct m0_dix_imask *mask);

/** Makes a deep copy of identity mask. */
M0_INTERNAL int m0_dix_imask_copy(struct m0_dix_imask       *dst,
				  const struct m0_dix_imask *src);

/** Checks whether imasks are equal */
M0_INTERNAL bool m0_dix_imask_eq(const struct m0_dix_imask *imask1,
				 const struct m0_dix_imask *imask2);

/** @} end of dix group */

#endif /* __MOTR_DIX_IMASK_H__ */

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
