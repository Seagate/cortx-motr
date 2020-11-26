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


/**
 * @addtogroup be
 *
 * @{
 */

#define M0_TRACE_SUBSYSTEM M0_TRACE_SUBSYS_BTREE
#include "lib/trace.h"

#include "lib/errno.h"
#include "lib/finject.h"       /* M0_FI_ENABLED() */
#include "lib/misc.h"          /* offsetof */
#include "be/alloc.h"
#include "be/btree.h"
#include "be/btree_internal.h" /* m0_be_bnode */
#include "be/seg.h"
#include "be/tx.h"             /* m0_be_tx_capture */

/* btree constants */
enum {
	BTREE_ALLOC_SHIFT = 0,
};

enum btree_save_optype {
	BTREE_SAVE_INSERT,
	BTREE_SAVE_UPDATE,
	BTREE_SAVE_OVERWRITE
};

struct btree_node_pos {
	struct m0_be_bnode *bnp_node;
	unsigned int        bnp_index;
};

M0_INTERNAL const struct m0_fid_type m0_btree_fid_type = {
	.ft_id   = 'b',
	.ft_name = "btree fid",
};

static struct m0_be_op__btree *op_tree(struct m0_be_op *op);
static struct m0_rwlock *btree_rwlock(struct m0_be_btree *tree);

static struct be_btree_key_val *be_btree_search(struct m0_be_btree *btree,
						void *key);

static void btree_root_set(struct m0_be_btree *btree,
			   struct m0_be_bnode *new_root)
{
	M0_PRE(btree != NULL);

	btree->bb_root = new_root;
	m0_format_footer_update(btree);
}

/* XXX Shouldn't we set other fields of m0_be_op__btree? */
static void btree_op_fill(struct m0_be_op *op, struct m0_be_btree *btree,
			  struct m0_be_tx *tx, enum m0_be_btree_op optype,
			  struct m0_be_btree_anchor *anchor)
{
	struct m0_be_op__btree *tree;

	M0_PRE(op != NULL);

	tree = &op->bo_u.u_btree;

	op->bo_utype   = M0_BOP_TREE;
	tree->t_tree   = btree;
	tree->t_tx     = tx;
	tree->t_op     = optype;
	tree->t_in     = NULL;
	tree->t_anchor = anchor;
	tree->t_rc     = 0;
}

static struct m0_be_allocator *tree_allocator(const struct m0_be_btree *btree)
{
	return m0_be_seg_allocator(btree->bb_seg);
}

static inline void mem_free(const struct m0_be_btree *btree,
			    struct m0_be_tx *tx, void *ptr)
{
	M0_BE_OP_SYNC(op,
		      m0_be_free_aligned(tree_allocator(btree), tx, &op, ptr));
}

/* XXX: check if region structure itself needed outside m0_be_tx_capture() */
static inline void mem_update(const struct m0_be_btree *btree,
			      struct m0_be_tx *tx, void *ptr, m0_bcount_t size)
{
	m0_be_tx_capture(tx, &M0_BE_REG(btree->bb_seg, size, ptr));
}

static inline void *mem_alloc(const struct m0_be_btree *btree,
			      struct m0_be_tx *tx, m0_bcount_t size,
			      uint64_t zonemask)
{
	void *p;

	M0_BE_OP_SYNC(op,
		      m0_be_alloc_aligned(tree_allocator(btree),
					  tx, &op, &p, size,
					  BTREE_ALLOC_SHIFT,
					  zonemask));
	M0_ASSERT(p != NULL);
	return p;
}

static void btree_mem_alloc_credit(const struct m0_be_btree *btree,
				   m0_bcount_t size,
				   struct m0_be_tx_credit *accum)
{
	m0_be_allocator_credit(NULL, M0_BAO_ALLOC_ALIGNED, size,
			       BTREE_ALLOC_SHIFT, accum);
}

static void btree_mem_free_credit(const struct m0_be_btree *btree,
				  m0_bcount_t size,
				  struct m0_be_tx_credit *accum)
{
	m0_be_allocator_credit(tree_allocator(btree),
			       M0_BAO_FREE_ALIGNED, 0, 0, accum);
}

static m0_bcount_t be_btree_ksize(const struct m0_be_btree *btree,
				  const void *key)
{
	const struct m0_be_btree_kv_ops *ops = btree->bb_ops;
	return ops->ko_ksize(key);
}

static m0_bcount_t be_btree_vsize(const struct m0_be_btree *btree,
				  const void *data)
{
	const struct m0_be_btree_kv_ops *ops = btree->bb_ops;

	return ops->ko_vsize(data);
}

static int be_btree_compare(const struct m0_be_btree *btree,
			    const void *key0, const void *key1)
{
	const struct m0_be_btree_kv_ops *ops = btree->bb_ops;
	return ops->ko_compare(key0, key1);
}

static inline int key_lt(const struct m0_be_btree *btree,
			 const void *key0, const void *key1)
{
	return be_btree_compare(btree, key0, key1)  <  0;
}

static inline int key_gt(const struct m0_be_btree *btree,
			 const void *key0, const void *key1)
{
	return be_btree_compare(btree, key0, key1)  >  0;
}

static inline int key_eq(const struct m0_be_btree *btree,
			 const void *key0, const void *key1)
{
	return be_btree_compare(btree, key0, key1) ==  0;
}

/* ------------------------------------------------------------------
 * Btree internals implementation
 * ------------------------------------------------------------------ */

enum position_t {
	P_LEFT = -1,
	P_RIGHT = 1
};

static struct m0_be_bnode *be_btree_node_alloc(const struct m0_be_btree *btree,
					       struct m0_be_tx *tx);

static void btree_node_free(struct m0_be_bnode *node,
			    const struct m0_be_btree *btree,
			    struct m0_be_tx *tx);

static void btree_pair_release(struct m0_be_btree *btree,
			       struct m0_be_tx *tx,
			       struct be_btree_key_val  *kv);

static struct btree_node_pos be_btree_get_btree_node(
					struct m0_be_btree_cursor *it,
					const void *key, bool slant);

static void be_btree_delete_key_from_node(struct m0_be_btree *tree,
					  struct m0_be_tx *tx,
					  struct btree_node_pos *node_pos);

static void be_btree_move_parent_key(struct m0_be_btree *tree,
				     struct m0_be_tx *tx,
				     struct m0_be_bnode *node,
				     enum position_t pos,
				     unsigned int index);

static void be_btree_get_max_key_pos(struct m0_be_bnode    *node,
				     struct btree_node_pos *pos);

static void be_btree_get_min_key_pos(struct m0_be_bnode    *node,
				     struct btree_node_pos *pos);

static int iter_prepare(struct m0_be_bnode *node, bool print);

/**
 * Btree backlink invariant implementation:
 * - checks that cookie is valid.
 * - btree type is valid.
 * - btree generation factor is valid.
 * @param h      link back to the btree.
 * @param seg    backend segment.
 * @param parent cookie of btree.
 * @return true if cookie, btree type and generation factor are valid,
 *         else false.
 */
static bool btree_backlink_invariant(const struct m0_be_btree_backlink *h,
				     const struct m0_be_seg *seg,
				     const uint64_t *parent)
{
	uint64_t *cookie;

	return
		_0C(m0_cookie_dereference(&h->bli_tree, &cookie) == 0) &&
		_0C(m0_be_seg_contains(seg, cookie)) &&
		_0C(parent == cookie) &&
		_0C(M0_BBT_INVALID < h->bli_type && h->bli_type < M0_BBT_NR) &&
		_0C(h->bli_gen == seg->bs_gen);
}

/**
 * Btree node invariant implementation:
 * - assuming that the tree is completely in memory.
 * - checks that keys are in order.
 * - node backlink to btree is valid.
 * - nodes have expected occupancy: [1..2*order-1] for root and
 *				    [order-1..2*order-1] for leafs.
 */
static bool btree_node_invariant(const struct m0_be_btree *btree,
				 const struct m0_be_bnode *node, bool root)
{
	return
		_0C(node->bt_header.hd_magic != 0) &&
		_0C(m0_format_footer_verify(&node->bt_header, true) == 0) &&
		_0C(btree_backlink_invariant(&node->bt_backlink, btree->bb_seg,
					     &btree->bb_cookie_gen)) &&
		_0C(node->bt_level <= BTREE_HEIGHT_MAX) &&
		_0C(memcmp(&node->bt_backlink, &btree->bb_backlink,
			   sizeof node->bt_backlink) == 0) &&
		/* Expected occupancy. */
		_0C(ergo(root, 0 <= node->bt_num_active_key &&
			 node->bt_num_active_key <= KV_NR)) &&
		_0C(ergo(!root, BTREE_FAN_OUT - 1 <= node->bt_num_active_key &&
			 node->bt_num_active_key <= KV_NR)) &&
		_0C(m0_forall(i, node->bt_num_active_key,
			      node->bt_kv_arr[i].btree_key != NULL &&
			      node->bt_kv_arr[i].btree_val != NULL &&
			      m0_be_seg_contains(btree->bb_seg,
						 node->bt_kv_arr[i].
						 btree_key) &&
			      m0_be_seg_contains(btree->bb_seg,
						 node->bt_kv_arr[i].
						 btree_val))) &&
		_0C(ergo(!node->bt_isleaf,
			 m0_forall(i, node->bt_num_active_key + 1,
				   node->bt_child_arr[i] != NULL &&
				   m0_be_seg_contains(btree->bb_seg,
						      node->
						      bt_child_arr[i])))) &&
		/* Keys are in order. */
		_0C(ergo(node->bt_num_active_key > 1,
			 m0_forall(i, node->bt_num_active_key - 1,
				   key_gt(btree, node->bt_kv_arr[i+1].btree_key,
					  node->bt_kv_arr[i].btree_key))));
}

/* ------------------------------------------------------------------
 * Node subtree invariant implementation:
 * - assuming that the tree is completely in memory;
 * - child nodes have keys matching parent;
 *
 * Note: as far as height of practical tree will be 10-15, invariant can be
 * written in recusieve form.
 * ------------------------------------------------------------------ */
static bool btree_node_subtree_invariant(const struct m0_be_btree *btree,
					 const struct m0_be_bnode *node)
{
	/* Kids are in order. */
	return	_0C(ergo(node->bt_num_active_key > 0 && !node->bt_isleaf,
			 m0_forall(i, node->bt_num_active_key,
				   key_gt(btree, node->bt_kv_arr[i].btree_key,
					  node->bt_child_arr[i]->
					  bt_kv_arr[node->bt_child_arr[i]->
					  bt_num_active_key - 1].btree_key) &&
				   key_lt(btree, node->bt_kv_arr[i].btree_key,
					  node->bt_child_arr[i+1]->
					  bt_kv_arr[0].btree_key)) &&
		         m0_forall(i, node->bt_num_active_key + 1,
				   btree_node_invariant(btree,
							node->bt_child_arr[i],
						        false)) &&
		         m0_forall(i, node->bt_num_active_key + 1,
				   btree_node_subtree_invariant(btree,
							node->bt_child_arr[i])))
		  );
}

/**
 * Btree invariant implementation:
 * - assuming that the tree is completely in memory.
 * - header and checksum are valid.
 * - btree backlink is valid.
 */
