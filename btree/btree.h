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

#ifndef __MOTR_BTREE_BTREE_H__
#define __MOTR_BTREE_BTREE_H__

#include "lib/types.h"
#include "lib/vec.h"
#include "xcode/xcode_attr.h"
#include "btree/internal.h"

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

struct m0_btree_key {
	struct m0_bufvec  k_data;
	struct m0_bcookie k_cookie;
};

struct m0_btree_rec {
	struct m0_btree_key r_key;
	struct m0_bufvec    r_val;
};

struct m0_btree_cb {
	int (*c_act)(struct m0_btree_cb *cb, struct m0_btree_rec *rec);
	void *c_datum;
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
