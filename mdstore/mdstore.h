/* -*- C -*- */
/*
 * Copyright (c) 2011-2020 Seagate Technology LLC and/or its Affiliates
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

#ifndef __MOTR_MDSTORE_MDSTORE_H__
#define __MOTR_MDSTORE_MDSTORE_H__

#include "cob/cob.h"

struct m0_sm_group;
struct m0_cob_domain_id;
struct m0_be_tx;
struct m0_fid;
struct m0_fop;
struct m0_cob;

/**
   @defgroup mdstore Meta-data Store
   @{
 */

/** Maximal name len during readdir */
#define M0_MD_MAX_NAME_LEN    256

struct m0_statfs {
        uint64_t              sf_type;
        uint32_t              sf_bsize;
        uint64_t              sf_blocks;
        uint64_t              sf_bfree;
        uint64_t              sf_bavail;
        uint64_t              sf_files;
        uint64_t              sf_ffree;
        uint32_t              sf_namelen;
        struct m0_fid         sf_root;
};

struct m0_mdstore {
	struct m0_cob_domain *md_dom;
	struct m0_cob        *md_root;
};

/**
 * Flags supplied to m0_mdstore_locate() to point out where a cob
 * should be found: on store, in opened files table or orhans table.
 */
enum m0_mdstore_locate_flags {
        /** Find cob on store. */
        M0_MD_LOCATE_STORED  = 1 << 0,
        /** Find cob in opened cobs table. */
        M0_MD_LOCATE_OPENED  = 1 << 1,
        /** Find cob in orphans table. */
        M0_MD_LOCATE_ORPHAN  = 1 << 2
};

typedef enum m0_mdstore_locate_flags m0_mdstore_locate_flags_t;

/**
 * Populate @statfs with storage data such as free files, etc.
 */
M0_INTERNAL int m0_mdstore_statfs(struct m0_mdstore      *md,
                                  struct m0_statfs       *statfs);

/**
 * Init mdstore and get it ready to work. If init_root == !0
 * then root cob is initialized.
 */
M0_INTERNAL int m0_mdstore_init(struct m0_mdstore *md,
				struct m0_be_seg  *db,
				bool               init_root);

/**
 * Finalize mdstore instance.
 */
M0_INTERNAL void m0_mdstore_fini(struct m0_mdstore *md);

/**
 * Creates and initialises mdstore.
 */
M0_INTERNAL int m0_mdstore_create(struct m0_mdstore       *md,
				  struct m0_sm_group      *grp,
				  struct m0_cob_domain_id *id,
				  struct m0_be_domain     *bedom,
				  struct m0_be_seg        *db);

/**
 * Finalises and destroys mdstore.
 */
M0_INTERNAL int m0_mdstore_destroy(struct m0_mdstore   *md,
				   struct m0_sm_group  *grp,
				   struct m0_be_domain *bedom);

/**
 * Handle link operation described by @pfid and @name. Input
 * cob is so called statdata cob and returned by m0_cob_locate().
 * Error code is returned in error case or zero otherwise.
 */
M0_INTERNAL int m0_mdstore_link(struct m0_mdstore       *md,
				struct m0_fid           *pfid,
				struct m0_cob           *cob,
				struct m0_buf           *name,
				struct m0_be_tx         *tx);

/**
 * Handle unlink operation described by @pfid and @name. Input
 * cob is so called statdata cob and returned by m0_cob_locate().
 * Error code is returned in error case or zero otherwise.
 */
M0_INTERNAL int m0_mdstore_unlink(struct m0_mdstore     *md,
				  struct m0_fid         *pfid,
				  struct m0_cob         *cob,
				  struct m0_buf         *name,
				  struct m0_be_tx       *tx);

/**
 * Handle rename operation described by params. Input cobs are
 * statdata cobs and returned by m0_cob_locate(). Rest of the
 * arguments are self explanatory.
 *
 * Error code is returned in error case or zero otherwise.
 */
M0_INTERNAL int m0_mdstore_rename(struct m0_mdstore     *md,
				  struct m0_fid         *pfid_tgt,
				  struct m0_fid         *pfid_src,
				  struct m0_cob         *cob_tgt,
				  struct m0_cob         *cob_src,
				  struct m0_buf         *tname,
				  struct m0_buf         *sname,
				  struct m0_be_tx       *tx);