static inline bool btree_invariant(const struct m0_be_btree *btree)
{
	return  _0C(m0_format_footer_verify(&btree->bb_header, true) == 0 &&
		    btree_backlink_invariant(&btree->bb_backlink, btree->bb_seg,
					     &btree->bb_cookie_gen));
}

static void btree_node_update(struct m0_be_bnode       *node,
			      const struct m0_be_btree *btree,
			      struct m0_be_tx          *tx)
{
	mem_update(btree, tx, node, offsetof(struct m0_be_bnode, bt_kv_arr));

	if (node->bt_num_active_key > 0) {
		mem_update(btree, tx, node->bt_kv_arr,
			   sizeof(*node->bt_kv_arr) * node->bt_num_active_key);
		mem_update(btree, tx, node->bt_child_arr,
			   sizeof(*node->bt_child_arr) *
			   (node->bt_num_active_key + 1));
	}

	mem_update(btree, tx, &node->bt_footer, sizeof(node->bt_footer));
}

static void btree_node_keyval_update(struct m0_be_bnode       *node,
				     const struct m0_be_btree *btree,
				     struct m0_be_tx          *tx,
				     unsigned int              index)
{
	m0_format_footer_update(node);
	mem_update(btree, tx, &node->bt_kv_arr[index],
			   sizeof node->bt_kv_arr[index]);
	mem_update(btree, tx, &node->bt_footer, sizeof node->bt_footer);
}

/**
 * Used to create a btree with just the root node
 */
static void btree_create(struct m0_be_btree  *btree,
			 struct m0_be_tx     *tx,
			 const struct m0_fid *btree_fid)
{
	m0_format_header_pack(&btree->bb_header, &(struct m0_format_tag){
		.ot_version = M0_BE_BTREE_FORMAT_VERSION,
		.ot_type    = M0_FORMAT_TYPE_BE_BTREE,
		.ot_footer_offset = offsetof(struct m0_be_btree, bb_footer)
	});
	m0_cookie_new(&btree->bb_cookie_gen);
	m0_cookie_init(&btree->bb_backlink.bli_tree, &btree->bb_cookie_gen);
	btree->bb_backlink.bli_gen = btree->bb_seg->bs_gen;
	btree->bb_backlink.bli_type = btree->bb_ops->ko_type;
	btree->bb_backlink.bli_fid = *btree_fid;
	btree_root_set(btree, be_btree_node_alloc(btree, tx));
	mem_update(btree, tx, btree, sizeof(struct m0_be_btree));

	/* memory for the node has to be reserved by m0_be_tx_open() */
	M0_ASSERT(btree->bb_root != NULL);
}

static void be_btree_set_node_params(struct m0_be_bnode *p_node,
				     unsigned int        num_active_key,
				     unsigned int        level,
				     bool                isleaf)
{
	p_node->bt_num_active_key = num_active_key;
	p_node->bt_level = level;
	p_node->bt_isleaf = isleaf;
}

/**
 * This function is used to allocate memory for the btree node
 *
 * @param btree the btree node to which the node is to be allocated
 * @param tx    the pointer to tx
 * @return      the allocated btree node
 */
static struct m0_be_bnode *
be_btree_node_alloc(const struct m0_be_btree *btree, struct m0_be_tx *tx)
{
	struct m0_be_bnode *node;

	/*  Allocate memory for the node */
	node = (struct m0_be_bnode *)mem_alloc(btree, tx,
					       sizeof(struct m0_be_bnode),
					       M0_BITS(M0_BAP_NORMAL));
	M0_ASSERT(node != NULL);	/* @todo: analyse return code */

	m0_format_header_pack(&node->bt_header, &(struct m0_format_tag){
		.ot_version = M0_BE_BNODE_FORMAT_VERSION,
		.ot_type    = M0_FORMAT_TYPE_BE_BNODE,
		.ot_footer_offset = offsetof(struct m0_be_bnode, bt_footer)
	});

	be_btree_set_node_params(node, 0, 0, true);
	node->bt_next = NULL;
	node->bt_backlink = btree->bb_backlink;

	m0_format_footer_update(node);
	mem_update(btree, tx, node, sizeof *node);

	return node;
}

/**
 * Frees the @node.
 */
static void btree_node_free(struct m0_be_bnode       *node,
			    const struct m0_be_btree *btree,
			    struct m0_be_tx          *tx)
{
	/* invalidate header so that recovery tool can skip freed records */
	node->bt_header.hd_magic = 0;
	M0_BE_TX_CAPTURE_PTR(btree->bb_seg, tx, &node->bt_header.hd_magic);
	mem_free(btree, tx, node);
}

/**
 * Splits the child node at @index and updates the @parent.
 *
 * @param btree  the btree to which the node is to be allocated
 * @param tx     the pointer to tx
 * @param parent the parent node
 * @param index  the index of the node to be split
 * @return       none
 */
static void be_btree_split_child(struct m0_be_btree *btree,
				 struct m0_be_tx    *tx,
				 struct m0_be_bnode *parent,
				 unsigned int	     index)
{
	int i;
	struct m0_be_bnode *child = parent->bt_child_arr[index];
	struct m0_be_bnode *new_child = be_btree_node_alloc(btree, tx);
	M0_ASSERT(new_child != NULL);

	be_btree_set_node_params(new_child, (BTREE_FAN_OUT - 1),
					    child->bt_level, child->bt_isleaf);

	/* Copy the latter half keys from the current child to the new child */
	i = 0;
	while (i < new_child->bt_num_active_key) {
		new_child->bt_kv_arr[i] = child->bt_kv_arr[i + BTREE_FAN_OUT];
		i++;
	}

	/*  Update the child pointer field */
	if (!child->bt_isleaf) {
		for (i = 0; i < BTREE_FAN_OUT; i++) {
			new_child->bt_child_arr[i] =
					child->bt_child_arr[i + BTREE_FAN_OUT];
		}
	}

	/* re-calculate checksum after all fields has been updated */
	m0_format_footer_update(new_child);

	child->bt_num_active_key = BTREE_FAN_OUT - 1;
	m0_format_footer_update(child);

	/* The index of the node to be split should be less than or equal to the
	   number of active keys on the parent node */
	M0_PRE(index <= parent->bt_num_active_key);

	/* In the parent node's arr, make space for the new child */
	for (i = parent->bt_num_active_key + 1; i > index + 1; i--) {
		parent->bt_child_arr[i] = parent->bt_child_arr[i - 1];
		parent->bt_kv_arr[i - 1] = parent->bt_kv_arr[i - 2];
	}

	/*  Update parent */
	parent->bt_child_arr[index + 1] = new_child;
	parent->bt_kv_arr[index] = child->bt_kv_arr[BTREE_FAN_OUT - 1];
	parent->bt_num_active_key++;

	/* re-calculate checksum after all fields has been updated */
	m0_format_footer_update(parent);

	/* Update affected memory regions in tx: */
	btree_node_update(parent, btree, tx);
	btree_node_update(child, btree, tx);
	btree_node_update(new_child, btree, tx);
}

/**
 * Inserts @kv entry into the non-full @node.
 *
 * @param btree  the btree where the kv is to be inserted
 * @param tx     the pointer to tx
 * @param node   the non-full node where the kv is to be inserted
 * @param kv     the key value to be inserted
 * @return       none
 */
static void be_btree_insert_into_nonfull(struct m0_be_btree      *btree,
					 struct m0_be_tx         *tx,
					 struct m0_be_bnode      *node,
					 struct be_btree_key_val *kv)
{
	void *key = kv->btree_key;
	int i = node->bt_num_active_key - 1;

	while (!node->bt_isleaf)
	{
		while (i >= 0 &&
		       key_lt(btree, key, node->bt_kv_arr[i].btree_key))
			i--;
		i++;

		if (node->bt_child_arr[i]->bt_num_active_key == KV_NR) {
			be_btree_split_child(btree, tx, node, i);
			if (key_gt(btree, key, node->bt_kv_arr[i].btree_key))
				i++;
		}
		node = node->bt_child_arr[i];
		i = node->bt_num_active_key - 1;
	}

	while (i >= 0 &&
	       key_lt(btree, key, node->bt_kv_arr[i].btree_key)) {
		node->bt_kv_arr[i + 1] = node->bt_kv_arr[i];
		i--;
	}
	node->bt_kv_arr[i + 1] = *kv;
	node->bt_num_active_key++;

	m0_format_footer_update(node);
	/* Update affected memory regions */
	btree_node_update(node, btree, tx);
}

/**
 * Inserts @kv entry into @btree.
 *
 * @param btree  the btree where the kv is to be inserted
 * @param tx     the pointer to tx
 * @param kv     the key value to be inserted
 * @return       none
 */
static void be_btree_insert_newkey(struct m0_be_btree      *btree,
				   struct m0_be_tx         *tx,
				   struct be_btree_key_val *kv)
{
	struct m0_be_bnode *old_root;
	struct m0_be_bnode *new_root;

	M0_PRE(btree_invariant(btree));
	M0_PRE(btree_node_invariant(btree, btree->bb_root, true));
	M0_PRE_EX(btree_node_subtree_invariant(btree, btree->bb_root));
	M0_PRE_EX(be_btree_search(btree, kv->btree_key) == NULL);

	old_root = btree->bb_root;
	if (old_root->bt_num_active_key != KV_NR) {
		be_btree_insert_into_nonfull(btree, tx, old_root, kv);
	} else {
		new_root = be_btree_node_alloc(btree, tx);
		M0_ASSERT(new_root != NULL);

		new_root->bt_level = btree->bb_root->bt_level + 1;
		btree_root_set(btree, new_root);
		be_btree_set_node_params(new_root, 0, new_root->bt_level,
					 false);
		new_root->bt_child_arr[0] = old_root;
		m0_format_footer_update(new_root);
		be_btree_split_child(btree, tx, new_root, 0);
		be_btree_insert_into_nonfull(btree, tx, new_root, kv);

		/* Update tree structure itself */
		mem_update(btree, tx, btree, sizeof(struct m0_be_btree));
	}

	M0_POST(btree_invariant(btree));
	M0_POST(btree_node_invariant(btree, btree->bb_root, true));
	M0_POST_EX(btree_node_subtree_invariant(btree, btree->bb_root));
}

/**
*   Get maxinimum key position in btree.
*
*   This routine will return position of maximum key in btree.
*
*   @param node pointer to root node of btree.
*   @param pos pointer to position structure in which return values are copied.
*/
static void be_btree_get_max_key_pos(struct m0_be_bnode    *node,
				     struct btree_node_pos *pos)
{
	while (node != NULL && !node->bt_isleaf)
		node = node->bt_child_arr[node->bt_num_active_key];

	pos->bnp_node  = node;
	pos->bnp_index = (node != NULL && node->bt_num_active_key > 0) ?
			  node->bt_num_active_key - 1 : 0;
}

/**
*   Get mininimum key position in btree.
*
*   This routine will return position of minimum key in btree.
*
*   @param node pointer to root node of btree.
*   @param pos pointer to position structure in which return values are copied.
*/
static void be_btree_get_min_key_pos(struct m0_be_bnode    *node,
				     struct btree_node_pos *pos)
{
	while (node != NULL && !node->bt_isleaf)
		node = node->bt_child_arr[0];

