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

#include "btree/btree.c"     /* include the whole btree implementation file. */

#define M0_TRACE_SUBSYSTEM M0_TRACE_SUBSYS_BTREE
#include "be/ut/helper.h"
#include "ut/ut.h"          /** struct m0_ut_suite */

static struct m0_be_ut_backend *ut_be;
static struct m0_be_ut_seg     *ut_seg;
static struct m0_be_seg        *seg;

#define m0_be_tx_init(tx,tid,dom,sm_group,persistent,discarded,filler,datum) \
	do {                                                                 \
	                                                                     \
	} while (0)

#define m0_be_tx_prep(tx,credit)                                             \
	do {                                                                 \
                                                                             \
	} while (0)

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

	find_key.k_data.ov_vec.v_nr            = 1;
	find_key.k_data.ov_vec.v_count         = &ksize;
	find_key.k_data.ov_buf                 = &p_key;

	slot.s_rec.r_key.k_data.ov_vec.v_nr    = 1;
	slot.s_rec.r_key.k_data.ov_vec.v_count = &ksize;
	slot.s_rec.r_key.k_data.ov_buf         = &p_key;

	slot.s_rec.r_val.ov_vec.v_nr           = 1;
	slot.s_rec.r_val.ov_vec.v_count        = &vsize;
	slot.s_rec.r_val.ov_buf                = &p_val;
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

static void get_rec_at_index(struct nd *node, int idx, uint64_t *key,
			     uint64_t *val)
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
 * This unit test will create a tree, add a node and then populate the node with
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

/**
 * This unit test exercises a few tree operations in both valid and invalid
 * conditions.
 */
static void btree_ut_basic_tree_operations(void)
{
	void                   *invalid_addr = (void *)0xbadbadbadbad;
	struct m0_btree        *btree;
	struct m0_btree_type    btree_type = {.tt_id = M0_BT_EMAP_EM_MAPPING };
	struct m0_be_tx        *tx = NULL;
	struct m0_btree_op      b_op;
	void                   *temp_node;
	return;

	/** Prepare transaction to capture tree operations. */
	m0_be_tx_init(tx, 0, NULL, NULL, NULL, NULL, NULL, NULL);
	m0_be_tx_prep(tx, NULL);

	/**
	 *  Run a valid scenario:
	 *  1) Create a btree
	 *  2) Close the btree
	 *  3) Open the btree
	 *  4) Close the btree
	 *  5) Destroy the btree
	 */

	/** Create temp node space*/
	temp_node = m0_alloc_aligned((1024 + sizeof(struct nd)), 10);
	M0_BTREE_OP_SYNC_WITH(&b_op.bo_op,
			      m0_btree_create(temp_node, 1024, &btree_type,
					      tx, &b_op));

	m0_btree_close(b_op.bo_arbor);

	m0_btree_open(temp_node, 1024, &btree);

	m0_btree_close(btree);
	M0_BTREE_OP_SYNC_WITH(&b_op.bo_op, m0_btree_destroy(btree, &b_op));

	m0_free_aligned(temp_node, (1024 + sizeof(struct nd)), 10);

	/** Now run some invalid cases */

	/** Open a non-existent btree */
	m0_btree_open(invalid_addr, 1024, &btree);

	/** Close a non-existent btree */
	m0_btree_close(btree);

	/** Destroy a non-existent btree */
	M0_BTREE_OP_SYNC_WITH(&b_op.bo_op, m0_btree_destroy(btree, &b_op));

	/** Create a new btree */
	temp_node = m0_alloc_aligned((1024 + sizeof(struct nd)), 10);
	M0_BTREE_OP_SYNC_WITH(&b_op.bo_op,
			      m0_btree_create(temp_node, 1024, &btree_type,
					      tx, &b_op));

	/** Close it */
	m0_btree_close(b_op.bo_arbor);

	/** Try closing again */
	m0_btree_close(b_op.bo_arbor);

	/** Re-open it */
	m0_btree_open(invalid_addr, 1024, &btree);

	/** Open it again */
	m0_btree_open(invalid_addr, 1024, &btree);

	/** Destory it */
	M0_BTREE_OP_SYNC_WITH(&b_op.bo_op, m0_btree_destroy(btree, &b_op));

	/** Attempt to reopen the destroyed tree */
	m0_btree_open(invalid_addr, 1024, &btree);

}


struct cb_data {
	struct m0_btree_key *key;
	struct m0_bufvec    *value;
};

