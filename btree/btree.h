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
	M0_BSC_KEY_EXISTS,
	M0_BSC_KEY_NOT_FOUND,
	M0_BSC_KEY_BTREE_BOUNDARY,
};

enum m0_btree_opflag {
	M0_BOF_UNIQUE = 1 << 0
};

int  m0_btree_open(void *addr, int nob, struct m0_btree **out);
void m0_btree_close(struct m0_btree *arbor);
void m0_btree_create(void *addr, int nob, const struct m0_btree_type *bt,
		     const struct node_type *nt, struct m0_be_tx *tx,
		     struct m0_btree_op *bop);
void m0_btree_destroy(struct m0_btree *arbor, struct m0_btree_op *bop);
void m0_btree_get(struct m0_btree *arbor, const struct m0_btree_key *key,
		  const struct m0_btree_cb *cb, uint64_t flags,
		  struct m0_btree_op *bop);
void m0_btree_put(struct m0_btree *arbor, struct m0_be_tx *tx,
		  const struct m0_btree_rec *rec, const struct m0_btree_cb *cb,
		  uint64_t flags, struct m0_btree_op *bop);
void m0_btree_del(struct m0_btree *arbor, const struct m0_btree_key *key,
		  const struct m0_btree_cb *cb, uint64_t flags,
		  struct m0_btree_op *bop);
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


#define M0_BTREE_OP_SYNC_WITH_RC(op, action, group, op_exec)    \
	({                                                      \
		struct m0_sm_op *__opp = (op);                  \
		int32_t          __op_rc;                       \
								\
		m0_sm_group_init(group);                        \
		m0_sm_group_lock(group);                        \
		m0_sm_op_exec_init(op_exec);                    \
								\
		action;                                         \
		m0_sm_op_tick(__opp);                           \
		__op_rc = __opp->o_sm.sm_rc;                    \
		m0_sm_op_fini(__opp);                           \
								\
		m0_sm_op_exec_fini(op_exec);                    \
		m0_sm_group_unlock(group);                      \
		m0_sm_group_fini(group);                        \
		__op_rc;                                        \
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
