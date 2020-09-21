/* -*- C -*- */
/*
 * Copyright (c) 2013-2020 Seagate Technology LLC and/or its Affiliates
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

#ifndef __MOTR_FILE_H__
#define __MOTR_FILE_H__

#include "lib/errno.h"
#include "lib/misc.h"
#include "lib/memory.h"
#include "fid/fid.h"
#include "rm/rm.h"
#include "rm/rm_internal.h"
#include "file/di.h"

/**
   @page FileLock Distributed File Lock DLD

   - @ref FileLockDLD-fspec-ds
   - @ref FileLockDLD-fspec-sub

   @section FileLockDLD-fspec Functional Specification
   This section describes the data structure and the external interfaces of
   the distributed file lock implemented using resource manager.

   @section FileLockDLD-fspec-ds Data Structures

   The distributed mutex will have the following data structure:
   - m0_file
     This holds generic RM resource, fid and data-integrity operations
     supported for this file.

   @section FileLockDLD-fspec-sub Subroutines

   The asynchronous distributed file lock provides the functions listed in
   in the sub-sections below:

   @subsection FileLockDLD-fspec-sub-cons Constructors and Destructors

   @subsection FileLockDLD-fspec-sub-opi Operational Interfaces
   - m0_file_lock()
   - m0_file_unlock()
   - m0_file_lock_type_register()
   - m0_file_lock_type_deregister()
   - m0_file_owner_init()
   - m0_file_owner_fini()

 */

/**
   @defgroup FileLock Distributed File Lock
   @ingroup rm

   @see rm
   @ref FileLockDLD-fspec "Functional Specification"

   @{
 */

/** File. */
struct m0_file {
	/**
	 * File identifier.
	 *
	 * This points to the fid stored in an ambient structure, for example,
	 * m0t1fs_inode::ci_fid.
	 */
	const struct m0_fid    *fi_fid;
	/** Embedded resource */
	struct m0_rm_resource   fi_res;
	/* Data-integrity operations supported by this file. */
	const struct m0_di_ops *fi_di_ops;
};

/**
 * Initialises a file-lock resource.
 *
 * @param file - a resource
 * @param fid - fid for the file-lock
 * @param dom - RM domain
 * @param di_type - di operations to be supported for this file
 */
M0_INTERNAL void m0_file_init(struct m0_file *file,
			      const struct m0_fid *fid,
			      struct m0_rm_domain *dom,
			      enum m0_di_types	   di_type);

/**
 * Finalises the file-lock resource.
 */
M0_INTERNAL void m0_file_fini(struct m0_file *file);

/**
 * Initialises owner for a file-lock resource.
 *
 * @param file - a resource
 * @param owner - Owner for file
 * @param creditor - Creditor for this owner
 */
M0_INTERNAL void m0_file_owner_init(struct m0_rm_owner      *owner,
				    const struct m0_uint128 *grp_id,
				    struct m0_file          *file,
				    struct m0_rm_remote     *creditor);

/**
 * Finalises the owner of file-lock
 */
M0_INTERNAL void m0_file_owner_fini(struct m0_rm_owner *owner);

/**
 * Initiates acquiring a distributed file lock request. A caller can
 * decide to wait, if necessary.
 */
M0_INTERNAL void m0_file_lock(struct m0_rm_owner *owner,
			      struct m0_rm_incoming *req);

/**
 * Unlocks the distributed file lock.
 */
M0_INTERNAL void m0_file_unlock(struct m0_rm_incoming *req);

/**
 * Registers the resource of type 'distributed mutex' with a resource domain.
 */
M0_INTERNAL
int m0_file_lock_type_register(struct m0_rm_domain *dom,
			       struct m0_rm_resource_type *flock_rt);

/**
 * De-registers the resource of type 'distributed mutex' from a resource domain.
 */
M0_INTERNAL
void m0_file_lock_type_deregister(struct m0_rm_resource_type *flock_rt);

extern const struct m0_fid_type m0_file_fid_type;

/**
   Module initializer.
 */
M0_INTERNAL int m0_file_mod_init(void);

/**
   Module finalizer.
 */
M0_INTERNAL void m0_file_mod_fini(void);

/** @} end of FileLock */

#endif /*  __MOTR_FILE_H__ */

/*
 *  Local variables:
 *  c-indentation-style: "K&R"
 *  c-basic-offset: 8
 *  tab-width: 8
 *  fill-column: 79
 *  scroll-step: 1
 *  End:
 */
