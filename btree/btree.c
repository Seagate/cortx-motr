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
 *   btree persistent state is stored as a tree header and a collection of btree
 *   nodes. The header and nodes are allocated within a segment.
 *
 * - btree header is a persistent data structure, describing the tree. The
 *   address of a tree is the address of its header. The header is read-only
 *   after the tree is created.
 *
 * - btree node is a contiguous region of a segment allocated to hold tree
 *   state. The nodes of a tree can have different size subject to tree type
 *   constraints. There are 2 types of nodes:
 *
 *       -# internal nodes contain delimiting keys and pointers to child nodes;
 *
 *       -# leaf nodes contain key-value records.
 *
 *   A tree always has at least a root node. The root node can be either leaf
 *   (if the tree is small) or internal. The root node is located right after
 *   the tree header. All other nodes are allocated and freed dynamically.
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
 * (https://asciiflow.com/#/share/eJyrVspLzE1VslJydw1RKC5JLElVyE1MzsjMS1XSUcpJrEwtAspVxyhVxChZWVga68QoVQJZRuYGQFZJakUJkBOjpKCg4OnnGfJoyh4saNouZ39%2Fb09X7LKoKCYmTwEEgEwFBEDlQQSQVKIhbEK4dMONx1CEXeW0PWCMSyUenwW7hoQGPJreQkwoQB0DNxNPSCjgUYvhUHS1WJGPv7O3o48PkU4lw51YQg%2FmXip5AYhc%2FMP9iA9tegU4cnKdtsvPNSIE7E6YACjo4RxnD1cgD%2BYFJP0Ik3GkbDSLUDyJqQUz1DEUOTqHEFSDLUDIsS3UDxQK1LaOCggYIz6ujn7ATAzju%2Fj7ucYo1SrVAgDtHU%2Bxhttps://asciiflow.com/#/share/eJyrVspLzE1VslJydw1RKC5JLElVyE1MzsjMS1XSUcpJrEwtAspVxyhVxChZWVga68QoVQJZRuYGQFZJakUJkBOjpKCg4OnnGfJoyh4saNouZ39%2Fb0%2FXmJg8BRAAiikgAIgHxEiSaAiHEEwDsiFwDqrktD1gjCSJ3aFgFOwaEhrwaHoLHiXICGIYqkuwsDCUTcOjDCvy8Xf2dvTxIdJldHMWELn4h%2FsRH2BUcdw0LMqQE5yfa0QI2FkwAVDoIZKjh6uzN5I2hNWoSROX%2BcgpEYuWaRgux1Dk6BxCUA22IMBjGxUQMGR8XB39gMkfxnfx93ONUapVqgUAYgr3kQ%3D%3D)
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
 * (https://asciiflow.com/#/share/eJyVUrEKwjAQ%2FZVws5OL4hZiQGlNC0Z0yFIkoKAdtIMigoifIPE%2FHMWv6ZeY1ja2TSt63HCXe7n3crkDhMFaQg%2F8CUfbKIgkWgfzxTKU0IJVsJcbXTsI2AnodbvtloC9jtqdJIrkLtKJAJTakA25ECHKLL4%2Bi5n6ZGPKJ358u2jE717qfEZVs9i%2BIVyPONh1f5RQ7W2x12As%2Fiqm703ZH%2FzZ3Zqipce6kR9aqpp6qwejM54qvJ4SV49kYiYhA0qc%2FCWxuuf9y73PBpHWrJEUNuLNq3%2FEI5%2F07SPs0LGPCTXDKoy5%2BJDK5zQAjQgT1QIx4aVZ6HVt3MYaMd%2BqqrFKXIpZwpSNue8xKuAIxxc9d2%2Bmhttps://asciiflow.com/#/share/eJyVUrFqwzAQ%2FRVxc4aQpSGbUAQ1diVDHNpBiymGFhoPjYeEECihY4cORu13dAz%2BGn9JZdeWJVsOqTnDnd6T7unpDpDGmwQWEK4jtM3iLEGb%2BPHpOU1gAi%2FxPnlV2EHATsBiPp9NBOxVNruZqixLdpkqBCCPeVGZvzlCngnnvkeFSBFCZX5C3VdVZV60UG7v%2FVRLH%2FbSd0c3Dji1hQXJov4H0IpG67D8eldr10evp04LI%2B01v8gJOPFxEFwpw3Hr%2FukOlkNDn7Xk9%2BwfGpq9DtChabBHY6Yy6eY2Ic%2BMPkS1ynaaKue60bqlxG9vU8of22%2FtlmbUmNFeizAYKtTLcNKVf3GHfboKMaHaMMPs4WuPjOXwdZxDqj9MIssLNbqjk%2BkQcwmVoygJKGZVp8bmJWdUwBGOv7OavB4%3D)
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
 * +------------+----------+----------+--------+----------+-----+----------+
 * | tree headr | root hdr | child[0] | key[0] | child[1] | ... | child[N] |
 * +------------+----------+----+-----+--------+----+-----+-----+----+-----+
 *                              |                   |                |
 *                              |                   |                |
 *                              |                   |                |
 *                              |                   |                |
 *  <---------------------------+                   |                +------>
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
 * +----------+-----------------+--------+--------+-----+--------+--------+
 * | leaf hdr | key[0]]Treel[0] | key[1] | val[1] | ... | key[N] | val[N] |
 * +----------+-----------------+--------+--------+-----+--------+--------+
 *
 * @endverbatim
 *
 * (Source: https://asciiflow.com/#/share/eJyrVspLzE1VssorzcnRUcpJrEwtUrJSqo5RqohRsrIws9SJUaoEsozMDYGsktSKEiAnRunRlD3DFcXE5AFJhZKi1FSFlNTi5CIFELcoP79EISMFwknOyMxJiTaIBXOyUythTIi4IYSjp6eHJOgHFoSYPVwR0HcK%2BAAoNIgSHUFmPZq2ixZRQbQbiELTdhHyNo6gIF%2FbgCdmXCkchZw2eJ1KjEcU8vJTUkdsqYYlcT6irCQgWfuj6S3Eu5doowkgonMzdk9SQyX9Y%2FoRlmzb82hKA160hgIFZOglqGUC2NlNyNm2CTnbNiGybRNytm2CZtsm5GzbBDZsCiE7txBy4xbceomJHFKtm4E13zVhT2pYFBKtXQ%2BXZnCmxR9oCNcSZzChWCA6y%2BoB1UEQhVkGlGIgqY2UDEPvIpx6pUOTQk5qYho8U0HyUSwoIMoSc5ArRWg9CBJFqxJBsn4IWaS6kWAmG%2FhAICfclGqVagHq968y)
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
 * Node code uses BE pager and allocator interfaces. It does not use BE
 * transaction interface. Calls to m0_be_tx_capture() are done by the upper
 * layers of btree implementation.
 *
 * Node itself exists in the segment (and the corresponding segment device). In
 * addition, for each actively used node, an additional data-structure, called
 * "node descriptor" (nd) is allocated in memory outside of the segment. The
 * descriptor is used to track the state of its node.
 *
 * Node format is constrained by conflicting requirements:
 *
 *     - space efficiency: as many keys, values and pointers should fit in a
 *       node as possible, to improve cache utilisation. This is especially
 *       important for the leaf nodes, because they consitute the majority of
 *       tree nodes.
 *
 *     - processor efficiency: key lookup be as cheap as possible (in terms of
 *       processor cycles). This is especially important for the internal nodes,
 *       because each tree traversal inspects multiple internal nodes before it
 *       gets to the leaf.
 *
 * To satisfy both constraints, the format of leaves is different from the
 * format of internal nodes.
 *
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
	struct node *l_node;
	uint64_t     l_seq;
	unsigned     l_pos;
	struct node *l_alloc;
};

struct m0_btree_oimpl {
	struct pg_op    i_pop;
	struct alloc_op i_aop;
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
	P_TRYCOOKIE,
	P_NR
};

static int get_tick(struct m0_btree_op *bop) {
	switch (bop->bo_op.o_sm.s_state) {
	case P_INIT:
		if (!m0_bcookie_is_null(&bop->bo_key.k_cookie))
			return cookie_op(&bop->bo_op,
					 &bop->bo_i->i_pop, P_TRYCOOKIE);
		else
			return P_SETUP;
	case P_SETUP:
		alloc(bop->bo_i);
		if (ENOMEM)
			return sub(P_CLEANUP, P_DONE);
	case P_DOWN:
		if (bop->bo_i->i_used < bop->bo_arbor->t_height) {
			return pg_op_init(&bop->bo_op,
					  &bop->bo_i->i_pop, P_DOWN);
		} else
			return P_LOCK;
	case P_LOCK:
		return lock_op_init(&bop->bo_op, &bop->bo_i->i_lop, P_CHECK);
	case P_CHECK:
		if (used_cookie || check_path())
			return P_ACT;
		else if (height_increased) {
			return sub(P_CLEANUP, P_INIT);
		} else {
			bop->bo_i->i_used = 0;
			return P_DOWN;
		}
	case P_ACT:
		bop->bo_cb->c_act(bop->bo_cb, ...);
		lock_op_unlock(&bop->bo_i->i_lop);
		return sub(P_CLEANUP, P_DONE);
	case P_CLEANUP:
		free(bop->bo_i);
		return ret();
	case P_TRYCOOKIE:
		if (cookie_is_valid)
			return P_LOCK;
		else
			return P_SETUP;
	default:
		M0_IMPOSSIBLE("Wrong state: %i", bop->bo_op.o_sm.s_state);
	};
}

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
