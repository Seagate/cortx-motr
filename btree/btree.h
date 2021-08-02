/* -*- C -*- */
/*
 * Copyright (c) 2013-2021 Seagate Technology LLC and/or its Affiliates
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

#ifndef __MOTR_BTREE_BTREE_H__
#define __MOTR_BTREE_BTREE_H__

#include "lib/types.h"
#include "lib/vec.h"
#include "xcode/xcode_attr.h"
#include "lib/errno.h"

/**
 * @defgroup btree
 *
 * @{
 */

struct m0_be_tx;
struct m0_be_tx_credit;

struct m0_btree_type;
struct m0_btree;
struct m0_btree_op;
struct m0_btree_rec;
struct m0_btree_cb;
struct m0_btree_key;
struct m0_btree_idata;

enum m0_btree_types {
	M0_BT_INVALID = 1,
	M0_BT_BALLOC_GROUP_EXTENTS,
	M0_BT_BALLOC_GROUP_DESC,
	M0_BT_EMAP_EM_MAPPING,
	M0_BT_CAS_CTG,
	M0_BT_COB_NAMESPACE,
	M0_BT_COB_OBJECT_INDEX,
	M0_BT_COB_FILEATTR_BASIC,
	M0_BT_COB_FILEATTR_EA,
	M0_BT_COB_FILEATTR_OMG,
	M0_BT_CONFDB,
	M0_BT_UT_KV_OPS,
	M0_BT_NR
};


struct m0_btree_type {
	enum m0_btree_types tt_id;
	int ksize;
	int vsize;
};


struct m0_bcookie {
	void     *segaddr;
	uint64_t  n_seq;
};

struct m0_btree_key {
	struct m0_bufvec  k_data;
	struct m0_bcookie k_cookie;
};

struct m0_btree_rec {
	struct m0_btree_key r_key;
	struct m0_bufvec    r_val;
	uint32_t            r_flags;
};

struct m0_btree_cb {
	int (*c_act)(struct m0_btree_cb *cb, struct m0_btree_rec *rec);
	void *c_datum;
};

/**
 * This structure is used to hold the data that is passed to m0_tree_create.
 */
struct m0_btree_idata {
	void                       *addr;
	struct m0_btree            *tree;
	int                         num_bytes;
	const struct m0_btree_type *bt;
	const struct node_type     *nt;
	int                         ks;
	int                         vs;
};

enum m0_btree_rec_type {
	M0_BRT_VALUE = 1,
	M0_BRT_CHILD = 2,
};

enum m0_btree_opcode {
	M0_BO_CREATE = 1,
	M0_BO_DESTROY,
	M0_BO_GET,
	M0_BO_PUT,
	M0_BO_UPDATE,
	M0_BO_DEL,
	M0_BO_ITER,

	M0_BO_NR
};

enum m0_btree_op_flags {
	BOF_PREV      = M0_BITS(0),
	BOF_NEXT      = M0_BITS(1),
	BOF_LOCKALL   = M0_BITS(2),
	BOF_COOKIE    = M0_BITS(3),
	BOF_EQUAL     = M0_BITS(4),
	BOF_SLANT     = M0_BITS(5),
};

/**
 * These status codes are filled in m0_btree_rec.r_flags to provide the callback
 * routine the status of the operatoin.
 */
enum m0_btree_status_codes {
	M0_BSC_SUCCESS = 0,
	M0_BSC_KEY_EXISTS = EEXIST,
	M0_BSC_KEY_NOT_FOUND = ENOENT,
	M0_BSC_KEY_BTREE_BOUNDARY,
};

enum m0_btree_opflag {
	M0_BOF_UNIQUE = 1 << 0
};


/**
 * Btree functions related to tree management
 */


/**
 * Opens the tree and returns the pointer to m0_btree which is used for
 * subsequent operations related to this btree.
 *
 * @param addr is the address of exsiting root node in BE segment.
 * @param nob is the size of the root node in BE segment.
 * @param out has the handle to the opened btree used in subsequent operations.
 * @param bop is consumed by the m0_btree_open for its operation.
 *
 * @return 0 if successful.
 */
int  m0_btree_open(void *addr, int nob, struct m0_btree **out,
		   struct m0_btree_op *bop);

/**
 * Closes the opened or created tree represented by arbor. Once the close
 * completes no further actions should be triggered for this btree until the
 * btree is opened again by calling m0_btree_open()
 *
 * If some of the nodes for this btree are still active in different threads or
 * FOMs then this function waits till all the active nodes of this btree have
 * been 'PUT' or destroyed.
 *
 * @param arbor is the btree which needs to be closed.
 */
void m0_btree_close(struct m0_btree *arbor, struct m0_btree_op *bop);

/**
 * Creates a new btree with the root node created at the address passed as the
 * parameter. The space of size nob for this root node is assumed to be
 * allocated, in the BE segment, by the caller. This function initializes and
 * uses this space for holding the root node of this newly created tree. The
 * routine return the pointer to m0_btree which is used for subsequent
 * operations related to this btree.
 * m0_btree_create(), if successful, returns the tree handle which can be used
 * for subsequent tree operations without having to call m0_tree_open().
 *
 * @param addr is the address of exsiting root node in BE segment.
 * @param nob is the size of the root node in BE segment.
 * @param bt provides more information about the btree to be created.
 * @param nt provides the node type which will be attached to this btree.
 * @param bop is consumed by the m0_btree_create for its operation. It contains
 *        the field bo_arbor which holds the tree pointer to be used by the
 *        caller after the call completes.
 * @param seg points to the BE segment which will host the nodes of the tree.
 * @param tx pointer to the transaction struture to capture BE segment changes.
 */
