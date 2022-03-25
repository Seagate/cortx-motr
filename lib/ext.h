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

#ifndef __MOTR_LIB_EXT_H__
#define __MOTR_LIB_EXT_H__

#include "format/format.h"     /* m0_format_header */
#include "format/format_xc.h"

/**
   @defgroup ext Extent
   @{
 */

/** extent [ e_start, e_end ) */
struct m0_ext {
	struct m0_format_header e_header;
	m0_bindex_t             e_start;
	m0_bindex_t             e_end;
	struct m0_format_footer e_footer;
} M0_XCA_RECORD M0_XCA_DOMAIN(be|rpc);

enum m0_ext_format_version {
	M0_EXT_FORMAT_VERSION_1 = 1,

	/* future versions, uncomment and update M0_EXT_FORMAT_VERSION */
	/*M0_EXT_FORMAT_VERSION_2,*/
	/*M0_EXT_FORMAT_VERSION_3,*/

	/** Current version, should point to the latest version present */
	M0_EXT_FORMAT_VERSION = M0_EXT_FORMAT_VERSION_1
};

#define M0_EXT(start, end) \
	((struct m0_ext){ .e_start = (start), .e_end = (end) })

M0_INTERNAL void m0_ext_init(struct m0_ext *ext);
M0_INTERNAL m0_bcount_t m0_ext_length(const struct m0_ext *ext);
M0_INTERNAL bool m0_ext_is_in(const struct m0_ext *ext, m0_bindex_t index);

M0_INTERNAL bool m0_ext_are_overlapping(const struct m0_ext *e0,
					const struct m0_ext *e1);
M0_INTERNAL bool m0_ext_is_partof(const struct m0_ext *super,
				  const struct m0_ext *sub);
M0_INTERNAL bool m0_ext_equal(const struct m0_ext *a, const struct m0_ext *b);
M0_INTERNAL bool m0_ext_is_empty(const struct m0_ext *ext);
M0_INTERNAL void m0_ext_intersection(const struct m0_ext *e0,
				     const struct m0_ext *e1,
				     struct m0_ext *result);
/* must work correctly when minuend == difference */
M0_INTERNAL void m0_ext_sub(const struct m0_ext *minuend,
			    const struct m0_ext *subtrahend,
			    struct m0_ext *difference);
/* must work correctly when sum == either of terms. */
M0_INTERNAL void m0_ext_add(const struct m0_ext *term0,
			    const struct m0_ext *term1, struct m0_ext *sum);

/* what about signed? */
M0_INTERNAL m0_bindex_t m0_ext_cap(const struct m0_ext *ext2, m0_bindex_t val);

/** Tells if start of extent is less than end of extent. */
M0_INTERNAL bool m0_ext_is_valid(const struct m0_ext *ext);

#define EXT_F  "[%" PRIx64 ", %" PRIx64 ")"
#define EXT_P(x)  (x)->e_start, (x)->e_end

/** @} end of ext group */
#endif /* __MOTR_LIB_EXT_H__ */

/*
 *  Local variables:
 *  c-indentation-style: "K&R"
 *  c-basic-offset: 8
 *  tab-width: 8
 *  fill-column: 80
 *  scroll-step: 1
 *  End:
 */