static int btree_kv_put_cb(struct m0_btree_cb *cb, struct m0_btree_rec *rec)
{
        struct m0_bufvec_cursor  scur;
	struct m0_bufvec_cursor  dcur;
	m0_bcount_t              ksize;
	m0_bcount_t              vsize;
	struct cb_data          *datum = cb->c_datum;

	ksize = m0_vec_count(&datum->key->k_data.ov_vec);
	M0_PRE(m0_vec_count(&rec->r_key.k_data.ov_vec) >= ksize);

	vsize = m0_vec_count(&datum->value->ov_vec);
	M0_PRE(m0_vec_count(&rec->r_val.ov_vec) >= vsize);

	m0_bufvec_cursor_init(&scur, &datum->key->k_data);
	m0_bufvec_cursor_init(&dcur, &rec->r_key.k_data);
	m0_bufvec_cursor_copy(&dcur, &scur, ksize);

	m0_bufvec_cursor_init(&scur, datum->value);
	m0_bufvec_cursor_init(&dcur, &rec->r_val);
	m0_bufvec_cursor_copy(&dcur, &scur, vsize);

	return 0;
}


static int btree_kv_get_cb(struct m0_btree_cb *cb, struct m0_btree_rec *rec)
{
	struct m0_bufvec_cursor scur;
	struct m0_bufvec_cursor dcur;
	m0_bcount_t             ksize;
	m0_bcount_t             vsize;
	struct cb_data          *datum = cb->c_datum;

	ksize = m0_vec_count(&datum->key->k_data.ov_vec);
	M0_PRE(m0_vec_count(&rec->r_key.k_data.ov_vec) <= ksize);

	vsize = m0_vec_count(&datum->value->ov_vec);
	M0_PRE(m0_vec_count(&rec->r_val.ov_vec) <= vsize);

	m0_bufvec_cursor_init(&dcur, &datum->key->k_data);
	m0_bufvec_cursor_init(&scur, &rec->r_key.k_data);
	m0_bufvec_cursor_copy(&dcur, &scur, ksize);

	m0_bufvec_cursor_init(&dcur, datum->value);
	m0_bufvec_cursor_init(&scur, &rec->r_val);
	m0_bufvec_cursor_copy(&dcur, &scur, vsize);

	return 0;
}


/**
 * This unit test exercises the KV operations for both valid and invalid
 * conditions.
 */
static void btree_ut_basic_kv_operations(void)
{
	struct m0_btree_type  btree_type   = {.tt_id = M0_BT_EMAP_EM_MAPPING};
	struct m0_be_tx      *tx           = NULL;
	struct m0_btree_op    b_op;
	void                 *temp_node;
	int                   i;
	time_t                curr_time;
	struct m0_btree_cb    ut_cb;
	uint64_t              first_key;
	bool                  first_key_initialized = false;
	struct m0_btree_op    kv_op;
	return;

	M0_ENTRY();

	time(&curr_time);
	M0_LOG(M0_DEBUG, "Using seed %lu", curr_time);
	srandom(curr_time);

	/** Prepare transaction to capture tree operations. */
	m0_be_tx_init(tx, 0, NULL, NULL, NULL, NULL, NULL, NULL);
	m0_be_tx_prep(tx, NULL);

	/**
	 *  Run valid scenario:
	 *  1) Create a btree
	 *  2) Adds a few records to the created tree.
	 *  3) Confirms the records are present in the tree.
	 *  4) Deletes all the records from the tree.
	 *  4) Close the btree
	 *  5) Destroy the btree
	 */

	/** Create temp node space*/
	temp_node = m0_alloc_aligned((1024 + sizeof(struct nd)), 10);
	M0_BTREE_OP_SYNC_WITH(&b_op.bo_op,
			      m0_btree_create(temp_node, 1024, &btree_type,
					      tx, &b_op));

	for (i = 0; i < 2048; i++) {
		uint64_t             key;
		uint64_t             value;
		struct cb_data       put_data;
		struct m0_btree_key  put_key;
		struct m0_bufvec     put_value;
		m0_bcount_t          ksize  = sizeof key;
		m0_bcount_t          vsize  = sizeof value;
		void                *k_ptr  = &key;
		void                *v_ptr  = &value;

		/**
		 *  There is a very low possibility of hitting the same key
		 *  again. This is fine as it helps debug the code when insert
		 *  is called with the same key instead of update function.
		 */
		key = value = random();

		if (!first_key_initialized) {
			first_key = key;
			first_key_initialized = true;
		}

		put_key.k_data     = M0_BUFVEC_INIT_BUF(&k_ptr, &ksize);
		put_value          = M0_BUFVEC_INIT_BUF(&v_ptr, &vsize);

		put_data.key       = &put_key;
		put_data.value     = &put_value;

		ut_cb.c_act        = btree_kv_put_cb;
		ut_cb.c_datum      = &put_data;

		M0_BTREE_OP_SYNC_WITH(&kv_op.bo_op,
				      m0_btree_put(b_op.bo_arbor, tx,
						   &put_key, &ut_cb, 0,
						   &kv_op));
	}

	{
		uint64_t             key;
		uint64_t             value;
		struct cb_data       get_data;
		struct m0_btree_key  get_key;
		struct m0_bufvec     get_value;
		m0_bcount_t          ksize     = sizeof key;
		m0_bcount_t          vsize     = sizeof value;
		void                *k_ptr    = &key;
		void                *v_ptr    = &value;
		uint64_t             search_key;
		void                *search_key_ptr = &search_key;
		m0_bcount_t          search_key_size = sizeof search_key;
		struct m0_btree_key  search_key_in_tree;

		get_key.k_data = M0_BUFVEC_INIT_BUF(&k_ptr, &ksize);
		get_value      = M0_BUFVEC_INIT_BUF(&v_ptr, &vsize);

		get_data.key    = &get_key;
		get_data.value  = &get_value;

		ut_cb.c_act   = btree_kv_get_cb;
		ut_cb.c_datum = &get_data;

		search_key = first_key;

		search_key_in_tree.k_data =
			M0_BUFVEC_INIT_BUF(&search_key_ptr, &search_key_size);

		M0_BTREE_OP_SYNC_WITH(&kv_op.bo_op,
				      m0_btree_get(b_op.bo_arbor,
						   &search_key_in_tree,
						   &ut_cb, 0, &kv_op));

		for (i = 1; i < 2048; i++) {
			search_key = key;
			M0_BTREE_OP_SYNC_WITH(&kv_op.bo_op,
					      m0_btree_nxt(b_op.bo_arbor,
							   &search_key_in_tree,
							   &ut_cb, 0, &kv_op));
		}
	}

	m0_btree_close(b_op.bo_arbor);
}