	pos->bnp_node  = node;
	pos->bnp_index = 0;
}

static void be_btree_shift_key_vals(struct m0_be_bnode *dest,
				    struct m0_be_bnode *src,
				    unsigned int        start_index,
				    unsigned int        key_src_offset,
				    unsigned int        key_dest_offset,
				    unsigned int        child_src_offset,
				    unsigned int        child_dest_offset,
				    unsigned int        stop_index)
{
	unsigned int i = start_index;
	while (i < stop_index)
	{
		dest->bt_kv_arr[i + key_dest_offset] =
					src->bt_kv_arr[i + key_src_offset];
		dest->bt_child_arr[i + child_dest_offset ] =
				src->bt_child_arr[i + child_src_offset];
		++i;
	}
}

/**
*   Merge siblings.
*
*   This routine will merge siblings in btree.
*
*   @param tx pointer to BE transaction.
*   @param tree pointer to btree.
*   @param parent pointer to parent node.
*   @param idx Index of the child array of parent node.
*   @return m0_be_bnode pointer to node after merging.
*/
static struct m0_be_bnode *
be_btree_merge_siblings(struct m0_be_tx    *tx,
			struct m0_be_btree *tree,
			struct m0_be_bnode *parent,
			unsigned int        idx)
{
	struct m0_be_bnode *node1;
	struct m0_be_bnode *node2;

	M0_ENTRY("n=%p i=%d", parent, idx);

	if (idx == parent->bt_num_active_key)
		idx--;

	node1 = parent->bt_child_arr[idx];
	node2 = parent->bt_child_arr[idx + 1];

	node1->bt_kv_arr[node1->bt_num_active_key++] = parent->bt_kv_arr[idx];

	M0_ASSERT(node1->bt_num_active_key + node2->bt_num_active_key <= KV_NR);

	be_btree_shift_key_vals(node1, node2, 0, 0, node1->bt_num_active_key, 0,
				node1->bt_num_active_key,
				node2->bt_num_active_key);

	node1->bt_child_arr[node2->bt_num_active_key+node1->bt_num_active_key] =
				node2->bt_child_arr[node2->bt_num_active_key];
	node1->bt_num_active_key += node2->bt_num_active_key;

	/* re-calculate checksum after all fields has been updated */
	m0_format_footer_update(node1);

	/* update parent */
	be_btree_shift_key_vals(parent, parent, idx, 1, 0, 2, 1,
				parent->bt_num_active_key - 1);

	parent->bt_num_active_key--;
	/* re-calculate checksum after all fields has been updated */
	m0_format_footer_update(parent);

	btree_node_free(node2, tree, tx);

	if (parent->bt_num_active_key == 0 && tree->bb_root == parent) {
		btree_node_free(parent, tree, tx);
		btree_root_set(tree, node1);
		mem_update(tree, tx, tree, sizeof(struct m0_be_btree));
	} else {
		/* Update affected memory regions */
		btree_node_update(parent, tree, tx);
	}

	btree_node_update(node1, tree, tx);

	M0_LEAVE();
	return node1;
}


static void be_btree_move_parent_key_to_right_child(struct m0_be_bnode *parent,
						    struct m0_be_bnode *lch,
						    struct m0_be_bnode *rch,
						    unsigned int        idx)
{
	unsigned int i = rch->bt_num_active_key;

	while (i > 0) {
		rch->bt_kv_arr[i] = rch->bt_kv_arr[i - 1];
		rch->bt_child_arr[i + 1] = rch->bt_child_arr[i];
		--i;
	}
	rch->bt_child_arr[1] = rch->bt_child_arr[0];
	rch->bt_kv_arr[0] = parent->bt_kv_arr[idx];
	rch->bt_child_arr[0] =
			lch->bt_child_arr[lch->bt_num_active_key];
	lch->bt_child_arr[lch->bt_num_active_key] = NULL;
	parent->bt_kv_arr[idx] =
			lch->bt_kv_arr[lch->bt_num_active_key-1];
	lch->bt_num_active_key--;
	rch->bt_num_active_key++;
}

static void be_btree_move_parent_key_to_left_child(struct m0_be_bnode *parent,
						   struct m0_be_bnode *lch,
						   struct m0_be_bnode *rch,
						   unsigned int        idx)
{
	unsigned int i;

	lch->bt_kv_arr[lch->bt_num_active_key] =
					parent->bt_kv_arr[idx];
	lch->bt_child_arr[lch->bt_num_active_key + 1] =
					rch->bt_child_arr[0];
	lch->bt_num_active_key++;
	parent->bt_kv_arr[idx] = rch->bt_kv_arr[0];
	i = 0;
	while (i < rch->bt_num_active_key - 1) {
		rch->bt_kv_arr[i] = rch->bt_kv_arr[i + 1];
		rch->bt_child_arr[i] = rch->bt_child_arr[i + 1];
		++i;
	}
	rch->bt_child_arr[i] = rch->bt_child_arr[i + 1];
	rch->bt_child_arr[i + 1] = NULL;
	rch->bt_num_active_key--;
}

/**
*   Move parent key to child.
*
*   This routine will move keys from parent to left or right child node.
*
*   @param tree pointer to btree.
*   @param tx pointer to BE transaction.
*   @param parent pointer to parent node.
*   @param idx Index of the child array of parent node.
*   @param pos position to move keys to LEFT or RIGHT child.
*/
static void be_btree_move_parent_key(struct m0_be_btree	  *tree,
				     struct m0_be_tx	  *tx,
				     struct m0_be_bnode	  *parent,
				     enum position_t	   pos,
				     unsigned int          idx)
{
	struct m0_be_bnode *lch;
	struct m0_be_bnode *rch;

	M0_ENTRY("n=%p i=%d dir=%d", parent, idx, pos);

	if (pos == P_RIGHT)
		idx--;

	lch = parent->bt_child_arr[idx];
	rch = parent->bt_child_arr[idx + 1];

	if (pos == P_LEFT)
		be_btree_move_parent_key_to_left_child(parent, lch, rch, idx);
	else
		be_btree_move_parent_key_to_right_child(parent, lch, rch, idx);

	/* re-calculate checksum after all fields has been updated */
	m0_format_footer_update(lch);
	m0_format_footer_update(rch);
	m0_format_footer_update(parent);

	/* Update affected memory regions in tx: */
	btree_node_update(parent, tree, tx);
	btree_node_update(lch, tree, tx);
	btree_node_update(rch, tree, tx);

	M0_LEAVE();
}

/**
*   Delete a key from node.
*
*   This routine will delete a key from node in btree.
*
*   @param tree pointer to btree.
*   @param tx pointer to BE transaction.
*   @param node_pos pointer to node position from which key would to be deleted.
*/
void be_btree_delete_key_from_node(struct m0_be_btree	 *tree,
				   struct m0_be_tx	 *tx,
				   struct btree_node_pos *bnode_pos)
{
	struct 		m0_be_bnode *bnode = bnode_pos->bnp_node;
	unsigned int 	idx;

	if (bnode->bt_isleaf) {
		idx = bnode_pos->bnp_index;

		btree_pair_release(tree, tx, &bnode->bt_kv_arr[idx]);

		while (idx < bnode->bt_num_active_key - 1) {
			bnode->bt_kv_arr[idx] = bnode->bt_kv_arr[idx+1];
			++idx;
		}
		/*
		 * There are chances that last key value might have swapped.
		 * btree_node_update() captures key values based on
		 * bt_num_active_key.
		 * Capture key values here to avoid checksum mismatch.
		 */
		if(bnode_pos->bnp_index == (bnode->bt_num_active_key - 1)) {
			mem_update(tree, tx,
				   &bnode->bt_kv_arr[bnode_pos->bnp_index],
				   sizeof
				   bnode->bt_kv_arr[bnode_pos->bnp_index]);
		}

		bnode->bt_num_active_key--;

		/* re-calculate checksum after all fields has been updated */
		m0_format_footer_update(bnode);

		if (bnode->bt_num_active_key == 0 && bnode != tree->bb_root)
			btree_node_free(bnode, tree, tx);
		else
			/* Update affected memory regions in tx: */
			btree_node_update(bnode, tree, tx);
	}
}

