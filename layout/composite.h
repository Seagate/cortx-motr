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

#ifndef __MOTR_LAYOUT_COMPOSITE_H__
#define __MOTR_LAYOUT_COMPOSITE_H__

/**
 * @defgroup composite Composite Layout Type.
 *
 * Composite layout. Composite layout is made up of multiple sub-layouts. Each
 * sub-layout needs to be read to obtain the overall layout details providing
 * all the COB identifiers.
 *
 * @{
 */

/* import */
#include "be/extmap.h"	    /* struct m0_be_emap */
#include "layout/layout.h"

/* export */
struct m0_composite_layout;

/**
 * Extension of the generic m0_layout for the composite layout type.
 */
struct m0_composite_layout {
	/** Super class. */
	struct m0_layout  cl_base;

	/** List of sub-layouts owned by this composite layout. */
	struct m0_tl      cl_sub_layouts;
};

M0_INTERNAL void m0_composite_build(struct m0_layout_domain *dom,
				    uint64_t lid,
				    struct m0_tl *sub_layouts,
				    struct m0_composite_layout **out);

extern const struct m0_layout_type m0_composite_layout_type;

/** @} end group composite */

/* __MOTR_LAYOUT_COMPOSITE_H__ */
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
