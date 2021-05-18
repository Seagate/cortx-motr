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

#include "btree/btree.c"
#define M0_TRACE_SUBSYSTEM M0_TRACE_SUBSYS_BTREE
#include "be/ut/helper.h"
#include "ut/ut.h"          /** struct m0_ut_suite */

/**
 * The code contained below is 'ut'. This is a little experiment to contain the
 * ut code in the same file containing the functionality code. We are open to
 * changes iff enough reasons are found that this model either does not work or
 * is not intuitive or maintainable.
 */

static struct m0_be_ut_backend *ut_be;
static struct m0_be_ut_seg     *ut_seg;
static struct m0_be_seg        *seg;

static bool btree_ut_initialised = false;
static void btree_ut_init(void)
{

	if (!btree_ut_initialised) {
		segops = (struct seg_ops *)&mem_seg_ops;
		m0_rwlock_init(&trees_lock);
		btree_ut_initialised = true;
	}
}

static void btree_ut_fini(void)
{
	segops = NULL;
	m0_rwlock_fini(&trees_lock);
	btree_ut_initialised = false;
}

/**
 * This test will create a few nodes and then delete them before exiting. The
 * main intent of this test is to debug the create and delete nodes functions.
 */
static void btree_ut_node_create_delete(void)
{
	struct node_op          op;
	struct m0_btree_type    tt;
	struct td              *tree;
	struct td              *tree_clone;
	struct nd              *node1;
	struct nd              *node2;
	const struct node_type *nt    = &fixed_format;

	M0_ENTRY();

	btree_ut_init();

	M0_SET0(&op);

	M0_ASSERT(trees_loaded == 0);

	// Create a Fixed-Format tree.
	op.no_opc = NOP_ALLOC;
	tree_create(&op, &tt, 10, NULL, 0);

	tree = op.no_tree;

	M0_ASSERT(tree->r_ref == 1);
	M0_ASSERT(tree->t_root != NULL);
	M0_ASSERT(trees_loaded == 1);

	// Add a few nodes to the created tree.
	op.no_opc = NOP_ALLOC;
	node_alloc(&op, tree, 10, nt, 8, 8, NULL, 0);
	node1 = op.no_node;

	op.no_opc = NOP_ALLOC;
	node_alloc(&op,  tree, 10, nt, 8, 8, NULL, 0);
	node2 = op.no_node;

	op.no_opc = NOP_FREE;
	node_free(&op, node1, NULL, 0);

	op.no_opc = NOP_FREE;
	node_free(&op, node2, NULL, 0);

	/* Get another reference to the same tree. */
	tree_get(&op, &tree->t_root->n_addr, 0);
	tree_clone = op.no_tree;
	M0_ASSERT(tree_clone->r_ref == 2);
	M0_ASSERT(tree->t_root == tree_clone->t_root);
	M0_ASSERT(trees_loaded == 1);


	tree_put(tree_clone);
	M0_ASSERT(trees_loaded == 1);

	// Done playing with the tree - delete it.
	op.no_opc = NOP_FREE;
	tree_delete(&op, tree, NULL, 0);
	M0_ASSERT(trees_loaded == 0);

	btree_ut_fini();
	M0_LEAVE();
}


static bool add_rec(struct nd *node,
		    uint64_t   key,
		    uint64_t   val)
{
	struct ff_head      *h = ff_data(node);
	struct slot          slot;
	struct m0_btree_key  find_key;
	m0_bcount_t          ksize;
	void                *p_key;
	m0_bcount_t          vsize;
	void                *p_val;

	/**
	 * To add a record if space is available in the node to hold a new
	 * record:
	 * 1) Search index in the node where the new record is to be inserted.
	 * 2) Get the location in the node where the key & value should be
	 *    inserted.
	 * 3) Insert the new record at the determined location.
	 */

	ksize = h->ff_ksize;
	p_key = &key;
	vsize = h->ff_vsize;
	p_val = &val;

	M0_SET0(&slot);
	slot.s_node                            = node;
	slot.s_rec.r_key.k_data.ov_vec.v_nr    = 1;
	slot.s_rec.r_key.k_data.ov_vec.v_count = &ksize;
	slot.s_rec.r_val.ov_vec.v_nr           = 1;
	slot.s_rec.r_val.ov_vec.v_count        = &vsize;

	if (node_count(node) != 0) {
		if (!node_isfit(&slot))
			return false;
		find_key.k_data.ov_vec.v_nr = 1;
		find_key.k_data.ov_vec.v_count = &ksize;
		find_key.k_data.ov_buf = &p_key;
		node_find(&slot, &find_key);
	}

	node_make(&slot, NULL);

	slot.s_rec.r_key.k_data.ov_buf = &p_key;
	slot.s_rec.r_val.ov_buf = &p_val;

	node_rec(&slot);

	*((uint64_t *)p_key) = key;
	*((uint64_t *)p_val) = val;

	return true;
}