static void btree_ut_kv_insert()
{
	struct m0_be_tx_credit	cred = {};
	struct m0_btree        *tree;
	struct m0_be_tx        *tx;
#if 0
	struct m0_btree        *btree;
	struct m0_btree_type    btree_type = {.tt_id = M0_BT_EMAP_EM_MAPPING };
	struct m0_btree_op      b_op;
	void                   *temp_node;
#endif
	int                     rc;

	M0_ENTRY();
	M0_ALLOC_PTR(ut_be);
	M0_UT_ASSERT(ut_be != NULL);

	M0_ALLOC_PTR(ut_seg);
	M0_UT_ASSERT(ut_seg != NULL);

	/* Init BE */
	m0_be_ut_backend_init(ut_be);
	m0_be_ut_seg_init(ut_seg, ut_be, 1ULL << 24);
	seg = ut_seg->bus_seg;

	M0_BE_ALLOC_CREDIT_PTR(tree, seg, &cred);

	M0_ALLOC_PTR(tx);
	M0_UT_ASSERT(tx != NULL);

	m0_be_ut_tx_init(tx, ut_be);
	m0_be_tx_prep(tx, &cred);
	rc = m0_be_tx_open_sync(tx);
	M0_UT_ASSERT(rc == 0);


#if 0
	/** Create temp node space*/
	temp_node = m0_alloc_aligned((1024 + sizeof(struct nd)), 10);
	M0_BTREE_OP_SYNC_WITH(&b_op.bo_op,
			      m0_btree_create(temp_node, 1024, &btree_type,
					      tx, &b_op));
	m0_btree_close(b_op.bo_arbor);

	m0_btree_open(temp_node, 1024, &btree);
	m0_btree_close(btree);

	m0_free_aligned(temp_node, (1024 + sizeof(struct nd)), 10);
#endif

	/* do something. */
	/* something. */
	m0_be_ut_seg_reload(ut_seg);

	/* do something. */
	/* something. */
	m0_be_ut_seg_reload(ut_seg);

	m0_be_tx_close_sync(tx); /* Make things persistent. */
	m0_be_tx_fini(tx);
	m0_free(tx);
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
		{"node_create_delete",     btree_ut_node_create_delete        },
		{"node_add_del_rec",       btree_ut_node_add_del_rec          },
		{"basic_tree_operations",  btree_ut_basic_tree_operations     },
		{"basic_kv_operations",    btree_ut_basic_kv_operations       },
		{"kv_insert",              btree_ut_kv_insert                 },
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