static void btree_node_child_delete(struct m0_be_btree    *btree,
				    struct m0_be_tx       *tx,
				    struct m0_be_bnode    *node,
				    unsigned               index,
				    struct btree_node_pos *child,
				    bool		   left)
{
	M0_ASSERT(child->bnp_node->bt_isleaf);
	M0_LOG(M0_DEBUG, "swap%s with n=%p i=%d", left ? "L" : "R",
						  child->bnp_node,
						  child->bnp_index);
	M0_SWAP(child->bnp_node->bt_kv_arr[child->bnp_index],
		node->bt_kv_arr[index]);
	/*
	 * Update checksum for parent, for child it will be updated
	 * in delete_key_from_node().
	 */
	btree_node_keyval_update(node, btree, tx, index);
}
/**
*   Delete a key from btree.
*
*   This routine will delete the entry specified by @key.
*
*   @param tree pointer to btree.
*   @param tx pointer to BE transaction.
*   @param node pointer to root node on btree.
*   @param key pointer to key which would needs to be deleted.
*   @return 0 (success) or -1 (failure).
*/
static int be_btree_delete_key(struct m0_be_btree *tree,
			       struct m0_be_tx    *tx,
			       struct m0_be_bnode *bnode,
			       void               *key)
{
	bool			outerloop = true;
	struct m0_be_bnode     *righsib;
	struct m0_be_bnode     *leftsib;
	struct m0_be_bnode     *p_node = NULL;
	struct btree_node_pos	child;
	struct btree_node_pos	bnode_pos;
	int			rc = -1;
	unsigned int		iter;
	unsigned int		idx;

	M0_PRE(btree_invariant(tree));
	M0_PRE(btree_node_invariant(tree, tree->bb_root, true));
	M0_PRE_EX(btree_node_subtree_invariant(tree, tree->bb_root));

	M0_ENTRY("n=%p", bnode);

	while (outerloop) {
		while (true) {
			/* Check if keys are available in bnode */
			if (bnode->bt_num_active_key == 0) {
				outerloop = false;
				break;
			}

			/*  Retrieve index of the key equal to or greater than*/
			/*  key being searched */
			iter = 0;
			while (iter < bnode->bt_num_active_key &&
			       key_gt(tree, key,
				      bnode->bt_kv_arr[iter].btree_key))
				iter++;

			idx = iter;

			/* check if key is found */
			if (iter < bnode->bt_num_active_key &&
			    key_eq(tree, key, bnode->bt_kv_arr[iter].btree_key))
				break;

			/* Reached leaf node, nothing left to search */
			if (bnode->bt_isleaf) {
				outerloop = false;
				break;
			}

			p_node = bnode;
			bnode = bnode->bt_child_arr[iter];
			if (bnode == NULL)  {
				outerloop = false;
				break;
			}

			if (iter == p_node->bt_num_active_key) {
				leftsib = p_node->bt_child_arr[iter - 1];
				righsib = NULL;
			} else if (iter == 0) {
				leftsib = NULL;
				righsib = p_node->bt_child_arr[iter + 1];
			} else {
				leftsib = p_node->bt_child_arr[iter - 1];
				righsib = p_node->bt_child_arr[iter + 1];
			}

			if (bnode->bt_num_active_key == BTREE_FAN_OUT - 1 &&
				p_node) {
				if (righsib &&
				   (righsib->bt_num_active_key >
							BTREE_FAN_OUT - 1)) {
					be_btree_move_parent_key(tree, tx,
							p_node, P_LEFT, iter);
				} else if (leftsib &&
				   (leftsib->bt_num_active_key >
							BTREE_FAN_OUT - 1)) {
					be_btree_move_parent_key(tree, tx,
							p_node, P_RIGHT, iter);
				} else if (leftsib &&
				   (leftsib->bt_num_active_key ==
							BTREE_FAN_OUT - 1)) {
					M0_LOG(M0_DEBUG, "mergeL");
					bnode = be_btree_merge_siblings(tx,
									tree,
									p_node,
									iter-1);
				} else if (righsib &&
				   (righsib->bt_num_active_key ==
							BTREE_FAN_OUT - 1)) {
					M0_LOG(M0_DEBUG, "mergeR");
					bnode = be_btree_merge_siblings(tx,
									tree,
									p_node,
									iter);
				}
			}
		}

		if (!outerloop)
			break;

		M0_LOG(M0_DEBUG, "found bnode=%p lf=%d nr=%d idx=%d", bnode,
		       !!bnode->bt_isleaf, bnode->bt_num_active_key, idx);
		rc = 0;

		M0_ASSERT(ergo(bnode->bt_isleaf && bnode != tree->bb_root,
			       bnode->bt_num_active_key > BTREE_FAN_OUT - 1));

		/* Node with the key is found and its leaf node, leaf node has
		 *  keys greater than the minimum required, remove key
		*/
		if (bnode->bt_isleaf &&
		   (bnode->bt_num_active_key > BTREE_FAN_OUT - 1 ||
		    bnode == tree->bb_root)) {
			M0_LOG(M0_DEBUG, "case1");
			bnode_pos.bnp_node = bnode;
			bnode_pos.bnp_index = idx;
			be_btree_delete_key_from_node(tree, tx, &bnode_pos);
			break;
		} else {
			M0_ASSERT(!bnode->bt_isleaf);
		}

		/* Internal node with key is found */
		M0_LOG(M0_DEBUG, "case2");
		if (bnode->bt_child_arr[idx]->bt_num_active_key >
							BTREE_FAN_OUT - 1) {
			be_btree_get_max_key_pos(bnode->bt_child_arr[idx],
						 &child);
			btree_node_child_delete(tree, tx, bnode,
						idx, &child, false);
			bnode = bnode->bt_child_arr[idx];
		} else if (bnode->bt_child_arr[idx + 1]->bt_num_active_key >
			   BTREE_FAN_OUT - 1) {
			be_btree_get_min_key_pos(bnode->bt_child_arr[idx + 1],
						 &child);
			btree_node_child_delete(tree, tx, bnode,
						idx, &child, true);
			bnode = bnode->bt_child_arr[idx + 1];
		} else {
			M0_LOG(M0_DEBUG, "case2-merge");
			bnode = be_btree_merge_siblings(tx, tree, bnode, idx);
		}
		continue;
	}

	M0_POST(btree_invariant(tree));
	M0_POST(btree_node_invariant(tree, tree->bb_root, true));
	M0_POST_EX(btree_node_subtree_invariant(tree, tree->bb_root));
	M0_LEAVE("rc=%d", rc);
	return M0_RC(rc);
}

static void node_push(struct m0_be_btree_cursor *it, struct m0_be_bnode *node,
				int idx)
{
	struct m0_be_btree_cursor_stack_entry *se;

	M0_ASSERT(it->bc_stack_pos < ARRAY_SIZE(it->bc_stack));
	se = &it->bc_stack[it->bc_stack_pos++];
	se->bs_node = node;
	se->bs_idx  = idx;
}

static struct m0_be_bnode *node_pop(struct m0_be_btree_cursor *it, int *idx)
{
	struct m0_be_bnode			*node = NULL;
	struct m0_be_btree_cursor_stack_entry	*se;

	if (it->bc_stack_pos > 0) {
		se = &it->bc_stack[--it->bc_stack_pos];
		node = se->bs_node;
		*idx = se->bs_idx;
	}
	return node;
}

/**
*   Get a node containing the given key.
*
*   This routine will return node position containing specified @key.
*
*   @param it pointer to btree cursor used to traverse btree.
*   @param key pointer to key which is used to search node.
*   @param slant bool to decide searching needs to be on leaf node.
*                if true, search leaf node, else search in non-leaf node
*   @return struct btree_node_pos.
*/
struct btree_node_pos
be_btree_get_btree_node(struct m0_be_btree_cursor *it, const void *key, bool slant)
{
	int 			 idx;
	struct m0_be_btree 	*tree = it->bc_tree;
	struct m0_be_bnode 	*bnode = tree->bb_root;
	struct btree_node_pos    bnode_pos = { .bnp_node = NULL };

	it->bc_stack_pos = 0;

	while (true) {
		/*  Retrieve index of the key equal to or greater than */
		/*  the key being searched */
		idx = 0;
		while (idx < bnode->bt_num_active_key &&
		       key_gt(tree, key, bnode->bt_kv_arr[idx].btree_key)) {
			idx++;
		}

		/*  If key is found, copy key-value pair */
		if (idx < bnode->bt_num_active_key &&
		    key_eq(tree, key, bnode->bt_kv_arr[idx].btree_key)) {
			bnode_pos.bnp_node = bnode;
			bnode_pos.bnp_index = idx;
			break;
		}

		/*  Return NULL in case of leaf node and did not get key*/
		if (bnode->bt_isleaf) {
			while (bnode != NULL && idx == bnode->bt_num_active_key)
				bnode = node_pop(it, &idx);
			if (slant && bnode != NULL) {
				bnode_pos.bnp_node = bnode;
				bnode_pos.bnp_index = idx;
			}
			break;
		}
		/*  Move to a child node */
		node_push(it, bnode, idx);
		bnode = bnode->bt_child_arr[idx];
	}
	return bnode_pos;
}

/*
 * This function is used to destory the btree
 * @param btree The btree to be destroyed
 * @param tx    The pointer to tx
 */
static void be_btree_destroy(struct m0_be_btree *btree, struct m0_be_tx *tx)
{
	int i = 0;
	struct m0_be_bnode *head;
	struct m0_be_bnode *tail;
	struct m0_be_bnode *node_to_del;

	head = btree->bb_root;
	tail = head;

	head->bt_next = NULL;
	tail->bt_next = NULL;
	while (head != NULL) {
		if (!head->bt_isleaf) {
			i = 0;
			while (i < head->bt_num_active_key + 1) {
				tail->bt_next = head->bt_child_arr[i];
				m0_format_footer_update(tail);
				tail = tail->bt_next;
				tail->bt_next = NULL;
				++i;
			}
			m0_format_footer_update(tail);
		}
		node_to_del = head;
		head = head->bt_next;
		i = 0;
		while (i < node_to_del->bt_num_active_key) {
			btree_pair_release(btree, tx,
					   &node_to_del->bt_kv_arr[i]);
			++i;
		}
		btree_node_free(node_to_del, btree, tx);
	}
	m0_format_footer_update(head);
	btree->bb_backlink.bli_type = M0_BBT_INVALID;
	btree_root_set(btree, NULL);
	mem_update(btree, tx, btree, sizeof(struct m0_be_btree));
}

/**
 * Truncate btree: truncate the tree till the limit provided in argument.
 *
 * That function can be called multiple times, having maximum number of records
 * to be deleted limited to not exceed transaction capacity.
 * After first call tree can't be used for operations other than truncate or
 * destroy.
 *
 * @param btee btree to truncate
 * @param tx transaction
 * @param limit maximum number of records to delete
 */
static void btree_truncate(struct m0_be_btree *btree, struct m0_be_tx *tx,
			   m0_bcount_t limit)

{
	struct m0_be_bnode *node;
	struct m0_be_bnode *parent;
	int                 i;

	/* Add one more reserve for non-leaf node. */
	if (limit > 1)
		limit--;

	node = btree->bb_root;

	while (node != NULL && limit > 0) {
		parent = NULL;
		if (!node->bt_isleaf) {
			parent = node;
			i = node->bt_num_active_key;
			node = node->bt_child_arr[i];
		}
		if (!node->bt_isleaf)
			continue;

		while (node->bt_num_active_key > 0 && limit > 0) {
			limit--;
			node->bt_num_active_key--;
			i = node->bt_num_active_key;
			btree_pair_release(btree, tx, &node->bt_kv_arr[i]);
		}
		m0_format_footer_update(node);
		if (node->bt_num_active_key > 0)
			continue;
		/*
		 * Cleared all keys in the leaf node.
		 */
		if (node == btree->bb_root) {
			/*
			 * Do not destroy tree root. Keep empty
			 * tree alive. So, we are done.
			 */
			break;
		}
		btree_node_free(node, btree, tx);
		if (parent != NULL) {
			/*
			 * If this is not a root (checked above), sure node has
			 * a parent.
			 * If parent is empty, reclassify it to a leaf.
			 */
			i = parent->bt_num_active_key;
			/*
			 * Release parent->bt_num_active_key -1  key-val pair
			 * after freeing
			 * node->bt_child_arr[node->bt_num_active_key]
			 * leaf node
			 */
			if (i > 0) {
				btree_pair_release(btree, tx,
						&parent->bt_kv_arr[i-1]);

				if (limit > 0)
					limit--;
			}

			if (i == 0)
				parent->bt_isleaf = true;
			else
				parent->bt_num_active_key--;
			if (parent == btree->bb_root &&
			    parent->bt_num_active_key == 0) {
				/*
				 * Cleared the root, but still have 1
				 * child. Move the root.
				 */
				btree_root_set(btree, parent->bt_child_arr[0]);
				mem_update(btree, tx, btree,
					   sizeof(struct m0_be_btree));
				btree_node_free(parent, btree, tx);
			} else {
				m0_format_footer_update(parent);
			}
			/* Simplify our life: restart from the root. */
			node = btree->bb_root;
		}
	}
	m0_format_footer_update(btree->bb_root);
}

/*
 * This function is used to search a node in the btree
 * @param btree The tree to be searched
 * @param key   Key of the node to be searched
 * @return      key-value pair
 */
static struct be_btree_key_val *be_btree_search(struct m0_be_btree *btree,
						void *key)
{
	struct m0_be_btree_cursor btree_cursor;
	struct btree_node_pos	  node_pos;
	struct be_btree_key_val   *key_val = NULL;

	btree_cursor.bc_tree = btree;
	node_pos = be_btree_get_btree_node(&btree_cursor, key, false);

	if (node_pos.bnp_node)
		key_val = &node_pos.bnp_node->bt_kv_arr[node_pos.bnp_index];

	return key_val;
}

/**
*   Get a maximum key in btree.
*
*   This routine will return a max key in btree.
*
*   @param tree pointer to btree.
*   @return maximum key in btree.
*/
static void *be_btree_get_max_key(struct m0_be_btree *tree)
{
	struct btree_node_pos node;

	be_btree_get_max_key_pos(tree->bb_root, &node);
	if (node.bnp_node->bt_num_active_key == 0)
		return NULL;
	else
		return node.bnp_node->bt_kv_arr[node.bnp_index].btree_key;
}

