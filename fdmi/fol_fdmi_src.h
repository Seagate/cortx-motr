/* -*- C -*- */
/*
 * Copyright (c) 2017-2020 Seagate Technology LLC and/or its Affiliates
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

#ifndef __MOTR_FOL_FDMI_SRC_H__
#define __MOTR_FOL_FDMI_SRC_H__

#include "fdmi/source_dock.h"

/* Forward declarations */
struct m0_fom;

/**
 * @defgroup fdmi_fol_src FDMI FOL source
 * @ingroup fdmi_main
 * @see @ref FDMI-DLD-fspec "FDMI Functional Specification"
 * @{
 */

struct ffs_fol_frag_handler {

	uint32_t ffh_opecode;

	int (*ffh_fol_frag_get_val)(void *fol_data,
				    struct m0_fdmi_flt_var_node *value_desc,
				    struct m0_fdmi_flt_operand  *value);

};

/** FOL source internal context. */
struct m0_fol_fdmi_src_ctx {
	/** Holds M0_FOL_FDMI_SRC_CTX_MAGIC. */
	uint64_t                        ffsc_magic;

	/** Source registration record  */
	struct m0_fdmi_src             *ffsc_src;

	/* FOL specific data */

	/** FOL frag handlers -- array of handler descriptors. */
	struct ffs_fol_frag_handler    *ffsc_frag_handler_vector;

	/** Count of handlers in ffsc_frag_handler_vector. */
	uint32_t                        ffsc_handler_number;
};

/**
 * Initializes/registers FOL FDMI source.
 */
M0_INTERNAL int m0_fol_fdmi_src_init(void);

/**
 * Deinitializes FOL FDMI source.
 *
 * Same as m0_fol_fdmi_src_deinit, but suppresses retcode.  Needed for
 * motr/init.c table.
 */
M0_INTERNAL void m0_fol_fdmi_src_fini(void);

/**
 * Deinitializes FOL FDMI source.
 */
M0_INTERNAL int m0_fol_fdmi_src_deinit(void);

/** Submit new FOL entry to FDMI. */
M0_INTERNAL void m0_fol_fdmi_post_record(struct m0_fom *fom);

/** @} group fdmi_fol_src */

#endif /* __MOTR_FOL_FDMI_SRC_H__ */

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
