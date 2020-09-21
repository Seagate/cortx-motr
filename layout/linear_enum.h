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

#ifndef __MOTR_LAYOUT_LINEAR_ENUM_H__
#define __MOTR_LAYOUT_LINEAR_ENUM_H__

/**
 * @defgroup linear_enum Linear Enumeration Type.
 *
 * A layout with linear enumeration type stores a linear formula that is
 * used to enumerate all the component object identifiers.
 *
 * @{
 */

/* import */
#include "lib/arith.h" /* M0_IS_8ALIGNED */
#include "layout/layout.h"

/* export */
struct m0_layout_linear_enum;

/**
 * Attributes specific to Linear enumeration type.
 * These attributes are part of m0_layout_linear_enum which is in-memory layout
 * enumeration object and are stored in the Layout DB as well.
 *
 * @note This structure needs to be maintained as 8 bytes aligned.
 */
struct m0_layout_linear_attr {
	/** Number of elements present in the enumeration. */
	uint32_t   lla_nr;

	/** Constant A used in the linear equation A + idx * B. */
	uint32_t   lla_A;

	/** Constant B used in the linear equation A + idx * B. */
	uint32_t   lla_B;

	/** Padding to make the structure 8 bytes aligned. */
	uint32_t   lla_pad;
};
M0_BASSERT(M0_IS_8ALIGNED(sizeof(struct m0_layout_linear_attr)));

/** Extension of the generic m0_layout_enum for the linear enumeration type. */
struct m0_layout_linear_enum {
	/** Super class. */
	struct m0_layout_enum        lle_base;

	struct m0_layout_linear_attr lle_attr;

	uint64_t                     lla_magic;
};

/**
 * Allocates and builds linear enumeration object.
 * @post ergo(rc == 0, linear_invariant_internal(lin_enum))
 *
 * @note Enum object is not to be finalised explicitly by the user. It is
 * finalised internally through m0_layout__striped_fini().
 */
M0_INTERNAL int m0_linear_enum_build(struct m0_layout_domain *dom,
				     const struct m0_layout_linear_attr *attr,
				     struct m0_layout_linear_enum **out);

extern struct m0_layout_enum_type m0_linear_enum_type;

/** @} end group linear_enum */

/* __MOTR_LAYOUT_LINEAR_ENUM_H__ */
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