/**
*   Get a minimum key in btree.
*
*   This routine will return a min key in btree.
*
*   @param tree pointer to btree.
*   @return minimum key in btree.
*/
static void *be_btree_get_min_key(struct m0_be_btree *tree)
{
	struct btree_node_pos node;

	be_btree_get_min_key_pos(tree->bb_root, &node);
	if (node.bnp_node->bt_num_active_key == 0)
		return NULL;
	else
		return node.bnp_node->bt_kv_arr[0].btree_key;
}

static void btree_pair_release(struct m0_be_btree *btree, struct m0_be_tx *tx,
			       struct be_btree_key_val  *kv)
{
	mem_free(btree, tx, kv->btree_key);
}

/**
 * Inserts or updates value by key
 * @param tree The btree
 * @param tx The transaction
 * @param op The operation
 * @param key Key of the node to be searched
 * @param value Value to be copied
 * @param optype Save operation type: insert, update or overwrite
 * @param zonemask Bitmask of allowed allocation zones for memory allocation
 */
static void btree_save(struct m0_be_btree        *tree,
		       struct m0_be_tx           *tx,
		       struct m0_be_op           *op,
		       const struct m0_buf       *key,
		       const struct m0_buf       *val,
		       struct m0_be_btree_anchor *anchor,
		       enum btree_save_optype     optype,
		       uint64_t                   zonemask)
{
	m0_bcount_t        ksz;
	m0_bcount_t        vsz;
	struct be_btree_key_val   new_kv;
	struct be_btree_key_val  *cur_kv;
	bool               val_overflow = false;

	M0_ENTRY("tree=%p", tree);

	M0_PRE(M0_IN(optype, (BTREE_SAVE_INSERT, BTREE_SAVE_UPDATE,
			      BTREE_SAVE_OVERWRITE)));

	switch (optype) {
		case BTREE_SAVE_OVERWRITE:
			M0_BE_CREDIT_DEC(M0_BE_CU_BTREE_DELETE, tx);
			/* fallthrough */
		case BTREE_SAVE_INSERT:
			M0_BE_CREDIT_DEC(M0_BE_CU_BTREE_INSERT, tx);
			break;
		case BTREE_SAVE_UPDATE:
			M0_BE_CREDIT_DEC(M0_BE_CU_BTREE_UPDATE, tx);
			break;
	}

	btree_op_fill(op, tree, tx, optype == BTREE_SAVE_UPDATE ?
		      M0_BBO_UPDATE : M0_BBO_INSERT, NULL);

	m0_be_op_active(op);
	m0_rwlock_write_lock(btree_rwlock(tree));
	if (anchor != NULL) {
		anchor->ba_tree = tree;
		anchor->ba_write = true;
		vsz = anchor->ba_value.b_nob;
		anchor->ba_value.b_addr = NULL;
	} else
		vsz = val->b_nob;

	if (M0_FI_ENABLED("already_exists"))
		goto fi_exist;

	op_tree(op)->t_rc = 0;
	cur_kv = be_btree_search(tree, key->b_addr);
	if ((cur_kv == NULL && optype != BTREE_SAVE_UPDATE) ||
	    (cur_kv != NULL && optype == BTREE_SAVE_UPDATE) ||
	    optype == BTREE_SAVE_OVERWRITE) {
		if (cur_kv != NULL && optype != BTREE_SAVE_INSERT) {
			if (vsz > be_btree_vsize(tree, cur_kv->btree_val)) {
				/*
				 * The size of new value is greater than the
				 * size of old value, old value area can not be
				 * re-used for new value. Delete old key/value
				 * and add new key/value.
				 */
				op_tree(op)->t_rc =
					be_btree_delete_key(tree, tx,
							 tree->bb_root,
							 cur_kv->btree_key);
				val_overflow = true;
			} else {
				/*
				 * The size of new value is less than or equal
				 * to the size of old value, simply rewrite
				 * old value in this case.
				 */
				if (val != NULL) {
					memcpy(cur_kv->btree_val, val->b_addr,
					       val->b_nob);
					mem_update(tree, tx, cur_kv->btree_val,
						   val->b_nob);
				} else
					anchor->ba_value.b_addr =
							cur_kv->btree_val;
			}
		}

		if (op_tree(op)->t_rc == 0 &&
		    (cur_kv == NULL || val_overflow)) {
			/* Avoid CPU alignment overhead on values. */
			ksz = m0_align(key->b_nob, sizeof(void*));
			new_kv.btree_key =
				      mem_alloc(tree, tx, ksz + vsz, zonemask);
			new_kv.btree_val = new_kv.btree_key + ksz;
			memcpy(new_kv.btree_key, key->b_addr, key->b_nob);
			memset(new_kv.btree_key + key->b_nob, 0,
							ksz - key->b_nob);
			if (val != NULL) {
				memcpy(new_kv.btree_val, val->b_addr, vsz);
				mem_update(tree, tx,
						new_kv.btree_key, ksz + vsz);
			} else {
				mem_update(tree, tx, new_kv.btree_key, ksz);
				anchor->ba_value.b_addr = new_kv.btree_val;
			}

			be_btree_insert_newkey(tree, tx, &new_kv);
		}
	} else {
fi_exist:
		op_tree(op)->t_rc = -EEXIST;
		M0_LOG(M0_NOTICE, "the key entry at %p already exist",
			key->b_addr);
	}

	if (anchor == NULL)
		m0_rwlock_write_unlock(btree_rwlock(tree));
	m0_be_op_done(op);
	M0_LEAVE("tree=%p", tree);
}


/* ------------------------------------------------------------------
 * Btree external interfaces implementation
 * ------------------------------------------------------------------ */

M0_INTERNAL void m0_be_btree_init(struct m0_be_btree *tree,
				  struct m0_be_seg   *seg,
				  const struct m0_be_btree_kv_ops *ops)
{
	M0_ENTRY("tree=%p seg=%p", tree, seg);
	M0_PRE(ops != NULL);

	m0_rwlock_init(btree_rwlock(tree));
	tree->bb_ops = ops;
	tree->bb_seg = seg;

	if (!m0_be_seg_contains(seg, tree->bb_root))
		tree->bb_root = NULL;
	M0_LEAVE();
}

M0_INTERNAL void m0_be_btree_fini(struct m0_be_btree *tree)
{
	M0_ENTRY("tree=%p", tree);
	M0_PRE(ergo(tree->bb_root != NULL && tree->bb_header.hd_magic != 0,
		    btree_invariant(tree)));
	m0_rwlock_fini(btree_rwlock(tree));
	M0_LEAVE();
}

M0_INTERNAL void m0_be_btree_create(struct m0_be_btree  *tree,
				    struct m0_be_tx     *tx,
				    struct m0_be_op     *op,
				    const struct m0_fid *btree_fid)
{
	M0_ENTRY("tree=%p", tree);
	M0_PRE(tree->bb_root == NULL && tree->bb_ops != NULL);
	M0_PRE(m0_fid_tget(btree_fid) == m0_btree_fid_type.ft_id);
	/* M0_PRE(m0_rwlock_is_locked(tx->t_be.b_tx.te_lock)); */

	btree_op_fill(op, tree, tx, M0_BBO_CREATE, NULL);

	m0_be_op_active(op);
	m0_rwlock_write_lock(btree_rwlock(tree));

	btree_create(tree, tx, btree_fid);

	m0_rwlock_write_unlock(btree_rwlock(tree));
	op_tree(op)->t_rc = 0;
	m0_be_op_done(op);
	M0_POST(btree_invariant(tree));
	M0_POST(btree_node_invariant(tree, tree->bb_root, true));
	M0_POST_EX(btree_node_subtree_invariant(tree, tree->bb_root));
	M0_LEAVE();
}

M0_INTERNAL void m0_be_btree_destroy(struct m0_be_btree *tree,
				     struct m0_be_tx *tx,
				     struct m0_be_op *op)
{
	M0_ENTRY("tree=%p", tree);
	/* XXX TODO The right approach to pursue is to let the user
	 * destroy only empty trees. So ideally here would be
	 * M0_PRE(m0_be_btree_is_empty(tree)); */
	M0_PRE(tree->bb_root != NULL && tree->bb_ops != NULL);
	M0_PRE(btree_invariant(tree));
	M0_PRE(btree_node_invariant(tree, tree->bb_root, true));
	M0_PRE_EX(btree_node_subtree_invariant(tree, tree->bb_root));

	btree_op_fill(op, tree, tx, M0_BBO_DESTROY, NULL);

	m0_be_op_active(op);
	m0_rwlock_write_lock(btree_rwlock(tree));

	be_btree_destroy(tree, tx);

	m0_rwlock_write_unlock(btree_rwlock(tree));
	op_tree(op)->t_rc = 0;
	m0_be_op_done(op);
	M0_LEAVE();
}

M0_INTERNAL void m0_be_btree_truncate(struct m0_be_btree *tree,
				      struct m0_be_tx    *tx,
				      struct m0_be_op    *op,
				      m0_bcount_t         limit)
{
	M0_ENTRY("tree=%p", tree);
	M0_PRE(tree->bb_root != NULL && tree->bb_ops != NULL);
	/*
	 * btree_truncate() violates the invariant, by removing the records from
	 * the leaf nodes without merging them. The only thing that can be done
	 * with a tree being truncated, is to truncate it further. Therefore
	 * tree_invariant() cannot be asserted on entry or exit.
	 */
	btree_op_fill(op, tree, tx, M0_BBO_DESTROY, NULL);

	m0_be_op_active(op);
	m0_rwlock_write_lock(btree_rwlock(tree));

	btree_truncate(tree, tx, limit);

	m0_rwlock_write_unlock(btree_rwlock(tree));
	op_tree(op)->t_rc = 0;
	m0_be_op_done(op);
	M0_LEAVE();
}

static void btree_node_alloc_credit(const struct m0_be_btree     *tree,
					  struct m0_be_tx_credit *accum)
{
	btree_mem_alloc_credit(tree, sizeof(struct m0_be_bnode), accum);
}

static void btree_node_update_credit(struct m0_be_tx_credit *accum,
				     m0_bcount_t nr)
{
	struct m0_be_tx_credit cred = {};

	/* struct m0_be_bnode update x2 */
	m0_be_tx_credit_mac(&cred,
			    &M0_BE_TX_CREDIT_TYPE(struct m0_be_bnode), 2);

	m0_be_tx_credit_mac(accum, &cred, nr);
}

static void btree_node_free_credit(const struct m0_be_btree     *tree,
					 struct m0_be_tx_credit *accum)
{
	btree_mem_free_credit(tree, sizeof(struct m0_be_bnode), accum);
	m0_be_tx_credit_add(accum,
			    &M0_BE_TX_CREDIT_TYPE(uint64_t));
	btree_node_update_credit(accum, 1); /* for parent */
}

/* XXX */
static void btree_credit(const struct m0_be_btree     *tree,
			       struct m0_be_tx_credit *accum)
{
	uint32_t height;

