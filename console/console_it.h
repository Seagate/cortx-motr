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

#ifndef __MOTR_CONSOLE_IT_H__
#define __MOTR_CONSOLE_IT_H__

#include "fop/fop.h"     /* m0_fop_field_type */
#include "xcode/xcode.h" /* m0_xcode_type */

/**
   @addtogroup console_it
   @{
 */

enum m0_cons_data_process_type {
	CONS_IT_INPUT,
	CONS_IT_OUTPUT,
	CONS_IT_SHOW
};

/**
 * @struct m0_cons_atom_ops
 *
 * @brief operation to get value of ATOM type (i.e. CHAR, U64 etc).
 */
struct m0_cons_atom_ops {
	void (*catom_val_get)(const struct m0_xcode_type *xct,
			      const char *name, void *data);
	void (*catom_val_set)(const struct m0_xcode_type *xct,
			      const char *name, void *data);
	void (*catom_val_show)(const struct m0_xcode_type *xct,
			       const char *name, void *data);
};

/**
 * @brief Iterate over FOP fields and prints the names.
 *
 * @param fop fop object.
 */
M0_INTERNAL int m0_cons_fop_fields_show(struct m0_fop *fop);

/**
 * @brief Helper function for FOP input
 *
 * @param fop fop object.
 */
M0_INTERNAL int m0_cons_fop_obj_input(struct m0_fop *fop);

/**
 * @brief Helper function for FOP output.
 *
 * @param fop fop object.
 */
M0_INTERNAL int m0_cons_fop_obj_output(struct m0_fop *fop);

/** @} end of console_it */

/* __MOTR_CONSOLE_IT_H__ */
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
