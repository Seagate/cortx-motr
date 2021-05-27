/* -*- C -*- */
/*
 * Copyright (c) 2012-2020 Seagate Technology LLC and/or its Affiliates
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


#define M0_TRACE_SUBSYSTEM M0_TRACE_SUBSYS_LIB
#include "lib/trace.h"       /* M0_ERR() */

#include "lib/finject.h"     /* m0_fi_enable() */
#include "lib/memory.h"      /* m0_alloc() */
#include "lib/misc.h"        /* M0_SET0() */
#include "fd/fd_internal.h"
#include "fd/fd.h"
#include "fd/ut/common.h"
#include "pool/pool.h"       /* m0_pool_version */
#include "ut/ut.h"           /* M0_UT_ASSERT() */
#include "lib/errno.h"       /* ENOMEM */

static int tree_populate(struct m0_fd_tree *tree, enum tree_type param_type);
static uint32_t geometric_sum(uint16_t r, uint16_t k);
static uint32_t int_pow(uint32_t num, uint32_t exp);

static void test_cache_init_fini(void)
{
	struct m0_fd_tree            *tree;
	struct m0_layout              l;
	struct m0_pdclust_instance    pi;
	struct m0_pool_version        pver;
	struct m0_fd_perm_cache      *cache;
	struct m0_fd_perm_cache_grid *cache_grid;
	struct m0_fd__tree_cursor     cursor;
	struct m0_fd_tree_node       *node;
	uint32_t                      children_nr;
	uint32_t                      i;
	m0_time_t                     seed;
	int                           rc;

	M0_SET0(&l);
	M0_SET0(&pi);
	M0_SET0(&pver);

	l.l_pver = &pver;
	tree = &pver.pv_fd_tree;
	seed = m0_time_now();
	children_nr = m0_rnd(TP_QUATERNARY, &seed);
	children_nr = TP_QUATERNARY - children_nr;
	rc = fd_ut_tree_init(tree, M0_CONF_PVER_HEIGHT - 1);
	M0_UT_ASSERT(rc == 0);
	rc = m0_fd__tree_root_create(tree, children_nr);
	M0_UT_ASSERT(rc == 0);
	for (i = 1; i < M0_CONF_PVER_HEIGHT; ++i) {
		children_nr = m0_rnd(TP_QUATERNARY, &seed);
		children_nr = TP_QUATERNARY - children_nr;
		children_nr = i == tree->ft_depth ? 0 : children_nr;
		rc = fd_ut_tree_level_populate(tree, children_nr, i, TA_SYMM);
		M0_UT_ASSERT(rc == 0);
	}
	rc = m0_fd_cache_grid_build(&l, &pi);
	M0_UT_ASSERT(rc == 0);
	cache_grid = pi.pi_perm_cache;
	/*
	 * Iterate over the tree and check if cache for each node is in place.
	 */
	for (i = 0; i < tree->ft_depth; ++i) {
		rc = m0_fd__tree_cursor_init(&cursor, tree, i);
		M0_UT_ASSERT(rc == 0);
		M0_UT_ASSERT(cache_grid->fcg_cache[i] != NULL);
		do {
			node = *(m0_fd__tree_cursor_get(&cursor));
			cache =
			  &cache_grid->fcg_cache[i][node->ftn_abs_idx];
			M0_UT_ASSERT(cache->fpc_len == node->ftn_child_nr);
		} while (m0_fd__tree_cursor_next(&cursor));
	}

	m0_fd_cache_grid_destroy(&l, &pi);
	m0_fd_tree_destroy(tree);
}

static void test_init_fini(void)
{
	struct m0_fd_tree tree;
	uint16_t          i;
	uint16_t          j;
	int               rc;

	for (j = TP_BINARY; j < TP_QUATERNARY + 1; ++j) {
		for (i = 1; i < M0_CONF_PVER_HEIGHT; ++i) {
			rc = fd_ut_tree_init(&tree, i);
			M0_UT_ASSERT(rc == 0);
			rc = m0_fd__tree_root_create(&tree, j);
			M0_UT_ASSERT(rc == 0);
			rc = tree_populate(&tree, j);
			M0_UT_ASSERT(rc == 0);
			M0_UT_ASSERT(geometric_sum(j, i) == tree.ft_cnt);
			m0_fd_tree_destroy(&tree);
			M0_UT_ASSERT(tree.ft_cnt == 0);
		}
	}
}

static void test_fault_inj(void)
{
	struct m0_fd_tree tree;
	m0_time_t         seed;
	uint64_t          n;
	int               rc;

	M0_SET0(&tree);
	rc = fd_ut_tree_init(&tree, M0_CONF_PVER_HEIGHT - 1);
	M0_UT_ASSERT(rc == 0);
	rc = m0_fd__tree_root_create(&tree, TP_BINARY);
	M0_UT_ASSERT(rc == 0);
	rc = tree_populate(&tree, TP_BINARY);
	M0_UT_ASSERT(rc == 0);
	m0_fd_tree_destroy(&tree);

	/* Fault injection. */
	seed = m0_time_now();
	/* Maximum nodes in a tree. */
	n    = m0_rnd(geometric_sum(TP_BINARY, M0_CONF_PVER_HEIGHT - 1),
		      &seed);
	m0_fi_enable_off_n_on_m("m0_alloc", "fail_allocation", n, 1);
	M0_SET0(&tree);
	rc = fd_ut_tree_init(&tree, M0_CONF_PVER_HEIGHT - 1) ?:
	     m0_fd__tree_root_create(&tree, TP_BINARY) ?:
	     tree_populate(&tree, TP_BINARY);
	m0_fi_disable("m0_alloc","fail_allocation");
	M0_UT_ASSERT(rc == -ENOMEM);
	/*
	 * NOTE: m0_fd__tree_root_create and tree_populate call
	 * m0_alloc two times per node, but m0_alloc may be recursively:
	 * m0_alloc call alloc_tail, but alloc_tail may call m0_alloc
	 */
	M0_UT_ASSERT((n >> 1) >= tree.ft_cnt);
	m0_fd_tree_destroy(&tree);
	M0_UT_ASSERT(tree.ft_cnt == 0);
}

static uint32_t geometric_sum(uint16_t r, uint16_t k)
{
	return (int_pow(r, k + 1) - 1) / (r - 1);
}

static uint32_t int_pow(uint32_t num, uint32_t exp)
{
	uint32_t ret = 1;
	uint32_t i;

	for (i = 0; i < exp; ++i) {
		ret *= num;
	}
	return ret;
}

static int tree_populate(struct m0_fd_tree *tree, enum tree_type param_type)
{
	uint16_t i;
	int      rc = 0;
	uint64_t children_nr;

	for (i = 1; i <= tree->ft_depth; ++i) {
		children_nr = i == tree->ft_depth ? 0 : param_type;
		rc = fd_ut_tree_level_populate(tree, children_nr, i, TA_SYMM);
		if (rc != 0)
			return rc;
	}
	return rc;
}

static void test_perm_cache(void)
{
	test_cache_init_fini();
}
static void test_fd_tree(void)
{
	test_init_fini();
	test_fault_inj();
}

struct m0_ut_suite failure_domains_tree_ut = {
	.ts_name = "failure_domains_tree-ut",
	.ts_init = NULL,
	.ts_fini = NULL,
	.ts_tests = {
		{"test_fd_tree", test_fd_tree},
		{"test_perm_cache", test_perm_cache},
		{ NULL, NULL }
	}
};
