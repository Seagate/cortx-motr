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
	M0_BO_NXT,

	M0_BO_NR
};

enum m0_btree_opflag {
	M0_BOF_UNIQUE = 1 << 0
};

int  m0_btree_open(void *addr, int nob, struct m0_btree **out);
void m0_btree_close(struct m0_btree *arbor);
void m0_btree_create(void *addr, int nob, const struct m0_btree_type *bt,
		     struct m0_be_tx *tx, struct m0_btree_op *bop);
void m0_btree_destroy(struct m0_btree *arbor, struct m0_btree_op *bop);
void m0_btree_get(struct m0_btree *arbor, const struct m0_btree_key *key,
		  const struct m0_btree_cb *cb, uint64_t flags,
		  struct m0_btree_op *bop);
void m0_btree_put(struct m0_btree *arbor, struct m0_be_tx *tx,
		  const struct m0_btree_key *key,
		  const struct m0_btree_cb *cb, uint64_t flags,
		  struct m0_btree_op *bop);
void m0_btree_del(struct m0_btree *arbor, const struct m0_btree_key *key,
		  const struct m0_btree_cb *cb, uint64_t flags,
		  struct m0_btree_op *bop);
void m0_btree_nxt(struct m0_btree *arbor, const struct m0_btree_key *key,
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


/**
 * This macro calls the 'action' function and follows it by calling
 * m0_sm_op_tick() to execute the state machine.
 *
 * IMPORTANT: The 'action' routine should execute the call m0_sm_op_init() for
 * setting the *_tick() function which is eventually be called by
 * m0_sm_op_tick() function.
 */
#define M0_BTREE_OP_SYNC_WITH(op, action)       \
	({                                      \
		struct m0_sm_op *__opp = (op);  \
						\
		action;                         \
		m0_sm_op_tick(__opp);           \
		m0_sm_op_fini(__opp);           \
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