/**
 * Handle create operation described by @attr on @cob. Input @cob
 * is returned by m0_cob_alloc().
 *
 * Error code is returned in error case or zero otherwise.
 */
M0_INTERNAL int m0_mdstore_fcreate(struct m0_mdstore     *md,
				  struct m0_fid         *pfid,
				  struct m0_cob_attr    *attr,
				  struct m0_cob        **out,
				  struct m0_be_tx       *tx);

/**
 * Handle open operation described by @flags on @cob. Input @cob
 * is so called statdata cob and returned by m0_cob_locate().
 * Error code is returned in error case or zero otherwise.
 */
M0_INTERNAL int m0_mdstore_open(struct m0_mdstore       *md,
				struct m0_cob           *cob,
				m0_mdstore_locate_flags_t flags,
				struct m0_be_tx         *tx);

/**
 * Handle close operation on @cob. Input @cob is so called statdata
 * cob and returned by m0_cob_locate().
 *
 * Error code is returned in error case or zero otherwise.
 */
M0_INTERNAL int m0_mdstore_close(struct m0_mdstore      *md,
				 struct m0_cob          *cob,
				 struct m0_be_tx        *tx);

/**
 * Handle setattr operation described by @attr on @cob. Input @cob
 * is so called statdata cob and returned by m0_cob_locate().
 *
 * Error code is returned in error case or zero otherwise.
 */
M0_INTERNAL int m0_mdstore_setattr(struct m0_mdstore    *md,
				   struct m0_cob        *cob,
				   struct m0_cob_attr   *attr,
				   struct m0_be_tx      *tx);

/**
 * Get attributes of @cob into passed @attr. Input @cob
 * is so called statdata cob and returned by m0_cob_locate().
 *
 * Error code is returned in error case or zero otherwise.
 */
M0_INTERNAL int m0_mdstore_getattr(struct m0_mdstore    *md,
				   struct m0_cob        *cob,
				   struct m0_cob_attr   *attr);

/**
 * Handle readdir operation described by @rdpg on @cob. Input @cob
 * is so called statdata cob and returned by m0_cob_locate().
 *
 * Error code is returned in error case or something >= 0 otherwise.
 */
M0_INTERNAL int m0_mdstore_readdir(struct m0_mdstore    *md,
				   struct m0_cob        *cob,
				   struct m0_rdpg       *rdpg);

/**
 * Find cob by fid.
 */
M0_INTERNAL int m0_mdstore_locate(struct m0_mdstore     *md,
				  const struct m0_fid   *fid,
				  struct m0_cob        **cob,
				  int                    flags);

/**
 * Find cob by parent fid and name.
 */
M0_INTERNAL int m0_mdstore_lookup(struct m0_mdstore     *md,
				  struct m0_fid         *pfid,
				  struct m0_buf         *name,
				  struct m0_cob        **cob);

/**
 * Get path by @fid. Path @path is allocated by
 * m0_alloc() on success and path is saved there.
 * When it is not longer needed it may be freed
 * with m0_free().
 */
M0_INTERNAL int m0_mdstore_path(struct m0_mdstore       *md,
				struct m0_fid           *fid,
				char                   **path);

M0_INTERNAL void
m0_mdstore_create_credit(struct m0_mdstore *md,
			 struct m0_be_tx_credit *accum);

M0_INTERNAL void
m0_mdstore_link_credit(struct m0_mdstore *md,
		       struct m0_be_tx_credit *accum);

M0_INTERNAL void
m0_mdstore_unlink_credit(struct m0_mdstore *md,
		         struct m0_be_tx_credit *accum);

M0_INTERNAL void
m0_mdstore_rename_credit(struct m0_mdstore *md,
		         struct m0_be_tx_credit *accum);

M0_INTERNAL void
m0_mdstore_setattr_credit(struct m0_mdstore *md,
		          struct m0_be_tx_credit *accum);

M0_INTERNAL int m0_mdstore_mod_init(void);
M0_INTERNAL void m0_mdstore_mod_fini(void);

/** @} */ /* end of mdstore group */

/* __MOTR_MDSTORE_MDSTORE_H__ */
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