static void get_next_rec_to_add(struct nd *node, uint64_t *key,  uint64_t *val)
{
	struct slot          slot;
	uint64_t             proposed_key;
	struct m0_btree_key  find_key;
	m0_bcount_t          ksize;
	void                *p_key;
	m0_bcount_t          vsize;
	void                *p_val;
	struct ff_head      *h = ff_data(node);

	M0_SET0(&slot);
	slot.s_node = node;

	ksize = h->ff_ksize;
	proposed_key = rand();

	find_key.k_data.ov_vec.v_nr    = 1;
	find_key.k_data.ov_vec.v_count = &ksize;
	find_key.k_data.ov_buf         = &p_key;

	slot.s_rec.r_key.k_data.ov_vec.v_nr = 1;
	slot.s_rec.r_key.k_data.ov_vec.v_count = &ksize;
	slot.s_rec.r_key.k_data.ov_buf = &p_key;

	slot.s_rec.r_val.ov_vec.v_nr = 1;
	slot.s_rec.r_val.ov_vec.v_count = &vsize;
	slot.s_rec.r_val.ov_buf = &p_val;
	while (true) {
		uint64_t found_key;

		proposed_key %= 256;
		p_key = &proposed_key;

		if (node_count(node) == 0)
			break;
		node_find(&slot, &find_key);
		node_rec(&slot);

		if (slot.s_idx >= node_count(node))
			break;

		found_key = *(uint64_t *)p_key;

		if (found_key == proposed_key)
			proposed_key++;
		else
			break;
	}

	*key = proposed_key;
	memset(val, *key, sizeof(*val));
}

static void get_rec_at_index(struct nd *node, int idx,
			     uint64_t *key,  uint64_t *val)
{
	struct slot          slot;
	m0_bcount_t          ksize;
	void                *p_key;
	m0_bcount_t          vsize;
	void                *p_val;

	M0_SET0(&slot);
	slot.s_node = node;
	slot.s_idx  = idx;

	M0_ASSERT(idx<node_count(node));

	slot.s_rec.r_key.k_data.ov_vec.v_nr = 1;
	slot.s_rec.r_key.k_data.ov_vec.v_count = &ksize;
	slot.s_rec.r_key.k_data.ov_buf = &p_key;

	slot.s_rec.r_val.ov_vec.v_nr = 1;
	slot.s_rec.r_val.ov_vec.v_count = &vsize;
	slot.s_rec.r_val.ov_buf = &p_val;

	node_rec(&slot);

	if (key != NULL)
		*key = *(uint64_t *)p_key;

	if (val != NULL)
		*val = *(uint64_t *)p_val;
}

/**
 * This test will create a tree, add a node and then populate the node with
 * some records. It will also confirm the records are in ascending order of Key.
 */
static void btree_ut_node_add_del_rec(void)
{
	struct node_op          op;
	struct m0_btree_type    tt;
	struct td              *tree;
	struct nd              *node1;
	const struct node_type *nt      = &fixed_format;
	uint64_t                key;
	uint64_t                val;
	uint64_t                prev_key;
	uint64_t                curr_key;
	time_t                  curr_time;
	int                     run_loop;

	M0_ENTRY();

	time(&curr_time);
	M0_LOG(M0_DEBUG, "Using seed %lu", curr_time);
	srand(curr_time);

	run_loop = 50000;

	btree_ut_init();

	M0_SET0(&op);

	op.no_opc = NOP_ALLOC;
	tree_create(&op, &tt, 10, NULL, 0);

	tree = op.no_tree;

	op.no_opc = NOP_ALLOC;
	node_alloc(&op, tree, 10, nt, 8, 8, NULL, 0);
	node1 = op.no_node;

	while (run_loop--) {
		int i;

		/** Add records */
		i = 0;
		while (true) {
			get_next_rec_to_add(node1, &key, &val);
			if (!add_rec(node1, key, val))
				break;
			M0_ASSERT(++i == node_count(node1));
		}

		/** Confirm all the records are in ascending value of key. */
		get_rec_at_index(node1, 0, &prev_key, NULL);
		for (i = 1; i < node_count(node1); i++) {
			get_rec_at_index(node1, i, &curr_key, NULL);
			M0_ASSERT(prev_key < curr_key);
			prev_key = curr_key;
		}

		/** Delete all the records from the node. */
		i = node_count(node1) - 1;
		while (node_count(node1) != 0) {
			int j = rand() % node_count(node1);
			node_del(node1, j, NULL);
			M0_ASSERT(i-- == node_count(node1));
		}
	}

	op.no_opc = NOP_FREE;
	node_free(&op, node1, NULL, 0);

	// Done playing with the tree - delete it.
	op.no_opc = NOP_FREE;
	tree_delete(&op, tree, NULL, 0);

	btree_ut_fini();

	M0_LEAVE();
}

static void btree_ut_insert_test()
{
	M0_ENTRY();
	M0_ALLOC_PTR(ut_be);
	M0_UT_ASSERT(ut_be != NULL);

	M0_ALLOC_PTR(ut_seg);
	M0_UT_ASSERT(ut_seg != NULL);

	/* Init BE */
	m0_be_ut_backend_init(ut_be);
	m0_be_ut_seg_init(ut_seg, ut_be, 1ULL << 24);
	seg = ut_seg->bus_seg;

	/* do something. */
	/* something. */
	m0_be_ut_seg_reload(ut_seg);

	/* do something. */
	/* something. */
	m0_be_ut_seg_reload(ut_seg);

	/* Fini BE */
	m0_be_ut_seg_fini(ut_seg);
	m0_be_ut_backend_fini(ut_be);
	m0_free(ut_seg);
	m0_free(ut_be);

	M0_LEAVE();
}

/**
 * btree_ut test suite.
 */
struct m0_ut_suite btree_ut = {
	.ts_name = "btree-ut",
	.ts_yaml_config_string = "{ valgrind: { timeout: 3600 },"
				 "  helgrind: { timeout: 3600 },"
				 "  exclude:  ["
				 "   "
				 "  ] }",
	.ts_init = NULL,
	.ts_fini = NULL,
	.ts_tests = {
		{"node_create_delete",          btree_ut_node_create_delete},
		{"node_add_del_rec",            btree_ut_node_add_del_rec  },
		{"kv_insert",                   btree_ut_insert_test       },
		{NULL, NULL}
	}
};

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
