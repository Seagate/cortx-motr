/* -*- C -*- */
/*
 * Copyright (c) 2020 Seagate Technology LLC and/or its Affiliates
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

#ifndef __MOTR_ISCSERVICE_ISC_SERVICE_H__
#define __MOTR_ISCSERVICE_ISC_SERVICE_H__

#include "reqh/reqh_service.h" /* m0_reqh_service */
#include "lib/hash.h"          /* m0_hlink */

struct m0_isc_comp_private;

/**
 * State of a computation with respect to its registration with the
 * ISC service.
 */
enum m0_isc_comp_state {
	/** If a computation is present in hash-table. */
	M0_ICS_REGISTERED,
	/** If a computation is not present in hash-table. */
	M0_ICS_UNREGISTERED,
};

/**
 * Represents a computation abstraction. This structure resides with the
 * hash table that's part of ISC service. A concurrent access to a computation
 * is handled by concurrent hash table.
 */
struct m0_isc_comp {
	/** A unique identifier for a computation. */
	struct m0_fid           ic_fid;
	/** Human readable name of the computation. */
	char                   *ic_name;
	/** A linkage in hash-table for storing computations. */
	struct m0_hlink         ic_hlink;
	/** A generation count for cookie associated with this computation. */
	uint64_t                ic_gen;
	/**
	 * A pointer to operation. The output of the function is populated
	 * in result and caller is expected to free it after usage.
	 */
	int                   (*ic_op)(struct m0_buf *args_in,
				       struct m0_buf *result,
				       struct m0_isc_comp_private *comp_data,
				       int *rc);
	/** Indicates one of the states from m0_isc_comp_state. */
	 enum m0_isc_comp_state ic_reg_state;
	/** Count for ongoing instances of the operation. */
	uint32_t                ic_ref_count;
	uint64_t                ic_magic;
};

/**
 * ISC service that resides with Motr request handler.
 */
struct m0_reqh_isc_service {
	/** Generic reqh service object */
	struct m0_reqh_service riscs_gen;
	uint64_t               riscs_magic;
};

/** Creates the hash-table of computations in m0 instance. */
M0_INTERNAL int m0_isc_mod_init(void);
M0_INTERNAL void m0_isc_mod_fini(void);

/** Returns the hash-table of computations stored with m0 instance. */
M0_INTERNAL struct m0_htable *m0_isc_htable_get(void);

M0_INTERNAL int m0_iscs_register(void);
M0_INTERNAL void m0_iscs_unregister(void);

/** Methods for hash-table holding external computations linked with Motr. */
M0_HT_DECLARE(m0_isc, M0_INTERNAL, struct m0_isc_comp, struct m0_fid);

extern struct m0_reqh_service_type m0_iscs_type;

#endif /* __MOTR_ISCSERVICE_ISC_SERVICE_H__ */
/*
 *  Local variables:
 *  c-indentation-style: "K&R"
 *  c-basic-offset: 8
 *  tab-width: 8
 *  fill-column: 80
 *  scroll-step: 1
 *  End:
 */