void m0_btree_create(void *addr, int nob, const struct m0_btree_type *bt,
		     const struct node_type *nt, struct m0_btree_op *bop,
		     struct m0_be_seg *seg, struct m0_be_tx *tx);

/**
 * Destroys the opened or created tree represented by arbor. Once the destroy
 * completes no further actions should be triggered for this btree as this tree
 * should be assumed to be deleted.
 * This routine expects all the nodes of this tree to have been deleted and no
 * records to be present in this btree.
 *
 * @param arbor is the btree which needs to be closed.
 * @param bop is consumed by the m0_btree_destroy for its operation.
 * @param tx pointer to the transaction structure to capture BE segment changes.
 */
void m0_btree_destroy(struct m0_btree *arbor, struct m0_btree_op *bop,
		      struct m0_be_tx *tx);

/**
 * Searches for the key/slant key provided as the search key. The callback
 * routine is called when the search yields the key/slant key and the location
 * of this key/slant key is passed to the callback.
 * The callback is NOT supposed to modify this record since these changes will
 * not get captured in any transaction and hence m0_btree_put() should be called
 * by the caller instead.
 *
 * @param arbor is the pointer to btree.
 * @param key   is the Key to be searched in the btree.
 * @param cb    Callback routine to be called on search success.
 * @param flags Operation specific flags (cookie, slant etc.).
 * @param bop   Btree operation related parameters.
 */
void m0_btree_get(struct m0_btree *arbor, const struct m0_btree_key *key,
		  const struct m0_btree_cb *cb, uint64_t flags,
		  struct m0_btree_op *bop);

/**
 * Inserts the record in the tree. The callback is called with the location
 * where the new key and value should be inserted in the tree.
 *
 * @param arbor is the pointer to btree.
 * @param rec   represents the record which needs to get inserted. Note that,
 *              user may or may not provide valid value but record should be
 *              provided with valid key, key size and value size as this
 *              information is needed for correct operation.
 * @param cb    routine to be called to PUT the record.
 * @param flags
 * @param bop   Btree operation related parameters.
 * @param tx    represents the transaction of which the current operation is
 *              part of.
 */
void m0_btree_put(struct m0_btree *arbor, const struct m0_btree_rec *rec,
		  const struct m0_btree_cb *cb, uint64_t flags,
		  struct m0_btree_op *bop, struct m0_be_tx *tx);

/**
 * Deletes the record in the tree. The callback is called with the location
 * where the original key and value should are present in the tree.
 *
 * @param arbor is the pointer to btree.
 * @param key   points to the Key whose record is to be deleted from the tree.
 * @param flags
 * @param bop   Btree operation related parameters.
 * @param tx    represents the transaction of which the current operation is
 *              part of.
 */
void m0_btree_del(struct m0_btree *arbor, const struct m0_btree_key *key,
		  const struct m0_btree_cb *cb, uint64_t flags,
		  struct m0_btree_op *bop, struct m0_be_tx *tx);

/**
 * Iterates through the tree and finds next/previous key from the given search
 * key based on the flag. The callback routine is provided with the record of
 * the next/previous Key which was found in the tree.
 *
 * @param arbor Btree parameteres.`
 * @param key   Key to be searched in the btree.
 * @param cb    Callback routine to return operation output.
 * @param flags Operation specific flags (cookie, slant, prev, next etc.).
 * @param bop   Btree operation related parameters.
 */
void m0_btree_iter(struct m0_btree *arbor, const struct m0_btree_key *key,
		   const struct m0_btree_cb *cb, uint64_t flags,
		   struct m0_btree_op *bop);

void m0_btree_op_init(struct m0_btree_op *bop, enum m0_btree_opcode *opc,
		      struct m0_btree *arbor,
		      struct m0_btree_key *key, const struct m0_btree_cb *cb,
		      uint64_t flags, struct m0_be_tx *tx);
void m0_btree_op_fini(struct m0_btree_op *bop);

void m0_btree_op_credit(const struct m0_btree_op *bt,
			struct m0_be_tx_credit *cr);

#include "btree/internal.h"

int  m0_btree_mod_init(void);
void m0_btree_mod_fini(void);


#define M0_BTREE_OP_SYNC_WITH_RC(bop, action)                           \
	({                                                              \
		struct m0_btree_op *__bopp = (bop);                     \
		int32_t             __op_rc;                            \
									\
		m0_sm_group_init(&__bopp->bo_sm_group);                 \
		m0_sm_group_lock(&__bopp->bo_sm_group);                 \
		m0_sm_op_exec_init(&__bopp->bo_op_exec);                \
									\
		action;                                                 \
		m0_sm_op_tick(&__bopp->bo_op);                          \
		__op_rc = __bopp->bo_op.o_sm.sm_rc;                     \
		m0_sm_op_fini(&__bopp->bo_op);                          \
									\
		m0_sm_op_exec_fini(&__bopp->bo_op_exec);                \
		m0_sm_group_unlock(&__bopp->bo_sm_group);               \
		m0_sm_group_fini(&__bopp->bo_sm_group);                 \
		__op_rc;                                                \
	})



/** @} end of btree group */
#endif /* __MOTR_BTREE_BTREE_H__ */

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
