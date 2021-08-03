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
 *   ancestor of a node is either its parent or the parent of an ancestor. The
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
 * node containing the target key. If the check is successful, the lookup
 * completes.
 *
 * Otherwise, lookup performs a tree traversal.
 *
 * @verbatim
 *
 *                        INIT------->COOKIE
 *                          |           | |
 *                          +----+ +----+ |
 *                               | |      |
 *                               v v      |
 *                     +--------SETUP<----+-------+
 *                     |          |       |       |
 *                     |          v       |       |
 *                     +-------LOCKALL<---+-------+
 *                     |          |       |       |
 *                     |          v       |       |
 *                     +--------DOWN<-----+-------+
 *                     |          |       |       |
 *                     |          v       v       |
 *                     |  +-->NEXTDOWN-->LOCK-->CHECK
 *                     |  |     |  |              |
 *                     |  +-----+  |              v
 *                     |           |             ACT
 *                     |           |              |
 *                     |           |              v
 *                     +-----------+---------->CLEANUP-->DONE
 *
 *
 * @endverbatim
 *
 * (https://asciiflow.com/#/share/eJyrVspLzE1VslJydw1RKC5JLElVyE1MzsjMS1XSUcpJrEwtAspVxyhVxChZWVga68QoVQJZRuYGQFZJakUJkBOjpKCg4OnnGfJoyh4saNouZ39%2Fb0%2FXmJg8BRAAiikgAIgHxEiSaAiHEEwDsiFwDqrktD1gjCSJ3aFgFOwaEhrwaHoLHiXICGIYqkuwsDCUTcOjDCvy8Xf2dvTxIdJldHMWELn4h%2FsRH2BUcdw0LMqQE5yfa0QI2FkwAVDoIZKjh6uzN5I2hNWoSROX%2BcgpEYuWaRgux1Dk6BxCUA22IMBjGxUQMGR8XB39gMkfxnfx93ONUapVqgUAYgr3kQ%3D%3D))
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
 *                      INIT------->COOKIE
 *                        |           | |
 *                        +----+ +----+ |
 *                             | |      |
 *                             v v      |
 *                           SETUP<-----+--------+
 *                             |        |        |
 *                             v        |        |
 *                          LOCKALL<----+------+ |
 *                             |        |      | |
 *                             v        |      | |
 *                           DOWN<------+----+ | |
 *                     +----+  |        |    | | |
 *                     |    |  v        v    | | |
 *                     +-->NEXTDOWN-->LOCK-->CHECK
 *                             |        ^      |
 *                             v        |      v
 *                        +--ALLOC------+ +---MAKESPACE<-+
 *                        |    ^          |       |      |
 *                        +----+          v       v      |
 *                                       ACT-->NEXTUP----+
 *                                                |
 *                                                v
 *                                             CLEANUP-->DONE
 *
 * @endverbatim
 *
 * (https://asciiflow.com/#/share/eJyVUj1rwzAQ%2FSvi5gwlS0M2oQhq7EqGOLSDFlMELSQeWg8JIVBCxw4djNrf0TH41%2FiXVA62LNnKR8UZTrqnu%2Bent4UsXUmYQrxI0Fue5hKt0qfnl0zCCJbpRr7q2lbAWsB0MhmPBGx0Nr690Vku17neCEABC5KqePeEOhDOw4AKkSGEqmKPulXvqqJsS4V790sffbpHPx3carBvN05JlcdvUJrTZBFX3x%2F67ProzTRpaaW94WcxESchjqIraXj%2But%2Fdg%2FJw6KNm%2FIH9g0Nz11P0cBrcMTWbmfJjm1AHRh%2BTI8vWTbVynbXuKAkdZazOv0atS6ooY8FmsH4aTk7KYOIeh3QeY0KNhqaPQ8GlNjSsT9AhYa%2Bb7YVJ4uimbX7SxZ51GaDOAUhEMatHNm8z44wK2MHuDxf739Y%3D))
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
 * There are basically 2 cases for deletion
 * 1. No underflow after deletion
 * 2. Underflow after deletion
 *  a. Balance by borrowing key from sibling
 *  b. Balance by merging with sibling
 *    b.i. No underflow at parent node
 *    b.ii. Underflow at parent node
 *      b.ii.A. Borrow key-pivot from sibling at parent node
 *      b.ii.B. Merge with sibling at parent node
 * @verbatim
 *
 *
 *                       INIT-------->COOKIE
 *                        |             | |
 *                        +-----+ +-----+ |
 *                              | |       |
 *                              v v       |
 *                             SETUP<-----+--------+
 *                               |        |        |
 *                               v        |        |
 *                            LOCKALL<----+------+ |
 *                               |        |      | |
 *                               v        |      | |
 *                             DOWN<------+----+ | |
 *                       +----+  |        |    | | |
 *                       |    |  v        |    | | |
 *                       +-->NEXTDOWN     |    | | |
 *                               |        |    | | |
 *                               v        v    | | |
 *                          +---LOAD--->LOCK-->CHECK     +--MOVEUP
 *                          |     ^              |       |      |
 *                          +-----+              v       v      |
 *                                              ACT--->RESOLVE--+
 *                                               |        |
 *                                               v        |
 *                                            CLEANUP<----+
 *                                               |
 *                                               v
 *                                             DONE
 * @endverbatim
 *
 * Phases Description:
 * step 1. NEXTDOWN: traverse down the tree searching for given key till we
 * 		      reach leaf node containing that key
 * step 2. LOAD : load left and/or, right only if there are chances of underflow
 * 		  at the node (i.e.number of keys == min or any other conditions
 * 		  defined for underflow can be used)
 * step 3. CHECK : check if any of the nodes referenced (or loaded) during the
 * 		   traversal have changed if the nodes have changed then repeat
 * 		   traversal again after UNLOCKING the tree if the nodes have
 * 		   not changed then check will call ACT
 * step 4. ACT: This state will find the key and delete it. If there is no
 * 		underflow, move to CLEANUP, otherwise move to RESOLVE.
 * step 5. RESOLVE: This state will resolve underflow, it will get sibling and
 * 		    perform merging or rebalancing with sibling. Once the
 * 		    underflow is resolved at the node, if there is an underflow
 * 		    at parent node Move to MOVEUP, else move to CEANUP.
 * step 6. MOVEUP: This state moves to the parent node
 *
 *
 * Iteration (PREVIOUS or NEXT)
 * ................
 * @verbatim
 *
 *			 INIT------->COOKIE
 * 			   |           | |
 * 			   +----+ +----+ |
 * 			        | |      |
 * 			        v v      |
 * 			      SETUP<-----+---------------+
 * 			        |        |               |
 * 			        v        |               |
 * 			     LOCKALL<----+-------+       |
 * 			        |        |       |       |
 * 			        v        |       |       |
 * 			      DOWN<------+-----+ |       |
 * 			+----+  |        |     | |       |
 * 			|    |  v        v     | |       |
 * 			+---NEXTDOWN-->LOCK-->CHECK-->CLEANUP
 * 			 +----+ |        ^      |      ^   |
 * 			 |    | v        |      v      |   v
 * 			 +---SIBLING-----+     ACT-----+  DONE
 *
 * @endverbatim
 *
 * Iteration operation traverses a tree to find the next or previous key
 * (depending on the the flag) to the given key. If the next or previous key is
 * found then the key and its value are returned as the result of operation.
 * Otherwise, a flag, indicating boundary keys, is returned.
 *
 * Iteration also takes a "cookie" as an additional optional parameter. A cookie
 * (returned by a previous tree operation) is a safe pointer to the leaf node. A
 * cookie can be tested to check whether it still points to a valid cached leaf
 * node containing the target key. If the check is successful and the next or
 * previous key is present in the cached leaf node then return its record
 * otherwise traverse through the tree to find next or previous tree.
 *
 * Iterator start traversing the tree till the leaf node to find the
 * next/previous key to the search key. While traversing down the the tree, it
 * marks level as pivot if the node at that level has valid sibling. At the end
 * of the tree traversal, the level which is closest to leaf level and has valid
 * sibling will be marked as pivot level.
 *
 * These are the possible scenarios after tree travesal:
 * case 1: The search key has valid sibling in the leaf node i.e. search key is
 *	   greater than first key (for previous key search operation) or search
 *	   key is less than last key (for next key search operation).
 *	   Return the next/previous key's record to the caller.
 * case 2: The pivot level is not updated with any of the non-leaf level. It
 *         means the search key is rightmost(for next operation) or leftmost(for
 *	   previous operation). Return a flag indicating the search key is
 *         boundary key.
 * case 3: The search key does not have valid sibling in the leaf node i.e.
 *	   search key is less than or equal to the first key (for previous key
 *	   search operation) or search key is greater than or equal to last key
 *	   (for next key search operation) and pivot level is updated with
 *	   non-leaf level.
 *	   In this case, start loading the sibling nodes from the node at pivot
 *	   level till the leaf level. Return the last record (for previous key
 *	   operation) or first record (for next operation) from the sibling leaf
 *	   node.
 *
 * Phases Description:
 * NEXTDOWN: This state will load nodes found during tree traversal for the
 *           search key.It will also update the pivot internal node. On reaching
 *           the leaf node if the found next/previous key is not valid and
 *           pivot level is updated, load the sibling index's child node from
 *           the internal node pivot level.
 * SIBLING: Load the sibling records (first key for next operation or last key
 *	    for prev operation) from the sibling nodes till leaf node.
 * CHECK: Check the traversal path for node with key and also the validity of
 *	  leaf sibling node (if loaded). If the traverse path of any of the node
 *	  has changed, repeat traversal again after UNLOCKING the tree else go
 *	  to ACT.
 * ACT: ACT will perform following actions:
 *	1. If it has a valid leaf node sibling index (next/prev key index)
 *	   return record.
 *	2. If the leaf node sibling index is invalid and pivot node is also
 *	   invalid, it means we are at the boundary of th btree (i.e. rightmost
 *	   or leftmost key). Return flag indicating boundary keys.
 *	3. If the leaf node sibling index is invalid and pivot level was updated
 *	   with the non-leaf level, return the record (first record for next
 *	   operation or last record for prev operation) from the sibling node.
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
 *
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
#include "lib/thread.h"     /** struct m0_thread */
#include "lib/bitmap.h"     /** struct m0_bitmap */
#include "lib/byteorder.h"  /** m0_byteorder_cpu_to_be64() */
#include "lib/atomic.h"     /** m0_atomic64_set() */
#include "btree/btree.h"
#include "fid/fid.h"
#include "format/format.h"   /** m0_format_header ff_fmt */
#include "module/instance.h"
#include "lib/memory.h"
#include "lib/misc.h"
#include "lib/assert.h"
#include "ut/ut.h"          /** struct m0_ut_suite */
#include "lib/tlist.h"     /** m0_tl */
#include "lib/time.h"      /** m0_time_t */

#include "be/ut/helper.h"  /** m0_be_ut_backend_init() */

#ifndef __KERNEL__
#include <stdlib.h>
#include <time.h>
#include <unistd.h>
#endif

#define AVOID_BE_SEGMENT    1
/**
 *  --------------------------------------------
 *  Section START - BTree Structure and Operations
 *  --------------------------------------------
 */

struct td;
struct m0_btree {
	const struct m0_btree_type *t_type;
	unsigned                    t_height;
	struct td                  *t_desc;
};

enum base_phase {
	P_INIT = M0_SOS_INIT,
	P_DONE = M0_SOS_DONE,
	P_DOWN = M0_SOS_NR,
	P_NEXTDOWN,
	P_SIBLING,
	P_ALLOC_REQUIRE,
	P_ALLOC_STORE,
	P_STORE_CHILD,
	P_SETUP,
	P_LOCKALL,
	P_LOCK,
	P_CHECK,
	P_SANITY_CHECK,
	P_MAKESPACE,
	P_ACT,
	P_CAPTURE,
	P_FREENODE,
	P_CLEANUP,
	P_FINI,
	P_COOKIE,
	P_TIMECHECK,
	P_TREE_GET,
	P_NR
};

enum btree_node_type {
	BNT_FIXED_FORMAT                         = 1,
	BNT_FIXED_KEYSIZE_VARIABLE_VALUESIZE     = 2,
	BNT_VARIABLE_KEYSIZE_FIXED_VALUESIZE     = 3,
	BNT_VARIABLE_KEYSIZE_VARIABLE_VALUESIZE  = 4,
};

enum {
	M0_TREE_COUNT = 20,
	M0_NODE_COUNT = 100,
};

enum {
	MAX_NODE_SIZE            = 10, /*node size is a power-of-2 this value.*/
	MAX_KEY_SIZE             = 8,
	MAX_VAL_SIZE             = 8,
	MAX_TRIALS               = 3,
	MAX_TREE_HEIGHT          = 5,
	BTREE_CALLBACK_CREDIT    = MAX_TREE_HEIGHT * 2 + 1,
	INTERNAL_NODE_VALUE_SIZE = sizeof(void *),
};

#define M0_BTREE_TX_CAPTURE(tx, seg, ptr, size)                              \
			   m0_be_tx_capture(tx, &M0_BE_REG(seg, size, ptr))

#define M0_BTREE_TX_CB_CAPTURE(tx, node, cb)                                 \
			      m0_be_tx_cb_capture(tx, node, cb)

#if AVOID_BE_SEGMENT

#undef M0_BTREE_TX_CAPTURE
#define M0_BTREE_TX_CAPTURE(tx, seg, ptr, size)                              \
	do {                                                                 \
		typeof(size) __size = (size);                                \
		(tx) = (tx);                                                 \
		(seg) = (seg);                                               \
		(ptr) = (ptr);                                               \
		(__size) = (__size);                                         \
	} while (0)

#undef M0_BTREE_TX_CB_CAPTURE
#define M0_BTREE_TX_CB_CAPTURE(tx, node, cb)                                 \
	do {                                                                 \
		typeof(cb) __cb = (cb);                                      \
		(tx)   = (tx);                                               \
		(node) = (node);                                             \
		(__cb) = (__cb);                                             \
	} while (0)

#undef M0_BE_ALLOC_ALIGN_BUF_SYNC
#define M0_BE_ALLOC_ALIGN_BUF_SYNC(buf, shift, seg, tx)                      \
		(buf)->b_addr = m0_alloc_aligned((buf)->b_nob, shift)

#undef M0_BE_ALLOC_CREDIT_BUF
#define M0_BE_ALLOC_CREDIT_BUF(buf, seg, cred)                               \
	do { *(buf) = *(buf); (seg) = (seg); *(cred) = *(cred); } while (0)

#define m0_be_ut_tx_init(tx, ut_be)                                          \
	do { } while (0)

#define m0_be_tx_init(tx,tid,dom,sm_group,persistent,discarded,filler,datum) \
	do { } while (0)

#define m0_be_tx_prep(tx,credit)                                             \
	do { } while (0)

#define m0_be_tx_open_sync(tx)  (0)

#define m0_be_tx_open(tx)                                                    \
	do { } while (0)

#define m0_be_tx_capture(tx,req)                                             \
	do { } while (0)

#define m0_be_tx_close_sync(tx)                                              \
	do { } while (0)

#define m0_be_tx_close(tx)                                                   \
	do { } while (0)

#define m0_be_tx_fini(tx)                                                    \
	do { } while (0)

#define m0_be_ut_seg_reload(ut_seg)                                          \
	do { } while (0)

#define m0_be_ut_backend_init(ut_be)                                         \
	do { } while (0)

#define m0_be_ut_seg_init(ut_seg, ut_be, size)                               \
	do { } while (0)

#define m0_be_ut_seg_fini(ut_seg)                                            \
	do { } while (0)

#define m0_be_ut_backend_fini(ut_be)                                         \
	do { } while (0)
#endif

#if 0
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

	switch (bop->bo_op.o_sm.sm_state) {
	case P_INIT:
		if ((flags & BOF_COOKIE) && cookie_is_set(&bop->bo_key.k_cookie))
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
		if (bop->bo_flags & BOF_LOCKALL)
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
			if (bop->bo_flags & BOF_LOCKALL)
				return fail(bop, -ETOOMANYREFS);
			else
				bop->bo_flags |= BOF_LOCKALL;
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
		M0_IMPOSSIBLE("Wrong state: %i", bop->bo_op.o_sm.sm_state);
	};
}
#endif

/**
 *  --------------------------------------------
 *  Section END - BTree Structure and Operations
 *  --------------------------------------------
 */

/**
 *  ---------------------------------------------------
 *  Section START - BTree Node Structure and Operations
 *  ---------------------------------------------------
 */


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

enum {
	NODE_SHIFT_MIN = 9,
};

#ifndef __KERNEL__
static struct segaddr  segaddr_build(const void *addr, int shift);
#endif
static void           *segaddr_addr (const struct segaddr *addr);
static int             segaddr_shift(const struct segaddr *addr);
#ifndef __KERNEL__
static uint32_t        segaddr_ntype_get(const struct segaddr *addr);
#endif
static bool            segaddr_header_isvalid(const struct segaddr *addr);

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
	struct nd                  *t_root;
	int                         t_height;
	int                         t_ref;
	struct m0_be_seg           *t_seg; /** Segment hosting tree nodes. */

	/**
	 * Start time is basically used in tree close to calculate certain time-
	 * frame for other threads to complete their operation when tree_close
	 * is called. This is used when the nd_active list has more members than
	 * expected.
	 */
	m0_time_t               t_starttime;
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
enum direction {
	/** Move (from right to) left. */
	D_LEFT = 1,
	/** Move (from left to) right. */
	D_RIGHT
};

struct nd;
struct slot;

/**
 *  Different types of btree node formats are supported. While the basic btree
 *  operations remain the same, the differences are encapsulated in the nodes
 *  contained in the btree.
 *  Each supported node type provides the same interface to implement the btree
 *  operations so that the node-specific changes are captured in the node
 *  implementation.
 */
struct node_type {
	uint32_t                    nt_id;
	const char                 *nt_name;
	const struct m0_format_tag  nt_tag;

	/** Initializes newly allocated node */
	void (*nt_init)(const struct segaddr *addr, int shift, int ksize,
			int vsize, uint32_t ntype, struct m0_be_seg *seg,
			struct m0_be_tx *tx);

	/** Cleanup of the node if any before deallocation */
	void (*nt_fini)(const struct nd *node, struct m0_be_tx *tx);

	/** Returns count of keys in the node */
	int  (*nt_count)(const struct nd *node);

	/** Returns count of records/values in the node*/
	int  (*nt_count_rec)(const struct nd *node);

	/** Returns the space (in bytes) available in the node */
	int  (*nt_space)(const struct nd *node);

	/** Returns level of this node in the btree */
	int  (*nt_level)(const struct nd *node);

	/** Returns size of the node (as a shift value) */
	int  (*nt_shift)(const struct nd *node);

	/**
	 * Returns size of the key of node. In case of variable key size return
	 * -1.
	 */
	int  (*nt_keysize)(const struct nd *node);

	/**
	 * Returns size of the value of node. In case variable value size
	 * return -1.
	 */
	int  (*nt_valsize)(const struct nd *node);

	/**
	 * If predict is set as true, function determines if there is
	 * possibility of underflow else it determines if there is an underflow
	 * at node.
	 */
	bool  (*nt_isunderflow)(const struct nd *node, bool predict);

	/** Returns true if there is possibility of overflow. */
	bool  (*nt_isoverflow)(const struct nd *node);

	/** Returns unique FID for this node */
	void (*nt_fid)  (const struct nd *node, struct m0_fid *fid);

	/** Returns record (KV pair) for specific index. */
	void (*nt_rec)  (struct slot *slot);

	/** Returns Key at a specifix index */
	void (*nt_key)  (struct slot *slot);

	/** Returns Child pointer (in segment) at specific index */
	void (*nt_child)(struct slot *slot, struct segaddr *addr);

	/**
	 *  Returns TRUE if node has space to fit a new entry whose key and
	 *  value length is provided in slot.
	 */
	bool (*nt_isfit)(struct slot *slot);

	/**
	 *  Node changes related to last record have completed; any post
	 *  processing related to the record needs to be done in this function.
	 */
	void (*nt_done)(struct slot *slot, struct m0_be_tx *tx, bool modified);

	/** Makes space in the node for inserting new entry at specific index */
	void (*nt_make) (struct slot *slot, struct m0_be_tx *tx);

	/** Returns index of the record containing the key in the node */
	bool (*nt_find) (struct slot *slot, const struct m0_btree_key *key);

	/**
	 *  All the changes to the node have completed. Any post processing can
	 *  be done here.
	 */
	void (*nt_fix)  (const struct nd *node, struct m0_be_tx *tx);

	/**
	 *  Changes the size of the value (increase or decrease) for the
	 *  specified key
	 */
	void (*nt_cut)  (const struct nd *node, int idx, int size,
			 struct m0_be_tx *tx);

	/** Deletes the record from the node at specific index */
	void (*nt_del)  (const struct nd *node, int idx, struct m0_be_tx *tx);

	/** Updates the level of node */
	void (*nt_set_level)  (const struct nd *node, uint8_t new_level,
			       struct m0_be_tx *tx);

	/** Moves record(s) between nodes */
	void (*nt_move) (struct nd *src, struct nd *tgt,
			 enum direction dir, int nr, struct m0_be_tx *tx);

	/** Validates node composition */
	bool (*nt_invariant)(const struct nd *node);

	/** Validates key order within node */
	bool (*nt_expensive_invariant)(const struct nd *node);

	/** Does a thorough validation */
	bool (*nt_verify)(const struct nd *node);

	/** Does minimal (or basic) validation */
	bool (*nt_isvalid)(const struct segaddr *addr);

	/** Saves opaque data. */
	void (*nt_opaque_set)(const struct segaddr *addr, void *opaque);

	/** Gets opaque data. */
	void* (*nt_opaque_get)(const struct segaddr *addr);

	/** Captures node data in segment */
	void (*nt_capture)(struct slot *slot, struct m0_be_tx *tx);

	/** Gets key size from segment. */
	/* uint16_t (*nt_ksize_get)(const struct segaddr *addr); */

	/** Gets value size from segment. */
	/* uint16_t (*nt_valsize_get)(const struct segaddr *addr); */
};

/**
 * Node descriptor.
 *
 * This structure is allocated (outside of the segment) for each node actively
 * used by the b-tree module. Node descriptors are cached.
 */
struct nd {
	struct segaddr          n_addr;
	struct td              *n_tree;
	const struct node_type *n_type;

	/**
	 * Linkage into node descriptor list.
	 * ndlist_tl, btree_active_nds, btree_lru_nds.
	 */
	struct m0_tlink	        n_linkage;
	uint64_t                n_magic;

	/**
	 * The lock that protects the fields below. The fields above are
	 * read-only after the node is loaded into memory.
	 */
	struct m0_rwlock        n_lock;

	/**
	 * Node refernce count. n_ref count indicates the number of times this
	 * node is fetched for different operations (KV delete, put, get etc.).
	 * If the n_ref count is non-zero the node should be in active node
	 * descriptor list. Once n_ref count reaches, it means the node is not
	 * in use by any operation and is safe to move to global lru list.
	 */
	int                     n_ref;

	/**
	 * Transaction reference count. A non-zero txref value indicates
	 * the active transactions for this node. Once the txref count goes to
	 * '0' the segment data in the mmapped memory can be released if the
	 * kernel starts to run out of physical memory in the system.
	 */
	int                     n_txref;

	uint64_t                n_seq;
	struct node_op         *n_op;

	/**
	 * flag for indicating if node on BE segment is valid or not, but it
	 * does not indicated anything about node descriptor validity.
	 */
	bool                    n_be_node_valid;
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
	const struct nd     *s_node;
	int                  s_idx;
	struct m0_btree_rec  s_rec;
};

#ifndef __KERNEL__
static int64_t tree_get   (struct node_op *op, struct segaddr *addr, int nxt);
#if 0
static int64_t tree_create(struct node_op *op, struct m0_btree_type *tt,
			   int rootshift, struct m0_be_tx *tx, int nxt);
#endif
static int64_t tree_delete(struct node_op *op, struct td *tree,
			   struct m0_be_tx *tx, int nxt);
static void    tree_put   (struct td *tree);

static int64_t    node_get  (struct node_op *op, struct td *tree,
			     struct segaddr *addr, int nxt);
static void       node_put  (struct node_op *op, struct nd *node,
			     struct m0_be_tx *tx);
#endif


#if 0
static struct nd *node_try  (struct td *tree, struct segaddr *addr);
#endif

#ifndef __KERNEL__
static int64_t    node_free(struct node_op *op, struct nd *node,
			    struct m0_be_tx *tx, int nxt);
static int64_t    node_alloc(struct node_op *op, struct td *tree, int shift,
			     const struct node_type *nt, int ksize, int vsize,
			     struct m0_be_tx *tx, int nxt);
static void node_op_fini(struct node_op *op);
#endif
#ifndef __KERNEL__
static int node_access(struct segaddr *addr, int shift, int nxt);
static int  node_init(struct segaddr *addr, int ksize, int vsize,
		      const struct node_type *nt, struct m0_be_seg *seg,
		      struct m0_be_tx *tx, int nxt);
#endif
#if 0
static bool node_verify(const struct nd *node);
#endif
static int  node_count(const struct nd *node);
static int  node_count_rec(const struct nd *node);
static int  node_space(const struct nd *node);
#ifndef __KERNEL__
static int  node_level(const struct nd *node);
static int  node_shift(const struct nd *node);
static int  node_keysize(const struct nd *node);
static int  node_valsize(const struct nd *node);
static bool  node_isunderflow(const struct nd *node, bool predict);
static bool  node_isoverflow(const struct nd *node);
#endif
#if 0
static void node_fid  (const struct nd *node, struct m0_fid *fid);
#endif

static void node_rec  (struct slot *slot);
#ifndef __KERNEL__
static void node_key  (struct slot *slot);
static void node_child(struct slot *slot, struct segaddr *addr);
#endif
static bool node_isfit(struct slot *slot);
static void node_done(struct slot *slot, struct m0_be_tx *tx, bool modified);
static void node_make(struct slot *slot, struct m0_be_tx *tx);

#ifndef __KERNEL__
static bool node_find (struct slot *slot, const struct m0_btree_key *key);
#endif
static void node_seq_cnt_update (struct nd *node);
static void node_fix  (const struct nd *node, struct m0_be_tx *tx);
#if 0
static void node_cut  (const struct nd *node, int idx, int size,
		       struct m0_be_tx *tx);
#endif
static void node_del  (const struct nd *node, int idx, struct m0_be_tx *tx);
#ifndef __KERNEL__
static void node_set_level  (const struct nd *node, uint8_t new_level,
			     struct m0_be_tx *tx);
static void node_move (struct nd *src, struct nd *tgt, enum direction dir,
		       int nr, struct m0_be_tx *tx);

static void node_capture(struct slot *slot, struct m0_be_tx *tx);

static void node_lock(struct nd *node);
static void node_unlock(struct nd *node);
static void node_fini(const struct nd *node, struct m0_be_tx *tx);
#endif
/**
 * Common node header.
 *
 * This structure is located at the beginning of every node, right after
 * m0_format_header. It is used by the segment operations (node_op) to identify
 * node and tree types.
 */
struct node_header {
	uint32_t h_node_type;
	uint32_t h_tree_type;
	uint64_t h_opaque;
};

/**
 * This structure will store information required at particular level
 */
struct level {
	/** nd for required node at currrent level. **/
	struct nd *l_node;

	/** Sequence number of the node */
	uint64_t   l_seq;

	/** nd for sibling node at current level. **/
	struct nd *l_sibling;

	/** Sequence number of the sibling node */
	uint64_t   l_sib_seq;

	/** Index for required record from the node. **/
	int        l_idx;

	/** nd for newly allocated node at the level. **/
	struct nd *l_alloc;

	/**
	 * Flag for indicating if l_alloc has been used or not. This flag is
	 * used by level_cleanup. If flag is set, node_put() will be called else
	 * node_free() will get called for l_alloc.
	 */
	bool      i_alloc_in_use;

	/**
	 * This is the flag for indicating if node needs to be freed. Currently
	 * this flag is set in delete operation and is used by P_FREENODE phase
	 * to determine if the node should be freed.
	 */
	bool       l_freenode;
};

/**
 * Node Capture Structure.
 *
 * This stucture will store the address of node descriptor and index of record
 * which will be used for capturing transaction.
 */
struct node_capture_info {
	/**
	 * Address of node descriptor which needs to be captured in
	 * transaction.
	 */
	struct nd  *nc_node;

	 /* Starting index from where record may have been added or deleted. */
	int         nc_idx;
};

/**
 * Btree implementation structure.
 *
 * This structure will get created for each operation on btree and it will be
 * used while executing the given operation.
 */
struct m0_btree_oimpl {
	struct node_op             i_nop;
	/* struct lock_op  i_lop; */

	/** Count of entries initialized in l_level array. **/
	unsigned                   i_used;

	/** Array of levels for storing data about each level. **/
	struct level              *i_level;

	/** Level from where sibling nodes needs to be loaded. **/
	int                        i_pivot;

	/** i_alloc_lev will be used for P_ALLOC_REQUIRE and P_ALLOC_STORE phase
	 * to indicate current level index. **/
	int                        i_alloc_lev;

	/** Store node_find() output. */
	bool                       i_key_found;

	/** When there will be requirement for new node in case of root
	 * splitting i_extra_node will be used. **/
	struct nd                 *i_extra_node;

	/** Track number of trials done to complete operation. **/
	unsigned                   i_trial;

	/** Used to store height of tree at the beginning of any operation **/
	unsigned                   i_height;

	/** Node descriptor for cookie if it is going to be used. **/
	struct nd                 *i_cookie_node;

	/**
	 * Array of node_capture structures which hold nodes that need to be
	 * captured in transactions. This array is populated when the nodes are
	 * modified as a part of KV operations. The nodes in this array are
	 * later captured in P_CAPTURE state of the KV_tick() function.
	 */
	struct node_capture_info   i_capture[BTREE_CALLBACK_CREDIT];

	/**
	 * Flag for indicating if root child needs to be freed. After deleting
	 * record from root node, if root node is an internal node and it
	 * contains only one child, we copy all records from that child node to
	 * root node, decrease the height of tree and set this flag. This flag
	 * will be used by P_FREENODE to determine if child node needs to be
	 * freed.
	*/
	bool                       i_root_child_free;

};


/**
 * Node descriptor LRU list.
 * Following actions will be performed on node descriptors:
 * 1. If nds are not active, they will be moved from btree_active_nds to
 * btree_lru_nds list head.
 * 2. If the nds in btree_lru_nds become active, they will be moved to
 * btree_active_nds list head.
 * 3. Based on certain conditions, the nds can be freed from btree_lru_nds
 * list tail.
 */
static struct m0_tl     btree_lru_nds;

/**
 * Active node descriptor list contains the node descriptors that are
 * currently in use by the trees.
 * Node descriptors are linked through nd:n_linkage to this list.
 */
struct m0_tl btree_active_nds;

/**
 * node descriptor list lock.
 * It protects node descriptor movement between lru node descriptor list and
 * active node descriptor list.
 */
static struct m0_rwlock list_lock;

M0_TL_DESCR_DEFINE(ndlist, "node descr list", static, struct nd,
		   n_linkage, n_magic, M0_BTREE_ND_LIST_MAGIC,
		   M0_BTREE_ND_LIST_HEAD_MAGIC);
M0_TL_DEFINE(ndlist, static, struct nd);

#ifndef __KERNEL__
static int node_access(struct segaddr *addr, int shift, int nxt)
{
	/**
	 * TODO: Implement node_access function to ensure that node data has
	 * been read from BE segment. This operation will be used during
	 * async mode of btree operations.
	 */
	return nxt;
}

static int node_init(struct segaddr *addr, int ksize, int vsize,
		     const struct node_type *nt, struct m0_be_seg *seg,
		     struct m0_be_tx *tx, int nxt)
{
	/**
	 * node_access() will ensure that we have node data loaded in our memory 
	 * before initialisation.
	 */
	nxt = node_access(addr, segaddr_shift(addr), nxt);

	/**
	 * TODO: Consider adding a state here to return in case node_access()
	 * requires some time to complete its operation.
	 */

	nt->nt_init(addr, segaddr_shift(addr), ksize, vsize, nt->nt_id, seg,
		    tx);
	return nxt;
}
#endif

static bool node_invariant(const struct nd *node)
{
	return node->n_type->nt_invariant(node);
}

#ifndef __KERNEL__
/**
 * This function is implemented for debugging purpose and should get called in
 * node lock mode.
 */
static bool node_expensive_invariant(const struct nd *node)
{
	return node->n_type->nt_expensive_invariant(node);
}
#endif

#if 0
static bool node_verify(const struct nd *node)
{
	return node->n_type->nt_verify(&node->n_addr);
}
#endif

#ifndef __KERNEL__
/**
 * This function should be called after acquiring node lock.
 */
static bool node_isvalid(const struct nd *node)
{
	if (node->n_be_node_valid)
		return node->n_type->nt_isvalid(&node->n_addr);

	return false;
}

#endif
static int node_count(const struct nd *node)
{
	M0_PRE(node_invariant(node));
	return node->n_type->nt_count(node);
}

static int node_count_rec(const struct nd *node)
{
	M0_PRE(node_invariant(node));
	return node->n_type->nt_count_rec(node);
}
static int node_space(const struct nd *node)
{
	M0_PRE(node_invariant(node));
	return node->n_type->nt_space(node);
}

#ifndef __KERNEL__
static int node_level(const struct nd *node)
{
	M0_PRE(node_invariant(node));
	return (node->n_type->nt_level(node));
}

static int node_shift(const struct nd *node)
{
	M0_PRE(node_invariant(node));
	return (node->n_type->nt_shift(node));
}
static int node_keysize(const struct nd *node)
{
	M0_PRE(node_invariant(node));
	return (node->n_type->nt_keysize(node));
}

static int node_valsize(const struct nd *node)
{
	M0_PRE(node_invariant(node));
	return (node->n_type->nt_valsize(node));
}

/**
 * If predict is set as true,
 *        If predict is 'true' the function returns a possibility of underflow if
 *         another record is deleted from this node without addition of any more
 *         records.
 * If predict is 'false' the function returns the node's current underflow
 * state.
 */
static bool  node_isunderflow(const struct nd *node, bool predict)
{
	M0_PRE(node_invariant(node));
	return node->n_type->nt_isunderflow(node, predict);
}

static bool  node_isoverflow(const struct nd *node)
{
	M0_PRE(node_invariant(node));
	return node->n_type->nt_isoverflow(node);
}
#endif
#if 0
static void node_fid(const struct nd *node, struct m0_fid *fid)
{
	M0_PRE(node_invariant(node));
	node->n_type->nt_fid(node, fid);
}
#endif

static void node_rec(struct slot *slot)
{
	M0_PRE(node_invariant(slot->s_node));
	slot->s_node->n_type->nt_rec(slot);
}

#ifndef __KERNEL__
static void node_key(struct slot *slot)
{
	M0_PRE(node_invariant(slot->s_node));
	slot->s_node->n_type->nt_key(slot);
}

static void node_child(struct slot *slot, struct segaddr *addr)
{
	M0_PRE(node_invariant(slot->s_node));
	slot->s_node->n_type->nt_child(slot, addr);
}
#endif

static bool node_isfit(struct slot *slot)
{
	M0_PRE(node_invariant(slot->s_node));
	return slot->s_node->n_type->nt_isfit(slot);
}

static void node_done(struct slot *slot, struct m0_be_tx *tx, bool modified)
{
	M0_PRE(node_invariant(slot->s_node));
	slot->s_node->n_type->nt_done(slot, tx, modified);
}

static void node_make(struct slot *slot, struct m0_be_tx *tx)
{
	M0_PRE(node_invariant(slot->s_node));
	slot->s_node->n_type->nt_make(slot, tx);
}

#ifndef __KERNEL__
static bool node_find(struct slot *slot, const struct m0_btree_key *key)
{
	M0_PRE(node_invariant(slot->s_node));
	return slot->s_node->n_type->nt_find(slot, key);
}
#endif

/**
 * Increment the sequence counter by one. This function needs to called whenever
 * there is change in node.
 */
static void node_seq_cnt_update(struct nd *node)
{
	M0_PRE(node_invariant(node));
	node->n_seq++;
}

static void node_fix(const struct nd *node, struct m0_be_tx *tx)
{
	M0_PRE(node_invariant(node));
	node->n_type->nt_fix(node, tx);
}

#if 0
static void node_cut(const struct nd *node, int idx, int size,
		     struct m0_be_tx *tx)
{
	M0_PRE(node_invariant(node));
	node->n_type->nt_cut(node, idx, size, tx);
}
#endif

static void node_del(const struct nd *node, int idx, struct m0_be_tx *tx)
{
	M0_PRE(node_invariant(node));
	node->n_type->nt_del(node, idx, tx);
}

#ifndef __KERNEL__
static void node_set_level(const struct nd *node, uint8_t new_level,
			   struct m0_be_tx *tx)
{
	M0_PRE(node_invariant(node));
	node->n_type->nt_set_level(node, new_level, tx);
}

static void node_move(struct nd *src, struct nd *tgt, enum direction dir,
		      int nr, struct m0_be_tx *tx)
{
	M0_PRE(node_invariant(src));
	M0_PRE(node_invariant(tgt));
	M0_IN(dir,(D_LEFT, D_RIGHT));
	tgt->n_type->nt_move(src, tgt, dir, nr, tx);
}

static void node_capture(struct slot *slot, struct m0_be_tx *tx)
{
	slot->s_node->n_type->nt_capture(slot, tx);
}

static void node_lock(struct nd *node)
{
	m0_rwlock_write_lock(&node->n_lock);
}

static void node_unlock(struct nd *node)
{
	m0_rwlock_write_unlock(&node->n_lock);
}

static void node_fini(const struct nd *node, struct m0_be_tx *tx)
{
	node->n_type->nt_fini(node, tx);
}
#endif
static struct mod *mod_get(void)
{
	return m0_get()->i_moddata[M0_MODULE_BTREE];
}

enum {
	NTYPE_NR = 0x100,
	TTYPE_NR = 0x100
};

struct mod {
	const struct node_type     *m_ntype[NTYPE_NR];
	const struct m0_btree_type *m_ttype[TTYPE_NR];
};

int m0_btree_mod_init(void)
{
	struct mod *m;

	/** Initialtise lru list, active list and lock. */
	ndlist_tlist_init(&btree_lru_nds);
	ndlist_tlist_init(&btree_active_nds);
	m0_rwlock_init(&list_lock);

	M0_ALLOC_PTR(m);
	if (m != NULL) {
		m0_get()->i_moddata[M0_MODULE_BTREE] = m;
		return 0;
	} else
		return M0_ERR(-ENOMEM);
}

void m0_btree_mod_fini(void)
{
	struct nd* node;

	if (!ndlist_tlist_is_empty(&btree_lru_nds))
		m0_tl_teardown(ndlist, &btree_lru_nds, node) {
			ndlist_tlink_fini(node);
			m0_rwlock_fini(&node->n_lock);
			m0_free(node);
		}
	ndlist_tlist_fini(&btree_lru_nds);

	if (!ndlist_tlist_is_empty(&btree_active_nds))
		m0_tl_teardown(ndlist, &btree_active_nds, node) {
			ndlist_tlink_fini(node);
			m0_rwlock_fini(&node->n_lock);
			m0_free(node);
		}
	ndlist_tlist_fini(&btree_active_nds);

	m0_rwlock_fini(&list_lock);
	m0_free(mod_get());
}

#ifndef __KERNEL__
static bool node_shift_is_valid(int shift)
{
	return shift >= NODE_SHIFT_MIN && shift < NODE_SHIFT_MIN + 0x10;
}

/**
 * Tells if the segment address is aligned to 512 bytes.
 * This function should be called right after the allocation to make sure that
 * the allocated memory starts at a properly aligned address.
 *
 * @param addr is the start address of the allocated space.
 *
 * @return True if the input address is properly aligned.
 */
static bool addr_is_aligned(const void *addr)
{
	return ((size_t)addr & ((1ULL << NODE_SHIFT_MIN) - 1)) == 0;
}
#endif

/**
 * Validates the segment address (of node).
 *
 * @param seg_addr points to the start address (of the node) in the segment.
 *
 * @return True if seg_addr is VALID according to the segment
 *                address semantics.
 */
static bool segaddr_is_valid(const struct segaddr *seg_addr)
{
	return (0xff000000000001f0ull & seg_addr->as_core) == 0;
}

#ifndef __KERNEL__
/**
 * Returns a segaddr formatted segment address.
 *
 * @param addr  is the start address (of the node) in the segment.
 *        shift is the size of the node as pow-of-2 value.
 *
 * @return Formatted Segment address.
 */
static struct segaddr segaddr_build(const void *addr, int shift)
{
	struct segaddr sa;
	M0_PRE(node_shift_is_valid(shift));
	M0_PRE(addr_is_aligned(addr));
	sa.as_core = ((uint64_t)addr) | (shift - NODE_SHIFT_MIN);
	M0_POST(segaddr_is_valid(&sa));
	M0_POST(segaddr_addr(&sa) == addr);
	M0_POST(segaddr_shift(&sa) == shift);
	return sa;
}
#endif

/**
 * Returns the CPU addressable pointer from the formatted segment address.
 *
 * @param seg_addr points to the formatted segment address.
 *
 * @return CPU addressable value.
 */
static void* segaddr_addr(const struct segaddr *seg_addr)
{
	M0_PRE(segaddr_is_valid(seg_addr));
	return (void *)(seg_addr->as_core & ~((1ULL << NODE_SHIFT_MIN) - 1));
}

/**
 * Returns the size (pow-of-2) of the node extracted out of the segment address.
 *
 * @param seg_addr points to the formatted segment address.
 *
 * @return Size of the node as pow-of-2 value.
 */
static int segaddr_shift(const struct segaddr *addr)
{
	M0_PRE(segaddr_is_valid(addr));
	return (addr->as_core & 0xf) + NODE_SHIFT_MIN;
}

#ifndef __KERNEL__
/**
 * Returns the node type stored at segment address.
 *
 * @param seg_addr points to the formatted segment address.
 *
 * @return Node type.
 */
uint32_t segaddr_ntype_get(const struct segaddr *addr)
{
	struct node_header *h  =  segaddr_addr(addr) +
				  sizeof(struct m0_format_header);

	/** Modify M0_PRE as and when more node type support is addedd. */
	M0_PRE(h->h_node_type == BNT_FIXED_FORMAT);
	return h->h_node_type;
}
#endif

#if 0
static void node_type_register(const struct node_type *nt)
{
	struct mod *m = mod_get();

	M0_PRE(IS_IN_ARRAY(nt->nt_id, m->m_ntype));
	M0_PRE(m->m_ntype[nt->nt_id] == NULL);
	m->m_ntype[nt->nt_id] = nt;
}

static void node_type_unregister(const struct node_type *nt)
{
	struct mod *m = mod_get();

	M0_PRE(IS_IN_ARRAY(nt->nt_id, m->m_ntype));
	M0_PRE(m->m_ntype[nt->nt_id] == nt);
	m->m_ntype[nt->nt_id] = NULL;
}

static void tree_type_register(const struct m0_btree_type *tt)
{
	struct mod *m = mod_get();

	M0_PRE(IS_IN_ARRAY(tt->tt_id, m->m_ttype));
	M0_PRE(m->m_ttype[tt->tt_id] == NULL);
	m->m_ttype[tt->tt_id] = tt;
}

static void tree_type_unregister(const struct m0_btree_type *tt)
{
	struct mod *m = mod_get();

	M0_PRE(IS_IN_ARRAY(tt->tt_id, m->m_ttype));
	M0_PRE(m->m_ttype[tt->tt_id] == tt);
	m->m_ttype[tt->tt_id] = NULL;
}
#endif
#ifndef __KERNEL__

static const struct node_type fixed_format;

static const struct node_type *btree_node_format[] = {
	[BNT_FIXED_FORMAT]                        = &fixed_format,
	[BNT_FIXED_KEYSIZE_VARIABLE_VALUESIZE]    = NULL,
	[BNT_VARIABLE_KEYSIZE_FIXED_VALUESIZE]    = NULL,
	[BNT_VARIABLE_KEYSIZE_VARIABLE_VALUESIZE] = NULL,
};
#endif

#ifndef __KERNEL__

/**
 * Locates a tree descriptor whose root node points to the node at addr and
 * return this tree to the caller.
 * If an existing tree descriptor pointing to this root node is not found then
 * a new tree descriptor is allocated from the free pool and the root node is
 * assigned to this new tree descriptor.
 * If root node pointer is not provided then this function will just allocate a
 * tree descriptor and return it to the caller. This functionality currently is
 * used by the create_tree function.
 *
 * @param op is used to exchange operation parameters and return values..
 * @param addr is the segment address of root node.
 * @param nxt is the next state to be returned to the caller.
 *
 * @return Next state to proceed in.
 */
static int64_t tree_get(struct node_op *op, struct segaddr *addr, int nxt)
{
	struct td              *tree = NULL;
	struct nd              *node = NULL;

	if (addr != NULL) {
		nxt  = node_get(op, NULL, addr, nxt);
		if (op->no_op.o_sm.sm_rc < 0)
			return op->no_op.o_sm.sm_rc;
		node = op->no_node;
		m0_rwlock_write_lock(&list_lock);
		tree = node->n_tree;

		if (tree == NULL) {
			tree = m0_alloc(sizeof *tree);
			m0_rwlock_init(&tree->t_lock);
			m0_rwlock_write_lock(&tree->t_lock);

			tree->t_ref = 1;
			tree->t_starttime = 0;
			tree->t_root = node;
			tree->t_height = node_level(node) + 1;
			node_lock(node);
			node->n_tree = tree;
			node_unlock(node);

		} else {
			m0_rwlock_write_lock(&tree->t_lock);
			tree->t_ref++;
		}

		op->no_node = tree->t_root;
		op->no_tree = tree;
		m0_rwlock_write_unlock(&tree->t_lock);
		m0_rwlock_write_unlock(&list_lock);
	} else
		return M0_ERR(-EINVAL);

	return nxt;
}
#endif


#if 0
/**
 * Creates a tree with an empty root node.
 *
 * @param op is used to exchange operation parameters and return values.
 * @param tt is the btree type to be assiged to the newly created btree.
 * @param rootshift is the size of the root node.
 * @param tx captures the operation in a transaction.
 * @param nxt is the next state to be returned to the caller.
 *
 * @return Next state to proceed in.
 */
static int64_t tree_create(struct node_op *op, struct m0_btree_type *tt,
			   int rootshift, struct m0_be_tx *tx, int nxt)
{
	struct td *tree;

	/**
	 * Creates root node and then assigns a tree descriptor for this root
	 * node.
	 */

	tree_get(op, NULL, nxt);

	tree = op->no_tree;
	node_alloc(op, tree, rootshift, &fixed_format, 8, 8, tx, nxt);

	m0_rwlock_write_lock(&tree->t_lock);
	tree->t_root = op->no_node;
	tree->t_type = tt;
	m0_rwlock_write_unlock(&tree->t_lock);

	return nxt;
}
#endif

#ifndef __KERNEL__
/**
 * Deletes an existing tree.
 *
 * @param op is used to exchange operation parameters and return values..
 * @param tree points to the tree to be deleted.
 * @param tx captures the operation in a transaction.
 * @param nxt is the next state to be returned to the caller.
 *
 * @return Next state to proceed in.
 */
static int64_t tree_delete(struct node_op *op, struct td *tree,
			   struct m0_be_tx *tx, int nxt)
{
	M0_PRE(tree != NULL);
	struct nd *root = tree->t_root;

	op->no_tree = tree;
	op->no_node = root;

	node_fini(op->no_node, tx);
	node_free(op, op->no_node, tx, nxt);
	tree_put(tree);

	return nxt;
}

/**
 * Returns the tree to the free tree pool if the reference count for this tree
 * reaches zero.
 *
 * @param tree points to the tree to be released.
 *
 * @return Next state to proceed in.
 */
static void tree_put(struct td *tree)
{
	m0_rwlock_write_lock(&list_lock);
	m0_rwlock_write_lock(&tree->t_lock);
	M0_ASSERT(tree->t_ref > 0);
	M0_ASSERT(tree->t_root != NULL);

	tree->t_ref--;

	if (tree->t_ref == 0) {
		node_lock(tree->t_root);
		tree->t_root->n_tree = NULL;
		node_unlock(tree->t_root);
		m0_rwlock_write_unlock(&tree->t_lock);
		m0_rwlock_fini(&tree->t_lock);
		m0_rwlock_write_unlock(&list_lock);
		m0_free0(&tree);
		return;
	}
	m0_rwlock_write_unlock(&tree->t_lock);
	m0_rwlock_write_unlock(&list_lock);

}

/**
 * This function loads the node descriptor for the node at segaddr in memory.
 * If a node descriptor pointing to this node is already loaded in memory then
 * this function will increment the reference count in the node descriptor
 * before returning it to the caller.
 * If the parameter tree is NULL then the function assumes the node at segaddr
 * to be the root node and will also load the tree descriptor in memory for
 * this root node.
 *
 * @param op load operation to perform.
 * @param tree pointer to tree whose node is to be loaded or NULL if tree has
 *             not been loaded.
 * @param addr node address in the segment.
 * @param nxt state to return on successful completion
 *
 * @return next state
 */
static int64_t node_get(struct node_op *op, struct td *tree,
			struct segaddr *addr, int nxt)
{
	const struct node_type *nt;
	struct nd              *node;
	bool                    in_lrulist;
	uint32_t                ntype;

	/**
	 * TODO: Include function node_access() during async mode of btree
	 * operations to ensure that the node data is loaded form the segment.
	 * Also consider adding a state here to return as we might need some
	 * time to load the node if it is not loaded.
	 */

	/**
	 * TODO: Adding list_lock to protect from multiple threads
	 * accessing the same node descriptor concurrently.
	 * Replace it with a different global lock once hash
	 * functionality is implemented.
	 */

	m0_rwlock_write_lock(&list_lock);

	/**
	 * If the node was deleted before node_get can acquire list_lock, then
	 * restart the tick funcions.
	 */
	if (!segaddr_header_isvalid(addr)) {
		op->no_op.o_sm.sm_rc = M0_ERR(-ECHILD);
		m0_rwlock_write_unlock(&list_lock);
		return nxt;
	}
	ntype = segaddr_ntype_get(addr);
	nt = btree_node_format[ntype];

	op->no_node = nt->nt_opaque_get(addr);

	if (op->no_node != NULL &&
	    op->no_node->n_addr.as_core == addr->as_core) {

		node_lock(op->no_node);
		if (!op->no_node->n_be_node_valid) {
			op->no_op.o_sm.sm_rc = M0_ERR(-EACCES);
			node_unlock(op->no_node);
			m0_rwlock_write_unlock(&list_lock);
			return nxt;
		}

		in_lrulist = op->no_node->n_ref == 0;
		op->no_node->n_ref++;
		if (in_lrulist) {
			/**
			 * The node descriptor is in LRU list. Remove from lru
			 * list and add to active list.
			 */
			ndlist_tlist_del(op->no_node);
			ndlist_tlist_add(&btree_active_nds, op->no_node);
			/**
			 * Update nd::n_tree  to point to tree descriptor as we
			 * as we had set it to NULL in node_put(). For more
			 * details Refer comment in node_put().
			 */
			op->no_node->n_tree = tree;
		}
		node_unlock(op->no_node);
	} else {
		/**
		 * If node descriptor is already allocated for the node, no need
		 * to allocate node descriptor again.
		 */
		op->no_node = nt->nt_opaque_get(addr);
		if (op->no_node != NULL &&
		    op->no_node->n_addr.as_core == addr->as_core) {
			node_lock(op->no_node);
			op->no_node->n_ref++;
			node_unlock(op->no_node);
			m0_rwlock_write_unlock(&list_lock);
			return nxt;
		}
		/**
		 * If node descriptor is not present allocate a new one
		 * and assign to node.
		 */
		node = m0_alloc(sizeof *node);
		/**
		 * TODO: If Node-alloc fails, free up any node descriptor from
		 * lru list and add assign to node. Unmap and map back the node
		 * segment. Take up with BE segment task.
		 */
		M0_ASSERT(node != NULL);
		node->n_addr          = *addr;
		node->n_tree          = tree;
		node->n_type          = nt;
		node->n_seq           = m0_time_now();
		node->n_ref           = 1;
		node->n_txref         = 0;
		node->n_be_node_valid = true;
		m0_rwlock_init(&node->n_lock);
		op->no_node           = node;
		nt->nt_opaque_set(addr, node);
		ndlist_tlink_init_at(op->no_node, &btree_active_nds);
	}
	m0_rwlock_write_unlock(&list_lock);
	return nxt;
}

/**
 * This function decrements the reference count for this node descriptor and if
 * the reference count reaches '0' then the node descriptor is moved to LRU
 * list.
 *
 * @param op load operation to perform.
 * @param node node descriptor.
 * @param tx changes will be captured in this transaction.
 *
 */
static void node_put(struct node_op *op, struct nd *node, struct m0_be_tx *tx)
{

	M0_PRE(node != NULL);

	m0_rwlock_write_lock(&list_lock);
	node_lock(node);
	node->n_ref--;
	if (node->n_ref == 0) {
		/**
		 * The node descriptor is in tree's active list. Remove from
		 * active list and add to lru list
		 */
		ndlist_tlist_del(node);
		ndlist_tlist_add(&btree_lru_nds, node);
		/**
		 * In case tree desriptor gets deallocated while node sits in
		 * the LRU list, we do not want node descriptor to point to an
		 * invalid tree descriptor. Hence setting nd::n_tree to NULL, it
		 * will again be populated in node_get().
		 */
		node->n_tree = NULL;

		if (!node->n_be_node_valid && node->n_txref == 0) {
			ndlist_tlink_del_fini(node);
			node_unlock(node);
			m0_rwlock_fini(&node->n_lock);
			m0_free(node);
			m0_rwlock_write_unlock(&list_lock);
			return;
		}
	}
	node_unlock(node);
	m0_rwlock_write_unlock(&list_lock);
}
#endif

# if 0
static struct nd *node_try(struct td *tree, struct segaddr *addr){
	return NULL;
}
#endif

#ifndef __KERNEL__
static int64_t node_free(struct node_op *op, struct nd *node,
			 struct m0_be_tx *tx, int nxt)
{
	int shift = node->n_type->nt_shift(node);

	m0_rwlock_write_lock(&list_lock);
	node_lock(node);
	node->n_ref--;
	node->n_be_node_valid = false;
	op->no_addr = node->n_addr;
	m0_free_aligned(segaddr_addr(&op->no_addr), 1ULL << shift, shift);
	/** Capture in transaction */

	if (node->n_ref == 0 && node->n_txref == 0) {
		ndlist_tlink_del_fini(node);
		node_unlock(node);
		m0_rwlock_fini(&node->n_lock);
		m0_free(node);
		m0_rwlock_write_unlock(&list_lock);
		/** Capture in transaction */
		return nxt;
	}
	node_unlock(node);
	m0_rwlock_write_unlock(&list_lock);
	return nxt;
}

/**
 * Allocates node in the segment and a node-descriptor if all the resources are
 * available.
 *
 * @param op indicates node allocate operation.
 * @param tree points to the tree this node will be a part-of.
 * @param shift is a power-of-2 size of this node.
 * @param nt points to the node type
 * @param ksize is the size of key (if constant) if not this contains '0'.
 * @param vsize is the size of value (if constant) if not this contains '0'.
 * @param tx points to the transaction which captures this operation.
 * @param nxt tells the next state to return when operation completes
 *
 * @return int64_t
 */
static int64_t node_alloc(struct node_op *op, struct td *tree, int shift,
			  const struct node_type *nt, int ksize, int vsize,
			  struct m0_be_tx *tx, int nxt)
{
	int            nxt_state = nxt;
	void          *area;
	int            size = 1ULL << shift;
	struct m0_buf  buf;

	M0_PRE(op->no_opc == NOP_ALLOC);
	M0_PRE(node_shift_is_valid(shift));

	buf = M0_BUF_INIT(size, NULL);
	M0_BE_ALLOC_ALIGN_BUF_SYNC(&buf, shift, tree->t_seg, tx);
	area = buf.b_addr;

	M0_ASSERT(area != NULL);

	op->no_addr = segaddr_build(area, shift);
	op->no_tree = tree;

	nxt_state = node_init(&op->no_addr, ksize, vsize, nt, tree->t_seg, tx,
			      nxt);
	/**
	 * TODO: Consider adding a state here to return in case we might need to
	 * visit node_init() again to complete its execution.
	 */
	
	nxt_state = node_get(op, tree, &op->no_addr, nxt_state);

	return nxt_state;
}

static void node_op_fini(struct node_op *op)
{
}

#endif



/**
 *  Structure of the node in persistent store.
 */
struct ff_head {
	struct m0_format_header  ff_fmt;    /*< Node Header */
	struct node_header       ff_seg;    /*< Node type information */
	uint16_t                 ff_used;   /*< Count of records */
	uint8_t                  ff_shift;  /*< Node size as pow-of-2 */
	uint8_t                  ff_level;  /*< Level in Btree */
	uint16_t                 ff_ksize;  /*< Size of key in bytes */
	uint16_t                 ff_vsize;  /*< Size of value in bytes */
	struct m0_format_footer  ff_foot;   /*< Node Footer */
	void                    *ff_opaque; /*< opaque data */
	/**
	 *  This space is used to host the Keys and Values upto the size of the
	 *  node
	 */
} M0_XCA_RECORD M0_XCA_DOMAIN(be);

enum m0_be_bnode_format_version {
	M0_BE_BNODE_FORMAT_VERSION_1 = 1,

	/* future versions, uncomment and update M0_BE_BNODE_FORMAT_VERSION */
	/*M0_BE_BNODE_FORMAT_VERSION_2,*/
	/*M0_BE_BNODE_FORMAT_VERSION_3,*/

	/** Current version, should point to the latest version present */
	M0_BE_BNODE_FORMAT_VERSION = M0_BE_BNODE_FORMAT_VERSION_1
};

static void ff_init(const struct segaddr *addr, int shift, int ksize, int vsize,
		    uint32_t ntype, struct m0_be_seg *seg, struct m0_be_tx *tx);
static void ff_fini(const struct nd *node, struct m0_be_tx *tx);
static int  ff_count(const struct nd *node);
static int  ff_count_rec(const struct nd *node);
static int  ff_space(const struct nd *node);
static int  ff_level(const struct nd *node);
static int  ff_shift(const struct nd *node);
static int  ff_valsize(const struct nd *node);
static int  ff_keysize(const struct nd *node);
static bool ff_isunderflow(const struct nd *node, bool predict);
static bool ff_isoverflow(const struct nd *node);
static void ff_fid(const struct nd *node, struct m0_fid *fid);
static void ff_rec(struct slot *slot);
static void ff_node_key(struct slot *slot);
static void ff_child(struct slot *slot, struct segaddr *addr);
static bool ff_isfit(struct slot *slot);
static void ff_done(struct slot *slot, struct m0_be_tx *tx, bool modified);
static void ff_make(struct slot *slot, struct m0_be_tx *tx);
static bool ff_find(struct slot *slot, const struct m0_btree_key *key);
static void ff_fix(const struct nd *node, struct m0_be_tx *tx);
static void ff_cut(const struct nd *node, int idx, int size,
		   struct m0_be_tx *tx);
static void ff_del(const struct nd *node, int idx, struct m0_be_tx *tx);
static void ff_set_level(const struct nd *node, uint8_t new_level,
			 struct m0_be_tx *tx);
static void generic_move(struct nd *src, struct nd *tgt,
			 enum direction dir, int nr, struct m0_be_tx *tx);
static bool ff_invariant(const struct nd *node);
static bool ff_expensive_invariant(const struct nd *node);
static bool ff_verify(const struct nd *node);
static void ff_opaque_set(const struct segaddr *addr, void *opaque);
static void *ff_opaque_get(const struct segaddr *addr);
static void ff_capture(struct slot *slot, struct m0_be_tx *tx);
/* uint16_t ff_ksize_get(const struct segaddr *addr); */
/* uint16_t ff_valsize_get(const struct segaddr *addr);  */

/**
 *  Implementation of node which supports fixed format/size for Keys and Values
 *  contained in it.
 */
static const struct node_type fixed_format = {
	.nt_id                    = BNT_FIXED_FORMAT,
	.nt_name                  = "m0_bnode_fixed_format",
	//.nt_tag,
	.nt_init                  = ff_init,
	.nt_fini                  = ff_fini,
	.nt_count                 = ff_count,
	.nt_count_rec             = ff_count_rec,
	.nt_space                 = ff_space,
	.nt_level                 = ff_level,
	.nt_shift                 = ff_shift,
	.nt_keysize               = ff_keysize,
	.nt_valsize               = ff_valsize,
	.nt_isunderflow           = ff_isunderflow,
	.nt_isoverflow            = ff_isoverflow,
	.nt_fid                   = ff_fid,
	.nt_rec                   = ff_rec,
	.nt_key                   = ff_node_key,
	.nt_child                 = ff_child,
	.nt_isfit                 = ff_isfit,
	.nt_done                  = ff_done,
	.nt_make                  = ff_make,
	.nt_find                  = ff_find,
	.nt_fix                   = ff_fix,
	.nt_cut                   = ff_cut,
	.nt_del                   = ff_del,
	.nt_set_level             = ff_set_level,
	.nt_move                  = generic_move,
	.nt_invariant             = ff_invariant,
	.nt_expensive_invariant   = ff_expensive_invariant,
	.nt_isvalid               = segaddr_header_isvalid,
	.nt_verify                = ff_verify,
	.nt_opaque_set            = ff_opaque_set,
	.nt_opaque_get            = ff_opaque_get,
	.nt_capture               = ff_capture,
	/* .nt_ksize_get          = ff_ksize_get, */
	/* .nt_valsize_get        = ff_valsize_get, */
};

#if 0
/**
 * Returns the key size stored at segment address.
 *
 * @param seg_addr points to the formatted segment address.
 *
 * @return key size
 */
uint16_t ff_ksize_get(const struct segaddr *addr)
{
	struct ff_head *h  =  segaddr_addr(addr);
	return h->ff_ksize;
}

/**
 * Returns the value size stored at segment address.
 *
 * @param seg_addr points to the formatted segment address.
 *
 * @return value size
 */
uint16_t ff_valsize_get(const struct segaddr *addr)
{
	struct ff_head *h  =  segaddr_addr(addr);
	return h->ff_vsize;
}
#endif

static struct ff_head *ff_data(const struct nd *node)
{
	return segaddr_addr(&node->n_addr);
}

static void *ff_key(const struct nd *node, int idx)
{
	struct ff_head *h    = ff_data(node);
	void           *area = h + 1;

	M0_PRE(ergo(!(h->ff_used == 0 && idx == 0),
		   (0 <= idx && idx <= h->ff_used)));
	return area + (h->ff_ksize + h->ff_vsize) * idx;
}

static void *ff_val(const struct nd *node, int idx)
{
	struct ff_head *h    = ff_data(node);
	void           *area = h + 1;

	M0_PRE(ergo(!(h->ff_used == 0 && idx == 0),
		    0 <= idx && idx <= h->ff_used));
	return area + (h->ff_ksize + h->ff_vsize) * idx + h->ff_ksize;
}

static bool ff_rec_is_valid(const struct slot *slot)
{
	struct ff_head *h = ff_data(slot->s_node);
	bool            val_is_valid;
	val_is_valid = h->ff_level > 0 ?
		       m0_vec_count(&slot->s_rec.r_val.ov_vec) <= h->ff_vsize :
		       m0_vec_count(&slot->s_rec.r_val.ov_vec) == h->ff_vsize;

	return
	   _0C(m0_vec_count(&slot->s_rec.r_key.k_data.ov_vec) == h->ff_ksize) &&
	   _0C(val_is_valid);
}

static bool ff_iskey_smaller(const struct nd *node, int cur_key_idx)
{
	struct ff_head          *h;
	struct m0_btree_key      key_prev;
	struct m0_btree_key      key_next;
	struct m0_bufvec_cursor  cur_prev;
	struct m0_bufvec_cursor  cur_next;
	void                    *p_key_prev;
	m0_bcount_t              ksize_prev;
	void                    *p_key_next;
	m0_bcount_t              ksize_next;
	int                      diff;
	int                      prev_key_idx = cur_key_idx;
	int                      next_key_idx = cur_key_idx + 1;

	h          = ff_data(node);
	ksize_prev = h->ff_ksize;
	ksize_next = h->ff_ksize;

	key_prev.k_data = M0_BUFVEC_INIT_BUF(&p_key_prev, &ksize_prev);
	key_next.k_data = M0_BUFVEC_INIT_BUF(&p_key_next, &ksize_next);

	p_key_prev = ff_key(node, prev_key_idx);
	p_key_next = ff_key(node, next_key_idx);

	m0_bufvec_cursor_init(&cur_prev, &key_prev.k_data);
	m0_bufvec_cursor_init(&cur_next, &key_next.k_data);
	diff = m0_bufvec_cursor_cmp(&cur_prev, &cur_next);
	if (diff >= 0)
		return false;
	return true;

}

static bool ff_expensive_invariant(const struct nd *node)
{
	int count = node_count(node);
	return _0C(ergo(count > 1, m0_forall(i, count - 1,
					     ff_iskey_smaller(node, i))));
}

static bool ff_invariant(const struct nd *node)
{
	const struct ff_head *h = ff_data(node);

	/* TBD: add check for h_tree_type after initializing it in node_init. */
	return  _0C(h->ff_fmt.hd_magic == M0_FORMAT_HEADER_MAGIC) &&
		_0C(h->ff_seg.h_node_type == BNT_FIXED_FORMAT) &&
		_0C(h->ff_ksize != 0) && _0C(h->ff_vsize != 0) &&
		_0C(h->ff_shift == segaddr_shift(&node->n_addr));
}

static bool ff_verify(const struct nd *node)
{
	const struct ff_head *h = ff_data(node);
	return m0_format_footer_verify(h, true) == 0;
}

static bool segaddr_header_isvalid(const struct segaddr *addr)
{
	struct ff_head       *h = segaddr_addr(addr);
	struct m0_format_tag  tag;

	if (h->ff_fmt.hd_magic != M0_FORMAT_HEADER_MAGIC)
		return false;

	m0_format_header_unpack(&tag, &h->ff_fmt);
	if (tag.ot_version != M0_BE_BNODE_FORMAT_VERSION ||
	    tag.ot_type != M0_FORMAT_TYPE_BE_BNODE)
	    return false;

	return true;
}

static void ff_init(const struct segaddr *addr, int shift, int ksize, int vsize,
		    uint32_t ntype, struct m0_be_seg *seg, struct m0_be_tx *tx)
{
	struct ff_head *h   = segaddr_addr(addr);

	M0_PRE(ksize != 0);
	M0_PRE(vsize != 0);
	M0_SET0(h);

	h->ff_shift           = shift;
	h->ff_ksize           = ksize;
	h->ff_vsize           = vsize;
	h->ff_seg.h_node_type = ntype;
	h->ff_opaque          = NULL;

	m0_format_header_pack(&h->ff_fmt, &(struct m0_format_tag){
		.ot_version       = M0_BE_BNODE_FORMAT_VERSION,
		.ot_type          = M0_FORMAT_TYPE_BE_BNODE,
		.ot_footer_offset = offsetof(struct ff_head, ff_foot)
	});
	m0_format_footer_update(h);

	/**
	 * This is the only time we capture the opaque data of the header. No
	 * other place should the opaque data get captured and written to BE
	 * segment.
	 */
}

static void ff_fini(const struct nd *node, struct m0_be_tx *tx)
{
	struct ff_head *h = ff_data(node);
	m0_format_header_pack(&h->ff_fmt, &(struct m0_format_tag){
		.ot_version       = 0,
		.ot_type          = 0,
		.ot_footer_offset = 0
	});

	h->ff_fmt.hd_magic = 0;
}

static int ff_count(const struct nd *node)
{
	int used = ff_data(node)->ff_used;
	if (ff_data(node)->ff_level > 0)
		used --;
	return used;
}

static int ff_count_rec(const struct nd *node)
{
	return ff_data(node)->ff_used;
}

static int ff_space(const struct nd *node)
{
	struct ff_head *h = ff_data(node);
	return (1ULL << h->ff_shift) - sizeof *h -
		(h->ff_ksize + h->ff_vsize) * h->ff_used;
}

static int ff_level(const struct nd *node)
{
	return ff_data(node)->ff_level;
}

static int ff_shift(const struct nd *node)
{
	return ff_data(node)->ff_shift;
}

static int ff_keysize(const struct nd *node)
{
	return ff_data(node)->ff_ksize;
}

static int ff_valsize(const struct nd *node)
{
	return ff_data(node)->ff_vsize;
}

static bool ff_isunderflow(const struct nd *node, bool predict)
{
	int16_t rec_count = ff_data(node)->ff_used;
	if (predict && rec_count != 0)
		rec_count--;
	return  rec_count == 0;
}

static bool ff_isoverflow(const struct nd *node)
{
	struct ff_head *h = ff_data(node);
	return (ff_space(node) < h->ff_ksize + h->ff_vsize) ? true : false;
}

static void ff_fid(const struct nd *node, struct m0_fid *fid)
{
}

static void ff_node_key(struct slot *slot);

static void ff_rec(struct slot *slot)
{
	struct ff_head *h = ff_data(slot->s_node);

	M0_PRE(ergo(!(h->ff_used == 0 && slot->s_idx == 0),
		    slot->s_idx <= h->ff_used));

	slot->s_rec.r_val.ov_vec.v_nr = 1;
	slot->s_rec.r_val.ov_vec.v_count[0] = h->ff_vsize;
	slot->s_rec.r_val.ov_buf[0] = ff_val(slot->s_node, slot->s_idx);
	ff_node_key(slot);
	M0_POST(ff_rec_is_valid(slot));
}

static void ff_node_key(struct slot *slot)
{
	const struct nd  *node = slot->s_node;
	struct ff_head   *h    = ff_data(node);

	M0_PRE(ergo(!(h->ff_used == 0 && slot->s_idx == 0),
		    slot->s_idx <= h->ff_used));

	slot->s_rec.r_key.k_data.ov_vec.v_nr = 1;
	slot->s_rec.r_key.k_data.ov_vec.v_count[0] = h->ff_ksize;
	slot->s_rec.r_key.k_data.ov_buf[0] = ff_key(slot->s_node, slot->s_idx);
}

static void ff_child(struct slot *slot, struct segaddr *addr)
{
	const struct nd *node = slot->s_node;
	struct ff_head  *h    = ff_data(node);

	M0_PRE(slot->s_idx < h->ff_used);
	*addr = *(struct segaddr *)ff_val(node, slot->s_idx);
}

static bool ff_isfit(struct slot *slot)
{
	struct ff_head *h = ff_data(slot->s_node);

	M0_PRE(ff_rec_is_valid(slot));
	return h->ff_ksize + h->ff_vsize <= ff_space(slot->s_node);
}

static void ff_done(struct slot *slot, struct m0_be_tx *tx, bool modified)
{
	/**
	 * not needed yet. In future if we want to calculate checksum per
	 * record, we might want to recode this function.
	*/
}

static void ff_make(struct slot *slot, struct m0_be_tx *tx)
{
	const struct nd *node  = slot->s_node;
	struct ff_head  *h     = ff_data(node);
	int              rsize = h->ff_ksize + h->ff_vsize;
	void            *start = ff_key(node, slot->s_idx);

	M0_PRE(ff_rec_is_valid(slot));
	M0_PRE(ff_isfit(slot));
	memmove(start + rsize, start, rsize * (h->ff_used - slot->s_idx));
	h->ff_used++;
	/** Capture these changes in ff_capture.*/
}

static bool ff_find(struct slot *slot, const struct m0_btree_key *find_key)
{
	struct ff_head          *h     = ff_data(slot->s_node);
	int                      i     = -1;
	int                      j     = node_count(slot->s_node);
	struct m0_btree_key      key;
	void                    *p_key;
	m0_bcount_t              ksize = h->ff_ksize;
	struct m0_bufvec_cursor  cur_1;
	struct m0_bufvec_cursor  cur_2;
	int                      diff;
	int                      m;

	key.k_data = M0_BUFVEC_INIT_BUF(&p_key, &ksize);

	M0_PRE(find_key->k_data.ov_vec.v_count[0] == h->ff_ksize);
	M0_PRE(find_key->k_data.ov_vec.v_nr == 1);

	while (i + 1 < j) {
		m = (i + j) / 2;

		key.k_data.ov_buf[0] = ff_key(slot->s_node, m);

		m0_bufvec_cursor_init(&cur_1, &key.k_data);
		m0_bufvec_cursor_init(&cur_2, &find_key->k_data);
		diff = m0_bufvec_cursor_cmp(&cur_1, &cur_2);

		M0_ASSERT(i < m && m < j);
		if (diff < 0)
			i = m;
		else if (diff > 0)
			j = m;
		else {
			i = j = m;
			break;
		}
	}

	slot->s_idx = j;

	return (i == j);
}

static void ff_fix(const struct nd *node, struct m0_be_tx *tx)
{
	struct ff_head *h = ff_data(node);

	m0_format_footer_update(h);
	/** Capture changes in ff_capture */
}

static void ff_cut(const struct nd *node, int idx, int size,
		   struct m0_be_tx *tx)
{
	M0_PRE(size == ff_data(node)->ff_vsize);
}

static void ff_del(const struct nd *node, int idx, struct m0_be_tx *tx)
{
	struct ff_head   *h     = ff_data(node);
	int               rsize = h->ff_ksize + h->ff_vsize;
	void             *start = ff_key(node, idx);

	M0_PRE(idx < h->ff_used);
	M0_PRE(h->ff_used > 0);
	memmove(start, start + rsize, rsize * (h->ff_used - idx - 1));
	h->ff_used--;
	/** Capture changes in ff_capture */
}

static void ff_set_level(const struct nd *node, uint8_t new_level,
			 struct m0_be_tx *tx)
{
	struct ff_head *h = ff_data(node);

	h->ff_level = new_level;
	/** Capture these changes in ff_capture.*/
}

static void ff_opaque_set(const struct  segaddr *addr, void *opaque)
{
	struct ff_head *h = segaddr_addr(addr);
	h->ff_opaque = opaque;
	/** This change should NEVER be captured.*/
}

static void *ff_opaque_get(const struct segaddr *addr)
{
	struct ff_head *h = segaddr_addr(addr);
	return h->ff_opaque;
}

static void generic_move(struct nd *src, struct nd *tgt,
			 enum direction dir, int nr, struct m0_be_tx *tx)
{
	struct slot  rec;
	struct slot  tmp;
	m0_bcount_t  rec_ksize;
	m0_bcount_t  rec_vsize;
	m0_bcount_t  temp_ksize;
	m0_bcount_t  temp_vsize;
	void        *rec_p_key;
	void        *rec_p_val;
	void        *temp_p_key;
	void        *temp_p_val;
	int          srcidx;
	int          tgtidx;
	int          last_idx_src;
	int          last_idx_tgt;

	rec.s_rec.r_key.k_data = M0_BUFVEC_INIT_BUF(&rec_p_key, &rec_ksize);
	rec.s_rec.r_val        = M0_BUFVEC_INIT_BUF(&rec_p_val, &rec_vsize);

	tmp.s_rec.r_key.k_data = M0_BUFVEC_INIT_BUF(&temp_p_key, &temp_ksize);
	tmp.s_rec.r_val        = M0_BUFVEC_INIT_BUF(&temp_p_val, &temp_vsize);

	M0_PRE(src != tgt);

	last_idx_src = node_count_rec(src);
	last_idx_tgt = node_count_rec(tgt);

	srcidx = dir == D_LEFT ? 0 : last_idx_src - 1;
	tgtidx = dir == D_LEFT ? last_idx_tgt : 0;

	while (true) {
		if (nr == 0 || (nr == NR_EVEN &&
			       (node_space(tgt) <= node_space(src))) ||
			       (nr == NR_MAX && (srcidx == -1 ||
			       node_count_rec(src) == 0)))
			break;

		/** Get the record at src index in rec. */
		rec.s_node = src;
		rec.s_idx  = srcidx;
		node_rec(&rec);

		/**
		 *  With record from src in rec; check if that record can fit in
		 *  the target node. If yes then make space to host this record
		 *  in target node.
		 */
		rec.s_node = tgt;
		rec.s_idx  = tgtidx;
		if (!node_isfit(&rec))
			break;
		node_make(&rec, tx);

		/** Get the location in the target node where the record from
		 *  the source node will be copied later
		 */
		tmp.s_node = tgt;
		tmp.s_idx  = tgtidx;
		node_rec(&tmp);

		rec.s_node = src;
		rec.s_idx  = srcidx;
		m0_bufvec_copy(&tmp.s_rec.r_key.k_data, &rec.s_rec.r_key.k_data,
			       m0_vec_count(&rec.s_rec.r_key.k_data.ov_vec));
		m0_bufvec_copy(&tmp.s_rec.r_val, &rec.s_rec.r_val,
			       m0_vec_count(&rec.s_rec.r_val.ov_vec));
		node_del(src, srcidx, tx);
		if (nr > 0)
			nr--;
		node_done(&tmp, tx, true);
		if (dir == D_LEFT)
			tgtidx++;
		else
			srcidx--;
	}
	node_seq_cnt_update(src);
	node_fix(src, tx);
	node_seq_cnt_update(tgt);
	node_fix(tgt, tx);
}

static void ff_capture(struct slot *slot, struct m0_be_tx *tx)
{
	struct ff_head   *h     = ff_data(slot->s_node);
	int               rsize = h->ff_ksize + h->ff_vsize;
	void             *start = ff_key(slot->s_node, slot->s_idx);
	struct m0_be_seg *seg   = slot->s_node->n_tree->t_seg;
	m0_bcount_t       hsize = sizeof(*h) - sizeof(h->ff_opaque);

	/**
	 *  Capture starting from the location where new record may have been
	 *  added or deleted. Capture till the last record. If the deleted
	 *  record was at the end then no records need to be captured only the
	 *  header modifications need to be persisted.
	 */
	if (h->ff_used > slot->s_idx)
		M0_BTREE_TX_CAPTURE(tx, seg, start,
				    rsize * (h->ff_used - slot->s_idx));
	else if (h->ff_opaque == NULL)
		/**
		 *  This will happen when the node is initialized in which case
		 *  we want to capture the opaque pointer.
		 */
		hsize += sizeof(h->ff_opaque);

	M0_BTREE_TX_CAPTURE(tx, seg, h, hsize);
}

#ifndef __KERNEL__
/**
 *  --------------------------------------------
 *  Section START - Btree Credit
 *  --------------------------------------------
 */
/**
 * Credit for btree put and delete operation.
 * For put operation, at max 2 nodes can get captured in each level plus an
 * extra node. The delete operation can use less nodes, still use the same api
 * to be on the safer side.
 *
 * @param accum transaction credit.
 */
static void btree_callback_credit(struct m0_be_tx_credit *accum)
{
	accum->tc_cb_nr += BTREE_CALLBACK_CREDIT;
}
#endif

/**
 *  --------------------------------------------
 *  Section END - Btree Credit
 *  --------------------------------------------
 */

#define COPY_RECORD(tgt, src)                                                  \
	({                                                                     \
		struct m0_btree_rec *__tgt_rec = (tgt);                        \
		struct m0_btree_rec *__src_rec = (src);                        \
									       \
		m0_bufvec_copy(&__tgt_rec->r_key.k_data,                       \
			       &__src_rec ->r_key.k_data,                      \
			       m0_vec_count(&__src_rec ->r_key.k_data.ov_vec));\
		m0_bufvec_copy(&__tgt_rec->r_val, &__src_rec->r_val,           \
			       m0_vec_count(&__src_rec ->r_val.ov_vec));       \
	})

#define COPY_VALUE(tgt, src)                                                   \
	({                                                                     \
		struct m0_btree_rec *__tgt_rec = (tgt);                        \
		struct m0_btree_rec *__src_rec = (src);                        \
									       \
		m0_bufvec_copy(&__tgt_rec->r_val, &__src_rec->r_val,           \
			       m0_vec_count(&__src_rec ->r_val.ov_vec));       \
	})

#define REC_INIT(pp_key, p_ksize, pp_val, p_vsize)                             \
	({                                                                     \
		struct m0_btree_rec __rec;                                     \
									       \
		__rec.r_key.k_data = M0_BUFVEC_INIT_BUF((pp_key), (p_ksize));  \
		__rec.r_val        = M0_BUFVEC_INIT_BUF((pp_val), (p_vsize));  \
		__rec;                                                         \
	})

/** Insert operation section start point: */
#ifndef __KERNEL__

static bool cookie_is_set(struct m0_bcookie *k_cookie)
{
	/* TBD : function definition */
	return false;
}

static bool cookie_is_used(void)
{
	/* TBD : function definition */
	return false;
}

static bool cookie_is_valid(struct td *tree, struct m0_bcookie *k_cookie)
{
	/* TBD : function definition */
	/* if given key is in cookie's last and first key */

	return false;
}

static int fail(struct m0_btree_op *bop, int rc)
{
	bop->bo_op.o_sm.sm_rc = rc;
	return m0_sm_op_sub(&bop->bo_op, P_CLEANUP, P_FINI);
}

/**
 * This function will validate the cookie or path traversed by the operation and
 * return result. If if cookie is used it will validate cookie else check for
 * traversed path.
 *
 * @param oi which provide all information about traversed nodes.
 * @param tree needed in case of cookie validation.
 * @param cookie provided by the user which needs to get validate if used.
 * @return bool return true if validation succeed else false.
 */
static bool path_check(struct m0_btree_oimpl *oi, struct td *tree,
		       struct m0_bcookie *k_cookie)
{
	int        total_level = oi->i_used;
	struct nd *l_node;

	if (cookie_is_used())
		return cookie_is_valid(tree, k_cookie);

	while (total_level >= 0) {
		l_node = oi->i_level[total_level].l_node;
		node_lock(l_node);
		if (!node_isvalid(l_node)) {
			node_unlock(l_node);
			node_op_fini(&oi->i_nop);
			return false;
		}
		if (oi->i_level[total_level].l_seq != l_node->n_seq) {
			node_unlock(l_node);
			return false;
		}
		node_unlock(l_node);
		total_level--;
	}
	return true;
}

/**
 * Validates the sibling node and its sequence number.
 *
 * @param oi provides traversed nodes information.
 * @return bool return true if validation succeeds else false.
 */
static bool sibling_node_check(struct m0_btree_oimpl *oi)
{
	struct nd *l_sibling = oi->i_level[oi->i_used].l_sibling;

	if (l_sibling == NULL || oi->i_pivot == -1)
		return true;

	node_lock(l_sibling);
	if (!node_isvalid(l_sibling)) {
		node_unlock(l_sibling);
		node_op_fini(&oi->i_nop);
		return false;
	}
	if (oi->i_level[oi->i_used].l_sib_seq != l_sibling->n_seq) {
		node_unlock(l_sibling);
		return false;
	}
	node_unlock(l_sibling);
	return true;
}

static int64_t lock_op_init(struct m0_sm_op *bo_op, struct node_op  *i_nop,
			    struct td *tree, int nxt)
{
	/** parameters which has passed but not will be used while state machine
	 *  implementation for  locks
	 */
	m0_rwlock_write_lock(&tree->t_lock);
	return nxt;
}

static void lock_op_unlock(struct td *tree)
{
	m0_rwlock_write_unlock(&tree->t_lock);
}

static void level_alloc(struct m0_btree_oimpl *oi, int height)
{
	oi->i_level = m0_alloc(height * (sizeof *oi->i_level));
}

static void level_put(struct m0_btree_oimpl *oi, struct m0_be_tx *tx)
{
	int i;
	for (i = 0; i <= oi->i_used; ++i) {
		if (oi->i_level[i].l_node != NULL) {
			node_put(&oi->i_nop, oi->i_level[i].l_node, tx);
			oi->i_level[i].l_node = NULL;
		}
		if (oi->i_level[i].l_sibling != NULL) {
			node_put(&oi->i_nop, oi->i_level[i].l_sibling, tx);
			oi->i_level[i].l_sibling = NULL;
		}
	}
}

static void level_cleanup(struct m0_btree_oimpl *oi, struct m0_be_tx *tx)
{
	/**
	 * This function assumes the thread is unlocked when level_cleanup runs.
	 * If ever there arises a need to call level_cleanup() with the lock
	 * owned by the calling thread then this routine will need some changes
	 * such as accepting a parameter which would tell us if the lock is
	 * already taken by this thread.
	 */
	int i;

	/** Put all the nodes back. */
	level_put(oi, tx);
	/** Free up allocated nodes. */
	for (i = 0; i < oi->i_height; ++i) {
		if (oi->i_level[i].l_alloc != NULL) {
			if (oi->i_level[i].i_alloc_in_use)
				node_put(&oi->i_nop, oi->i_level[i].l_alloc,
					 tx);
			else {
				oi->i_nop.no_opc = NOP_FREE;
				/**
				 * node_free() will not cause any I/O delay
				 * since this node was allocated in P_ALLOC
				 * phase in put_tick and I/O delay would have
				 * happened during the allocation.
				 */
				node_fini(oi->i_level[i].l_alloc, tx);
				node_free(&oi->i_nop, oi->i_level[i].l_alloc,
					  tx, 0);
				oi->i_level[i].l_alloc = NULL;
			}
		}
	}

	if (oi->i_extra_node != NULL) {
		/**
		 * extra_node will be used only if root splitting is done due to
		 * overflow at root node. Therefore, extra_node must have been
		 * used if l_alloc at root level is used.
		 */
		if (oi->i_level[0].i_alloc_in_use)
			node_put(&oi->i_nop, oi->i_extra_node, tx);
		else {
			oi->i_nop.no_opc = NOP_FREE;
			node_fini(oi->i_extra_node, tx);
			node_free(&oi->i_nop, oi->i_extra_node, tx, 0);
			oi->i_extra_node = NULL;
		}
	}
	m0_free(oi->i_level);
}

/**
 * Adds unique node descriptor address to m0_btree_oimpl::i_capture structure.
 */
static void btree_node_capture_enlist(struct m0_btree_oimpl *oi,
				      struct nd *addr, int start_idx)
{
	struct node_capture_info *arr = oi->i_capture;
	int                       i;

	M0_PRE(addr != NULL);

	for (i = 0; i < BTREE_CALLBACK_CREDIT; i++) {
		if (arr[i].nc_node == NULL) {
			arr[i].nc_node = addr;
			arr[i].nc_idx  = start_idx;
			break;
		} else if (arr[i].nc_node == addr) {
			arr[i].nc_idx = arr[i].nc_idx < start_idx ?
				        arr[i].nc_idx : start_idx;
			break;
		}
	}
}

/**
 * Checks if given segaddr is within segment boundaries.
*/
static bool address_in_segment(struct segaddr addr)
{
	//TBD: function definition
	return true;
}

/**
 * Callback to be invoked on transaction commit.
 */
static void btree_tx_commit_cb(void *payload)
{
	struct nd *node = payload;

	node_lock(node);
	M0_ASSERT(node->n_txref != 0);
	node->n_txref--;
	if (!node->n_be_node_valid && node->n_ref == 0 && node->n_txref == 0) {
		ndlist_tlink_del_fini(node);
		node_unlock(node);
		m0_rwlock_fini(&node->n_lock);
		m0_free(node);
		return;
	}
	node_unlock(node);
}

static void btree_tx_nodes_capture(struct m0_btree_oimpl *oi,
				  struct m0_be_tx *tx)
{
	struct node_capture_info *arr = oi->i_capture;
	struct slot          node_slot;
	int                       i;

	for (i = 0; i < BTREE_CALLBACK_CREDIT; i++) {
		if (arr[i].nc_node == NULL)
			break;

		node_slot.s_node = arr[i].nc_node;
		node_slot.s_idx  = arr[i].nc_idx;
		node_capture(&node_slot, tx);

		node_lock(arr[i].nc_node);
		arr[i].nc_node->n_txref++;
		node_unlock(arr[i].nc_node);
		M0_BTREE_TX_CB_CAPTURE(tx, arr[i].nc_node, &btree_tx_commit_cb);
	}
}

/**
 * This function gets called when splitting is done at root node. This function
 * is responsible to handle this scanario and ultimately root will point out to
 * the two splitted node.
 * @param bop structure for btree operation which contains all required data
 * @param new_rec will contain key and value as address pointing to newly
 * allocated node at root
 * @return int64_t return state which needs to get executed next
 */
static int64_t btree_put_root_split_handle(struct m0_btree_op *bop,
					   struct m0_btree_rec *new_rec)
{
	struct td              *tree       = bop->bo_arbor->t_desc;
	struct m0_btree_oimpl  *oi         = bop->bo_i;
	struct level           *lev        = &oi->i_level[0];
	m0_bcount_t             ksize;
	void                   *p_key;
	m0_bcount_t             vsize;
	void                   *p_val;

	bop->bo_rec   = *new_rec;

	/**
	 * When splitting is done at root node, tree height needs to get
	 * increased by one. As, we do not want to change the pointer to the
	 * root node, we will copy all contents from root to i_extra_node and
	 * make i_extra_node as one of the child of existing root
	 * 1) First copy all contents from root node to extra_node
	 * 2) add new 2 records at root node:
	 *      i.for first record, key = rec.r_key, value = rec.r_val
	 *      ii.for second record, key = null, value = segaddr(i_extra_node)
	 */

	int curr_max_level = node_level(lev->l_node);

	node_lock(lev->l_node);
	node_lock(oi->i_extra_node);

	node_set_level(oi->i_extra_node, curr_max_level, bop->bo_tx);
	node_set_level(lev->l_node, curr_max_level + 1, bop->bo_tx);

	node_move(lev->l_node, oi->i_extra_node, D_RIGHT, NR_MAX, bop->bo_tx);
	M0_ASSERT(node_count_rec(lev->l_node) == 0);

	/* 2) add new 2 records at root node. */

	/* Add first rec at root */
	struct slot node_slot = {
		.s_node = lev->l_node,
		.s_idx  = 0
	};
	node_slot.s_rec = bop->bo_rec;

	/* M0_ASSERT(node_isfit(&node_slot)) */
	node_make(&node_slot, bop->bo_tx);
	node_slot.s_rec = REC_INIT(&p_key, &ksize, &p_val, &vsize);
	node_rec(&node_slot);
	COPY_RECORD(&node_slot.s_rec, &bop->bo_rec);
	/* if we need to update vec_count for node, update here */

	node_done(&node_slot, bop->bo_tx, true);

	/* Add second rec at root */

	/**
	 * For second record, value = segaddr(i_extra_node).
	 * Note that, last key is not considered as valid key for internal node.
	 * Therefore, currently, key is not set as NULL explicitly.
	 * In future, depending on requirement, key, key size, value size might
	 * need to be set/store explicitly.
	 */
	bop->bo_rec.r_val.ov_buf[0] = &(oi->i_extra_node->n_addr);
	node_slot.s_idx  = 1;
	node_slot.s_rec = bop->bo_rec;
	node_make(&node_slot, bop->bo_tx);
	node_slot.s_rec = REC_INIT(&p_key, &ksize, &p_val, &vsize);
	node_rec(&node_slot);
	COPY_VALUE(&node_slot.s_rec, &bop->bo_rec);
	/* if we need to update vec_count for root slot, update at this place */

	node_done(&node_slot, bop->bo_tx, true);
	node_seq_cnt_update(lev->l_node);
	node_fix(lev->l_node, bop->bo_tx);
	/**
	 * Note : not capturing l_node as it must have already been captured in
	 * btree_put_makespace_phase().
	 */
	btree_node_capture_enlist(oi, oi->i_extra_node, 0);

	/* Increase height by one */
	tree->t_height++;
	bop->bo_arbor->t_height = tree->t_height;
	/* Capture this change in transaction */

	/* TBD : This check needs to be removed when debugging is done. */
	M0_ASSERT(node_expensive_invariant(lev->l_node));
	M0_ASSERT(node_expensive_invariant(oi->i_extra_node));
	node_unlock(lev->l_node);
	node_unlock(oi->i_extra_node);

	return P_CAPTURE;
}

/**
 * This function is called when there is overflow and splitting needs to be
 * done. It will move some records from right node(l_node) to left node(l_alloc)
 * and find the appropriate slot for given record. It will store the node and
 * index (where we need to insert given record) in tgt slot as a result.
 *
 * @param l_alloc It is the newly allocated node, where we want to move record.
 * @param l_node It is the current node, from where we want to move record.
 * @param rec It is the given record for which we want to find slot
 * @param tgt result of record find will get stored in tgt slot
 * @param tx It represents the transaction of which the current operation is
 * part of.
 */
static void btree_put_split_and_find(struct nd *allocated_node,
				     struct nd *current_node,
				     struct m0_btree_rec *rec, struct slot *tgt,
				     struct m0_be_seg *seg, struct m0_be_tx *tx)
{
	struct slot              right_slot;
	struct slot              left_slot;
	struct m0_bufvec_cursor  cur_1;
	struct m0_bufvec_cursor  cur_2;
	int                      diff;
	m0_bcount_t              ksize;
	void                    *p_key;
	m0_bcount_t              vsize;
	void                    *p_val;

	/* intialised slot for left and right node*/
	left_slot.s_node  = allocated_node;
	right_slot.s_node = current_node;

	/* 1)Move some records from current node to new node */

	node_set_level(allocated_node, node_level(current_node), tx);

	node_move(current_node, allocated_node, D_LEFT, NR_EVEN, tx);

	/*2) Find appropriate slot for given record */

	right_slot.s_idx = 0;
	right_slot.s_rec = REC_INIT(&p_key, &ksize, &p_val, &vsize);
	node_key(&right_slot);

	m0_bufvec_cursor_init(&cur_1, &rec->r_key.k_data);
	m0_bufvec_cursor_init(&cur_2, &right_slot.s_rec.r_key.k_data);

	diff = m0_bufvec_cursor_cmp(&cur_1, &cur_2);
	tgt->s_node = diff < 0 ? left_slot.s_node : right_slot.s_node;

	/**
	 * Corner case: If given record needs to be inseted at internal left
	 * node and if the key of given record is greater than key at last index
	 * of left record, initialised tgt->s_idx explicitly, as node_find will
	 * not compare key with last indexed key.
	 */
	if (node_level(tgt->s_node) > 0 && tgt->s_node == left_slot.s_node) {
		left_slot.s_idx = node_count(left_slot.s_node);
		left_slot.s_rec = REC_INIT(&p_key, &ksize, &p_val, &vsize);
		node_key(&left_slot);
		m0_bufvec_cursor_init(&cur_2, &left_slot.s_rec.r_key.k_data);
		diff = m0_bufvec_cursor_cmp(&cur_1, &cur_2);
		if (diff > 0) {
			tgt->s_idx = node_count(left_slot.s_node) + 1;
			return;
		}
	}
	node_find(tgt, &rec->r_key);
}

/**
 * This function is responsible to handle the overflow at node at particular
 * level. It will get called when given record is not able to fit in node. This
 * function will split the node and update bop->bo_rec which needs to get added
 * at parent node.
 *
 * If record is not able to fit in the node, split the node
 *     1) Move some records from current node(l_node) to new node(l_alloc).
 *     2) Insert given record to appropriate node.
 *     3) Modify last key from left node(in case of internal node) and key,
 *       value for record which needs to get inserted at parent.
 *
 * @param bop structure for btree operation which contains all required data.
 * @return int64_t return state which needs to get executed next.
 */
static int64_t btree_put_makespace_phase(struct m0_btree_op *bop)
{
	struct m0_btree_oimpl *oi         = bop->bo_i;
	struct level          *lev = &oi->i_level[oi->i_used];
	m0_bcount_t            ksize;
	void                  *p_key;
	m0_bcount_t            vsize;
	void                  *p_val;
	m0_bcount_t            ksize_1;
	void                  *p_key_1;
	m0_bcount_t            vsize_1;
	void                  *p_val_1;
	uint64_t               newvalue;
	m0_bcount_t            newvsize  = INTERNAL_NODE_VALUE_SIZE;
	void                  *newv_ptr  = &newvalue;
	struct m0_btree_rec    new_rec;
	struct slot            tgt;
	struct slot            node_slot;
	int                    i;

	/**
	 * move records from current node to new node and find slot for given
	 * record
	 */
	node_lock(lev->l_alloc);
	node_lock(lev->l_node);

	lev->i_alloc_in_use = true;

	btree_put_split_and_find(lev->l_alloc, lev->l_node, &bop->bo_rec, &tgt,
				 bop->bo_seg, bop->bo_tx);

	tgt.s_rec = bop->bo_rec;
	node_make (&tgt, bop->bo_tx);
	tgt.s_rec = REC_INIT(&p_key, &ksize, &p_val, &vsize);
	node_rec(&tgt);
	tgt.s_rec.r_flags = M0_BSC_SUCCESS;
	int rc = bop->bo_cb.c_act(&bop->bo_cb, &tgt.s_rec);
	if (rc) {
		/* If callback failed, undo make space, splitted node */
		node_del(tgt.s_node, tgt.s_idx, bop->bo_tx);
		node_done(&tgt, bop->bo_tx, true);
		tgt.s_node == lev->l_node ? node_seq_cnt_update(lev->l_node) :
					    node_seq_cnt_update(lev->l_alloc);
		node_fix(lev->l_node, bop->bo_tx);

		node_move(lev->l_alloc, lev->l_node, D_RIGHT,
		          NR_MAX, bop->bo_tx);
		lev->i_alloc_in_use = false;

		node_unlock(lev->l_alloc);
		node_unlock(lev->l_node);
		lock_op_unlock(bop->bo_arbor->t_desc);
		return fail(bop, rc);
	}
	node_done(&tgt, bop->bo_tx, true);
	tgt.s_node == lev->l_node ? node_seq_cnt_update(lev->l_node) :
				    node_seq_cnt_update(lev->l_alloc);
	node_fix(tgt.s_node, bop->bo_tx);
	btree_node_capture_enlist(oi, lev->l_alloc, 0);
	btree_node_capture_enlist(oi, lev->l_node, 0);

	/* TBD : This check needs to be removed when debugging is done. */
	M0_ASSERT(node_expensive_invariant(lev->l_alloc));
	M0_ASSERT(node_expensive_invariant(lev->l_node));
	node_unlock(lev->l_alloc);
	node_unlock(lev->l_node);

	/* Initialized new record which will get inserted at parent */
	node_slot.s_node = lev->l_node;
	node_slot.s_idx = 0;
	node_slot.s_rec = REC_INIT(&p_key, &ksize, &p_val, &vsize);
	node_key(&node_slot);
	new_rec.r_key = node_slot.s_rec.r_key;

	newvalue      = INTERNAL_NODE_VALUE_SIZE;
	newv_ptr      = &(lev->l_alloc->n_addr);
	new_rec.r_val = M0_BUFVEC_INIT_BUF(&newv_ptr, &newvsize);

	for (i = oi->i_used - 1; i >= 0; i--) {
		lev = &oi->i_level[i];
		node_slot.s_node = lev->l_node;
		node_slot.s_idx  = lev->l_idx;
		node_slot.s_rec  = new_rec;
		if (node_isfit(&node_slot)) {
			struct m0_btree_rec *rec;

			node_lock(lev->l_node);

			node_make(&node_slot, bop->bo_tx);
			node_slot.s_rec = REC_INIT(&p_key_1, &ksize_1,
						   &p_val_1, &vsize_1);
			node_rec(&node_slot);
			rec = &new_rec;
			COPY_RECORD(&node_slot.s_rec, rec);

			node_done(&node_slot, bop->bo_tx, true);
			node_seq_cnt_update(lev->l_node);
			node_fix(lev->l_node, bop->bo_tx);
			btree_node_capture_enlist(oi, lev->l_node, lev->l_idx);

			/**
			 * TBD : This check needs to be removed when debugging
			 * is done.
			 */
			M0_ASSERT(node_expensive_invariant(lev->l_node));
			node_unlock(lev->l_node);
			return P_CAPTURE;
		}

		node_lock(lev->l_alloc);
		node_lock(lev->l_node);

		lev->i_alloc_in_use = true;

		btree_put_split_and_find(lev->l_alloc, lev->l_node, &new_rec,
					 &tgt, bop->bo_seg, bop->bo_tx);

		tgt.s_rec = new_rec;
		node_make(&tgt, bop->bo_tx);
		tgt.s_rec = REC_INIT(&p_key_1, &ksize_1, &p_val_1, &vsize_1);
		node_rec(&tgt);
		COPY_RECORD(&tgt.s_rec,  &new_rec);

		node_done(&tgt, bop->bo_tx, true);
		tgt.s_node == lev->l_node ? node_seq_cnt_update(lev->l_node) :
					    node_seq_cnt_update(lev->l_alloc);
		node_fix(tgt.s_node, bop->bo_tx);
		btree_node_capture_enlist(oi, lev->l_alloc, 0);
		btree_node_capture_enlist(oi, lev->l_node, 0);

		/**
		 * TBD : This check needs to be removed when debugging is
		 * done.
		 */
		M0_ASSERT(node_expensive_invariant(lev->l_alloc));
		M0_ASSERT(node_expensive_invariant(lev->l_node));
		node_unlock(lev->l_alloc);
		node_unlock(lev->l_node);

		node_slot.s_node = lev->l_alloc;
		node_slot.s_idx = node_count(node_slot.s_node);
		node_slot.s_rec = REC_INIT(&p_key, &ksize, &p_val, &vsize);
		node_key(&node_slot);
		new_rec.r_key = node_slot.s_rec.r_key;
		newv_ptr = &(lev->l_alloc->n_addr);
	}

	/**
	 * If we reach root node and splitting is done at root handle spliting
	 * of root
	*/
	return btree_put_root_split_handle(bop, &new_rec);
}

/* get_tick for insert operation */
static int64_t btree_put_kv_tick(struct m0_sm_op *smop)
{
	struct m0_btree_op    *bop            = M0_AMB(bop, smop, bo_op);
	struct td             *tree           = bop->bo_arbor->t_desc;
	uint64_t               flags          = bop->bo_flags;
	struct m0_btree_oimpl *oi             = bop->bo_i;
	bool                   lock_acquired  = bop->bo_flags & BOF_LOCKALL;
	struct level          *lev;

	switch (bop->bo_op.o_sm.sm_state) {
	case P_INIT:
		M0_ASSERT(bop->bo_i == NULL);
		bop->bo_i = m0_alloc(sizeof *oi);
		if (bop->bo_i == NULL) {
			bop->bo_op.o_sm.sm_rc = M0_ERR(-ENOMEM);
			return P_DONE;
		}
		if ((flags & BOF_COOKIE) &&
		    cookie_is_set(&bop->bo_rec.r_key.k_cookie))
			return P_COOKIE;
		else
			return P_SETUP;
	case P_COOKIE:
		if (cookie_is_valid(tree, &bop->bo_rec.r_key.k_cookie) &&
		    !node_isoverflow(oi->i_cookie_node))
			return P_LOCK;
		else
			return P_SETUP;
	case P_LOCKALL:
		M0_ASSERT(bop->bo_flags & BOF_LOCKALL);
		return lock_op_init(&bop->bo_op, &bop->bo_i->i_nop,
				    bop->bo_arbor->t_desc, P_SETUP);
	case P_SETUP:
		oi->i_height = tree->t_height;
		level_alloc(oi, oi->i_height);
		if (oi->i_level == NULL) {
			if (lock_acquired)
				lock_op_unlock(tree);
			return fail(bop, M0_ERR(-ENOMEM));
		}
		bop->bo_i->i_key_found = false;
		oi->i_nop.no_op.o_sm.sm_rc = 0;
		/** Fall through to P_DOWN. */
	case P_DOWN:
		oi->i_used = 0;
		M0_SET0(&oi->i_capture);
		/* Load root node. */
		return node_get(&oi->i_nop, tree, &tree->t_root->n_addr,
				P_NEXTDOWN);
	case P_NEXTDOWN:
		if (oi->i_nop.no_op.o_sm.sm_rc == 0) {
			struct slot    node_slot = {};
			struct segaddr child_node_addr;

			lev = &oi->i_level[oi->i_used];
			lev->l_node = oi->i_nop.no_node;
			node_slot.s_node = oi->i_nop.no_node;

			node_lock(lev->l_node);
			lev->l_seq = oi->i_nop.no_node->n_seq;
			oi->i_nop.no_node = NULL;

			/**
			 * Node validation is required to determine that the
			 * node(lev->l_node) which is pointed by current thread
			 * is not freed by any other thread till current thread
			 * reaches NEXTDOWN phase.
			 *
			 * Node verification is required to determine that no
			 * other thread which has lock is working on the same
			 * node(lev->l_node) which is pointed by current thread.
			 */

			if (!node_isvalid(lev->l_node) || (oi->i_used > 0 &&
			    node_count_rec(lev->l_node) == 0)) {
				node_unlock(lev->l_node);
				return m0_sm_op_sub(&bop->bo_op, P_CLEANUP,
						    P_SETUP);
			}

			oi->i_key_found = node_find(&node_slot,
						    &bop->bo_rec.r_key);
			lev->l_idx = node_slot.s_idx;
			if (node_level(node_slot.s_node) > 0) {
				if (oi->i_key_found) {
					lev->l_idx++;
					node_slot.s_idx++;
				}
				node_child(&node_slot, &child_node_addr);
				if (!address_in_segment(child_node_addr)) {
					node_unlock(lev->l_node);
					node_op_fini(&oi->i_nop);
					return fail(bop, M0_ERR(-EFAULT));
				}
				oi->i_used++;

				if (oi->i_used >= oi->i_height) {
					/* If height of tree increased. */
					oi->i_used = oi->i_height - 1;
					node_unlock(lev->l_node);
					return m0_sm_op_sub(&bop->bo_op,
							    P_CLEANUP, P_SETUP);
				}
				node_unlock(lev->l_node);
				return node_get(&oi->i_nop, tree,
						&child_node_addr, P_NEXTDOWN);
			} else {
				node_unlock(lev->l_node);
				if (bop->bo_opc == M0_BO_UPDATE) {
					/**
					 * TBD : Revisit  this part, while
					 * implmenting UPDATE operation for
					 * variable value size.
					 */
					return P_LOCK;
				}

				if (oi->i_key_found)
					return P_LOCK;
				/**
				 * Initialize i_alloc_lev to level of leaf
				 * node.
				 */
				oi->i_alloc_lev = oi->i_used;
				return P_ALLOC_REQUIRE;
			}
		} else {
			node_op_fini(&oi->i_nop);
			return m0_sm_op_sub(&bop->bo_op, P_CLEANUP, P_SETUP);
		}
	case P_ALLOC_REQUIRE:{
		do {
			lev = &oi->i_level[oi->i_alloc_lev];
			node_lock(lev->l_node);
			if (!node_isvalid(lev->l_node)) {
				node_unlock(lev->l_node);
				return m0_sm_op_sub(&bop->bo_op, P_CLEANUP,
						    P_SETUP);
			}

			if (!node_isoverflow(lev->l_node)) {
				node_unlock(lev->l_node);
				break;
			}
			if (lev->l_alloc == NULL || (oi->i_alloc_lev == 0 &&
			    oi->i_extra_node == NULL)) {
				/**
				 * Depending on the level of node, shift can be
				 * updated.
				 */
				int ksize = node_keysize(lev->l_node);
				int vsize = node_valsize(lev->l_node);
				int shift = node_shift(lev->l_node);
				oi->i_nop.no_opc = NOP_ALLOC;
				node_unlock(lev->l_node);
				return node_alloc(&oi->i_nop, tree,
						  shift, lev->l_node->n_type,
						  ksize, vsize, bop->bo_tx,
						  P_ALLOC_STORE);

			}
			node_unlock(lev->l_node);
			oi->i_alloc_lev--;
		} while (oi->i_alloc_lev >= 0);
		return P_LOCK;
	}
	case P_ALLOC_STORE: {
		if (oi->i_nop.no_op.o_sm.sm_rc != 0) {
			if (lock_acquired)
				lock_op_unlock(tree);
			node_op_fini(&oi->i_nop);
			return fail(bop, oi->i_nop.no_op.o_sm.sm_rc);
		}
		lev = &oi->i_level[oi->i_alloc_lev];

		if (oi->i_alloc_lev == 0) {
			/**
			 * If we are at root node and if there is possibility of
			 * overflow at root node, allocate two nodes for l_alloc
			 * and oi->extra_node, as both nodes will be used in
			 * case of root overflow.
			 */
			if (lev->l_alloc == NULL) {
				int ksize;
				int vsize;
				int shift;
				lev->l_alloc = oi->i_nop.no_node;
				oi->i_nop.no_node = NULL;
				node_lock(lev->l_node);
				if (!node_isvalid(lev->l_node)) {
					node_unlock(lev->l_node);
					return m0_sm_op_sub(&bop->bo_op,
							    P_CLEANUP, P_SETUP);
				}
				ksize = node_keysize(lev->l_node);
				vsize = node_valsize(lev->l_node);
				shift = node_shift(lev->l_node);
				oi->i_nop.no_opc = NOP_ALLOC;
				node_unlock(lev->l_node);
				return node_alloc(&oi->i_nop, tree,
						  shift, lev->l_node->n_type,
						  ksize, vsize, bop->bo_tx,
						  P_ALLOC_STORE);

			} else if (oi->i_extra_node == NULL) {
				oi->i_extra_node = oi->i_nop.no_node;
				oi->i_nop.no_node = NULL;
				return P_LOCK;
			} else
				M0_ASSERT(0);
		}

		lev->l_alloc = oi->i_nop.no_node;
		oi->i_alloc_lev--;
		return P_ALLOC_REQUIRE;
	}
	case P_LOCK:
		if (!lock_acquired)
			return lock_op_init(&bop->bo_op, &bop->bo_i->i_nop,
					    bop->bo_arbor->t_desc, P_CHECK);
		/** Fall through if LOCK is already acquired. */
	case P_CHECK:
		if (!path_check(oi, tree, &bop->bo_rec.r_key.k_cookie)) {
			oi->i_trial++;
			if (oi->i_trial >= MAX_TRIALS) {
				M0_ASSERT_INFO((bop->bo_flags & BOF_LOCKALL) ==
					       0, "Put record failure in tree"
					       "lock mode");
				bop->bo_flags |= BOF_LOCKALL;
				lock_op_unlock(tree);
				return m0_sm_op_sub(&bop->bo_op, P_CLEANUP,
						    P_LOCKALL);
			}
			if (oi->i_height != tree->t_height) {
				/* If height has changed. */
				lock_op_unlock(tree);
				return m0_sm_op_sub(&bop->bo_op, P_CLEANUP,
						    P_SETUP);
			} else {
				/* If height is same, put back all the nodes. */
				lock_op_unlock(tree);
				level_put(oi, bop->bo_tx);
				return P_DOWN;
			}
		}
		/** Fall through if path_check is successful. */
	case P_SANITY_CHECK: {
		int  rc = 0;
		if (bop->bo_opc == M0_BO_PUT && oi->i_key_found)
			rc = M0_BSC_KEY_EXISTS;
		else if (bop->bo_opc == M0_BO_UPDATE && !oi->i_key_found)
			rc = M0_BSC_KEY_NOT_FOUND;

		if (rc) {
			lock_op_unlock(tree);
			return fail(bop, rc);
		}
		if (bop->bo_opc == M0_BO_PUT)
			return  P_MAKESPACE;
		else
			return P_ACT;
	}
	case P_MAKESPACE: {
		M0_ASSERT(!oi->i_key_found);
		lev = &oi->i_level[oi->i_used];
		struct slot slot_for_right_node = {
			.s_node = lev->l_node,
			.s_idx  = lev->l_idx,
			.s_rec  = bop->bo_rec
		};
		if (!node_isfit(&slot_for_right_node))
			return btree_put_makespace_phase(bop);

		node_lock(lev->l_node);
		node_make(&slot_for_right_node, bop->bo_tx);
		/** Fall through if there is no overflow.  **/
	}
	case P_ACT: {
		m0_bcount_t          ksize;
		void                *p_key;
		m0_bcount_t          vsize;
		void                *p_val;
		struct slot          node_slot;
		int                  rc;

		lev = &oi->i_level[oi->i_used];

		if (bop->bo_opc == M0_BO_UPDATE)
			node_lock(lev->l_node);

		node_slot.s_node = lev->l_node;
		node_slot.s_idx  = lev->l_idx;

		node_slot.s_rec = REC_INIT(&p_key, &ksize, &p_val, &vsize);
		node_rec(&node_slot);

		/**
		 * If we are at leaf node, and we have made the space
		 * for inserting a record, callback will be called.
		 * Callback will be provided with the record. It is
		 * user's responsibility to fill the value as well as
		 * key in the given record. if callback failed, we will
		 * revert back the changes made on btree. Detailed
		 * explination is provided at P_MAKESPACE stage.
		 */
		node_slot.s_rec.r_flags = M0_BSC_SUCCESS;
		rc = bop->bo_cb.c_act(&bop->bo_cb, &node_slot.s_rec);
		if (rc) {
			/* handle if callback fail i.e undo make */
			if (bop->bo_opc == M0_BO_PUT) {
				node_del(node_slot.s_node, node_slot.s_idx,
					 bop->bo_tx);
				node_done(&node_slot, bop->bo_tx, true);
				node_seq_cnt_update(lev->l_node);
				node_fix(lev->l_node, bop->bo_tx);
			}

			node_unlock(lev->l_node);
			lock_op_unlock(tree);
			return fail(bop, rc);
		}
		node_done(&node_slot, bop->bo_tx, true);
		node_seq_cnt_update(lev->l_node);
		node_fix(lev->l_node, bop->bo_tx);
		btree_node_capture_enlist(oi, lev->l_node, lev->l_idx);

		/**
		 * TBD : This check needs to be removed when debugging is
		 * done.
		 */
		M0_ASSERT(node_expensive_invariant(lev->l_node));
		node_unlock(lev->l_node);
		return P_CAPTURE;
	}
	case P_CAPTURE:
		btree_tx_nodes_capture(oi, bop->bo_tx);
		bop->bo_op.o_sm.sm_rc = M0_BSC_SUCCESS;
		lock_op_unlock(tree);
		return m0_sm_op_sub(&bop->bo_op, P_CLEANUP, P_FINI);
	case P_CLEANUP:
		level_cleanup(oi, bop->bo_tx);
		return m0_sm_op_ret(&bop->bo_op);
	case P_FINI :
		M0_ASSERT(oi);
		m0_free(oi);
		return P_DONE;
	default:
		M0_IMPOSSIBLE("Wrong state: %i", bop->bo_op.o_sm.sm_state);
	};
}
/* Insert operation section end point */

#endif
#ifndef __KERNEL__
//static struct m0_sm_group G;

static struct m0_sm_state_descr btree_states[P_NR] = {
	[P_INIT] = {
		.sd_flags   = M0_SDF_INITIAL,
		.sd_name    = "P_INIT",
		.sd_allowed = M0_BITS(P_COOKIE, P_SETUP, P_ACT, P_TIMECHECK,
				      P_TREE_GET, P_DONE),
	},
	[P_TREE_GET] = {
		.sd_flags   = 0,
		.sd_name    = "P_TREE_GET",
		.sd_allowed = M0_BITS(P_ACT),
	},
	[P_COOKIE] = {
		.sd_flags   = 0,
		.sd_name    = "P_COOKIE",
		.sd_allowed = M0_BITS(P_LOCK, P_SETUP),
	},
	[P_SETUP] = {
		.sd_flags   = 0,
		.sd_name    = "P_SETUP",
		.sd_allowed = M0_BITS(P_CLEANUP, P_NEXTDOWN),
	},
	[P_LOCKALL] = {
		.sd_flags   = 0,
		.sd_name    = "P_LOCKALL",
		.sd_allowed = M0_BITS(P_SETUP),
	},
	[P_DOWN] = {
		.sd_flags   = 0,
		.sd_name    = "P_DOWN",
		.sd_allowed = M0_BITS(P_NEXTDOWN),
	},
	[P_NEXTDOWN] = {
		.sd_flags   = 0,
		.sd_name    = "P_NEXTDOWN",
		.sd_allowed = M0_BITS(P_NEXTDOWN, P_ALLOC_REQUIRE,
				      P_STORE_CHILD, P_SIBLING, P_CLEANUP,
				      P_LOCK),
	},
	[P_SIBLING] = {
		.sd_flags   = 0,
		.sd_name    = "P_SIBLING",
		.sd_allowed = M0_BITS(P_SIBLING, P_LOCK, P_CLEANUP),
	},
	[P_ALLOC_REQUIRE] = {
		.sd_flags   = 0,
		.sd_name    = "P_ALLOC_REQUIRE",
		.sd_allowed = M0_BITS(P_ALLOC_STORE, P_LOCK, P_CLEANUP),
	},
	[P_ALLOC_STORE] = {
		.sd_flags   = 0,
		.sd_name    = "P_ALLOC_STORE",
		.sd_allowed = M0_BITS(P_ALLOC_REQUIRE, P_ALLOC_STORE, P_LOCK,
				      P_CLEANUP),
	},
	[P_STORE_CHILD] = {
		.sd_flags   = 0,
		.sd_name    = "P_STORE_CHILD",
		.sd_allowed = M0_BITS(P_CHECK, P_CLEANUP, P_DOWN, P_CAPTURE),
	},
	[P_LOCK] = {
		.sd_flags   = 0,
		.sd_name    = "P_LOCK",
		.sd_allowed = M0_BITS(P_CHECK, P_MAKESPACE, P_ACT, P_CAPTURE,
				      P_CLEANUP),
	},
	[P_CHECK] = {
		.sd_flags   = 0,
		.sd_name    = "P_CHECK",
		.sd_allowed = M0_BITS(P_CAPTURE, P_MAKESPACE, P_ACT, P_CLEANUP,
				      P_DOWN),
	},
	[P_SANITY_CHECK] = {
		.sd_flags   = 0,
		.sd_name    = "P_SANITY_CHECK",
		.sd_allowed = M0_BITS(P_MAKESPACE, P_ACT, P_CLEANUP),
	},
	[P_MAKESPACE] = {
		.sd_flags   = 0,
		.sd_name    = "P_MAKESPACE",
		.sd_allowed = M0_BITS(P_CAPTURE, P_CLEANUP),
	},
	[P_ACT] = {
		.sd_flags   = 0,
		.sd_name    = "P_ACT",
		.sd_allowed = M0_BITS(P_CAPTURE, P_CLEANUP, P_DONE),
	},
	[P_CAPTURE] = {
		.sd_flags   = 0,
		.sd_name    = "P_CAPTURE",
		.sd_allowed = M0_BITS(P_FREENODE, P_CLEANUP),
	},
	[P_FREENODE] = {
		.sd_flags   = 0,
		.sd_name    = "P_FREENODE",
		.sd_allowed = M0_BITS(P_CLEANUP, P_FINI),
	},
	[P_CLEANUP] = {
		.sd_flags   = 0,
		.sd_name    = "P_CLEANUP",
		.sd_allowed = M0_BITS(P_SETUP, P_LOCKALL, P_FINI),
	},
	[P_FINI] = {
		.sd_flags   = 0,
		.sd_name    = "P_FINI",
		.sd_allowed = M0_BITS(P_DONE),
	},
	[P_TIMECHECK] = {
		.sd_flags   = 0,
		.sd_name    = "P_TIMECHECK",
		.sd_allowed = M0_BITS(P_TIMECHECK),
	},
	[P_DONE] = {
		.sd_flags   = M0_SDF_TERMINAL,
		.sd_name    = "P_DONE",
		.sd_allowed = 0,
	},
};

static struct m0_sm_trans_descr btree_trans[] = {
	{ "open/close-init", P_INIT, P_ACT },
	{ "open/close-act", P_ACT, P_DONE },
	{ "create-init-tree_get", P_INIT, P_TREE_GET },
	{ "create-tree_get-act", P_TREE_GET, P_ACT },
	{ "close/destroy", P_INIT, P_DONE},
	{ "close-init-timecheck", P_INIT, P_TIMECHECK},
	{ "close-timecheck-repeat", P_TIMECHECK, P_TIMECHECK},
	{ "kvop-init-cookie", P_INIT, P_COOKIE },
	{ "kvop-init", P_INIT, P_SETUP },
	{ "kvop-cookie-valid", P_COOKIE, P_LOCK },
	{ "kvop-cookie-invalid", P_COOKIE, P_SETUP },
	{ "kvop-setup-failed", P_SETUP, P_CLEANUP },
	{ "kvop-setup-down-fallthrough", P_SETUP, P_NEXTDOWN },
	{ "kvop-lockall", P_LOCKALL, P_SETUP },
	{ "kvop-down", P_DOWN, P_NEXTDOWN },
	{ "kvop-nextdown-repeat", P_NEXTDOWN, P_NEXTDOWN },
	{ "put-nextdown-next", P_NEXTDOWN, P_ALLOC_REQUIRE },
	{ "del-nextdown-load", P_NEXTDOWN, P_STORE_CHILD },
	{ "get-nextdown-next", P_NEXTDOWN, P_LOCK },
	{ "iter-nextdown-sibling", P_NEXTDOWN, P_SIBLING },
	{ "kvop-nextdown-failed", P_NEXTDOWN, P_CLEANUP },
	{ "iter-sibling-repeat", P_SIBLING, P_SIBLING },
	{ "iter-sibling-next", P_SIBLING, P_LOCK },
	{ "iter-sibling-failed", P_SIBLING, P_CLEANUP },
	{ "put-alloc-load", P_ALLOC_REQUIRE, P_ALLOC_STORE },
	{ "put-alloc-next", P_ALLOC_REQUIRE, P_LOCK },
	{ "put-alloc-fail", P_ALLOC_REQUIRE, P_CLEANUP },
	{ "put-allocstore-require", P_ALLOC_STORE, P_ALLOC_REQUIRE },
	{ "put-allocstore-repeat", P_ALLOC_STORE, P_ALLOC_STORE },
	{ "put-allocstore-next", P_ALLOC_STORE, P_LOCK },
	{ "put-allocstore-fail", P_ALLOC_STORE, P_CLEANUP },
	{ "del-child-check", P_STORE_CHILD, P_CHECK },
	{ "del-child-check-ht-changed", P_STORE_CHILD, P_CLEANUP },
	{ "del-child-check-ht-same", P_STORE_CHILD, P_DOWN },
	{ "del-child-check-ft", P_STORE_CHILD, P_CAPTURE },
	{ "kvop-lock", P_LOCK, P_CHECK },
	{ "kvop-lock-check-ht-changed", P_LOCK, P_CLEANUP },
	{ "put-lock-ft-capture", P_LOCK, P_CAPTURE },
	{ "put-lock-ft-makespace", P_LOCK, P_MAKESPACE },
	{ "put-lock-ft-act", P_LOCK, P_ACT },
	{ "kvop-check-height-changed", P_CHECK, P_CLEANUP },
	{ "kvop-check-height-same", P_CHECK, P_DOWN },
	{ "put-check-ft-capture", P_CHECK, P_CAPTURE },
	{ "put-check-ft-makespace", P_LOCK, P_MAKESPACE },
	{ "put-check-ft-act", P_LOCK, P_ACT },
	{ "put-sanity-makespace", P_SANITY_CHECK, P_MAKESPACE },
	{ "put-sanity-act", P_SANITY_CHECK, P_ACT },
	{ "put-sanity-cleanup", P_SANITY_CHECK, P_CLEANUP},
	{ "put-makespace-capture", P_MAKESPACE, P_CAPTURE },
	{ "put-makespace-cleanup", P_MAKESPACE, P_CLEANUP },
	{ "kvop-act", P_ACT, P_CLEANUP },
	{ "put-del-act", P_ACT, P_CAPTURE },
	{ "put-capture", P_CAPTURE, P_CLEANUP},
	{ "del-capture-freenode", P_CAPTURE, P_FREENODE},
	{ "del-freenode-cleanup", P_FREENODE, P_CLEANUP },
	{ "del-freenode-fini", P_FREENODE, P_FINI},
	{ "kvop-cleanup-setup", P_CLEANUP, P_SETUP },
	{ "kvop-lockall", P_CLEANUP, P_LOCKALL },
	{ "kvop-done", P_CLEANUP, P_FINI },
	{ "kvop-fini", P_FINI, P_DONE },
};

static struct m0_sm_conf btree_conf = {
	.scf_name      = "btree-conf",
	.scf_nr_states = ARRAY_SIZE(btree_states),
	.scf_state     = btree_states,
	.scf_trans_nr  = ARRAY_SIZE(btree_trans),
	.scf_trans     = btree_trans
};

#endif

#ifndef __KERNEL__
/**
 * calc_shift is used to calculate the shift for the given number of bytes.
 * Shift is the exponent of nearest power-of-2 value greater than or equal to
 * number of bytes.
 *
 * @param value represents the number of bytes
 * @return int  returns the shift value.
 */

int calc_shift(int value)
{
	unsigned int sample = (unsigned int) value;
	unsigned int pow    = 0;

	while (sample > 0)
	{
		sample >>=1;
		pow += 1;
	}

	return pow - 1;
}

/**
 * btree_create_tree_tick function is the main function used to create btree.
 * It traverses through multiple states to perform its operation.
 *
 * @param smop     represents the state machine operation
 * @return int64_t returns the next state to be executed.
 */
int64_t btree_create_tree_tick(struct m0_sm_op *smop)
{
	struct m0_btree_op    *bop    = M0_AMB(bop, smop, bo_op);
	struct m0_btree_oimpl *oi     = bop->bo_i;
	struct m0_btree_idata *data   = &bop->b_data;
	int                    k_size = data->bt->ksize == -1 ? MAX_KEY_SIZE :
					data->bt->ksize;
	int                    v_size = data->bt->vsize == -1 ? MAX_VAL_SIZE :
					data->bt->vsize;

	switch (bop->bo_op.o_sm.sm_state) {
	case P_INIT:
		/**
		 * This following check has been added to enforce the
		 * requirement that nodes have aligned addresses.
		 * However, in future, this check can be removed if
		 * such a requirement is invalidated.
		 */
		if (!addr_is_aligned(data->addr))
			return M0_ERR(-EFAULT);

		oi = m0_alloc(sizeof *bop->bo_i);
		if (oi == NULL)
			return M0_ERR(-ENOMEM);
		bop->bo_i = oi;
		bop->bo_arbor = m0_alloc(sizeof *bop->bo_arbor);
		if (bop->bo_arbor == NULL) {
			m0_free(oi);
			return M0_ERR(-ENOMEM);
		}

		oi->i_nop.no_addr = segaddr_build(data->addr, calc_shift(data->
						  num_bytes));
		return node_init(&oi->i_nop.no_addr, k_size, v_size,
				 data->nt, bop->bo_seg, bop->bo_tx, P_TREE_GET);

	case P_TREE_GET:
		return tree_get(&oi->i_nop, &oi->i_nop.no_addr, P_ACT);

	case P_ACT:
		M0_ASSERT(oi->i_nop.no_op.o_sm.sm_rc == 0);
		oi->i_nop.no_node->n_type = data->nt;
		oi->i_nop.no_tree->t_type = data->bt;
		oi->i_nop.no_tree->t_seg  = bop->bo_seg;

		bop->bo_arbor->t_desc           = oi->i_nop.no_tree;
		bop->bo_arbor->t_type           = data->bt;
		bop->bo_arbor->t_height         = node_level(oi->i_nop.no_node) + 1;

		m0_rwlock_write_lock(&bop->bo_arbor->t_desc->t_lock);
		bop->bo_arbor->t_desc->t_height = bop->bo_arbor->t_height;
		m0_rwlock_write_unlock(&bop->bo_arbor->t_desc->t_lock);

		m0_free(oi);
		bop->bo_i = NULL;
		return P_DONE;

	default:
		M0_IMPOSSIBLE("Wrong state: %i", bop->bo_op.o_sm.sm_state);
	}
}

/**
 * btree_destroy_tree_tick function is the main function used to destroy btree.
 *
 * @param smop     represents the state machine operation
 * @return int64_t returns the next state to be executed.
 */
int64_t btree_destroy_tree_tick(struct m0_sm_op *smop)
{
	struct m0_btree_op *bop  = M0_AMB(bop, smop, bo_op);
	struct td          *tree = bop->bo_arbor->t_desc;

	M0_PRE(bop->bo_op.o_sm.sm_state == P_INIT);

	if (tree == NULL)
		return M0_ERR(-EINVAL);

	m0_rwlock_write_lock(&tree->t_lock);
	if (tree->t_ref != 1) {
		m0_rwlock_write_unlock(&tree->t_lock);
		return M0_ERR(-EPERM);
	}

	M0_PRE(node_invariant(tree->t_root));
	M0_PRE(node_count(tree->t_root) == 0);
	m0_rwlock_write_unlock(&tree->t_lock);
	node_fini(tree->t_root, bop->bo_tx);
	node_put(tree->t_root->n_op, tree->t_root, bop->bo_tx);
	tree_put(tree);

	/**
	 * ToDo: We need to capture the changes occuring in the
	 * root node after tree_descriptor has been freed using
	 * m0_be_tx_capture().
	 * Only those fields that have changed need to be
	 * updated.
	 */
	m0_free0(&bop->bo_arbor);
	return P_DONE;
}

/**
 * btree_open_tree_tick function is used to traverse through different states to
 * facilitate the working of m0_btree_open().
 *
 * @param smop     represents the state machine operation
 * @return int64_t returns the next state to be executed.
 */
int64_t btree_open_tree_tick(struct m0_sm_op *smop)
{
	struct m0_btree_op    *bop  = M0_AMB(bop, smop, bo_op);
	struct m0_btree_oimpl *oi   = bop->bo_i;

	switch (bop->bo_op.o_sm.sm_state) {
	case P_INIT:

		/**
		 * This following check has been added to enforce the
		 * requirement that nodes have aligned addresses.
		 * However, in future, this check can be removed if
		 * such a requirement is invalidated.
		 */
		if (!addr_is_aligned(bop->b_data.addr))
			return M0_ERR(-EFAULT);

		oi = m0_alloc(sizeof *bop->bo_i);
		if (oi == NULL)
			return M0_ERR(-ENOMEM);
		bop->bo_i = oi;
		oi->i_nop.no_addr = segaddr_build(bop->b_data.addr,
						  calc_shift(bop->b_data.
							     num_bytes));

		return tree_get(&oi->i_nop, &oi->i_nop.no_addr, P_ACT);

	case P_ACT:
		M0_ASSERT(oi->i_nop.no_op.o_sm.sm_rc == 0);

		if (!oi->i_nop.no_tree->t_type)
			oi->i_nop.no_tree->t_type = bop->b_data.bt;

		bop->bo_arbor->t_type   = oi->i_nop.no_tree->t_type;
		bop->bo_arbor->t_desc   = oi->i_nop.no_tree;
		bop->bo_arbor->t_height = oi->i_nop.no_tree->t_height;

		m0_free(oi);
		return P_DONE;

	default:
		M0_IMPOSSIBLE("Wrong state: %i", bop->bo_op.o_sm.sm_state);
	}
}

/**
 * btree_close_tree_tick function is used to traverse through different states
 * to facilitate the working of m0_btree_close().
 *
 * @param smop     represents the state machine operation
 * @return int64_t returns the next state to be executed.
 */
int64_t btree_close_tree_tick(struct m0_sm_op *smop)
{
	struct m0_btree_op *bop  = M0_AMB(bop, smop, bo_op);
	struct td          *tree = bop->bo_arbor->t_desc;
	struct nd          *node;

	switch (bop->bo_op.o_sm.sm_state) {
	case P_INIT:
		if (tree == NULL)
			return M0_ERR(-EINVAL);

		M0_PRE(tree->t_ref != 0);

		m0_rwlock_write_lock(&tree->t_lock);
		if (tree->t_ref > 1) {
			m0_rwlock_write_unlock(&tree->t_lock);
			node_put(tree->t_root->n_op, tree->t_root, bop->bo_tx);
			tree_put(tree);
			return P_DONE;
		}
		tree->t_starttime = m0_time_now();
		m0_rwlock_write_unlock(&tree->t_lock);

		/** put tree's root node. */
		node_put(tree->t_root->n_op, tree->t_root, bop->bo_tx);
		/** Fallthrough to P_TIMECHECK */
	case P_TIMECHECK:
		/**
		 * This code is meant for debugging. In future, this case needs
		 * to be handled in a better way.
		 */
		m0_rwlock_write_lock(&list_lock);
		m0_tl_for(ndlist, &btree_active_nds, node) {
			if (node->n_tree == tree && node->n_ref > 0) {
				if (m0_time_seconds(m0_time_now() -
						    tree->t_starttime) > 5) {
					M0_LOG(M0_ERROR, "tree close timeout");
					M0_ASSERT(false);
				}
				m0_rwlock_write_unlock(&list_lock);
				return P_TIMECHECK;
			}
		} m0_tl_endfor;
		m0_rwlock_write_unlock(&list_lock);
		/** Fallthrough to P_ACT */
	case P_ACT:
		tree->t_starttime = 0;
		tree_put(tree);
		bop->bo_arbor->t_desc = NULL;
		return P_DONE;
	default:
		M0_IMPOSSIBLE("Wrong state: %i", bop->bo_op.o_sm.sm_state);
	}
}

/* Based on the flag get the next/previous sibling index. */
static int sibling_index_get(int index, uint64_t flags, bool key_exists)
{
	if (flags & BOF_NEXT)
		return key_exists ? ++index : index;
	return --index;
}

/* Checks if the index is in the range of valid key range for node. */
static bool index_is_valid(struct level *lev)
{
	return (lev->l_idx >= 0) && (lev->l_idx < node_count(lev->l_node));
}

/**
 *  Search from the leaf + 1 level till the root level and find a node
 *  which has valid sibling. Once found, get the leftmost leaf record from the
 *  sibling subtree.
 */
int  btree_sibling_first_key(struct m0_btree_oimpl *oi, struct td *tree,
			     struct slot *s)
{
	int             i;
	struct level   *lev;
	struct segaddr  child;

	for (i = oi->i_used - 1; i >= 0; i--) {
		lev = &oi->i_level[i];
		if (lev->l_idx < node_count(lev->l_node)) {
			s->s_node = oi->i_nop.no_node = lev->l_node;
			s->s_idx = lev->l_idx + 1;
			while (i != oi->i_used) {
				node_child(s, &child);
				if (!address_in_segment(child))
					return M0_ERR(-EFAULT);
				i++;
				node_get(&oi->i_nop, tree, &child, P_CLEANUP);
				s->s_idx = 0;
				s->s_node = oi->i_nop.no_node;
				oi->i_level[i].l_sibling = oi->i_nop.no_node;
			}
			node_rec(s);
			return 0;
		}
	}
	s->s_rec.r_flags = M0_BSC_KEY_NOT_FOUND;
	return 0;

}

/**
 * State machine for fetching exact key / slant key / min key or max key from
 * the tree.
 */
static int64_t btree_get_kv_tick(struct m0_sm_op *smop)
{
	struct m0_btree_op    *bop            = M0_AMB(bop, smop, bo_op);
	struct td             *tree           = bop->bo_arbor->t_desc;
	struct m0_btree_oimpl *oi             = bop->bo_i;
	bool                   lock_acquired  = bop->bo_flags & BOF_LOCKALL;
	struct level          *lev;

	switch (bop->bo_op.o_sm.sm_state) {
	case P_INIT:
		M0_ASSERT(bop->bo_opc == M0_BO_GET ||
			  bop->bo_opc == M0_BO_MINKEY ||
			  bop->bo_opc == M0_BO_MAXKEY);
		M0_ASSERT(bop->bo_i == NULL);
		bop->bo_i = m0_alloc(sizeof *oi);
		if (bop->bo_i == NULL) {
			bop->bo_op.o_sm.sm_rc = M0_ERR(-ENOMEM);
			return P_DONE;
		}
		if ((bop->bo_flags & BOF_COOKIE) &&
		    cookie_is_set(&bop->bo_rec.r_key.k_cookie))
			return P_COOKIE;
		else
			return P_SETUP;
	case P_COOKIE:
		if (cookie_is_valid(tree, &bop->bo_rec.r_key.k_cookie))
			return P_LOCK;
		else
			return P_SETUP;
	case P_LOCKALL:
		M0_ASSERT(bop->bo_flags & BOF_LOCKALL);
		return lock_op_init(&bop->bo_op, &bop->bo_i->i_nop,
				    bop->bo_arbor->t_desc, P_SETUP);
	case P_SETUP:
		oi->i_height = tree->t_height;
		level_alloc(oi, oi->i_height);
		if (oi->i_level == NULL) {
			if (lock_acquired)
				lock_op_unlock(tree);
			return fail(bop, M0_ERR(-ENOMEM));
		}
		oi->i_nop.no_op.o_sm.sm_rc = 0;
		/** Fall through to P_DOWN. */
	case P_DOWN:
		oi->i_used = 0;
		return node_get(&oi->i_nop, tree, &tree->t_root->n_addr,
				P_NEXTDOWN);
	case P_NEXTDOWN:
		if (oi->i_nop.no_op.o_sm.sm_rc == 0) {
			struct slot    s = {};
			struct segaddr child;

			lev = &oi->i_level[oi->i_used];
			lev->l_node = oi->i_nop.no_node;
			s.s_node = oi->i_nop.no_node;

			node_lock(lev->l_node);
			lev->l_seq = lev->l_node->n_seq;

			/**
			 * Node validation is required to determine that the
			 * node(lev->l_node) which is pointed by current thread
			 * is not freed by any other thread till current thread
			 * reaches NEXTDOWN phase.
			 *
			 * Node verification is required to determine that no
			 * other thread which has lock is working on the same
			 * node(lev->l_node) which is pointed by current thread.
			 */
			if (!node_isvalid(lev->l_node) || (oi->i_used > 0 &&
			    node_count_rec(lev->l_node) == 0)) {
				node_unlock(lev->l_node);
				return m0_sm_op_sub(&bop->bo_op, P_CLEANUP,
						    P_SETUP);
			}

			if (bop->bo_opc == M0_BO_GET) {
				oi->i_key_found = node_find(&s,
							    &bop->bo_rec.r_key);
				lev->l_idx = s.s_idx;
			}

			if (node_level(s.s_node) > 0) {
				if (bop->bo_opc == M0_BO_GET) {
					if (oi->i_key_found) {
						s.s_idx++;
						lev->l_idx++;
					}
				} else
					s.s_idx = bop->bo_opc == M0_BO_MINKEY ?
						  0 : node_count(s.s_node);

				node_child(&s, &child);
				if (!address_in_segment(child)) {
					node_unlock(lev->l_node);
					node_op_fini(&oi->i_nop);
					return fail(bop, M0_ERR(-EFAULT));
				}
				oi->i_used++;
				if (oi->i_used >= oi->i_height) {
					/* If height of tree increased. */
					oi->i_used = oi->i_height - 1;
					node_unlock(lev->l_node);
					return m0_sm_op_sub(&bop->bo_op,
							    P_CLEANUP, P_SETUP);
				}
				node_unlock(lev->l_node);
				return node_get(&oi->i_nop, tree, &child,
						P_NEXTDOWN);
			} else {
				node_unlock(lev->l_node);
				return P_LOCK;
			}
		} else {
			node_op_fini(&oi->i_nop);
			return m0_sm_op_sub(&bop->bo_op, P_CLEANUP, P_SETUP);
		}
	case P_LOCK:
		if (!lock_acquired)
			return lock_op_init(&bop->bo_op, &bop->bo_i->i_nop,
					    bop->bo_arbor->t_desc, P_CHECK);
		/** Fall through if LOCK is already acquired. */
	case P_CHECK:
		if (!path_check(oi, tree, &bop->bo_rec.r_key.k_cookie)) {
			oi->i_trial++;
			if (oi->i_trial >= MAX_TRIALS) {
				M0_ASSERT_INFO((bop->bo_flags & BOF_LOCKALL) ==
					       0, "Get record failure in tree"
					       "lock mode");
				bop->bo_flags |= BOF_LOCKALL;
				lock_op_unlock(tree);
				return m0_sm_op_sub(&bop->bo_op, P_CLEANUP,
						    P_LOCKALL);
			}
			if (oi->i_height != tree->t_height) {
				/* If height has changed. */
				lock_op_unlock(tree);
				return m0_sm_op_sub(&bop->bo_op, P_CLEANUP,
				                    P_SETUP);
			} else {
				/* If height is same, put back all the nodes. */
				lock_op_unlock(tree);
				level_put(oi, bop->bo_tx);
				return P_DOWN;
			}
		}
		/** Fall through if path_check is successful. */
	case P_ACT: {
		m0_bcount_t  ksize;
		m0_bcount_t  vsize;
		void        *pkey;
		void        *pval;
		struct slot  s = {};
		int          rc;
		int          count;

		lev = &oi->i_level[oi->i_used];

		s.s_node        = lev->l_node;
		s.s_idx         = lev->l_idx;
		s.s_rec	        = REC_INIT(&pkey, &ksize, &pval, &vsize);
		s.s_rec.r_flags = M0_BSC_SUCCESS;

		count = node_count(s.s_node);
		/**
		 *  There are two cases based on the flag set by user for GET OP
		 *  1. Flag BRF_EQUAL: If requested key found return record else
		 *  return key not exist.
		 *  2. Flag BRF_SLANT: If the key index(found during P_NEXTDOWN)
		 *  is less than total number of keys, return the record at key
		 *  index. Else loop through the levels to find valid sibling.
		 *  If valid sibling found, return first key of the sibling
		 *  subtree else return key not exist.
		 */
		if (bop->bo_opc == M0_BO_GET) {
			if (bop->bo_flags & BOF_EQUAL) {
				if (oi->i_key_found)
					node_rec(&s);
				else
					s.s_rec.r_flags = M0_BSC_KEY_NOT_FOUND;
			} else {
				if (lev->l_idx < count)
					node_rec(&s);
				else {
					rc = btree_sibling_first_key(oi, tree,
								     &s);
					if (rc != 0) {
						node_op_fini(&oi->i_nop);
						return fail(bop, rc);
					}
				}
			}
		} else {
			/** MIN/MAX key operation. */
			if (count > 0) {
				s.s_idx = bop->bo_opc == M0_BO_MINKEY ? 0 :
					  count - 1;
				node_rec(&s);
			} else
				/** Only root node is present and is empty. */
				s.s_rec.r_flags = M0_BSC_KEY_NOT_FOUND;
		}

		bop->bo_cb.c_act(&bop->bo_cb, &s.s_rec);

		lock_op_unlock(tree);
		return m0_sm_op_sub(&bop->bo_op, P_CLEANUP, P_FINI);
	}
	case P_CLEANUP:
		level_cleanup(oi, bop->bo_tx);
		return m0_sm_op_ret(&bop->bo_op);
	case P_FINI :
		M0_ASSERT(oi);
		m0_free(oi);
		return P_DONE;
	default:
		M0_IMPOSSIBLE("Wrong state: %i", bop->bo_op.o_sm.sm_state);
	};
}

/** Iterator state machine. */
int64_t btree_iter_kv_tick(struct m0_sm_op *smop)
{
	struct m0_btree_op    *bop            = M0_AMB(bop, smop, bo_op);
	struct td             *tree           = bop->bo_arbor->t_desc;
	struct m0_btree_oimpl *oi             = bop->bo_i;
	bool                   lock_acquired  = bop->bo_flags & BOF_LOCKALL;
	struct level          *lev;

	switch (bop->bo_op.o_sm.sm_state) {
	case P_INIT:
		M0_ASSERT(bop->bo_i == NULL);
		bop->bo_i = m0_alloc(sizeof *oi);
		if (bop->bo_i == NULL) {
			bop->bo_op.o_sm.sm_rc = M0_ERR(-ENOMEM);
			return P_DONE;
		}
		if ((bop->bo_flags & BOF_COOKIE) &&
		    cookie_is_set(&bop->bo_rec.r_key.k_cookie))
			return P_COOKIE;
		else
			return P_SETUP;
	case P_COOKIE:
		if (cookie_is_valid(tree, &bop->bo_rec.r_key.k_cookie))
			return P_LOCK;
		else
			return P_SETUP;
	case P_LOCKALL:
		M0_ASSERT(bop->bo_flags & BOF_LOCKALL);
		return lock_op_init(&bop->bo_op, &bop->bo_i->i_nop,
				    bop->bo_arbor->t_desc, P_SETUP);
	case P_SETUP:
		oi->i_height = tree->t_height;
		level_alloc(oi, oi->i_height);
		if (oi->i_level == NULL) {
			if (lock_acquired)
				lock_op_unlock(tree);
			return fail(bop, M0_ERR(-ENOMEM));
		}
		oi->i_nop.no_op.o_sm.sm_rc = 0;
		/** Fall through to P_DOWN. */
	case P_DOWN:
		oi->i_used  = 0;
		oi->i_pivot = -1;
		return node_get(&oi->i_nop, tree, &tree->t_root->n_addr,
				P_NEXTDOWN);
	case P_NEXTDOWN:
		if (oi->i_nop.no_op.o_sm.sm_rc == 0) {
			struct slot    s = {};
			struct segaddr child;

			lev = &oi->i_level[oi->i_used];
			lev->l_node = oi->i_nop.no_node;
			s.s_node = oi->i_nop.no_node;

			node_lock(lev->l_node);
			lev->l_seq = lev->l_node->n_seq;

			/**
			 * Node validation is required to determine that the
			 * node(lev->l_node) which is pointed by current thread
			 * is not freed by any other thread till current thread
			 * reaches NEXTDOWN phase.
			 *
			 * Node verification is required to determine that no
			 * other thread which has lock is working on the same
			 * node(lev->l_node) which is pointed by current thread.
			 */
			if (!node_isvalid(lev->l_node) || (oi->i_used > 0 &&
			    node_count_rec(lev->l_node) == 0)) {
				node_unlock(lev->l_node);
				return m0_sm_op_sub(&bop->bo_op, P_CLEANUP,
						    P_SETUP);
			}

			oi->i_key_found = node_find(&s, &bop->bo_rec.r_key);
			lev->l_idx = s.s_idx;

			if (node_level(s.s_node) > 0) {
				if (oi->i_key_found) {
					s.s_idx++;
					lev->l_idx++;
				}
				/**
				 * Check if the node has valid left or right
				 * index based on previous/next flag. If valid
				 * left/right index found, mark this level as
				 * pivot level.The pivot level is the level
				 * closest to leaf level having valid sibling
				 * index.
				 */
				if (((bop->bo_flags & BOF_NEXT) &&
				    (lev->l_idx < node_count(lev->l_node))) ||
				    ((bop->bo_flags & BOF_PREV) &&
				    (lev->l_idx > 0)))
					oi->i_pivot = oi->i_used;

				node_child(&s, &child);
				if (!address_in_segment(child)) {
					node_unlock(lev->l_node);
					node_op_fini(&oi->i_nop);
					return fail(bop, M0_ERR(-EFAULT));
				}
				oi->i_used++;
				if (oi->i_used >= oi->i_height) {
					/* If height of tree increased. */
					oi->i_used = oi->i_height - 1;
					node_unlock(lev->l_node);
					return m0_sm_op_sub(&bop->bo_op,
							    P_CLEANUP, P_SETUP);
				}
				node_unlock(lev->l_node);
				return node_get(&oi->i_nop, tree, &child,
						P_NEXTDOWN);
			} else	{
				/* Get sibling index based on PREV/NEXT flag. */
				lev->l_idx = sibling_index_get(s.s_idx,
							       bop->bo_flags,
							       oi->i_key_found);
				/**
				 * In the following cases jump to LOCK state:
				 * 1. the found key idx is within the valid
				 *    index range of the node.
				 * 2.i_pivot is equal to -1. It means, tree
				 *   traversal reached at the leaf level without
				 *   finding any valid sibling in the non-leaf
				 *   levels.
				 *   This indicates that the search key is the
				 *   boundary key (rightmost for NEXT flag and
				 *   leftmost for PREV flag).
				 */
				if (index_is_valid(lev) || oi->i_pivot == -1) {
					node_unlock(lev->l_node);
					return P_LOCK;
				}
				node_unlock(lev->l_node);
				/**
				 * We are here, it means we want to load
				 * sibling node of the leaf node.
				 * Start traversing the sibling node path
				 * starting from the pivot level. If the node
				 * at pivot level is still valid, load sibling
				 * idx's child node else clean up and restart
				 * state machine.
				 */
				lev = &oi->i_level[oi->i_pivot];
				node_lock(lev->l_node);
				if (!node_isvalid(lev->l_node) ||
				    (oi->i_pivot > 0 &&
				     node_count_rec(lev->l_node) == 0)) {
					node_unlock(lev->l_node);
					node_op_fini(&oi->i_nop);
					return m0_sm_op_sub(&bop->bo_op,
							    P_CLEANUP, P_SETUP);
				}
				if (lev->l_seq != lev->l_node->n_seq) {
					node_unlock(lev->l_node);
					return m0_sm_op_sub(&bop->bo_op,
							    P_CLEANUP, P_SETUP);
				}

				s.s_node = lev->l_node;
				s.s_idx = sibling_index_get(lev->l_idx,
							    bop->bo_flags,
							    true);
				/**
				 * We have already checked node and its sequence
				 * number validity. Do we still need to check
				 * sibling index validity?
				 */

				node_child(&s, &child);
				if (!address_in_segment(child)) {
					node_unlock(lev->l_node);
					node_op_fini(&oi->i_nop);
					return fail(bop, M0_ERR(-EFAULT));
				}
				oi->i_pivot++;
				node_unlock(lev->l_node);
				return node_get(&oi->i_nop, tree, &child,
						P_SIBLING);
			}
		} else {
			node_op_fini(&oi->i_nop);
			return m0_sm_op_sub(&bop->bo_op, P_CLEANUP, P_SETUP);
		}
	case P_SIBLING:
		if (oi->i_nop.no_op.o_sm.sm_rc == 0) {
			struct slot    s = {};
			struct segaddr child;

			lev = &oi->i_level[oi->i_pivot];
			lev->l_sibling = oi->i_nop.no_node;
			s.s_node = oi->i_nop.no_node;
			node_lock(lev->l_sibling);
			lev->l_sib_seq = lev->l_sibling->n_seq;

			/**
			 * Node validation is required to determine that the
			 * node(lev->l_node) which is pointed by current thread
			 * is not freed by any other thread till current thread
			 * reaches NEXTDOWN phase.
			 *
			 * Node verification is required to determine that no
			 * other thread which has lock is working on the same
			 * node(lev->l_node) which is pointed by current thread.
			 */
			if (!node_isvalid(lev->l_sibling) || (oi->i_pivot > 0 &&
			    node_count_rec(lev->l_sibling) == 0)) {
				node_unlock(lev->l_sibling);
				return m0_sm_op_sub(&bop->bo_op, P_CLEANUP,
						    P_SETUP);
			}

			if (node_level(s.s_node) > 0) {
				s.s_idx = (bop->bo_flags & BOF_NEXT) ? 0 :
					  node_count(s.s_node);
				node_child(&s, &child);
				if (!address_in_segment(child)) {
					node_unlock(lev->l_sibling);
					node_op_fini(&oi->i_nop);
					return fail(bop, M0_ERR(-EFAULT));
				}
				oi->i_pivot++;
				if (oi->i_pivot >= oi->i_height) {
					/* If height of tree increased. */
					node_unlock(lev->l_sibling);
					return m0_sm_op_sub(&bop->bo_op,
							    P_CLEANUP, P_SETUP);
				}
				node_unlock(lev->l_sibling);
				return node_get(&oi->i_nop, tree, &child,
						P_SIBLING);
			} else {
				node_unlock(lev->l_sibling);
				return P_LOCK;
			}
		} else {
			node_op_fini(&oi->i_nop);
			return m0_sm_op_sub(&bop->bo_op, P_CLEANUP, P_SETUP);
		}
	case P_LOCK:
		if (!lock_acquired)
			return lock_op_init(&bop->bo_op, &bop->bo_i->i_nop,
					    bop->bo_arbor->t_desc, P_CHECK);
		/** Fall through if LOCK is already acquired. */
	case P_CHECK:
		if (!path_check(oi, tree, &bop->bo_rec.r_key.k_cookie) ||
		    !sibling_node_check(oi)) {
			oi->i_trial++;
			if (oi->i_trial >= MAX_TRIALS) {
				M0_ASSERT_INFO((bop->bo_flags & BOF_LOCKALL) ==
					       0, "Iterator failure in tree"
					       "lock mode");
				bop->bo_flags |= BOF_LOCKALL;
				lock_op_unlock(tree);
				return m0_sm_op_sub(&bop->bo_op, P_CLEANUP,
						    P_LOCKALL);
			}
			if (oi->i_height != tree->t_height) {
				lock_op_unlock(tree);
				return m0_sm_op_sub(&bop->bo_op, P_CLEANUP,
				                    P_SETUP);
			} else {
				/* If height is same, put back all the nodes. */
				lock_op_unlock(tree);
				level_put(oi, bop->bo_tx);
				return P_DOWN;
			}
		}
		/**
		 * Fall through if path_check and sibling_node_check are
		 * successful.
		 */
	case P_ACT: {
		m0_bcount_t		 ksize;
		m0_bcount_t		 vsize;
		void			*pkey;
		void			*pval;
		struct slot		 s = {};

		lev = &oi->i_level[oi->i_used];

		s.s_rec.r_key.k_data = M0_BUFVEC_INIT_BUF(&pkey, &ksize);
		s.s_rec.r_val	     = M0_BUFVEC_INIT_BUF(&pval, &vsize);
		s.s_rec.r_flags      = M0_BSC_SUCCESS;

		/* Return record if idx fit in the node. */
		if (index_is_valid(lev)) {
			s.s_node = lev->l_node;
			s.s_idx  = lev->l_idx;
			node_rec(&s);
		} else if (oi->i_pivot == -1)
			/* Handle rightmost/leftmost key case. */
			s.s_rec.r_flags = M0_BSC_KEY_BTREE_BOUNDARY;
		else {
			/* Return sibling record based on flag. */
			s.s_node = lev->l_sibling;
			s.s_idx = (bop->bo_flags & BOF_NEXT) ? 0 :
				  node_count(s.s_node) - 1;
			node_rec(&s);
		}
		bop->bo_cb.c_act(&bop->bo_cb, &s.s_rec);
		lock_op_unlock(tree);
		return m0_sm_op_sub(&bop->bo_op, P_CLEANUP, P_FINI);
	}
	case P_CLEANUP:
		level_cleanup(oi, bop->bo_tx);
		return m0_sm_op_ret(&bop->bo_op);
	case P_FINI:
		M0_ASSERT(oi);
		m0_free(oi);
		return P_DONE;
	default:
		M0_IMPOSSIBLE("Wrong state: %i", bop->bo_op.o_sm.sm_state);
	};
}

/* Delete Operation */

/**
 * This function will get called if there is an underflow at current node after
 * deletion of the record. Currently, underflow condition is defined based on
 * record count. If record count is 0, there will be underflow. To resolve
 * underflow,
 * 1) delete the node from parent.
 * 2) check if there is an underflow at parent due to deletion of it's child.
 * 3) if there is an underflow,
 *        if, we have reached root, handle underflow at root.
 *        else, repeat steps from step 1.
 *    else, return next phase which needs to be executed.
 *
 * @param bop will provide all required information about btree operation.
 * @return int64_t return state which needs to get executed next.
 */
static int64_t btree_del_resolve_underflow(struct m0_btree_op *bop)
{
	struct td              *tree        = bop->bo_arbor->t_desc;
	struct m0_btree_oimpl  *oi          = bop->bo_i;
	int                     used_count  = oi->i_used;
	struct level           *lev         = &oi->i_level[used_count];
	bool                    flag        = false;
	struct slot             node_slot;
	int                     curr_root_level;
	struct slot             root_slot;
	struct nd              *root_child;
	bool                    node_underflow;

	do {
		used_count--;
		lev = &oi->i_level[used_count];
		node_lock(lev->l_node);

		node_del(lev->l_node, lev->l_idx, bop->bo_tx);
		node_slot.s_node = lev->l_node;
		node_slot.s_idx  = lev->l_idx;
		node_done(&node_slot, bop->bo_tx, true);

		/**
		 * once underflow is resolved at child by deleteing child node
		 * from parent, determine next step:
		 * If we reach the root node,
		 *      if record count > 1, go to P_FREENODE.
		 *      if record count = 0, set level = 0, height=1, go to
		 *         P_FREENODE.
		 *       else record count == 1, break the loop handle root case
		 *           condition.
		 * else if record count at parent is greater than 0, go to
		 *         P_FREENODE.
		 *      else, resolve the underflow at parent reapeat the steps
		 *            in loop.
		 */
		if (used_count == 0) {
			if (node_count_rec(lev->l_node) > 1)
				flag = true;
			else if (node_count_rec(lev->l_node) == 0) {
				node_set_level(lev->l_node, 0, bop->bo_tx);
				tree->t_height = 1;
				bop->bo_arbor->t_height = tree->t_height;
				/* Capture this change in transaction */
				flag = true;
			} else
				break;
		}
		node_seq_cnt_update(lev->l_node);
		node_fix(node_slot.s_node, bop->bo_tx);
		btree_node_capture_enlist(oi, lev->l_node, lev->l_idx);
		/**
		 * TBD : This check needs to be removed when debugging is
		 * done.
		 */
		M0_ASSERT(node_expensive_invariant(lev->l_node));

		node_underflow = node_isunderflow(lev->l_node, false);
		if (used_count != 0 && node_underflow) {
			node_fini(lev->l_node, bop->bo_tx);
			lev->l_freenode = true;
		}

		node_unlock(lev->l_node);

		/* check if underflow after deletion */
		if (flag || !node_underflow)
			return P_CAPTURE;

	} while (1);

	/**
	 * handle root cases :
	 * If we have reached the root and root contains only one child pointer
	 * due to the deletion of l_node from the level below the root,
	 * 1) get the root's only child
	 * 2) delete the existing record from root
	 * 3) copy the record from its only child to root
	 * 4) free that child node
	 */

	curr_root_level  = node_level(lev->l_node);
	root_slot.s_node = lev->l_node;
	root_slot.s_idx  = 0;
	node_del(lev->l_node, 0, bop->bo_tx);
	node_done(&root_slot, bop->bo_tx, true);

	/* l_sib is node below root which is root's only child */
	root_child = oi->i_level[1].l_sibling;
	node_lock(root_child);

	node_set_level(lev->l_node, curr_root_level - 1, bop->bo_tx);
	tree->t_height--;
	bop->bo_arbor->t_height = tree->t_height;
	/* Capture this change in transaction */

	node_move(root_child, lev->l_node, D_RIGHT, NR_MAX, bop->bo_tx);
	M0_ASSERT(node_count_rec(root_child) == 0);
	btree_node_capture_enlist(oi, lev->l_node, 0);
	btree_node_capture_enlist(oi, root_child, 0);
	oi->i_root_child_free = true;

	/* TBD : This check needs to be removed when debugging is done. */
	M0_ASSERT(node_expensive_invariant(lev->l_node));
	node_unlock(lev->l_node);
	node_unlock(root_child);

	return P_CAPTURE;
}

/**
 * Validates the child node of root and its sequence number if it is loaded.
 *
 * @param oi provides traversed nodes information.
 * @return bool return true if validation succeeds else false.
 */
static bool child_node_check(struct m0_btree_oimpl *oi)
{
	struct nd *l_node;

	if (cookie_is_used() || oi->i_used == 0)
		return true;

	l_node = oi->i_level[1].l_sibling;

	if (l_node) {
		node_lock(l_node);
		if (!node_isvalid(l_node)) {
			node_unlock(l_node);
			return false;
		}
		if (oi->i_level[1].l_sib_seq != l_node->n_seq) {
			node_unlock(l_node);
			return false;
		}
		node_unlock(l_node);
	}
	return true;
}

/**
 * This function will determine if there is requirement of loading root child.
 * If root contains only two records and if any of them is going to get deleted,
 * it is required to load the other child of root as well to handle root case.
 *
 * @param bop will provide all required information about btree operation.
 * @return int8_t return -1 if any ancestor node is not valid. return 1, if
 *                loading of child is needed, else return 0;
 */
static int8_t root_child_is_req(struct m0_btree_op *bop)
{
	struct m0_btree_oimpl *oi         = bop->bo_i;
	int8_t                 load       = 0;
	int                    used_count = oi->i_used;
	struct level          *lev;

	do {
		lev = &oi->i_level[used_count];
		node_lock(lev->l_node);
		if (!node_isvalid(lev->l_node)) {
			node_unlock(lev->l_node);
			return -1;
		}
		if (used_count == 0) {
			if (node_count_rec(lev->l_node) == 2)
				load = 1;
			node_unlock(lev->l_node);
			break;
		}
		if (!node_isunderflow(lev->l_node, true)) {
			node_unlock(lev->l_node);
			break;
		}
		node_unlock(lev->l_node);
		used_count--;
	}while (1);
	return load;
}

/**
 * This function will get called if root is an internal node and it contains
 * only two records. It will check if there is requirement for loading root's
 * other child and accordingly return the next state for execution.
 *
 * @param bop will provide all required information about btree operation.
 * @return int64_t return state which needs to get executed next.
 */
static int64_t root_case_handle(struct m0_btree_op *bop)
{
	/**
	 * If root is an internal node and it contains only two records, check
	 * if any record is going to be deleted if yes, we also have to load
	 * other child of root so that we can copy the content from that child
	 * at root and decrease the level by one.
	 */
	struct m0_btree_oimpl *oi            = bop->bo_i;
	int8_t                 load;

	load = root_child_is_req(bop);
	if (load == -1)
		return m0_sm_op_sub(&bop->bo_op, P_CLEANUP, P_SETUP);
	if (load) {
		struct slot     root_slot = {};
		struct segaddr  root_child;
		struct level   *root_lev = &oi->i_level[0];

		node_lock(root_lev->l_node);

		if (!node_isvalid(root_lev->l_node) ||
		    root_lev->l_node->n_seq != root_lev->l_seq) {
			node_unlock(root_lev->l_node);
			return m0_sm_op_sub(&bop->bo_op, P_CLEANUP, P_SETUP);
		}
		root_slot.s_node = root_lev->l_node;
		root_slot.s_idx  = root_lev->l_idx == 0 ? 1 : 0;

		node_child(&root_slot, &root_child);
		if (!address_in_segment(root_child)) {
			node_unlock(root_lev->l_node);
			node_op_fini(&oi->i_nop);
			return fail(bop, M0_ERR(-EFAULT));
		}
		node_unlock(root_lev->l_node);
		return node_get(&oi->i_nop, bop->bo_arbor->t_desc,
				&root_child, P_STORE_CHILD);
	}
	return P_LOCK;
}

/* State machine implementation for delete operation */
static int64_t btree_del_kv_tick(struct m0_sm_op *smop)
{
	struct m0_btree_op    *bop            = M0_AMB(bop, smop, bo_op);
	struct td             *tree           = bop->bo_arbor->t_desc;
	uint64_t               flags          = bop->bo_flags;
	struct m0_btree_oimpl *oi             = bop->bo_i;
	bool                   lock_acquired  = bop->bo_flags & BOF_LOCKALL;
	struct level          *lev;

	switch (bop->bo_op.o_sm.sm_state) {
	case P_INIT:
		M0_ASSERT(bop->bo_i == NULL);
		bop->bo_i = m0_alloc(sizeof *oi);
		if (bop->bo_i == NULL) {
			bop->bo_op.o_sm.sm_rc = M0_ERR(-ENOMEM);
			return P_DONE;
		}
		if ((flags & BOF_COOKIE) &&
		    cookie_is_set(&bop->bo_rec.r_key.k_cookie))
			return P_COOKIE;
		else
			return P_SETUP;
	case P_COOKIE:
		if (cookie_is_valid(tree, &bop->bo_rec.r_key.k_cookie) &&
		    !node_isunderflow(oi->i_cookie_node, true))
			return P_LOCK;
		else
			return P_SETUP;
	case P_LOCKALL:
		M0_ASSERT(bop->bo_flags & BOF_LOCKALL);
		return lock_op_init(&bop->bo_op, &bop->bo_i->i_nop,
				    bop->bo_arbor->t_desc, P_SETUP);
	case P_SETUP:
		oi->i_height = tree->t_height;
		level_alloc(oi, oi->i_height);
		if (oi->i_level == NULL) {
			if (lock_acquired)
				lock_op_unlock(tree);
			return fail(bop, M0_ERR(-ENOMEM));
		}
		bop->bo_i->i_key_found = false;
		oi->i_nop.no_op.o_sm.sm_rc = 0;
		/** Fall through to P_DOWN. */
	case P_DOWN:
		oi->i_used = 0;
		M0_SET0(&oi->i_capture);
		/* Load root node. */
		return node_get(&oi->i_nop, tree, &tree->t_root->n_addr,
				P_NEXTDOWN);
	case P_NEXTDOWN:
		if (oi->i_nop.no_op.o_sm.sm_rc == 0) {
			struct slot    node_slot = {};
			struct segaddr child_node_addr;

			lev = &oi->i_level[oi->i_used];
			lev->l_node = oi->i_nop.no_node;
			node_slot.s_node = oi->i_nop.no_node;

			node_lock(lev->l_node);
			lev->l_seq = oi->i_nop.no_node->n_seq;
			oi->i_nop.no_node = NULL;

			/**
			 * Node validation is required to determine that the
			 * node(lev->l_node) which is pointed by current thread
			 * is not freed by any other thread till current thread
			 * reaches NEXTDOWN phase.
			 *
			 * Node verification is required to determine that no
			 * other thread which has lock is working on the same
			 * node(lev->l_node) which is pointed by current thread.
			 */
			if (!node_isvalid(lev->l_node) || (oi->i_used > 0 &&
			    node_count_rec(lev->l_node) == 0)) {
				node_unlock(lev->l_node);
				return m0_sm_op_sub(&bop->bo_op, P_CLEANUP,
						    P_SETUP);
			}

			oi->i_key_found = node_find(&node_slot,
						    &bop->bo_rec.r_key);
			lev->l_idx = node_slot.s_idx;

			if (node_level(node_slot.s_node) > 0) {
				if (oi->i_key_found) {
					lev->l_idx++;
					node_slot.s_idx++;
				}
				node_child(&node_slot, &child_node_addr);

				if (!address_in_segment(child_node_addr)) {
					node_unlock(lev->l_node);
					node_op_fini(&oi->i_nop);
					return fail(bop, M0_ERR(-EFAULT));
				}
				oi->i_used++;
				if (oi->i_used >= oi->i_height) {
					/* If height of tree increased. */
					oi->i_used = oi->i_height - 1;
					node_unlock(lev->l_node);
					return m0_sm_op_sub(&bop->bo_op,
							    P_CLEANUP, P_SETUP);
				}
				node_unlock(lev->l_node);
				return node_get(&oi->i_nop, tree,
						&child_node_addr, P_NEXTDOWN);
			} else {
				node_unlock(lev->l_node);
				if (!oi->i_key_found)
					return P_LOCK;
				/**
				 * If root is an internal node and it contains
				 * only two record, if any of the record is
				 * going to be deleted, load the other child of
				 * root.
				 */
				if (oi->i_used > 0) {
					struct nd *root_node;
					root_node = oi->i_level[0].l_node;
					node_lock(root_node);
					if (node_count_rec(root_node) == 2) {
						node_unlock(root_node);
						return root_case_handle(bop);
					}
					node_unlock(root_node);
				}

				return P_LOCK;
			}
		} else {
			node_op_fini(&oi->i_nop);
			return m0_sm_op_sub(&bop->bo_op, P_CLEANUP, P_SETUP);
		}
	case P_STORE_CHILD: {
		if (oi->i_nop.no_op.o_sm.sm_rc == 0) {
			struct nd *root_child;

			oi->i_level[1].l_sibling = oi->i_nop.no_node;
			root_child = oi->i_level[1].l_sibling;
			node_lock(root_child);

			oi->i_level[1].l_sib_seq = oi->i_nop.no_node->n_seq;
			oi->i_nop.no_node = NULL;

			if (!node_isvalid(root_child) ||
			    node_count_rec(root_child) == 0) {
				node_unlock(root_child);
 				return m0_sm_op_sub(&bop->bo_op, P_CLEANUP,
						    P_SETUP);
			}

			node_unlock(root_child);
			/* Fall through to the next step */
		} else {
			node_op_fini(&oi->i_nop);
			return m0_sm_op_sub(&bop->bo_op, P_CLEANUP, P_SETUP);
		}
	}
	case P_LOCK:
		if (!lock_acquired)
			return lock_op_init(&bop->bo_op, &bop->bo_i->i_nop,
					    bop->bo_arbor->t_desc, P_CHECK);
		/* Fall through to the next step */
	case P_CHECK:
		if (!path_check(oi, tree, &bop->bo_rec.r_key.k_cookie) ||
		    !child_node_check(oi)) {
			oi->i_trial++;
			if (oi->i_trial >= MAX_TRIALS) {
				M0_ASSERT_INFO((bop->bo_flags & BOF_LOCKALL) ==
					       0, "Delete record failure in"
					       "tree lock mode");
				bop->bo_flags |= BOF_LOCKALL;
				lock_op_unlock(tree);
				return m0_sm_op_sub(&bop->bo_op, P_CLEANUP,
						    P_LOCKALL);
			}
			if (oi->i_height != tree->t_height) {
				/* If height has changed. */
				lock_op_unlock(tree);
				return m0_sm_op_sub(&bop->bo_op, P_CLEANUP,
					            P_SETUP);
			} else {
				/* If height is same, put back all the nodes. */
				lock_op_unlock(tree);
				level_put(oi, bop->bo_tx);
				return P_DOWN;
			}
		}
		/**
		 * Fall through if path_check and child_node_check are
		 * successful.
		 */
	case P_ACT: {
		struct m0_btree_rec rec;
		struct slot         node_slot;
		bool                node_underflow;
		/**
		 *  if key exists, delete the key, if there is an underflow, go
		 *  to resolve function else return P_CLEANUP.
		*/

		if (!oi->i_key_found)
			rec.r_flags = M0_BSC_KEY_NOT_FOUND;
		else {
			lev = &oi->i_level[oi->i_used];
			node_slot.s_node = lev->l_node;
			node_slot.s_idx  = lev->l_idx;

			node_lock(lev->l_node);

			node_del(node_slot.s_node, node_slot.s_idx, bop->bo_tx);
			node_done(&node_slot, bop->bo_tx, true);
			node_seq_cnt_update(lev->l_node);
			node_fix(node_slot.s_node, bop->bo_tx);
			btree_node_capture_enlist(oi, lev->l_node, lev->l_idx);
			/**
			 * TBD : This check needs to be removed when debugging
			 * is done.
			 */
			M0_ASSERT(node_expensive_invariant(lev->l_node));
			node_underflow = node_isunderflow(lev->l_node, false);
			if (oi->i_used != 0  && node_underflow) {
				node_fini(lev->l_node, bop->bo_tx);
				lev->l_freenode = true;
			}

			node_unlock(lev->l_node);

			rec.r_flags = M0_BSC_SUCCESS;
		}
		int rc = bop->bo_cb.c_act(&bop->bo_cb, &rec);
		if (rc) {
			M0_ASSERT(!oi->i_key_found);
			lock_op_unlock(tree);
			return fail(bop, rc);
		}

		if (oi->i_key_found) {
			if (oi->i_used == 0 || !node_underflow) {
				/* No Underflow */
				return P_CAPTURE;
			}
			return btree_del_resolve_underflow(bop);
		}
		M0_ASSERT(0);
	}
	case P_CAPTURE:
		btree_tx_nodes_capture(oi, bop->bo_tx);
		return P_FREENODE;
	case P_FREENODE : {
		int i;
		for (i = oi->i_used; i >= 0; i--) {
			lev = &oi->i_level[i];
			if (lev->l_freenode) {
				M0_ASSERT(oi->i_used > 0);
				oi->i_nop.no_opc = NOP_FREE;
				node_free(&oi->i_nop, lev->l_node,
					  bop->bo_tx, 0);
				lev->l_node = NULL;
			} else
				break;
		}
		if (oi->i_root_child_free) {
			lev = &oi->i_level[1];
			oi->i_nop.no_opc = NOP_FREE;
			node_free(&oi->i_nop, lev->l_sibling, bop->bo_tx, 0);
			lev->l_sibling = NULL;
		}

		lock_op_unlock(tree);
		return m0_sm_op_sub(&bop->bo_op, P_CLEANUP, P_FINI);
	}
	case P_CLEANUP :
		level_cleanup(oi, bop->bo_tx);
		return m0_sm_op_ret(&bop->bo_op);
	case P_FINI :
		M0_ASSERT(oi);
		m0_free(oi);
		return P_DONE;
	default:
		M0_IMPOSSIBLE("Wrong state: %i", bop->bo_op.o_sm.sm_state);
	};
}

/**
 * TODO: Call this function to free up node descriptor from LRU list.
 * A daemon should run in parallel to check the health of the system. If it
 * requires more memory the node descriptors can be freed from LRU list.
 *
 * @param count number of node descriptors to be freed.
 */
void m0_btree_lrulist_purge(uint64_t count)
{
	struct nd* node;
	struct nd* prev;

	m0_rwlock_write_lock(&list_lock);
	node = ndlist_tlist_tail(&btree_lru_nds);
	for (;  node != NULL && count > 0; count --) {
		prev = ndlist_tlist_prev(&btree_lru_nds, node);
		if (node->n_txref == 0) {
			ndlist_tlink_del_fini(node);
			m0_rwlock_fini(&node->n_lock);
			m0_free(node);
		}
		node = prev;
	}
	m0_rwlock_write_unlock(&list_lock);
}

int  m0_btree_open(void *addr, int nob, struct m0_btree **out,
		   struct m0_btree_op *bop)
{
	bop->b_data.addr      = addr;
	bop->b_data.num_bytes = nob;
	bop->bo_arbor         = *out;

	m0_sm_op_init(&bop->bo_op, &btree_open_tree_tick, &bop->bo_op_exec,
		      &btree_conf, &bop->bo_sm_group);
	return 0;
}

void m0_btree_close(struct m0_btree *arbor, struct m0_btree_op *bop)
{
	bop->bo_arbor = arbor;
	m0_sm_op_init(&bop->bo_op, &btree_close_tree_tick, &bop->bo_op_exec,
		      &btree_conf, &bop->bo_sm_group);
}

void m0_btree_create(void *addr, int nob, const struct m0_btree_type *bt,
		     const struct node_type *nt, struct m0_btree_op *bop,
		     struct m0_be_seg *seg, struct m0_be_tx *tx)
{
	bop->b_data.addr        = addr;
	bop->b_data.num_bytes   = nob;
	bop->b_data.bt          = bt;
	bop->b_data.nt          = nt;
	bop->bo_tx              = tx;
	bop->bo_seg             = seg;

	m0_sm_op_init(&bop->bo_op, &btree_create_tree_tick, &bop->bo_op_exec,
		      &btree_conf, &bop->bo_sm_group);
}

void m0_btree_destroy(struct m0_btree *arbor, struct m0_btree_op *bop,
		      struct m0_be_tx *tx)
{
	bop->bo_arbor = arbor;
	bop->bo_tx    = tx;
	bop->bo_seg   = arbor->t_desc->t_seg;

	m0_sm_op_init(&bop->bo_op, &btree_destroy_tree_tick, &bop->bo_op_exec,
		      &btree_conf, &bop->bo_sm_group);
}

void m0_btree_get(struct m0_btree *arbor, const struct m0_btree_key *key,
		  const struct m0_btree_cb *cb, uint64_t flags,
		  struct m0_btree_op *bop)
{
	bop->bo_opc       = M0_BO_GET;
	bop->bo_arbor     = arbor;
	bop->bo_rec.r_key = *key;
	bop->bo_flags     = flags;
	bop->bo_cb        = *cb;
	bop->bo_tx        = NULL;
	bop->bo_seg       = NULL;
	bop->bo_i         = NULL;
	m0_sm_op_init(&bop->bo_op, &btree_get_kv_tick, &bop->bo_op_exec,
		      &btree_conf, &bop->bo_sm_group);
}

void m0_btree_iter(struct m0_btree *arbor, const struct m0_btree_key *key,
		   const struct m0_btree_cb *cb, uint64_t flags,
		   struct m0_btree_op *bop)
{
	M0_PRE(flags & BOF_NEXT || flags & BOF_PREV);

	bop->bo_opc       = M0_BO_ITER;
	bop->bo_arbor     = arbor;
	bop->bo_rec.r_key = *key;
	bop->bo_flags     = flags;
	bop->bo_cb        = *cb;
	bop->bo_tx        = NULL;
	bop->bo_seg       = NULL;
	bop->bo_i         = NULL;
	m0_sm_op_init(&bop->bo_op, &btree_iter_kv_tick, &bop->bo_op_exec,
		      &btree_conf, &bop->bo_sm_group);
}

void m0_btree_put(struct m0_btree *arbor, const struct m0_btree_rec *rec,
		  const struct m0_btree_cb *cb, uint64_t flags,
		  struct m0_btree_op *bop, struct m0_be_tx *tx)
{
	bop->bo_opc    = M0_BO_PUT;
	bop->bo_arbor  = arbor;
	bop->bo_rec    = *rec;
	bop->bo_cb     = *cb;
	bop->bo_tx     = tx;
	bop->bo_flags  = flags;
	bop->bo_seg    = arbor->t_desc->t_seg;
	bop->bo_i      = NULL;

	m0_sm_op_init(&bop->bo_op, &btree_put_kv_tick, &bop->bo_op_exec,
		      &btree_conf, &bop->bo_sm_group);
}

void m0_btree_del(struct m0_btree *arbor, const struct m0_btree_key *key,
		  const struct m0_btree_cb *cb, uint64_t flags,
		  struct m0_btree_op *bop, struct m0_be_tx *tx)
{
	bop->bo_opc       = M0_BO_DEL;
	bop->bo_arbor     = arbor;
	bop->bo_rec.r_key = *key;
	bop->bo_cb        = *cb;
	bop->bo_tx        = tx;
	bop->bo_flags     = flags;
	bop->bo_seg       = arbor->t_desc->t_seg;
	bop->bo_i         = NULL;

	m0_sm_op_init(&bop->bo_op, &btree_del_kv_tick, &bop->bo_op_exec,
		      &btree_conf, &bop->bo_sm_group);
}

void m0_btree_minkey(struct m0_btree *arbor, const struct m0_btree_cb *cb,
		     uint64_t flags, struct m0_btree_op *bop)
{
	bop->bo_opc       = M0_BO_MINKEY;
	bop->bo_arbor     = arbor;
	bop->bo_flags     = flags;
	bop->bo_cb        = *cb;
	bop->bo_tx        = NULL;
	bop->bo_seg       = NULL;
	bop->bo_i         = NULL;

	m0_sm_op_init(&bop->bo_op, &btree_get_kv_tick, &bop->bo_op_exec,
		      &btree_conf, &bop->bo_sm_group);
}

void m0_btree_maxkey(struct m0_btree *arbor, const struct m0_btree_cb *cb,
		     uint64_t flags, struct m0_btree_op *bop)
{
	bop->bo_opc       = M0_BO_MAXKEY;
	bop->bo_arbor     = arbor;
	bop->bo_flags     = flags;
	bop->bo_cb        = *cb;
	bop->bo_tx        = NULL;
	bop->bo_seg       = NULL;
	bop->bo_i         = NULL;

	m0_sm_op_init(&bop->bo_op, &btree_get_kv_tick, &bop->bo_op_exec,
		      &btree_conf, &bop->bo_sm_group);
}

void m0_btree_update(struct m0_btree *arbor, const struct m0_btree_rec *rec,
		     const struct m0_btree_cb *cb, uint64_t flags,
		     struct m0_btree_op *bop, struct m0_be_tx *tx)
{
	bop->bo_opc    = M0_BO_UPDATE;
	bop->bo_arbor  = arbor;
	bop->bo_rec    = *rec;
	bop->bo_cb     = *cb;
	bop->bo_tx     = tx;
	bop->bo_flags  = flags;
	bop->bo_seg    = arbor->t_desc->t_seg;
	bop->bo_i      = NULL;

	m0_sm_op_init(&bop->bo_op, &btree_put_kv_tick, &bop->bo_op_exec,
		      &btree_conf, &bop->bo_sm_group);
}

#endif

#ifndef __KERNEL__
/**
 *  --------------------------
 *  Section START - Unit Tests
 *  --------------------------
 */

/**
 * The code contained below is 'ut'. This is a little experiment to contain the
 * ut code in the same file containing the functionality code. We are open to
 * changes iff enough reasons are found that this model either does not work or
 * is not intuitive or maintainable.
 */

static struct m0_be_ut_backend *ut_be;
static struct m0_be_ut_seg     *ut_seg;
static struct m0_be_seg        *seg;
static bool                     btree_ut_initialised = false;

static void btree_ut_init(void)
{
	if (!btree_ut_initialised) {
		m0_btree_mod_init();
		btree_ut_initialised = true;
	}
}

static void btree_ut_fini(void)
{
	m0_btree_mod_fini();
	btree_ut_initialised = false;
}

/**
 * This test will create a few nodes and then delete them before exiting. The
 * main intent of this test is to debug the create and delete nodes functions.
 */
static void ut_node_create_delete(void)
{
	struct node_op          op;
	struct node_op          op1;
	struct node_op          op2;
	/* struct m0_btree_type    tt; */
	struct td              *tree;
	struct td              *tree_clone;
	struct nd              *node1;
	struct nd              *node2;
	const struct node_type *nt    = &fixed_format;

	M0_ENTRY();

	btree_ut_init();

	M0_SET0(&op);

	// Create a Fixed-Format tree.
	op.no_opc = NOP_ALLOC;
	/* tree_create(&op, &tt, 10, NULL, 0); */

	tree = op.no_tree;

	M0_ASSERT(tree->t_ref == 1);
	M0_ASSERT(tree->t_root != NULL);

	// Add a few nodes to the created tree.
	op1.no_opc = NOP_ALLOC;
	node_alloc(&op1, tree, 10, nt, 8, 8, NULL, 0);
	node1 = op1.no_node;

	op2.no_opc = NOP_ALLOC;
	node_alloc(&op2,  tree, 10, nt, 8, 8, NULL, 0);
	node2 = op2.no_node;

	op1.no_opc = NOP_FREE;
	node_free(&op1, node1, NULL, 0);

	op2.no_opc = NOP_FREE;
	node_free(&op2, node2, NULL, 0);

	/* Get another reference to the same tree. */
	tree_get(&op, &tree->t_root->n_addr, 0);
	tree_clone = op.no_tree;
	M0_ASSERT(tree_clone->t_ref == 2);
	M0_ASSERT(tree->t_root == tree_clone->t_root);

	tree_put(tree_clone);

	// Done playing with the tree - delete it.
	op.no_opc = NOP_FREE;
	tree_delete(&op, tree, NULL, 0);

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
	struct m0_be_tx     *tx = NULL;

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

	node_capture(&slot, tx);
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

	find_key.k_data = M0_BUFVEC_INIT_BUF(&p_key, &ksize);

	slot.s_rec.r_key.k_data = M0_BUFVEC_INIT_BUF(&p_key, &ksize);

	slot.s_rec.r_val = M0_BUFVEC_INIT_BUF(&p_val, &vsize);

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

void get_rec_at_index(struct nd *node, int idx, uint64_t *key,  uint64_t *val)
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

void get_key_at_index(struct nd *node, int idx, uint64_t *key)
{
	struct slot          slot;
	m0_bcount_t          ksize;
	void                *p_key;

	M0_SET0(&slot);
	slot.s_node = node;
	slot.s_idx  = idx;

	M0_ASSERT(idx<node_count(node));

	slot.s_rec.r_key.k_data.ov_vec.v_nr = 1;
	slot.s_rec.r_key.k_data.ov_vec.v_count = &ksize;
	slot.s_rec.r_key.k_data.ov_buf = &p_key;

	node_key(&slot);

	if (key != NULL)
		*key = *(uint64_t *)p_key;
}
/**
 * This unit test will create a tree, add a node and then populate the node with
 * some records. It will also confirm the records are in ascending order of Key.
 */
static void ut_node_add_del_rec(void)
{
	struct node_op          op;
	struct node_op          op1;
	/* struct m0_btree_type    tt; */
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
	M0_LOG(M0_INFO, "Using seed %lu", curr_time);
	srand(curr_time);

	run_loop = 50000;

	btree_ut_init();

	M0_SET0(&op);

	op.no_opc = NOP_ALLOC;
	/* tree_create(&op, &tt, 10, NULL, 0); */

	tree = op.no_tree;

	op1.no_opc = NOP_ALLOC;
	node_alloc(&op1, tree, 10, nt, 8, 8, NULL, 0);
	node1 = op1.no_node;

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

	op1.no_opc = NOP_FREE;
	node_free(&op1, node1, NULL, 0);

	// Done playing with the tree - delete it.
	op.no_opc = NOP_FREE;
	tree_delete(&op, tree, NULL, 0);

	btree_ut_fini();

	M0_LEAVE();
}

/**
 * In this unit test we exercise a few tree operations in valid conditions only.
 */
static void ut_basic_tree_oper_cp(void)
{
	/** void                   *invalid_addr = (void *)0xbadbadbadbad; */
	struct m0_btree        *btree;
	struct m0_btree        *temp_btree;
	struct m0_btree_type    btree_type = {  .tt_id = M0_BT_UT_KV_OPS,
						.ksize = 8,
						.vsize = 8, };
	struct m0_be_tx        *tx         = NULL;
	struct m0_be_seg       *seg        = NULL;
	struct m0_btree_op      b_op       = {};
	void                   *temp_node;
	const struct node_type *nt = &fixed_format;
	int                     rc;

	/** Prepare transaction to capture tree operations. */
	m0_be_tx_init(tx, 0, NULL, NULL, NULL, NULL, NULL, NULL);
	m0_be_tx_prep(tx, NULL);
	btree_ut_init();
	/**
	 *  Run a valid scenario which:
	 *  1) Creates a btree
	 *  2) Opens the btree
	 *  3) Closes the btree
	 *  4) Destroys the btree
	 */

	/** Create temp node space*/
	temp_node = m0_alloc_aligned((1024 + sizeof(struct nd)), 10);
	btree = m0_alloc(sizeof *btree);
	rc = M0_BTREE_OP_SYNC_WITH_RC(&b_op, m0_btree_create(temp_node, 1024,
				      &btree_type, nt, &b_op, seg, tx));
	M0_ASSERT(rc == 0);
	temp_btree = b_op.bo_arbor;

	rc = M0_BTREE_OP_SYNC_WITH_RC(&b_op, m0_btree_open(temp_node, 1024,
							   &btree, &b_op));
	M0_ASSERT(rc == 0);

	rc = M0_BTREE_OP_SYNC_WITH_RC(&b_op, m0_btree_close(btree, &b_op));
	M0_ASSERT(rc == 0);
	b_op.bo_arbor = temp_btree;

	rc = M0_BTREE_OP_SYNC_WITH_RC(&b_op, m0_btree_destroy(b_op.bo_arbor,
							      &b_op, tx));
	M0_ASSERT(rc == 0);
	m0_free(btree);
	m0_free_aligned(temp_node, (1024 + sizeof(struct nd)), 10);

	/**
	 *  Run a valid scenario which:
	 *  1) Creates a btree
	 *  2) Destroys the btree
	 */
	/** Create a new btree */
	temp_node = m0_alloc_aligned((1024 + sizeof(struct nd)), 10);
	rc = M0_BTREE_OP_SYNC_WITH_RC(&b_op, m0_btree_create(temp_node, 1024,
							     &btree_type, nt,
							     &b_op, seg, tx));
	M0_ASSERT(rc == 0);

	/** Destory it */
	rc = M0_BTREE_OP_SYNC_WITH_RC(&b_op, m0_btree_destroy(b_op.bo_arbor,
							      &b_op, tx));
	M0_ASSERT(rc == 0);
	m0_free_aligned(temp_node, (1024 + sizeof(struct nd)), 10);

	/**
	 *  Run a valid scenario which:
	 *  1) Creates a btree
	 *  2) Opens the btree
	 *  3) Closes the btree
	 *  4) Destroys the btree
	 */

	/** Create temp node space*/
	temp_node = m0_alloc_aligned((1024 + sizeof(struct nd)), 10);
	rc = M0_BTREE_OP_SYNC_WITH_RC(&b_op, m0_btree_create(temp_node, 1024,
				      &btree_type, nt, &b_op, seg, tx));
	M0_ASSERT(rc == 0);

	rc = M0_BTREE_OP_SYNC_WITH_RC(&b_op,
				      m0_btree_close(b_op.bo_arbor, &b_op));
	M0_ASSERT(rc == 0);

	rc = M0_BTREE_OP_SYNC_WITH_RC(&b_op, m0_btree_open(temp_node, 1024,
							   &b_op.bo_arbor,
							   &b_op));
	M0_ASSERT(rc == 0);

	rc = M0_BTREE_OP_SYNC_WITH_RC(&b_op, m0_btree_destroy(b_op.bo_arbor,
							      &b_op, tx));
	M0_ASSERT(rc == 0);
	m0_free_aligned(temp_node, (1024 + sizeof(struct nd)), 10);

	btree_ut_fini();
}

/**
 * In this unit test we exercise a few tree operations in invalid conditions
 * only.
 */
static void ut_basic_tree_oper_icp(void)
{
	void                   *invalid_addr = (void *)0xbadbadbadbad;
	struct m0_btree        *btree;
	struct m0_btree_type    btree_type = {  .tt_id = M0_BT_UT_KV_OPS,
						.ksize = 8,
						.vsize = 8, };
	struct m0_be_tx        *tx         = NULL;
	struct m0_be_seg       *seg        = NULL;
	struct m0_btree_op      b_op       = {};
	void                   *temp_node;
	const struct node_type *nt = &fixed_format;
	int                     rc;

	/** Prepare transaction to capture tree operations. */
	m0_be_tx_init(tx, 0, NULL, NULL, NULL, NULL, NULL, NULL);
	m0_be_tx_prep(tx, NULL);
	btree_ut_init();

	/**
	 *  Run a invalid scenario which:
	 *  1) Attempts to create a btree with invalid address
	 * 
	 * This scenario is invalid because the root node address is incorrect.
	 * In this case m0_btree_create() will return -EFAULT.
	 */
	rc = M0_BTREE_OP_SYNC_WITH_RC(&b_op, m0_btree_create(invalid_addr, 1024,
				      &btree_type, nt, &b_op, seg, tx));
	M0_ASSERT(rc == -EFAULT);

	/**
	 *  Run a invalid scenario which:
	 *  1) Attempts to open a btree with invalid address
	 * 
	 * This scenario is invalid because the root node address is incorrect.
	 * In this case m0_btree_open() will return -EFAULT.
	 */
	rc = M0_BTREE_OP_SYNC_WITH_RC(&b_op, m0_btree_open(invalid_addr, 1024,
							   &btree, &b_op));
	M0_ASSERT(rc == -EFAULT);

	/**
	 *  Run a invalid scenario which:
	 *  1) Creates a btree
	 *  2) Opens the btree
	 *  3) Destroys the btree
	 * 
	 * This scenario is invalid because we are trying to destroy a tree that
	 * has not been closed. Thus, we should get -EPERM error from
	 * m0_btree_destroy().
	 */

	/** Create temp node space*/
	temp_node = m0_alloc_aligned((1024 + sizeof(struct nd)), 10);
	btree = m0_alloc(sizeof *btree);
	rc = M0_BTREE_OP_SYNC_WITH_RC(&b_op, m0_btree_create(temp_node, 1024,
				      &btree_type, nt, &b_op, seg, tx));
	M0_ASSERT(rc == 0);

	rc = M0_BTREE_OP_SYNC_WITH_RC(&b_op, m0_btree_open(temp_node, 1024,
							   &btree, &b_op));
	M0_ASSERT(rc == 0);

	rc = M0_BTREE_OP_SYNC_WITH_RC(&b_op, m0_btree_destroy(b_op.bo_arbor,
							      &b_op, tx));
	M0_ASSERT(rc == -EPERM);
	m0_free(btree);
	m0_free_aligned(temp_node, (1024 + sizeof(struct nd)), 10);

	/**
	 *  Run a invalid scenario which:
	 *  1) Creates a btree
	 *  2) Close the btree
	 *  3) Again Attempt to close the btree
	 * 
	 * This scenario is invalid because we are trying to destroy a tree that
	 * has been already destroyed. Thus, we should get -EINVAL error from
	 * m0_btree_destroy().
	 */

	/** Create temp node space*/
	temp_node = m0_alloc_aligned((1024 + sizeof(struct nd)), 10);
	rc = M0_BTREE_OP_SYNC_WITH_RC(&b_op, m0_btree_create(temp_node, 1024,
				      &btree_type, nt, &b_op, seg, tx));
	M0_ASSERT(rc == 0);

	rc = M0_BTREE_OP_SYNC_WITH_RC(&b_op, m0_btree_close(b_op.bo_arbor,
							    &b_op));
	M0_ASSERT(rc == 0);

	rc = M0_BTREE_OP_SYNC_WITH_RC(&b_op, m0_btree_close(b_op.bo_arbor,
							    &b_op));
	M0_ASSERT(rc == -EINVAL);

	m0_free_aligned(temp_node, (1024 + sizeof(struct nd)), 10);
	btree_ut_fini();
}

struct cb_data {
	/** Key that needs to be stored or retrieved. */
	struct m0_btree_key *key;

	/** Value associated with the key that is to be stored or retrieved. */
	struct m0_bufvec    *value;

	/** If value is retrieved (GET) then check if has expected contents. */
	bool                 check_value;

	/**
	 *  This field is filled by the callback routine with the flags which
	 *  the CB routine received from the _tick(). This flag can then be
	 *  analyzed by the caller for further processing.
	 */
	uint32_t             flags;
};

/**
 * Put callback will receive record pointer from put routine which will contain
 * r_flag which will indicate SUCCESS or ERROR code.
 * If put routine is able to find the record, it will set SUCCESS code and
 * callback routine needs to put key-value to the given record and return
 * success code.
 * else put routine will set the ERROR code to indicate failure and it is
 * expected to return ERROR code by callback routine.
 */
static int btree_kv_put_cb(struct m0_btree_cb *cb, struct m0_btree_rec *rec)
{
	struct m0_bufvec_cursor  scur;
	struct m0_bufvec_cursor  dcur;
	m0_bcount_t              ksize;
	m0_bcount_t              vsize;
	struct cb_data          *datum = cb->c_datum;

	/** The caller can look at these flags if he needs to. */
	datum->flags = rec->r_flags;
	M0_ASSERT(datum->flags == M0_BSC_SUCCESS);

	ksize = m0_vec_count(&datum->key->k_data.ov_vec);
	M0_ASSERT(m0_vec_count(&rec->r_key.k_data.ov_vec) >= ksize);

	vsize = m0_vec_count(&datum->value->ov_vec);
	M0_ASSERT(m0_vec_count(&rec->r_val.ov_vec) >= vsize);

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
	struct m0_bufvec_cursor  scur;
	struct m0_bufvec_cursor  dcur;
	m0_bcount_t              ksize;
	m0_bcount_t              vsize;
	struct cb_data          *datum = cb->c_datum;

	/** The caller can look at these flags if he needs to. */
	datum->flags = rec->r_flags;

	if (rec->r_flags == M0_BSC_KEY_NOT_FOUND ||
	    rec->r_flags == M0_BSC_KEY_BTREE_BOUNDARY)
		return rec->r_flags;

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

	if (datum->check_value) {
		struct m0_bufvec_cursor kcur;
		struct m0_bufvec_cursor vcur;
		m0_bcount_t             v_off = 0;
		bool                    check_failed = false;
		uint64_t                key;
		uint64_t                value;

		m0_bufvec_cursor_init(&kcur, &rec->r_key.k_data);
		m0_bufvec_cursor_copyfrom(&kcur, &key, sizeof(key));
		m0_bufvec_cursor_init(&vcur, &rec->r_val);
		vsize = sizeof(value);
		while (v_off < vsize) {

			m0_bufvec_cursor_copyfrom(&vcur, &value, vsize);
			if (key != value)
				check_failed = true;
			v_off += vsize;
		}

		/**
		 * If check_failed then maybe this entry was updated in which
		 * case we use the complement of the key for comparison.
		 */
		if (check_failed) {
			m0_bufvec_cursor_init(&vcur, &rec->r_val);
			v_off = 0;
			key = ~key;
			while (v_off < vsize) {
				m0_bufvec_cursor_copyfrom(&vcur, &value, vsize);
				if (key != value)
					M0_ASSERT(0);
				v_off += vsize;
			}
		}
	}

	return 0;
}

static int btree_kv_del_cb(struct m0_btree_cb *cb, struct m0_btree_rec *rec)
{
	struct cb_data          *datum = cb->c_datum;

	/** The caller can look at these flags if he needs to. */
	datum->flags = rec->r_flags;

	return rec->r_flags;
}

static int btree_kv_update_cb(struct m0_btree_cb *cb, struct m0_btree_rec *rec)
{
	struct m0_bufvec_cursor  scur;
	struct m0_bufvec_cursor  dcur;
	m0_bcount_t              vsize;
	struct cb_data          *datum = cb->c_datum;

	/** The caller can look at these flags if he needs to. */
	datum->flags = rec->r_flags;
	M0_ASSERT(datum->flags == M0_BSC_SUCCESS);

	vsize = m0_vec_count(&datum->value->ov_vec);
	M0_ASSERT(m0_vec_count(&rec->r_val.ov_vec) >= vsize);

	m0_bufvec_cursor_init(&scur, datum->value);
	m0_bufvec_cursor_init(&dcur, &rec->r_val);
	m0_bufvec_cursor_copy(&dcur, &scur, vsize);

	return 0;
}

/**
 * This unit test exercises the KV operations for both valid and invalid
 * conditions.
 */
static void ut_basic_kv_oper(void)
{
	struct m0_btree_type    btree_type  = {.tt_id = M0_BT_UT_KV_OPS,
					      .ksize = 8,
					      .vsize = 8, };
	struct m0_be_tx        *tx          = NULL;
	struct m0_be_seg       *seg         = NULL;
	struct m0_be_tx_credit  cred        = {};
	struct m0_btree_op      b_op        = {};
	struct m0_btree        *tree;
	void                   *temp_node;
	int                     i;
	time_t                  curr_time;
	struct m0_btree_cb      ut_cb;
	uint64_t                first_key;
	bool                    first_key_initialized = false;
	struct m0_btree_op      kv_op                 = {};
	const struct node_type *nt                    = &fixed_format;
	int                     rc;
	M0_ENTRY();

	time(&curr_time);
	M0_LOG(M0_INFO, "Using seed %lu", curr_time);
	srandom(curr_time);

	/** Prepare transaction to capture tree operations. */
	m0_be_tx_init(tx, 0, NULL, NULL, NULL, NULL, NULL, NULL);
	m0_be_tx_prep(tx, NULL);
	btree_ut_init();
	/**
	 *  Run valid scenario:
	 *  1) Create a btree
	 *  2) Adds a few records to the created tree.
	 *  3) Confirms the records are present in the tree.
	 *  4) Deletes all the records from the tree.
	 *  4) Close the btree
	 *  5) Destroy the btree
	 */

	/** Create temp node space and use it as root node for btree */
	temp_node = m0_alloc_aligned((1024 + sizeof(struct nd)), 10);
	rc = M0_BTREE_OP_SYNC_WITH_RC(&b_op, m0_btree_create(temp_node, 1024,
							&btree_type, nt,
							&b_op, seg, tx));
	M0_ASSERT(rc == 0);
	tree = b_op.bo_arbor;

	for (i = 0; i < 2048; i++) {
		uint64_t             key;
		uint64_t             value;
		struct cb_data       put_data;
		struct m0_btree_rec  rec;
		m0_bcount_t          ksize  = sizeof key;
		m0_bcount_t          vsize  = sizeof value;
		void                *k_ptr  = &key;
		void                *v_ptr  = &value;

		cred = M0_BE_TX_CB_CREDIT(0, 0, 0);
		btree_callback_credit(&cred);

		/**
		 *  There is a very low possibility of hitting the same key
		 *  again. This is fine as it helps debug the code when insert
		 *  is called with the same key instead of update function.
		 */
		key = value = m0_byteorder_cpu_to_be64(random());

		if (!first_key_initialized) {
			first_key = key;
			first_key_initialized = true;
		}

		rec.r_key.k_data   = M0_BUFVEC_INIT_BUF(&k_ptr, &ksize);
		rec.r_val          = M0_BUFVEC_INIT_BUF(&v_ptr, &vsize);

		put_data.key       = &rec.r_key;
		put_data.value     = &rec.r_val;

		ut_cb.c_act        = btree_kv_put_cb;
		ut_cb.c_datum      = &put_data;

		M0_BTREE_OP_SYNC_WITH_RC(&kv_op, m0_btree_put(tree, &rec,
							      &ut_cb, 0,
							      &kv_op, tx));
	}

	{
		uint64_t             key;
		uint64_t             value;
		struct cb_data       get_data;
		struct m0_btree_key  get_key;
		struct m0_bufvec     get_value;
		m0_bcount_t          ksize            = sizeof key;
		m0_bcount_t          vsize            = sizeof value;
		void                *k_ptr            = &key;
		void                *v_ptr            = &value;
		uint64_t             find_key;
		void                *find_key_ptr     = &find_key;
		m0_bcount_t          find_key_size    = sizeof find_key;
		struct m0_btree_key  find_key_in_tree;

		get_key.k_data = M0_BUFVEC_INIT_BUF(&k_ptr, &ksize);
		get_value      = M0_BUFVEC_INIT_BUF(&v_ptr, &vsize);

		get_data.key    = &get_key;
		get_data.value  = &get_value;

		ut_cb.c_act   = btree_kv_get_cb;
		ut_cb.c_datum = &get_data;

		find_key = first_key;

		find_key_in_tree.k_data =
				M0_BUFVEC_INIT_BUF(&find_key_ptr, &find_key_size);

		M0_BTREE_OP_SYNC_WITH_RC(&kv_op,
					 m0_btree_get(tree,
						      &find_key_in_tree,
						      &ut_cb, BOF_EQUAL,
						      &kv_op));

		for (i = 1; i < 2048; i++) {
			find_key = key;
			M0_BTREE_OP_SYNC_WITH_RC(&kv_op,
						 m0_btree_iter(tree,
							       &find_key_in_tree,
							       &ut_cb, BOF_NEXT,
							       &kv_op));
		}
	}

	srandom(curr_time);
	for (i = 0; i < 2048; i++) {
		uint64_t             key;
		struct cb_data       delete_data;
		struct m0_btree_rec  rec;
		m0_bcount_t          ksize  = sizeof key;
		void                *k_ptr  = &key;

		cred = M0_BE_TX_CB_CREDIT(0, 0, 0);
		btree_callback_credit(&cred);

		/**
		*  There is a very low possibility of hitting the same key
		*  again. This is fine as it helps debug the code when insert
		*  is called with the same key instead of update function.
		*/
		key = m0_byteorder_cpu_to_be64(random());

		rec.r_key.k_data   = M0_BUFVEC_INIT_BUF(&k_ptr, &ksize);

		delete_data.key       = &rec.r_key;

		ut_cb.c_act        = btree_kv_del_cb;
		ut_cb.c_datum      = &delete_data;

		M0_BTREE_OP_SYNC_WITH_RC(&kv_op, m0_btree_del(tree, &rec.r_key,
								&ut_cb, 0,
								&kv_op, tx));
	}

	rc = M0_BTREE_OP_SYNC_WITH_RC(&b_op, m0_btree_destroy(tree, &b_op, tx));
	M0_ASSERT(rc == 0);

	btree_ut_fini();
}

#if (AVOID_BE_SEGMENT == 1)
enum {
	MIN_STREAM_CNT         = 5,
	MAX_STREAM_CNT         = 20,

	MIN_RECS_PER_STREAM    = 5,
	MAX_RECS_PER_STREAM    = 2048,

	MAX_RECS_PER_THREAD    = 100000, /** Records count for each thread */

	MIN_TREE_LOOPS         = 5000,
	MAX_TREE_LOOPS         = 15000,
	MAX_RECS_FOR_TREE_TEST = 100,

	RANDOM_TREE_COUNT      = -1,
	RANDOM_THREAD_COUNT    = -1,
};
#else
enum {
	MIN_STREAM_CNT         = 5,
	MAX_STREAM_CNT         = 10,

	MIN_RECS_PER_STREAM    = 5,
	MAX_RECS_PER_STREAM    = 512,

	MAX_RECS_PER_THREAD    = 100000, /** Records count for each thread */

	MIN_TREE_LOOPS         = 5000,
	MAX_TREE_LOOPS         = 15000,
	MAX_RECS_FOR_TREE_TEST = 100,
};
#endif

/**
 * This unit test exercises the KV operations triggered by multiple streams.
 */
static void ut_multi_stream_kv_oper(void)
{
	void                   *temp_node;
	int                     i;
	time_t                  curr_time;
	struct m0_btree_cb      ut_cb;
	struct m0_be_tx        *tx              = NULL;
	struct m0_be_tx_credit  cred            = {};
	struct m0_btree_op      b_op            = {};
	uint32_t                stream_count    = 0;
	uint64_t                recs_per_stream = 0;
	struct m0_btree_op      kv_op           = {};
	struct m0_btree        *tree;
	const struct node_type *nt              = &fixed_format;
	struct m0_btree_type    btree_type      = {.tt_id = M0_BT_UT_KV_OPS,
						  .ksize = sizeof(uint64_t),
						  .vsize = btree_type.ksize*2,
						  };
	int                     rc;
	struct m0_buf           buf;
	uint64_t                maxkey = 0;
	uint64_t                minkey = UINT64_MAX;
	M0_ENTRY();

	time(&curr_time);
	M0_LOG(M0_INFO, "Using seed %lu", curr_time);
	srandom(curr_time);

	stream_count = (random() % (MAX_STREAM_CNT - MIN_STREAM_CNT)) +
			MIN_STREAM_CNT;

	recs_per_stream = (random()%
			   (MAX_RECS_PER_STREAM - MIN_RECS_PER_STREAM)) +
			    MIN_RECS_PER_STREAM;

	btree_ut_init();
	/**
	 *  Run valid scenario:
	 *  1) Create a btree
	 *  2) Adds records in multiple streams to the created tree.
	 *  3) Confirms the records are present in the tree.
	 *  4) Deletes all the records from the tree using multiple streams.
	 *  5) Close the btree
	 *  6) Destroy the btree
	 *
	 *  Capture each operation in a separate transaction.
	 */

	/** TBD - Replace the following line to call the credit calculator. */
	cred = M0_BE_TX_CREDIT(20, 5 * (1 << 10));
	buf = M0_BUF_INIT((1 << 10), NULL);
	M0_BE_ALLOC_CREDIT_BUF(&buf, seg, &cred);

	/** Allocate and prepare transaction to capture tree operations. */
	M0_ALLOC_PTR(tx);
	M0_ASSERT(tx != NULL);
	m0_be_ut_tx_init(tx, ut_be);
	m0_be_tx_prep(tx, &cred);
	rc = m0_be_tx_open_sync(tx);
	M0_ASSERT(rc == 0);

	/** Create temp node space and use it as root node for btree */
	M0_BE_ALLOC_ALIGN_BUF_SYNC(&buf, 10, seg, tx);
	temp_node = buf.b_addr;
	M0_BTREE_OP_SYNC_WITH_RC(&b_op, m0_btree_create(temp_node, 1024,
							&btree_type, nt,
							&b_op, seg, tx));
	m0_be_tx_close_sync(tx);
	m0_be_tx_fini(tx);

	tree = b_op.bo_arbor;

	/** Dummy credit calculation for PUT operation. Replace when possible.*/
	cred = M0_BE_TX_CREDIT(100, 200 * 1024);

	for (i = 1; i <= recs_per_stream; i++) {
		uint64_t             key;
		uint64_t             value[btree_type.vsize / sizeof(uint64_t)];
		struct cb_data       put_data;
		struct m0_btree_rec  rec;
		m0_bcount_t          ksize  = sizeof key;
		m0_bcount_t          vsize  = sizeof value;
		void                *k_ptr  = &key;
		void                *v_ptr  = &value;
		uint32_t             stream_num;

		for (stream_num = 0; stream_num < stream_count; stream_num++) {
			int k;

			cred = M0_BE_TX_CB_CREDIT(0, 0, 0);
			btree_callback_credit(&cred);

			key = i + (stream_num * recs_per_stream);
			if (key < minkey)
				minkey = key;
			if (key > maxkey)
				maxkey = key;

			key = m0_byteorder_cpu_to_be64(key);
			for (k = 0; k < ARRAY_SIZE(value);k++) {
				value[k] = key;
			}

			rec.r_key.k_data   = M0_BUFVEC_INIT_BUF(&k_ptr, &ksize);
			rec.r_val          = M0_BUFVEC_INIT_BUF(&v_ptr, &vsize);

			put_data.key       = &rec.r_key;
			put_data.value     = &rec.r_val;

			ut_cb.c_act        = btree_kv_put_cb;
			ut_cb.c_datum      = &put_data;

			m0_be_ut_tx_init(tx, ut_be);
			m0_be_tx_prep(tx, &cred);
			rc = m0_be_tx_open_sync(tx);
			M0_ASSERT(rc == 0);

			rc = M0_BTREE_OP_SYNC_WITH_RC(&kv_op,
						      m0_btree_put(tree, &rec,
								   &ut_cb, 0,
								   &kv_op, tx));
			M0_ASSERT(rc == M0_BSC_SUCCESS);
			m0_be_tx_close_sync(tx);
			m0_be_tx_fini(tx);
		}
	}

	{
		/** Min/Max key verification test. */
		uint64_t             key;
		uint64_t             value[btree_type.vsize/sizeof(uint64_t)];
		struct cb_data       get_data;
		struct m0_btree_key  get_key;
		struct m0_bufvec     get_value;
		m0_bcount_t          ksize             = sizeof key;
		m0_bcount_t          vsize             = sizeof value;
		void                *k_ptr             = &key;
		void                *v_ptr             = &value;

		get_key.k_data = M0_BUFVEC_INIT_BUF(&k_ptr, &ksize);
		get_value      = M0_BUFVEC_INIT_BUF(&v_ptr, &vsize);

		get_data.key         = &get_key;
		get_data.value       = &get_value;
		get_data.check_value = true;

		ut_cb.c_act   = btree_kv_get_cb;
		ut_cb.c_datum = &get_data;

		M0_BTREE_OP_SYNC_WITH_RC(&kv_op,
					 m0_btree_minkey(tree, &ut_cb, 0,
							 &kv_op));
		M0_ASSERT(minkey == m0_byteorder_be64_to_cpu(key));

		M0_BTREE_OP_SYNC_WITH_RC(&kv_op,
					 m0_btree_maxkey(tree, &ut_cb, 0,
							 &kv_op));
		M0_ASSERT(maxkey == m0_byteorder_be64_to_cpu(key));
	}

	/**
	 *  Close and Reopen the BE segment and confirm if the records are still
	 *  present.
	 */
	m0_be_ut_seg_reload(ut_seg);

	for (i = 1; i <= (recs_per_stream*stream_count); i++) {
		uint64_t             key;
		uint64_t             value[btree_type.vsize/sizeof(uint64_t)];
		struct cb_data       get_data;
		struct m0_btree_key  get_key;
		struct m0_bufvec     get_value;
		m0_bcount_t          ksize             = sizeof key;
		m0_bcount_t          vsize             = sizeof value;
		void                *k_ptr             = &key;
		void                *v_ptr             = &value;
		uint64_t             find_key;
		void                *find_key_ptr      = &find_key;
		m0_bcount_t          find_key_size     = sizeof find_key;
		struct m0_btree_key  find_key_in_tree;

		find_key = m0_byteorder_cpu_to_be64(i);
		find_key_in_tree.k_data =
			M0_BUFVEC_INIT_BUF(&find_key_ptr, &find_key_size);

		get_key.k_data = M0_BUFVEC_INIT_BUF(&k_ptr, &ksize);
		get_value      = M0_BUFVEC_INIT_BUF(&v_ptr, &vsize);

		get_data.key         = &get_key;
		get_data.value       = &get_value;
		get_data.check_value = true;

		ut_cb.c_act   = btree_kv_get_cb;
		ut_cb.c_datum = &get_data;

		M0_BTREE_OP_SYNC_WITH_RC(&kv_op,
					 m0_btree_get(tree,
						      &find_key_in_tree,
						      &ut_cb, BOF_EQUAL,
						      &kv_op));
	}

	/** Dummy credit calculation for DEL operation. Replace when possible.*/
	cred = M0_BE_TX_CREDIT(20, 100 * 1024);

	for (i = 1; i <= recs_per_stream; i++) {
		uint64_t            del_key;
		struct m0_btree_key del_key_in_tree;
		void                *p_del_key      = &del_key;
		m0_bcount_t         del_key_size    = sizeof del_key;
		struct cb_data      del_data;
		uint32_t            stream_num;

		del_data = (struct cb_data){.key = &del_key_in_tree,
			.value = NULL,
			.check_value = false,
		};

		del_key_in_tree.k_data = M0_BUFVEC_INIT_BUF(&p_del_key,
							    &del_key_size);

		ut_cb.c_act   = btree_kv_del_cb;
		ut_cb.c_datum = &del_data;

		for (stream_num = 0; stream_num < stream_count; stream_num++) {
			cred = M0_BE_TX_CB_CREDIT(0, 0, 0);
			btree_callback_credit(&cred);

			del_key = i + (stream_num * recs_per_stream);
			del_key = m0_byteorder_cpu_to_be64(del_key);

			m0_be_ut_tx_init(tx, ut_be);
			m0_be_tx_prep(tx, &cred);
			rc = m0_be_tx_open_sync(tx);
			M0_ASSERT(rc == 0);

			M0_BTREE_OP_SYNC_WITH_RC(&kv_op,
						 m0_btree_del(tree,
							      &del_key_in_tree,
							      &ut_cb, 0,
							      &kv_op, tx));
			m0_be_tx_close_sync(tx);
			m0_be_tx_fini(tx);
		}
	}


	/** TBD - Replace the following line to call the credit
	 *  calculator. */
	cred = M0_BE_TX_CREDIT(20, 5 * (1 << 10));
	buf = M0_BUF_INIT((1 << 10), NULL);
	M0_BE_ALLOC_CREDIT_BUF(&buf, seg, &cred);

	m0_be_ut_tx_init(tx, ut_be);
	m0_be_tx_prep(tx, &cred);
	rc = m0_be_tx_open_sync(tx);
	M0_ASSERT(rc == 0);

	rc = M0_BTREE_OP_SYNC_WITH_RC(&b_op, m0_btree_destroy(tree, &b_op, tx));
	M0_ASSERT(rc == 0);

	m0_be_tx_close_sync(tx);
	m0_be_tx_fini(tx);

	m0_free0(&tx);
	btree_ut_fini();
}

struct btree_ut_thread_info {
	struct m0_thread   ti_q;             /** Used for thread operations. */
	struct m0_bitmap   ti_cpu_map;       /** CPU map to run this thread. */
	uint64_t           ti_key_first;     /** First Key value to use. */
	uint64_t           ti_key_count;     /** Keys to use. */
	uint64_t           ti_key_incr;      /** Key value to increment by. */
	uint16_t           ti_thread_id;     /** Thread ID <= 65535. */
	struct m0_btree   *ti_tree;          /** Tree for KV operations */
	uint16_t           ti_key_size;      /** Key size in bytes. */
	uint16_t           ti_value_size;    /** Value size in bytes. */
	bool               ti_random_bursts; /** Burstiness in IO pattern. */
	uint64_t           ti_rng_seed_base; /** Base used for RNG seed. */

	/**
	 *  The fields below are used by the thread functions (init and func)
	 *  to share information. These fields should not be touched by thread
	 *  launcher.
	 */
	struct random_data  ti_random_buf;    /** Buffer used by random func. */
	char               *ti_rnd_state_ptr; /** State array used by RNG. */
};

/**
 *  All the threads wait for this variable to turn TRUE.
 *  The main thread sets to TRUE once it has initialized all the threads so
 *  that all the threads start running on different CPU cores and can compete
 *  for the same btree nodes to work on thus exercising possible race
 *  conditions.
 */
static int64_t thread_run;


static struct m0_atomic64 threads_quiesced;
static struct m0_atomic64 threads_running;

#define  UT_START_THREADS()                                                   \
	thread_run = true

#define  UT_STOP_THREADS()                                                    \
	thread_run = false

#define UT_THREAD_WAIT()                                                      \
	do {                                                                  \
		while (!thread_run)                                            \
			;                                                     \
	} while (0)

#define UT_THREAD_QUIESCE_IF_REQUESTED()                                      \
	do {                                                                  \
		if (!thread_run) {                                            \
			m0_atomic64_inc(&threads_quiesced);                   \
			UT_THREAD_WAIT();                                     \
			m0_atomic64_dec(&threads_quiesced);                   \
		}                                                             \
	} while (0)

#define UT_REQUEST_PEER_THREADS_TO_QUIESCE()                                  \
	do {                                                                  \
		bool try_again;                                               \
		do {                                                          \
			try_again = false;                                    \
			if (m0_atomic64_cas(&thread_run, true, false)) {      \
				while (m0_atomic64_get(&threads_quiesced) <    \
				      m0_atomic64_get(&threads_running) - 1)  \
					;                                     \
			} else {                                              \
				UT_THREAD_QUIESCE_IF_REQUESTED();             \
				try_again = true;                             \
			}                                                     \
		} while (try_again);                                          \
	} while (0)

/**
 * Thread init function which will do basic setup such as setting CPU affinity
 * and initializing the RND seed for the thread. Any other initialization that
 * might be needed such as resource allocation/initialization for the thread
 * handler function can also be done here.
 */
static int btree_ut_thread_init(struct btree_ut_thread_info *ti)
{
	M0_ALLOC_ARR(ti->ti_rnd_state_ptr, 64);
	if (ti->ti_rnd_state_ptr == NULL) {
		return -ENOMEM;
	}

	M0_SET0(&ti->ti_random_buf);
	initstate_r(ti->ti_rng_seed_base, ti->ti_rnd_state_ptr, 64,
		    &ti->ti_random_buf);

	srandom_r(ti->ti_thread_id + 1, &ti->ti_random_buf);

	return m0_thread_confine(&ti->ti_q, &ti->ti_cpu_map);
}

/**
 * This routine is a thread handler which launches PUT, GET, ITER, SLANT and DEL
 * operations on the btree passed as parameter.
 */
static void btree_ut_kv_oper_thread_handler(struct btree_ut_thread_info *ti)
{
	uint64_t                key[ti->ti_key_size / sizeof(uint64_t)];
	uint64_t                value[ti->ti_value_size / sizeof(uint64_t)];
	m0_bcount_t             ksize         = sizeof key;
	m0_bcount_t             vsize         = sizeof value;
	void                   *k_ptr         = &key;
	void                   *v_ptr         = &value;
	struct m0_btree_rec     rec;
	struct m0_btree_cb      ut_cb;
	struct cb_data          data;

	uint64_t                get_key[ti->ti_key_size / sizeof(uint64_t)];
	uint64_t                get_value[ti->ti_value_size / sizeof(uint64_t)];
	m0_bcount_t             get_ksize     = sizeof get_key;
	m0_bcount_t             get_vsize     = sizeof get_value;
	void                   *get_k_ptr     = &get_key;
	void                   *get_v_ptr     = &get_value;
	struct m0_btree_rec     get_rec;
	struct m0_btree_cb      ut_get_cb;
	struct cb_data          get_data;

	uint64_t                key_iter_start;
	uint64_t                key_end;
	struct m0_btree_op      kv_op     = {};
	struct m0_btree        *tree;
	struct m0_be_tx        *tx        = NULL;
	struct m0_be_tx_credit  cred      = {};

	/**
	 *  Currently our thread routine only supports Keys and Values which are
	 *  a multiple of 8 bytes.
	 */
	M0_ASSERT(ti->ti_key_size % sizeof(uint64_t) == 0);
	M0_ASSERT(ti->ti_value_size % sizeof(uint64_t) == 0);

	/** Prepare transaction to capture tree operations. */
	m0_be_tx_init(tx, 0, NULL, NULL, NULL, NULL, NULL, NULL);
	m0_be_tx_prep(tx, NULL);

	key_iter_start = ti->ti_key_first;
	key_end        = ti->ti_key_first +
			 (ti->ti_key_count * ti->ti_key_incr) - ti->ti_key_incr;

	rec.r_key.k_data   = M0_BUFVEC_INIT_BUF(&k_ptr, &ksize);
	rec.r_val          = M0_BUFVEC_INIT_BUF(&v_ptr, &vsize);

	data.key           = &rec.r_key;
	data.value         = &rec.r_val;

	ut_cb.c_act        = btree_kv_put_cb;
	ut_cb.c_datum      = &data;

	get_rec.r_key.k_data   = M0_BUFVEC_INIT_BUF(&get_k_ptr, &get_ksize);
	get_rec.r_val          = M0_BUFVEC_INIT_BUF(&get_v_ptr, &get_vsize);

	get_data.key           = &get_rec.r_key;
	get_data.value         = &get_rec.r_val;
	get_data.check_value   = true;

	ut_get_cb.c_act        = btree_kv_get_cb;
	ut_get_cb.c_datum      = &get_data;

	tree                   = ti->ti_tree;

	/** Wait till all the threads have been initialised. */
	UT_THREAD_WAIT();
	m0_atomic64_inc(&threads_running);

	while (key_iter_start <= key_end) {
		uint64_t  key_first;
		uint64_t  key_last;
		uint64_t  keys_put_count = 0;
		uint64_t  keys_found_count = 0;
		int       i;
		int32_t   r;
		uint64_t  iter_dir;
		uint64_t  del_key;
		int       rc;

		key_first = key_iter_start;
		if (ti->ti_random_bursts) {
			random_r(&ti->ti_random_buf, &r);
			if (key_first == key_end)
				key_last = key_end;
			else
				key_last = (r % (key_end - key_first)) +
					   key_first;
			key_last = (key_last / ti->ti_key_incr) *
				   ti->ti_key_incr + ti->ti_key_first;
		} else
			key_last = key_end;

		/** PUT keys and their corresponding values in the tree. */

		ut_cb.c_act   = btree_kv_put_cb;
		ut_cb.c_datum = &data;

		while (key_first <= key_last) {
			/**
			 *  Embed the thread-id in LSB so that different threads
			 *  will target the same node thus causing race
			 *  conditions useful to mimic and test btree operations
			 *  in a loaded system.
			 */
			key[0] = (key_first << (sizeof(ti->ti_thread_id) * 8)) +
				 ti->ti_thread_id;
			key[0] = m0_byteorder_cpu_to_be64(key[0]);
			for (i = 1; i < ARRAY_SIZE(key); i++)
				key[i] = key[0];

			value[0] = key[0];
			for (i = 1; i < ARRAY_SIZE(value); i++)
				value[i] = value[0];

			cred = M0_BE_TX_CB_CREDIT(0, 0, 0);
			btree_callback_credit(&cred);

			rc = M0_BTREE_OP_SYNC_WITH_RC(&kv_op,
						      m0_btree_put(tree, &rec,
								   &ut_cb, 0,
								   &kv_op, tx));
			M0_ASSERT(rc == M0_BSC_SUCCESS &&
				  data.flags == M0_BSC_SUCCESS);

			keys_put_count++;
			key_first += ti->ti_key_incr;

			UT_THREAD_QUIESCE_IF_REQUESTED();
		}

		/**
		 * Execute one error case where we PUT a key which already
		 * exists in the btree.
		 */
		key_first = key_iter_start;
		if ((key_last - key_first) > (ti->ti_key_incr * 2))
			key_first += ti->ti_key_incr;
		key[0] = (key_first << (sizeof(ti->ti_thread_id) * 8)) +
			 ti->ti_thread_id;
		key[0] = m0_byteorder_cpu_to_be64(key[0]);
		for (i = 1; i < ARRAY_SIZE(key); i++)
			key[i] = key[0];
		/** Skip initializing the value as this is an error case */

		cred = M0_BE_TX_CB_CREDIT(0, 0, 0);
		btree_callback_credit(&cred);

		rc = M0_BTREE_OP_SYNC_WITH_RC(&kv_op,
					      m0_btree_put(tree, &rec, &ut_cb,
							   0, &kv_op, tx));
		M0_ASSERT(rc == M0_BSC_KEY_EXISTS);

		/** Modify at least 20% of the values which have been inserted. */

		key_first     = key_iter_start;
		ut_cb.c_act   = btree_kv_update_cb;
		ut_cb.c_datum = &data;

		while (key_first <= key_last) {
			/**
			 *  Embed the thread-id in LSB so that different threads
			 *  will target the same node thus causing race
			 *  conditions useful to mimic and test btree operations
			 *  in a loaded system.
			 */
			key[0] = (key_first << (sizeof(ti->ti_thread_id) * 8)) +
				 ti->ti_thread_id;
			key[0] = m0_byteorder_cpu_to_be64(key[0]);
			for (i = 1; i < ARRAY_SIZE(key); i++)
				key[i] = key[0];

			value[0] = ~key[0];
			for (i = 1; i < ARRAY_SIZE(value); i++)
				value[i] = value[0];

			cred = M0_BE_TX_CB_CREDIT(0, 0, 0);
			btree_callback_credit(&cred);

			rc = M0_BTREE_OP_SYNC_WITH_RC(&kv_op,
						      m0_btree_update(tree,
								      &rec,
								      &ut_cb, 0,
								      &kv_op,
								      tx));
			M0_ASSERT(rc == M0_BSC_SUCCESS &&
				  data.flags == M0_BSC_SUCCESS);

			key_first += (ti->ti_key_incr * 5);
		}

		/**
		 * Execute one error case where we UPDATE a key that does not
		 * exist in the btree.
		 */
		key_first = key_iter_start;
		key[0] = (key_first << (sizeof(ti->ti_thread_id) * 8)) +
			 (typeof(ti->ti_thread_id))-1;
		key[0] = m0_byteorder_cpu_to_be64(key[0]);
		for (i = 1; i < ARRAY_SIZE(key); i++)
			key[i] = key[0];
		/** Skip initializing the value as this is an error case */

		cred = M0_BE_TX_CB_CREDIT(0, 0, 0);
		btree_callback_credit(&cred);

		rc = M0_BTREE_OP_SYNC_WITH_RC(&kv_op,
					      m0_btree_update(tree, &rec,
							      &ut_cb, 0,
							      &kv_op, tx));
		M0_ASSERT(rc == M0_BSC_KEY_NOT_FOUND);


		/** GET and ITERATE over the keys which we inserted above. */

		/**  Randomly decide the iteration direction. */
		random_r(&ti->ti_random_buf, &r);

		key_first = key_iter_start;
		if (r % 2) {
			/** Iterate forward. */
			iter_dir = BOF_NEXT;
			key[0] = (key_first <<
				  (sizeof(ti->ti_thread_id) * 8)) +
				 ti->ti_thread_id;
			key[0] = m0_byteorder_cpu_to_be64(key[0]);
			for (i = 1; i < ARRAY_SIZE(key); i++)
				key[i] = key[0];
		} else {
			/** Iterate backward. */
			iter_dir = BOF_PREV;
			key[0] = (key_last <<
				  (sizeof(ti->ti_thread_id) * 8)) +
				 ti->ti_thread_id;
			key[0] = m0_byteorder_cpu_to_be64(key[0]);
			for (i = 1; i < ARRAY_SIZE(key); i++)
				key[i] = key[0];
		}

		get_data.check_value = true; /** Compare value with key */

		M0_BTREE_OP_SYNC_WITH_RC(&kv_op,
					 m0_btree_get(tree,
						      &rec.r_key, &ut_get_cb,
						      BOF_EQUAL, &kv_op));
		M0_ASSERT(get_data.flags == M0_BSC_SUCCESS);

		keys_found_count++;

		while (1) {
			M0_BTREE_OP_SYNC_WITH_RC(&kv_op,
						 m0_btree_iter(tree,
							       &rec.r_key,
							       &ut_get_cb,
							       iter_dir,
							       &kv_op));
			if (get_data.flags == M0_BSC_KEY_BTREE_BOUNDARY)
				break;

			keys_found_count++;

			/** Copy over the gotten key for the next search. */
			for (i = 0; i < ARRAY_SIZE(key); i++)
				key[i] = get_key[i];

			UT_THREAD_QUIESCE_IF_REQUESTED();
		}

		/**
		 * For single thread, keys_found_count should be equal to
		 * keys_put_count. But for multi-thread, multiple threads can
		 * put records, hence keys_found_count will be greater than
		 * keys_put_count.
		 */
		M0_ASSERT(keys_found_count >= keys_put_count);

		/**
		 * Test for MIN and MAX keys.
		 * Testing is difficult since multiple threads will be adding
		 * or deleting keys from the btree at any time. To get around
		 * this we first quiesce all the threads and then work with
		 * the current btree to find out the MIN and the MAX values.
		 * To confirm if the values are MIN and MAX we will iterate
		 * the PREV and NEXT values for both MIN key and MAX key.
		 * In case of MIN key the PREV iterator should FAIL but NEXT
		 * iterator should succeed. Conversely for MAX key the PREV
		 * iterator should succeed while the NEXT iterator should fail.
		 */
		UT_REQUEST_PEER_THREADS_TO_QUIESCE();

		/**
		 * Fill a value in the buffer which we know cannot be the
		 * MIN key
		 */
		get_key[0] = key_last + 1;

		M0_BTREE_OP_SYNC_WITH_RC(&kv_op,
					 m0_btree_minkey(tree, &ut_get_cb, 0,
							 &kv_op));
		M0_ASSERT(get_data.flags == M0_BSC_SUCCESS);

		for (i = 0; i < ARRAY_SIZE(key); i += sizeof(key[0]))
			key[i] = get_key[i];

		M0_BTREE_OP_SYNC_WITH_RC(&kv_op,
					 m0_btree_iter(tree, &rec.r_key,
						       &ut_get_cb, BOF_NEXT,
						       &kv_op));
		M0_ASSERT((get_data.flags == M0_BSC_SUCCESS) ||
			  (get_data.flags == M0_BSC_KEY_BTREE_BOUNDARY &&
			   key_iter_start == key_last));
		/**
		 * The second condition in the above assert is rare but can
		 * happen if only one Key is present in the btree. We presume
		 * that no Keys from other other threads are currently present
		 * in the btree and also the current thread  added just one
		 * key in this iteration.
		 */

		for (i = 0; i < ARRAY_SIZE(key); i += sizeof(key[0]))
			get_key[i] = key[i];

		M0_BTREE_OP_SYNC_WITH_RC(&kv_op,
					 m0_btree_iter(tree, &rec.r_key,
						       &ut_get_cb, BOF_PREV,
						       &kv_op));
		M0_ASSERT(get_data.flags == M0_BSC_KEY_BTREE_BOUNDARY);

		get_key[0] = key_iter_start - 1;

		M0_BTREE_OP_SYNC_WITH_RC(&kv_op,
					 m0_btree_maxkey(tree, &ut_get_cb, 0,
							 &kv_op));
		M0_ASSERT(get_data.flags == M0_BSC_SUCCESS);

		for (i = 0; i < ARRAY_SIZE(key); i += sizeof(key[0]))
			key[i] = get_key[i];

		M0_BTREE_OP_SYNC_WITH_RC(&kv_op,
					 m0_btree_iter(tree, &rec.r_key,
						       &ut_get_cb, BOF_PREV,
						       &kv_op));
		M0_ASSERT((get_data.flags == M0_BSC_SUCCESS) ||
			  (get_data.flags == M0_BSC_KEY_BTREE_BOUNDARY &&
			   key_iter_start == key_last));

		for (i = 0; i < ARRAY_SIZE(key); i += sizeof(key[0]))
			get_key[i] = key[i];

		M0_BTREE_OP_SYNC_WITH_RC(&kv_op,
					 m0_btree_iter(tree, &rec.r_key,
						       &ut_get_cb, BOF_NEXT,
						       &kv_op));
		M0_ASSERT(get_data.flags == M0_BSC_KEY_BTREE_BOUNDARY);

		UT_START_THREADS();

		/**
		 *  Test slant only if possible. If the increment counter is
		 *  more than 1 we can provide the intermediate value to be got
		 *  in slant mode.
		 */

		if (ti->ti_key_incr > 1) {
			uint64_t  slant_key;
			uint64_t  got_key;
			struct m0_btree_rec r;
			struct m0_btree_cb  cb;

			M0_ASSERT(key_first >= 1);

			slant_key = key_first - 1;
			get_data.check_value = false;

			/**
			 *  The following short named variables are used just
			 *  to maintain the code decorum by limiting code lines
			 *  within 80 chars..
			 */
			r = rec;
			cb = ut_get_cb;

			do {
				key[0] = (slant_key <<
					  (sizeof(ti->ti_thread_id) * 8)) +
					 ti->ti_thread_id;
				key[0] = m0_byteorder_cpu_to_be64(key[0]);
				for (i = 1; i < ARRAY_SIZE(key); i++)
					key[i] = key[0];

				M0_BTREE_OP_SYNC_WITH_RC(&kv_op,
							 m0_btree_get(tree,
								      &r.r_key,
								      &cb,
								      BOF_SLANT,
								      &kv_op));

				/**
				 *  If multiple threads are running then slant
				 *  could return us the value which was added
				 *  by a different thread. We anyways make sure
				 *  that the got value (without the embedded
				 *  thread ID) is more than the slant value.
				 */
				got_key = m0_byteorder_cpu_to_be64(get_key[0]);
				got_key >>= (sizeof(ti->ti_thread_id) * 8);
				M0_ASSERT(got_key == slant_key + 1);

				slant_key += ti->ti_key_incr;

				UT_THREAD_QUIESCE_IF_REQUESTED();
			} while (slant_key <= key_last);
		}

		/**
		 *  DEL the keys which we had created in this iteration. The
		 *  direction of traversing the delete keys is randomly
		 *  selected.
		 */
		random_r(&ti->ti_random_buf, &r);

		key_first = key_iter_start;
		del_key = (r % 2 == 0) ? key_first : key_last;

		ut_cb.c_act   = btree_kv_del_cb;
		ut_cb.c_datum = &data;
		while (keys_put_count) {
			key[0] = (del_key << (sizeof(ti->ti_thread_id) * 8)) +
				 ti->ti_thread_id;
			key[0] = m0_byteorder_cpu_to_be64(key[0]);
			for (i = 1; i < ARRAY_SIZE(key); i++)
				key[i] = key[0];

			cred = M0_BE_TX_CB_CREDIT(0, 0, 0);
			btree_callback_credit(&cred);

			M0_BTREE_OP_SYNC_WITH_RC(&kv_op,
						 m0_btree_del(tree, &rec.r_key,
							      &ut_cb, 0,
							      &kv_op, tx));
			del_key = (r % 2 == 0) ?
						del_key + ti->ti_key_incr :
						del_key - ti->ti_key_incr;
			keys_put_count--;

			UT_THREAD_QUIESCE_IF_REQUESTED();
		}

		key_iter_start = key_last + ti->ti_key_incr;

		UT_THREAD_QUIESCE_IF_REQUESTED();
	}

	m0_atomic64_dec(&threads_running);
	/** Free resources. */
	m0_free(ti->ti_rnd_state_ptr);
}

/**
 * This function allocates an array pointed by cpuid_ptr and fills it with the
 * CPU ID of the CPUs which are currently online.
 */
static void online_cpu_id_get(uint16_t **cpuid_ptr, uint16_t *cpu_count)
{
	size_t           cpu_max;
	uint32_t         cpuid;
	struct m0_bitmap map_cpu_online  = {};
	int              rc;

	*cpu_count = 0;
	cpu_max = m0_processor_nr_max();
	rc = m0_bitmap_init(&map_cpu_online, cpu_max);
	if (rc != 0)
		return;

	m0_processors_online(&map_cpu_online);

	for (cpuid = 0; cpuid < map_cpu_online.b_nr; cpuid++) {
		if (m0_bitmap_get(&map_cpu_online, cpuid)) {
			(*cpu_count)++;
		}
	}

	if (*cpu_count) {
		M0_ALLOC_ARR(*cpuid_ptr, *cpu_count);
		M0_ASSERT(*cpuid_ptr != NULL);

		*cpu_count = 0;
		for (cpuid = 0; cpuid < map_cpu_online.b_nr; cpuid++) {
			if (m0_bitmap_get(&map_cpu_online, cpuid)) {
				(*cpuid_ptr)[*cpu_count] = cpuid;
				(*cpu_count)++;
			}
		}
	}
}


/**
 * This test launches multiple threads which launch KV operations against one
 * btree in parallel. If thread_count is passed as '0' then one thread per core
 * is launched. If tree_count is passed as '0' then one tree per thread is
 * created.
 */
static void btree_ut_num_threads_num_trees_kv_oper(int32_t thread_count,
						   int32_t tree_count)
{
	int                           rc;
	struct btree_ut_thread_info  *ti;
	int                           i;
	struct m0_btree             **ut_trees;
	uint16_t                      cpu;
	void                         *temp_node;
	struct m0_btree_op            b_op         = {};
	struct m0_be_tx              *tx           = NULL;
	struct m0_be_seg             *seg          = NULL;
	const struct node_type       *nt           = &fixed_format;
	const uint32_t                ksize_to_use = sizeof(uint64_t);
	struct m0_btree_type          btree_type   = {.tt_id = M0_BT_UT_KV_OPS,
						      .ksize = ksize_to_use,
						      .vsize = ksize_to_use*2,
						     };
	uint16_t                     *cpuid_ptr;
	uint16_t                      cpu_count;
	size_t                        cpu_max;
	time_t                        curr_time;

	M0_ENTRY();

	time(&curr_time);
	M0_LOG(M0_INFO, "Using seed %lu", curr_time);
	srandom(curr_time);

	/**
	 *  1) Create btree(s) to be used by all the threads.
	 *  2) Assign CPU cores to the threads.
	 *  3) Init and Start the threads which do KV operations.
	 *  4) Wait till all the threads are done.
	 *  5) Close the btree
	 *  6) Destroy the btree
	 */

	/** Prepare transaction to capture tree operations. */
	m0_be_tx_init(tx, 0, NULL, NULL, NULL, NULL, NULL, NULL);
	m0_be_tx_prep(tx, NULL);
	btree_ut_init();

	online_cpu_id_get(&cpuid_ptr, &cpu_count);

	if (thread_count == 0)
		thread_count = cpu_count - 1; /** Skip Core-0 */
	else if (thread_count == RANDOM_THREAD_COUNT) {
		thread_count = 1;
		if (cpu_count > 2) {
			/**
			 *  Avoid the extreme cases i.e. thread_count
			 *  cannot be 1 or cpu_count - 1
			 */
			thread_count = (random() % (cpu_count - 2)) + 1;
		}
	}

	if (tree_count == 0)
		tree_count = thread_count;
	else if (tree_count == RANDOM_THREAD_COUNT) {
		tree_count = 1;
		if (thread_count > 2) {
			/**
			 *  Avoid the extreme cases i.e. tree_count
			 *  cannot be 1 or thread_count
			 */
			tree_count = (random() % (thread_count - 1)) + 1;
		}
	}

	M0_ASSERT(thread_count >= tree_count);

	UT_STOP_THREADS();
	m0_atomic64_set(&threads_running, 0);
	m0_atomic64_set(&threads_quiesced, 0);

	M0_ALLOC_ARR(ut_trees, tree_count);
	M0_ASSERT(ut_trees != NULL);

	for (i = 0; i < tree_count; i++) {
		M0_SET0(&b_op);

		/** Create temp node space and use it as root node for btree */
		temp_node = m0_alloc_aligned((1024 + sizeof(struct nd)), 10);

		M0_BTREE_OP_SYNC_WITH_RC(&b_op,
					 m0_btree_create(temp_node, 1024,
							 &btree_type, nt, &b_op,
							 seg, tx));

		ut_trees[i] = b_op.bo_arbor;
	}

	m0_be_tx_close(tx);
	m0_be_tx_fini(tx);

	M0_ALLOC_ARR(ti, thread_count);
	M0_ASSERT(ti != NULL);

	cpu_max = m0_processor_nr_max();

	cpu = 1; /** We skip Core-0 for Linux kernel and other processes. */
	for (i = 0; i < thread_count; i++) {
		rc = m0_bitmap_init(&ti[i].ti_cpu_map, cpu_max);
		m0_bitmap_set(&ti[i].ti_cpu_map, cpuid_ptr[cpu], true);
		cpu++;
		if (cpu >= cpu_count)
			/**
			 *  Circle around if thread count is higher than the
			 *  CPU cores in the system.
			 */
			cpu = 1;

		ti[i].ti_key_first  = 1;
		ti[i].ti_key_count  = MAX_RECS_PER_THREAD;
		ti[i].ti_key_incr   = 5;
		ti[i].ti_thread_id  = i;
		ti[i].ti_tree       = ut_trees[i % tree_count];
		ti[i].ti_key_size   = btree_type.ksize;
		ti[i].ti_value_size = btree_type.vsize;
		ti[i].ti_random_bursts = (thread_count > 1);
		do {
			ti[i].ti_rng_seed_base = random();
		} while (ti[i].ti_rng_seed_base == 0);
	}

	for (i = 0; i < thread_count; i++) {
		rc = M0_THREAD_INIT(&ti[i].ti_q, struct btree_ut_thread_info *,
				    btree_ut_thread_init,
				    &btree_ut_kv_oper_thread_handler, &ti[i],
				    "Thread-%d", i);
		M0_ASSERT(rc == 0);
	}

	/** Initialized all the threads by now. Let's get rolling ... */
	UT_START_THREADS();

	for (i = 0; i < thread_count;i++) {
		m0_thread_join(&ti[i].ti_q);
		m0_thread_fini(&ti[i].ti_q);
	}

	for (i = 0; i < tree_count; i++) {
		rc = M0_BTREE_OP_SYNC_WITH_RC(&b_op,
					      m0_btree_destroy(ut_trees[i],
							       &b_op, tx));
		M0_ASSERT(rc == 0);
	}

	m0_free(ut_trees);


	m0_free(ti);
	btree_ut_fini();

	M0_LEAVE();
}

static void ut_st_st_kv_oper(void)
{
	btree_ut_num_threads_num_trees_kv_oper(1, 1);
}

static void ut_mt_st_kv_oper(void)
{
	btree_ut_num_threads_num_trees_kv_oper(0, 1);
}

static void ut_mt_mt_kv_oper(void)
{
	btree_ut_num_threads_num_trees_kv_oper(0, 0);
}

static void ut_rt_rt_kv_oper(void)
{
	btree_ut_num_threads_num_trees_kv_oper(RANDOM_THREAD_COUNT,
					       RANDOM_TREE_COUNT);
}

/**
 * This routine is a thread handler which primarily involves in creating,
 * opening, closing and destroying btree. To run out-of-sync with other threads
 * it also launches PUT, GET, ITER and DEL operations on the btree for a random
 * count.
 */
static void btree_ut_tree_oper_thread_handler(struct btree_ut_thread_info *ti)
{
	uint64_t               key;
	uint64_t               value;
	m0_bcount_t            ksize = sizeof key;
	m0_bcount_t            vsize = sizeof value;
	void                  *k_ptr = &key;
	void                  *v_ptr = &value;
	struct m0_btree_rec    rec   = {
				     .r_key.k_data = M0_BUFVEC_INIT_BUF(&k_ptr,
									&ksize),
				     .r_val        = M0_BUFVEC_INIT_BUF(&v_ptr,
									&vsize),
				     .r_flags      = 0,
				     };
	struct cb_data         data  = {
					.key         = &rec.r_key,
					.value       = &rec.r_val,
					.check_value = false,
					.flags       = 0,
				       };
	struct m0_btree_cb     ut_cb   = {
					  .c_act       = btree_kv_put_cb,
					  .c_datum     = &data,
					 };
	int32_t                loop_count;
	struct m0_btree_op     kv_op     = {};
	void                  *temp_node;
	struct m0_btree_type   btree_type  = {.tt_id = M0_BT_UT_KV_OPS,
					      .ksize = sizeof(key),
					      .vsize = sizeof(value),
					     };
	const struct node_type *nt         = &fixed_format;
	struct m0_be_tx        *tx         = NULL;
	struct m0_be_seg       *seg        = NULL;
	struct m0_be_tx_credit  cred       = {};
	int                     rc;

	random_r(&ti->ti_random_buf, &loop_count);
	loop_count %= (MAX_TREE_LOOPS - MIN_TREE_LOOPS);
	loop_count += MIN_TREE_LOOPS;

	UT_THREAD_WAIT();
	m0_atomic64_inc(&threads_running);

	/** Create temp node space and use it as root node for btree */
	temp_node = m0_alloc_aligned((1024 + sizeof(struct nd)), 10);

	while (loop_count--) {
		struct m0_btree_op  b_op        = {};
		struct m0_btree    *tree;
		int32_t             rec_count;
		uint32_t            i;

		/**
		 * 1) Create a tree
		 * 2) Add a few random count of records in the tree.
		 * 3) Close the tree
		 * 4) Open the tree
		 * 5) Confirm the records are present in the tree.
		 * 6) Close the tree
		 * 4) Open the tree
		 * 5) Delete all the records from the tree.
		 * 6) Close the tree
		 * 7) Destroy the tree
		 */
		rc = M0_BTREE_OP_SYNC_WITH_RC(&b_op,
					      m0_btree_create(temp_node, 1024,
							      &btree_type, nt,
							      &b_op, seg, tx));
		M0_ASSERT(rc == 0);
		tree = b_op.bo_arbor;

		random_r(&ti->ti_random_buf, &rec_count);
		rec_count %= MAX_RECS_FOR_TREE_TEST;
		rec_count = rec_count ? : (MAX_RECS_FOR_TREE_TEST / 2);

		ut_cb.c_act = btree_kv_put_cb;
		for (i = 1; i <= rec_count; i++) {
			value = key = i;

			cred = M0_BE_TX_CB_CREDIT(0, 0, 0);
			btree_callback_credit(&cred);

			rc = M0_BTREE_OP_SYNC_WITH_RC(&kv_op,
						      m0_btree_put(tree, &rec,
								   &ut_cb, 0,
								   &kv_op, tx));
			M0_ASSERT(data.flags == M0_BSC_SUCCESS &&
				  rc == M0_BSC_SUCCESS);
		}

		rc = M0_BTREE_OP_SYNC_WITH_RC(&b_op,
					      m0_btree_close(tree, &b_op));
		M0_ASSERT(rc == 0);

		rc = M0_BTREE_OP_SYNC_WITH_RC(&b_op,
					      m0_btree_open(temp_node,
							    1024, &tree,
							    &b_op));
		M0_ASSERT(rc == 0);

		ut_cb.c_act = btree_kv_get_cb;
		for (i = 1; i <= rec_count; i++) {
			value = key = i;

			rc = M0_BTREE_OP_SYNC_WITH_RC(&kv_op,
						      m0_btree_get(tree,
								   &rec.r_key,
								   &ut_cb,
								   BOF_EQUAL,
								   &kv_op));
			M0_ASSERT(data.flags == M0_BSC_SUCCESS && rc == 0);
		}

		rc = M0_BTREE_OP_SYNC_WITH_RC(&b_op,
					      m0_btree_close(tree, &b_op));
		M0_ASSERT(rc == 0);

		rc = M0_BTREE_OP_SYNC_WITH_RC(&b_op,
					      m0_btree_open(temp_node,
							    1024, &tree,
							    &b_op));
		M0_ASSERT(rc == 0);

		ut_cb.c_act = btree_kv_del_cb;
		for (i = 1; i <= rec_count; i++) {
			value = key = i;

			cred = M0_BE_TX_CB_CREDIT(0, 0, 0);
			btree_callback_credit(&cred);

			rc = M0_BTREE_OP_SYNC_WITH_RC(&kv_op,
						      m0_btree_del(tree,
								   &rec.r_key,
								   &ut_cb, 0,
								   &kv_op, tx));
			M0_ASSERT(data.flags == M0_BSC_SUCCESS && rc == 0);
		}

		rc = M0_BTREE_OP_SYNC_WITH_RC(&b_op,
					      m0_btree_destroy(tree,
							       &b_op, tx));
		M0_ASSERT(rc == 0);
	}

	m0_atomic64_dec(&threads_running);
	m0_free_aligned(temp_node, (1024 + sizeof(struct nd)), 10);
}

static void btree_ut_num_threads_tree_oper(uint32_t thread_count)
{
	uint16_t                    *cpuid_ptr;
	uint16_t                     cpu_count;
	size_t                       cpu_max;
	struct btree_ut_thread_info *ti;
	uint16_t                     cpu;
	int                          i;
	int                          rc;

	btree_ut_init();
	online_cpu_id_get(&cpuid_ptr, &cpu_count);

	if (thread_count == 0)
		thread_count = cpu_count - 1; /** Skip Core-0 */

	UT_STOP_THREADS();
	m0_atomic64_set(&threads_running, 0);
	m0_atomic64_set(&threads_quiesced, 0);

	M0_ALLOC_ARR(ti, thread_count);
	M0_ASSERT(ti != NULL);

	cpu_max = m0_processor_nr_max();

	cpu = 1; /** We skip Core-0 for Linux kernel and other processes. */
	for (i = 0; i < thread_count; i++) {
		rc = m0_bitmap_init(&ti[i].ti_cpu_map, cpu_max);
		m0_bitmap_set(&ti[i].ti_cpu_map, cpuid_ptr[cpu], true);
		cpu++;
		if (cpu >= cpu_count)
			/**
			 *  Circle around if thread count is higher than the
			 *  CPU cores in the system.
			 */
			cpu = 1;

		ti[i].ti_thread_id  = i;
	}

	for (i = 0; i < thread_count; i++) {
		rc = M0_THREAD_INIT(&ti[i].ti_q, struct btree_ut_thread_info *,
				    btree_ut_thread_init,
				    &btree_ut_tree_oper_thread_handler, &ti[i],
				    "Thread-%d", i);
		M0_ASSERT(rc == 0);
	}

	/** Initialized all the threads. Now start the chaos ... */
	UT_START_THREADS();

	for (i = 0; i < thread_count; i++) {
		m0_thread_join(&ti[i].ti_q);
		m0_thread_fini(&ti[i].ti_q);
	}

	m0_free(ti);
	btree_ut_fini();
}

static void ut_st_tree_oper(void)
{
	btree_ut_num_threads_tree_oper(1);
}

static void ut_mt_tree_oper(void)
{
	btree_ut_num_threads_tree_oper(0);
}

/**
 * Commenting this ut as it is not required as a part for test-suite but my
 * required for testing purpose
**/
#if 0
/**
 * This function is for traversal of tree in breadth-first order and it will
 * print level and key-value pair for each node.
 */
static void ut_traversal(struct td *tree)
{
	struct nd *root = tree->t_root;
	struct nd *queue[1000000];
	int front = 0, rear = 0;
	queue[front] = root;

        int count = 0;
	int lev = -1;

	while (front != -1 && rear != -1)
	{
		//pop one elemet
		struct nd* element = queue[front];
		if (front == rear) {
			front = -1;
			rear = -1;
		} else {
			front++;
		}
		printf("\n");
		int level = node_level(element);
		if (level > 0) {
			printf("level : %d =>    ", level);
			if (level != lev)
			{
				lev = level;
				count =0;

			}
			printf("count : %d =>\n", count++);
			int total_count = node_count(element);
			int j;
			for (j=0 ; j < total_count; j++)
			{
				uint64_t key = 0;
				get_key_at_index(element, j, &key);

				key = m0_byteorder_be64_to_cpu(key);
				printf("%"PRIu64"\t", key);

				struct segaddr child_node_addr;
				struct slot    node_slot = {};
				node_slot.s_node = element;

				node_slot.s_idx = j;
				node_child(&node_slot, &child_node_addr);
				struct node_op  i_nop;
				i_nop.no_opc = NOP_LOAD;
				node_get(&i_nop, tree, &child_node_addr,
					 P_NEXTDOWN);
				if (front == -1) {
					front = 0;
				}
				rear++;
				if (rear == 999999) {
					printf("***********OVERFLOW**********");
					break;
				}
				queue[rear] = i_nop.no_node;
			}
			/* store last child: */
			struct segaddr child_node_addr;
			struct slot    node_slot = {};
			node_slot.s_node = element;

			node_slot.s_idx = j;
			node_child(&node_slot, &child_node_addr);
			struct node_op  i_nop;
			i_nop.no_opc = NOP_LOAD;
			node_get(&i_nop, tree, &child_node_addr, P_NEXTDOWN);
			if (front == -1) {
				front = 0;
			}
			rear++;
			if (rear == 999999) {
				printf("***********OVERFLOW**********");
				break;
			}
			queue[rear] = i_nop.no_node;
			printf("\n\n");
		} else {
			printf("level : %d =>", level);
			if (level != lev)
			{
				lev = level;
				count =0;

			}
			printf("count : %d =>\n", count++);
			int total_count = node_count(element);
			int j;
			for (j=0 ; j < total_count; j++)
			{
				uint64_t key = 0;
				uint64_t val = 0;
				get_rec_at_index(element, j, &key, &val);

				key = m0_byteorder_be64_to_cpu(key);
				val = m0_byteorder_be64_to_cpu(val);
				printf("%"PRIu64",%"PRIu64"\t", key, val);


			}
			printf("\n\n");
		}
		if (element != root) {
			struct node_op  i_nop;
			node_put(&i_nop, element, NULL);
		}
	}
}
/**
 * This function will check if the keys of records present in the nodes at each
 * level are in increasing order or not.
 */
static void ut_invariant_check(struct td *tree)
{
	struct nd *root = tree->t_root;
	struct nd *queue[1000000];
	int front = 0, rear = 0;
	queue[front] = root;
	bool firstkey = true;
	int lev = -1;
	uint64_t prevkey;
	int max_level = node_level(root);
	while (front != -1 && rear != -1)
	{
		struct nd* element = queue[front];
		if (front == rear) {
			front = -1;
			rear = -1;
		} else {
			front++;
		}
		int level = node_level(element);
		if (level > 0) {
			if (level != lev)
			{
				lev = level;
				firstkey = true;
			}
			int total_count = node_count(element);
			if (level == max_level){
				if (element->n_ref > 2){
					printf("***INVARIENT FAIL***");
					M0_ASSERT(0);
				}
			} else {
				if (element->n_ref != 1) {
					printf("***INVARIENT FAIL***");
					M0_ASSERT(0);
				}
			}
			int j;
			for (j=0 ; j < total_count; j++)
			{
				uint64_t key = 0;
				get_key_at_index(element, j, &key);

				key = m0_byteorder_be64_to_cpu(key);
				if (!firstkey) {
					if (key < prevkey) {
						printf("***INVARIENT FAIL***");
						M0_ASSERT(0);
					}
				}
				prevkey = key;
				firstkey = false;
				struct segaddr child_node_addr;
				struct slot    node_slot = {};
				node_slot.s_node = element;

				node_slot.s_idx = j;
				node_child(&node_slot, &child_node_addr);
				struct node_op  i_nop;
				i_nop.no_opc = NOP_LOAD;
				node_get(&i_nop, tree, &child_node_addr,
					 P_NEXTDOWN);
				if (front == -1) {
					front = 0;
				}
				rear++;
				if (rear == 999999) {
					printf("***********OVERFLW***********");
					break;
				}
				queue[rear] = i_nop.no_node;
			}
			/* store last child: */
			struct segaddr child_node_addr;
			struct slot    node_slot = {};
			node_slot.s_node = element;

			node_slot.s_idx = j;
			node_child(&node_slot, &child_node_addr);
			struct node_op  i_nop;
			i_nop.no_opc = NOP_LOAD;
			node_get(&i_nop, tree, &child_node_addr, P_NEXTDOWN);
			if (front == -1) {
				front = 0;
			}
			rear++;
			if (rear == 999999) {
				printf("***********OVERFLW***********");
				break;
			}
			queue[rear] = i_nop.no_node;
		} else {
			if (level != lev)
			{
				lev = level;
				firstkey = true;
			}
			int total_count = node_count(element);
			if (level == max_level){
				if (element->n_ref > 2){
					printf("***INVARIENT FAIL***");
					M0_ASSERT(0);
				}
			} else {
				if (element->n_ref != 1) {
					printf("***INVARIENT FAIL***");
					M0_ASSERT(0);
				}
			}
			int j;
			for (j=0 ; j < total_count; j++)
			{
				uint64_t key = 0;
				uint64_t val = 0;
				get_rec_at_index(element, j, &key, &val);

				key = m0_byteorder_be64_to_cpu(key);
				val = m0_byteorder_be64_to_cpu(val);
				if (!firstkey) {
					if (key < prevkey) {
						printf("***INVARIENT FAIL***");
						M0_ASSERT(0);
					}

				}
				prevkey = key;
				firstkey = false;
			}
		}
		if (element != root) {
			struct node_op  i_nop;
			node_put(&i_nop, element, NULL);
		}
	}
}

/**
 * This ut will put records in the tree then update and delete those records in
 * sequencial manner.
 */
static void ut_put_update_del_operation(void)
{
	struct m0_btree_type    btree_type = {.tt_id = M0_BT_UT_KV_OPS,
					      .ksize = 8,
					      .vsize = 8, };
	struct m0_be_tx        *tx          = NULL;
	struct m0_be_seg       *seg         = NULL;
	struct m0_btree_op      b_op        = {};
	struct m0_btree        *tree;
	void                   *temp_node;
	int                     i;
	struct m0_btree_cb      ut_cb;
	struct m0_btree_op      kv_op                = {};
	const struct node_type *nt                   = &fixed_format;
	int                     total_records        = 1000000;
	bool                    inc;
	M0_ENTRY();

	/** Prepare transaction to capture tree operations. */
	m0_be_tx_init(tx, 0, NULL, NULL, NULL, NULL, NULL, NULL);
	m0_be_tx_prep(tx, NULL);
	btree_ut_init();
	/**
	 *  Run valid scenario:
	 *  1) Create a btree
	 *  2) Adds a few records to the created tree.
	 *  3) Confirms the records are present in the tree.
	 *  4) Deletes all the records from the tree.
	 *  4) Close the btree
	 *  5) Destroy the btree
	 */

	/** Create temp node space and use it as root node for btree */
	temp_node = m0_alloc_aligned((1024 + sizeof(struct nd)), 10);
	M0_BTREE_OP_SYNC_WITH_RC(&b_op,
				 m0_btree_create(temp_node, 1024, &btree_type,
						 nt, &b_op, seg, tx));

	tree = b_op.bo_arbor;
	inc = false;
	for (i = 0; i < 1000000; i++) {
		uint64_t             key;
		uint64_t             value;
		struct cb_data       put_data;
		struct m0_btree_rec  rec;
		m0_bcount_t          ksize  = sizeof key;
		m0_bcount_t          vsize  = sizeof value;
		void                *k_ptr  = &key;
		void                *v_ptr  = &value;

		/**
		 *  There is a very low possibility of hitting the same key
		 *  again. This is fine as it helps debug the code when insert
		 *  is called with the same key instead of update function.
		 */
		int temp = inc ? total_records - i : i;
		key = value = m0_byteorder_cpu_to_be64(temp);

		rec.r_key.k_data   = M0_BUFVEC_INIT_BUF(&k_ptr, &ksize);
		rec.r_val          = M0_BUFVEC_INIT_BUF(&v_ptr, &vsize);

		put_data.key       = &rec.r_key;
		put_data.value     = &rec.r_val;

		ut_cb.c_act        = btree_kv_put_cb;
		ut_cb.c_datum      = &put_data;

		int rc = M0_BTREE_OP_SYNC_WITH_RC(&kv_op,
					 m0_btree_put(tree, &rec, &ut_cb, 0,
						      &kv_op, tx));
		M0_ASSERT(rc == M0_BSC_SUCCESS);

		if (i >= total_records - 5) {
			int rc = M0_BTREE_OP_SYNC_WITH_RC(&kv_op,
							  m0_btree_put(tree,
							  	       &rec,
								       &ut_cb,
								       0,
								       &kv_op,
						      		       tx));
			M0_ASSERT(rc == M0_BSC_KEY_EXISTS);
		}

	}
	printf("level : %d\n", node_level(tree->t_desc->t_root));
	M0_ASSERT(node_expensive_invariant(tree->t_desc->t_root));

	inc = false;
	for (i = 0; i < 1000000; i++) {
		uint64_t             key;
		uint64_t             value;
		struct cb_data       update_data;
		struct m0_btree_rec  rec;
		m0_bcount_t          ksize  = sizeof key;
		m0_bcount_t          vsize  = sizeof value;
		void                *k_ptr  = &key;
		void                *v_ptr  = &value;

		/**
		 *  There is a very low possibility of hitting the same key
		 *  again. This is fine as it helps debug the code when insert
		 *  is called with the same key instead of update function.
		 */
		int temp = inc ? total_records - i : i;
		key = m0_byteorder_cpu_to_be64(temp);
		value = m0_byteorder_cpu_to_be64(temp + 1);

		rec.r_key.k_data   = M0_BUFVEC_INIT_BUF(&k_ptr, &ksize);
		rec.r_val          = M0_BUFVEC_INIT_BUF(&v_ptr, &vsize);

		update_data.key    = &rec.r_key;
		update_data.value  = &rec.r_val;

		ut_cb.c_act        = btree_kv_update_cb;
		ut_cb.c_datum      = &update_data;

		int rc = M0_BTREE_OP_SYNC_WITH_RC(&kv_op,
					 m0_btree_update(tree, &rec, &ut_cb, 0,
							 &kv_op, tx));
		if (rc) {
			M0_ASSERT(rc == M0_BSC_KEY_NOT_FOUND);
			printf("M0_BSC_KEY_NOT_FOUND ");
		}

	}
	printf("level : %d\n", node_level(tree->t_desc->t_root));
	ut_invariant_check(tree->t_desc);
	M0_ASSERT(node_expensive_invariant(tree->t_desc->t_root));

	inc = false;
	for (i = 0; i < 1000000; i++) {
		uint64_t             key;
		uint64_t             value;
		struct cb_data       del_data;
		struct m0_btree_rec  rec;
		m0_bcount_t          ksize  = sizeof key;
		m0_bcount_t          vsize  = sizeof value;
		void                *k_ptr  = &key;
		void                *v_ptr  = &value;

		/**
		 *  There is a very low possibility of hitting the same key
		 *  again. This is fine as it helps debug the code when insert
		 *  is called with the same key instead of update function.
		 */
		int temp = inc ? total_records - i : i;
		key = value = m0_byteorder_cpu_to_be64(temp);

		rec.r_key.k_data   = M0_BUFVEC_INIT_BUF(&k_ptr, &ksize);
		rec.r_val          = M0_BUFVEC_INIT_BUF(&v_ptr, &vsize);

		del_data.key       = &rec.r_key;
		del_data.value     = &rec.r_val;

		ut_cb.c_act        = btree_kv_del_cb;
		ut_cb.c_datum      = &del_data;

		M0_BTREE_OP_SYNC_WITH_RC(&kv_op,
					 m0_btree_del(tree, &rec.r_key,
						      &ut_cb, 0, &kv_op, tx));
		if (del_data.flags == M0_BSC_KEY_NOT_FOUND) {
			printf("M0_BSC_KEY_NOT_FOUND ");
		} else {
			M0_ASSERT(del_data.flags == M0_BSC_SUCCESS);
		}



	}
	printf("\n After deletion:\n");
	ut_traversal(tree->t_desc);
	//m0_btree_close(tree);
	/**
	 * Commenting this code as the delete operation is not done here.
	 * Due to this, the destroy operation will crash.
	 *
	 *
	 * M0_BTREE_OP_SYNC_WITH_RC(&b_op.bo_op,
	 *				 m0_btree_destroy(b_op.bo_arbor, &b_op),
	 *				 &b_op.bo_sm_group, &b_op.bo_op_exec);
	 */
	btree_ut_fini();
}
#endif

static int ut_btree_suite_init(void)
{
	M0_ENTRY();

	M0_ALLOC_PTR(ut_be);
	M0_ASSERT(ut_be != NULL);

	M0_ALLOC_PTR(ut_seg);
	M0_ASSERT(ut_seg != NULL);
	/* Init BE */
	m0_be_ut_backend_init(ut_be);
	m0_be_ut_seg_init(ut_seg, ut_be, 1ULL << 24);
	seg = ut_seg->bus_seg;

	M0_LEAVE();
	return 0;
}

static int ut_btree_suite_fini(void)
{
	M0_ENTRY();

	m0_be_ut_seg_reload(ut_seg);
	m0_be_ut_seg_fini(ut_seg);
	m0_be_ut_backend_fini(ut_be);
	m0_free(ut_seg);
	m0_free(ut_be);

	M0_LEAVE();
	return 0;
}

struct m0_ut_suite btree_ut = {
	.ts_name = "btree-ut",
	.ts_yaml_config_string = "{ valgrind: { timeout: 3600 },"
	"  helgrind: { timeout: 3600 },"
	"  exclude:  ["
	"   "
	"  ] }",
	.ts_init = ut_btree_suite_init,
	.ts_fini = ut_btree_suite_fini,
	.ts_tests = {
		{"basic_tree_op_cp",                ut_basic_tree_oper_cp},
		{"basic_tree_op_icp",               ut_basic_tree_oper_icp},
		{"basic_kv_ops",                    ut_basic_kv_oper},
		{"multi_stream_kv_op",              ut_multi_stream_kv_oper},
		{"single_thread_single_tree_kv_op", ut_st_st_kv_oper},
		{"single_thread_tree_op",           ut_st_tree_oper},
		{"multi_thread_single_tree_kv_op",  ut_mt_st_kv_oper},
		{"multi_thread_multi_tree_kv_op",   ut_mt_mt_kv_oper},
		{"random_threads_and_trees_kv_op",  ut_rt_rt_kv_oper},
		{"multi_thread_tree_op",            ut_mt_tree_oper},
		{"node_create_delete",              ut_node_create_delete},
		{"node_add_del_rec",                ut_node_add_del_rec},
		/* {"btree_kv_add_upd_del",            ut_put_update_del_operation}, */
		{NULL, NULL}
	}
};

#endif  /** KERNEL */
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
