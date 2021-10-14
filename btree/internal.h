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

#ifndef __MOTR_BTREE_INTERNAL_H__
#define __MOTR_BTREE_INTERNAL_H__

#include "sm/op.h"
/**
 * @defgroup btree
 *
 * @{
 */

enum m0_btree_opcode;
struct m0_btree_oimpl;

struct m0_btree_op {
	struct m0_sm_op             bo_op;
	struct m0_sm_group          bo_sm_group;
	struct m0_sm_op_exec        bo_op_exec;
	enum m0_btree_opcode        bo_opc;
	struct m0_btree            *bo_arbor;
	struct m0_btree_rec         bo_rec;
	struct m0_btree_cb          bo_cb;
	struct m0_be_tx            *bo_tx;
	struct m0_be_seg           *bo_seg;
	uint64_t                    bo_flags;
	m0_bcount_t                 bo_limit;
	struct m0_btree_oimpl      *bo_i;
	struct m0_btree_idata       bo_data;
	struct m0_btree_rec_key_op  bo_keycmp;
};

enum m0_btree_node_format_version {
	M0_BTREE_NODE_FORMAT_VERSION_1 = 1,

	/* future versions, uncomment and update M0_BE_BNODE_FORMAT_VERSION */
	/*M0_BTREE_NODE_FORMAT_VERSION_2,*/
	/*M0_BTREE_NODE_FORMAT_VERSION_3,*/

	/** Current version, should point to the latest version present */
	M0_BTREE_NODE_FORMAT_VERSION = M0_BTREE_NODE_FORMAT_VERSION_1
};

struct m0_btree_cursor {
	struct m0_buf    bc_key;
	struct m0_buf    bc_val;
	struct m0_btree *bc_arbor;
};

struct td;
struct m0_btree {
	const struct m0_btree_type *t_type;
	unsigned                    t_height;
	struct td                  *t_desc;
};

/** @} end of btree group */
#endif /* __MOTR_BTREE_INTERNAL_H__ */

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