	height = tree->bb_root == NULL ? 2 : tree->bb_root->bt_level;
	m0_be_tx_credit_mul(accum, 2*height + 1);
}

static void btree_rebalance_credit(const struct m0_be_btree *tree,
				   struct m0_be_tx_credit   *accum)
{
	struct m0_be_tx_credit cred = {};

	btree_node_alloc_credit(tree, &cred);
	btree_node_update_credit(&cred, 1);
	btree_credit(tree, &cred);

	m0_be_tx_credit_add(accum, &cred);
}

static void kv_insert_credit(const struct m0_be_btree     *tree,
				   m0_bcount_t             ksize,
				   m0_bcount_t             vsize,
				   struct m0_be_tx_credit *accum)
{
	struct m0_be_tx_credit kv_update_cred;

	ksize = m0_align(ksize, sizeof(void*));
	kv_update_cred = M0_BE_TX_CREDIT(1, ksize + vsize);
	btree_mem_alloc_credit(tree, ksize + vsize, accum);
	m0_be_tx_credit_add(accum, &kv_update_cred);
}

static void kv_delete_credit(const struct m0_be_btree *tree,
			     m0_bcount_t               ksize,
			     m0_bcount_t               vsize,
			     struct m0_be_tx_credit   *accum)
{
	btree_mem_free_credit(tree, m0_align(ksize, sizeof(void*)) + vsize,
			      accum);
	m0_be_tx_credit_add(accum,
			    &M0_BE_TX_CREDIT_TYPE(struct be_btree_key_val));
	/* capture parent csum in case values swapped during delete */
	m0_be_tx_credit_add(accum,
			    &M0_BE_TX_CREDIT(1,
					     sizeof(struct m0_format_footer)));
}

static void btree_node_split_child_credit(const struct m0_be_btree *tree,
					  struct m0_be_tx_credit   *accum)
{
	btree_node_alloc_credit(tree, accum);
	btree_node_update_credit(accum, 3);
}

static void insert_credit(const struct m0_be_btree *tree,
			  m0_bcount_t               nr,
			  m0_bcount_t               ksize,
			  m0_bcount_t               vsize,
			  struct m0_be_tx_credit   *accum,
			  bool                      use_current_height)
{
	struct m0_be_tx_credit cred = {};
	uint32_t               height;

	if (use_current_height)
		height = tree->bb_root == NULL ? 2 : tree->bb_root->bt_level;
	else
		height = BTREE_HEIGHT_MAX;

	/* for be_btree_insert_into_nonfull() */
	btree_node_split_child_credit(tree, &cred);
	m0_be_tx_credit_mul(&cred, height);
	btree_node_update_credit(&cred, 1);

	/* for be_btree_insert_newkey() */
	btree_node_alloc_credit(tree, &cred);
	btree_node_split_child_credit(tree, &cred);
	m0_be_tx_credit_add(&cred,
			    &M0_BE_TX_CREDIT(1, sizeof(struct m0_be_btree)));

	kv_insert_credit(tree, ksize, vsize, &cred);
	m0_be_tx_credit_mac(accum, &cred, nr);
}

static void delete_credit(const struct m0_be_btree *tree,
			  m0_bcount_t               nr,
			  m0_bcount_t               ksize,
			  m0_bcount_t               vsize,
			  struct m0_be_tx_credit   *accum)
{
	struct m0_be_tx_credit cred = {};

	kv_delete_credit(tree, ksize, vsize, &cred);
	btree_node_update_credit(&cred, 1);
	btree_node_free_credit(tree, &cred);
	btree_rebalance_credit(tree, &cred);
	m0_be_tx_credit_mac(accum, &cred, nr);
}


M0_INTERNAL void m0_be_btree_insert_credit(const struct m0_be_btree *tree,
					   m0_bcount_t               nr,
					   m0_bcount_t               ksize,
					   m0_bcount_t               vsize,
					   struct m0_be_tx_credit   *accum)
{
	insert_credit(tree, nr, ksize, vsize, accum, false);
	M0_BE_CREDIT_INC(nr, M0_BE_CU_BTREE_INSERT, accum);
}

M0_INTERNAL void m0_be_btree_insert_credit2(const struct m0_be_btree *tree,
					    m0_bcount_t               nr,
					    m0_bcount_t               ksize,
					    m0_bcount_t               vsize,
					    struct m0_be_tx_credit   *accum)
{
	insert_credit(tree, nr, ksize, vsize, accum, true);
	M0_BE_CREDIT_INC(nr, M0_BE_CU_BTREE_INSERT, accum);
}

M0_INTERNAL void m0_be_btree_delete_credit(const struct m0_be_btree     *tree,
						 m0_bcount_t             nr,
						 m0_bcount_t             ksize,
						 m0_bcount_t             vsize,
						 struct m0_be_tx_credit *accum)
{
	delete_credit(tree, nr, ksize, vsize, accum);
	M0_BE_CREDIT_INC(nr, M0_BE_CU_BTREE_DELETE, accum);
}

M0_INTERNAL void m0_be_btree_update_credit(const struct m0_be_btree     *tree,
						 m0_bcount_t             nr,
						 m0_bcount_t             vsize,
						 struct m0_be_tx_credit *accum)
{
	struct m0_be_tx_credit cred = {};
	struct m0_be_tx_credit val_update_cred =
		M0_BE_TX_CREDIT(1, vsize + sizeof(struct be_btree_key_val));
	/* capture parent checksum */
	m0_be_tx_credit_add(&val_update_cred,
			    &M0_BE_TX_CREDIT(1,
					     sizeof(struct m0_format_footer)));

	/* @todo: is alloc/free credits are really needed??? */
	btree_mem_alloc_credit(tree, vsize, &cred);
	btree_mem_free_credit(tree, vsize, &cred);
	m0_be_tx_credit_add(&cred, &val_update_cred);
	m0_be_tx_credit_mac(accum, &cred, nr);

	M0_BE_CREDIT_INC(nr, M0_BE_CU_BTREE_UPDATE, accum);
}

M0_INTERNAL void m0_be_btree_update_credit2(const struct m0_be_btree *tree,
					    m0_bcount_t               nr,
					    m0_bcount_t               ksize,
					    m0_bcount_t               vsize,
					    struct m0_be_tx_credit   *accum)
{
	delete_credit(tree, nr, ksize, vsize, accum);
	insert_credit(tree, nr, ksize, vsize, accum, true);
	M0_BE_CREDIT_INC(nr, M0_BE_CU_BTREE_UPDATE, accum);
}

M0_INTERNAL void m0_be_btree_create_credit(const struct m0_be_btree     *tree,
						 m0_bcount_t             nr,
						 struct m0_be_tx_credit *accum)
{
	struct m0_be_tx_credit cred = {};

	btree_node_alloc_credit(tree, &cred);
	btree_node_update_credit(&cred, 1);
	m0_be_tx_credit_mac(accum, &cred, nr);
}

static int btree_count_items(struct m0_be_btree *tree, m0_bcount_t *ksize,
			     m0_bcount_t *vsize)
{
	struct m0_be_btree_cursor cur;
	struct m0_be_op          *op = &cur.bc_op;
	struct m0_buf             start;
	int                       count = 0;
	struct m0_buf             key;
	struct m0_buf             val;
	int                       rc;

	*ksize = 0;
	*vsize = 0;
	if (tree->bb_root != NULL) {
		m0_be_btree_cursor_init(&cur, tree);

		M0_SET0(op);
		M0_BE_OP_SYNC_WITH(op, m0_be_btree_minkey(tree, op, &start));

		rc = m0_be_btree_cursor_get_sync(&cur, &start, true);

		while (rc != -ENOENT) {
			m0_be_btree_cursor_kv_get(&cur, &key, &val);
			if (key.b_nob > *ksize)
				*ksize = key.b_nob;
			if (val.b_nob > *vsize)
				*vsize = val.b_nob;
			rc = m0_be_btree_cursor_next_sync(&cur);
			++count;
		}

		m0_be_btree_cursor_fini(&cur);
	}

	return count;
}

M0_INTERNAL void m0_be_btree_destroy_credit(struct m0_be_btree     *tree,
					    struct m0_be_tx_credit *accum)
{
	/* XXX
	 * Current implementation of m0_be_btree_destroy_credit() is
	 * not right. First of all, `tree' parameter must be const.
	 * Secondly, it is user's responsibility to ensure that the
	 * tree being deleted is empty.
	 */
	struct m0_be_tx_credit cred = {};
	int		       nodes_nr;
	int		       items_nr;
	m0_bcount_t	       ksize;
	m0_bcount_t	       vsize;

	nodes_nr = iter_prepare(tree->bb_root, false);
	items_nr = btree_count_items(tree, &ksize, &vsize);
	M0_LOG(M0_DEBUG, "nodes=%d items=%d ksz=%d vsz%d",
		nodes_nr, items_nr, (int)ksize, (int)vsize);

	kv_delete_credit(tree, ksize, vsize, &cred);
	m0_be_tx_credit_mac(accum, &cred, items_nr);

	M0_SET0(&cred);
	btree_node_free_credit(tree, &cred);
	m0_be_tx_credit_mac(accum, &cred, nodes_nr);

	cred = M0_BE_TX_CREDIT_TYPE(struct m0_be_btree);
	m0_be_tx_credit_add(accum, &cred);
}

M0_INTERNAL void m0_be_btree_clear_credit(struct m0_be_btree     *tree,
					  struct m0_be_tx_credit *fixed_part,
					  struct m0_be_tx_credit *single_record,
					  m0_bcount_t            *records_nr)
{
	struct m0_be_tx_credit cred = {};
	int                    nodes_nr;
	int                    items_nr;
	m0_bcount_t            ksize;
	m0_bcount_t            vsize;

	nodes_nr = iter_prepare(tree->bb_root, false);
	items_nr = btree_count_items(tree, &ksize, &vsize);
	items_nr++;
	M0_LOG(M0_DEBUG, "nodes=%d items=%d ksz=%d vsz%d",
		nodes_nr, items_nr, (int)ksize, (int)vsize);

	M0_SET0(single_record);
	kv_delete_credit(tree, ksize, vsize, single_record);
	*records_nr = items_nr;

	M0_SET0(&cred);
	btree_node_free_credit(tree, &cred);
	m0_be_tx_credit_mac(fixed_part, &cred, nodes_nr);

	cred = M0_BE_TX_CREDIT_TYPE(struct m0_be_btree);
	m0_be_tx_credit_add(fixed_part, &cred);
	m0_be_tx_credit_add(fixed_part, single_record);
}

static void be_btree_insert(struct m0_be_btree        *tree,
			    struct m0_be_tx           *tx,
			    struct m0_be_op           *op,
			    const struct m0_buf       *key,
			    const struct m0_buf       *val,
			    struct m0_be_btree_anchor *anchor,
			    uint64_t                   zonemask)
{
	M0_ENTRY("tree=%p", tree);
	M0_PRE(tree->bb_root != NULL && tree->bb_ops != NULL);
	M0_PRE(key->b_nob == be_btree_ksize(tree, key->b_addr));
	M0_PRE((val != NULL) == (anchor == NULL));
	M0_PRE(ergo(anchor == NULL,
		    val->b_nob == be_btree_vsize(tree, val->b_addr)));

	btree_save(tree, tx, op, key, val, anchor, BTREE_SAVE_INSERT, zonemask);

	M0_LEAVE("tree=%p", tree);
}

