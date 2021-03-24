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
 * @addtogroup btree
 *
 * Overview
 * --------
 *
 * Glossary
 * --------
 *
 * Btree documentation and implementation use the following terms.
 *
 * - segment, segment device, log device: segment is an area of motr process
 *   address space in which meta-data are memory mapped. motr meta-data beck-end
 *   (BE) retrieves meta-data from and stores them to a segment device. To
 *   achieve transactional meta-data updates, meta-data are also logged to a log
 *   device.
 *
 * - btree is a persistent container of key-value records, implemented by this
 *   module. Key-value records and additional internal btree data are stored in
 *   a segment. When a btree is actively used, some additional state is kept in
 *   memory outside of the segment. A btree is an instance of btree type, which
 *   specifies certain operational parameters.
 *
 *   btree persistent state is stored as a collection of btree nodes. The nodes
 *   are allocated within a segment. A btree node is a contiguous region of a
 *   segment allocated to hold tree state. The nodes of a tree can have
 *   different size subject to tree type constraints. There are 2 types of
 *   nodes:
 *
 *       -# internal nodes contain delimiting keys and pointers to child nodes;
 *
 *       -# leaf nodes contain key-value records.
 *
 *   A tree always has at least a root node. The root node can be either leaf
 *   (if the tree is small) or internal. Root node is allocated when the tree is
 *   created. All other nodes are allocated and freed dynamically.
 *
 * - tree structure. An internal node has a set of children. A descendant of a
 *   node is either its child or a descendant of a child. The parent of a node
 *   is the (only) node (necessarily internal) of which the node is a child. An
 *   ancestor of a node is either its parent of the parent of an ancestor. The
 *   sub-tree rooted at a node is the node together with all its descendants.
 *
 *   A node has a level, assigned when the node is allocated. Leaves are on the
 *   level 0 and the level of an internal node is one larger than the
 *   (identical) level of its children. In other words, the tree is balanced:
 *   the path from the root to any leaf has the same length;
 *
 * - delimiting key is a key separating ("delimiting") two children of an
 *   internal node. E.g., in the diagram below, key[0] of the root node is the
 *   delimiting key for child[0] and child[1]. btree algorithms guarantee that
 *   any key in the sub-tree rooted an a child is less than the delimiting key
 *   between this child and the next one, and not less than the delimiting key
 *   between this child and the previous one;
 *
 * - node split ...
 *
 * - adding new root ...
 *
 * - tree traversal is a process of descending through the tree from the root to
 *   the target leaf. Tree traversal takes a key as an input and returns the
 *   leaf node that contains the given key (or should contain it, if the key is
 *   missing from the tree). Such a leaf is unique by btree construction. All
 *   tree operations (lookup, insertion, deletion) start with tree traversal.
 *
 *   Traversal starts with the root. By doing binary search over delimiting keys
 *   in the root node, the target child, at which the sub-tree with the target
 *   key is rooted, is found. The child is loaded in memory, if necessary, and
 *   the process continues until a leaf is reached.
 *
 * - smop. State machine operation (smop, m0_sm_op) is a type of state machine
 *   (m0_sm) tailored for asynchronous non-blocking execution. See sm/op.h for
 *   details.
 *
 * Functional specification
 * ------------------------
 *
 * Logical specification
 * ---------------------
 *
 * Lookup
 * ......
 *
 * Tree lookup (GET) operation traverses a tree to find a given key. If the key
 * is found, the key and its value are the result of the operation. If the key
 * is not present in the tree, the operation (depending on flags) either fails,
 * or returns the next key (the smallest key in the tree greater than the
 * missing key) and its value.
 *
 * Lookup takes a "cookie" as an additional optional parameter. A cookie
 * (returned by a previous tree operation) is a safe pointer to the leaf node. A
 * cookie can be tested to check whether it still points to a valid cached leaf
 * node containing the target key. In the check is successful, the lookup
 * completes.
 *
 * Otherwise, lookup performs a tree traversal.
 *
 * @verbatim
 *
 *                         INIT------->COOKIE
 *                           |           | |
 *                           +----+ +----+ |
 *                                | |      |
 *                                v v      |
 *                      +--------SETUP<------------+
 *                      |          |       |       |
 *                      |          v       |       |
 *                      +-------LOCKALL<-----------+
 *                      |          |       |       |
 *                      |          v       |       |
 *                      +--------DOWN<-------------+
 *                      |          |       |       |
 *                      |          v       v       |
 *                      |  +-->NEXTDOWN-->LOCK-->CHECK
 *                      |  |     |  |              |
 *                      |  +-----+  |              v
 *                      |           |             ACT
 *                      |           |              |
 *                      |           |              v
 *                      +-----------+---------->CLEANUP-->DONE
 *
 *
 * @endverbatim
 *
 * (https://asciiflow.com/#/share/eJyrVspLzE1VslJydw1RKC5JLElVyE1MzsjMS1XSUcpJrEwtAspVxyhVxChZWVga68QoVQJZRuYGQFZJakUJkBOjpKCg4OnnGfJoyh4saNouZ39%2Fb0%2FXmJg8BRAAiikgAIgHxEiSaAiHEEwDsiFwDqrktD1gjCSJ3aFgFOwaEhrwaHoLHiXICGIYqkuwsDCUTcOjDCvy8Xf2dvTxIdJldHMWELn4h%2FsRH2BUcdw0LMqQE5yfa0QI2FkwAVDoIZKjh6uzN5I2hNWoSROX%2BcgpEYuWaRgux1Dk6BxCUA22IMBjGxUQMGR8XB39gMkfxnfx93ONUapVqgUAYgr3kQ%3D%3D)
 *
 * @verbatim
 *
 *                                                   OPERATION
 *                           +----------------------------tree
 *                           |                            level
 *                           |                            +---+
 *                           |     +----------------------+[0]|
 *                           v     v                      +---+
 *                           +-----+---------+   +--------+[1]|
 *                           |HEADR|ROOT NODE|   |        +---+
 *                           +-----++-+--+---+   |  +-----+[2]|
 *                                  | |  |       |  |     +---+
 *                         <--------+ |  +->     |  |  +--+[3]|
 *                                    v          |  |  |  +---+
 *                                 +--------+    |  |  |  |[4]| == NULL
 *                                 |INTERNAL|<---+  |  |  +---+
 *                                 +-+--+--++       |  |  |...|
 *                                   |  |  |        |  |  +---+
 *                          +--------+  |  +->      |  |  |[N]| == NULL
 *                          |           |           |  |  +---+
 *                          v           v           |  |
 *                         +--------+               |  |
 *                         |INTERNAL|<--------------+  |
 *                         +-+-+--+-+                  |
 *                           | |  |                    |
 *                      <----+ |  +----+               |
 *                             |       |               |
 *                             v       v               |
 *                                     +---------+     |
 *                                     |LEAF     |<----+
 *                                     +---------+
 *
 * @endverbatim
 *
 * (https://asciiflow.com/#/share/eJytU70OwiAQfhVyc9NoNf49hLoLAwOJgxpTiWljTBwcHRya6sP4NH0SESsebakMkgt80Lvv7uPoATZ8LWACMuZ7Ee%2F4CgJY8VTE6uxAIaEwGY3GAYVUoWgwVEiKRKoNBdIyZnNKN2otsscfTcZCGF4D%2FpJmy%2BVy0WElaf54zxURCl2K7OS2K3EVo%2Fm7rE6YaXT6zNjUyZ1gsaTclaqJtTYljF4JW1TrwDAMrWBD944lODGGyCtHXl%2BsoSkXldVjSI%2Fjwmyt9hW4Gn47N%2BmrBdtakBpdXJ95Odecch8nVytQ5eTTFN9g87UaUC2%2ByKoP2tMwl76jKcN%2FX9P45sp%2Fu%2Fg108daCCkc4fgEE6VxEw%3D%3D)
 *
 * Insertion (PUT)
 * ...............
 *
 * @verbatim
 *
 *                       INIT------->COOKIE
 *                         |           | |
 *                         +----+ +----+ |
 *                              | |      |
 *                              v v      |
 *                            SETUP<--------------+
 *                              |        |        |
 *                              v        |        |
 *                           LOCKALL<-----------+ |
 *                              |        |      | |
 *                              v        |      | |
 *                            DOWN<-----------+ | |
 *                      +----+  |        |    | | |
 *                      |    |  v        v    | | |
 *                      +-->NEXTDOWN-->LOCK-->CHECK
 *                           ^   |              |
 *                           |   v              v
 *                           +-ALLOC    +---MAKESPACE<-+
 *                                      |       |      |
 *                                      v       v      |
 *                                     ACT-->NEXTUP----+
 *                                              |
 *                                              v
 *                                           CLEANUP-->DONE
 *
 * @endverbatim
 *
 * (https://asciiflow.com/#/share/eJyVUrFqwzAQ%2FRVxc4aQpSGbUAQ1diVDHNpBiymGFhoPjYeEECihY4cORu13dAz%2BGn9JZdeWJVsOqTnDnd6T7unpDpDGmwQWEK4jtM3iLEGb%2BPHpOU1gAi%2FxPnlV2EHATsBiPp9NBOxVNruZqixLdpkqBCCPeVGZvzlCngnnvkeFSBFCZX5C3VdVZV60UG7v%2FVRLH%2FbSd0c3Dji1hQXJov4H0IpG67D8eldr10evp04LI%2B01v8gJOPFxEFwpw3Hr%2FukOlkNDn7Xk9%2BwfGpq9DtChabBHY6Yy6eY2Ic%2BMPkS1ynaaKue60bqlxG9vU8of22%2FtlmbUmNFeizAYKtTLcNKVf3GHfboKMaHaMMPs4WuPjOXwdZxDqj9MIssLNbqjk%2BkQcwmVoygJKGZVp8bmJWdUwBGOv7OavB4%3D)
 *
 * MAKESPACE provides sufficient free space in the current node. It handles
 * multple cases:
 *
 *     - on the leaf level, provide space for the new record being inserted;
 *
 *     - on an internal level, provide space for the new child pointer;
 *
 *     - insert new root.
 *
 * For an insert operation, the cookie is usable only if it is not stale (the
 * node is still here) and there is enough free space in the leaf node to
 * complete insertion without going up through the tree.
 *
 * Deletion (DEL)
 * ..............
 *
 * Iteration (NEXT)
 * ................
 *
 * Data structures
 * ---------------
 *
 * Concurrency
 * -----------
 *
 * Persistent state
 * ----------------
 *
 * @verbatim
 *
/*
 *              +----------+----------+--------+----------+-----+----------+
 *              | root hdr | child[0] | key[0] | child[1] | ... | child[N] |
 *              +----------+----+-----+--------+----+-----+-----+----+-----+
 *                              |                   |                |
 *                              |                   |                |
 *                              |                   |                |
 *                              |                   |                |
 *  <---------------------------+                   |                +-------->
 *                                                  |
 *                                                  |
 * +------------------------------------------------+
 * |
 * |
 * v
 * +----------+----------+--------+----------+-----+----------+
 * | node hdr | child[0] | key[0] | child[1] | ... | child[N] |
 * +----------+----+-----+--------+----+-----+-----+----+-----+
 *                 |                   |                |
 *                 |                   |                |
 *   <-------------+                   |                +---------->
 *                                     |
 *                                     |
 *                                     |
 * +-----------------------------------+
 * |
 * |
 * v
 * +----------+----------+--------+----------+-----+----------+
 * | node hdr | child[0] | key[0] | child[1] | ... | child[N] |
 * +----------+----+-----+--------+----+-----+-----+----+-----+
 *                 |                   |                |
 *                 |                   .                |
 *   <-------------+                   .                +---------->
 *                                     .
 *
 *
 * +-------------------- ...
 * |
 * v
 * +----------+--------+--------+--------+--------+-----+--------+--------+
 * | leaf hdr | key[0] | val[0] | key[1] | val[1] | ... | key[N] | val[N] |
 * +----------+--------+--------+--------+--------+-----+--------+--------+
 *
 * @endverbatim
 *
 * (https://asciiflow.com/#/share/eJyrVspLzE1VslLKL0stSszJUUjJTEwvSsxV0lHKSaxMLQLKVMcoVcQoWVmYWerEKFUCWUbmhkBWSWpFCZATo6SADB5N2TPkUExMHrofFIry80sUMlKKwJzkjMyclGiDWDAnO7USxoSIG0I4enp6SIJ%2BYEEsJg85hO4HdADyM1GiI8isR9N20SIqiHYDUWjaLkLexhEU5Gsb8MSMK4WjkNMGr1OJ8YhCXn5KKlXKrgH3EHlhQEQewS5KJe2PprcQ716ijSaAiM7N2D1JDZX0j%2BlHWLJtz6MpDXjRGgoUkKGXoJYJYGc3IWfbJuRs24TItk3I2bYJmm2bkLNtE9iwKYTs3ELIjVtw6yUmcki1bgbWfNeEPalhUUi0dj1cmsGZFn%2BgIVxLnMGEYoHoLKsHVAdBFGYZUIqBpDZSMgy9i3DqlQ5NCjmpiWnwTIVU%2FZUl5iBXioYIUbQqESTrh5BFqhsJZrKBDwRywk2pVqkWAP5HeOE%3D))
 *
 * Liveness and ownership
 * ----------------------
 *
 * Use cases
 * ---------
 *
 * Tests
 * -----
 *
 * State machines
 * --------------
 *
 * Sub-modules
 * -----------
 *
 * Node
 * ....
 *
 * Node sub-module provides interfaces that the rest of btree implementation
 * uses to access nodes. This interface includes operations to:
 *
 *     - load an existing node to memory. This is an asynchronous operation
 *       implemented as a smop. It uses BE pager interface to initiate read
 *       operation;
 *
 *     - pin a node in memory, release pinned node;
 *
 *     - access node header;
 *
 *     - access keys, values and child pointers in the node;
 *
 *     - access auxiliary information, for example, flags, check-sums;
 *
 *     - allocate a new node or free an existing one. These smops use BE
 *       allocator interface.
 *
 * Node code uses BE pager and allocator interfaces and BE transaction interface
 * (m0_be_tx_capture()).
 *
 * Node itself exists in the segment (and the corresponding segment device). In
 * addition, for each actively used node, an additional data-structure, called
 * "node descriptor" (nd) is allocated in memory outside of the segment. The
 * descriptor is used to track the state of its node.
 *
 * Node format is constrained by conflicting requirements:
 *
 *     - space efficiency: as many keys, values and pointers should fit in a
 *       node as possible, to improve cache utilisation and reduce the number of
 *       read operations necessary for tree traversal. This is especially
 *       important for the leaf nodes, because they consitute the majority of
 *       tree nodes;
 *
 *     - processor efficiency: key lookup be as cheap as possible (in terms of
 *       processor cycles). This is especially important for the internal nodes,
 *       because each tree traversal inspects multiple internal nodes before it
 *       gets to the leaf;
 *
 *     - fault-tolerance. It is highly desirable to be able to recover as much
 *       as possible of btree contents in case of media corruption. Because
 *       btree can be much larger than the primary memory, this means that each
 *       btree node should be self-contained so that it can be recovered
 *       individually.
 *
 * To satisfy all these constraints, the format of leaves is different from the
 * format of internal nodes.
 *
 * @verbatim
 *
 *  node index
 * +-----------+                                     segment
 * |           |                                    +-----------------------+
 * | . . . . . |   +-------------+                  |                       |
 * +-----------+   |             v                  |                       |
 * | &root     +---+           +----+               |   +------+            |
 * +-----------+      +--------| nd |---------------+-->| root |            |
 * | . . . . . |      v        +----+               |   +----+-+            |
 * |           |   +----+                           |        |              |
 * |           |   | td |                           |        |              |
 * |           |   +----+                           |        v              |
 * |           |      ^        +----+               |        +------+       |
 * |           |      +--------| nd |---------------+------->| node |       |
 * | . . . . . |               +---++               |        +------+       |
 * +-----------+                ^  |                |                       |
 * | &node     +----------------+  +-----+          |                       |
 * +-----------+                         v          |                       |
 * | . . . . . |                   +--------+       |                       |
 * |           |                   | nodeop |       |                       |
 * |           |                   +-----+--+       |                       |
 * |           |                         |          |                       |
 * +-----------+                         v          |                       |
 *                                 +--------+       |                       |
 *                                 | nodeop |       +-----------------------+
 *                                 +--------+
 *
 * @endverbatim
 *
 * (https://asciiflow.com/#/share/eJzNVM1OwzAMfpXIB04TEhxg2kPwBLlMJEKTWDK1OXSaJqGdd%2BBQTTwHR8TT9Elow0Z%2FsBundGitD2li%2B%2Fv82c0GzHypYQYPVmmhdPqYLFbOJilM4Hm%2B1kl5tJGQSZhN76YTCetydXt%2FU66czlz5IUGYKnZhlM6kNEX%2ByTHBeVL9tNTGfWdt7DPjmVQ4dqRw7d%2BaQlQOlCBNPUrLbqbiMAxOXCXWOky9Xl1RII4OsXUGNRdG%2FaHvh48qhZeAIvprBtpqn0MbRjTR1xZLZAB6IYRTgT9tcOph7LszTUN47%2FfGHqcnhG8roh%2FyjJOJDqq%2FeDFyyIw26Y6uBsdEF6ZqELbPuaV85WHNaS4BigwSQ2puFB8H1tvRsA4RQCFap7mzKzF64v%2BpADm7qHaTWX689kX%2BQtjraCC7us27OnQsY1HI6TrfJGxh%2BwUTzJjQ))
 *
 * Interfaces
 * ----------
 *
 * Failures
 * --------
 *
 * Compatibility
 * -------------
 *
 * @{
 */

#define M0_TRACE_SUBSYSTEM M0_TRACE_SUBSYS_BTREE
#include "lib/trace.h"
#include "lib/rwlock.h"
#include "btree/btree.h"

struct m0_btree {
	const struct m0_btree_type *t_type;
	unsigned                    t_height;
	struct tree                *t_addr;
	struct m0_rwlock            t_lock;
};

struct level {
	struct nd *l_node;
	uint64_t   l_seq;
	unsigned   l_idx;
	struct nd *l_alloc;
	struct nd *l_prev;
	struct nd *l_next;
};

struct m0_btree_oimpl {
	struct node_op  i_nop;
	struct lock_op  i_lop;
	unsigned        i_used;
	struct level    i_level[0];
};

enum base_phase {
	P_INIT = M0_SOS_INIT,
	P_DONE = M0_SOS_DONE,
	P_DOWN = M0_SOS_NR,
	P_SETUP,
	P_LOCK,
	P_CHECK,
	P_ACT,
	P_CLEANUP,
	P_COOKIE,
	P_NR
};

enum op_flags {
	OF_PREV    = M0_BITS(0),
	OF_NEXT    = M0_BITS(1),
	OF_LOCKALL = M0_BITS(2),
	OF_COOKIE  = M0_BITS(3)
};

static int fail(struct m0_btree_op *bop, int rc)
{
	bop->bo_op.o_sm.sm_rc = rc;
	return m0_sm_op_sub(&bop->bo_op, P_CLEANUP, P_DONE);
}

static int get_tick(struct m0_btree_op *bop)
{
	struct td             *tree  = (void *)bop->bo_arbor;
	uint64_t               flags = bop->bo_flags;
	struct m0_btree_oimpl *oi    = bop->bo_i;
	struct level          *level = &oi->i_level[oi->i_used];

	switch (bop->bo_op.o_sm.s_state) {
	case P_INIT:
		if ((flags & OF_COOKIE) && cookie_is_set(&bop->bo_key.k_cookie))
			return P_COOKIE;
		else
			return P_SETUP;
	case P_COOKIE:
		if (cookie_is_valid(tree, &bop->bo_key.k_cookie))
			return P_LOCK;
		else
			return P_SETUP;
	case P_SETUP:
		alloc(bop->bo_i, tree->t_height);
		if (bop->bo_i == NULL)
			return fail(bop, M0_ERR(-ENOMEM));
		return P_LOCKALL;
	case P_LOCKALL:
		if (bop->bo_flags & OF_LOCKALL)
			return m0_sm_op_sub(&bop->bo_op, P_LOCK, P_DOWN);
	case P_DOWN:
		oi->i_used = 0;
		/* Load root node. */
		return node_get(&oi->i_nop, tree, &tree->t_root, P_NEXTDOWN);
	case P_NEXTDOWN:
		if (oi->i_nop.no_op.o_sm.sm_rc == 0) {
			struct slot    slot = {};
			struct segaddr down;

			level->l_node = slot.s_node = oi->i_nop.no_node;
			node_op_fini(&oi->i_nop);
			node_find(&slot, bop->bo_rec.r_key);
			if (node_level(slot.s_node) > 0) {
				level->l_idx = slot.s_idx;
				node_child(&slot, &down);
				oi->i_used++;
				return node_get(&oi->i_nop, tree,
						&down, P_NEXTDOWN);
			} else
				return P_LOCK;
		} else {
			node_op_fini(&oi->i_nop);
			return fail(bop, oi->i_nop.no_op.o_sm.sm_rc);
		}
	case P_LOCK:
		if (!locked)
			return lock_op_init(&bop->bo_op, &bop->bo_i->i_lop,
					    P_CHECK);
		else
			return P_CHECK;
	case P_CHECK:
		if (used_cookie || check_path())
			return P_ACT;
		if (too_many_restarts) {
			if (bop->bo_flags & OF_LOCKALL)
				return fail(bop, -ETOOMANYREFS);
			else
				bop->bo_flags |= OF_LOCKALL;
		}
		if (height_increased) {
			return m0_sm_op_sub(&bop->bo_op, P_CLEANUP, P_INIT);
		} else {
			oi->i_used = 0;
			return P_DOWN;
		}
	case P_ACT: {
		struct slot slot = {
			.s_node = level->l_node;
			.s_idx  = level->l_idx;
		};
		node_rec(&slot);
		bop->bo_cb->c_act(&bop->bo_cb, &slot.s_rec);
		lock_op_unlock(&bop->bo_i->i_lop);
		return m0_sm_op_sub(&bop->bo_op, P_CLEANUP, P_DONE);
	}
	case P_CLEANUP: {
		int i;

		for (i = 0; i < oi->i_used; ++i) {
			if (oi->i_level[i].l_node != NULL) {
				node_put(oi->i_level[i].l_node);
				oi->i_level[i].l_node = NULL;
			}
		}
		free(bop->bo_i);
		return m0_sm_op_ret(&bop->bo_op);
	}
	default:
		M0_IMPOSSIBLE("Wrong state: %i", bop->bo_op.o_sm.s_state);
	};
}

/**
 * "Address" of a node in a segment.
 *
 * Highest 8 bits (56--63) are reserved and must be 0.
 *
 * Lowest 4 bits (0--3) contains the node size, see below.
 *
 * Next 5 bits (4--8) are reserved and must be 0.
 *
 * Remaining 47 (9--55) bits contain the address in the segment, measured in 512
 * byte units.
 *
 * @verbatim
 *
 *  6      5 5                                            0 0   0 0  0
 *  3      6 5                                            9 8   4 3  0
 * +--------+----------------------------------------------+-----+----+
 * |   0    |                     ADDR                     |  0  | X  |
 * +--------+----------------------------------------------+-----+----+
 *
 * @endverbatim
 *
 * Node size is 2^(9+X) bytes (i.e., the smallest node is 512 bytes and the
 * largest node is 2^(9+15) == 16MB).
 *
 * Node address is ADDR << 9.
 *
 * This allows for 128T nodes (2^47) and total of 64PB (2^56) of meta-data per
 * segment.
 */
struct segaddr {
	uint64_t as_core;
};

/**
 * B-tree node in a segment.
 *
 * This definition is private to the node sub-module.
 */
struct segnode;

/**
 * Tree descriptor.
 *
 * A tree descriptor is allocated for each b-tree actively used by the b-tree
 * module.
 */
struct td {
	const struct m0_btree_type *t_type;
	/**
	 * The lock that protects the fields below. The fields above are
	 * read-only after the tree root is loaded into memory.
	 */
	struct m0_rwlock            t_lock;
	struct segaddr              t_root;
	int                         t_height;
	int                         r_ref;
};

struct node_type {
};

/**
 * Node descriptor.
 *
 * This structure is allocated (outside of the segment) for each node actively
 * used by the b-tree module. Node descriptors are cached.
 */
struct nd {
	struct segaddr         *n_addr;
	struct td              *n_tree;
	const struct node_type *n_type;
	/**
	 * The lock that protects the fields below. The fields above are
	 * read-only after the node is loaded into memory.
	 */
	struct m0_rwlock        n_lock;
	int                     n_ref;
	uint64_t                n_seq;
	struct node_op         *n_op;
};

enum node_opcode {
	NOP_LOAD = 1,
	NOP_ALLOC,
	NOP_FREE
};

/**
 * Node operation state-machine.
 *
 * This represents a state-machine used to execute a potentially blocking tree
 * or node operation.
 */
struct node_op {
	/** Operation to do. */
	enum node_opcode no_opc;
	struct m0_sm_op  no_op;
	/** Which tree to operate on. */
	struct td       *no_tree;
	/** Address of the node withing the segment. */
	struct segaddr   no_addr;
	/** The node to operate on. */
	struct nd       *no_node;
	/** Optional transaction. */
	struct m0_be_tx *no_tx;
	/** Next operation acting on the same node. */
	struct node_op  *no_next;
};

/**
 * Key-value record within a node.
 *
 * When the node is a leaf, ->s_rec means key and value. When the node is
 * internal, ->s_rec means the key and the corresponding child pointer
 * (potentially with some node-format specific data such as child checksum).
 *
 * Slot is used as a parameter of many node_*() functions. In some functions,
 * all fields must be set by the caller. In others, only ->s_node and ->s_idx
 * are set by the caller, and the function sets ->s_rec.
 */
struct slot {
	struct nd          *s_node;
	int                 s_idx;
	struct m0_btree_rec s_rec;
};

/** Special values that can be passed to node_move() as 'nr' parameter. */
enum {
	/**
	 * Move records so that both nodes has approximately the same amount of
	 * free space.
	 */
	NR_EVEN = -1,
	/**
	 * Move as many records as possible without overflowing the target node.
	 */
	NR_MAX  = -2
};

/** Direction of move in node_move(). */
enum dir {
	/** Move (from right to) left. */
	D_LEFT,
	/** Move (from left to) right. */
	D_RIGHT
};

int64_t tree_get   (struct node_op *op, struct segaddr *addr, int nxt);
int64_t tree_create(struct node_op *op, int rootsize, struct tree_type *tt,
		    struct m0_be_tx *tx, int nxt);
int64_t tree_delete(struct node_op *op, struct td *tree, struct m0_be_tx *tx);
void    tree_put   (struct td *tree);

int64_t    node_get  (struct node_op *op, struct td *tree,
		      struct segaddr *addr, int nxt);
void       node_put  (struct nd *node);
struct nd *node_try  (struct td *tree, struct segaddr *addr, void *addr);
int64_t    node_alloc(struct node_op *op, struct td *tree, int size,
		      struct node_type *nt, struct m0_be_tx *tx, int nxt);
int64_t    node_free (struct node_op *op, struct nd *node, struct m0_be_tx *tx,
		      int nxt);
void node_op_fini(struct node_op *op);

int  node_count(const struct nd *node);
int  node_space(const struct nd *node);
int  node_level(const struct nd *node);
int  node_size (const struct nd *node);
void node_rec  (struct slot *slot);
void node_key  (struct slot *slot);
void node_child(struct slot *slot, struct segaddr *addr);
bool node_isfit(struct slot *slot);
bool node_set  (struct slot *slot, struct m0_be_tx *tx);
void node_make (struct slot *slot, struct m0_be_tx *tx);
void node_find (struct slot *slot, struct m0_btree_key *key);
void node_fix  (struct slot *slot, struct m0_be_tx *tx);
int  node_cut  (const struct nd *node, int idx, int size, struct m0_be_tx *tx);
int  node_del  (const struct nd *node, int idx, struct m0_be_tx *tx);
void node_move (const struct nd *prev, const struct nd *next, enum dir dir,
		int nr, struct m0_be_tx *tx);

#undef M0_TRACE_SUBSYSTEM

/*
 * Test plan:
 *
 * - test how cookies affect performance (for large trees and small trees);
 */
/** @} end of btree group */

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

/*  LocalWords:  btree allocator smop smops
 */
