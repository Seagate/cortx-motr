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
#ifndef __MOTR_FD_INT_H__
#define __MOTR_FD_INT_H__

#include "fd/fd.h"
#include "conf/obj.h" /* m0_conf_pver */
#include "lib/misc.h" /* uint16_t uint32_t */

struct m0_fd__tree_cursor {
	/** Failure domain tree to which a cursor is associated. */
	struct m0_fd_tree      *ftc_tree;
	/** Depth of current cursor location in the tree. */
	uint16_t                ftc_depth;
	/** Current node at which cursor resides. */
	struct m0_fd_tree_node *ftc_node;
	/** Index in m0_fd_tree_node::ftn_children, at which cursor points. */
	int32_t                 ftc_child_idx;
	/** Absolute index of child at which cursor points. */
	int32_t                 ftc_child_abs_idx;
	/** Counts the cursor movement at ftc_depth. */
	uint32_t                ftc_cnt;
	/** Starting from the tree root, Holds absolute indices of ancestors of
	 * the current cursor location */
	uint64_t                ftc_path[M0_CONF_PVER_HEIGHT + 1];
};

/** Calculates the tile parameters using pdclust layout. */
M0_INTERNAL int m0_fd__tile_init(struct m0_fd_tile *tile,
				 const struct m0_pdclust_attr *la_attr,
				 uint64_t *children_nr, uint64_t depth);
/** Populates the tile with appropriate values. */
M0_INTERNAL void m0_fd__tile_populate(struct m0_fd_tile *tile);

/** Allocates and initializes the root node of the failure domains tree.
 */
M0_INTERNAL int m0_fd__tree_root_create(struct m0_fd_tree *tree,
				        uint64_t root_children);

/** Allocates and initializes the nodes at a given level, in failure
 *  domains tree, using the input pool version.
 */
M0_INTERNAL int m0_fd__tree_level_populate(const struct m0_conf_pver *pv,
				           struct m0_fd_tree *tree,
				           uint32_t level);


M0_INTERNAL int m0_fd__tree_node_init(struct m0_fd_tree *tree,
				      struct m0_fd_tree_node *node,
				      uint16_t child_nr,
				      const struct m0_fd__tree_cursor *cursor);
M0_INTERNAL void m0_fd__tree_node_fini(struct m0_fd_tree *tree,
				       struct m0_fd_tree_node *node);

M0_INTERNAL int m0_fd__tree_cursor_init(struct m0_fd__tree_cursor *cursor,
					const struct m0_fd_tree *tree,
					uint16_t depth);

M0_INTERNAL int m0_fd__tree_cursor_init_at(struct m0_fd__tree_cursor *cursor,
				           const struct m0_fd_tree *tree,
					   const struct m0_fd_tree_node *node,
				           uint32_t child_idx);

M0_INTERNAL struct m0_fd_tree_node **
	m0_fd__tree_cursor_get(struct m0_fd__tree_cursor *cursor);

M0_INTERNAL int m0_fd__tree_cursor_next(struct m0_fd__tree_cursor *cursor);


M0_INTERNAL int m0_fd__perm_cache_build(struct m0_fd_tree *tree);

M0_INTERNAL void m0_fd__perm_cache_destroy(struct m0_fd_tree *tree);

M0_INTERNAL bool m0_fd__tree_invariant(const struct m0_fd_tree *tree);

M0_INTERNAL bool m0_fd__tree_node_invariant(const struct m0_fd_tree *tree,
					    const struct m0_fd_tree_node *node);
#endif
