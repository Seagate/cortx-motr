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

#ifndef __MOTR_HELPERS_HELPERS_H__
#define __MOTR_HELPERS_HELPERS_H__

#include "helpers/ufid.h"

/**
 * Forward declaration: m0_ufid_generator represents a UFID generator
 * which is defined in helpers/ufid.h
 */
struct m0_ufid_generator;

/**
 * Initialises Unique File/object ID (UFID) generator.
 *
 * @param  m0c The Client instance contains required process FID.
 * @param  gr  The UFID generator to be initialised.
 * @return     Returns 0 on success. Negative errno in case of error.
 */
M0_INTERNAL int m0_ufid_init(struct m0_client *m0c,
			     struct m0_ufid_generator *gr);

/**
 * Finalises Unique File/object ID (UFID) generator.
 *
 * @param  gr  The UFID generator to be finalised.
 */
M0_INTERNAL void m0_ufid_fini(struct m0_ufid_generator *gr);

/**
 * Primary interface to obtain one or more unique file/object IDs of 128 bits.
 *
 * m0_ufid_new() obtains new FID range which is unique across
 * nodes/processes.
 *
 * Input Params:
 * @param gr            Pointer to an UFID generator.
 * @param nr_ids        Number of IDs that should be allocated.
 *                      nr_ids value cannot be greater than (2^20-1).
 *
 * @param nr_skip_ids   Number of FIDs to skip. It invalidates nr_skip_ids
 *                      ID range and cannot be greater than (2^20-1).
 *
 * @param id128[Output] Starting ID of generated range.
 *
 * @return              Returns 0 on success. Negative errno in case of error.
 *
 * Error Values:
 * -EINVAL  In case of errors in parameters.
 * -ETIME   In case clock errors are detected.
 * -EPERM   In case module is not initialized before invoking this interface.
 */
int m0_ufid_new(struct m0_ufid_generator *gr, uint32_t nr_ids,
		uint32_t nr_skip_ids, struct m0_uint128 *id128);

/**
 * m0_ufid_next() is a simple wrapper of m0_ufid_new() which
 * always tries to allocate FID range starting from current available FID.
 *
 * @see m0_ufid_new() above for argument descriptions.
 */
int m0_ufid_next(struct m0_ufid_generator *gr,
		 uint32_t nr_ids, struct m0_uint128 *id128);


#endif /* __MOTR_HELPERS_HELPERS_H__ */

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
