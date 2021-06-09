/* -*- C -*- */
/*
 * Copyright (c) 2015-2020 Seagate Technology LLC and/or its Affiliates
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

#ifndef __MOTR_FD_UT_COMMON_H__
#define __MOTR_FD_UT_COMMON_H__

enum layout_attr {
	la_N = 8,
        la_K = 2,
	la_S = 2
};

enum tree_ut_attr {
	/* Max number of racks in a pool version. */
	TUA_RACKS        = 8,
	/* Max number of enclosures per rack. */
	TUA_ENC          = 7,
	/* Max number of controllers per enclosure. */
	TUA_CON          = 2,
	/* Max number of disks per controller. */
	TUA_DISKS        = 82,
	TUA_ITER         = 20,
	/* Max number of children for any node in UT for the m0_fd_tree-tree. */
	TUA_CHILD_NR_MAX = 13,
	/* Maximum pool width of system. */
	TUA_MAX_POOL_WIDTH = 5000,
};
M0_BASSERT(TUA_CHILD_NR_MAX >= la_N + la_K + la_S &&
	   TUA_MAX_POOL_WIDTH >= TUA_CHILD_NR_MAX);

enum tree_type {
	TP_LEAF,
	TP_UNARY,
	TP_BINARY,
	TP_TERNARY,
	TP_QUATERNARY,
	TP_NR,
};

enum tree_gen_type {
	/* Generation with deterministic parameters. */
	TG_DETERM,
	/* Generation with random parameters. */
	TG_RANDOM,
};

enum tree_attr {
	/* A tree in which all nodes at same level have same number of
	 * children. */
	TA_SYMM,
	/* A tree that is not TA_SYMM. */
	TA_ASYMM,
};

M0_INTERNAL int fd_ut_tree_init(struct m0_fd_tree *tree, uint64_t tree_depth);

M0_INTERNAL void fd_ut_children_populate(uint64_t *child_nr, uint32_t length);


M0_INTERNAL void fd_ut_symm_tree_create(struct m0_fd_tree *tree,
					enum tree_gen_type tg_type,
					uint64_t *child_nr, uint64_t depth);

M0_INTERNAL int fd_ut_tree_level_populate(struct m0_fd_tree *tree,
			                  uint64_t children, uint16_t level,
					  enum tree_attr ta);

M0_INTERNAL void fd_ut_symm_tree_get(struct m0_fd_tree *tree, uint64_t *children_nr);
M0_INTERNAL uint64_t fd_ut_random_cnt_get(uint64_t max_cnt);

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