M0_INTERNAL void m0_be_btree_save(struct m0_be_btree  *tree,
				  struct m0_be_tx     *tx,
				  struct m0_be_op     *op,
				  const struct m0_buf *key,
				  const struct m0_buf *val,
				  bool                 overwrite)
{
	M0_ENTRY("tree=%p", tree);
	M0_PRE(tree->bb_root != NULL && tree->bb_ops != NULL);
	M0_PRE(key->b_nob == be_btree_ksize(tree, key->b_addr));
	M0_PRE(val->b_nob == be_btree_vsize(tree, val->b_addr));

	/* We can't be here during DIX Repair, so never use repair zone. */
	btree_save(tree, tx, op, key, val, NULL, overwrite ?
		   BTREE_SAVE_OVERWRITE : BTREE_SAVE_INSERT,
		   M0_BITS(M0_BAP_NORMAL));

	M0_LEAVE("tree=%p", tree);
}

M0_INTERNAL void m0_be_btree_insert(struct m0_be_btree *tree,
				    struct m0_be_tx *tx,
				    struct m0_be_op *op,
				    const struct m0_buf *key,
				    const struct m0_buf *val)
{
	be_btree_insert(tree, tx, op, key, val, NULL, M0_BITS(M0_BAP_NORMAL));
}

M0_INTERNAL void m0_be_btree_update(struct m0_be_btree *tree,
				    struct m0_be_tx *tx,
				    struct m0_be_op *op,
				    const struct m0_buf *key,
				    const struct m0_buf *val)
{
	M0_ENTRY("tree=%p", tree);
	M0_PRE(tree->bb_root != NULL && tree->bb_ops != NULL);
	M0_PRE(key->b_nob == be_btree_ksize(tree, key->b_addr));
	M0_PRE(val->b_nob == be_btree_vsize(tree, val->b_addr));

	btree_save(tree, tx, op, key, val, NULL, BTREE_SAVE_UPDATE,
		   M0_BITS(M0_BAP_NORMAL));

	M0_LEAVE();
}

M0_INTERNAL void m0_be_btree_delete(struct m0_be_btree *tree,
				    struct m0_be_tx *tx,
				    struct m0_be_op *op,
				    const struct m0_buf *key)
{
	int rc;

	M0_ENTRY("tree=%p", tree);
	M0_PRE(tree->bb_root != NULL && tree->bb_ops != NULL);

	M0_BE_CREDIT_DEC(M0_BE_CU_BTREE_DELETE, tx);

	btree_op_fill(op, tree, tx, M0_BBO_DELETE, NULL);

	m0_be_op_active(op);
	m0_rwlock_write_lock(btree_rwlock(tree));

	op_tree(op)->t_rc = rc = be_btree_delete_key(tree, tx, tree->bb_root,
						     key->b_addr);
	if (rc != 0)
		op_tree(op)->t_rc = -ENOENT;

	m0_rwlock_write_unlock(btree_rwlock(tree));
	m0_be_op_done(op);
	M0_LEAVE("tree=%p", tree);
}

static void be_btree_lookup(struct m0_be_btree *tree,
			    struct m0_be_op *op,
			    const struct m0_buf *key_in,
			    struct m0_buf *key_out,
			    struct m0_buf *value)
{
	struct m0_be_btree_cursor  it;
	struct btree_node_pos      kp;
	struct be_btree_key_val   *kv;
	m0_bcount_t                ksize;
	m0_bcount_t                vsize;

	M0_ENTRY("tree=%p key_in=%p key_out=%p value=%p",
		 tree, key_in, key_out, value);
	M0_PRE(tree->bb_root != NULL && tree->bb_ops != NULL);

	btree_op_fill(op, tree, NULL, M0_BBO_LOOKUP, NULL);

	m0_be_op_active(op);
	m0_rwlock_read_lock(btree_rwlock(tree));

	it.bc_tree = tree;
	kp = be_btree_get_btree_node(&it, key_in->b_addr,
			    /* slant: */ key_out == NULL ? false : true);
	if (kp.bnp_node) {
		kv = &kp.bnp_node->bt_kv_arr[kp.bnp_index];

		vsize = be_btree_vsize(tree, kv->btree_val);
		if (vsize < value->b_nob)
			value->b_nob = vsize;
		/* XXX handle vsize > value->b_nob */
		memcpy(value->b_addr, kv->btree_val, value->b_nob);

		if (key_out != NULL) {
			ksize = be_btree_ksize(tree, kv->btree_key);
			if (ksize < key_out->b_nob)
				key_out->b_nob = ksize;
			memcpy(key_out->b_addr, kv->btree_key, key_out->b_nob);
		}
		op_tree(op)->t_rc = 0;
	} else
		op_tree(op)->t_rc = -ENOENT;

	m0_rwlock_read_unlock(btree_rwlock(tree));
	m0_be_op_done(op);
	M0_LEAVE("rc=%d", op_tree(op)->t_rc);
}

M0_INTERNAL void m0_be_btree_lookup(struct m0_be_btree *tree,
				    struct m0_be_op *op,
				    const struct m0_buf *key,
				    struct m0_buf *value)
{
	be_btree_lookup(tree, op, key, NULL, value);
}

M0_INTERNAL void m0_be_btree_lookup_slant(struct m0_be_btree *tree,
					  struct m0_be_op *op,
					  struct m0_buf *key,
					  struct m0_buf *value)
{
	be_btree_lookup(tree, op, key, key, value);
}

M0_INTERNAL void m0_be_btree_maxkey(struct m0_be_btree *tree,
				    struct m0_be_op *op,
				    struct m0_buf *out)
{
	void *key;

	M0_PRE(tree->bb_root != NULL && tree->bb_ops != NULL);

	btree_op_fill(op, tree, NULL, M0_BBO_MAXKEY, NULL);

	m0_be_op_active(op);
	m0_rwlock_read_lock(btree_rwlock(tree));

	key = be_btree_get_max_key(tree);
	op_tree(op)->t_rc = key == NULL ? -ENOENT : 0;
	m0_buf_init(out, key, key == NULL ? 0 : be_btree_ksize(tree, key));

	m0_rwlock_read_unlock(btree_rwlock(tree));
	m0_be_op_done(op);
}

M0_INTERNAL void m0_be_btree_minkey(struct m0_be_btree *tree,
				    struct m0_be_op *op,
				    struct m0_buf *out)
{
	void *key;

	M0_PRE(tree->bb_root != NULL && tree->bb_ops != NULL);

	btree_op_fill(op, tree, NULL, M0_BBO_MINKEY, NULL);

	m0_be_op_active(op);
	m0_rwlock_read_lock(btree_rwlock(tree));

	key = be_btree_get_min_key(tree);
	op_tree(op)->t_rc = key == NULL ? -ENOENT : 0;
	m0_buf_init(out, key, key == NULL ? 0 : be_btree_ksize(tree, key));

	m0_rwlock_read_unlock(btree_rwlock(tree));
	m0_be_op_done(op);
}

/* ------------------------------------------------------------------
 * Btree external inplace interfaces implementation
 * ------------------------------------------------------------------ */

M0_INTERNAL void m0_be_btree_update_inplace(struct m0_be_btree        *tree,
					    struct m0_be_tx           *tx,
					    struct m0_be_op           *op,
					    const struct m0_buf       *key,
					    struct m0_be_btree_anchor *anchor)
{
	struct be_btree_key_val  *kv;

	M0_ENTRY("tree=%p", tree);
	M0_PRE(tree->bb_root != NULL && tree->bb_ops != NULL);
	M0_PRE(key->b_nob == be_btree_ksize(tree, key->b_addr));

	btree_op_fill(op, tree, tx, M0_BBO_UPDATE, NULL);

	m0_be_op_active(op);
	m0_rwlock_write_lock(btree_rwlock(tree));

	anchor->ba_write = true;
	anchor->ba_tree  = tree;
	kv = be_btree_search(tree, key->b_addr);
	if (kv != NULL) {
		M0_ASSERT(anchor->ba_value.b_nob <=
			  be_btree_vsize(tree, kv->btree_val));
		anchor->ba_value.b_addr = kv->btree_val;
	} else
		op_tree(op)->t_rc = -ENOENT;

	m0_be_op_done(op);
	M0_LEAVE();
}

M0_INTERNAL void m0_be_btree_insert_inplace(struct m0_be_btree        *tree,
					    struct m0_be_tx           *tx,
					    struct m0_be_op           *op,
					    const struct m0_buf       *key,
					    struct m0_be_btree_anchor *anchor,
					    uint64_t                   zonemask)
{
	be_btree_insert(tree, tx, op, key, NULL, anchor, zonemask);
}

M0_INTERNAL void m0_be_btree_save_inplace(struct m0_be_btree        *tree,
					  struct m0_be_tx           *tx,
					  struct m0_be_op           *op,
					  const struct m0_buf       *key,
					  struct m0_be_btree_anchor *anchor,
					  bool                       overwrite,
					  uint64_t                   zonemask)
{
	M0_ENTRY("tree=%p zonemask=%"PRIx64, tree, zonemask);
	M0_PRE(tree->bb_root != NULL && tree->bb_ops != NULL);
	M0_PRE(key->b_nob == be_btree_ksize(tree, key->b_addr));

	btree_save(tree, tx, op, key, NULL, anchor, overwrite ?
		   BTREE_SAVE_OVERWRITE : BTREE_SAVE_INSERT, zonemask);
}

M0_INTERNAL void m0_be_btree_lookup_inplace(struct m0_be_btree        *tree,
					    struct m0_be_op           *op,
					    const struct m0_buf       *key,
					    struct m0_be_btree_anchor *anchor)
{
	struct be_btree_key_val  *kv;

	M0_ENTRY("tree=%p", tree);
	M0_PRE(tree->bb_root != NULL && tree->bb_ops != NULL);

	btree_op_fill(op, tree, NULL, M0_BBO_INSERT, anchor);

	m0_be_op_active(op);
	m0_rwlock_read_lock(btree_rwlock(tree));

	anchor->ba_tree = tree;
	anchor->ba_write = false;
	kv = be_btree_search(tree, key->b_addr);
	if (kv == NULL)
		op_tree(op)->t_rc = -ENOENT;
	else
		m0_buf_init(&anchor->ba_value, kv->btree_val,
			    be_btree_vsize(tree, kv->btree_val));

	m0_be_op_done(op);
	M0_LEAVE();
}

M0_INTERNAL void m0_be_btree_release(struct m0_be_tx           *tx,
				     struct m0_be_btree_anchor *anchor)
{
	struct m0_be_btree *tree = anchor->ba_tree;

	M0_ENTRY("tree=%p", tree);
	M0_PRE(ergo(anchor->ba_write, tx != NULL));

	if (tree != NULL) {
		if (anchor->ba_write) {
			if (anchor->ba_value.b_addr != NULL) {
				mem_update(tree, tx, anchor->ba_value.b_addr,
					   anchor->ba_value.b_nob);
				anchor->ba_value.b_addr = NULL;
			}
			m0_rwlock_write_unlock(btree_rwlock(tree));
		} else
			m0_rwlock_read_unlock(btree_rwlock(tree));
		anchor->ba_tree = NULL;
	}
	M0_LEAVE();
}

