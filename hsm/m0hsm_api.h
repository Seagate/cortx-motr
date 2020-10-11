/*
 * Copyright (c) 2018-2020 Seagate Technology LLC and/or its Affiliates
 * COPYRIGHT 2017-2018 CEA[1] and SAGE partners
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
 * [1]Commissariat a l'energie atomique et aux energies alternatives
 *
 * Original author: Thomas Leibovici <thomas.leibovici@cea.fr>
 */

#pragma once

#ifndef __MOTR_HSM_M0HSM_API_H__
#define __MOTR_HSM_M0HSM_API_H__

#include <stdio.h>
#include <stdint.h>

#include "motr/client.h"

/*
 * Before using HSM calls, application should initialize Motr client API
 * with DIX index enabled (see m0_client_init()). It should then pass the
 * created client context to HSM by using m0hsm_init().
 */

/** level of HSM traces */
enum hsm_log_level {
	LOG_NONE = 0,
	LOG_ERROR,
	LOG_INFO,
	LOG_VERB,
	LOG_DEBUG
};

struct m0hsm_options {
	/** level of traces (default LOG_INFO) */
	enum hsm_log_level trace_level;
	/** timeout for client operations in seconds (default 10) */
	time_t		   op_timeout;
	/** log stream (default stderr) */
	FILE		  *log_stream;
	/** rc-file with config params */
	FILE		  *rcfile;
};

/** Max Object Store I/O buffer size */
enum {MAX_M0_BUFSZ = 128*1024*1024};

/** Max allowed tier index (UINT8_MAX is reserved for internal use) */
#define HSM_TIER_MAX	(UINT8_MAX - 1)

/**
 * Initialize HSM API.
 *
 * @param options       HSM options. NULL to keep defaults.
 */
int m0hsm_init(struct m0_client *instance, struct m0_realm *uber_realm,
	       const struct m0hsm_options *options);

/**
 * Create an object to be managed by HSM.
 * @param id		Identifier of the object to be created.
 * @param obj		Pointer to the created object.
 * @param tier_idx	Index of the target tier (0 is the top tier).
 * @param keep_open	Keep the object entity opened.
 */
int m0hsm_create(struct m0_uint128 id, struct m0_obj *obj,
		 uint8_t tier_idx, bool keep_open);

/**
 * [WORKAROUND CALL] This call fixes a miss in composite
 * layout management i.e. create a associated read extent
 * when a data is written.
 *
 * Write data to an object.
 * Object's entity must be opened prior to calling this.
 * offset and length must respect usual m0 requirements
 * of alignment (PAGE_SIZE minimum).
 * @param obj	  Object to write data to.
 * @param iov	  Data buffer.
 * @param off     Start offset.
 */
int m0hsm_pwrite(struct m0_obj *obj, void *buf, size_t len, off_t off);

/**
 * Selects the target tier for next write operations.
 * This does not move existing data.
 * @param obj Object for which we want to change the write tier.
 */
int m0hsm_set_write_tier(struct m0_uint128 id, uint8_t tier_idx);


/* -------------- HSM low-level calls ------------ */

/** copy options */
enum hsm_cp_flags {
	HSM_MOVE          = (1 << 0), /**< Remove source extent after copy
					   (default: leave it on the source
					   tier) */
	HSM_KEEP_OLD_VERS = (1 << 1), /**< Preserve previous versions of copied
					   extent on the target tier
					   (default: only keep the latest
					   version). */
	HSM_WRITE_TO_DEST = (1 << 2), /**< Indicate the target tier becomes
					   the preferred tier for next write
					   operations.
					   For example, when data needs fast
					   write access after staging. Or to
					   direct next writes to a slower tier
					   after archiving.
					   Default is to keep writing in the
					   current tier. */
};

/**
 * Copy a region of an object from one tier to another.
 * This is transparent to applications if they use the I/O
 * calls defined above (m0hsm_create/pread/pwrite).
 * @param obj_id	Id of the object to be copied.
 * @param src_tier_idx	Source tier index (0 is the top tier).
 * @param tgt_tier_idx	Target tier index (0 is the top tier).
 * @param offset	Start offset of the region to be copied.
 * @param length	Size of the region to be copied.
 * @param flags		Set of OR'ed hsm_cp_flags.
 */
int m0hsm_copy(struct m0_uint128 obj_id, uint8_t src_tier_idx,
	      uint8_t tgt_tier_idx, off_t offset, size_t length,
	      enum hsm_cp_flags flags);

/** release options */
enum hsm_rls_flags {
	HSM_KEEP_LATEST = (1 << 0), /**< Release all data versions in the given
					 tier except the freshest version.
					 This makes it possible to free disk
					 space from old data versions in
					 the use-case of versioning (e.g.
					 copy made with 'HSM_KEEP_OLD_VERS'
					 flag). */
};

/**
 * Release a region of an object from the given tier.
 * @param obj_id	Id of the object to be released.
 * @param tier_idx	Tier to drop the data from.
 * @param offset	Start offset of the region to be released.
 * @param length	Size of the region to be released.
 * @param flags		Set of OR'ed hsm_rls_flags.
 */
int m0hsm_release(struct m0_uint128 obj_id, uint8_t tier_idx,
	         off_t offset, size_t length, enum hsm_rls_flags flags);

/* -------------- HSM higer-level calls ------------ */

/**
 * Request to stage data to a faster tier.
 * @param obj_id	Id of the object to be staged.
 * @param targer_tier	Target tier index (0 is the top tier).
 * @param offset	Start offset of the region to be staged.
 * @param length	Size of the region to be staged.
 * @param flags	HSM_WRITE_TO_DEST if next write operations should also
 *              be directed to the same fast tier (intensive writes expected).
 */
int m0hsm_stage(struct m0_uint128 obj_id, uint8_t target_tier,
		off_t offset, size_t length, enum hsm_cp_flags flags);

/**
 * Request to archive data to a slower tier.
 * @param obj_id	Id of the object to be archived.
 * @param targer_tier	Target tier index (0 is the top tier).
 * @param offset	Start offset of the region to be staged.
 * @param length	Size of the region to be staged.
 * @param flags		Set of OR'ed hsm_cp_flags.
 */
int m0hsm_archive(struct m0_uint128 obj_id, uint8_t target_tier,
		  off_t offset, size_t length, enum hsm_cp_flags flags);

/**
 * Release a region of an object from multiple tiers.
 * All data in tiers up to max_tier are released.
 * @param obj_id	Id of the object to be archived.
 * @param targer_tier	Target tier index (0 is the top tier).
 * @param offset	Start offset of the region to be staged.
 * @param length	Size of the region to be staged.
 * @param flags		Set of OR'ed hsm_rls_flags.
 */
int m0hsm_multi_release(struct m0_uint128 obj_id, uint8_t max_tier,
			off_t offset, size_t length, enum hsm_rls_flags flags);


/**
 * Dump HSM information about a composite object.
 * @param stream   FILE* to write information to.
 * @param id	   Identifier of the object to dump.
 * @param details  If true dump all details, else print brief output.
 */
int m0hsm_dump(FILE *stream, struct m0_uint128 id, bool details);

#endif /* __M0HSM_API_H__ */
/*
 * vim: tabstop=8 shiftwidth=8 noexpandtab textwidth=80 nowrap
 */