/* ------------------------------------------------------------------
 * Btree cursor interfaces implementation
 * ------------------------------------------------------------------ */

static void print_single_node(struct m0_be_bnode *node)
{
	int i;

	M0_LOG(M0_DEBUG, "{");
	for (i = 0; i < node->bt_num_active_key; ++i) {
		void *key = node->bt_kv_arr[i].btree_key;
		void *val = node->bt_kv_arr[i].btree_val;

		if (node->bt_isleaf)
			M0_LOG(M0_DEBUG, "%02d: key=%s val=%s", i,
			       (char *)key, (char *)val);
		else
			M0_LOG(M0_DEBUG, "%02d: key=%s val=%s child=%p", i,
			       (char *)key, (char *)val, node->bt_child_arr[i]);
	}
	if (!node->bt_isleaf)
		M0_LOG(M0_DEBUG, "%02d: child=%p", i, node->bt_child_arr[i]);
	M0_LOG(M0_DEBUG, "} (%p, %d)", node, node->bt_level);
}

static int iter_prepare(struct m0_be_bnode *node, bool print)
{

	int		 i = 0;
	int		 count = 0;
	unsigned int	 current_level;

	struct m0_be_bnode *head;
	struct m0_be_bnode *tail;
	struct m0_be_bnode *child = NULL;

	if (print)
		M0_LOG(M0_DEBUG, "---8<---8<---8<---8<---8<---8<---");

	if (node == NULL)
		goto out;

	count = 1;
	current_level = node->bt_level;
	head = node;
	tail = node;

	head->bt_next = NULL;
	m0_format_footer_update(head);
	while (head != NULL) {
		if (head->bt_level < current_level) {
			current_level = head->bt_level;
			if (print)
				M0_LOG(M0_DEBUG, "***");
		}
		if (print)
			print_single_node(head);

		if (!head->bt_isleaf) {
			for (i = 0; i < head->bt_num_active_key + 1; i++) {
				child = head->bt_child_arr[i];
				tail->bt_next = child;
				m0_format_footer_update(tail);
				tail = child;
				child->bt_next = NULL;
				m0_format_footer_update(child);
			}
		}
		head = head->bt_next;
		count++;
	}
out:
	if (print)
		M0_LOG(M0_DEBUG, "---8<---8<---8<---8<---8<---8<---");

	return count;
}

M0_INTERNAL void m0_be_btree_cursor_init(struct m0_be_btree_cursor *cur,
					 struct m0_be_btree *btree)
{
	cur->bc_tree = btree;
	cur->bc_node = NULL;
	cur->bc_pos = 0;
	cur->bc_stack_pos = 0;
}

M0_INTERNAL void m0_be_btree_cursor_fini(struct m0_be_btree_cursor *cursor)
{
	cursor->bc_tree = NULL;
}

M0_INTERNAL void m0_be_btree_cursor_get(struct m0_be_btree_cursor *cur,
					const struct m0_buf *key, bool slant)
{
	struct btree_node_pos     last;
	struct be_btree_key_val   *kv;
	struct m0_be_op    *op   = &cur->bc_op;
	struct m0_be_btree *tree = cur->bc_tree;

	btree_op_fill(op, tree, NULL, M0_BBO_CURSOR_GET, NULL);

	m0_be_op_active(op);
	m0_rwlock_read_lock(btree_rwlock(tree));

	last = be_btree_get_btree_node(cur, key->b_addr, slant);

	if (last.bnp_node == NULL) {
		M0_SET0(&op_tree(op)->t_out_val);
		M0_SET0(&op_tree(op)->t_out_key);
		op_tree(op)->t_rc = -ENOENT;
	} else {
		cur->bc_pos  = last.bnp_index;
		cur->bc_node = last.bnp_node;

		kv = &cur->bc_node->bt_kv_arr[cur->bc_pos];

		m0_buf_init(&op_tree(op)->t_out_val, kv->btree_val,
			    be_btree_vsize(tree, kv->btree_val));
		m0_buf_init(&op_tree(op)->t_out_key, kv->btree_key,
			    be_btree_ksize(tree, kv->btree_key));
		op_tree(op)->t_rc = 0;
	}

	m0_rwlock_read_unlock(btree_rwlock(tree));
	m0_be_op_done(op);
}

M0_INTERNAL int m0_be_btree_cursor_get_sync(struct m0_be_btree_cursor *cur,
					    const struct m0_buf *key,
					    bool slant)
{
	M0_SET0(&cur->bc_op);
	return M0_RC(M0_BE_OP_SYNC_RET_WITH(&cur->bc_op,
			      m0_be_btree_cursor_get(cur, key, slant),
			      bo_u.u_btree.t_rc));
}

static int btree_cursor_seek(struct m0_be_btree_cursor *cur, void *key)
{
	const struct m0_buf kbuf =
		M0_BUF_INIT(be_btree_ksize(cur->bc_tree, key), key);
	return m0_be_btree_cursor_get_sync(cur, &kbuf, true);
}

M0_INTERNAL int m0_be_btree_cursor_first_sync(struct m0_be_btree_cursor *cur)
{
	return btree_cursor_seek(cur, be_btree_get_min_key(cur->bc_tree));
}

M0_INTERNAL int m0_be_btree_cursor_last_sync(struct m0_be_btree_cursor *cur)
{
	return btree_cursor_seek(cur, be_btree_get_max_key(cur->bc_tree));
}

M0_INTERNAL void m0_be_btree_cursor_next(struct m0_be_btree_cursor *cur)
{
	struct be_btree_key_val   *kv;
	struct m0_be_op    *op   = &cur->bc_op;
	struct m0_be_btree *tree = cur->bc_tree;
	struct m0_be_bnode *node;

	btree_op_fill(op, tree, NULL, M0_BBO_CURSOR_NEXT, NULL);

	m0_be_op_active(op);
	m0_rwlock_read_lock(btree_rwlock(tree));

	node = cur->bc_node;
	if (node == NULL) {
		op_tree(op)->t_rc = -EINVAL;
		goto out;
	}

	/* cursor move */
	++cur->bc_pos;
	if (node->bt_isleaf) {
		while (node && cur->bc_pos >= node->bt_num_active_key)
			node = node_pop(cur, &cur->bc_pos);
	} else {
		for (;;) {
			node_push(cur, node, cur->bc_pos);
			node = node->bt_child_arr[cur->bc_pos];
			cur->bc_pos = 0;
			if (node->bt_isleaf)
				break;
		}
	}

	if (node == NULL) {
		M0_SET0(&op_tree(op)->t_out_val);
		M0_SET0(&op_tree(op)->t_out_key);
		op_tree(op)->t_rc = -ENOENT;
		goto out;
	}
	/* cursor end move */

	cur->bc_node = node;

	kv = &node->bt_kv_arr[cur->bc_pos];
	m0_buf_init(&op_tree(op)->t_out_val, kv->btree_val,
		    be_btree_vsize(tree, kv->btree_val));
	m0_buf_init(&op_tree(op)->t_out_key, kv->btree_key,
		    be_btree_ksize(tree, kv->btree_key));
out:
	m0_rwlock_read_unlock(btree_rwlock(tree));
	m0_be_op_done(op);
}

M0_INTERNAL void m0_be_btree_cursor_prev(struct m0_be_btree_cursor *cur)
{
	struct be_btree_key_val   *kv;
	struct m0_be_op    *op   = &cur->bc_op;
	struct m0_be_btree *tree = cur->bc_tree;
	struct m0_be_bnode *node;

	btree_op_fill(op, tree, NULL, M0_BBO_CURSOR_PREV, NULL);

	m0_be_op_active(op);
	m0_rwlock_read_lock(btree_rwlock(tree));

	node = cur->bc_node;

	/* cursor move */
	if (node->bt_isleaf) {
		--cur->bc_pos;
		while (node && cur->bc_pos < 0) {
			node = node_pop(cur, &cur->bc_pos);
			--cur->bc_pos;
		}
	} else {
		for (;;) {
			node_push(cur, node, cur->bc_pos);
			node = node->bt_child_arr[cur->bc_pos];
			if (node->bt_isleaf) {
				cur->bc_pos = node->bt_num_active_key - 1;
				break;
			} else
				cur->bc_pos = node->bt_num_active_key;
		}
	}

	if (node == NULL) {
		M0_SET0(&op_tree(op)->t_out_val);
		M0_SET0(&op_tree(op)->t_out_key);
		op_tree(op)->t_rc = -ENOENT;
		goto out;
	}
	/* cursor end move */

	cur->bc_node = node;

	kv = &cur->bc_node->bt_kv_arr[cur->bc_pos];
	m0_buf_init(&op_tree(op)->t_out_val, kv->btree_val,
		    be_btree_vsize(tree, kv->btree_val));
	m0_buf_init(&op_tree(op)->t_out_key, kv->btree_key,
		    be_btree_ksize(tree, kv->btree_key));
out:
	m0_rwlock_read_unlock(btree_rwlock(tree));
	m0_be_op_done(op);
}

M0_INTERNAL int m0_be_btree_cursor_next_sync(struct m0_be_btree_cursor *cur)
{
	M0_SET0(&cur->bc_op);
	return M0_BE_OP_SYNC_RET_WITH(&cur->bc_op,
				      m0_be_btree_cursor_next(cur),
				      bo_u.u_btree.t_rc);
}

M0_INTERNAL int m0_be_btree_cursor_prev_sync(struct m0_be_btree_cursor *cur)
{
	M0_SET0(&cur->bc_op);
	return M0_BE_OP_SYNC_RET_WITH(&cur->bc_op,
				      m0_be_btree_cursor_prev(cur),
				      bo_u.u_btree.t_rc);
}

M0_INTERNAL void m0_be_btree_cursor_put(struct m0_be_btree_cursor *cursor)
{
	cursor->bc_node = NULL;
}

M0_INTERNAL void m0_be_btree_cursor_kv_get(struct m0_be_btree_cursor *cur,
					   struct m0_buf *key,
					   struct m0_buf *val)
{
	struct m0_be_op *op = &cur->bc_op;
	M0_PRE(m0_be_op_is_done(op));
	M0_PRE(key != NULL || val != NULL);

	if (key != NULL)
		*key = op_tree(op)->t_out_key;
	if (val != NULL)
		*val = op_tree(op)->t_out_val;
}

M0_INTERNAL bool m0_be_btree_is_empty(struct m0_be_btree *tree)
{
	M0_PRE(tree->bb_root != NULL);
	return tree->bb_root->bt_num_active_key == 0;
}

M0_INTERNAL void btree_dbg_print(struct m0_be_btree *tree)
{
	iter_prepare(tree->bb_root, true);
}

static struct m0_be_op__btree *op_tree(struct m0_be_op *op)
{
	M0_PRE(op->bo_utype == M0_BOP_TREE);
	return &op->bo_u.u_btree;
}

static struct m0_rwlock *btree_rwlock(struct m0_be_btree *tree)
{
	return &tree->bb_lock.bl_u.rwlock;
}

/** @} end of be group */
#undef M0_TRACE_SUBSYSTEM

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
