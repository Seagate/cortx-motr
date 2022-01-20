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
#include "format/format.h"   /** m0_format_header ff_fmt */
#include "module/instance.h"
#include "lib/memory.h"
#include "lib/misc.h"
#include "lib/assert.h"
#include "lib/tlist.h"     /** m0_tl */
#include "lib/time.h"      /** m0_time_t */
#include "lib/hash_fnc.h"  /** m0_hash_fnc_fnv1 */

#include "be/ut/helper.h"  /** m0_be_ut_backend_init() */
#include "be/engine.h"     /** m0_be_engine_tx_size_max() */
#include "motr/iem.h"       /* M0_MOTR_IEM_DESC */
#include "be/alloc.h"      /** m0_be_chunk_header_size() */


#ifndef __KERNEL__
#include <stdlib.h>
#include <time.h>
#include <unistd.h>
#include <sys/mman.h>
#include "ut/ut.h"          /** struct m0_ut_suite */
#endif

#define AVOID_BE_SEGMENT    0
/**
 *  --------------------------------------------
 *  Section START - BTree Structure and Operations
 *  --------------------------------------------
 */

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
	P_WAITCHECK,
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
	MIN_KEY_SIZE             = 8,
	MAX_KEY_SIZE             = 32,
	MIN_VAL_SIZE             = 8,
	MAX_VAL_SIZE             = 48,
	MAX_TRIALS               = 3,
	MAX_TREE_HEIGHT          = 5,
	BTREE_CB_CREDIT_CNT      = MAX_TREE_HEIGHT * 2 + 1,
	INTERNAL_NODE_VALUE_SIZE = sizeof(void *),
	CRC_VALUE_SIZE           = sizeof(uint64_t),
};

#define IS_INTERNAL_NODE(node) bnode_level(node) > 0 ? true : false

#define M0_BTREE_TX_CAPTURE(tx, seg, ptr, size)                              \
			   m0_be_tx_capture(tx, &M0_BE_REG(seg, size, ptr))

#ifndef __KERNEL__
#define M0_BTREE_TX_CB_CAPTURE(tx, node, cb)                                 \
			      m0_be_tx_cb_capture(tx, node, cb)
#else
#define M0_BTREE_TX_CB_CAPTURE(tx, node, cb)                                 \
	do {                                                                 \
		typeof(cb) __cb = (cb);                                      \
		(__cb) = (__cb);                                             \
	} while (0)

#define m0_be_engine_tx_size_max(en, cred, payload_size)                     \
	do {                                                                 \
		*(cred) = M0_BE_TX_CREDIT(0, 0);                             \
	} while (0)

#define m0_be_chunk_header_size(void)   80
#endif

#if (AVOID_BE_SEGMENT == 1)

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
		(__cb) = (__cb);                                             \
	} while (0)

#undef M0_BE_ALLOC_CHUNK_ALIGN_BUF_SYNC
#define M0_BE_ALLOC_CHUNK_ALIGN_BUF_SYNC(buf, shift, seg, tx)                \
		(buf)->b_addr = m0_alloc_aligned((buf)->b_nob, shift)

#undef M0_BE_ALLOC_ALIGN_BUF_SYNC
#define M0_BE_ALLOC_ALIGN_BUF_SYNC(buf, shift, seg, tx)                      \
		(buf)->b_addr = m0_alloc_aligned((buf)->b_nob, shift)

#undef M0_BE_FREE_ALIGN_BUF_SYNC
#define M0_BE_FREE_ALIGN_BUF_SYNC(buf, shift, seg, tx)                       \
		m0_free_aligned((buf)->b_addr, (buf)->b_nob, shift)

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
	do {                                                                 \
		M0_ALLOC_PTR(ut_seg->bus_seg);                               \
		ut_seg->bus_seg->bs_gen = m0_time_now();                     \
	} while (0)

#define m0_be_ut_seg_fini(ut_seg)                                            \
	do {                                                                 \
		m0_free(ut_seg->bus_seg);                                    \
	} while (0)

#define m0_be_ut_backend_fini(ut_be)                                         \
	do { } while (0)

#define m0_be_seg_close(ut_seg)                                              \
	do { } while (0)

#define m0_be_seg_open(ut_seg)                                               \
	do { } while (0)

#define madvise(rnode, rnode_sz, MADV_NORMAL)                                \
	-1, errno = ENOMEM

#undef m0_be_engine_tx_size_max
#define m0_be_engine_tx_size_max(engine, cred, payload_size)                 \
	do {                                                                 \
		*(cred) = M0_BE_TX_CREDIT(50, (4 * 1024));            \
	} while(0)

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
 *
 * NOTE: Above design is made obsolete from EOS-25149. Now Address of the node
 * in segment will be stored as it is. The node size will be stored in the
 * respective node type header.
 */
struct segaddr {
	uint64_t as_core;
};

enum {
	NODE_SHIFT_MIN = 9,
};

static struct segaddr  segaddr_build(const void *addr);
static void           *segaddr_addr (const struct segaddr *addr);
static uint32_t        segaddr_ntype_get(const struct segaddr *addr);
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
	struct m0_fid               t_fid; /** Fid of the tree. */
	struct m0_btree_rec_key_op  t_keycmp;
};

/** Special values that can be passed to bnode_move() as 'nr' parameter. */
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

/** Direction of move in bnode_move(). */
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
	void (*nt_init)(const struct segaddr *addr, int ksize, int vsize,
			int nsize, uint32_t ntype, uint64_t crc_type,
			uint64_t gen, struct m0_fid fid);

	/** Cleanup of the node if any before deallocation */
	void (*nt_fini)(const struct nd *node);

	uint32_t (*nt_crctype_get)(const struct nd *node);

	/** Returns count of records/values in the node*/
	int  (*nt_count_rec)(const struct nd *node);

	/** Returns the space (in bytes) available in the node */
	int  (*nt_space)(const struct nd *node);

	/** Returns level of this node in the btree */
	int  (*nt_level)(const struct nd *node);

	/** Returns size of the node (as a shift value) */
	int  (*nt_shift)(const struct nd *node);

	/** Returns size of the node */
	int  (*nt_nsize)(const struct nd *node);

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
	bool  (*nt_isoverflow)(const struct nd *node,
			       const struct m0_btree_rec *rec);

	/** Returns FID stored in the node. */
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
	 *  If record is inserted or updated, modified flag will be true,
	 *  whereas in case of record deletion, the flag will be false.
	 */
	void (*nt_done)(struct slot *slot, bool modified);

	/** Makes space in the node for inserting new entry at specific index */
	void (*nt_make) (struct slot *slot);

	/**
	 * Resize the existing value. If vsize_diff < 0, value size will get
	 * decreased by vsize_diff else it will get increased by vsize_diff.
	 */
	void (*nt_val_resize) (struct slot *slot, int vsize_diff);

	/** Returns index of the record containing the key in the node */
	bool (*nt_find) (struct slot *slot, const struct m0_btree_key *key);

	/**
	 *  All the changes to the node have completed. Any post processing can
	 *  be done here.
	 */
	void (*nt_fix)  (const struct nd *node);

	/**
	 *  Changes the size of the value (increase or decrease) for the
	 *  specified key
	 */
	void (*nt_cut)  (const struct nd *node, int idx, int size);

	/** Deletes the record from the node at specific index */
	void (*nt_del)  (const struct nd *node, int idx);

	/** Updates the level of node */
	void (*nt_set_level)  (const struct nd *node, uint8_t new_level);

	/** Updates the record count of the node */
	void (*nt_set_rec_count)  (const struct nd *node, uint16_t count);

	/** Moves record(s) between nodes */
	void (*nt_move) (struct nd *src, struct nd *tgt, enum direction dir,
			 int nr);

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

	/** Returns the header size for credit calculation of tree operations */
	int  (*nt_create_delete_credit_size)(void);

	/**
	 * Calculates credits required to allocate the node and adds those
	 * credits to @accum.
	 */
	void (*nt_node_alloc_credit)(const struct nd *node,
				     struct m0_be_tx_credit *accum);

	/**
	 * Calculates credits required to free the node and adds those credits
	 * to @accum.
	 */
	void (*nt_node_free_credit)(const struct nd *node,
				    struct m0_be_tx_credit *accum);

	/**
	 * Calculates credits required to put record in the node and adds those
	 * credits to @accum.
	 */
	void (*nt_rec_put_credit)(const struct nd *node, m0_bcount_t ksize,
				  m0_bcount_t vsize,
				  struct m0_be_tx_credit *accum);

	/**
	 * Calculates credits required to update the record and adds those
	 * credits to @accum.
	 */
	void (*nt_rec_update_credit)(const struct nd *node, m0_bcount_t ksize,
				     m0_bcount_t vsize,
				     struct m0_be_tx_credit *accum);

	/**
	 * Calculates credits required to delete the record and adds those
	 * credits to @accum.
	 */
	void (*nt_rec_del_credit)(const struct nd *node, m0_bcount_t ksize,
				  m0_bcount_t vsize,
				  struct m0_be_tx_credit *accum);

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
	 * BE segment address is needed for LRU nodes because we set tree
	 * descriptor to NULL and therefore loose access to segment information.
	 */
	struct m0_be_seg       *n_seg;
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
	 *  Size of the node on BE segment pointed by this nd. Added here for
	 *  easy reference.
	 */
	uint32_t                n_size;

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

#define REC_INIT(p_rec, pp_key, p_ksz, pp_val, p_vsz)                          \
	({                                                                     \
		(p_rec)->r_key.k_data = M0_BUFVEC_INIT_BUF((pp_key), (p_ksz)); \
		(p_rec)->r_val        = M0_BUFVEC_INIT_BUF((pp_val), (p_vsz)); \
	})
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

static int64_t tree_get   (struct node_op *op, struct segaddr *addr, int nxt);
static void    tree_put   (struct td *tree);

static int64_t    bnode_get  (struct node_op *op, struct td *tree,
			      struct segaddr *addr, int nxt);
static void       bnode_put  (struct node_op *op, struct nd *node);

static void bnode_crc_validate(struct nd *node);

static int64_t    bnode_free(struct node_op *op, struct nd *node,
			     struct m0_be_tx *tx, int nxt);
static int64_t    bnode_alloc(struct node_op *op, struct td *tree, int shift,
			      const struct node_type *nt,
			      const enum m0_btree_crc_type crc_type,
			      int ksize, int vsize,
			      struct m0_be_tx *tx, int nxt);
static void bnode_op_fini(struct node_op *op);
static int bnode_access(struct segaddr *addr, int nxt);
static int  bnode_init(struct segaddr *addr, int ksize, int vsize, int nsize,
		       const struct node_type *nt,
		       const enum m0_btree_crc_type crc_type,uint64_t gen,
		       struct m0_fid fid, int nxt);
static uint32_t bnode_crctype_get(const struct nd *node);
/* Returns the number of valid key in the node. */
static int  bnode_count(const struct nd *node);

/* Returns the number of records in the node. */
static int  bnode_count_rec(const struct nd *node);
static int  bnode_space(const struct nd *node);
static int  bnode_level(const struct nd *node);
static int  bnode_nsize(const struct nd *node);
static int  bnode_keysize(const struct nd *node);
static int  bnode_valsize(const struct nd *node);
static bool  bnode_isunderflow(const struct nd *node, bool predict);
static bool  bnode_isoverflow(const struct nd *node,
			      const struct m0_btree_rec *rec);

static void bnode_rec  (struct slot *slot);
static void bnode_key  (struct slot *slot);
static void bnode_child(struct slot *slot, struct segaddr *addr);
static bool bnode_isfit(struct slot *slot);
static void bnode_done(struct slot *slot, bool modified);
static void bnode_make(struct slot *slot);
static void bnode_val_resize(struct slot *slot, int vsize_diff);

static bool bnode_find (struct slot *slot, struct m0_btree_key *key);
static void bnode_seq_cnt_update (struct nd *node);
static void bnode_fix  (const struct nd *node);
static void bnode_del  (const struct nd *node, int idx);
static void bnode_set_level  (const struct nd *node, uint8_t new_level);
static void bnode_set_rec_count(const struct nd *node, uint16_t count);
static void bnode_move (struct nd *src, struct nd *tgt, enum direction dir,
		        int nr);

static void bnode_capture(struct slot *slot, struct m0_be_tx *tx);

static void bnode_lock(struct nd *node);
static void bnode_unlock(struct nd *node);
static void bnode_fini(const struct nd *node);

/**
 * Common node header.
 *
 * This structure is located at the beginning of every node, right after
 * m0_format_header. It is used by the segment operations (node_op) to identify
 * node and tree types.
 */
struct node_header {
	uint32_t      h_node_type;
	uint32_t      h_tree_type;
	uint32_t      h_crc_type;
	uint64_t      h_gen;
	struct m0_fid h_fid;
	uint64_t      h_opaque;
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
	 * used by level_cleanup. If flag is set, bnode_put() will be called else
	 * bnode_free() will get called for l_alloc.
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
	struct level               i_level[MAX_TREE_HEIGHT];

	/** Level from where sibling nodes needs to be loaded. **/
	int                        i_pivot;

	/** i_alloc_lev will be used for P_ALLOC_REQUIRE and P_ALLOC_STORE phase
	 * to indicate current level index. **/
	int                        i_alloc_lev;

	/** Store bnode_find() output. */
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
	struct node_capture_info   i_capture[BTREE_CB_CREDIT_CNT];

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
 * Adding following functions prototype in btree temporarily. It should be move
 * to crc.h file which is currently not presesnt.
 */
M0_INTERNAL bool m0_crc32_chk(const void *data, uint64_t len,
			      const uint64_t *cksum);
M0_INTERNAL void m0_crc32(const void *data, uint64_t len,
			  uint64_t *cksum);
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

/**
 * Total space used by nodes in lru list.
 */
static int64_t lru_space_used = 0;

/** Lru used space watermark default values. */
enum lru_used_space_watermark{
	LUSW_LOW    = 2 * 1024 * 1024 * 1024ULL,
	LUSW_TARGET = 3 * 1024 * 1024 * 1024ULL,
	LUSW_HIGH   = 4 * 1024 * 1024 * 1024ULL,
};

/**
 * Watermarks for BE space occupied by nodes in lru list.
 */
/** LRU purging should not happen below low used space watermark. */
int64_t lru_space_wm_low    = LUSW_LOW;

/**
 * An ongoing LRU purging can be stopped after reaching target used space
 * watermark.
 */
int64_t lru_space_wm_target = LUSW_TARGET;

/**
 * LRU purging should be triggered if used space is above high used space
 * watermark.
 */
int64_t lru_space_wm_high   = LUSW_HIGH;

M0_TL_DESCR_DEFINE(ndlist, "node descr list", static, struct nd,
		   n_linkage, n_magic, M0_BTREE_ND_LIST_MAGIC,
		   M0_BTREE_ND_LIST_HEAD_MAGIC);
M0_TL_DEFINE(ndlist, static, struct nd);

static int bnode_access(struct segaddr *addr, int nxt)
{
	/**
	 * TODO: Implement node_access function to ensure that node data has
	 * been read from BE segment. This operation will be used during
	 * async mode of btree operations.
	 */
	return nxt;
}

static int bnode_init(struct segaddr *addr, int ksize, int vsize, int nsize,
		      const struct node_type *nt,
		      const enum m0_btree_crc_type crc_type,uint64_t gen,
		      struct m0_fid fid, int nxt)
{
	/**
	 * bnode_access() will ensure that we have node data loaded in our
	 * memory before initialisation.
	 */
	nxt = bnode_access(addr, nxt);

	/**
	 * TODO: Consider adding a state here to return in case bnode_access()
	 * requires some time to complete its operation.
	 */

	nt->nt_init(addr, ksize, vsize, nsize, nt->nt_id, crc_type, gen, fid);
	return nxt;
}

static uint32_t bnode_crctype_get(const struct nd *node)
{
	return node->n_type->nt_crctype_get(node);
}

static bool bnode_invariant(const struct nd *node)
{
	return node->n_type->nt_invariant(node);
}

/**
 * This function is implemented for debugging purpose and should get called in
 * node lock mode.
 */
static bool bnode_expensive_invariant(const struct nd *node)
{
	return node->n_type->nt_expensive_invariant(node);
}

/**
 * This function should be called after acquiring node lock.
 */
static bool bnode_isvalid(const struct nd *node)
{
	if (node->n_be_node_valid)
		return node->n_type->nt_isvalid(&node->n_addr);

	return false;
}

static int bnode_count(const struct nd *node)
{
	int key_count;
	M0_PRE(bnode_invariant(node));
	key_count = node->n_type->nt_count_rec(node);
	if (IS_INTERNAL_NODE(node))
		key_count--;
	return key_count;
}

static int bnode_count_rec(const struct nd *node)
{
	M0_PRE(bnode_invariant(node));
	return node->n_type->nt_count_rec(node);
}
static int bnode_space(const struct nd *node)
{
	M0_PRE(bnode_invariant(node));
	return node->n_type->nt_space(node);
}

static int bnode_level(const struct nd *node)
{
	M0_PRE(bnode_invariant(node));
	return (node->n_type->nt_level(node));
}

static int bnode_nsize(const struct nd *node)
{
	M0_PRE(bnode_invariant(node));
	return (node->n_type->nt_nsize(node));
}

static int bnode_keysize(const struct nd *node)
{
	M0_PRE(bnode_invariant(node));
	return (node->n_type->nt_keysize(node));
}

static int bnode_valsize(const struct nd *node)
{
	M0_PRE(bnode_invariant(node));
	return (node->n_type->nt_valsize(node));
}

/**
 * If predict is 'true' the function returns a possibility of underflow if
 * another record is deleted from this node without addition of any more
 * records.
 * If predict is 'false' the function returns the node's current underflow
 * state.
 */
static bool  bnode_isunderflow(const struct nd *node, bool predict)
{
	M0_PRE(bnode_invariant(node));
	return node->n_type->nt_isunderflow(node, predict);
}

static bool  bnode_isoverflow(const struct nd *node,
			      const struct m0_btree_rec *rec)
{
	M0_PRE(bnode_invariant(node));
	return node->n_type->nt_isoverflow(node, rec);
}

static void bnode_fid(const struct nd *node, struct m0_fid *fid)
{
	M0_PRE(bnode_invariant(node));
	node->n_type->nt_fid(node, fid);
}

static void bnode_rec(struct slot *slot)
{
	M0_PRE(bnode_invariant(slot->s_node));
	slot->s_node->n_type->nt_rec(slot);
}

static void bnode_key(struct slot *slot)
{
	M0_PRE(bnode_invariant(slot->s_node));
	slot->s_node->n_type->nt_key(slot);
}

static void bnode_child(struct slot *slot, struct segaddr *addr)
{
	M0_PRE(bnode_invariant(slot->s_node));
	slot->s_node->n_type->nt_child(slot, addr);
}

static bool bnode_isfit(struct slot *slot)
{
	M0_PRE(bnode_invariant(slot->s_node));
	return slot->s_node->n_type->nt_isfit(slot);
}

static void bnode_done(struct slot *slot, bool modified)
{
	M0_PRE(bnode_invariant(slot->s_node));
	slot->s_node->n_type->nt_done(slot, modified);
}

static void bnode_make(struct slot *slot)
{
	M0_PRE(bnode_isfit(slot));
	slot->s_node->n_type->nt_make(slot);
}

static void bnode_val_resize(struct slot *slot, int vsize_diff)
{
	M0_PRE(bnode_invariant(slot->s_node));
	slot->s_node->n_type->nt_val_resize(slot, vsize_diff);
}

static bool bnode_find(struct slot *slot, struct m0_btree_key *find_key)
{
	int                         i     = -1;
	int                         j     = bnode_count(slot->s_node);
	struct m0_btree_key         key;
	void                       *p_key;
	struct slot                 key_slot;
	m0_bcount_t                 ksize;
	struct m0_bufvec_cursor     cur_1;
	struct m0_bufvec_cursor     cur_2;
	int                         diff;
	int                         m;
	struct m0_btree_rec_key_op *keycmp = &slot->s_node->n_tree->t_keycmp;

	key.k_data           = M0_BUFVEC_INIT_BUF(&p_key, &ksize);
	key_slot.s_node      = slot->s_node;
	key_slot.s_rec.r_key = key;

	M0_PRE(bnode_invariant(slot->s_node));
	M0_PRE(find_key->k_data.ov_vec.v_nr == 1);

	while (i + 1 < j) {
		m = (i + j) / 2;

		key_slot.s_idx = m;
		bnode_key(&key_slot);

		if (keycmp->rko_keycmp != NULL) {
			void     *key_data;
			void     *find_data;

			key_data = M0_BUFVEC_DATA(&key.k_data);
			find_data = M0_BUFVEC_DATA(&find_key->k_data);
			diff = keycmp->rko_keycmp(key_data, find_data);
		} else {
			m0_bufvec_cursor_init(&cur_1, &key.k_data);
			m0_bufvec_cursor_init(&cur_2, &find_key->k_data);
			diff = m0_bufvec_cursor_cmp(&cur_1, &cur_2);
		}

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

/**
 * Increment the sequence counter by one. This function needs to called whenever
 * there is change in node.
 */
static void bnode_seq_cnt_update(struct nd *node)
{
	M0_PRE(bnode_invariant(node));
	node->n_seq++;
}

static void bnode_fix(const struct nd *node)
{
	M0_PRE(bnode_invariant(node));
	node->n_type->nt_fix(node);
}

static void bnode_del(const struct nd *node, int idx)
{
	M0_PRE(bnode_invariant(node));
	node->n_type->nt_del(node, idx);
}

static void bnode_set_level(const struct nd *node, uint8_t new_level)
{
	M0_PRE(bnode_invariant(node));
	node->n_type->nt_set_level(node, new_level);
}

static void bnode_set_rec_count(const struct nd *node, uint16_t count)
{
	M0_PRE(bnode_invariant(node));
	node->n_type->nt_set_rec_count(node, count);
}

static void bnode_move(struct nd *src, struct nd *tgt, enum direction dir,
		       int nr)
{
	M0_PRE(bnode_invariant(src));
	M0_PRE(bnode_invariant(tgt));
	M0_IN(dir,(D_LEFT, D_RIGHT));
	tgt->n_type->nt_move(src, tgt, dir, nr);
}

static void bnode_capture(struct slot *slot, struct m0_be_tx *tx)
{
	slot->s_node->n_type->nt_capture(slot, tx);
}

static void bnode_lock(struct nd *node)
{
	m0_rwlock_write_lock(&node->n_lock);
}

static void bnode_unlock(struct nd *node)
{
	m0_rwlock_write_unlock(&node->n_lock);
}

static void bnode_fini(const struct nd *node)
{
	node->n_type->nt_fini(node);
}

static void bnode_alloc_credit(const struct nd *node, m0_bcount_t ksize,
			       m0_bcount_t vsize, struct m0_be_tx_credit *accum)
{
	node->n_type->nt_node_alloc_credit(node, accum);
}

static void bnode_free_credit(const struct nd *node,
			      struct m0_be_tx_credit *accum)
{
	node->n_type->nt_node_free_credit(node, accum);
}

static void bnode_rec_put_credit(const struct nd *node, m0_bcount_t ksize,
				 m0_bcount_t vsize,
				struct m0_be_tx_credit *accum)
{
	node->n_type->nt_rec_put_credit(node, ksize, vsize, accum);
}

static void bnode_rec_del_credit(const struct nd *node, m0_bcount_t ksize,
				 m0_bcount_t vsize,
				 struct m0_be_tx_credit *accum)
{
	node->n_type->nt_rec_del_credit(node, ksize, vsize, accum);
}

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

M0_INTERNAL int m0_btree_mod_init(void)
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

M0_INTERNAL void m0_btree_mod_fini(void)
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

/**
 * Returns a segaddr formatted segment address.
 *
 * @param addr  is the start address (of the node) in the segment.
 *
 * @return Formatted Segment address.
 */
static struct segaddr segaddr_build(const void *addr)
{
	struct segaddr sa;
	sa.as_core = (uint64_t)addr;
	return sa;
}

/**
 * Returns the CPU addressable pointer from the formatted segment address.
 *
 * @param seg_addr points to the formatted segment address.
 *
 * @return CPU addressable value.
 */
static void* segaddr_addr(const struct segaddr *seg_addr)
{
	return (void *)(seg_addr->as_core);
}

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

	M0_IN(h->h_node_type, (BNT_FIXED_FORMAT,
			       BNT_FIXED_KEYSIZE_VARIABLE_VALUESIZE,
			       BNT_VARIABLE_KEYSIZE_VARIABLE_VALUESIZE));
	return h->h_node_type;
}

static const struct node_type fixed_format;
static const struct node_type fixed_ksize_variable_vsize_format;
static const struct node_type variable_kv_format;

static const struct node_type *btree_node_format[] = {
	[BNT_FIXED_FORMAT]                        = &fixed_format,
	[BNT_FIXED_KEYSIZE_VARIABLE_VALUESIZE]    = &fixed_ksize_variable_vsize_format,
	[BNT_VARIABLE_KEYSIZE_FIXED_VALUESIZE]    = NULL,
	[BNT_VARIABLE_KEYSIZE_VARIABLE_VALUESIZE] = &variable_kv_format,
};


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
		nxt  = bnode_get(op, NULL, addr, nxt);
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
			tree->t_root = node;
			tree->t_height = bnode_level(node) + 1;
			bnode_fid(node, &tree->t_fid);
			bnode_lock(node);
			node->n_tree = tree;
			bnode_unlock(node);
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

	tree->t_ref--;

	if (tree->t_ref == 0) {
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
static int64_t bnode_get(struct node_op *op, struct td *tree,
			 struct segaddr *addr, int nxt)
{
	const struct node_type *nt;
	struct nd              *node;
	bool                    in_lrulist;
	uint32_t                ntype;

	/**
	 * TODO: Include function bnode_access() during async mode of btree
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
		op->no_op.o_sm.sm_rc = M0_ERR(-EINVAL);
		m0_rwlock_write_unlock(&list_lock);
		return nxt;
	}
	ntype = segaddr_ntype_get(addr);
	nt = btree_node_format[ntype];

	op->no_node = nt->nt_opaque_get(addr);

	if (op->no_node != NULL &&
	    op->no_node->n_addr.as_core == addr->as_core) {

		bnode_lock(op->no_node);
		if (!op->no_node->n_be_node_valid) {
			op->no_op.o_sm.sm_rc = M0_ERR(-EACCES);
			bnode_unlock(op->no_node);
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
			lru_space_used -= (m0_be_chunk_header_size() +
					   op->no_node->n_size);
			/**
			 * Update nd::n_tree  to point to tree descriptor as we
			 * as we had set it to NULL in bnode_put(). For more
			 * details Refer comment in bnode_put().
			 */
			op->no_node->n_tree = tree;
		}
		bnode_unlock(op->no_node);
	} else {
		/**
		 * If node descriptor is already allocated for the node, no need
		 * to allocate node descriptor again.
		 */
		op->no_node = nt->nt_opaque_get(addr);
		if (op->no_node != NULL &&
		    op->no_node->n_addr.as_core == addr->as_core) {
			bnode_lock(op->no_node);
			op->no_node->n_ref++;
			bnode_unlock(op->no_node);
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
		node->n_size          = nt->nt_nsize(node);
		node->n_be_node_valid = true;
		node->n_seg           = tree == NULL ? NULL : tree->t_seg;
		m0_rwlock_init(&node->n_lock);
		op->no_node           = node;
		nt->nt_opaque_set(addr, node);
		ndlist_tlink_init_at(op->no_node, &btree_active_nds);

		if ((!IS_INTERNAL_NODE(op->no_node)) &&
		    bnode_crctype_get(op->no_node) != M0_BCT_NO_CRC) {
			bnode_crc_validate(op->no_node);
		}
	}
	m0_rwlock_write_unlock(&list_lock);
	return nxt;
}

static void bnode_crc_validate(struct nd *node)
{
	struct slot             node_slot;
	m0_bcount_t             ksize;
	void                   *p_key;
	m0_bcount_t             vsize;
	void                   *p_val;
	int                     i;
	int                     count;
	bool                    rc = true;
	enum m0_btree_crc_type  crc_type;

	count = bnode_count(node);
	node_slot.s_node = node;
	REC_INIT(&node_slot.s_rec, &p_key, &ksize, &p_val, &vsize);
	crc_type = bnode_crctype_get(node);
	M0_ASSERT(crc_type != M0_BCT_NO_CRC);

	for (i = 0; i < count; i++)
	{
		node_slot.s_idx = i;
		bnode_rec(&node_slot);
		/* CRC check function can be updated according to CRC type. */
		if (crc_type == M0_BCT_USER_ENC_RAW_HASH) {
			uint64_t crc;

			crc = *((uint64_t *)(p_val + vsize - sizeof(crc)));
			rc = (crc == m0_hash_fnc_fnv1(p_val,
						      vsize - sizeof(crc)));
		} else if (crc_type == M0_BCT_BTREE_ENC_RAW_HASH) {
			uint64_t crc;

			crc = *((uint64_t *)(p_val + vsize));
			rc = (crc == m0_hash_fnc_fnv1(p_val, vsize));

		} else if (crc_type == M0_BCT_USER_ENC_FORMAT_FOOTER)
			rc = m0_format_footer_verify(p_val, false) == M0_RC(0);

		if (!rc) {
			M0_MOTR_IEM_DESC(M0_MOTR_IEM_SEVERITY_E_ERROR,
					 M0_MOTR_IEM_MODULE_IO,
					 M0_MOTR_IEM_EVENT_MD_ERROR, "%s",
					 "data corruption for object with \
					  possible key: %d..., hence removing \
					  the object", *(int*)p_key);
			bnode_del(node_slot.s_node, node_slot.s_idx);
		}
	}
}
/**
 * This function decrements the reference count for this node descriptor and if
 * the reference count reaches '0' then the node descriptor is moved to LRU
 * list.
 *
 * @param op load operation to perform.
 * @param node node descriptor.
 *
 */
static void bnode_put(struct node_op *op, struct nd *node)
{
	bool purge_check   = false;
	bool is_root_node  = false;

	M0_PRE(node != NULL);

	m0_rwlock_write_lock(&list_lock);
	bnode_lock(node);
	node->n_ref--;
	if (node->n_ref == 0) {
		/**
		 * The node descriptor is in tree's active list. Remove from
		 * active list and add to lru list
		 */
		ndlist_tlist_del(node);
		ndlist_tlist_add(&btree_lru_nds, node);
		lru_space_used += (m0_be_chunk_header_size() + node->n_size);
		purge_check = true;

		is_root_node = node->n_tree->t_root == node;
		/**
		 * In case tree desriptor gets deallocated while node sits in
		 * the LRU list, we do not want node descriptor to point to an
		 * invalid tree descriptor. Hence setting nd::n_tree to NULL, it
		 * will again be populated in bnode_get().
		 */
		node->n_tree = NULL;
		if ((!node->n_be_node_valid || is_root_node) &&
		    node->n_txref == 0) {
			ndlist_tlink_del_fini(node);
			if (is_root_node)
				node->n_type->nt_opaque_set(&node->n_addr,
							    NULL);
			bnode_unlock(node);
			m0_rwlock_fini(&node->n_lock);
			m0_free(node);
			m0_rwlock_write_unlock(&list_lock);
			return;
		}
	}
	bnode_unlock(node);
	m0_rwlock_write_unlock(&list_lock);
#ifndef __KERNEL__
	if (purge_check)
		m0_btree_lrulist_purge_check(M0_PU_BTREE, 0);
#endif
}

static int64_t bnode_free(struct node_op *op, struct nd *node,
			  struct m0_be_tx *tx, int nxt)
{
	int           size  = node->n_type->nt_nsize(node);
	struct m0_buf buf;

	m0_rwlock_write_lock(&list_lock);
	bnode_lock(node);
	node->n_ref--;
	node->n_be_node_valid = false;
	op->no_addr = node->n_addr;
	buf = M0_BUF_INIT(size, segaddr_addr(&op->no_addr));
	/** Passing 0 as second parameter to avoid compilation warning. */
	M0_BE_FREE_ALIGN_BUF_SYNC(&buf, 0, node->n_tree->t_seg, tx);
	/** Capture in transaction */

	if (node->n_ref == 0 && node->n_txref == 0) {
		ndlist_tlink_del_fini(node);
		bnode_unlock(node);
		m0_rwlock_fini(&node->n_lock);
		m0_free(node);
		m0_rwlock_write_unlock(&list_lock);
		/** Capture in transaction */
		return nxt;
	}
	bnode_unlock(node);
	m0_rwlock_write_unlock(&list_lock);
	return nxt;
}

/**
 * Allocates node in the segment and a node-descriptor if all the resources are
 * available.
 *
 * @param op indicates node allocate operation.
 * @param tree points to the tree this node will be a part-of.
 * @param nsize size of the node.
 * @param nt points to the node type
 * @param ksize is the size of key (if constant) if not this contains '0'.
 * @param vsize is the size of value (if constant) if not this contains '0'.
 * @param tx points to the transaction which captures this operation.
 * @param nxt tells the next state to return when operation completes
 *
 * @return int64_t
 */
static int64_t bnode_alloc(struct node_op *op, struct td *tree, int nsize,
			   const struct node_type *nt,
			   const enum m0_btree_crc_type crc_type, int ksize,
			   int vsize, struct m0_be_tx *tx, int nxt)
{
	int            nxt_state = nxt;
	void          *area;
	struct m0_buf  buf;
	int            chunk_header_size = m0_be_chunk_header_size();
	int            page_shift = __builtin_ffsl(m0_pagesize_get()) - 1;


	M0_PRE(op->no_opc == NOP_ALLOC);

	nsize -= chunk_header_size;
	buf = M0_BUF_INIT(nsize, NULL);
	M0_BE_ALLOC_CHUNK_ALIGN_BUF_SYNC(&buf, page_shift, tree->t_seg, tx);
	area = buf.b_addr;

	M0_ASSERT(area != NULL);

	op->no_addr = segaddr_build(area);
	op->no_tree = tree;

	nxt_state = bnode_init(&op->no_addr, ksize, vsize, nsize, nt,
			       crc_type, tree->t_seg->bs_gen, tree->t_fid, nxt);
	/**
	 * TODO: Consider adding a state here to return in case we might need to
	 * visit bnode_init() again to complete its execution.
	 */

	nxt_state = bnode_get(op, tree, &op->no_addr, nxt_state);

	return nxt_state;
}

static void bnode_op_fini(struct node_op *op)
{
}


/**
 *  --------------------------------------------
 *  Section START - Fixed Format Node Structure
 *  --------------------------------------------
 */

/**
 *  Structure of the node in persistent store.
 */
struct ff_head {
	struct m0_format_header  ff_fmt;    /*< Node Header */
	struct node_header       ff_seg;    /*< Node type information */

	/**
	 * The above 2 structures should always be together with node_header
	 * following the m0_format_header.
	 */

	uint16_t                 ff_used;   /*< Count of records */
	uint8_t                  ff_level;  /*< Level in Btree */
	uint16_t                 ff_ksize;  /*< Size of key in bytes */
	uint16_t                 ff_vsize;  /*< Size of value in bytes */
	uint32_t                 ff_nsize;  /*< Node size */
	struct m0_format_footer  ff_foot;   /*< Node Footer */
	void                    *ff_opaque; /*< opaque data */
	/**
	 *  This space is used to host the Keys and Values upto the size of the
	 *  node
	 */
} M0_XCA_RECORD M0_XCA_DOMAIN(be);

static void ff_init(const struct segaddr *addr, int ksize, int vsize, int nsize,
		    uint32_t ntype, uint64_t crc_type, uint64_t gen,
		    struct m0_fid fid);
static void ff_fini(const struct nd *node);
static uint32_t ff_crctype_get(const struct nd *node);
static int  ff_count_rec(const struct nd *node);
static int  ff_space(const struct nd *node);
static int  ff_level(const struct nd *node);
static int  ff_shift(const struct nd *node);
static int  ff_nsize(const struct nd *node);
static inline int  ff_valsize(const struct nd *node);
static int  ff_keysize(const struct nd *node);
static bool ff_isunderflow(const struct nd *node, bool predict);
static bool ff_isoverflow(const struct nd *node,
			  const struct m0_btree_rec *rec);
static void ff_fid(const struct nd *node, struct m0_fid *fid);
static void ff_rec(struct slot *slot);
static void ff_node_key(struct slot *slot);
static void ff_child(struct slot *slot, struct segaddr *addr);
static bool ff_isfit(struct slot *slot);
static void ff_done(struct slot *slot, bool modified);
static void ff_make(struct slot *slot);
static void ff_val_resize(struct slot *slot, int vsize_diff);
static void ff_fix(const struct nd *node);
static void ff_cut(const struct nd *node, int idx, int size);
static void ff_del(const struct nd *node, int idx);
static void ff_set_level(const struct nd *node, uint8_t new_level);
static void ff_set_rec_count(const struct nd *node, uint16_t count);
static void generic_move(struct nd *src, struct nd *tgt, enum direction dir,
			 int nr);
static bool ff_invariant(const struct nd *node);
static bool ff_expensive_invariant(const struct nd *node);
static bool ff_verify(const struct nd *node);
static void ff_opaque_set(const struct segaddr *addr, void *opaque);
static void *ff_opaque_get(const struct segaddr *addr);
static void ff_capture(struct slot *slot, struct m0_be_tx *tx);
static void ff_node_alloc_credit(const struct nd *node,
				 struct m0_be_tx_credit *accum);
static void ff_node_free_credit(const struct nd *node,
				struct m0_be_tx_credit *accum);
static void ff_rec_put_credit(const struct nd *node, m0_bcount_t ksize,
			      m0_bcount_t vsize,
			      struct m0_be_tx_credit *accum);
static void ff_rec_update_credit(const struct nd *node, m0_bcount_t ksize,
				m0_bcount_t vsize,
				struct m0_be_tx_credit *accum);
static void ff_rec_del_credit(const struct nd *node, m0_bcount_t ksize,
			      m0_bcount_t vsize,
			      struct m0_be_tx_credit *accum);
static int  ff_create_delete_credit_size(void);

/**
 *  Implementation of node which supports fixed format/size for Keys and Values
 *  contained in it.
 */
static const struct node_type fixed_format = {
	.nt_id                        = BNT_FIXED_FORMAT,
	.nt_name                      = "m0_bnode_fixed_format",
	//.nt_tag,
	.nt_init                      = ff_init,
	.nt_fini                      = ff_fini,
	.nt_crctype_get               = ff_crctype_get,
	.nt_count_rec                 = ff_count_rec,
	.nt_space                     = ff_space,
	.nt_level                     = ff_level,
	.nt_shift                     = ff_shift,
	.nt_nsize                     = ff_nsize,
	.nt_keysize                   = ff_keysize,
	.nt_valsize                   = ff_valsize,
	.nt_isunderflow               = ff_isunderflow,
	.nt_isoverflow                = ff_isoverflow,
	.nt_fid                       = ff_fid,
	.nt_rec                       = ff_rec,
	.nt_key                       = ff_node_key,
	.nt_child                     = ff_child,
	.nt_isfit                     = ff_isfit,
	.nt_done                      = ff_done,
	.nt_make                      = ff_make,
	.nt_val_resize                = ff_val_resize,
	.nt_fix                       = ff_fix,
	.nt_cut                       = ff_cut,
	.nt_del                       = ff_del,
	.nt_set_level                 = ff_set_level,
	.nt_set_rec_count             = ff_set_rec_count,
	.nt_move                      = generic_move,
	.nt_invariant                 = ff_invariant,
	.nt_expensive_invariant       = ff_expensive_invariant,
	.nt_isvalid                   = segaddr_header_isvalid,
	.nt_verify                    = ff_verify,
	.nt_opaque_set                = ff_opaque_set,
	.nt_opaque_get                = ff_opaque_get,
	.nt_capture                   = ff_capture,
	.nt_create_delete_credit_size = ff_create_delete_credit_size,
	.nt_node_alloc_credit         = ff_node_alloc_credit,
	.nt_node_free_credit          = ff_node_free_credit,
	.nt_rec_put_credit            = ff_rec_put_credit,
	.nt_rec_update_credit         = ff_rec_update_credit,
	.nt_rec_del_credit            = ff_rec_del_credit,
};

static int ff_create_delete_credit_size(void)
{
	struct ff_head *h;
	return sizeof(*h);
}

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
	return area + h->ff_ksize * idx;
}

static void *ff_val(const struct nd *node, int idx)
{
	void           *node_start_addr = ff_data(node);
	struct ff_head *h               = node_start_addr;
	void           *node_end_addr;
	int             value_offset;

	M0_PRE(ergo(!(h->ff_used == 0 && idx == 0),
		   (0 <= idx && idx <= h->ff_used)));

	node_end_addr = node_start_addr + h->ff_nsize;
	if (h->ff_level == 0 &&
	    ff_crctype_get(node) == M0_BCT_BTREE_ENC_RAW_HASH)
		value_offset  = (ff_valsize(node) + CRC_VALUE_SIZE) * (idx + 1);
	else
		value_offset  = ff_valsize(node) * (idx + 1);

	return node_end_addr - value_offset;
}

static bool ff_rec_is_valid(const struct slot *slot)
{
	struct ff_head *h     = ff_data(slot->s_node);
	int             vsize = m0_vec_count(&slot->s_rec.r_val.ov_vec);
	bool            val_is_valid;

	val_is_valid = h->ff_level > 0 ? vsize == INTERNAL_NODE_VALUE_SIZE :
		       vsize == h->ff_vsize;

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

	if (node->n_tree->t_keycmp.rko_keycmp != NULL) {
		diff = node->n_tree->t_keycmp.rko_keycmp(
					      M0_BUFVEC_DATA(&key_prev.k_data),
					      M0_BUFVEC_DATA(&key_next.k_data));
	} else {
		m0_bufvec_cursor_init(&cur_prev, &key_prev.k_data);
		m0_bufvec_cursor_init(&cur_next, &key_next.k_data);
		diff = m0_bufvec_cursor_cmp(&cur_prev, &cur_next);
	}
	if (diff >= 0)
		return false;
	return true;

}

static bool ff_expensive_invariant(const struct nd *node)
{
	int count = bnode_count(node);
	return _0C(ergo(count > 1, m0_forall(i, count - 1,
					     ff_iskey_smaller(node, i))));
}

static bool ff_invariant(const struct nd *node)
{
	const struct ff_head *h = ff_data(node);

	/* TBD: add check for h_tree_type after initializing it in node_init. */
	return  _0C(h->ff_fmt.hd_magic == M0_FORMAT_HEADER_MAGIC) &&
		_0C(h->ff_seg.h_node_type == BNT_FIXED_FORMAT) &&
		_0C(h->ff_ksize != 0) && _0C(h->ff_vsize != 0);
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
	if (tag.ot_version != M0_BTREE_NODE_FORMAT_VERSION ||
	    tag.ot_type != M0_FORMAT_TYPE_BE_BNODE)
	    return false;

	return true;
}

static void ff_init(const struct segaddr *addr, int ksize, int vsize, int nsize,
		    uint32_t ntype, uint64_t crc_type, uint64_t gen,
		    struct m0_fid fid)
{
	struct ff_head *h   = segaddr_addr(addr);

	M0_PRE(ksize != 0);
	M0_PRE(vsize != 0);
	M0_SET0(h);

	h->ff_seg.h_crc_type  = crc_type;
	h->ff_ksize           = ksize;
	h->ff_vsize           = vsize;
	h->ff_nsize           = nsize;
	h->ff_seg.h_node_type = ntype;
	h->ff_seg.h_gen       = gen;
	h->ff_seg.h_fid       = fid;
	h->ff_opaque          = NULL;

	m0_format_header_pack(&h->ff_fmt, &(struct m0_format_tag){
		.ot_version       = M0_BTREE_NODE_FORMAT_VERSION,
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

static void ff_fini(const struct nd *node)
{
	struct ff_head *h = ff_data(node);
	m0_format_header_pack(&h->ff_fmt, &(struct m0_format_tag){
		.ot_version       = 0,
		.ot_type          = 0,
		.ot_footer_offset = 0
	});
	h->ff_opaque       = NULL;
	h->ff_fmt.hd_magic = 0;
}

static uint32_t ff_crctype_get(const struct nd *node)
{
	struct ff_head *h = ff_data(node);
	return h->ff_seg.h_crc_type;
}

static int ff_count_rec(const struct nd *node)
{
	return ff_data(node)->ff_used;
}

static int ff_space(const struct nd *node)
{
	struct ff_head *h = ff_data(node);
	int             crc_size = 0;

	if (h->ff_level == 0 &&
	    ff_crctype_get(node) == M0_BCT_BTREE_ENC_RAW_HASH)
		crc_size = CRC_VALUE_SIZE;

	return h->ff_nsize - sizeof *h -
		(h->ff_ksize + ff_valsize(node) + crc_size) * h->ff_used;
}

static int ff_level(const struct nd *node)
{
	return ff_data(node)->ff_level;
}

static int ff_shift(const struct nd *node)
{
	return 0;
}

static int ff_nsize(const struct nd *node)
{
	return ff_data(node)->ff_nsize;
}

static int ff_keysize(const struct nd *node)
{
	return ff_data(node)->ff_ksize;
}

static inline int ff_valsize(const struct nd *node)
{
	struct ff_head *h = ff_data(node);

	if (h->ff_level == 0)
		return ff_data(node)->ff_vsize;
	else
		return INTERNAL_NODE_VALUE_SIZE;
}

static bool ff_isunderflow(const struct nd *node, bool predict)
{
	int16_t rec_count = ff_data(node)->ff_used;
	if (predict && rec_count != 0)
		rec_count--;
	return  rec_count == 0;
}

static bool ff_isoverflow(const struct nd *node, const struct m0_btree_rec *rec)
{
	struct ff_head *h = ff_data(node);
	int             crc_size = 0;

	if (h->ff_level == 0 &&
	    ff_crctype_get(node) == M0_BCT_BTREE_ENC_RAW_HASH)
		crc_size = sizeof(uint64_t);

	return (ff_space(node) < h->ff_ksize + ff_valsize(node) + crc_size) ?
	       true : false;
}

static void ff_fid(const struct nd *node, struct m0_fid *fid)
{
	struct ff_head *h = ff_data(node);
	*fid = h->ff_seg.h_fid;
}

static void ff_node_key(struct slot *slot);

static void ff_rec(struct slot *slot)
{
	struct ff_head *h = ff_data(slot->s_node);

	M0_PRE(ergo(!(h->ff_used == 0 && slot->s_idx == 0),
		    slot->s_idx <= h->ff_used));

	slot->s_rec.r_val.ov_vec.v_nr = 1;
	slot->s_rec.r_val.ov_vec.v_count[0] = ff_valsize(slot->s_node);
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
	int             crc_size = 0;

	M0_PRE(ff_rec_is_valid(slot));

	if (h->ff_level == 0 &&
	   ff_crctype_get(slot->s_node) == M0_BCT_BTREE_ENC_RAW_HASH)
		crc_size = CRC_VALUE_SIZE;
	return h->ff_ksize + ff_valsize(slot->s_node) + crc_size <=
	       ff_space(slot->s_node);
}

static void ff_done(struct slot *slot, bool modified)
{
	/**
	 * After record modification, this function will be used to perform any
	 * post operations, such as CRC calculations.
	 */
	const struct nd *node = slot->s_node;
	struct ff_head  *h    = ff_data(node);
	void            *val_addr;
	uint64_t         calculated_csum;

	if (modified && h->ff_level == 0 &&
	    ff_crctype_get(node) == M0_BCT_BTREE_ENC_RAW_HASH) {
		val_addr = ff_val(slot->s_node, slot->s_idx);
		calculated_csum = m0_hash_fnc_fnv1(val_addr, h->ff_vsize);
		*(uint64_t*)(val_addr + h->ff_vsize) = calculated_csum;
	}
}

static void ff_make(struct slot *slot)
{
	struct ff_head *h  = ff_data(slot->s_node);
	void           *key_addr;
	void           *val_addr;
	int             crc_size = 0;
	int             vsize;
	int             total_key_size;
	int             total_val_size;

	if (h->ff_used == 0 || slot->s_idx == h->ff_used) {
		h->ff_used++;
		return;
	}

	if (h->ff_level == 0 &&
	    ff_crctype_get(slot->s_node) == M0_BCT_BTREE_ENC_RAW_HASH)
		crc_size = CRC_VALUE_SIZE;

	key_addr       = ff_key(slot->s_node, slot->s_idx);
	val_addr       = ff_val(slot->s_node, h->ff_used - 1);

	vsize          = ff_valsize(slot->s_node);
	total_key_size = h->ff_ksize * (h->ff_used - slot->s_idx);
	total_val_size = (vsize + crc_size) * (h->ff_used - slot->s_idx);

	m0_memmove(key_addr + h->ff_ksize, key_addr, total_key_size);
	m0_memmove(val_addr - (vsize + crc_size), val_addr,
		   total_val_size);

	h->ff_used++;
}

static void ff_val_resize(struct slot *slot, int vsize_diff)
{
	struct ff_head  *h     = ff_data(slot->s_node);

	M0_PRE(vsize_diff == 0);
	M0_PRE(!IS_INTERNAL_NODE(slot->s_node));
	M0_PRE(slot->s_idx < h->ff_used && h->ff_used > 0);
}

static void ff_fix(const struct nd *node)
{
	struct ff_head *h = ff_data(node);

	m0_format_footer_update(h);
	/** Capture changes in ff_capture */
}

static void ff_cut(const struct nd *node, int idx, int size)
{
	M0_PRE(size == ff_data(node)->ff_vsize);
}

static void ff_del(const struct nd *node, int idx)
{
	struct ff_head *h     = ff_data(node);
	void           *key_addr;
	void           *val_addr;
	int             crc_size = 0;
	int             vsize;
	int             total_key_size;
	int             total_val_size;

	M0_PRE(h->ff_used > 0 && idx < h->ff_used);

	if (idx == h->ff_used - 1) {
		h->ff_used--;
		return;
	}

	if (h->ff_level == 0 &&
	    ff_crctype_get(node) == M0_BCT_BTREE_ENC_RAW_HASH)
		crc_size = CRC_VALUE_SIZE;

	key_addr       = ff_key(node, idx);
	val_addr       = ff_val(node, h->ff_used - 1);

	vsize          = ff_valsize(node);
	total_key_size = h->ff_ksize * (h->ff_used - idx - 1);
	total_val_size = (vsize + crc_size) * (h->ff_used - idx - 1);

	m0_memmove(key_addr, key_addr + h->ff_ksize, total_key_size);
	m0_memmove(val_addr + (vsize + crc_size), val_addr,
		   total_val_size);

	h->ff_used--;
}

static void ff_set_level(const struct nd *node, uint8_t new_level)
{
	struct ff_head *h = ff_data(node);

	h->ff_level = new_level;
	/** Capture these changes in ff_capture.*/
}

static void ff_set_rec_count(const struct nd *node, uint16_t count)
{
	struct ff_head *h = ff_data(node);

	h->ff_used = count;
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

static void generic_move(struct nd *src, struct nd *tgt, enum direction dir,
			 int nr)
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

	last_idx_src = bnode_count_rec(src);
	last_idx_tgt = bnode_count_rec(tgt);

	srcidx = dir == D_LEFT ? 0 : last_idx_src - 1;
	tgtidx = dir == D_LEFT ? last_idx_tgt : 0;

	while (true) {
		if (nr == 0 || (nr == NR_EVEN &&
			       (bnode_space(tgt) <= bnode_space(src))) ||
			       (nr == NR_MAX && (srcidx == -1 ||
			       bnode_count_rec(src) == 0)))
			break;

		/** Get the record at src index in rec. */
		rec.s_node = src;
		rec.s_idx  = srcidx;
		bnode_rec(&rec);

		/**
		 *  With record from src in rec; check if that record can fit in
		 *  the target node. If yes then make space to host this record
		 *  in target node.
		 */
		rec.s_node = tgt;
		rec.s_idx  = tgtidx;
		if (!bnode_isfit(&rec))
			break;
		bnode_make(&rec);

		/** Get the location in the target node where the record from
		 *  the source node will be copied later
		 */
		tmp.s_node = tgt;
		tmp.s_idx  = tgtidx;
		bnode_rec(&tmp);

		rec.s_node = src;
		rec.s_idx  = srcidx;
		m0_bufvec_copy(&tmp.s_rec.r_key.k_data, &rec.s_rec.r_key.k_data,
			       m0_vec_count(&rec.s_rec.r_key.k_data.ov_vec));
		m0_bufvec_copy(&tmp.s_rec.r_val, &rec.s_rec.r_val,
			       m0_vec_count(&rec.s_rec.r_val.ov_vec));
		bnode_del(src, srcidx);
		if (nr > 0)
			nr--;
		bnode_done(&tmp, true);
		if (dir == D_LEFT)
			tgtidx++;
		else
			srcidx--;
	}
	bnode_seq_cnt_update(src);
	bnode_fix(src);
	bnode_seq_cnt_update(tgt);
	bnode_fix(tgt);
}

static void ff_capture(struct slot *slot, struct m0_be_tx *tx)
{
	struct ff_head   *h     = ff_data(slot->s_node);
	struct m0_be_seg *seg   = slot->s_node->n_tree->t_seg;
	m0_bcount_t       hsize = sizeof(*h) - sizeof(h->ff_opaque);

	/**
	 *  Capture starting from the location where new record may have been
	 *  added or deleted. Capture till the last record. If the deleted
	 *  record was at the end then no records need to be captured only the
	 *  header modifications need to be persisted.
	 */
	if (h->ff_used > slot->s_idx) {
		void *start_key        = ff_key(slot->s_node, slot->s_idx);
		void *last_val         = ff_val(slot->s_node, h->ff_used - 1);
		int   rec_modify_count = h->ff_used - slot->s_idx;
		int   vsize            = ff_valsize(slot->s_node);
		int   crc_size         = 0;
		int   krsize;
		int   vrsize;

		if (h->ff_level == 0 &&
		    ff_crctype_get(slot->s_node) == M0_BCT_BTREE_ENC_RAW_HASH)
			crc_size = CRC_VALUE_SIZE;

		krsize     = h->ff_ksize * rec_modify_count;
		vrsize     = (vsize + crc_size) * rec_modify_count;

		M0_BTREE_TX_CAPTURE(tx, seg, start_key, krsize);
		M0_BTREE_TX_CAPTURE(tx, seg, last_val, vrsize);
	} else if (h->ff_opaque == NULL)
		/**
		 *  This will happen when the node is initialized in which case
		 *  we want to capture the opaque pointer.
		 */
		hsize += sizeof(h->ff_opaque);

	M0_BTREE_TX_CAPTURE(tx, seg, h, hsize);
}

static void ff_node_alloc_credit(const struct nd *node,
				struct m0_be_tx_credit *accum)
{
	int             node_size   = node->n_size;
	int             shift       = __builtin_ffsl(node_size) - 1;

	m0_be_allocator_credit(NULL, M0_BAO_ALLOC_ALIGNED,
			       node_size, shift, accum);
}

static void ff_node_free_credit(const struct nd *node,
				struct m0_be_tx_credit *accum)
{
	int             node_size   = node->n_size;
	int             shift       = __builtin_ffsl(node_size) - 1;
	int             header_size = sizeof(struct ff_head);

	m0_be_allocator_credit(NULL, M0_BAO_FREE_ALIGNED,
			       node_size, shift, accum);

	m0_be_tx_credit_add(accum, &M0_BE_TX_CREDIT(1, header_size));
}

static void ff_rec_put_credit(const struct nd *node, m0_bcount_t ksize,
			      m0_bcount_t vsize,
			      struct m0_be_tx_credit *accum)
{
	int             node_size   = node->n_size;

	m0_be_tx_credit_add(accum, &M0_BE_TX_CREDIT(3, node_size));
}

static void ff_rec_update_credit(const struct nd *node, m0_bcount_t ksize,
				 m0_bcount_t vsize,
				 struct m0_be_tx_credit *accum)
{
	int             node_size   = node->n_size;

	m0_be_tx_credit_add(accum, &M0_BE_TX_CREDIT(3, node_size));
}

static void ff_rec_del_credit(const struct nd *node, m0_bcount_t ksize,
			      m0_bcount_t vsize,
			      struct m0_be_tx_credit *accum)
{
	int             node_size   = node->n_size;

	m0_be_tx_credit_add(accum, &M0_BE_TX_CREDIT(3, node_size));
}

/**
 *  --------------------------------------------
 *  Section END - Fixed Format Node Structure
 *  --------------------------------------------
 */

/**
 *  --------------------------------------------------------
 *  Section START -
 *  Fix Sized Keys and Variable Sized Value Node Structure
 *  ---------------------------------------------------------
 */

/**
 *
 * Proposed Node Structure for Fix Sized Keys and Variable Sized Value :
 *
 * Leaf Node Structure :
 *
 * +-----------+----+------+----+------+----+------+------------+----+----+----+
 * |           |    |      |    |      |    |      |            |    |    |    |
 * |           |    |      |    |      |    |      |            |    |    |    |
 * |Node Header| K0 |V0_off| K1 |V1_off| K2 |V2_off| --->  <--- | V2 | V1 | V0 |
 * |           |    |      |    |      |    |      |            |    |    |    |
 * |           |    |      |    |      |    |      |            |    |    |    |
 * +-----------+----+------+----+------+----+------+------------+----+----+----+
 *                      |           |           |               ^    ^    ^
 *                      |           |           |               |    |    |
 *                      |           |           |               |    |    |
 *                      |           |           +---------------+    |    |
 *                      |           |                                |    |
 *                      |           +------------------------------ -+    |
 *                      |                                                 |
 *                      +-------------------------------------------------+
 *
 * The above structure represents the way fixed key size and variable value size
 * node format will get stored in memory.
 *
 * Node can be mainly divided into three region, node header, region for keys
 * and region for values.
 *
 * Node header will contain all the relevant information about the node
 * including node type.
 *
 * The key region will start after the node header region. Keys will be added
 * (from the end of node header region) in the sorted order and each key will be
 * followed with a field containing the byte-offset of the value (within the
 * node). The offset for value will be useful to get absolute address of value
 * associated with that key.
 *
 * The values will be added from the end of the node such that value for the
 * first record will be present at the end of the node. To get the starting
 * address of value at index i, we will add offset associated with ith key to
 * the end address of the node; the size of the value will be difference of
 * offset for value at index i and offset for previous value.
 *
 * In this way, keys (which will also include offset for value) will be added
 * from right to left starting after node header region and value will be added
 * from left to right starting from end of the node.
 *
 *
 * Internal Node Structure:
 *
 * +-----------+----+----+----+--------------------------------+----+----+----+
 * |           |    |    |    |                                |    |    |    |
 * |           |    |    |    |                                |    |    |    |
 * |Node Header| K0 | K1 | K2 | ----->                  <----- | V2 | V1 | V0 |
 * |           |    |    |    |                                |    |    |    |
 * |           |    |    |    |                                |    |    |    |
 * +-----------+----+----+----+--------------------------------+----+----+----+
 *
 * The internal node structure is laid out similar to the leaf node structure.
 * Since the record values in internal nodes are pointers to child nodes all the
 * values are of a constant size; this eliminates the need to maintain an offset
 * to Value close to the corresponding Key as it is done in the leaf node.
 *
 */
/**
 * CRC Support:
 *
 * A btree node consists of node header, keys, values and dictionary(in case
 * of variable key/value size format). Due to bugs in the code, storage bitrot,
 * memory bitrot etc., the btree node can get corrupted. CRC will be used to
 * detect such corruption in the btree node.
 *
 * Common Assumptions:
 *	a. For internal nodes, Motr will calculate CRCs of only keys.
 *	b. For internal nodes, Motr will not calculate CRCs of values instead
 *	sanity checks will verify the validity of values(pointer to child nodes).
 *	c. CRC is updated when a record is added or deleted.
 *	d. While traversing down the tree for the first time for any operation
 *	(get/delete/put), CRC will not be checked.
 *	e. For get and delete operation, if tree traversal reaches leaf node and
 *	does not find the record then tree traversal will be re-initiated with
 *	CRC check on.
 *	f. For insert operation, any corruption in node can result in tree
 *	traversal pointing to wrong memory location. Since CRC is not verified
 *	during tree traversal, we cannot determine if there was any corruption
 *	and the memory location(at the end of tree traversal) is wrong. We have
 *	two options to handle this issue:
 *		i. We do not worry about this issue and add the new record at
 *		whatever location we reach and continue. This is because the
 *		path to the correct location where the record should be inserted
 *		is corrupted anyways and there is no way to recover the original
 *		path leading to the correct location.
 *		ii. We maintain a redundant tree and during traversal from the
 *		root node towards the leaf node we confirm the CRC of every Key
 *		which we encounter. If at any point we discover the Key node is
 *		corrupted then we fix this corrupt Key using the copy from the
 *		redundant tree and continue. This way the btree is corrected
 *		while online and also the record is inserted at the correct
 *		location in the tree.
 *
 * These are the proposals for supporting CRCs in the node:
 *
 * 1. User Provided CRC: The btree user will provide checksum as part of value.
 * The checksum can cover either the value or key+value of the record. The
 * algorithm used for calculating the checksum is known only to the btree user.
 * Motr is agnostic of the checksum details provided by the user. Motr will
 * calculate the CRC of only keys and can store it in either of the following
 * two ways:
 *	a. Motr will checksum all the keys and store checksum in the node header.
 *		i. The checksum is verified when node is loaded from the disk.
 *		ii. The checksum is updated when record is added/deleted.
 *		iii. The checksum is not updated when record is updated since
 *		the keys do not change in update operation.
 *
 *		Pros: Checksum will require less space.
 *		Cons: On record addition/deletion, the CRC needs to be
 *		calculated over all the remaining Keys in the node.
 *
 * +------+----+----+----+----+-------------------------------+----+----+----+
 * |      |CRC |    |    |    |                               |    |    |    |
 * |      +----+    |    |    |                               |    |    |    |
 * |Node Header| K0 | K1 | K2 |  ----->                <----- | V2 | V1 | V0 |
 * |           |    |    |    |                               |    |    |    |
 * |           |    |    |    |                               |    |    |    |
 * +-----------+----+----+----+-------------------------------+----+----+----+
 *
 *	b. Motr will calculate individual checksum for each key and save it next
 *	to the key in the node.
 *		i. Checksum of all the keys are verified when node is loaded
 *		from the disk.
 *		ii. Only the checksum of the newly added key is calculated and
 *		stored after the key.
 *		iii. The checksum is deleted along with the respective deleted
 *		key.
 *		iv. The checksum is not updated when record is updated since
 *		the keys do not change in update operation.
 *
 *		Pros: On record addition/deletion no need to recalculate CRCs.
 *		Cons: Checksums will require more space.
 *
 * +-----------+----+----+----+----+---------------------------+----+----+----+
 * |           |    |    |    |    |                           |    |    |    |
 * |           |    |    |    |    |                           |    |    |    |
 * |Node Header| K0 |CRC0| K1 |CRC1|  ----->            <----- | V2 | V1 | V0 |
 * |           |    |    |    |    |                           |    |    |    |
 * |           |    |    |    |    |                           |    |    |    |
 * +-----------+----+----+----+----+---------------------------+----+----+----+
 *
 * 2. User does not provide checksum in the record. It is the responsibility of
 * Motr to calculate and verify CRC. Motr can use any of the CRC calculator
 * routines to store/verify the CRC. Motr can store CRC in either of the
 * following two ways:
 *	a. Motr will checksum all the keys and values and store in the node
 *	header.
 *		i. The checksum is verified when node is loaded from the disk.
 *		ii. The checksum is updated when record is added/deleted/updated.
 *
 *		Pros: Checksum will require less space.
 *		Cons: On record addition/deletion, whole node needs to be
 *		traversed for calculating CRC of the keys.
 *
 * +------+----+----+----+----+-------------------------------+----+----+----+
 * |      |CRC |    |    |    |                               |    |    |    |
 * |      +----+    |    |    |                               |    |    |    |
 * |Node Header| K0 | K1 | K2 |  ----->                <----- | V2 | V1 | V0 |
 * |           |    |    |    |                               |    |    |    |
 * |           |    |    |    |                               |    |    |    |
 * +-----------+----+----+----+-------------------------------+----+----+----+
 *
 *	b. Motr will calculate individual checksum for each key and save it next
 *	to the key in the node.
 *		i. Checksum of all the keys are verified when node is loaded
 *		from the disk.
 *		ii. Only the checksum of the newly added key is calculated and
 *		stored after the key.
 *		iii. The checksum is deleted along with the respective deleted
 *		key.
 *		iv. The checksum is not updated when record is updated since
 *		the keys do not change in update operation.
 *
 *		Pros: On record addition/deletion no need to recalculate CRCs.
 *		Cons: Checksums will require more space.
 *
 * +-----------+----+----+----+----+---------------------------+----+----+----+
 * |           |    |    |    |    |                           |    |    |    |
 * |           |    |    |    |    |                           |    |    |    |
 * |Node Header| K0 |CRC0| K1 |CRC1|  ----->            <----- | V2 | V1 | V0 |
 * |           |    |    |    |    |                           |    |    |    |
 * |           |    |    |    |    |                           |    |    |    |
 * +-----------+----+----+----+----+---------------------------+----+----+----+
 *
 * 3. User provides the details of the checksum calculator routine to Motr. The
 * checksum calculator routine will be identified by a unique id. User will
 * calculate the checksum by using a routine and share the routine's unique id
 * with Motr. Motr can verify the checksum of the leaf nodes using the checksum
 * calculator(identified by the unique id). CRC will be calculated over both
 * keys and values of the leaf nodes for better integrity.
 *	Pros: As both user and Motr will be using the same checksum calculator
 *	routine, any corruption will be captured at Motr level.
 *
 * 4. User does not include CRC in the record and also does not want Motr to
 * calculate CRC. Btree will save the record as it received from the user.
 *	Pros: The performance can be slightly better as it will remove CRC
 *	storage and calculation.
 *	Cons: Any metadata corruption will result in undefined behavior.
 */
struct fkvv_head {
	struct m0_format_header  fkvv_fmt;    /*< Node Header */
	struct node_header       fkvv_seg;    /*< Node type information */

	/**
	 * The above 2 structures should always be together with node_header
	 * following the m0_format_header.
	 */

	uint16_t                 fkvv_used;   /*< Count of records */
	uint8_t                  fkvv_level;  /*< Level in Btree */
	uint16_t                 fkvv_ksize;  /*< Size of key in bytes */
	uint32_t                 fkvv_nsize;  /*< Node size */
	struct m0_format_footer  fkvv_foot;   /*< Node Footer */
	void                    *fkvv_opaque; /*< opaque data */
	/**
	 *  This space is used to host the Keys and Values upto the size of the
	 *  node
	 */
} M0_XCA_RECORD M0_XCA_DOMAIN(be);

#define OFFSET_SIZE sizeof(uint32_t)

static void fkvv_init(const struct segaddr *addr, int ksize, int vsize,
		      int nsize, uint32_t ntype, uint64_t crc_type,
		      uint64_t gen, struct m0_fid fid);
static void fkvv_fini(const struct nd *node);
static uint32_t fkvv_crctype_get(const struct nd *node);
static int  fkvv_count_rec(const struct nd *node);
static int  fkvv_space(const struct nd *node);
static int  fkvv_level(const struct nd *node);
static int  fkvv_shift(const struct nd *node);
static int  fkvv_nsize(const struct nd *node);
static int  fkvv_keysize(const struct nd *node);
static int  fkvv_valsize(const struct nd *node);
static bool fkvv_isunderflow(const struct nd *node, bool predict);
static bool fkvv_isoverflow(const struct nd *node,
			    const struct m0_btree_rec *rec);
static void fkvv_fid(const struct nd *node, struct m0_fid *fid);
static void fkvv_rec(struct slot *slot);
static void fkvv_node_key(struct slot *slot);
static void fkvv_child(struct slot *slot, struct segaddr *addr);
static bool fkvv_isfit(struct slot *slot);
static void fkvv_done(struct slot *slot, bool modified);
static void fkvv_make(struct slot *slot);
static void fkvv_val_resize(struct slot *slot, int vsize_diff);
static void fkvv_fix(const struct nd *node);
static void fkvv_cut(const struct nd *node, int idx, int size);
static void fkvv_del(const struct nd *node, int idx);
static void fkvv_set_level(const struct nd *node, uint8_t new_level);
static void fkvv_set_rec_count(const struct nd *node, uint16_t count);
static bool fkvv_invariant(const struct nd *node);
static bool fkvv_expensive_invariant(const struct nd *node);
static bool fkvv_verify(const struct nd *node);
static void fkvv_opaque_set(const struct segaddr *addr, void *opaque);
static void *fkvv_opaque_get(const struct segaddr *addr);
static void fkvv_capture(struct slot *slot, struct m0_be_tx *tx);
static void fkvv_node_alloc_credit(const struct nd *node,
				   struct m0_be_tx_credit *accum);
static void fkvv_node_free_credit(const struct nd *node,
				  struct m0_be_tx_credit *accum);
static void fkvv_rec_put_credit(const struct nd *node, m0_bcount_t ksize,
				m0_bcount_t vsize,
				struct m0_be_tx_credit *accum);
static void fkvv_rec_update_credit(const struct nd *node, m0_bcount_t ksize,
				   m0_bcount_t vsize,
				   struct m0_be_tx_credit *accum);
static void fkvv_rec_del_credit(const struct nd *node, m0_bcount_t ksize,
				m0_bcount_t vsize,
				struct m0_be_tx_credit *accum);
static int  fkvv_create_delete_credit_size(void);
static const struct node_type fixed_ksize_variable_vsize_format = {
	.nt_id                        = BNT_FIXED_KEYSIZE_VARIABLE_VALUESIZE,
	.nt_name                      = "m0_bnode_fixed_ksize_variable_vsize_format",
	//.nt_tag,
	.nt_init                      = fkvv_init,
	.nt_fini                      = fkvv_fini,
	.nt_crctype_get               = fkvv_crctype_get,
	.nt_count_rec                 = fkvv_count_rec,
	.nt_space                     = fkvv_space,
	.nt_level                     = fkvv_level,
	.nt_shift                     = fkvv_shift,
	.nt_nsize                     = fkvv_nsize,
	.nt_keysize                   = fkvv_keysize,
	.nt_valsize                   = fkvv_valsize,
	.nt_isunderflow               = fkvv_isunderflow,
	.nt_isoverflow                = fkvv_isoverflow,
	.nt_fid                       = fkvv_fid,
	.nt_rec                       = fkvv_rec,
	.nt_key                       = fkvv_node_key,
	.nt_child                     = fkvv_child,
	.nt_isfit                     = fkvv_isfit,
	.nt_done                      = fkvv_done,
	.nt_make                      = fkvv_make,
	.nt_val_resize                = fkvv_val_resize,
	.nt_fix                       = fkvv_fix,
	.nt_cut                       = fkvv_cut,
	.nt_del                       = fkvv_del,
	.nt_set_level                 = fkvv_set_level,
	.nt_set_rec_count             = fkvv_set_rec_count,
	.nt_move                      = generic_move,
	.nt_invariant                 = fkvv_invariant,
	.nt_expensive_invariant       = fkvv_expensive_invariant,
	.nt_isvalid                   = segaddr_header_isvalid,
	.nt_verify                    = fkvv_verify,
	.nt_opaque_set                = fkvv_opaque_set,
	.nt_opaque_get                = fkvv_opaque_get,
	.nt_capture                   = fkvv_capture,
	.nt_create_delete_credit_size = fkvv_create_delete_credit_size,
	.nt_node_alloc_credit         = fkvv_node_alloc_credit,
	.nt_node_free_credit          = fkvv_node_free_credit,
	.nt_rec_put_credit            = fkvv_rec_put_credit,
	.nt_rec_update_credit         = fkvv_rec_update_credit,
	.nt_rec_del_credit            = fkvv_rec_del_credit,
};

static struct fkvv_head *fkvv_data(const struct nd *node)
{
	return segaddr_addr(&node->n_addr);
}

static void fkvv_init(const struct segaddr *addr, int ksize, int vsize,
		      int nsize, uint32_t ntype, uint64_t crc_type,
		      uint64_t gen, struct m0_fid fid)
{
	struct fkvv_head *h       = segaddr_addr(addr);

	M0_PRE(ksize != 0);
	M0_SET0(h);

	h->fkvv_seg.h_crc_type    = crc_type;
	h->fkvv_ksize             = ksize;
	h->fkvv_nsize             = nsize;
	h->fkvv_seg.h_node_type   = ntype;
	h->fkvv_seg.h_gen         = gen;
	h->fkvv_seg.h_fid         = fid;
	h->fkvv_opaque            = NULL;

	m0_format_header_pack(&h->fkvv_fmt, &(struct m0_format_tag){
		.ot_version       = M0_BTREE_NODE_FORMAT_VERSION,
		.ot_type          = M0_FORMAT_TYPE_BE_BNODE,
		.ot_footer_offset = offsetof(struct fkvv_head, fkvv_foot)
	});
	m0_format_footer_update(h);

	/**
	 * This is the only time we capture the opaque data of the header. No
	 * other place should the opaque data get captured and written to BE
	 * segment.
	 */
}

static void fkvv_fini(const struct nd *node)
{
	struct fkvv_head *h       = fkvv_data(node);
	m0_format_header_pack(&h->fkvv_fmt, &(struct m0_format_tag){
		.ot_version       = 0,
		.ot_type          = 0,
		.ot_footer_offset = 0
	});

	h->fkvv_fmt.hd_magic = 0;
	h->fkvv_opaque       = NULL;
}

static uint32_t fkvv_crctype_get(const struct nd *node)
{
	struct fkvv_head *h = fkvv_data(node);
	return h->fkvv_seg.h_crc_type;
}

static int fkvv_count_rec(const struct nd *node)
{
	return fkvv_data(node)->fkvv_used;
}

static uint32_t *fkvv_val_offset_get(const struct nd *node, int idx)
{
	/**
	 * This function will return pointer to the offset of value at given
	 * index @idx.
	 */
	struct fkvv_head *h                 = fkvv_data(node);
	void             *start_addr        = h + 1;
	int               unit_key_rsize    = h->fkvv_ksize + OFFSET_SIZE;
	uint32_t         *p_val_offset;

	M0_PRE(h->fkvv_used > 0 && idx < h->fkvv_used);

	p_val_offset = start_addr + (h->fkvv_ksize + (unit_key_rsize * idx));
	return p_val_offset;
}

static int fkvv_space(const struct nd *node)
{
	struct fkvv_head *h         = fkvv_data(node);
	int               count     = h->fkvv_used;
	int               key_rsize;
	int               val_rsize;

	if (count == 0) {
		key_rsize = 0;
		val_rsize = 0;
	} else {
		if (IS_INTERNAL_NODE(node)) {
			key_rsize = (h->fkvv_ksize) * count;
			val_rsize = INTERNAL_NODE_VALUE_SIZE * count;
		} else {
			key_rsize = (h->fkvv_ksize + OFFSET_SIZE) * count;
			val_rsize = *fkvv_val_offset_get(node, count - 1);
		}
	}
	return h->fkvv_nsize - sizeof *h - key_rsize - val_rsize;
}

static int fkvv_level(const struct nd *node)
{
	return fkvv_data(node)->fkvv_level;
}

static int fkvv_shift(const struct nd *node)
{
	return 0;
}

static int fkvv_nsize(const struct nd *node)
{
	return fkvv_data(node)->fkvv_nsize;
}

static int fkvv_keysize(const struct nd *node)
{
	return fkvv_data(node)->fkvv_ksize;
}

static int fkvv_valsize(const struct nd *node)
{
	/**
	 * This function will return value size present in node. As, the value
	 * size will be not be fixed for fixed key, variable value size format
	 * returning -1.
	*/
	return -1;
}

static bool fkvv_isunderflow(const struct nd *node, bool predict)
{
	int16_t rec_count = fkvv_data(node)->fkvv_used;
	if (predict && rec_count != 0)
		rec_count--;
	return rec_count == 0;
}

static bool fkvv_isoverflow(const struct nd *node,
			    const struct m0_btree_rec *rec)
{
	struct fkvv_head *h     = fkvv_data(node);
	m0_bcount_t       vsize;
	int               rsize;

	if (IS_INTERNAL_NODE(node))
		rsize = h->fkvv_ksize + INTERNAL_NODE_VALUE_SIZE;
	else {
		vsize = m0_vec_count(&rec->r_val.ov_vec);
		if (fkvv_crctype_get(node) == M0_BCT_BTREE_ENC_RAW_HASH)
			vsize += CRC_VALUE_SIZE;
		rsize = h->fkvv_ksize + OFFSET_SIZE + vsize;
	}

	return (fkvv_space(node) < rsize) ? true : false;
}

static void fkvv_fid(const struct nd *node, struct m0_fid *fid)
{
	struct fkvv_head *h = fkvv_data(node);
	*fid = h->fkvv_seg.h_fid;
}

static void *fkvv_key(const struct nd *node, int idx)
{
	struct fkvv_head *h              = fkvv_data(node);
	void             *key_start_addr = h + 1;
	int               key_offset;

	M0_PRE(ergo(!(h->fkvv_used == 0 && idx == 0),
		   (0 <= idx && idx <= h->fkvv_used)));

	key_offset = (IS_INTERNAL_NODE(node)) ? (h->fkvv_ksize) * idx :
		     (h->fkvv_ksize + OFFSET_SIZE) * idx;

	return key_start_addr + key_offset;
}

static void *fkvv_val(const struct nd *node, int idx)
{
	void             *node_start_addr = fkvv_data(node);
	struct fkvv_head *h               = node_start_addr;
	void             *node_end_addr;
	int               value_offset;

	M0_PRE(ergo(!(h->fkvv_used == 0 && idx == 0),
		   (0 <= idx && idx <= h->fkvv_used)));

	node_end_addr = node_start_addr + h->fkvv_nsize;
	value_offset  = (IS_INTERNAL_NODE(node)) ?
			INTERNAL_NODE_VALUE_SIZE * (idx + 1) :
			*(fkvv_val_offset_get(node, idx));

	return node_end_addr - value_offset;
}

static bool fkvv_rec_is_valid(const struct slot *slot)
{
	struct fkvv_head *h = fkvv_data(slot->s_node);
	m0_bcount_t       ksize;
	m0_bcount_t       vsize;

	ksize = m0_vec_count(&slot->s_rec.r_key.k_data.ov_vec);
	vsize = m0_vec_count(&slot->s_rec.r_val.ov_vec);

	return _0C(ksize == h->fkvv_ksize) &&
	       _0C(ergo(IS_INTERNAL_NODE(slot->s_node),
			vsize == INTERNAL_NODE_VALUE_SIZE));
}

static int fkvv_rec_val_size(const struct nd *node, int idx)
{
	int vsize;

	if (IS_INTERNAL_NODE(node))
		vsize = INTERNAL_NODE_VALUE_SIZE;
	else {
		uint32_t *curr_val_offset;
		uint32_t *prev_val_offset;

		curr_val_offset = fkvv_val_offset_get(node, idx);
		if (idx == 0)
			vsize = *curr_val_offset;
		else {
			prev_val_offset = fkvv_val_offset_get(node, idx - 1);
			vsize = *curr_val_offset - *prev_val_offset;
		}
		if (fkvv_crctype_get(node) == M0_BCT_BTREE_ENC_RAW_HASH)
			vsize -= CRC_VALUE_SIZE;
	}
	M0_ASSERT(vsize > 0);
	return vsize;
}

static void fkvv_rec(struct slot *slot)
{
	struct fkvv_head *h = fkvv_data(slot->s_node);

	M0_PRE(h->fkvv_used > 0 && slot->s_idx < h->fkvv_used);

	slot->s_rec.r_val.ov_vec.v_nr = 1;
	slot->s_rec.r_val.ov_vec.v_count[0] =
		fkvv_rec_val_size(slot->s_node, slot->s_idx);
	slot->s_rec.r_val.ov_buf[0] = fkvv_val(slot->s_node, slot->s_idx);
	fkvv_node_key(slot);
	M0_POST(fkvv_rec_is_valid(slot));
}

static void fkvv_node_key(struct slot *slot)
{
	const struct nd  *node = slot->s_node;
	struct fkvv_head   *h    = fkvv_data(node);

	M0_PRE(h->fkvv_used > 0 && slot->s_idx < h->fkvv_used);

	slot->s_rec.r_key.k_data.ov_vec.v_nr = 1;
	slot->s_rec.r_key.k_data.ov_vec.v_count[0] = h->fkvv_ksize;
	slot->s_rec.r_key.k_data.ov_buf[0] = fkvv_key(slot->s_node,
						      slot->s_idx);
}

static void fkvv_child(struct slot *slot, struct segaddr *addr)
{
	/**
	 * This function will return the memory address pointing to its child
	 * node. We will call the function fkvv_val() for this purpose.
	 * This function is called for internal nodes.
	 */
	const struct nd  *node = slot->s_node;
	struct fkvv_head *h    = fkvv_data(node);

	M0_PRE(h->fkvv_used > 0 && slot->s_idx < h->fkvv_used);

	*addr = *(struct segaddr *)fkvv_val(node, slot->s_idx);
}

static bool fkvv_isfit(struct slot *slot)
{
	/**
	 * This function will determine if the given record provided by
	 * the solt can be added to the node.
	 */
	struct fkvv_head *h     = fkvv_data(slot->s_node);
	int               vsize = m0_vec_count(&slot->s_rec.r_val.ov_vec);
	int               rsize;

	M0_PRE(fkvv_rec_is_valid(slot));
	if (IS_INTERNAL_NODE(slot->s_node)) {
		M0_ASSERT(vsize == INTERNAL_NODE_VALUE_SIZE);
		rsize = h->fkvv_ksize + vsize;
	} else {
		if (fkvv_crctype_get(slot->s_node) == M0_BCT_BTREE_ENC_RAW_HASH)
			vsize += CRC_VALUE_SIZE;
		rsize = h->fkvv_ksize + OFFSET_SIZE + vsize;
	}
	return rsize <= fkvv_space(slot->s_node);
}

static void fkvv_done(struct slot *slot, bool modified)
{
	/**
	 * After record modification, this function will be used to perform any
	 * post operations, such as CRC calculations.
	 */
	const struct nd  *node = slot->s_node;
	struct fkvv_head *h    = fkvv_data(node);
	void             *val_addr;
	int               vsize;
	uint64_t          calculated_csum;

	if (modified && h->fkvv_level == 0 &&
	    fkvv_crctype_get(node) == M0_BCT_BTREE_ENC_RAW_HASH) {
		val_addr        = fkvv_val(slot->s_node, slot->s_idx);
		vsize           = fkvv_rec_val_size(slot->s_node, slot->s_idx);
		calculated_csum = m0_hash_fnc_fnv1(val_addr, vsize);

		*(uint64_t*)(val_addr + vsize) = calculated_csum;
	}
}

static void fkvv_make_internal(struct slot *slot)
{
	struct fkvv_head *h  = fkvv_data(slot->s_node);
	void             *key_addr;
	void             *val_addr;
	int               total_key_size;
	int               total_val_size;


	if (h->fkvv_used == 0 || slot->s_idx == h->fkvv_used) {
		h->fkvv_used++;
		return;
	}

	key_addr       = fkvv_key(slot->s_node, slot->s_idx);
	val_addr       = fkvv_val(slot->s_node, h->fkvv_used - 1);

	total_key_size = h->fkvv_ksize * (h->fkvv_used - slot->s_idx);
	total_val_size = INTERNAL_NODE_VALUE_SIZE * (h->fkvv_used - slot->s_idx);

	m0_memmove(key_addr + h->fkvv_ksize, key_addr, total_key_size);
	m0_memmove(val_addr - INTERNAL_NODE_VALUE_SIZE, val_addr,
		   total_val_size);

	h->fkvv_used++;
}

static void fkvv_make_leaf(struct slot *slot)
{
	struct fkvv_head *h = fkvv_data(slot->s_node);
	uint32_t         *curr_val_offset;
	uint32_t         *prev_val_offset   = NULL;
	uint32_t         *last_val_offset;
	uint32_t          new_val_offset;
	void             *key_addr;
	void             *val_addr;
	int               unit_key_offset_rsize;
	int               total_key_size;
	int               total_val_size;
	int               new_val_size;
	int               idx       = slot->s_idx;
	int               last_idx  = h->fkvv_used - 1;
	int               i;

	new_val_size =  m0_vec_count(&slot->s_rec.r_val.ov_vec);
	if (fkvv_crctype_get(slot->s_node) == M0_BCT_BTREE_ENC_RAW_HASH)
		new_val_size += CRC_VALUE_SIZE;

	if (slot->s_idx == 0)
		new_val_offset  = new_val_size;
	else {
		prev_val_offset = fkvv_val_offset_get(slot->s_node, idx - 1);
		new_val_offset  = *prev_val_offset + new_val_size;
	}

	if (slot->s_idx == h->fkvv_used) {
		h->fkvv_used++;
		curr_val_offset  = fkvv_val_offset_get(slot->s_node, idx);
		*curr_val_offset = new_val_offset;
		return;
	}

	last_val_offset        = fkvv_val_offset_get(slot->s_node, last_idx);
	unit_key_offset_rsize  = h->fkvv_ksize + OFFSET_SIZE;

	key_addr               = fkvv_key(slot->s_node, idx);
	val_addr               = fkvv_val(slot->s_node, last_idx);

	total_key_size         = (unit_key_offset_rsize) * (h->fkvv_used - idx);
	total_val_size         = (idx == 0) ? *last_val_offset :
				 *last_val_offset - *prev_val_offset;

	m0_memmove(key_addr + unit_key_offset_rsize, key_addr, total_key_size);
	m0_memmove(val_addr - new_val_size, val_addr, total_val_size);

	h->fkvv_used++;

	/* Update offeset values */
	curr_val_offset  = fkvv_val_offset_get(slot->s_node, idx);
	*curr_val_offset = new_val_offset;

	for (i = idx + 1; i < h->fkvv_used; i++) {
		curr_val_offset  = fkvv_val_offset_get(slot->s_node, i);
		*curr_val_offset = *curr_val_offset + new_val_size;
	}
}

static void fkvv_make(struct slot *slot)
{
	(IS_INTERNAL_NODE(slot->s_node)) ? fkvv_make_internal(slot)
					 : fkvv_make_leaf(slot);
}

static void fkvv_val_resize(struct slot *slot, int vsize_diff)
{
	struct fkvv_head *h = fkvv_data(slot->s_node);
	void             *val_addr;
	uint32_t         *curr_val_offset;
	uint32_t         *last_val_offset;
	int               total_val_size;
	int               i;

	M0_PRE(!IS_INTERNAL_NODE(slot->s_node));
	M0_PRE(slot->s_idx < h->fkvv_used && h->fkvv_used > 0);

	curr_val_offset = fkvv_val_offset_get(slot->s_node, slot->s_idx);
	last_val_offset = fkvv_val_offset_get(slot->s_node, h->fkvv_used - 1);

	val_addr        = fkvv_val(slot->s_node, h->fkvv_used - 1);
	total_val_size  = *last_val_offset - *curr_val_offset;

	m0_memmove(val_addr - vsize_diff, val_addr, total_val_size);
	for (i = slot->s_idx; i < h->fkvv_used; i++) {
		curr_val_offset  = fkvv_val_offset_get(slot->s_node, i);
		*curr_val_offset = *curr_val_offset + vsize_diff;
	}
}

static void fkvv_fix(const struct nd *node)
{
	struct fkvv_head *h = fkvv_data(node);

	m0_format_footer_update(h);
	/** Capture changes in fkvv_capture */
}

static void fkvv_cut(const struct nd *node, int idx, int size)
{

}

static void fkvv_del_internal(const struct nd *node, int idx)
{
	struct fkvv_head *h     = fkvv_data(node);
	void             *key_addr;
	void             *val_addr;
	int               total_key_size;
	int               total_val_size;

	M0_PRE(h->fkvv_used > 0 && idx < h->fkvv_used);

	if (idx == h->fkvv_used - 1) {
		h->fkvv_used--;
		return;
	}

	key_addr       = fkvv_key(node, idx);
	val_addr       = fkvv_val(node, h->fkvv_used - 1);

	total_key_size = h->fkvv_ksize * (h->fkvv_used - idx - 1);
	total_val_size = INTERNAL_NODE_VALUE_SIZE * (h->fkvv_used - idx - 1);

	m0_memmove(key_addr, key_addr + h->fkvv_ksize, total_key_size);
	m0_memmove(val_addr + INTERNAL_NODE_VALUE_SIZE, val_addr,
		   total_val_size);

	h->fkvv_used--;
}

static void fkvv_del_leaf(const struct nd *node, int idx)
{
	struct fkvv_head *h     = fkvv_data(node);
	int               key_offset_rsize;
	int               value_size;
	void             *key_addr;
	void             *val_addr;
	uint32_t         *curr_val_offset;
	uint32_t         *prev_val_offset;
	uint32_t         *last_val_offset;
	int               total_key_size;
	int               total_val_size;
	int               i;

	M0_PRE(h->fkvv_used > 0 && idx < h->fkvv_used);

	if (idx == h->fkvv_used - 1) {
		h->fkvv_used--;
		return;
	}

	curr_val_offset         = fkvv_val_offset_get(node, idx);
	last_val_offset         = fkvv_val_offset_get(node, h->fkvv_used - 1);
	key_offset_rsize        = h->fkvv_ksize + OFFSET_SIZE;
	if (idx == 0)
		value_size      = *curr_val_offset;
	else {
		prev_val_offset = fkvv_val_offset_get(node, idx - 1);
		value_size      = *curr_val_offset - *prev_val_offset;
	}

	key_addr       = fkvv_key(node, idx);
	val_addr       = fkvv_val(node, h->fkvv_used - 1);

	total_key_size = (key_offset_rsize) * (h->fkvv_used - idx - 1);
	total_val_size = *last_val_offset - *curr_val_offset;

	m0_memmove(key_addr, key_addr + key_offset_rsize, total_key_size);
	m0_memmove(val_addr + value_size, val_addr, total_val_size);

	h->fkvv_used--;

	/* Update value offset */
	for (i = idx; i < h->fkvv_used; i++) {
		curr_val_offset = fkvv_val_offset_get(node, i);
		*curr_val_offset = *curr_val_offset - value_size;
	}
}

static void fkvv_del(const struct nd *node, int idx)
{
	(IS_INTERNAL_NODE(node)) ? fkvv_del_internal(node, idx)
				 : fkvv_del_leaf(node, idx);
}

static void fkvv_set_level(const struct nd *node, uint8_t new_level)
{
	struct fkvv_head *h = fkvv_data(node);

	h->fkvv_level = new_level;
}

static void fkvv_set_rec_count(const struct nd *node, uint16_t count)
{
	struct fkvv_head *h = fkvv_data(node);

	h->fkvv_used = count;
}

static bool fkvv_invariant(const struct nd *node)
{
	const struct fkvv_head *h = fkvv_data(node);

	/* TBD: add check for h_tree_type after initializing it in node_init. */
	return
	_0C(h->fkvv_fmt.hd_magic == M0_FORMAT_HEADER_MAGIC) &&
	_0C(h->fkvv_seg.h_node_type == BNT_FIXED_KEYSIZE_VARIABLE_VALUESIZE) &&
	_0C(h->fkvv_ksize != 0);
}

static bool fkvv_iskey_smaller(const struct nd *node, int cur_key_idx)
{
	struct fkvv_head        *h;
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

	h          = fkvv_data(node);
	ksize_prev = h->fkvv_ksize;
	ksize_next = h->fkvv_ksize;

	key_prev.k_data = M0_BUFVEC_INIT_BUF(&p_key_prev, &ksize_prev);
	key_next.k_data = M0_BUFVEC_INIT_BUF(&p_key_next, &ksize_next);

	p_key_prev = fkvv_key(node, prev_key_idx);
	p_key_next = fkvv_key(node, next_key_idx);

	if (node->n_tree->t_keycmp.rko_keycmp != NULL) {
		diff = node->n_tree->t_keycmp.rko_keycmp(
					      M0_BUFVEC_DATA(&key_prev.k_data),
					      M0_BUFVEC_DATA(&key_next.k_data));
	} else {
		m0_bufvec_cursor_init(&cur_prev, &key_prev.k_data);
		m0_bufvec_cursor_init(&cur_next, &key_next.k_data);
		diff = m0_bufvec_cursor_cmp(&cur_prev, &cur_next);
	}
	if (diff >= 0)
		return false;
	return true;
}

static bool fkvv_expensive_invariant(const struct nd *node)
{
	int count = bnode_count(node);
	return _0C(ergo(count > 1, m0_forall(i, count - 1,
					     fkvv_iskey_smaller(node, i))));
}

static bool fkvv_verify(const struct nd *node)
{
	const struct fkvv_head *h = fkvv_data(node);
	return m0_format_footer_verify(h, true) == 0;
}

static void fkvv_opaque_set(const struct segaddr *addr, void *opaque)
{
	/**
	 * This function saves the opaque data.
	 */
	struct fkvv_head *h = segaddr_addr(addr);
	h->fkvv_opaque = opaque;
	/** This change should NEVER be captured.*/
}

static void *fkvv_opaque_get(const struct segaddr *addr)
{
	/**
	 * This function return the opaque data.
	 */
	struct fkvv_head *h = segaddr_addr(addr);
	return h->fkvv_opaque;
}

/**
 * This function will calculate key and value region size which needs to be
 * captured in transaction.
 */
static void fkvv_capture_krsize_vrsize_cal(struct slot *slot, int *p_krsize,
					   int *p_vrsize)
{
	struct fkvv_head *h                = fkvv_data(slot->s_node);
	int               rec_modify_count = h->fkvv_used - slot->s_idx;

	if (IS_INTERNAL_NODE(slot->s_node)) {
		*p_krsize = h->fkvv_ksize * rec_modify_count;
		*p_vrsize = INTERNAL_NODE_VALUE_SIZE * rec_modify_count;
	} else {
		uint32_t *last_val_offset;
		uint32_t *prev_val_offset;

		*p_krsize = (h->fkvv_ksize + OFFSET_SIZE) * rec_modify_count;

		last_val_offset = fkvv_val_offset_get(slot->s_node,
						      h->fkvv_used - 1);
		if (slot->s_idx == 0) {
			*p_vrsize = *last_val_offset;
		} else {
			prev_val_offset = fkvv_val_offset_get(slot->s_node,
							      slot->s_idx - 1);
			*p_vrsize = *last_val_offset - *prev_val_offset;
		}
	}
}

static void fkvv_capture(struct slot *slot, struct m0_be_tx *tx)
{
	/**
	 * This function will capture the data in node segment.
	 */
	struct fkvv_head   *h         = fkvv_data(slot->s_node);
	struct m0_be_seg   *seg       = slot->s_node->n_tree->t_seg;
	m0_bcount_t         hsize     = sizeof(*h) - sizeof(h->fkvv_opaque);
	void               *start_key;
	void               *last_val;

	if (h->fkvv_used > slot->s_idx) {
		int  krsize;
		int  vrsize;
		int *p_krsize = &krsize;
		int *p_vrsize = &vrsize;

		start_key = fkvv_key(slot->s_node, slot->s_idx);
		last_val  = fkvv_val(slot->s_node, h->fkvv_used - 1);

		fkvv_capture_krsize_vrsize_cal(slot, p_krsize, p_vrsize);

		M0_BTREE_TX_CAPTURE(tx, seg, start_key, krsize);
		M0_BTREE_TX_CAPTURE(tx, seg, last_val, vrsize);

	} else if (h->fkvv_opaque == NULL)
		hsize += sizeof(h->fkvv_opaque);

	M0_BTREE_TX_CAPTURE(tx, seg, h, hsize);
}

static int fkvv_create_delete_credit_size(void)
{
	struct fkvv_head *h;
	return sizeof(*h);
}


static void fkvv_node_alloc_credit(const struct nd *node,
				struct m0_be_tx_credit *accum)
{
	int             node_size   = node->n_size;
	int             shift       = __builtin_ffsl(node_size) - 1;

	m0_be_allocator_credit(NULL, M0_BAO_ALLOC_ALIGNED,
			       node_size, shift, accum);
}

static void fkvv_node_free_credit(const struct nd *node,
				  struct m0_be_tx_credit *accum)
{
	int             node_size   = node->n_size;
	int             shift       = __builtin_ffsl(node_size) - 1;
	int             header_size = sizeof(struct fkvv_head);

	m0_be_allocator_credit(NULL, M0_BAO_FREE_ALIGNED,
			       node_size, shift, accum);

	m0_be_tx_credit_add(accum, &M0_BE_TX_CREDIT(1, header_size));
}

static void fkvv_rec_put_credit(const struct nd *node, m0_bcount_t ksize,
			        m0_bcount_t vsize,
				struct m0_be_tx_credit *accum)
{
	int             node_size   = node->n_size;

	m0_be_tx_credit_add(accum, &M0_BE_TX_CREDIT(3, node_size));
}

static void fkvv_rec_update_credit(const struct nd *node, m0_bcount_t ksize,
				   m0_bcount_t vsize,
				   struct m0_be_tx_credit *accum)
{
	int             node_size   = node->n_size;

	m0_be_tx_credit_add(accum, &M0_BE_TX_CREDIT(3, node_size));
}

static void fkvv_rec_del_credit(const struct nd *node, m0_bcount_t ksize,
				m0_bcount_t vsize,
				struct m0_be_tx_credit *accum)
{
	int             node_size   = node->n_size;

	m0_be_tx_credit_add(accum, &M0_BE_TX_CREDIT(3, node_size));
}

/**
 *  --------------------------------------------------------
 *  Section END -
 *  Fixed Sized Keys and Variable Sized Value Node Structure
 *  ---------------------------------------------------------
 */

/**
 *  --------------------------------------------
 *  Section START -
 *  Variable Sized Keys and Values Node Structure
 *  --------------------------------------------
 *
 * Proposed internal node structure
 *
 * +----------+----+------+----+----------------+----+----+----+----+----+----+
 * |          |    |      |    |                |    |    |    |    |    |    |
 * | Node Hdr | K0 |  K1  | K2 +-->          <--+ K2 | V2 | K1 | V1 | K0 | V0 |
 * |          |    |      |    |                | Off|    | Off|    | Off|    |
 * +----------+----+------+----+-----------------+++--------+++-------+++-----+
 *                 ^      ^    ^                  |          |         |
 *                 |      |    |                  |          |         |
 *                 +------+----+------------------+----------+---------+
 *                        |    |                  |          |
 *                        +----+------------------+----------+
 *                             |                  |
 *                             +------------------+
 *
 * For the internal nodes, the above structure will be used.
 * The internal nodes will have keys of variable size whereas the values will be
 * of fixed size as they are pointers to child nodes.
 * The Keys will be added starting from the end of the node header in an
 * increasing order whereas the Values will be added starting from the end of
 * the node where each value will be preceded by the offset pointing to the
 * end of the Key associated with that particular Value.
 * Using this structure, the overhead of maintaining a directory is reduced.
 *
 *
 * Proposed leaf node structure
 *
 *                        +--------------------------+
 *                        |                          |
 *                 +------+-----------------------+  |
 *                 |      |                       |  |
 *             +---+------+-------------------+   |  |
 *             |   |      |                   |   |  |
 *             v   v      v                   |   |  | DIR                    16   11       3  0
 * +----------+--+----+--------+-------------++-+-++-++--+--+--+--------------+----+--------+--+
 * |          |  |    |        |             |  |  |  |  |  |  |              |    |        |  |
 * | Node Hdr |K0| K1 |   K2   |         +-->+--+--+--+--+--+--+              | V2 |   V1   |V0|
 * |          |  |    |        |         |   |  |  |  |  |  |  |              |    |        |  |
 * +--------+-+--+----+--------+---------+---++-++-++-+--+--+--+--------------+----+--------+--+
 *          | 0  5    12                 |    |  |  |                           ^       ^     ^
 *          +----------------------------+    |  |  |                           |       |     |
 *                                            +--+--+---------------------------+-------+-----+
 *                                               |  |                           |       |
 *                                               +--+---------------------------+-------+
 *                                                  |                           |
 *                                                  +---------------------------+
 *
 *
 *
 *  The above structure represents the way variable sized keys and values will
 *  be stored in memory.
 *  Node Hdr or Node Header will store all the relevant info regarding this node
 *  type.
 *  The Keys will be added starting from the end of the node header in an
 *  increasing order whereas the Values will be added starting from the end of
 *  the node such that the Value of the first record will be at the extreme
 *  right, the Value for the second record to the left of the Value of the
 *  first record and so on.
 *  This way the Keys start populating from the left of the node (after the
 *  Node Header) while the Values start populating from the right of the node.
 *  As the records get added the empty space between the Keys list and the
 *  Values list starts to shrink.
 *
 *  Additionally, we will maintain a directory in the central region of the node
 *  which will contain the offsets to identify the address of the particular
 *  key and value.
 *  This Directory starts in the center of the empty space between Keys and
 *  Values and will float in this region so that if addition of the new record
 *  causes either Key or Value to overwrite the Directory then the Directory
 *  will need to be shifted to make space for the incoming Key/Value;
 *  accordingly the Directory pointer in the Node Header will be updated. If no
 *  space exists to add the new Record then an Overflow has happened and the new
 *  record cannot be inserted in this node. The credit calculations for the
 *  addition of the Record need to account for the moving of this Directory.
 *
 * CRC SUPPORT FOR VARIABLE SIZED KEY AND VALUE NODE FORMAT:
 * 1. CRC ENCODED BY USER:
 * The CRC will be embedded in the Value field as the last word of Value. The
 * CRC type provided by the user will be used to identify functions for CRC
 * calculation and the same will be used for CRC verification. Verification
 * of CRC will be done whenever the node descriptor is loaded from the storage.
 *
 * 2. CRC ENCODED BY BTREE:
 * CRC will be calculated and updated in the record by BTree module when a
 * record is inserted or updated.This CRC will be placed at the end of Value.
 * Verification of CRC will be done whenever the node descriptor is loaded from
 * the storage.
 *
 * Note that, CRC can not be embedded in the directory structure as whole
 * directory will be captured for every update operation even if there is no
 * change for most of the records.
 *
 */
#define INT_OFFSET sizeof(uint32_t)

static void vkvv_init(const struct segaddr *addr, int ksize, int vsize,
		      int nsize, uint32_t ntype, uint64_t crc_type,
		      uint64_t gen, struct m0_fid fid);
static void vkvv_fini(const struct nd *node);
static uint32_t vkvv_crctype_get(const struct nd *node);
static int  vkvv_count_rec(const struct nd *node);
static int  vkvv_space(const struct nd *node);
static int  vkvv_level(const struct nd *node);
static int  vkvv_shift(const struct nd *node);
static int  vkvv_nsize(const struct nd *node);
static int  vkvv_keysize(const struct nd *node);
static int  vkvv_valsize(const struct nd *node);
static bool vkvv_isunderflow(const struct nd *node, bool predict);
static bool vkvv_isoverflow(const struct nd *node,
			    const struct m0_btree_rec *rec);
static void vkvv_fid(const struct nd *node, struct m0_fid *fid);
static void vkvv_rec(struct slot *slot);
static void vkvv_node_key(struct slot *slot);
static void vkvv_child(struct slot *slot, struct segaddr *addr);
static bool vkvv_isfit(struct slot *slot);
static void vkvv_done(struct slot *slot, bool modified);
static void vkvv_make(struct slot *slot);
static void vkvv_val_resize(struct slot *slot, int vsize_diff);
static void vkvv_fix(const struct nd *node);
static void vkvv_cut(const struct nd *node, int idx, int size);
static void vkvv_del(const struct nd *node, int idx);
static void vkvv_set_level(const struct nd *node, uint8_t new_level);
static void vkvv_set_rec_count(const struct nd *node, uint16_t count);
static bool vkvv_invariant(const struct nd *node);
static bool vkvv_expensive_invariant(const struct nd *node);
static bool vkvv_verify(const struct nd *node);
static void vkvv_opaque_set(const struct segaddr *addr, void *opaque);
static void *vkvv_opaque_get(const struct segaddr *addr);
static void vkvv_capture(struct slot *slot, struct m0_be_tx *tx);
static int  vkvv_create_delete_credit_size(void);
static void vkvv_node_alloc_credit(const struct nd *node,
				struct m0_be_tx_credit *accum);
static void vkvv_node_free_credit(const struct nd *node,
				struct m0_be_tx_credit *accum);
static void vkvv_rec_put_credit(const struct nd *node, m0_bcount_t ksize,
			      m0_bcount_t vsize,
			      struct m0_be_tx_credit *accum);
static void vkvv_rec_update_credit(const struct nd *node, m0_bcount_t ksize,
				 m0_bcount_t vsize,
				 struct m0_be_tx_credit *accum);
static void vkvv_rec_del_credit(const struct nd *node, m0_bcount_t ksize,
			      m0_bcount_t vsize,
			      struct m0_be_tx_credit *accum);

static const struct node_type variable_kv_format = {
	.nt_id                        = BNT_VARIABLE_KEYSIZE_VARIABLE_VALUESIZE,
	.nt_name                      = "m0_bnode_variable_kv_size_format",
	.nt_init                      = vkvv_init,
	.nt_fini                      = vkvv_fini,
	.nt_crctype_get               = vkvv_crctype_get,
	.nt_count_rec                 = vkvv_count_rec,
	.nt_space                     = vkvv_space,
	.nt_level                     = vkvv_level,
	.nt_shift                     = vkvv_shift,
	.nt_nsize                     = vkvv_nsize,
	.nt_keysize                   = vkvv_keysize,
	.nt_valsize                   = vkvv_valsize,
	.nt_isunderflow               = vkvv_isunderflow,
	.nt_isoverflow                = vkvv_isoverflow,
	.nt_fid                       = vkvv_fid,
	.nt_rec                       = vkvv_rec,
	.nt_key                       = vkvv_node_key,
	.nt_child                     = vkvv_child,
	.nt_isfit                     = vkvv_isfit,
	.nt_done                      = vkvv_done,
	.nt_make                      = vkvv_make,
	.nt_val_resize                = vkvv_val_resize,
	.nt_fix                       = vkvv_fix,
	.nt_cut                       = vkvv_cut,
	.nt_del                       = vkvv_del,
	.nt_set_level                 = vkvv_set_level,
	.nt_set_rec_count             = vkvv_set_rec_count,
	.nt_move                      = generic_move,
	.nt_invariant                 = vkvv_invariant,
	.nt_expensive_invariant       = vkvv_expensive_invariant,
	.nt_isvalid                   = segaddr_header_isvalid,
	.nt_verify                    = vkvv_verify,
	.nt_opaque_set                = vkvv_opaque_set,
	.nt_opaque_get                = vkvv_opaque_get,
	.nt_capture                   = vkvv_capture,
	.nt_create_delete_credit_size = vkvv_create_delete_credit_size,
	.nt_node_alloc_credit         = vkvv_node_alloc_credit,
	.nt_node_free_credit          = vkvv_node_free_credit,
	.nt_rec_put_credit            = vkvv_rec_put_credit,
	.nt_rec_update_credit         = vkvv_rec_update_credit,
	.nt_rec_del_credit            = vkvv_rec_del_credit,
	//.nt_node_free_credits         = NULL,
	/* .nt_ksize_get          = ff_ksize_get, */
	/* .nt_valsize_get        = ff_valsize_get, */
};

struct dir_rec {
	uint32_t key_offset;
	uint32_t val_offset;
};
struct vkvv_head {
	struct m0_format_header  vkvv_fmt;        /*< Node Header */
	struct node_header       vkvv_seg;        /*< Node type information */
	/**
	 * The above 2 structures should always be together with node_header
	 * following the m0_format_header.
	 */

	uint16_t                 vkvv_used;       /*< Count of records */
	uint8_t                  vkvv_level;      /*< Level in Btree */
	uint32_t                 vkvv_dir_offset; /*< Offset pointing to dir */
	uint32_t                 vkvv_nsize;      /*< Node size */

	struct m0_format_footer  vkvv_foot;       /*< Node Footer */
	void                    *vkvv_opaque;     /*< opaque data */
	/**
	 *  This space is used to host the Keys, Values and Directory upto the
	 *  size of the node.
	 */
} M0_XCA_RECORD M0_XCA_DOMAIN(be);

static struct vkvv_head *vkvv_data(const struct nd *node)
{
	return segaddr_addr(&node->n_addr);
}

/**
 * @brief This function returns the size occupied by each value entry for
 *        internal node.
 */
static uint32_t vkvv_get_vspace(void)
{
	return sizeof(void *) + INT_OFFSET;
}

/**
 * @brief  This function will do all the initialisations. Here, it
 *         will call a function to return the offset for the directory
 *         present in the middle of the node memory for leaf nodes.
 */
static void vkvv_init(const struct segaddr *addr, int ksize, int vsize,
		      int nsize, uint32_t ntype, uint64_t crc_type,
		      uint64_t gen, struct m0_fid fid)
{
	struct vkvv_head *h     = segaddr_addr(addr);
	M0_SET0(h);

	h->vkvv_seg.h_crc_type  = crc_type;
	h->vkvv_dir_offset      = (nsize - sizeof(*h))/2;
	h->vkvv_nsize           = nsize;
	h->vkvv_seg.h_node_type = ntype;
	h->vkvv_seg.h_gen       = gen;
	h->vkvv_seg.h_fid       = fid;
	h->vkvv_opaque          = NULL;

	m0_format_header_pack(&h->vkvv_fmt, &(struct m0_format_tag){
		.ot_version       = M0_BTREE_NODE_FORMAT_VERSION,
		.ot_type          = M0_FORMAT_TYPE_BE_BNODE,
		.ot_footer_offset = offsetof(struct vkvv_head, vkvv_foot)
	});
	m0_format_footer_update(h);
}

/**
 * @brief This function will do all the finalisations in the header.
 */
static void vkvv_fini(const struct nd *node)
{
	struct vkvv_head *h = vkvv_data(node);
	m0_format_header_pack(&h->vkvv_fmt, &(struct m0_format_tag){
		.ot_version       = 0,
		.ot_type          = 0,
		.ot_footer_offset = 0
	});

	h->vkvv_fmt.hd_magic = 0;
	h->vkvv_opaque       = NULL;
}

static uint32_t vkvv_crctype_get(const struct nd *node)
{
	struct vkvv_head *h = vkvv_data(node);
	return h->vkvv_seg.h_crc_type;
}

/**
 * @brief This function returns the offset pointing to the end of key for
 *        internal nodes.
 */
static uint32_t *vkvv_get_key_offset(const struct nd *node, int idx)
{
	struct vkvv_head *h          = vkvv_data(node);
	uint32_t          vspace     = vkvv_get_vspace();
	uint32_t          size       = h->vkvv_nsize;
	void             *start_addr = (void*)h + size - vspace;
	uint32_t         *offset;

	offset = start_addr - vspace * idx;
	return offset;
}

/**
 * @brief This function returns the directory address for leaf node.
 */
static struct dir_rec *vkvv_get_dir_addr(const struct nd *node)
{
	struct vkvv_head *h = vkvv_data(node);
	return ((void *)h + sizeof(*h) + h->vkvv_dir_offset);
}

/**
 * @brief This function returns the count of values in the node space.
 *        It is achieved using the vkvv_used variable.
 */
static int  vkvv_count_rec(const struct nd *node)
{
	return vkvv_data(node)->vkvv_used;
}

/**
 * @brief This function returns the space(in bytes) available in the
 *        node. This can be achieved by maintaining an entry in the
 *        directory of the next location where upcoming KV will be
 *        added. Using this entry, we can calculate the available
 *        space.
 */
static int  vkvv_space(const struct nd *node)
{
	struct vkvv_head      *h          = vkvv_data(node);
	uint32_t               total_size = h->vkvv_nsize;

	uint32_t               size_of_all_keys;
	uint32_t               size_of_all_values;
	uint32_t               available_size;
	struct dir_rec        *dir_entry;
	uint32_t               dir_size;
	uint32_t              *offset;

	if (h->vkvv_level == 0) {
		dir_entry = vkvv_get_dir_addr(node);
		dir_size  = (sizeof(struct dir_rec)) * (h->vkvv_used + 1);
		if (h->vkvv_used == 0){
			size_of_all_keys   = 0;
			size_of_all_values = 0;
		} else {
			size_of_all_keys   = dir_entry[h->vkvv_used].key_offset;
			size_of_all_values = dir_entry[h->vkvv_used].val_offset;
		}
		available_size = total_size - sizeof(*h) - dir_size -
				 size_of_all_keys - size_of_all_values;
	} else {
		if (h->vkvv_used == 0)
			size_of_all_keys = 0;
		else {
			offset = vkvv_get_key_offset(node, h->vkvv_used - 1);
			size_of_all_keys = *offset;
		}

		size_of_all_values = vkvv_get_vspace() * h->vkvv_used;
		available_size     = total_size - sizeof(*h) -
				     size_of_all_values - size_of_all_keys;
	}

	return available_size;
}

/**
 * @brief This function will return the vkvv_shift from node_header.
 */
static int  vkvv_shift(const struct nd *node)
{
	return 0;
}

/**
 * @brief This function will return the vkvv_nsize from node_header.
 */
static int vkvv_nsize(const struct nd *node)
{
	return vkvv_data(node)->vkvv_nsize;
}

/**
 * @brief This function will return the vkvv_level from node_header.
 */
static int  vkvv_level(const struct nd *node)
{
	return vkvv_data(node)->vkvv_level;
}

/**
 * @brief This function will return the -1 as the key size because all
 *        the keys, for leaf or internal node, will be of variable size.
 */
static int  vkvv_keysize(const struct nd *node)
{
	return -1;
}

/**
 * @brief This function will return the -1 as the value size for leaf
 *        nodes because the values are of variable size. Whereas for
 *        internal nodes, the values will essentially be pointers to the
 *        child nodes which will have a constant size.
 */
static int  vkvv_valsize(const struct nd *node)
{
	if (vkvv_level(node) != 0)
		return sizeof(void *);
	else
		return -1;
}

/**
 * @brief This function will identify the possibility of underflow
 *        while adding a new record. We need to check if ff_used will
 *        become 0 or not.
 */
static bool vkvv_isunderflow(const struct nd *node, bool predict)
{
	int16_t rec_count = vkvv_data(node)->vkvv_used;
	if (predict && rec_count != 0)
		rec_count--;
	return  rec_count == 0;
}

/**
 * @brief This function will identify the possibility of overflow
 *        while adding a new record.
 */
static bool vkvv_isoverflow(const struct nd *node,
			    const struct m0_btree_rec *rec)
{
	m0_bcount_t vsize;
	m0_bcount_t ksize;
	uint16_t    dir_entry;

	if (vkvv_level(node) == 0) {
		ksize     = m0_vec_count(&rec->r_key.k_data.ov_vec);
		vsize     = m0_vec_count(&rec->r_val.ov_vec);
		dir_entry = sizeof(struct dir_rec);
		if (vkvv_crctype_get(node) == M0_BCT_BTREE_ENC_RAW_HASH)
			vsize += CRC_VALUE_SIZE;
	} else {
		ksize     = MAX_KEY_SIZE + sizeof(uint64_t);
		vsize     = vkvv_get_vspace();
		dir_entry = 0;
	}
	return (ksize + vsize + dir_entry < vkvv_space(node)) ? false : true;
}

static void vkvv_fid(const struct nd *node, struct m0_fid *fid)
{
	struct vkvv_head *h   = vkvv_data(node);
	*fid = h->vkvv_seg.h_fid;
}

static uint32_t vkvv_lnode_rec_key_size(const struct nd *node, int idx)
{
	struct dir_rec *dir_entry = vkvv_get_dir_addr(node);

	return dir_entry[idx + 1].key_offset - dir_entry[idx].key_offset;
}

static uint32_t vkvv_inode_rec_key_size(const struct nd *node, int idx)
{
	uint32_t *offset;
	uint32_t *prev_offset;

	if (idx == 0) {
		offset = vkvv_get_key_offset(node, idx);
		return *offset;
	} else {
		offset      = vkvv_get_key_offset(node, idx);
		prev_offset = vkvv_get_key_offset(node, idx - 1);
		return (*offset - *prev_offset);
	}
}

/**
 * @brief This function is used to get the size of the key at the given
 *        index. This can be achieved by checking the difference in
 *        offsets of the current key and the key immediately after it.
 */
static uint32_t vkvv_rec_key_size(const struct nd *node, int idx)
{
	struct vkvv_head *h = vkvv_data(node);
	if (h->vkvv_level == 0)
		return vkvv_lnode_rec_key_size(node, idx);
	else
		return vkvv_inode_rec_key_size(node, idx);
}

static uint32_t vkvv_lnode_rec_val_size(const struct nd *node, int idx)
{
	struct dir_rec *dir_entry = vkvv_get_dir_addr(node);

	return dir_entry[idx + 1].val_offset - dir_entry[idx].val_offset;
}

/**
 * @brief This function is used to get the size of the value at the
 *        given index. This can be achieved by checking the difference
 *        in offsets of the current key and the key immediately after
 *        it for leaf nodes, and since values have a constant size for internal
 *        nodes, we will return that particular value.
 */
static uint32_t vkvv_rec_val_size(const struct nd *node, int idx)
{
	struct vkvv_head *h = vkvv_data(node);
	if (h->vkvv_level == 0) {
		int vsize = vkvv_lnode_rec_val_size(node, idx);
		return vkvv_crctype_get(node) == M0_BCT_BTREE_ENC_RAW_HASH ?
		       vsize - CRC_VALUE_SIZE : vsize;
	} else
		return vkvv_valsize(node);
}

static void *vkvv_lnode_key(const struct nd *node, int idx)
{
	struct vkvv_head *h         = vkvv_data(node);
	struct dir_rec   *dir_entry = vkvv_get_dir_addr(node);

	return ((void*)h + sizeof(*h) + dir_entry[idx].key_offset);
}

static void *vkvv_inode_key(const struct nd *node, int idx)
{
	struct vkvv_head *h = vkvv_data(node);
	uint32_t         *offset;

	if (idx == 0)
		return ((void*)h + sizeof(*h));
	else {
		offset = vkvv_get_key_offset(node, idx - 1);
		return ((void*)h + sizeof(*h) + *offset);
	}
}

/**
 * @brief Return the memory address pointing to key space.
 */
static void *vkvv_key(const struct nd *node, int idx)
{
	if (vkvv_level(node) == 0)
		return vkvv_lnode_key(node, idx);
	else
		return vkvv_inode_key(node, idx);
}

static void *vkvv_lnode_val(const struct nd *node, int idx)
{
	struct vkvv_head *h         = vkvv_data(node);
	int               size      = h->vkvv_nsize;
	struct dir_rec   *dir_entry = vkvv_get_dir_addr(node);

	return ((void*)h + size - dir_entry[idx].val_offset);
}

static void *vkvv_inode_val(const struct nd *node, int idx)
{
	struct vkvv_head *h      = vkvv_data(node);
	uint32_t          size   = h->vkvv_nsize;
	uint32_t          vspace = vkvv_get_vspace();

	return ((void*)h + size - vspace * idx + INT_OFFSET);
}

/**
 * @brief Return the memory address pointing to value space.
 */
static void *vkvv_val(const struct nd *node, int idx)
{
	if (vkvv_level(node) == 0)
		return vkvv_lnode_val(node, idx);
	else
		return vkvv_inode_val(node, idx);
}

/**
 * @brief This function will fill the provided slot with its key and
 *        value based on the index provided.
 *        vkvv_key() functions should be used to get the
 *        memory address pointing to key and val.
 *        vkvv_keysize() functions should be used to
 *        fill the v_count parameter of the slot vector.
 */
static void vkvv_node_key(struct slot *slot)
{
	const struct nd  *node = slot->s_node;
	struct vkvv_head *h    = vkvv_data(node);

	M0_PRE(ergo(!(h->vkvv_used == 0 && slot->s_idx == 0),
		    slot->s_idx <= h->vkvv_used));

	slot->s_rec.r_key.k_data.ov_vec.v_nr = 1;
	slot->s_rec.r_key.k_data.ov_vec.v_count[0] =
		vkvv_rec_key_size(slot->s_node, slot->s_idx);
	slot->s_rec.r_key.k_data.ov_buf[0] =
		vkvv_key(slot->s_node, slot->s_idx);
}

static bool vkvv_rec_is_valid(const struct slot *slot)
{
	m0_bcount_t ksize;
	m0_bcount_t vsize;

	ksize = m0_vec_count(&slot->s_rec.r_key.k_data.ov_vec);
	vsize = m0_vec_count(&slot->s_rec.r_val.ov_vec);

	return _0C(ksize > 0) &&
	       _0C(ergo(IS_INTERNAL_NODE(slot->s_node),
			vsize == INTERNAL_NODE_VALUE_SIZE));
}

/**
 * @brief This function will fill the provided slot with its key and
 *        value based on the index provided.
 *        vkvv_key() and vkvv_val() functions are used to get the
 *        memory address pointing to key and value.
 *        vkvv_keysize() and vkvv_valsize() functions are used to
 *        fill the v_count parameter of the s_rec for key and value.
 */
static void vkvv_rec(struct slot *slot)
{
	struct vkvv_head *h = vkvv_data(slot->s_node);

	M0_PRE(ergo(!(h->vkvv_used == 0 && slot->s_idx == 0),
		    slot->s_idx <= h->vkvv_used));

	slot->s_rec.r_val.ov_vec.v_nr = 1;
	slot->s_rec.r_val.ov_vec.v_count[0] =
		vkvv_rec_val_size(slot->s_node, slot->s_idx);
	slot->s_rec.r_val.ov_buf[0] =
		vkvv_val(slot->s_node, slot->s_idx + 1);
	vkvv_node_key(slot);
	M0_POST(vkvv_rec_is_valid(slot));
}

/**
 * @brief This function validates the structure of the node.
 */
static bool vkvv_invariant(const struct nd *node)
{
	struct vkvv_head *h = vkvv_data(node);

	return  _0C(h->vkvv_fmt.hd_magic == M0_FORMAT_HEADER_MAGIC) &&
		_0C(h->vkvv_seg.h_node_type ==
		    BNT_VARIABLE_KEYSIZE_VARIABLE_VALUESIZE);
}

/**
 * @brief This function validates the key order within node.
 * Implementation will be thought of once the basic functionality has
 * been implemented.
 */
static bool vkvv_expensive_invariant(const struct nd *node)
{
	return true;
}

static void vkvv_opaque_set(const struct segaddr *addr, void *opaque)
{
	struct vkvv_head *h = segaddr_addr(addr);
	h->vkvv_opaque = opaque;
}

static void *vkvv_opaque_get(const struct segaddr *addr)
{
	struct vkvv_head *h = segaddr_addr(addr);
	return h->vkvv_opaque;
}

static int vkvv_create_delete_credit_size(void)
{
	struct vkvv_head *h;
	return sizeof(*h);
}

/**
 * @brief This function will return the memory address pointing to its
 *        child node.
 */
static void vkvv_child(struct slot *slot, struct segaddr *addr)
{
	struct vkvv_head *h              = vkvv_data(slot->s_node);
	int               total_size     =  h->vkvv_nsize;
	void             *start_val_addr = (void*)h + total_size;
	int               index          = slot->s_idx;
	uint32_t          vspace         = vkvv_get_vspace();
	void             *new_addr;

	new_addr = start_val_addr - vspace * (index + 1) + INT_OFFSET;
	*addr    = *(struct segaddr *)new_addr;
}

/**
 * @brief This function will determine if the current key and value
 *        can be added to the node.
 */
static bool vkvv_isfit(struct slot *slot)
{
	m0_bcount_t ksize = m0_vec_count(&slot->s_rec.r_key.k_data.ov_vec);
	m0_bcount_t vsize;
	uint16_t    dir_entry;

	if (vkvv_level(slot->s_node) == 0) {
		dir_entry = sizeof(struct dir_rec);
		vsize     = m0_vec_count(&slot->s_rec.r_val.ov_vec);
		if (vkvv_crctype_get(slot->s_node) == M0_BCT_BTREE_ENC_RAW_HASH)
			vsize += CRC_VALUE_SIZE;
	} else {
		dir_entry = 0;
		vsize     = vkvv_get_vspace();
	}
	return ksize + vsize + dir_entry <= vkvv_space(slot->s_node);
}

static void vkvv_done(struct slot *slot, bool modified)
{
	/**
	 * After record modification, this function will be used to perform any
	 * post operations, such as CRC calculations.
	 */
	const struct nd  *node = slot->s_node;
	struct vkvv_head *h    = vkvv_data(node);
	void             *val_addr;
	int               vsize;
	uint64_t          calculated_csum;

	if (modified && h->vkvv_level == 0 &&
	    vkvv_crctype_get(node) == M0_BCT_BTREE_ENC_RAW_HASH) {
		val_addr        = vkvv_val(slot->s_node, slot->s_idx + 1);
		vsize           = vkvv_rec_val_size(slot->s_node, slot->s_idx);
		calculated_csum = m0_hash_fnc_fnv1(val_addr, vsize);

		*(uint64_t*)(val_addr + vsize) = calculated_csum;
	}
}

/**
 * @brief This function will determine if the current space for values or keys
 *        overlaps with the space occupied by directory.
 */
static bool vkvv_is_dir_overlap(struct slot *slot)
{
	uint32_t          ksize          =
			m0_vec_count(&slot->s_rec.r_key.k_data.ov_vec);
	uint32_t          vsize          =
			m0_vec_count(&slot->s_rec.r_val.ov_vec);
	struct vkvv_head *h              = vkvv_data(slot->s_node);
	int               count          = h->vkvv_used;

	void             *start_dir_addr = vkvv_get_dir_addr(slot->s_node);
	void             *key_addr       = vkvv_key(slot->s_node, count);
	void             *val_addr       = vkvv_val(slot->s_node, count);
	void             *end_dir_addr   = start_dir_addr +
					   sizeof(struct dir_rec) * (count + 2);

	if (vkvv_crctype_get(slot->s_node) == M0_BCT_BTREE_ENC_RAW_HASH)
		vsize += CRC_VALUE_SIZE;

	if (key_addr + ksize > start_dir_addr ||
	    val_addr - vsize < end_dir_addr)
		return true;
	else
		return false;
}

/**
 * @brief This function will move directory if the current space for values or
 *        keys overlaps with the space occupied by directory.
 */
static void vkvv_move_dir(struct slot *slot)
{
	struct vkvv_head *h              = vkvv_data(slot->s_node);
	int               count          = h->vkvv_used;
	uint32_t          ksize          =
			m0_vec_count(&slot->s_rec.r_key.k_data.ov_vec);
	uint32_t          vsize          =
			m0_vec_count(&slot->s_rec.r_val.ov_vec);
	void             *start_dir_addr = vkvv_get_dir_addr(slot->s_node);
	uint32_t          dir_size       = sizeof(struct dir_rec) * (count + 2);
	void             *end_dir_addr   = start_dir_addr + dir_size;
	void             *key_addr       = vkvv_key(slot->s_node, count);
	void             *val_addr       = vkvv_val(slot->s_node, count);
	uint32_t          diff;

	if (vkvv_crctype_get(slot->s_node) == M0_BCT_BTREE_ENC_RAW_HASH)
		vsize += CRC_VALUE_SIZE;

	/* Key space overlaps with directory */
	if (key_addr + ksize > start_dir_addr) {
		diff = (key_addr + ksize) - start_dir_addr;
		m0_memmove(start_dir_addr + diff, start_dir_addr, dir_size);
		h->vkvv_dir_offset += diff;
		return;
	}

	/* Value space overlaps with directory */
	if (val_addr - vsize < end_dir_addr) {
		diff = end_dir_addr - (val_addr - vsize);
		m0_memmove(start_dir_addr - diff, start_dir_addr, dir_size);
		h->vkvv_dir_offset -= diff;
		return;
	}
}

static void vkvv_lnode_make(struct slot *slot)
{
	int               index          = slot->s_idx;
	uint32_t          ksize          =
			m0_vec_count(&slot->s_rec.r_key.k_data.ov_vec);
	uint32_t          vsize          =
			m0_vec_count(&slot->s_rec.r_val.ov_vec);
	struct vkvv_head *h              = vkvv_data(slot->s_node);
	struct dir_rec   *dir_entry      = vkvv_get_dir_addr(slot->s_node);
	int               count          = h->vkvv_used;
	void             *start_key_addr;
	void             *start_val_addr;
	int               t_count;
	uint32_t          total_ksize;
	uint32_t          total_vsize;

	if (count != 0 && vkvv_is_dir_overlap(slot)) {
		vkvv_move_dir(slot);
		dir_entry = vkvv_get_dir_addr(slot->s_node);;
	}

	if (vkvv_crctype_get(slot->s_node) == M0_BCT_BTREE_ENC_RAW_HASH)
		vsize += CRC_VALUE_SIZE;

	if (index == count) {
		/**
		 * No need to do m0_memmove() as data will be added to the end
		 * of current series of keys and values. Just update the
		 * directory to keep a record of the next possible offset.
		 */
		if (index == 0) {
			dir_entry[index].key_offset = 0;
			dir_entry[index].val_offset = 0;
		}
		dir_entry[index + 1].key_offset = dir_entry[index].key_offset +
						  ksize;
		dir_entry[index + 1].val_offset = dir_entry[index].val_offset +
						  vsize;
	} else {
		start_key_addr = vkvv_key(slot->s_node, index);
		start_val_addr = vkvv_val(slot->s_node, index);
		t_count        = count;
		total_ksize    = dir_entry[count].key_offset -
				 dir_entry[index].key_offset;
		total_vsize    = dir_entry[count].val_offset -
				 dir_entry[index].val_offset;

		while (t_count >= index) {
			dir_entry[t_count + 1].key_offset =
				dir_entry[t_count].key_offset;
			dir_entry[t_count + 1].val_offset =
				dir_entry[t_count].val_offset;

			dir_entry[t_count + 1].key_offset =
				dir_entry[t_count + 1].key_offset + ksize;
			dir_entry[t_count + 1].val_offset =
				dir_entry[t_count + 1].val_offset + vsize;
			t_count--;
		}

		m0_memmove(start_key_addr + ksize, start_key_addr, total_ksize);
		m0_memmove(start_val_addr - total_vsize - vsize,
			   start_val_addr - total_vsize, total_vsize);
	}
}


static void vkvv_inode_make(struct slot *slot)
{
	int               index          = slot->s_idx;
	uint32_t          ksize          =
			m0_vec_count(&slot->s_rec.r_key.k_data.ov_vec);
	struct vkvv_head *h              = vkvv_data(slot->s_node);
	uint16_t          count          = h->vkvv_used;
	uint32_t          vspace         = vkvv_get_vspace();
	uint32_t          vsize          = vspace;
	uint32_t         *count_offset;
	uint32_t         *index_offset;
	uint32_t         *index_offset_2;
	uint32_t         *t_offset;
	uint32_t         *t_offset_2;
	uint32_t          total_ksize;
	uint32_t          total_vsize;
	uint32_t         *offset;
	int               t_count;
	void             *start_key_addr;
	void             *start_val_addr;

	if (index == count) {
		/**
		 * No need to do m0_memmove() as data will be added to the end of
		 * current series of keys and values. Just update the
		 * directory to keep a record of the next possible offset.
		 */

		index_offset = vkvv_get_key_offset(slot->s_node, index);

		if (index == 0)
			*index_offset = ksize;
		else {
			 index_offset_2 = vkvv_get_key_offset(slot->s_node,
							      index - 1);
			*index_offset   = *index_offset_2 + ksize;
		}
	} else {
		start_key_addr = vkvv_key(slot->s_node, index);
		start_val_addr = vkvv_val(slot->s_node, index);
		total_vsize    = vspace * (count - index);
		t_count        = count;

		if (index == 0) {
			index_offset = vkvv_get_key_offset(slot->s_node,
							   count - 1);
			total_ksize  = *index_offset;
		} else {
			count_offset = vkvv_get_key_offset(slot->s_node,
							   count - 1);
			index_offset = vkvv_get_key_offset(slot->s_node,
							   index - 1);
			total_ksize  = *count_offset - *index_offset;
		}

		start_val_addr -= INT_OFFSET;

		m0_memmove(start_key_addr + ksize, start_key_addr, total_ksize);
		m0_memmove(start_val_addr - total_vsize - vsize,
			   start_val_addr - total_vsize, total_vsize);

		while (t_count > index) {
			offset  = vkvv_get_key_offset(slot->s_node, t_count);
			*offset = *offset + ksize;
			t_count--;
		}

		if (index == 0) {
			t_offset  = vkvv_get_key_offset(slot->s_node, t_count);
			*t_offset = ksize;
		} else {
			t_offset   = vkvv_get_key_offset(slot->s_node, t_count);
			t_offset_2 = vkvv_get_key_offset(slot->s_node,
							 t_count - 1);
			*t_offset  = *t_offset_2 + ksize;
		}
	}
}

/**
 * @brief This function will create space to add a new entry at the
 *        given index. It is assumed that the possibility whether this
 *        can occur or not is determined before calling this function.
 *
 *        We will maintain an extra record than vkvv_used in the
 *        directory. Maintaining this extra offset value will help in
 *        size calculations.
 */
static void vkvv_make(struct slot *slot)
{
	struct vkvv_head *h = vkvv_data(slot->s_node);

	if (vkvv_level(slot->s_node) == 0)
		vkvv_lnode_make(slot);
	else
		vkvv_inode_make(slot);
	h->vkvv_used++;
}

static void vkvv_val_resize(struct slot *slot, int vsize_diff)
{
	struct vkvv_head *h              = vkvv_data(slot->s_node);
	struct dir_rec   *dir_entry      = vkvv_get_dir_addr(slot->s_node);
	int               idx            = slot->s_idx;
	int               count          = h->vkvv_used;
	void             *end_val_addr   = vkvv_val(slot->s_node, count);
	uint32_t          dir_size       = sizeof(struct dir_rec) * (count + 1);
	void             *start_dir_addr = (void*)dir_entry;
	void             *end_dir_addr   = start_dir_addr + dir_size;
	void             *end_key_addr   = vkvv_key(slot->s_node, count);
	void             *start_val_addr;
	uint32_t          total_vsize;
	int               diff;

	M0_PRE(slot->s_idx < h->vkvv_used && h->vkvv_used > 0);

	if (vsize_diff > 0 &&
	    (end_val_addr - end_dir_addr) < vsize_diff) {
		diff = vsize_diff - (end_val_addr - end_dir_addr);

		if (start_dir_addr - end_key_addr < diff)
			M0_ASSERT(0);

		m0_memmove(start_dir_addr - diff, start_dir_addr, dir_size);
		h->vkvv_dir_offset -= diff;
		dir_entry = vkvv_get_dir_addr(slot->s_node);
	}

	if (idx == count - 1) {
		dir_entry[idx + 1].val_offset += vsize_diff;
	} else {
		start_val_addr = vkvv_val(slot->s_node, idx + 1);
		total_vsize    = dir_entry[count].val_offset -
				 dir_entry[idx + 1].val_offset;
		while (count > idx) {
			dir_entry[count].val_offset += vsize_diff;
			count--;
		}
		m0_memmove(start_val_addr - total_vsize - vsize_diff,
			   start_val_addr - total_vsize, total_vsize);
	}
}

/**
 * @brief This function will do any post processing required after the
 *        node operations.
 */
static void vkvv_fix(const struct nd *node)
{
	struct vkvv_head *h = vkvv_data(node);
	m0_format_footer_update(h);
}

static void vkvv_cut(const struct nd *node, int idx, int size)
{
	/**
	 * @brief This function changes the size of value for the specified
	 *        key. Care must be taken to update the indices in the
	 *        directory while moving the values. Also, there is a need to
	 *        predetermine if the node directory itself needs to be moved.
	 */
}

/**
 * @brief This function will delete the given record or KV pair from
 *        the node. Keys and values need to moved accordingly, and
 *        the new indices should be updated in the node directory.
 */
static void vkvv_lnode_del(const struct nd *node, int idx)
{
	int               index          = idx;
	void             *start_key_addr = vkvv_key(node, idx);
	void             *start_val_addr = vkvv_val(node, idx);
	struct vkvv_head *h              = vkvv_data(node);
	struct dir_rec   *dir_entry      = vkvv_get_dir_addr(node);
	int               count          = h->vkvv_used;
	uint32_t          ksize;
	uint32_t          vsize;
	uint32_t          total_ksize;
	uint32_t          total_vsize;
	int               temp_idx;

	if (index == count - 1) {
		/**
		 * No need to do m0_memmove() as data will be added to the end of
		 * current series of keys and values. Just update the
		 * directory to keep a record of the next possible offset.
		 */
		if (index == 0) {
			dir_entry[index].key_offset = 0;
			dir_entry[index].val_offset = 0;
		}
		dir_entry[index + 1].key_offset = 0;
		dir_entry[index + 1].val_offset = 0;
	} else {
		ksize       = dir_entry[index + 1].key_offset -
			      dir_entry[index].key_offset;
		vsize       = dir_entry[index + 1].val_offset -
			      dir_entry[index].val_offset;
		total_ksize = dir_entry[count].key_offset -
			      dir_entry[index + 1].key_offset;
		total_vsize = dir_entry[count].val_offset -
			      dir_entry[index + 1].val_offset;
		temp_idx    = index;

		while (temp_idx < count) {
			dir_entry[temp_idx].key_offset =
				dir_entry[temp_idx + 1].key_offset;
			dir_entry[temp_idx].val_offset =
				dir_entry[temp_idx + 1].val_offset;

			dir_entry[temp_idx].key_offset =
				dir_entry[temp_idx].key_offset - ksize;
			dir_entry[temp_idx].val_offset =
				dir_entry[temp_idx].val_offset - vsize;
			temp_idx++;
		}
		dir_entry[temp_idx].key_offset = 0;
		dir_entry[temp_idx].val_offset = 0;

		m0_memmove(start_key_addr, start_key_addr + ksize, total_ksize);
		m0_memmove(start_val_addr - total_vsize, start_val_addr -
			   total_vsize - vsize, total_vsize);
	}
}

static void vkvv_inode_del(const struct nd *node, int idx)
{
	int               index          = idx;
	void             *start_key_addr = vkvv_key(node, index);
	void             *start_val_addr = vkvv_val(node, index);
	uint32_t          ksize          = vkvv_inode_rec_key_size(node, index);
	struct vkvv_head *h              = vkvv_data(node);
	int               count          = h->vkvv_used;
	uint32_t          vspace         = vkvv_get_vspace();
	uint32_t          vsize          = vspace;
	uint32_t         *offset;
	uint32_t          total_ksize;
	uint32_t          total_vsize;
	int               t_count;
	uint32_t         *count_offset;
	uint32_t         *index_offset;

	if (index == count - 1) {
		/**
		 * No need to do m0_memmove() as data will be added to the end of
		 * current series of keys and values. Just update the
		 * directory to keep a record of the next possible offset.
		 */
		offset  = vkvv_get_key_offset(node, index);
		*offset = 0;
	} else {
		t_count      = count - 1;
		total_vsize  = vspace * (count - index - 1);
		count_offset = vkvv_get_key_offset(node, count - 1);
		index_offset = vkvv_get_key_offset(node, index);;
		total_ksize  = *count_offset - *index_offset;

		start_val_addr -= INT_OFFSET;

		m0_memmove(start_key_addr, start_key_addr + ksize, total_ksize);
		m0_memmove(start_val_addr - total_vsize, start_val_addr -
			   total_vsize - vsize, total_vsize);

		while (t_count > index) {
			offset = vkvv_get_key_offset(node, t_count - 1);
			*offset = *offset - ksize;
			t_count--;
		}
	}
}

static void vkvv_del(const struct nd *node, int idx)
{
	struct vkvv_head *h = vkvv_data(node);

	if (vkvv_level(node) == 0)
		vkvv_lnode_del(node,idx);
	else
		vkvv_inode_del(node,idx);
	h->vkvv_used--;
}

/**
 * @brief This function will set the level for the given node.
 */
static void vkvv_set_level(const struct nd *node, uint8_t new_level)
{
	struct vkvv_head *h = vkvv_data(node);
	h->vkvv_level = new_level;
}

static void vkvv_set_rec_count(const struct nd *node, uint16_t count)
{
	struct vkvv_head *h = vkvv_data(node);

	h->vkvv_used = count;
}

/**
 * @brief This function verify the data in the node.
 */
static bool vkvv_verify(const struct nd *node)
{
	struct vkvv_head *h = vkvv_data(node);
	return m0_format_footer_verify(h, true) == 0;
}

static void vkvv_calc_size_for_capture(struct slot *slot, int count,
				       int *p_ksize, int *p_vsize, int *p_dsize)
{
	int idx = slot->s_idx;
	struct dir_rec *dir_entry;
	uint32_t       *t_ksize_1;
	uint32_t       *t_ksize_2;

	if (vkvv_level(slot->s_node) == 0) {
		dir_entry = vkvv_get_dir_addr(slot->s_node);
		*p_ksize  = dir_entry[count].key_offset -
			    dir_entry[idx].key_offset;
		*p_vsize  = dir_entry[count].val_offset -
			    dir_entry[idx].val_offset;
		*p_dsize = (sizeof(struct dir_rec)) * (count + 1);
	} else {
		*p_dsize = 0;
		*p_vsize = vkvv_get_vspace() * (count - idx);
		if (idx == 0) {
			t_ksize_1 = vkvv_get_key_offset(slot->s_node,
							count - 1);
			*p_ksize  = *t_ksize_1;
		} else {
			t_ksize_1 = vkvv_get_key_offset(slot->s_node,
							count - 1);
			t_ksize_2 = vkvv_get_key_offset(slot->s_node, idx - 1);
			*p_ksize  = *t_ksize_1 - *t_ksize_2;
		}
	}
}

/**
 * @brief This function will capture the data in BE segment.
 */
static void vkvv_capture(struct slot *slot, struct m0_be_tx *tx)
{
	struct vkvv_head *h         = vkvv_data(slot->s_node);
	struct m0_be_seg *seg       = slot->s_node->n_tree->t_seg;
	m0_bcount_t       hsize     = sizeof(*h) - sizeof(h->vkvv_opaque);

	void *key_addr;
	int   ksize;
	int  *p_ksize = &ksize;
	void *val_addr;
	int   vsize;
	int  *p_vsize = &vsize;
	void *dir_addr;
	int   dsize;
	int  *p_dsize = &dsize;

	if (slot->s_idx < h->vkvv_used) {
		key_addr = vkvv_key(slot->s_node, slot->s_idx);
		val_addr = vkvv_val(slot->s_node, h->vkvv_used);
		dir_addr = vkvv_get_dir_addr(slot->s_node);

		if (bnode_level(slot->s_node) != 0)
			val_addr -= INT_OFFSET;

		vkvv_calc_size_for_capture(slot, h->vkvv_used, p_ksize, p_vsize,
					   p_dsize);
		M0_BTREE_TX_CAPTURE(tx, seg, key_addr, ksize);
		M0_BTREE_TX_CAPTURE(tx, seg, val_addr, vsize);
		if (bnode_level(slot->s_node) == 0)
			M0_BTREE_TX_CAPTURE(tx, seg, dir_addr, dsize);

	} else if (h->vkvv_opaque == NULL)
		hsize += sizeof(h->vkvv_opaque);

	M0_BTREE_TX_CAPTURE(tx, seg, h, hsize);
}

static void vkvv_node_alloc_credit(const struct nd *node,
				struct m0_be_tx_credit *accum)
{
	int             node_size   = node->n_size;
	int             shift       = __builtin_ffsl(node_size) - 1;

	m0_be_allocator_credit(NULL, M0_BAO_ALLOC_ALIGNED,
			       node_size, shift, accum);
}

static void vkvv_node_free_credit(const struct nd *node,
				struct m0_be_tx_credit *accum)
{
	int             node_size   = node->n_size;
	int             shift       = __builtin_ffsl(node_size) - 1;
	int             header_size = sizeof(struct vkvv_head);

	m0_be_allocator_credit(NULL, M0_BAO_FREE_ALIGNED,
			       node_size, shift, accum);

	m0_be_tx_credit_add(accum, &M0_BE_TX_CREDIT(1, header_size));
}

static void vkvv_rec_put_credit(const struct nd *node, m0_bcount_t ksize,
			      m0_bcount_t vsize,
			      struct m0_be_tx_credit *accum)
{
	int             node_size   = node->n_size;

	m0_be_tx_credit_add(accum, &M0_BE_TX_CREDIT(4, node_size));
}

static void vkvv_rec_update_credit(const struct nd *node, m0_bcount_t ksize,
				 m0_bcount_t vsize,
				 struct m0_be_tx_credit *accum)
{
	int             node_size   = node->n_size;

	m0_be_tx_credit_add(accum, &M0_BE_TX_CREDIT(4, node_size));
}

static void vkvv_rec_del_credit(const struct nd *node, m0_bcount_t ksize,
			      m0_bcount_t vsize,
			      struct m0_be_tx_credit *accum)
{
	int             node_size   = node->n_size;

	m0_be_tx_credit_add(accum, &M0_BE_TX_CREDIT(4, node_size));
}
/**
 *  --------------------------------------------
 *  Section END -
 *  Variable Sized Keys and Values Node Structure
 *  --------------------------------------------
 */

static const struct node_type* btree_nt_from_bt(const struct m0_btree_type *bt)
{
	if (bt->ksize != -1 && bt->vsize != -1)
		return &fixed_format;
	else if (bt->ksize != -1 && bt->vsize == -1)
		return &fixed_ksize_variable_vsize_format;
	else if (bt->ksize == -1 && bt->vsize != -1)
		M0_ASSERT(0); /** Currently we do not support this */
	else
		return &variable_kv_format;; /** Replace with correct type. */
}

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
	struct m0_be_tx_credit cb_cred = M0_BE_TX_CB_CREDIT(0,  0, 1);
	m0_be_tx_credit_add(accum,  &cb_cred);
}

/**
 * This function will calculate credits required to split node and it will add
 * those credits to @accum.
 */
static void btree_node_split_credit(const struct m0_btree  *tree,
				    m0_bcount_t             ksize,
				    m0_bcount_t             vsize,
				    struct m0_be_tx_credit *accum)
{
	struct m0_be_tx_credit cred = {};

	bnode_alloc_credit(tree->t_desc->t_root, ksize, vsize, accum);

	/* credits to update two nodes : existing and newly allocated. */
	bnode_rec_put_credit(tree->t_desc->t_root, ksize, vsize, &cred);
	btree_callback_credit(&cred);
	m0_be_tx_credit_mul(&cred, 2);

	m0_be_tx_credit_add(accum, &cred);
}

M0_INTERNAL void m0_btree_put_credit(const struct m0_btree  *tree,
				     m0_bcount_t             nr,
				     m0_bcount_t             ksize,
				     m0_bcount_t             vsize,
				     struct m0_be_tx_credit *accum)
{
	struct m0_be_tx_credit cred = {};

	/* Credits for split operation */
	btree_node_split_credit(tree, ksize, vsize, &cred);
	m0_be_tx_credit_mul(&cred, MAX_TREE_HEIGHT);
	m0_be_tx_credit_mac(accum, &cred, nr);
}

M0_INTERNAL void m0_btree_put_credit2(const struct m0_btree_type *type,
				      int                         nob,
				      m0_bcount_t                 nr,
				      m0_bcount_t                 ksize,
				      m0_bcount_t                 vsize,
				      struct m0_be_tx_credit     *accum)
{
	struct m0_btree dummy_btree;
	struct td       dummy_td;
	struct nd       dummy_nd;

	dummy_nd.n_size = nob;
	dummy_nd.n_type = btree_nt_from_bt(type);
	dummy_nd.n_tree = &dummy_td;

	dummy_td.t_root = &dummy_nd;
	dummy_td.t_type = type;

	dummy_btree.t_desc = &dummy_td;
	dummy_btree.t_type = type;

	m0_btree_put_credit(&dummy_btree, nr, ksize, vsize, accum);
}

M0_INTERNAL void m0_btree_del_credit(const struct m0_btree  *tree,
				     m0_bcount_t             nr,
				     m0_bcount_t             ksize,
				     m0_bcount_t             vsize,
				     struct m0_be_tx_credit *accum)
{
	struct m0_be_tx_credit cred = {};

	/* Credits for freeing the node. */
	bnode_free_credit(tree->t_desc->t_root, &cred);
	btree_callback_credit(&cred);
	m0_be_tx_credit_mul(&cred, MAX_TREE_HEIGHT);

	/* Credits for deleting record from the node. */
	bnode_rec_del_credit(tree->t_desc->t_root, ksize, vsize, &cred);
	btree_callback_credit(&cred);
	m0_be_tx_credit_mac(accum, &cred, nr);
}

M0_INTERNAL void m0_btree_del_credit2(const struct m0_btree_type *type,
				      int                         nob,
				      m0_bcount_t                 nr,
				      m0_bcount_t                 ksize,
				      m0_bcount_t                 vsize,
				      struct m0_be_tx_credit     *accum)
{
	struct m0_btree dummy_btree;
	struct td       dummy_td;
	struct nd       dummy_nd;

	dummy_nd.n_size = nob;
	dummy_nd.n_type = btree_nt_from_bt(type);
	dummy_nd.n_tree = &dummy_td;

	dummy_td.t_root = &dummy_nd;
	dummy_td.t_type = type;

	dummy_btree.t_desc = &dummy_td;
	dummy_btree.t_type = type;

	m0_btree_del_credit(&dummy_btree, nr, ksize, vsize, accum);
}

M0_INTERNAL void m0_btree_update_credit(const struct m0_btree  *tree,
					m0_bcount_t             nr,
					m0_bcount_t             ksize,
					m0_bcount_t             vsize,
					struct m0_be_tx_credit *accum)
{
	/**
	 * If the new value size is different than existing value size. It
	 * is required to allocate credit same as put operation.
	 */
	m0_btree_put_credit(tree, nr, ksize, vsize, accum);
}

M0_INTERNAL void m0_btree_update_credit2(const struct m0_btree_type *type,
					 int                         nob,
					 m0_bcount_t                 nr,
					 m0_bcount_t                 ksize,
					 m0_bcount_t                 vsize,
					 struct m0_be_tx_credit     *accum)
{
	struct m0_btree dummy_btree;
	struct td       dummy_td;
	struct nd       dummy_nd;

	dummy_nd.n_size = nob;
	dummy_nd.n_type = btree_nt_from_bt(type);
	dummy_nd.n_tree = &dummy_td;

	dummy_td.t_root = &dummy_nd;
	dummy_td.t_type = type;

	dummy_btree.t_desc = &dummy_td;
	dummy_btree.t_type = type;

	m0_btree_update_credit(&dummy_btree, nr, ksize, vsize, accum);
}

M0_INTERNAL void m0_btree_create_credit(const struct m0_btree_type *bt,
					struct m0_be_tx_credit *accum,
					m0_bcount_t nr)
{
	const struct node_type *nt   = btree_nt_from_bt(bt);
	int                     size = nt->nt_create_delete_credit_size();
	struct m0_be_tx_credit  cred = M0_BE_TX_CREDIT(1, size);
	m0_be_tx_credit_add(accum, &cred);
	m0_be_tx_credit_mac(accum, &cred, nr);
}

M0_INTERNAL void m0_btree_destroy_credit(struct m0_btree *tree,
					 const struct m0_btree_type *bt,
					 struct m0_be_tx_credit *accum,
					 m0_bcount_t nr)
{
	const struct node_type *nt   = (tree != NULL) ?
					tree->t_desc->t_root->n_type :
					btree_nt_from_bt(bt);
	int                     size = nt->nt_create_delete_credit_size();
	struct m0_be_tx_credit  cred = M0_BE_TX_CREDIT(1, size);

	m0_be_tx_credit_add(accum, &cred);
	m0_be_tx_credit_mac(accum, &cred, nr);
}

M0_INTERNAL void m0_btree_truncate_credit(struct m0_be_tx        *tx,
					  struct m0_btree        *tree,
					  struct m0_be_tx_credit *accum,
					  m0_bcount_t            *limit)
{
	struct m0_be_tx_credit max;
	struct m0_be_tx_credit cred = {};
	m0_bcount_t            node_nr1;
	m0_bcount_t            node_nr2;

	/** Credit for freeing up a node.*/
	bnode_free_credit(tree->t_desc->t_root, &cred);

	m0_be_engine_tx_size_max(tx->t_engine, &max, NULL);

	/** Taking half of the max credits to be on safer side. */
	max.tc_reg_size /= 2;
	max.tc_reg_nr   /= 2;

	/**
	 * Calculate the minimum number of nodes that can be freed based on
	 * maximum reg_size and reg_nr.
	 */
	node_nr1 = max.tc_reg_size / cred.tc_reg_size;
	node_nr2 = max.tc_reg_nr / cred.tc_reg_nr;

	*limit = min_type(m0_bcount_t, node_nr1, node_nr2);

	m0_be_tx_credit_mac(accum, &cred, *limit);
}

/**
 *  --------------------------------------------
 *  Section END - Btree Credit
 *  --------------------------------------------
 */


/** Insert operation section start point: */

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
		bnode_lock(l_node);
		if (!bnode_isvalid(l_node)) {
			bnode_unlock(l_node);
			bnode_op_fini(&oi->i_nop);
			return false;
		}
		if (oi->i_level[total_level].l_seq != l_node->n_seq) {
			bnode_unlock(l_node);
			return false;
		}
		bnode_unlock(l_node);
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

	bnode_lock(l_sibling);
	if (!bnode_isvalid(l_sibling)) {
		bnode_unlock(l_sibling);
		bnode_op_fini(&oi->i_nop);
		return false;
	}
	if (oi->i_level[oi->i_used].l_sib_seq != l_sibling->n_seq) {
		bnode_unlock(l_sibling);
		return false;
	}
	bnode_unlock(l_sibling);
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

static void level_put(struct m0_btree_oimpl *oi)
{
	int i;
	for (i = 0; i <= oi->i_used; ++i) {
		if (oi->i_level[i].l_node != NULL) {
			bnode_put(&oi->i_nop, oi->i_level[i].l_node);
			oi->i_level[i].l_node = NULL;
		}
		if (oi->i_level[i].l_sibling != NULL) {
			bnode_put(&oi->i_nop, oi->i_level[i].l_sibling);
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
	level_put(oi);
	/** Free up allocated nodes. */
	for (i = 0; i < oi->i_height; ++i) {
		if (oi->i_level[i].l_alloc != NULL) {
			if (oi->i_level[i].i_alloc_in_use)
				bnode_put(&oi->i_nop, oi->i_level[i].l_alloc);
			else {
				oi->i_nop.no_opc = NOP_FREE;
				/**
				 * bnode_free() will not cause any I/O delay
				 * since this node was allocated in P_ALLOC
				 * phase in put_tick and I/O delay would have
				 * happened during the allocation.
				 */
				bnode_fini(oi->i_level[i].l_alloc);
				bnode_free(&oi->i_nop, oi->i_level[i].l_alloc,
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
			bnode_put(&oi->i_nop, oi->i_extra_node);
		else {
			oi->i_nop.no_opc = NOP_FREE;
			bnode_fini(oi->i_extra_node);
			bnode_free(&oi->i_nop, oi->i_extra_node, tx, 0);
			oi->i_extra_node = NULL;
		}
	}
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

	for (i = 0; i < BTREE_CB_CREDIT_CNT; i++) {
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

	m0_rwlock_write_lock(&list_lock);
	bnode_lock(node);
	M0_ASSERT(node->n_txref != 0);
	node->n_txref--;
	if (!node->n_be_node_valid && node->n_ref == 0 && node->n_txref == 0) {
		ndlist_tlink_del_fini(node);
		bnode_unlock(node);
		m0_rwlock_fini(&node->n_lock);
		m0_free(node);
		m0_rwlock_write_unlock(&list_lock);
		return;
	}
	bnode_unlock(node);
	m0_rwlock_write_unlock(&list_lock);
}

static void btree_tx_nodes_capture(struct m0_btree_oimpl *oi,
				  struct m0_be_tx *tx)
{
	struct node_capture_info *arr = oi->i_capture;
	struct slot          node_slot;
	int                       i;

	for (i = 0; i < BTREE_CB_CREDIT_CNT; i++) {
		if (arr[i].nc_node == NULL)
			break;

		node_slot.s_node = arr[i].nc_node;
		node_slot.s_idx  = arr[i].nc_idx;
		bnode_capture(&node_slot, tx);

		bnode_lock(arr[i].nc_node);
		arr[i].nc_node->n_txref++;
		bnode_unlock(arr[i].nc_node);
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
	int                     curr_max_level = bnode_level(lev->l_node);
	struct slot             node_slot;

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
	M0_PRE(oi->i_extra_node != NULL);
	bnode_lock(lev->l_node);
	bnode_lock(oi->i_extra_node);

	bnode_set_level(oi->i_extra_node, curr_max_level);

	bnode_move(lev->l_node, oi->i_extra_node, D_RIGHT, NR_MAX);
	M0_ASSERT(bnode_count_rec(lev->l_node) == 0);

	bnode_set_level(lev->l_node, curr_max_level + 1);

	/* 2) add new 2 records at root node. */

	/* Add first rec at root */
	node_slot = (struct slot) {
		.s_node = lev->l_node,
		.s_idx  = 0
	};
	node_slot.s_rec = bop->bo_rec;

	/* M0_ASSERT(bnode_isfit(&node_slot)) */
	bnode_make(&node_slot);
	REC_INIT(&node_slot.s_rec, &p_key, &ksize, &p_val, &vsize);
	bnode_rec(&node_slot);
	COPY_RECORD(&node_slot.s_rec, &bop->bo_rec);
	/* if we need to update vec_count for node, update here */

	bnode_done(&node_slot, true);

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
	bnode_make(&node_slot);
	REC_INIT(&node_slot.s_rec, &p_key, &ksize, &p_val, &vsize);
	bnode_rec(&node_slot);
	COPY_VALUE(&node_slot.s_rec, &bop->bo_rec);
	/* if we need to update vec_count for root slot, update at this place */

	bnode_done(&node_slot, true);
	bnode_seq_cnt_update(lev->l_node);
	bnode_fix(lev->l_node);
	/**
	 * Note : not capturing l_node as it must have already been captured in
	 * btree_put_makespace_phase().
	 */
	btree_node_capture_enlist(oi, oi->i_extra_node, 0);

	/* Increase height by one */
	tree->t_height++;
	bop->bo_arbor->t_height = tree->t_height;
	if (tree->t_height > MAX_TREE_HEIGHT) {
		M0_LOG(M0_ERROR,
		       "Tree height increased beyond MAX_TREE_HEIGHT");
		M0_ASSERT(0);
	}
	/* Capture this change in transaction */

	/* TBD : This check needs to be removed when debugging is done. */
	M0_ASSERT(bnode_expensive_invariant(lev->l_node));
	M0_ASSERT(bnode_expensive_invariant(oi->i_extra_node));
	bnode_unlock(lev->l_node);
	bnode_unlock(oi->i_extra_node);

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
 */
static void btree_put_split_and_find(struct nd *allocated_node,
				     struct nd *current_node,
				     struct m0_btree_rec *rec, struct slot *tgt)
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
	int                      min_rec_count;

	/* intialised slot for left and right node*/
	left_slot.s_node  = allocated_node;
	right_slot.s_node = current_node;

	/* 1)Move some records from current node to new node */

	bnode_set_level(allocated_node, bnode_level(current_node));

	bnode_move(current_node, allocated_node, D_LEFT, NR_EVEN);

	/**
	 * Assert that nodes still contain minimum number of records in the node
	 * required by btree. If Assert fails, increase the node size or
	 * decrease the object size.
	 */
	min_rec_count = bnode_level(current_node) ? 2 : 1;
	M0_ASSERT(bnode_count_rec(current_node) >= min_rec_count);
	M0_ASSERT(bnode_count_rec(allocated_node) >= min_rec_count);
	/*2) Find appropriate slot for given record */

	right_slot.s_idx = 0;
	REC_INIT(&right_slot.s_rec, &p_key, &ksize, &p_val, &vsize);
	bnode_key(&right_slot);

	if (current_node->n_tree->t_keycmp.rko_keycmp != NULL) {
		diff = current_node->n_tree->t_keycmp.rko_keycmp(
				M0_BUFVEC_DATA(&rec->r_key.k_data),
				M0_BUFVEC_DATA(&right_slot.s_rec.r_key.k_data));
	} else {
		m0_bufvec_cursor_init(&cur_1, &rec->r_key.k_data);
		m0_bufvec_cursor_init(&cur_2, &right_slot.s_rec.r_key.k_data);

		diff = m0_bufvec_cursor_cmp(&cur_1, &cur_2);
	}
	tgt->s_node = diff < 0 ? left_slot.s_node : right_slot.s_node;

	/**
	 * Corner case: If given record needs to be inseted at internal left
	 * node and if the key of given record is greater than key at last index
	 * of left record, initialised tgt->s_idx explicitly, as node_find will
	 * not compare key with last indexed key.
	 */
	if (bnode_level(tgt->s_node) > 0 && tgt->s_node == left_slot.s_node) {
		left_slot.s_idx = bnode_count(left_slot.s_node);
		REC_INIT(&left_slot.s_rec, &p_key, &ksize, &p_val, &vsize);
		bnode_key(&left_slot);
		if (current_node->n_tree->t_keycmp.rko_keycmp != NULL) {
			diff = current_node->n_tree->t_keycmp.rko_keycmp(
				 M0_BUFVEC_DATA(&rec->r_key.k_data),
				 M0_BUFVEC_DATA(&left_slot.s_rec.r_key.k_data));
		} else {
			m0_bufvec_cursor_init(&cur_2,
					      &left_slot.s_rec.r_key.k_data);
			diff = m0_bufvec_cursor_cmp(&cur_1, &cur_2);
		}
		if (diff > 0) {
			tgt->s_idx = bnode_count(left_slot.s_node) + 1;
			return;
		}
	}
	bnode_find(tgt, &rec->r_key);
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
	struct m0_btree_oimpl *oi        = bop->bo_i;
	struct level          *lev       = &oi->i_level[oi->i_used];
	m0_bcount_t            ksize;
	void                  *p_key;
	m0_bcount_t            vsize;
	void                  *p_val;
	m0_bcount_t            ksize_1;
	void                  *p_key_1;
	m0_bcount_t            vsize_1;
	void                  *p_val_1;
	m0_bcount_t            newvsize  = INTERNAL_NODE_VALUE_SIZE;
	void                  *newv_ptr;
	struct m0_btree_rec    new_rec;
	struct slot            tgt;
	struct slot            node_slot;
	int                    i;
	int                    vsize_diff = 0;
	int                    rc;

	/**
	 * move records from current node to new node and find slot for given
	 * record
	 */
	M0_PRE(lev->l_alloc != NULL);
	bnode_lock(lev->l_alloc);
	bnode_lock(lev->l_node);

	lev->i_alloc_in_use = true;

	btree_put_split_and_find(lev->l_alloc, lev->l_node, &bop->bo_rec, &tgt);

	if (!oi->i_key_found) {
		/* PUT operation */
		tgt.s_rec = bop->bo_rec;
		/**
		 * Check if crc type of new record is same as crc type of node.
		 * If it is not same, perform upgrade operation for node.
		*/
		bnode_make (&tgt);
		REC_INIT(&tgt.s_rec, &p_key, &ksize, &p_val, &vsize);
		bnode_rec(&tgt);
	} else {
		/* UPDATE operation */
		REC_INIT(&tgt.s_rec, &p_key, &ksize, &p_val, &vsize);
		bnode_rec(&tgt);
		vsize_diff = m0_vec_count(&bop->bo_rec.r_val.ov_vec) -
			     m0_vec_count(&tgt.s_rec.r_val.ov_vec);
		M0_ASSERT(vsize_diff > 0);
		/**
		 * Check if crc type of new record is same as crc type of node.
		 * If it is not same, perform upgrade operation for node.
		*/
		bnode_val_resize(&tgt, vsize_diff);
		bnode_rec(&tgt);
	}

	tgt.s_rec.r_flags = M0_BSC_SUCCESS;
	rc = bop->bo_cb.c_act(&bop->bo_cb, &tgt.s_rec);
	if (rc) {
		/**
		 * Handle callback failure by reverting changes on
		 * btree
		 */
		if (!oi->i_key_found)
			bnode_del(tgt.s_node, tgt.s_idx);
		else
			bnode_val_resize(&tgt, -vsize_diff);

		bnode_done(&tgt, true);
		tgt.s_node == lev->l_node ? bnode_seq_cnt_update(lev->l_node) :
					    bnode_seq_cnt_update(lev->l_alloc);
		bnode_fix(lev->l_node);

		bnode_move(lev->l_alloc, lev->l_node, D_RIGHT, NR_MAX);
		lev->i_alloc_in_use = false;

		bnode_unlock(lev->l_alloc);
		bnode_unlock(lev->l_node);
		lock_op_unlock(bop->bo_arbor->t_desc);
		return fail(bop, rc);
	}
	bnode_done(&tgt, true);
	tgt.s_node == lev->l_node ? bnode_seq_cnt_update(lev->l_node) :
				    bnode_seq_cnt_update(lev->l_alloc);
	bnode_fix(tgt.s_node);
	btree_node_capture_enlist(oi, lev->l_alloc, 0);
	btree_node_capture_enlist(oi, lev->l_node, 0);

	/* TBD : This check needs to be removed when debugging is done. */
	M0_ASSERT(bnode_expensive_invariant(lev->l_alloc));
	M0_ASSERT(bnode_expensive_invariant(lev->l_node));
	bnode_unlock(lev->l_alloc);
	bnode_unlock(lev->l_node);

	/* Initialized new record which will get inserted at parent */
	node_slot.s_node = lev->l_node;
	node_slot.s_idx = 0;
	REC_INIT(&node_slot.s_rec, &p_key, &ksize, &p_val, &vsize);
	bnode_key(&node_slot);
	new_rec.r_key = node_slot.s_rec.r_key;

	newv_ptr      = &(lev->l_alloc->n_addr);
	new_rec.r_val = M0_BUFVEC_INIT_BUF(&newv_ptr, &newvsize);

	for (i = oi->i_used - 1; i >= 0; i--) {
		lev = &oi->i_level[i];
		node_slot.s_node = lev->l_node;
		node_slot.s_idx  = lev->l_idx;
		node_slot.s_rec  = new_rec;
		if (bnode_isfit(&node_slot)) {
			struct m0_btree_rec *rec;

			bnode_lock(lev->l_node);

			bnode_make(&node_slot);
			REC_INIT(&node_slot.s_rec, &p_key_1, &ksize_1,
						   &p_val_1, &vsize_1);
			bnode_rec(&node_slot);
			rec = &new_rec;
			COPY_RECORD(&node_slot.s_rec, rec);

			bnode_done(&node_slot, true);
			bnode_seq_cnt_update(lev->l_node);
			bnode_fix(lev->l_node);
			btree_node_capture_enlist(oi, lev->l_node, lev->l_idx);

			/**
			 * TBD : This check needs to be removed when debugging
			 * is done.
			 */
			M0_ASSERT(bnode_expensive_invariant(lev->l_node));
			bnode_unlock(lev->l_node);
			return P_CAPTURE;
		}

		M0_PRE(lev->l_alloc != NULL);
		bnode_lock(lev->l_alloc);
		bnode_lock(lev->l_node);

		lev->i_alloc_in_use = true;

		btree_put_split_and_find(lev->l_alloc, lev->l_node, &new_rec,
					 &tgt);

		tgt.s_rec = new_rec;
		bnode_make(&tgt);
		REC_INIT(&tgt.s_rec, &p_key_1, &ksize_1, &p_val_1, &vsize_1);
		bnode_rec(&tgt);
		COPY_RECORD(&tgt.s_rec,  &new_rec);

		bnode_done(&tgt, true);
		tgt.s_node == lev->l_node ? bnode_seq_cnt_update(lev->l_node) :
					    bnode_seq_cnt_update(lev->l_alloc);
		bnode_fix(tgt.s_node);
		btree_node_capture_enlist(oi, lev->l_alloc, 0);
		btree_node_capture_enlist(oi, lev->l_node, 0);

		/**
		 * TBD : This check needs to be removed when debugging is
		 * done.
		 */
		M0_ASSERT(bnode_expensive_invariant(lev->l_alloc));
		M0_ASSERT(bnode_expensive_invariant(lev->l_node));
		bnode_unlock(lev->l_alloc);
		bnode_unlock(lev->l_node);

		node_slot.s_node = lev->l_alloc;
		node_slot.s_idx = bnode_count(node_slot.s_node);
		REC_INIT(&node_slot.s_rec, &p_key, &ksize, &p_val, &vsize);
		bnode_key(&node_slot);
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
	int                    vsize_diff     = 0;
	struct level          *lev;

	switch (bop->bo_op.o_sm.sm_state) {
	case P_INIT:
		if (M0_FI_ENABLED("already_exists")) {
			/**
			 * Return error if failure condition is explicitly
			 * enabled by finject Fault Injection while testing.
			 */
			bop->bo_op.o_sm.sm_rc = M0_ERR(-EEXIST);
			return P_DONE;
		}
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
		    !bnode_isoverflow(oi->i_cookie_node, &bop->bo_rec))
			return P_LOCK;
		else
			return P_SETUP;
	case P_LOCKALL:
		M0_ASSERT(bop->bo_flags & BOF_LOCKALL);
		return lock_op_init(&bop->bo_op, &bop->bo_i->i_nop,
				    bop->bo_arbor->t_desc, P_SETUP);
	case P_SETUP:
		oi->i_height = tree->t_height;
		memset(&oi->i_level, 0, sizeof oi->i_level);
		bop->bo_i->i_key_found = false;
		oi->i_nop.no_op.o_sm.sm_rc = 0;
		/** Fall through to P_DOWN. */
	case P_DOWN:
		oi->i_used = 0;
		M0_SET0(&oi->i_capture);
		/* Load root node. */
		return bnode_get(&oi->i_nop, tree, &tree->t_root->n_addr,
				 P_NEXTDOWN);
	case P_NEXTDOWN:
		if (oi->i_nop.no_op.o_sm.sm_rc == 0) {
			struct slot    node_slot = {};
			struct segaddr child_node_addr;

			lev = &oi->i_level[oi->i_used];
			lev->l_node = oi->i_nop.no_node;
			node_slot.s_node = oi->i_nop.no_node;

			bnode_lock(lev->l_node);
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

			if (!bnode_isvalid(lev->l_node) || (oi->i_used > 0 &&
			    bnode_count_rec(lev->l_node) == 0)) {
				bnode_unlock(lev->l_node);
				return m0_sm_op_sub(&bop->bo_op, P_CLEANUP,
						    P_SETUP);
			}

			oi->i_key_found = bnode_find(&node_slot,
						     &bop->bo_rec.r_key);
			lev->l_idx = node_slot.s_idx;
			if (bnode_level(node_slot.s_node) > 0) {
				if (oi->i_key_found) {
					lev->l_idx++;
					node_slot.s_idx++;
				}
				bnode_child(&node_slot, &child_node_addr);
				if (!address_in_segment(child_node_addr)) {
					bnode_unlock(lev->l_node);
					bnode_op_fini(&oi->i_nop);
					return fail(bop, M0_ERR(-EFAULT));
				}
				oi->i_used++;

				if (oi->i_used >= oi->i_height) {
					/* If height of tree increased. */
					oi->i_used = oi->i_height - 1;
					bnode_unlock(lev->l_node);
					return m0_sm_op_sub(&bop->bo_op,
							    P_CLEANUP, P_SETUP);
				}
				bnode_unlock(lev->l_node);
				return bnode_get(&oi->i_nop, tree,
						 &child_node_addr, P_NEXTDOWN);
			} else {
				bnode_unlock(lev->l_node);
				if (oi->i_key_found && bop->bo_opc == M0_BO_PUT)
					return P_LOCK;
				if (!oi->i_key_found &&
				    bop->bo_opc == M0_BO_UPDATE &&
				    !(bop->bo_flags & BOF_INSERT_IF_NOT_FOUND))
					return P_LOCK;
				/**
				 * Initialize i_alloc_lev to level of leaf
				 * node.
				 */
				oi->i_alloc_lev = oi->i_used;
				return P_ALLOC_REQUIRE;
			}
		} else {
			bnode_op_fini(&oi->i_nop);
			return m0_sm_op_sub(&bop->bo_op, P_CLEANUP, P_SETUP);
		}
	case P_ALLOC_REQUIRE:{
		do {
			lev = &oi->i_level[oi->i_alloc_lev];
			bnode_lock(lev->l_node);
			if (!bnode_isvalid(lev->l_node)) {
				bnode_unlock(lev->l_node);
				return m0_sm_op_sub(&bop->bo_op, P_CLEANUP,
						    P_SETUP);
			}

			if (!bnode_isoverflow(lev->l_node, &bop->bo_rec)) {
				bnode_unlock(lev->l_node);
				break;
			}
			if (lev->l_alloc == NULL || (oi->i_alloc_lev == 0 &&
			    oi->i_extra_node == NULL)) {
				/**
				 * Depending on the level of node, shift can be
				 * updated.
				 */
				int ksize   = bnode_keysize(lev->l_node);
				int vsize   = bnode_valsize(lev->l_node);
				int nsize   = bnode_nsize(tree->t_root);
				int crctype = bnode_crctype_get(lev->l_node);
				oi->i_nop.no_opc = NOP_ALLOC;
				bnode_unlock(lev->l_node);
				return bnode_alloc(&oi->i_nop, tree,
						  nsize, lev->l_node->n_type,
						  crctype, ksize, vsize,
						  bop->bo_tx, P_ALLOC_STORE);

			}
			bnode_unlock(lev->l_node);
			oi->i_alloc_lev--;
		} while (oi->i_alloc_lev >= 0);
		return P_LOCK;
	}
	case P_ALLOC_STORE: {
		if (oi->i_nop.no_op.o_sm.sm_rc != 0) {
			if (lock_acquired)
				lock_op_unlock(tree);
			bnode_op_fini(&oi->i_nop);
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
				int nsize;
				int crctype;

				lev->l_alloc = oi->i_nop.no_node;
				oi->i_nop.no_node = NULL;
				bnode_lock(lev->l_node);
				if (!bnode_isvalid(lev->l_node)) {
					bnode_unlock(lev->l_node);
					return m0_sm_op_sub(&bop->bo_op,
							    P_CLEANUP, P_SETUP);
				}
				ksize  = bnode_keysize(lev->l_node);
				vsize   = bnode_valsize(lev->l_node);
				nsize   = bnode_nsize(tree->t_root);
				crctype = bnode_crctype_get(lev->l_node);
				oi->i_nop.no_opc = NOP_ALLOC;
				bnode_unlock(lev->l_node);
				return bnode_alloc(&oi->i_nop, tree,
						  nsize, lev->l_node->n_type,
						  crctype, ksize, vsize,
						  bop->bo_tx, P_ALLOC_STORE);

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
				level_put(oi);
				return P_DOWN;
			}
		}
		/** Fall through if path_check is successful. */
	case P_SANITY_CHECK: {
		int  rc = 0;
		if (oi->i_key_found && bop->bo_opc == M0_BO_PUT)
			rc = M0_ERR(-EEXIST);
		else if (!oi->i_key_found && bop->bo_opc == M0_BO_UPDATE &&
			 !(bop->bo_flags & BOF_INSERT_IF_NOT_FOUND))
			rc = M0_ERR(-ENOENT);

		if (rc) {
			lock_op_unlock(tree);
			return fail(bop, rc);
		}
		return P_MAKESPACE;
	}
	case P_MAKESPACE: {
		struct slot node_slot;
		lev = &oi->i_level[oi->i_used];
		node_slot = (struct slot){
			.s_node = lev->l_node,
			.s_idx  = lev->l_idx,
		};
		if (!oi->i_key_found) {
			M0_ASSERT(bop->bo_opc == M0_BO_PUT ||
				  (bop->bo_opc == M0_BO_UPDATE &&
				   (bop->bo_flags & BOF_INSERT_IF_NOT_FOUND)));

			node_slot.s_rec  = bop->bo_rec;
			if (!bnode_isfit(&node_slot))
				return btree_put_makespace_phase(bop);

			bnode_lock(lev->l_node);
			/**
			 * Check if crc type of new record is same as crc type
			 * of node. If it is not same, perform upgrade operation
			 * for node.
			*/
			bnode_make(&node_slot);
		} else {
			m0_bcount_t          ksize;
			void                *p_key;
			m0_bcount_t          vsize;
			void                *p_val;
			int                  new_vsize;
			int                  old_vsize;

			M0_ASSERT(bop->bo_opc == M0_BO_UPDATE);

			REC_INIT(&node_slot.s_rec, &p_key, &ksize,
						    &p_val, &vsize);
			bnode_rec(&node_slot);

			new_vsize = m0_vec_count(&bop->bo_rec.r_val.ov_vec);
			old_vsize = m0_vec_count(&node_slot.s_rec.r_val.ov_vec);

			vsize_diff = new_vsize - old_vsize;

			if (vsize_diff <= 0 ||
			    bnode_space(lev->l_node) >= vsize_diff) {
				/**
				 * If new value size is able to accomodate in
				 * node.
				 */
				bnode_lock(lev->l_node);
				bnode_val_resize(&node_slot, vsize_diff);
			} else
				return btree_put_makespace_phase(bop);
		}
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

		node_slot.s_node = lev->l_node;
		node_slot.s_idx  = lev->l_idx;

		REC_INIT(&node_slot.s_rec, &p_key, &ksize, &p_val, &vsize);
		bnode_rec(&node_slot);

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
			/**
			 * Handle callback failure by reverting changes on
			 * btree
			 */
			if (bop->bo_opc == M0_BO_PUT)
				bnode_del(node_slot.s_node, node_slot.s_idx);
			else
				bnode_val_resize(&node_slot, -vsize_diff);

			bnode_done(&node_slot, true);
			bnode_seq_cnt_update(lev->l_node);
			bnode_fix(lev->l_node);
			bnode_unlock(lev->l_node);
			lock_op_unlock(tree);
			return fail(bop, rc);
		}
		bnode_done(&node_slot, true);
		bnode_seq_cnt_update(lev->l_node);
		bnode_fix(lev->l_node);
		btree_node_capture_enlist(oi, lev->l_node, lev->l_idx);

		/**
		 * TBD : This check needs to be removed when debugging is
		 * done.
		 */
		M0_ASSERT(bnode_expensive_invariant(lev->l_node));
		bnode_unlock(lev->l_node);
		return P_CAPTURE;
	}
	case P_CAPTURE:
		btree_tx_nodes_capture(oi, bop->bo_tx);
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

static struct m0_sm_state_descr btree_states[P_NR] = {
	[P_INIT] = {
		.sd_flags   = M0_SDF_INITIAL,
		.sd_name    = "P_INIT",
		.sd_allowed = M0_BITS(P_COOKIE, P_SETUP, P_ACT, P_WAITCHECK,
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
				      P_LOCK, P_ACT),
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
		.sd_allowed = M0_BITS(P_CAPTURE, P_CLEANUP, P_NEXTDOWN, P_DONE),
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
	[P_WAITCHECK] = {
		.sd_flags   = 0,
		.sd_name    = "P_WAITCHECK",
		.sd_allowed = M0_BITS(P_WAITCHECK),
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
	{ "close-init-timecheck", P_INIT, P_WAITCHECK},
	{ "close-timecheck-repeat", P_WAITCHECK, P_WAITCHECK},
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
	{ "truncate-nextdown-act", P_NEXTDOWN, P_ACT },
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
	{ "truncate-act-nextdown", P_ACT, P_NEXTDOWN },
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

/**
 * btree_create_tree_tick function is the main function used to create btree.
 * It traverses through multiple states to perform its operation.
 *
 * @param smop     represents the state machine operation
 * @return int64_t returns the next state to be executed.
 */
static int64_t btree_create_tree_tick(struct m0_sm_op *smop)
{
	struct m0_btree_op    *bop    = M0_AMB(bop, smop, bo_op);
	struct m0_btree_oimpl *oi     = bop->bo_i;
	struct m0_btree_idata *data   = &bop->bo_data;
	int                    k_size = data->bt->ksize == -1 ? MAX_KEY_SIZE :
								data->bt->ksize;
	int                    v_size = data->bt->vsize == -1 ? MAX_VAL_SIZE :
								data->bt->vsize;
	struct slot            node_slot;

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

		oi->i_nop.no_addr = segaddr_build(data->addr);
		return bnode_init(&oi->i_nop.no_addr, k_size, v_size,
				  data->num_bytes, data->nt, data->crc_type,
				  bop->bo_seg->bs_gen, data->fid, P_TREE_GET);

	case P_TREE_GET:
		return tree_get(&oi->i_nop, &oi->i_nop.no_addr, P_ACT);

	case P_ACT:
		M0_ASSERT(oi->i_nop.no_op.o_sm.sm_rc == 0);
		oi->i_nop.no_node->n_type = data->nt;
		oi->i_nop.no_node->n_seg  = bop->bo_seg;
		oi->i_nop.no_tree->t_type = data->bt;
		oi->i_nop.no_tree->t_seg  = bop->bo_seg;

		bop->bo_arbor->t_desc     = oi->i_nop.no_tree;
		bop->bo_arbor->t_type     = data->bt;
		bop->bo_arbor->t_height   = bnode_level(oi->i_nop.no_node) + 1;

		m0_rwlock_write_lock(&bop->bo_arbor->t_desc->t_lock);
		bop->bo_arbor->t_desc->t_height = bop->bo_arbor->t_height;
		bop->bo_arbor->t_desc->t_keycmp = bop->bo_keycmp;
		node_slot.s_node                = oi->i_nop.no_node;
		node_slot.s_idx                 = 0;
		bnode_capture(&node_slot, bop->bo_tx);
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
static int64_t btree_destroy_tree_tick(struct m0_sm_op *smop)
{
	struct m0_btree_op *bop  = M0_AMB(bop, smop, bo_op);
	struct td          *tree = bop->bo_arbor->t_desc;
	struct slot         _slot;

	M0_PRE(bop->bo_op.o_sm.sm_state == P_INIT);

	if (tree == NULL)
		return M0_ERR(-EINVAL);

	m0_rwlock_write_lock(&tree->t_lock);
	if (tree->t_ref != 1) {
		m0_rwlock_write_unlock(&tree->t_lock);
		return M0_ERR(-EPERM);
	}

	M0_PRE(bnode_invariant(tree->t_root));
	M0_PRE(bnode_count(tree->t_root) == 0);
	m0_rwlock_write_unlock(&tree->t_lock);
	bnode_fini(tree->t_root);
	_slot.s_node                    = tree->t_root;
	_slot.s_idx                     = 0;
	bnode_capture(&_slot, bop->bo_tx);
	bnode_put(tree->t_root->n_op, tree->t_root);
	tree_put(tree);

	return P_DONE;
}

/**
 * btree_open_tree_tick function is used to traverse through different states to
 * facilitate the working of m0_btree_open().
 *
 * @param smop     represents the state machine operation
 * @return int64_t returns the next state to be executed.
 */
static int64_t btree_open_tree_tick(struct m0_sm_op *smop)
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
		if (!addr_is_aligned(bop->bo_data.addr))
			return M0_ERR(-EFAULT);

		oi = m0_alloc(sizeof *bop->bo_i);
		if (oi == NULL)
			return M0_ERR(-ENOMEM);
		bop->bo_i = oi;
		oi->i_nop.no_addr = segaddr_build(bop->bo_data.addr);

		return tree_get(&oi->i_nop, &oi->i_nop.no_addr, P_ACT);

	case P_ACT:
		M0_ASSERT(oi->i_nop.no_op.o_sm.sm_rc == 0);

		if (!oi->i_nop.no_tree->t_type)
			oi->i_nop.no_tree->t_type = bop->bo_data.bt;

		bop->bo_arbor->t_type           = oi->i_nop.no_tree->t_type;
		bop->bo_arbor->t_desc           = oi->i_nop.no_tree;
		bop->bo_arbor->t_height         = oi->i_nop.no_tree->t_height;
		bop->bo_arbor->t_desc->t_seg    = bop->bo_seg;
		bop->bo_arbor->t_desc->t_fid    = oi->i_nop.no_tree->t_fid;
		bop->bo_arbor->t_desc->t_keycmp = bop->bo_keycmp;

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
static int64_t btree_close_tree_tick(struct m0_sm_op *smop)
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
			bnode_put(tree->t_root->n_op, tree->t_root);
			tree_put(tree);
			return P_DONE;
		}
		m0_rwlock_write_unlock(&tree->t_lock);

		/** put tree's root node. */
		bnode_put(tree->t_root->n_op, tree->t_root);
		/** Fallthrough to P_WAITCHECK */
	case P_WAITCHECK:
		m0_rwlock_write_lock(&list_lock);
		m0_tl_for(ndlist, &btree_active_nds, node) {
			if (node->n_tree == tree && node->n_ref > 0) {
				m0_rwlock_write_unlock(&list_lock);
				return P_WAITCHECK;
			}
		} m0_tl_endfor;
		m0_rwlock_write_unlock(&list_lock);
		/** Fallthrough to P_ACT */
	case P_ACT:
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
	return (lev->l_idx >= 0) && (lev->l_idx < bnode_count(lev->l_node));
}

/**
 *  Search from the leaf + 1 level till the root level and find a node
 *  which has valid sibling. Once found, get the leftmost leaf record from the
 *  sibling subtree.
 */
static int  btree_sibling_first_key(struct m0_btree_oimpl *oi, struct td *tree,
				    struct slot *s)
{
	int             i;
	struct level   *lev;
	struct segaddr  child;

	for (i = oi->i_used - 1; i >= 0; i--) {
		lev = &oi->i_level[i];
		if (lev->l_idx < bnode_count(lev->l_node)) {
			s->s_node = oi->i_nop.no_node = lev->l_node;
			s->s_idx = lev->l_idx + 1;
			while (i != oi->i_used) {
				bnode_child(s, &child);
				if (!address_in_segment(child))
					return M0_ERR(-EFAULT);
				i++;
				bnode_get(&oi->i_nop, tree, &child, P_CLEANUP);
				s->s_idx = 0;
				s->s_node = oi->i_nop.no_node;
				oi->i_level[i].l_sibling = oi->i_nop.no_node;
			}
			bnode_rec(s);
			return 0;
		}
	}
	return M0_ERR(-ENOENT);
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
		memset(&oi->i_level, 0, sizeof oi->i_level);
		oi->i_nop.no_op.o_sm.sm_rc = 0;
		/** Fall through to P_DOWN. */
	case P_DOWN:
		oi->i_used = 0;
		return bnode_get(&oi->i_nop, tree, &tree->t_root->n_addr,
				 P_NEXTDOWN);
	case P_NEXTDOWN:
		if (oi->i_nop.no_op.o_sm.sm_rc == 0) {
			struct slot    s = {};
			struct segaddr child;

			lev = &oi->i_level[oi->i_used];
			lev->l_node = oi->i_nop.no_node;
			s.s_node = oi->i_nop.no_node;

			bnode_lock(lev->l_node);
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
			if (!bnode_isvalid(lev->l_node) || (oi->i_used > 0 &&
			    bnode_count_rec(lev->l_node) == 0)) {
				bnode_unlock(lev->l_node);
				return m0_sm_op_sub(&bop->bo_op, P_CLEANUP,
						    P_SETUP);
			}

			if (bop->bo_opc == M0_BO_GET) {
				oi->i_key_found =
					    bnode_find(&s, &bop->bo_rec.r_key);
				lev->l_idx = s.s_idx;
			}

			if (bnode_level(s.s_node) > 0) {
				if (bop->bo_opc == M0_BO_GET) {
					if (oi->i_key_found) {
						s.s_idx++;
						lev->l_idx++;
					}
				} else
					s.s_idx = bop->bo_opc == M0_BO_MINKEY ?
						  0 : bnode_count(s.s_node);

				bnode_child(&s, &child);
				if (!address_in_segment(child)) {
					bnode_unlock(lev->l_node);
					bnode_op_fini(&oi->i_nop);
					return fail(bop, M0_ERR(-EFAULT));
				}
				oi->i_used++;
				if (oi->i_used >= oi->i_height) {
					/* If height of tree increased. */
					oi->i_used = oi->i_height - 1;
					bnode_unlock(lev->l_node);
					return m0_sm_op_sub(&bop->bo_op,
							    P_CLEANUP, P_SETUP);
				}
				bnode_unlock(lev->l_node);
				return bnode_get(&oi->i_nop, tree, &child,
						 P_NEXTDOWN);
			} else {
				bnode_unlock(lev->l_node);
				return P_LOCK;
			}
		} else {
			bnode_op_fini(&oi->i_nop);
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
				level_put(oi);
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
		s.s_rec.r_flags = M0_BSC_SUCCESS;
		REC_INIT(&s.s_rec, &pkey, &ksize, &pval, &vsize);

		count = bnode_count(s.s_node);
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
			if (oi->i_key_found)
				bnode_rec(&s);
			else if (bop->bo_flags & BOF_EQUAL) {
				lock_op_unlock(tree);
				return fail(bop, M0_ERR(-ENOENT));
			} else { /** bop->bo_flags & BOF_SLANT */
				if (lev->l_idx < count)
					bnode_rec(&s);
				else {
					rc = btree_sibling_first_key(oi, tree,
								     &s);
					if (rc != 0) {
						bnode_op_fini(&oi->i_nop);
						lock_op_unlock(tree);
						return fail(bop, rc);
					}
				}
			}
		} else {
			/** MIN/MAX key operation. */
			if (count > 0) {
				s.s_idx = bop->bo_opc == M0_BO_MINKEY ? 0 :
					  count - 1;
				bnode_rec(&s);
			} else {
				/** Only root node is present and is empty. */
				lock_op_unlock(tree);
				return fail(bop, M0_ERR(-ENOENT));
			}
		}

		rc = bop->bo_cb.c_act(&bop->bo_cb, &s.s_rec);

		lock_op_unlock(tree);
		if (rc != 0)
			return fail(bop, rc);
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
static int64_t btree_iter_kv_tick(struct m0_sm_op *smop)
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
		memset(&oi->i_level, 0, sizeof oi->i_level);
		oi->i_nop.no_op.o_sm.sm_rc = 0;
		/** Fall through to P_DOWN. */
	case P_DOWN:
		oi->i_used  = 0;
		oi->i_pivot = -1;
		return bnode_get(&oi->i_nop, tree, &tree->t_root->n_addr,
				 P_NEXTDOWN);
	case P_NEXTDOWN:
		if (oi->i_nop.no_op.o_sm.sm_rc == 0) {
			struct slot    s = {};
			struct segaddr child;

			lev = &oi->i_level[oi->i_used];
			lev->l_node = oi->i_nop.no_node;
			s.s_node = oi->i_nop.no_node;

			bnode_lock(lev->l_node);
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
			if (!bnode_isvalid(lev->l_node) || (oi->i_used > 0 &&
			    bnode_count_rec(lev->l_node) == 0)) {
				bnode_unlock(lev->l_node);
				return m0_sm_op_sub(&bop->bo_op, P_CLEANUP,
						    P_SETUP);
			}

			oi->i_key_found = bnode_find(&s, &bop->bo_rec.r_key);
			lev->l_idx = s.s_idx;

			if (bnode_level(s.s_node) > 0) {
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
				    (lev->l_idx < bnode_count(lev->l_node))) ||
				    ((bop->bo_flags & BOF_PREV) &&
				    (lev->l_idx > 0)))
					oi->i_pivot = oi->i_used;

				bnode_child(&s, &child);
				if (!address_in_segment(child)) {
					bnode_unlock(lev->l_node);
					bnode_op_fini(&oi->i_nop);
					return fail(bop, M0_ERR(-EFAULT));
				}
				oi->i_used++;
				if (oi->i_used >= oi->i_height) {
					/* If height of tree increased. */
					oi->i_used = oi->i_height - 1;
					bnode_unlock(lev->l_node);
					return m0_sm_op_sub(&bop->bo_op,
							    P_CLEANUP, P_SETUP);
				}
				bnode_unlock(lev->l_node);
				return bnode_get(&oi->i_nop, tree, &child,
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
					bnode_unlock(lev->l_node);
					return P_LOCK;
				}
				bnode_unlock(lev->l_node);
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
				bnode_lock(lev->l_node);
				if (!bnode_isvalid(lev->l_node) ||
				    (oi->i_pivot > 0 &&
				     bnode_count_rec(lev->l_node) == 0)) {
					bnode_unlock(lev->l_node);
					bnode_op_fini(&oi->i_nop);
					return m0_sm_op_sub(&bop->bo_op,
							    P_CLEANUP, P_SETUP);
				}
				if (lev->l_seq != lev->l_node->n_seq) {
					bnode_unlock(lev->l_node);
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

				bnode_child(&s, &child);
				if (!address_in_segment(child)) {
					bnode_unlock(lev->l_node);
					bnode_op_fini(&oi->i_nop);
					return fail(bop, M0_ERR(-EFAULT));
				}
				oi->i_pivot++;
				bnode_unlock(lev->l_node);
				return bnode_get(&oi->i_nop, tree, &child,
						 P_SIBLING);
			}
		} else {
			bnode_op_fini(&oi->i_nop);
			return m0_sm_op_sub(&bop->bo_op, P_CLEANUP, P_SETUP);
		}
	case P_SIBLING:
		if (oi->i_nop.no_op.o_sm.sm_rc == 0) {
			struct slot    s = {};
			struct segaddr child;

			lev = &oi->i_level[oi->i_pivot];
			lev->l_sibling = oi->i_nop.no_node;
			s.s_node = oi->i_nop.no_node;
			bnode_lock(lev->l_sibling);
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
			if (!bnode_isvalid(lev->l_sibling) ||
			    (oi->i_pivot > 0 &&
			     bnode_count_rec(lev->l_sibling) == 0)) {
				bnode_unlock(lev->l_sibling);
				return m0_sm_op_sub(&bop->bo_op, P_CLEANUP,
						    P_SETUP);
			}

			if (bnode_level(s.s_node) > 0) {
				s.s_idx = (bop->bo_flags & BOF_NEXT) ? 0 :
					  bnode_count(s.s_node);
				bnode_child(&s, &child);
				if (!address_in_segment(child)) {
					bnode_unlock(lev->l_sibling);
					bnode_op_fini(&oi->i_nop);
					return fail(bop, M0_ERR(-EFAULT));
				}
				oi->i_pivot++;
				if (oi->i_pivot >= oi->i_height) {
					/* If height of tree increased. */
					bnode_unlock(lev->l_sibling);
					return m0_sm_op_sub(&bop->bo_op,
							    P_CLEANUP, P_SETUP);
				}
				bnode_unlock(lev->l_sibling);
				return bnode_get(&oi->i_nop, tree, &child,
						 P_SIBLING);
			} else {
				bnode_unlock(lev->l_sibling);
				return P_LOCK;
			}
		} else {
			bnode_op_fini(&oi->i_nop);
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
				level_put(oi);
				return P_DOWN;
			}
		}
		/**
		 * Fall through if path_check and sibling_node_check are
		 * successful.
		 */
	case P_ACT: {
		int			 rc;
		m0_bcount_t		 ksize;
		m0_bcount_t		 vsize;
		void			*pkey;
		void			*pval;
		struct slot		 s = {};

		lev = &oi->i_level[oi->i_used];

		REC_INIT(&s.s_rec, &pkey, &ksize, &pval, &vsize);
		s.s_rec.r_flags = M0_BSC_SUCCESS;

		/* Return record if idx fit in the node. */
		if (index_is_valid(lev)) {
			s.s_node = lev->l_node;
			s.s_idx  = lev->l_idx;
			bnode_rec(&s);
		} else if (oi->i_pivot == -1) {
			/* Handle rightmost/leftmost key case. */
			lock_op_unlock(tree);
			return fail(bop, M0_ERR(-ENOENT));
		} else {
			/* Return sibling record based on flag. */
			s.s_node = lev->l_sibling;
			s.s_idx = (bop->bo_flags & BOF_NEXT) ? 0 :
				  bnode_count(s.s_node) - 1;
			bnode_rec(&s);
		}
		rc = bop->bo_cb.c_act(&bop->bo_cb, &s.s_rec);
		lock_op_unlock(tree);
		if (rc != 0)
			return fail(bop, rc);
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
		bnode_lock(lev->l_node);

		bnode_del(lev->l_node, lev->l_idx);
		node_slot.s_node = lev->l_node;
		node_slot.s_idx  = lev->l_idx;
		bnode_done(&node_slot, false);

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
			if (bnode_count_rec(lev->l_node) > 1)
				flag = true;
			else if (bnode_count_rec(lev->l_node) == 0) {
				bnode_set_level(lev->l_node, 0);
				tree->t_height = 1;
				bop->bo_arbor->t_height = tree->t_height;
				/* Capture this change in transaction */
				flag = true;
			} else
				break;
		}
		bnode_seq_cnt_update(lev->l_node);
		bnode_fix(node_slot.s_node);
		btree_node_capture_enlist(oi, lev->l_node, lev->l_idx);
		/**
		 * TBD : This check needs to be removed when debugging is
		 * done.
		 */
		M0_ASSERT(bnode_expensive_invariant(lev->l_node));

		node_underflow = bnode_isunderflow(lev->l_node, false);
		if (used_count != 0 && node_underflow) {
			bnode_fini(lev->l_node);
			lev->l_freenode = true;
		}

		bnode_unlock(lev->l_node);

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

	curr_root_level  = bnode_level(lev->l_node);
	root_slot.s_node = lev->l_node;
	root_slot.s_idx  = 0;
	bnode_del(lev->l_node, 0);
	bnode_done(&root_slot, false);

	/* l_sib is node below root which is root's only child */
	root_child = oi->i_level[1].l_sibling;
	bnode_lock(root_child);

	bnode_set_level(lev->l_node, curr_root_level - 1);
	tree->t_height--;
	bop->bo_arbor->t_height = tree->t_height;
	/* Capture this change in transaction */

	bnode_move(root_child, lev->l_node, D_RIGHT, NR_MAX);
	M0_ASSERT(bnode_count_rec(root_child) == 0);
	btree_node_capture_enlist(oi, lev->l_node, 0);
	btree_node_capture_enlist(oi, root_child, 0);
	oi->i_root_child_free = true;

	/* TBD : This check needs to be removed when debugging is done. */
	M0_ASSERT(bnode_expensive_invariant(lev->l_node));
	bnode_unlock(lev->l_node);
	bnode_unlock(root_child);

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
		bnode_lock(l_node);
		if (!bnode_isvalid(l_node)) {
			bnode_unlock(l_node);
			return false;
		}
		if (oi->i_level[1].l_sib_seq != l_node->n_seq) {
			bnode_unlock(l_node);
			return false;
		}
		bnode_unlock(l_node);
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
		bnode_lock(lev->l_node);
		if (!bnode_isvalid(lev->l_node)) {
			bnode_unlock(lev->l_node);
			return -1;
		}
		if (used_count == 0) {
			if (bnode_count_rec(lev->l_node) == 2)
				load = 1;
			bnode_unlock(lev->l_node);
			break;
		}
		if (!bnode_isunderflow(lev->l_node, true)) {
			bnode_unlock(lev->l_node);
			break;
		}
		bnode_unlock(lev->l_node);
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

		bnode_lock(root_lev->l_node);

		if (!bnode_isvalid(root_lev->l_node) ||
		    root_lev->l_node->n_seq != root_lev->l_seq) {
			bnode_unlock(root_lev->l_node);
			return m0_sm_op_sub(&bop->bo_op, P_CLEANUP, P_SETUP);
		}
		root_slot.s_node = root_lev->l_node;
		root_slot.s_idx  = root_lev->l_idx == 0 ? 1 : 0;

		bnode_child(&root_slot, &root_child);
		if (!address_in_segment(root_child)) {
			bnode_unlock(root_lev->l_node);
			bnode_op_fini(&oi->i_nop);
			return fail(bop, M0_ERR(-EFAULT));
		}
		bnode_unlock(root_lev->l_node);
		return bnode_get(&oi->i_nop, bop->bo_arbor->t_desc,
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
		    !bnode_isunderflow(oi->i_cookie_node, true))
			return P_LOCK;
		else
			return P_SETUP;
	case P_LOCKALL:
		M0_ASSERT(bop->bo_flags & BOF_LOCKALL);
		return lock_op_init(&bop->bo_op, &bop->bo_i->i_nop,
				    bop->bo_arbor->t_desc, P_SETUP);
	case P_SETUP:
		oi->i_height = tree->t_height;
		memset(&oi->i_level, 0, sizeof oi->i_level);
		bop->bo_i->i_key_found = false;
		oi->i_nop.no_op.o_sm.sm_rc = 0;
		/** Fall through to P_DOWN. */
	case P_DOWN:
		oi->i_used = 0;
		M0_SET0(&oi->i_capture);
		/* Load root node. */
		return bnode_get(&oi->i_nop, tree, &tree->t_root->n_addr,
				 P_NEXTDOWN);
	case P_NEXTDOWN:
		if (oi->i_nop.no_op.o_sm.sm_rc == 0) {
			struct slot    node_slot = {};
			struct segaddr child_node_addr;

			lev = &oi->i_level[oi->i_used];
			lev->l_node = oi->i_nop.no_node;
			node_slot.s_node = oi->i_nop.no_node;

			bnode_lock(lev->l_node);
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
			if (!bnode_isvalid(lev->l_node) || (oi->i_used > 0 &&
			    bnode_count_rec(lev->l_node) == 0)) {
				bnode_unlock(lev->l_node);
				return m0_sm_op_sub(&bop->bo_op, P_CLEANUP,
						    P_SETUP);
			}

			oi->i_key_found = bnode_find(&node_slot,
						     &bop->bo_rec.r_key);
			lev->l_idx = node_slot.s_idx;

			if (bnode_level(node_slot.s_node) > 0) {
				if (oi->i_key_found) {
					lev->l_idx++;
					node_slot.s_idx++;
				}
				bnode_child(&node_slot, &child_node_addr);

				if (!address_in_segment(child_node_addr)) {
					bnode_unlock(lev->l_node);
					bnode_op_fini(&oi->i_nop);
					return fail(bop, M0_ERR(-EFAULT));
				}
				oi->i_used++;
				if (oi->i_used >= oi->i_height) {
					/* If height of tree increased. */
					oi->i_used = oi->i_height - 1;
					bnode_unlock(lev->l_node);
					return m0_sm_op_sub(&bop->bo_op,
							    P_CLEANUP, P_SETUP);
				}
				bnode_unlock(lev->l_node);
				return bnode_get(&oi->i_nop, tree,
						 &child_node_addr, P_NEXTDOWN);
			} else {
				bnode_unlock(lev->l_node);
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
					bnode_lock(root_node);
					if (bnode_count_rec(root_node) == 2) {
						bnode_unlock(root_node);
						return root_case_handle(bop);
					}
					bnode_unlock(root_node);
				}

				return P_LOCK;
			}
		} else {
			bnode_op_fini(&oi->i_nop);
			return m0_sm_op_sub(&bop->bo_op, P_CLEANUP, P_SETUP);
		}
	case P_STORE_CHILD: {
		if (oi->i_nop.no_op.o_sm.sm_rc == 0) {
			struct nd *root_child;

			oi->i_level[1].l_sibling = oi->i_nop.no_node;
			root_child = oi->i_level[1].l_sibling;
			bnode_lock(root_child);

			oi->i_level[1].l_sib_seq = oi->i_nop.no_node->n_seq;
			oi->i_nop.no_node = NULL;

			if (!bnode_isvalid(root_child) ||
			    bnode_count_rec(root_child) == 0) {
				bnode_unlock(root_child);
 				return m0_sm_op_sub(&bop->bo_op, P_CLEANUP,
						    P_SETUP);
			}

			bnode_unlock(root_child);
			/* Fall through to the next step */
		} else {
			bnode_op_fini(&oi->i_nop);
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
				level_put(oi);
				return P_DOWN;
			}
		}
		/**
		 * Fall through if path_check and child_node_check are
		 * successful.
		 */
	case P_ACT: {

		struct slot         node_slot;
		bool                node_underflow;
		int                 rc;
		/**
		 *  if key exists, delete the key, if there is an underflow, go
		 *  to resolve function else return P_CLEANUP.
		*/

		if (!oi->i_key_found) {
			lock_op_unlock(tree);
			return fail(bop, M0_ERR(-ENOENT));
		}

		lev = &oi->i_level[oi->i_used];

		node_slot.s_node = lev->l_node;
		node_slot.s_idx  = lev->l_idx;

		if (bop->bo_cb.c_act != NULL) {
			m0_bcount_t          ksize;
			void                *p_key;
			m0_bcount_t          vsize;
			void                *p_val;

			REC_INIT(&node_slot.s_rec, &p_key, &ksize,
						    &p_val, &vsize);
			bnode_rec(&node_slot);
			node_slot.s_rec.r_flags = M0_BSC_SUCCESS;
			rc = bop->bo_cb.c_act(&bop->bo_cb, &node_slot.s_rec);
			if (rc) {
				lock_op_unlock(tree);
				return fail(bop, rc);
			}
		}

		bnode_lock(lev->l_node);

		bnode_del(node_slot.s_node, node_slot.s_idx);
		bnode_done(&node_slot, false);
		bnode_seq_cnt_update(lev->l_node);
		bnode_fix(node_slot.s_node);
		btree_node_capture_enlist(oi, lev->l_node, lev->l_idx);
		/**
		 * TBD : This check needs to be removed when debugging
		 * is done.
		 */
		M0_ASSERT(bnode_expensive_invariant(lev->l_node));
		node_underflow = bnode_isunderflow(lev->l_node, false);
		if (oi->i_used != 0  && node_underflow) {
			bnode_fini(lev->l_node);
			lev->l_freenode = true;
		}

		bnode_unlock(lev->l_node);

		if (oi->i_used == 0 || !node_underflow)
			return P_CAPTURE; /* No Underflow */

		return btree_del_resolve_underflow(bop);
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
				bnode_free(&oi->i_nop, lev->l_node,
					   bop->bo_tx, 0);
				lev->l_node = NULL;
			} else
				break;
		}
		if (oi->i_root_child_free) {
			lev = &oi->i_level[1];
			oi->i_nop.no_opc = NOP_FREE;
			bnode_free(&oi->i_nop, lev->l_sibling, bop->bo_tx, 0);
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
 * State machine for truncate operation.
 */
static int64_t btree_truncate_tick(struct m0_sm_op *smop)
{
	struct m0_btree_op    *bop            = M0_AMB(bop, smop, bo_op);
	struct td             *tree           = bop->bo_arbor->t_desc;
	struct m0_btree_oimpl *oi             = bop->bo_i;
	struct level          *lev;

	switch (bop->bo_op.o_sm.sm_state) {
	case P_INIT:
		M0_ASSERT(bop->bo_i == NULL);
		bop->bo_i = m0_alloc(sizeof *oi);
		if (bop->bo_i == NULL) {
			bop->bo_op.o_sm.sm_rc = M0_ERR(-ENOMEM);
			return P_DONE;
		}
		/** Fall through to P_LOCKDOWN. */
	case P_LOCKALL:
		return lock_op_init(&bop->bo_op, &bop->bo_i->i_nop,
				    bop->bo_arbor->t_desc, P_SETUP);
	case P_SETUP:
		oi->i_height = tree->t_height;
		memset(&oi->i_level, 0, sizeof oi->i_level);
		oi->i_nop.no_op.o_sm.sm_rc = 0;
		/** Fall through to P_DOWN. */
	case P_DOWN:
		oi->i_used = 0;
		return bnode_get(&oi->i_nop, tree, &tree->t_root->n_addr,
				 P_NEXTDOWN);
	case P_NEXTDOWN:
		if (oi->i_nop.no_op.o_sm.sm_rc == 0) {
			struct slot    s = {};
			struct segaddr child;

			lev = &oi->i_level[oi->i_used];
			lev->l_node = oi->i_nop.no_node;
			s.s_node = oi->i_nop.no_node;

			bnode_lock(lev->l_node);
			lev->l_seq = lev->l_node->n_seq;

			if (bnode_level(s.s_node) > 0) {
				s.s_idx = bnode_count(s.s_node);

				bnode_child(&s, &child);
				if (!address_in_segment(child)) {
					bnode_unlock(lev->l_node);
					bnode_op_fini(&oi->i_nop);
					lock_op_unlock(tree);
					return fail(bop, M0_ERR(-EFAULT));
				}
				oi->i_used++;
				bnode_unlock(lev->l_node);
				return bnode_get(&oi->i_nop, tree, &child,
						 P_NEXTDOWN);
			} else {
				bnode_unlock(lev->l_node);
				return P_ACT;
			}
		} else {
			bnode_op_fini(&oi->i_nop);
			lock_op_unlock(tree);
			return m0_sm_op_sub(&bop->bo_op, P_CLEANUP, P_FINI);
		}
	case P_ACT: {
		int           rec_count;
		struct slot   node_slot = {};
		struct level *parent;

		lev = &oi->i_level[oi->i_used];

		/**
		 * lev->l_node points to the node that needs to be truncated.
		 * Set record count as zero and handle root/non-root case.
		 */
		bnode_set_rec_count(lev->l_node, 0);
		node_slot.s_node = lev->l_node;
		node_slot.s_idx  = 0;
		/**
		 * Case: root node as leaf node.
		 * Mark it as leaf node, capture in transaction and exit state
		 * machine.
		 */
		if (oi->i_used == 0) {
			bnode_set_level(lev->l_node, 0);
			bnode_capture(&node_slot, bop->bo_tx);
			lock_op_unlock(tree);
			return m0_sm_op_sub(&bop->bo_op, P_CLEANUP, P_FINI);
		}
		/**
		 * Case: Tree has more than one level.
		 * Tree traversal has reached the leaf node. Delete the leaf
		 * node.
		 */
		bnode_fini(lev->l_node);
		bnode_capture(&node_slot, bop->bo_tx);
		bnode_free(&oi->i_nop, lev->l_node, bop->bo_tx, 0);
		lev->l_node = NULL;
		bop->bo_limit--;

		/** Decrease parent's record count. */
		oi->i_used--;
		parent = &oi->i_level[oi->i_used];
		oi->i_nop.no_node = parent->l_node;
		rec_count = bnode_count_rec(parent->l_node);
		rec_count--;
		bnode_set_rec_count(parent->l_node, rec_count);

		/**
		 * If parent's record count is zero then mark it as leaf node.
		 */
		if (rec_count == 0) {
			bnode_set_level(parent->l_node, 0);
			node_slot.s_node = parent->l_node;
			node_slot.s_idx  = 0;
			bnode_capture(&node_slot, bop->bo_tx);
			bop->bo_limit--;
		}
		/** Exit state machine if ran out of credits. */
		if (bop->bo_limit <= 2) {
			node_slot.s_node = parent->l_node;
			node_slot.s_idx  = rec_count;
			bnode_capture(&node_slot, bop->bo_tx);
			lock_op_unlock(tree);
			return m0_sm_op_sub(&bop->bo_op, P_CLEANUP, P_FINI);
		}
		/** Still have credits, retraverse the tree from parent. */
		return P_NEXTDOWN;
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

#ifndef __KERNEL__
static int unmap_node(void* addr, int64_t size)
{
	return munmap(addr, size);
}

static int remap_node(void* addr, int64_t size, struct m0_be_seg *seg)
{
	void *p;
	m0_bcount_t file_offset = (m0_bcount_t)(addr - seg->bs_addr);
	p = mmap(addr, size, PROT_READ | PROT_WRITE,
		 MAP_FIXED | MAP_PRIVATE | MAP_NORESERVE,
		 m0_stob_fd(seg->bs_stob), file_offset);
	if (p == MAP_FAILED)
		return M0_ERR(-EFAULT);
	return 0;
}

/**
 * This function will try to unmap and remap the nodes in LRU list to free up
 * virtual page memory. The amount of memory to be freed will be given, and
 * attempt will be made to free up the requested size.
 *
 * @param size the total size in bytes to be freed from the swap.
 *
 * @return int the total size in bytes that was freed.
 */
M0_INTERNAL int64_t m0_btree_lrulist_purge(int64_t size)
{
	struct nd              *node;
	struct nd              *prev;
	int64_t                 curr_size;
	int64_t                 total_size = 0;
	void                   *rnode;
	struct m0_be_seg       *seg;
	struct m0_be_allocator *a;
	int                     rc;

	m0_rwlock_write_lock(&list_lock);
	node = ndlist_tlist_tail(&btree_lru_nds);
	while (node != NULL && size > 0) {
		curr_size = 0;
		prev      = ndlist_tlist_prev(&btree_lru_nds, node);
		if (node->n_txref == 0 && node->n_ref == 0) {
			curr_size = node->n_size + m0_be_chunk_header_size();
			seg       = node->n_seg;
			a         = m0_be_seg_allocator(seg);
			rnode     = segaddr_addr(&node->n_addr);
			rnode    -= m0_be_chunk_header_size();

			m0_mutex_lock(&a->ba_lock);
			rc = unmap_node(rnode, curr_size);
			if (rc == 0) {
				rc = remap_node(rnode, curr_size, seg);
				if (rc == 0) {
					size       -= curr_size;
					total_size += curr_size;
					ndlist_tlink_del_fini(node);
					lru_space_used -= curr_size;
					m0_rwlock_fini(&node->n_lock);
					m0_free(node);
				} else
					M0_LOG(M0_ERROR,
					       "Remapping of memory failed");
			} else
				M0_LOG(M0_ERROR, "Unmapping of memory failed");
			m0_mutex_unlock(&a->ba_lock);
		}
		node = prev;
	}
	m0_rwlock_write_unlock(&list_lock);
	return total_size;
}

/**
 * Checks whether lru list purging is required based on used space
 * watermark(enum m0_btree_used_space_watermark).
 *
 * @param user purge user
 * @param size requested memory to be freed.
 *
 * @return int total freed memory.
 */
M0_INTERNAL int64_t m0_btree_lrulist_purge_check(enum m0_btree_purge_user user,
						 int64_t size)
{
	int64_t size_to_purge;
	int64_t purged_size = 0;

	if (lru_space_used < lru_space_wm_low) {
		/** Do nothing. */
		if (user == M0_PU_EXTERNAL)
			M0_LOG(M0_INFO, "Skipping memory release since used "
			       "space is below threshold requested size=%"PRId64
			       " used space=%"PRId64, size, lru_space_used);
		return 0;
	}
	if (lru_space_used < lru_space_wm_high) {
		/**
		 * If user is btree then do nothing. For external user,
		 * purge lrulist till low watermark or size whichever is
		 * higher.
		 */
		if (user == M0_PU_EXTERNAL) {
			size_to_purge = min64(lru_space_used - lru_space_wm_low,
					      size);
			purged_size = m0_btree_lrulist_purge(size_to_purge);
			M0_LOG(M0_INFO, " Below critical External user Purge,"
			       " requested size=%"PRId64" used space=%"PRId64
			       " purged size=%"PRId64, size, lru_space_used,
			       purged_size);
		}
		return purged_size;
	}
	/**
	 * If user is btree then purge lru list till lru_space_used reaches
	 * target watermark. For external user, purge lrulist till low watermark
	 * or size whichever is higher.
	 */
	size_to_purge = user == M0_PU_BTREE ?
				(lru_space_used - lru_space_wm_target) :
				min64(lru_space_used - lru_space_wm_low, size);
	purged_size = m0_btree_lrulist_purge(size_to_purge);
	M0_LOG(M0_INFO, " Above critical purge, User=%s requested size="
	       "%"PRId64" used space=%"PRIu64" purged size="
	       "%"PRIu64, user == M0_PU_BTREE ? "btree" : "external", size,
	       lru_space_used, purged_size);
	return purged_size;
}
#endif

M0_INTERNAL int  m0_btree_open(void *addr, int nob, struct m0_btree *out,
			       struct m0_be_seg *seg, struct m0_btree_op *bop,
			       struct m0_btree_rec_key_op *keycmp)
{
	bop->bo_data.addr      = addr;
	bop->bo_data.num_bytes = nob;
	bop->bo_arbor          = out;
	bop->bo_seg            = seg;
	if (keycmp == NULL)
		bop->bo_keycmp  = (struct m0_btree_rec_key_op){
							    .rko_keycmp = NULL,
							};
	else
		bop->bo_keycmp  = *keycmp;

	m0_sm_op_init(&bop->bo_op, &btree_open_tree_tick, &bop->bo_op_exec,
		      &btree_conf, &bop->bo_sm_group);
	return 0;
}

M0_INTERNAL void m0_btree_close(struct m0_btree *arbor, struct m0_btree_op *bop)
{
	bop->bo_arbor = arbor;
	m0_sm_op_init(&bop->bo_op, &btree_close_tree_tick, &bop->bo_op_exec,
		      &btree_conf, &bop->bo_sm_group);
}

M0_INTERNAL void m0_btree_create(void *addr, int nob,
				 const struct m0_btree_type *bt,
				 enum m0_btree_crc_type crc_type,
				 struct m0_btree_op *bop, struct m0_btree *tree,
				 struct m0_be_seg *seg,
				 const struct m0_fid *fid, struct m0_be_tx *tx,
				 struct m0_btree_rec_key_op *keycmp)
{
	bop->bo_data.addr       = addr;
	bop->bo_data.num_bytes  = nob;
	bop->bo_data.bt         = bt;
	bop->bo_data.nt         = btree_nt_from_bt(bt);
	bop->bo_data.crc_type   = crc_type;
	bop->bo_data.fid        = *fid;
	bop->bo_tx              = tx;
	bop->bo_seg             = seg;
	bop->bo_arbor           = tree;
	if (keycmp == NULL)
		bop->bo_keycmp  = (struct m0_btree_rec_key_op){
							    .rko_keycmp = NULL,
							};
	else
		bop->bo_keycmp  = *keycmp;

	m0_sm_op_init(&bop->bo_op, &btree_create_tree_tick, &bop->bo_op_exec,
		      &btree_conf, &bop->bo_sm_group);
}

M0_INTERNAL void m0_btree_destroy(struct m0_btree *arbor,
				  struct m0_btree_op *bop, struct m0_be_tx *tx)
{
	bop->bo_arbor = arbor;
	bop->bo_tx    = tx;
	bop->bo_seg   = arbor->t_desc->t_seg;

	m0_sm_op_init(&bop->bo_op, &btree_destroy_tree_tick, &bop->bo_op_exec,
		      &btree_conf, &bop->bo_sm_group);
}

M0_INTERNAL void m0_btree_get(struct m0_btree *arbor,
			      const struct m0_btree_key *key,
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

M0_INTERNAL void m0_btree_iter(struct m0_btree *arbor,
			       const struct m0_btree_key *key,
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

M0_INTERNAL void m0_btree_put(struct m0_btree *arbor,
			      const struct m0_btree_rec *rec,
			      const struct m0_btree_cb *cb,
			      struct m0_btree_op *bop, struct m0_be_tx *tx)
{
	bop->bo_opc    = M0_BO_PUT;
	bop->bo_arbor  = arbor;
	bop->bo_rec    = *rec;
	bop->bo_cb     = *cb;
	bop->bo_tx     = tx;
	bop->bo_flags  = 0;
	bop->bo_seg    = arbor->t_desc->t_seg;
	bop->bo_i      = NULL;

	m0_sm_op_init(&bop->bo_op, &btree_put_kv_tick, &bop->bo_op_exec,
		      &btree_conf, &bop->bo_sm_group);
}

M0_INTERNAL void m0_btree_del(struct m0_btree *arbor,
			      const struct m0_btree_key *key,
			      const struct m0_btree_cb *cb,
			      struct m0_btree_op *bop, struct m0_be_tx *tx)
{
	bop->bo_opc        = M0_BO_DEL;
	bop->bo_arbor      = arbor;
	bop->bo_rec.r_key  = *key;
	if (cb == NULL)
		M0_SET0(&bop->bo_cb);
	else
		bop->bo_cb = *cb;
	bop->bo_tx         = tx;
	bop->bo_flags      = 0;
	bop->bo_seg        = arbor->t_desc->t_seg;
	bop->bo_i          = NULL;

	m0_sm_op_init(&bop->bo_op, &btree_del_kv_tick, &bop->bo_op_exec,
		      &btree_conf, &bop->bo_sm_group);
}

M0_INTERNAL void m0_btree_minkey(struct m0_btree *arbor,
				 const struct m0_btree_cb *cb, uint64_t flags,
				 struct m0_btree_op *bop)
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

M0_INTERNAL void m0_btree_maxkey(struct m0_btree *arbor,
				 const struct m0_btree_cb *cb, uint64_t flags,
				 struct m0_btree_op *bop)
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

M0_INTERNAL void m0_btree_update(struct m0_btree *arbor,
				 const struct m0_btree_rec *rec,
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

M0_INTERNAL void m0_btree_truncate(struct m0_btree *arbor, m0_bcount_t limit,
				   struct m0_be_tx *tx, struct m0_btree_op *bop)
{
	bop->bo_opc    = M0_BO_TRUNCATE;
	bop->bo_arbor  = arbor;
	bop->bo_limit  = limit;
	bop->bo_tx     = tx;
	bop->bo_flags  = 0;
	bop->bo_seg    = arbor->t_desc->t_seg;
	bop->bo_i      = NULL;

	m0_sm_op_init(&bop->bo_op, &btree_truncate_tick, &bop->bo_op_exec,
		      &btree_conf, &bop->bo_sm_group);
}
struct cursor_cb_data {
	struct m0_btree_rec ccd_rec;
	m0_bcount_t         ccd_keysz;
	m0_bcount_t         ccd_valsz;
};

static int btree_cursor_kv_get_cb(struct m0_btree_cb  *cb,
				  struct m0_btree_rec *rec)
{
	struct m0_btree_cursor  *datum = cb->c_datum;
	struct m0_bufvec_cursor  cur;

	/** Currently we support bufvec with only one segment. */
	M0_ASSERT(M0_BUFVEC_SEG_COUNT(&rec->r_key.k_data) == 1 &&
		  M0_BUFVEC_SEG_COUNT(&rec->r_val) == 1);

	/** Save returned Key and Value in the iterator */
	m0_bufvec_cursor_init(&cur, &rec->r_key.k_data);
	datum->bc_key = M0_BUF_INIT(m0_bufvec_cursor_step(&cur),
				    m0_bufvec_cursor_addr(&cur));

	m0_bufvec_cursor_init(&cur, &rec->r_val);
	datum->bc_val = M0_BUF_INIT(m0_bufvec_cursor_step(&cur),
				    m0_bufvec_cursor_addr(&cur));

	return 0;
}

M0_INTERNAL void m0_btree_cursor_init(struct m0_btree_cursor *it,
				      struct m0_btree        *arbor)
{
	M0_SET0(it);
	it->bc_arbor = arbor;
}

M0_INTERNAL void m0_btree_cursor_fini(struct m0_btree_cursor *it)
{
}

M0_INTERNAL int m0_btree_cursor_get(struct m0_btree_cursor     *it,
				    const struct m0_btree_key *key,
				    bool                       slant)
{
	struct m0_btree_op    kv_op = {};
	struct m0_btree_cb    cursor_cb;
	uint64_t              flags = slant ? BOF_SLANT : BOF_EQUAL;
	int                   rc;

	cursor_cb.c_act   = btree_cursor_kv_get_cb;
	cursor_cb.c_datum = it;
	rc = M0_BTREE_OP_SYNC_WITH_RC(&kv_op,
				      m0_btree_get(it->bc_arbor, key,
						   &cursor_cb, flags, &kv_op));
	return rc;
}

static int btree_cursor_iter(struct m0_btree_cursor *it,
			     enum m0_btree_op_flags  dir)
{
	struct m0_btree_op    kv_op = {};
	struct m0_btree_cb    cursor_cb;
	struct m0_btree_key   key;
	int                   rc;

	M0_ASSERT(M0_IN(dir, (BOF_NEXT, BOF_PREV)));

	cursor_cb.c_act   = btree_cursor_kv_get_cb;
	cursor_cb.c_datum = it;

	key.k_data = M0_BUFVEC_INIT_BUF(&it->bc_key.b_addr, &it->bc_key.b_nob);
	rc = M0_BTREE_OP_SYNC_WITH_RC(&kv_op,
				      m0_btree_iter(it->bc_arbor,
						    &key,
						    &cursor_cb, dir, &kv_op));
	return rc;
}

M0_INTERNAL int m0_btree_cursor_next(struct m0_btree_cursor *it)
{
	return btree_cursor_iter(it, BOF_NEXT);
}

M0_INTERNAL int m0_btree_cursor_prev(struct m0_btree_cursor *it)
{
	return btree_cursor_iter(it, BOF_PREV);
}

M0_INTERNAL int m0_btree_cursor_first(struct m0_btree_cursor *it)
{
	struct m0_btree_op    kv_op = {};
	struct m0_btree_cb    cursor_cb;
	int                   rc;

	cursor_cb.c_act   = btree_cursor_kv_get_cb;
	cursor_cb.c_datum = it;
	rc = M0_BTREE_OP_SYNC_WITH_RC(&kv_op,
				      m0_btree_minkey(it->bc_arbor, &cursor_cb,
						      0, &kv_op));
	return rc;
}

M0_INTERNAL int m0_btree_cursor_last(struct m0_btree_cursor *it)
{
	struct m0_btree_op    kv_op = {};
	struct m0_btree_cb    cursor_cb;
	int                   rc;

	cursor_cb.c_act   = btree_cursor_kv_get_cb;
	cursor_cb.c_datum = it;
	rc = M0_BTREE_OP_SYNC_WITH_RC(&kv_op,
				      m0_btree_maxkey(it->bc_arbor, &cursor_cb,
						      0, &kv_op));
	return rc;
}

M0_INTERNAL void m0_btree_cursor_put(struct m0_btree_cursor *it)
{
	/** Do nothing. */
}

M0_INTERNAL void m0_btree_cursor_kv_get(struct m0_btree_cursor *it,
					struct m0_buf *key, struct m0_buf *val)
{
	if (key)
		*key = M0_BUF_INIT(it->bc_key.b_nob, it->bc_key.b_addr);
	if (val)
		*val = M0_BUF_INIT(it->bc_val.b_nob, it->bc_val.b_addr);
}

M0_INTERNAL bool m0_btree_is_empty(struct m0_btree *btree)
{
	M0_PRE(btree != NULL);
	M0_PRE(btree->t_desc->t_root != NULL);
	return (bnode_count_rec(btree->t_desc->t_root) == 0);
}

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
		btree_ut_initialised = true;
	}
}

static void btree_ut_fini(void)
{
	btree_ut_initialised = false;
}

/**
 * In this unit test we exercise a few tree operations in invalid conditions
 * only.
 */
static void ut_basic_tree_oper_icp(void)
{
	void                   *invalid_addr = (void *)0xbadbadbadbad;
	struct m0_btree         btree;
	struct m0_btree_type    btree_type = {  .tt_id = M0_BT_UT_KV_OPS,
						.ksize = 8,
						.vsize = 8, };
	struct m0_be_tx         tx_data    = {};
	struct m0_be_tx        *tx         = &tx_data;
	struct m0_btree_op      b_op       = {};
	struct m0_be_tx_credit  cred       = {};
	void                   *temp_node;
	int                     rc;
	struct m0_fid           fid        = M0_FID_TINIT('b', 0, 1);
	uint32_t                rnode_sz   = m0_pagesize_get();
	uint32_t                rnode_sz_shift;
	struct m0_buf           buf;

	btree_ut_init();

	M0_ASSERT(rnode_sz != 0 && m0_is_po2(rnode_sz));
	rnode_sz_shift = __builtin_ffsl(rnode_sz) - 1;
	cred = M0_BE_TX_CB_CREDIT(0, 0, 0);
	m0_be_allocator_credit(NULL, M0_BAO_ALLOC_ALIGNED, rnode_sz,
			       rnode_sz_shift, &cred);
	m0_btree_create_credit(&btree_type, &cred, 1);

	/** Prepare transaction to capture tree operations. */
	m0_be_ut_tx_init(tx, ut_be);
	m0_be_tx_prep(tx, &cred);
	rc = m0_be_tx_open_sync(tx);
	M0_ASSERT(rc == 0);

	/**
	 *  Run a invalid scenario which:
	 *  1) Attempts to create a btree with invalid address
	 *
	 * This scenario is invalid because the root node address is incorrect.
	 * In this case m0_btree_create() will return -EFAULT.
	 */
	rc = M0_BTREE_OP_SYNC_WITH_RC(&b_op, m0_btree_create(invalid_addr,
				      rnode_sz, &btree_type, M0_BCT_NO_CRC,
				      &b_op, &btree, seg, &fid, tx, NULL));
	M0_ASSERT(rc == -EFAULT);
	m0_be_tx_close_sync(tx);
	m0_be_tx_fini(tx);

	/**
	 *  Run a invalid scenario which:
	 *  1) Attempts to open a btree with invalid address
	 *
	 * This scenario is invalid because the root node address is incorrect.
	 * In this case m0_btree_open() will return -EFAULT.
	 */
	rc = M0_BTREE_OP_SYNC_WITH_RC(&b_op, m0_btree_open(invalid_addr,
							   rnode_sz, &btree,
							   seg, &b_op, NULL));
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

	m0_be_ut_tx_init(tx, ut_be);
	m0_be_tx_prep(tx, &cred);
	rc = m0_be_tx_open_sync(tx);
	M0_ASSERT(rc == 0);
	/** Create temp node space*/
	buf = M0_BUF_INIT(rnode_sz, NULL);
	M0_BE_ALLOC_ALIGN_BUF_SYNC(&buf, rnode_sz_shift, seg, tx);
	temp_node = buf.b_addr;
	rc = M0_BTREE_OP_SYNC_WITH_RC(&b_op, m0_btree_create(temp_node,
				      rnode_sz, &btree_type, M0_BCT_NO_CRC,
				      &b_op, &btree, seg, &fid, tx, NULL));
	M0_ASSERT(rc == 0);
	m0_be_tx_close_sync(tx);
	m0_be_tx_fini(tx);

	rc = M0_BTREE_OP_SYNC_WITH_RC(&b_op, m0_btree_open(temp_node, rnode_sz,
							   &btree, seg, &b_op,
							   NULL));
	M0_ASSERT(rc == 0);

	cred = M0_BE_TX_CREDIT(0, 0);
	m0_be_allocator_credit(NULL, M0_BAO_FREE_ALIGNED, rnode_sz,
			       rnode_sz_shift, &cred);
	m0_btree_destroy_credit(b_op.bo_arbor, NULL, &cred, 1);

	m0_be_ut_tx_init(tx, ut_be);
	m0_be_tx_prep(tx, &cred);
	rc = m0_be_tx_open_sync(tx);
	M0_ASSERT(rc == 0);
	rc = M0_BTREE_OP_SYNC_WITH_RC(&b_op, m0_btree_destroy(b_op.bo_arbor,
							      &b_op, tx));
	M0_ASSERT(rc == -EPERM);
	M0_SET0(&btree);
	buf = M0_BUF_INIT(rnode_sz, temp_node);
	M0_BE_FREE_ALIGN_BUF_SYNC(&buf, rnode_sz_shift, seg, tx);
	m0_be_tx_close_sync(tx);
	m0_be_tx_fini(tx);

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
	cred = M0_BE_TX_CREDIT(0, 0);
	m0_be_allocator_credit(NULL, M0_BAO_ALLOC_ALIGNED, rnode_sz,
			       rnode_sz_shift, &cred);
	m0_btree_create_credit(&btree_type, &cred, 1);

	/** Prepare transaction to capture tree operations. */
	m0_be_ut_tx_init(tx, ut_be);
	m0_be_tx_prep(tx, &cred);
	rc = m0_be_tx_open_sync(tx);
	M0_ASSERT(rc == 0);

	buf = M0_BUF_INIT(rnode_sz, NULL);
	M0_BE_ALLOC_ALIGN_BUF_SYNC(&buf, rnode_sz_shift, seg, tx);
	temp_node = buf.b_addr;
	rc = M0_BTREE_OP_SYNC_WITH_RC(&b_op, m0_btree_create(temp_node, rnode_sz,
				      &btree_type, M0_BCT_NO_CRC, &b_op,
				      &btree, seg, &fid, tx, NULL));
	M0_ASSERT(rc == 0);
	m0_be_tx_close_sync(tx);
	m0_be_tx_fini(tx);

	rc = M0_BTREE_OP_SYNC_WITH_RC(&b_op, m0_btree_close(b_op.bo_arbor,
							    &b_op));
	M0_ASSERT(rc == 0);
	M0_SET0(&btree);

	rc = M0_BTREE_OP_SYNC_WITH_RC(&b_op, m0_btree_close(b_op.bo_arbor,
							    &b_op));
	M0_ASSERT(rc == -EINVAL);
	M0_SET0(&btree);

	rc = M0_BTREE_OP_SYNC_WITH_RC(&b_op, m0_btree_open(temp_node, rnode_sz,
							   &btree, seg, &b_op,
							   NULL));
	M0_ASSERT(rc == 0);

	cred = M0_BE_TX_CREDIT(0, 0);
	m0_be_allocator_credit(NULL, M0_BAO_FREE_ALIGNED, rnode_sz,
			       rnode_sz_shift, &cred);
	m0_btree_destroy_credit(b_op.bo_arbor, NULL, &cred, 1);

	m0_be_ut_tx_init(tx, ut_be);
	m0_be_tx_prep(tx, &cred);
	rc = m0_be_tx_open_sync(tx);
	M0_ASSERT(rc == 0);
	rc = M0_BTREE_OP_SYNC_WITH_RC(&b_op, m0_btree_destroy(b_op.bo_arbor,
							      &b_op, tx));
	M0_ASSERT(rc == 0);
	M0_SET0(&btree);
	buf = M0_BUF_INIT(rnode_sz, temp_node);
	M0_BE_FREE_ALIGN_BUF_SYNC(&buf, rnode_sz_shift, seg, tx);
	m0_be_tx_close_sync(tx);
	m0_be_tx_fini(tx);

	btree_ut_fini();
}

struct ut_cb_data {
	/** Key that needs to be stored or retrieved. */
	struct m0_btree_key      *key;

	/** Value associated with the key that is to be stored or retrieved. */
	struct m0_bufvec         *value;

	/** If value is retrieved (GET) then check if has expected contents. */
	bool                      check_value;

	/** Indicates if CRC is present in the last word of the value. */
	enum m0_btree_crc_type    crc;

	bool                      embedded_ksize;

	bool                      embedded_vsize;

	/**
	 *  This field is filled by the callback routine with the flags which
	 *  the CB routine received from the _tick(). This flag can then be
	 *  analyzed by the caller for further processing.
	 */
	uint32_t                  flags;
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
static int ut_btree_kv_put_cb(struct m0_btree_cb *cb, struct m0_btree_rec *rec)
{
	struct m0_bufvec_cursor  scur;
	struct m0_bufvec_cursor  dcur;
	m0_bcount_t              ksize;
	m0_bcount_t              vsize;
	struct ut_cb_data       *datum = cb->c_datum;

	/** The caller can look at these flags if he needs to. */
	datum->flags = rec->r_flags;
	M0_ASSERT(datum->flags == M0_BSC_SUCCESS);

	ksize = m0_vec_count(&datum->key->k_data.ov_vec);
	M0_ASSERT(m0_vec_count(&rec->r_key.k_data.ov_vec) >= ksize);

	vsize = m0_vec_count(&datum->value->ov_vec);
	M0_ASSERT(m0_vec_count(&rec->r_val.ov_vec) >= vsize);

	m0_bufvec_cursor_init(&scur, &datum->key->k_data);
	m0_bufvec_cursor_init(&dcur, &rec->r_key.k_data);
	rec->r_key.k_data.ov_vec.v_count[0] =
	m0_vec_count(&datum->key->k_data.ov_vec);
	m0_bufvec_cursor_copy(&dcur, &scur, ksize);

	m0_bufvec_cursor_init(&scur, datum->value);
	m0_bufvec_cursor_init(&dcur, &rec->r_val);
	rec->r_val.ov_vec.v_count[0]=
	m0_vec_count(&datum->value->ov_vec);
	m0_bufvec_cursor_copy(&dcur, &scur, vsize);

	return 0;
}

static int ut_btree_kv_get_cb(struct m0_btree_cb *cb, struct m0_btree_rec *rec)
{
	struct m0_bufvec_cursor  scur;
	struct m0_bufvec_cursor  dcur;
	m0_bcount_t              ksize;
	m0_bcount_t              vsize;
	struct ut_cb_data       *datum = cb->c_datum;
	int                      i      = 0;

	/** The caller can look at these flags if he needs to. */
	datum->flags = rec->r_flags;

	M0_ASSERT(rec->r_flags == M0_BSC_SUCCESS);

	if (datum->embedded_ksize) {
		/** Extract Key size present in the 2nd word.*/
		m0_bufvec_cursor_init(&scur, &rec->r_key.k_data);
		m0_bufvec_cursor_move(&scur, sizeof(uint64_t));
		m0_bufvec_cursor_copyfrom(&scur, &ksize, sizeof(ksize));
		ksize = m0_byteorder_cpu_to_be64(ksize);
		M0_PRE(ksize <= MAX_KEY_SIZE &&
		       m0_vec_count(&rec->r_key.k_data.ov_vec) == ksize);
	} else {
		ksize = m0_vec_count(&rec->r_key.k_data.ov_vec);
		M0_PRE(m0_vec_count(&datum->key->k_data.ov_vec) >= ksize);
	}

	if (datum->embedded_vsize) {
		/** Extract Value size present in the 2nd word.*/
		m0_bufvec_cursor_init(&scur, &rec->r_val);
		m0_bufvec_cursor_move(&scur, sizeof(uint64_t));
		m0_bufvec_cursor_copyfrom(&scur, &vsize, sizeof(vsize));
		vsize = m0_byteorder_cpu_to_be64(vsize);
		M0_PRE(vsize <= MAX_VAL_SIZE &&
		       m0_vec_count(&rec->r_val.ov_vec) == vsize);
	} else {
		vsize = m0_vec_count(&rec->r_val.ov_vec);
		M0_PRE(m0_vec_count(&datum->value->ov_vec) >= vsize);
	}

	/** Copy Key and Value from the btree node to the caller's data space */
	m0_bufvec_cursor_init(&dcur, &datum->key->k_data);
	m0_bufvec_cursor_init(&scur, &rec->r_key.k_data);
	m0_bufvec_cursor_copy(&dcur, &scur, ksize);

	m0_bufvec_cursor_init(&dcur, datum->value);
	m0_bufvec_cursor_init(&scur, &rec->r_val);
	m0_bufvec_cursor_copy(&dcur, &scur, vsize);

	if (datum->check_value) {
		struct m0_bufvec_cursor kcur;
		struct m0_bufvec_cursor vcur;
		m0_bcount_t             v_off;
		bool                    check_failed = false;
		uint64_t                key;
		uint64_t                value;
		m0_bcount_t             valuelen     = vsize;

		v_off = 0;

		/**
		 *  Key present in the 1st word is repeated in the Value array
		 *  except in word #2 which contains the size of the Value. Use
		 *  this Key to make sure all the words in the Value array are
		 *  correct.
		 */
		m0_bufvec_cursor_init(&kcur, &rec->r_key.k_data);
		m0_bufvec_cursor_copyfrom(&kcur, &key, sizeof(key));
		m0_bufvec_cursor_init(&vcur, &rec->r_val);

		if (datum->crc != M0_BCT_NO_CRC &&
		    datum->crc != M0_BCT_BTREE_ENC_RAW_HASH)
			valuelen -= sizeof(uint64_t);

		while (v_off < valuelen) {
			m0_bufvec_cursor_copyfrom(&vcur, &value, sizeof(value));
			v_off += sizeof(value);

			if (i++ == 1)
				/** Skip the element containing value size.*/
				continue;

			if (key != value)
				check_failed = true;
		}

		/**
		 * If check_failed then maybe this entry was updated in which
		 * case we use the complement of the key for comparison.
		 */
		if (check_failed) {
			v_off = 0;
			m0_bufvec_cursor_init(&vcur, &rec->r_val);
			key = ~key;
			i = 0;

			while (v_off < valuelen) {
				m0_bufvec_cursor_copyfrom(&vcur, &value,
							  sizeof(value));
				v_off += sizeof(value);

				if (i++ == 1)
					continue;

					if (key != value)
						M0_ASSERT(0);
			}
		}
	}

	/** Verify the CRC if present. */
	if (datum->crc != M0_BCT_NO_CRC) {
		uint64_t value_ary[vsize / sizeof(uint64_t)];
		uint64_t csum_in_value;
		uint64_t calculated_csum;

		m0_bufvec_cursor_init(&scur, &rec->r_val);
		m0_bufvec_cursor_copyfrom(&scur, value_ary, sizeof(value_ary));

		if (datum->crc == M0_BCT_USER_ENC_RAW_HASH) {
			vsize -= sizeof(uint64_t);
			m0_bufvec_cursor_init(&scur, &rec->r_val);
			m0_bufvec_cursor_move(&scur, vsize);
			m0_bufvec_cursor_copyfrom(&scur, &csum_in_value,
						  sizeof(csum_in_value));

			calculated_csum = m0_hash_fnc_fnv1(value_ary, vsize);
			M0_ASSERT(csum_in_value == calculated_csum);
		} else if (datum->crc == M0_BCT_BTREE_ENC_RAW_HASH) {
			void *p_crc = rec->r_val.ov_buf[0] + vsize;
			calculated_csum = m0_hash_fnc_fnv1(value_ary, vsize);
			M0_ASSERT(*(uint64_t*)(p_crc) == calculated_csum);
		} else if (datum->crc == M0_BCT_USER_ENC_FORMAT_FOOTER)
			M0_ASSERT(m0_format_footer_verify(value_ary, false) == 0);
	}

	return 0;
}

/**
 * Callback will be called before deleting value. If needed, caller can copy the
 * content of record.
 */
static int ut_btree_kv_del_cb(struct m0_btree_cb *cb, struct m0_btree_rec *rec)
{
	m0_bcount_t              ksize;
	struct ut_cb_data       *datum = cb->c_datum;

	/** The caller can look at the record if he needs to. */
	ksize = m0_vec_count(&datum->key->k_data.ov_vec);
	M0_PRE(ksize <= MAX_KEY_SIZE + sizeof(uint64_t) &&
	       m0_vec_count(&rec->r_key.k_data.ov_vec) == ksize);
	M0_PRE(m0_vec_count(&rec->r_val.ov_vec) <=
	       MAX_VAL_SIZE + sizeof(uint64_t));

	datum->flags = rec->r_flags;
	M0_ASSERT(datum->flags == M0_BSC_SUCCESS);

	return 0;
}

static int ut_btree_kv_update_cb(struct m0_btree_cb *cb,
				 struct m0_btree_rec *rec)
{
	struct m0_bufvec_cursor  scur;
	struct m0_bufvec_cursor  dcur;
	m0_bcount_t              vsize;
	struct ut_cb_data       *datum = cb->c_datum;

	/** The caller can look at these flags if he needs to. */
	datum->flags = rec->r_flags;
	M0_ASSERT(datum->flags == M0_BSC_SUCCESS);

	vsize = m0_vec_count(&datum->value->ov_vec);
	M0_ASSERT(m0_vec_count(&rec->r_val.ov_vec) == vsize);

	m0_bufvec_cursor_init(&scur, datum->value);
	m0_bufvec_cursor_init(&dcur, &rec->r_val);
	rec->r_val.ov_vec.v_count[0]=
	m0_vec_count(&datum->value->ov_vec);
	m0_bufvec_cursor_copy(&dcur, &scur, vsize);

	return 0;
}

#if (AVOID_BE_SEGMENT == 1)
enum {
	MIN_STREAM_CNT         = 5,
	MAX_STREAM_CNT         = 20,

	MIN_RECS_PER_STREAM    = 5,
	MAX_RECS_PER_STREAM    = 2048,

	MAX_RECS_PER_THREAD    = 10000, /** Records count for each thread */

	MIN_TREE_LOOPS         = 1000,
	MAX_TREE_LOOPS         = 2000,
	MAX_RECS_FOR_TREE_TEST = 100,

	RANDOM_TREE_COUNT      = -1,
	RANDOM_THREAD_COUNT    = -1,

	/** Key/Value array size used in UTs. Increment for CRC in Value. */
	KEY_ARR_ELE_COUNT      = MAX_KEY_SIZE / sizeof(uint64_t),
	VAL_ARR_ELE_COUNT      = MAX_VAL_SIZE / sizeof(uint64_t),
	RANDOM_KEY_SIZE        = -1,
	RANDOM_VALUE_SIZE      = -1,

	BE_UT_SEG_SIZE         = 0,
};
#else
enum {
	MIN_STREAM_CNT         = 5,
	MAX_STREAM_CNT         = 10,

	MIN_RECS_PER_STREAM    = 5,
	MAX_RECS_PER_STREAM    = 200,

	MAX_RECS_PER_THREAD    = 1000, /** Records count for each thread */

	MIN_TREE_LOOPS         = 50,
	MAX_TREE_LOOPS         = 100,
	MAX_RECS_FOR_TREE_TEST = 50,

	RANDOM_TREE_COUNT      = -1,
	RANDOM_THREAD_COUNT    = -1,

	/** Key/Value array size used in UTs. Increment for CRC in Value. */
	KEY_ARR_ELE_COUNT      = MAX_KEY_SIZE / sizeof(uint64_t),
	VAL_ARR_ELE_COUNT      = MAX_VAL_SIZE / sizeof(uint64_t),
	RANDOM_KEY_SIZE        = -1,
	RANDOM_VALUE_SIZE      = -1,

	BE_UT_SEG_SIZE         = 10ULL * 1024ULL * 1024ULL * 1024ULL,
};
#endif

/**
 * This unit test exercises the KV operations triggered by multiple streams.
 */
static void ut_multi_stream_kv_oper(void)
{
	void                   *rnode;
	int                     i;
	time_t                  curr_time;
	struct m0_btree_cb      ut_cb;
	struct m0_be_tx         tx_data         = {};
	struct m0_be_tx        *tx              = &tx_data;
	struct m0_be_tx_credit  cred            = {};
	struct m0_btree_op      b_op            = {};
	uint32_t                stream_count    = 0;
	uint64_t                recs_per_stream = 0;
	struct m0_btree_op      kv_op           = {};
	struct m0_btree        *tree;
	struct m0_btree         btree = {};
	struct m0_btree_type    btree_type      = {.tt_id = M0_BT_UT_KV_OPS,
						  .ksize = sizeof(uint64_t),
						  .vsize = btree_type.ksize*2,
						  };
	uint64_t                key;
	uint64_t                value[btree_type.vsize / sizeof(uint64_t)];
	m0_bcount_t             ksize           = sizeof key;
	m0_bcount_t             vsize           = sizeof value;
	void                   *k_ptr           = &key;
	void                   *v_ptr           = &value;
	int                     rc;
	struct m0_buf           buf;
	uint64_t                maxkey          = 0;
	uint64_t                minkey          = UINT64_MAX;
	uint32_t                rnode_sz        = m0_pagesize_get();
	uint32_t                rnode_sz_shift;
	struct m0_fid           fid             = M0_FID_TINIT('b', 0, 1);

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

	M0_ASSERT(rnode_sz != 0 && m0_is_po2(rnode_sz));
	rnode_sz_shift = __builtin_ffsl(rnode_sz) - 1;
	cred = M0_BE_TX_CB_CREDIT(0, 0, 0);
	m0_be_allocator_credit(NULL, M0_BAO_ALLOC_ALIGNED, rnode_sz,
			       rnode_sz_shift, &cred);
	m0_btree_create_credit(&btree_type, &cred, 1);

	/** Prepare transaction to capture tree operations. */
	m0_be_ut_tx_init(tx, ut_be);
	m0_be_tx_prep(tx, &cred);
	rc = m0_be_tx_open_sync(tx);
	M0_ASSERT(rc == 0);

	/** Create temp node space and use it as root node for btree */
	buf = M0_BUF_INIT(rnode_sz, NULL);
	M0_BE_ALLOC_ALIGN_BUF_SYNC(&buf, rnode_sz_shift, seg, tx);
	rnode = buf.b_addr;

	rc = M0_BTREE_OP_SYNC_WITH_RC(&b_op, m0_btree_create(rnode, rnode_sz,
							     &btree_type,
							     M0_BCT_NO_CRC,
							     &b_op, &btree, seg,
							     &fid, tx, NULL));
	M0_ASSERT(rc == M0_BSC_SUCCESS);
	m0_be_tx_close_sync(tx);
	m0_be_tx_fini(tx);

	tree = b_op.bo_arbor;

	cred = M0_BE_TX_CB_CREDIT(0, 0, 0);
	m0_btree_put_credit(tree, 1, ksize, vsize, &cred);
	{
		struct m0_be_tx_credit  cred2 = {};

		m0_btree_put_credit2(&btree_type, rnode_sz, 1, ksize, vsize,
				     &cred2);
		M0_ASSERT(m0_be_tx_credit_eq(&cred,  &cred2));
	}

	for (i = 1; i <= recs_per_stream; i++) {
		struct ut_cb_data    put_data;
		struct m0_btree_rec  rec;
		uint32_t             stream_num;

		rec.r_key.k_data   = M0_BUFVEC_INIT_BUF(&k_ptr, &ksize);
		rec.r_val          = M0_BUFVEC_INIT_BUF(&v_ptr, &vsize);
		rec.r_crc_type     = M0_BCT_NO_CRC;

		for (stream_num = 0; stream_num < stream_count; stream_num++) {
			int k;

			key = i + (stream_num * recs_per_stream);
			if (key < minkey)
				minkey = key;
			if (key > maxkey)
				maxkey = key;

			key = m0_byteorder_cpu_to_be64(key);
			for (k = 0; k < ARRAY_SIZE(value); k++) {
				value[k] = key;
			}

			put_data.key       = &rec.r_key;
			put_data.value     = &rec.r_val;

			ut_cb.c_act        = ut_btree_kv_put_cb;
			ut_cb.c_datum      = &put_data;

			m0_be_ut_tx_init(tx, ut_be);
			m0_be_tx_prep(tx, &cred);
			rc = m0_be_tx_open_sync(tx);
			M0_ASSERT(rc == 0);

			rc = M0_BTREE_OP_SYNC_WITH_RC(&kv_op,
						      m0_btree_put(tree, &rec,
								   &ut_cb,
								   &kv_op, tx));
			M0_ASSERT(rc == 0 && put_data.flags == M0_BSC_SUCCESS);
			m0_be_tx_close_sync(tx);
			m0_be_tx_fini(tx);
		}
	}

	{
		/** Min/Max key verification test. */
		struct ut_cb_data    get_data;
		struct m0_btree_key  get_key;
		struct m0_bufvec     get_value;

		get_key.k_data          = M0_BUFVEC_INIT_BUF(&k_ptr, &ksize);
		get_value               = M0_BUFVEC_INIT_BUF(&v_ptr, &vsize);

		get_data.key            = &get_key;
		get_data.value          = &get_value;
		get_data.check_value    = true;
		get_data.crc            = M0_BCT_NO_CRC;
		get_data.embedded_ksize = false;
		get_data.embedded_vsize = false;

		ut_cb.c_act             = ut_btree_kv_get_cb;
		ut_cb.c_datum           = &get_data;

		rc = M0_BTREE_OP_SYNC_WITH_RC(&kv_op,
					      m0_btree_minkey(tree, &ut_cb, 0,
							      &kv_op));
		M0_ASSERT(rc == M0_BSC_SUCCESS &&
			  minkey == m0_byteorder_be64_to_cpu(key));

		rc = M0_BTREE_OP_SYNC_WITH_RC(&kv_op,
					      m0_btree_maxkey(tree, &ut_cb, 0,
							      &kv_op));
		M0_ASSERT(rc == M0_BSC_SUCCESS &&
			  maxkey == m0_byteorder_be64_to_cpu(key));
	}

	for (i = 1; i <= (recs_per_stream*stream_count); i++) {
		struct ut_cb_data    get_data;
		struct m0_btree_key  get_key;
		struct m0_bufvec     get_value;
		uint64_t             find_key;
		void                *find_key_ptr      = &find_key;
		m0_bcount_t          find_key_size     = sizeof find_key;
		struct m0_btree_key  find_key_in_tree;

		find_key = m0_byteorder_cpu_to_be64(i);
		find_key_in_tree.k_data =
			M0_BUFVEC_INIT_BUF(&find_key_ptr, &find_key_size);

		get_key.k_data = M0_BUFVEC_INIT_BUF(&k_ptr, &ksize);
		get_value      = M0_BUFVEC_INIT_BUF(&v_ptr, &vsize);

		get_data.key            = &get_key;
		get_data.value          = &get_value;
		get_data.check_value    = true;
		get_data.crc            = M0_BCT_NO_CRC;
		get_data.embedded_ksize = false;
		get_data.embedded_vsize = false;

		ut_cb.c_act    = ut_btree_kv_get_cb;
		ut_cb.c_datum  = &get_data;

		rc = M0_BTREE_OP_SYNC_WITH_RC(&kv_op,
					      m0_btree_get(tree,
							   &find_key_in_tree,
							   &ut_cb, BOF_EQUAL,
							   &kv_op));
		M0_ASSERT(rc == M0_BSC_SUCCESS &&
			  i == m0_byteorder_be64_to_cpu(key));
	}

	{
		/** UT for Cursor verification. */
		struct m0_btree_cursor *cursor;
		struct m0_buf           cur_key;
		struct m0_buf           cur_val;
		uint64_t                find_key;
		void                   *find_key_ptr  = &find_key;
		m0_bcount_t             find_key_size = sizeof find_key;
		struct m0_btree_key     find_key_in_tree;

		M0_ALLOC_PTR(cursor);
		M0_ASSERT(cursor != NULL);

		m0_btree_cursor_init(cursor, tree);
		/** Get and verify the first key. */
		find_key = m0_byteorder_cpu_to_be64(1);
		find_key_in_tree.k_data =
			M0_BUFVEC_INIT_BUF(&find_key_ptr, &find_key_size);
		rc = m0_btree_cursor_get(cursor, &find_key_in_tree, false);
		M0_ASSERT(rc == 0);

		m0_btree_cursor_kv_get(cursor, &cur_key, &cur_val);
		key = *(uint64_t*)cur_key.b_addr;
		M0_ASSERT(m0_byteorder_be64_to_cpu(key) == 1);

		/** Verify the next key till the last key of btree.*/
		for (i = 1; i < (recs_per_stream*stream_count); i++) {
			rc = m0_btree_cursor_next(cursor);
			M0_ASSERT(rc == 0);
			m0_btree_cursor_kv_get(cursor, &cur_key, &cur_val);
			key = *(uint64_t*)cur_key.b_addr;
			M0_ASSERT(m0_byteorder_be64_to_cpu(key) - 1 == i);
		}
		/** There should not be any key after last key.*/
		rc = m0_btree_cursor_next(cursor);
		M0_ASSERT(rc == -ENOENT);

		/**
		 * Cursor is at the last key. Verify the previous key till the
		 * first key of btree.
		 */
		for (i = (recs_per_stream*stream_count); i > 1; i--) {
			rc = m0_btree_cursor_prev(cursor);
			M0_ASSERT(rc == 0);
			m0_btree_cursor_kv_get(cursor, &cur_key, &cur_val);
			key = *(uint64_t*)cur_key.b_addr;
			M0_ASSERT(m0_byteorder_be64_to_cpu(key) + 1 == i);
		}
		/** There should not be any key before first key.*/
		rc = m0_btree_cursor_prev(cursor);
		M0_ASSERT(rc == -ENOENT);

		/** The last key should be equal to max key. */
		rc = m0_btree_cursor_last(cursor);
		M0_ASSERT(rc == 0);
		m0_btree_cursor_kv_get(cursor, &cur_key, &cur_val);
		key = *(uint64_t*)cur_key.b_addr;
		M0_ASSERT(m0_byteorder_be64_to_cpu(key) ==
			  (recs_per_stream*stream_count));

		/** The first key should be equal to min key. */
		rc = m0_btree_cursor_first(cursor);
		M0_ASSERT(rc == 0);
		m0_btree_cursor_kv_get(cursor, &cur_key, &cur_val);
		key = *(uint64_t*)cur_key.b_addr;
		M0_ASSERT(m0_byteorder_be64_to_cpu(key) == 1);
		m0_btree_cursor_fini(cursor);
		m0_free(cursor);
	}

	cred = M0_BE_TX_CREDIT(0, 0);
	m0_btree_del_credit(tree, 1, ksize, -1, &cred);
	{
		struct m0_be_tx_credit  cred2 = {};

		m0_btree_del_credit2(&btree_type, rnode_sz, 1, ksize, vsize,
				     &cred2);
		M0_ASSERT(m0_be_tx_credit_eq(&cred,  &cred2));
	}

	for (i = 1; i <= recs_per_stream; i++) {
		uint64_t            del_key;
		struct m0_btree_key del_key_in_tree;
		void                *p_del_key      = &del_key;
		m0_bcount_t         del_key_size    = sizeof del_key;
		struct ut_cb_data   del_data;
		uint32_t            stream_num;

		del_data = (struct ut_cb_data){.key = &del_key_in_tree,
			.value       = NULL,
			.check_value = false,
			.crc         = M0_BCT_NO_CRC,
		};

		del_key_in_tree.k_data = M0_BUFVEC_INIT_BUF(&p_del_key,
							    &del_key_size);

		ut_cb.c_act   = ut_btree_kv_del_cb;
		ut_cb.c_datum = &del_data;

		for (stream_num = 0; stream_num < stream_count; stream_num++) {
			struct m0_btree_cb *del_cb;
			del_key = i + (stream_num * recs_per_stream);
			del_key = m0_byteorder_cpu_to_be64(del_key);

			del_cb = (del_key % 5 == 0) ? NULL : &ut_cb;
			m0_be_ut_tx_init(tx, ut_be);
			m0_be_tx_prep(tx, &cred);
			rc = m0_be_tx_open_sync(tx);
			M0_ASSERT(rc == 0);

			rc = M0_BTREE_OP_SYNC_WITH_RC(&kv_op,
						      m0_btree_del(tree,
							      &del_key_in_tree,
							      del_cb,
							      &kv_op, tx));
			M0_ASSERT(rc == 0 && del_data.flags == M0_BSC_SUCCESS);
			m0_be_tx_close_sync(tx);
			m0_be_tx_fini(tx);
		}
	}

	cred = M0_BE_TX_CREDIT(0, 0);
	m0_be_allocator_credit(NULL, M0_BAO_FREE_ALIGNED, rnode_sz,
			       rnode_sz_shift, &cred);
	m0_btree_destroy_credit(tree, NULL, &cred, 1);

	m0_be_ut_tx_init(tx, ut_be);
	m0_be_tx_prep(tx, &cred);
	rc = m0_be_tx_open_sync(tx);
	M0_ASSERT(rc == 0);

	rc = M0_BTREE_OP_SYNC_WITH_RC(&b_op, m0_btree_destroy(tree, &b_op, tx));
	M0_ASSERT(rc == 0);
	M0_SET0(&btree);

	/** Delete temp node space which was used as root node for the tree. */
	buf = M0_BUF_INIT(rnode_sz, rnode);
	M0_BE_FREE_ALIGN_BUF_SYNC(&buf, rnode_sz_shift, seg, tx);

	m0_be_tx_close_sync(tx);
	m0_be_tx_fini(tx);

	btree_ut_fini();
}

struct btree_ut_thread_info {
	struct m0_thread      ti_q;           /** Used for thread operations. */
	struct m0_bitmap      ti_cpu_map;     /** CPU map to run this thread. */
	uint64_t              ti_key_first;     /** First Key value to use. */
	uint64_t              ti_key_count;     /** Keys to use. */
	uint64_t              ti_key_incr;     /** Key value to increment by. */
	uint16_t              ti_thread_id;     /** Thread ID <= 65535. */
	struct m0_btree      *ti_tree;          /** Tree for KV operations */
	int                   ti_key_size;      /** Key size in bytes. */
	int                   ti_value_size;    /** Value size in bytes. */
	bool                  ti_random_bursts; /** Burstiness in IO pattern. */
	uint64_t              ti_rng_seed_base; /** Base used for RNG seed. */
	uint64_t              ti_crc_type;      /** CRC type of records. */

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
	int rc;

	M0_ALLOC_ARR(ti->ti_rnd_state_ptr, 64);
	if (ti->ti_rnd_state_ptr == NULL) {
		return -ENOMEM;
	}

	M0_SET0(&ti->ti_random_buf);
	initstate_r(ti->ti_rng_seed_base, ti->ti_rnd_state_ptr, 64,
		    &ti->ti_random_buf);

	srandom_r(ti->ti_thread_id + 1, &ti->ti_random_buf);

	rc = m0_thread_confine(&ti->ti_q, &ti->ti_cpu_map);

	m0_bitmap_fini(&ti->ti_cpu_map);
	return rc;
}

#define FILL_KEY(key, ksize, data)                                             \
	do {                                                                   \
		uint32_t i;                                                    \
		key[0] = data;                                                 \
		key[1] = m0_byteorder_cpu_to_be64(ksize);                      \
		for (i = 2; i < (ksize / sizeof(key[0])); i++)                 \
			key[i] = data;                                         \
	} while (0);

#define FILL_VALUE(value, vsize, data, crc)                                    \
	do {                                                                   \
		uint32_t i = 0;                                                \
		if (crc == M0_BCT_NO_CRC) {                                    \
			value[0] = data;                                       \
			value[1] = m0_byteorder_cpu_to_be64(vsize);            \
			for (i = 2; i < (vsize / sizeof(value[0])); i++)       \
				value[i] = data;                               \
		} else if (crc == M0_BCT_USER_ENC_RAW_HASH) {                  \
			value[0] = data;                                       \
			value[1] = m0_byteorder_cpu_to_be64(vsize);            \
			for (i = 2; i < ((vsize / sizeof(value[0]) - 1)); i++) \
				value[i] = data;                               \
			value[i] = m0_hash_fnc_fnv1(value,                     \
						    vsize - sizeof(value[0])); \
		} else if (crc == M0_BCT_USER_ENC_FORMAT_FOOTER) {             \
			struct m0_format_header *header;                       \
			struct m0_format_footer *footer;                       \
			struct m0_format_tag     tag = {                       \
				.ot_version = 1,                               \
				.ot_type = 2,                                  \
				.ot_size = (vsize -                            \
					    sizeof(struct m0_format_footer)),  \
			};                                                     \
                                                                               \
			M0_CASSERT(sizeof(*header) % sizeof(value[0]) == 0);   \
			M0_CASSERT(sizeof(*footer) % sizeof(value[0]) == 0);   \
                                                                               \
			header = (struct m0_format_header *)value;             \
			m0_format_header_pack(header, &tag);                   \
			for (i = (sizeof(*header)/sizeof(value[0]));           \
			     i < ARRAY_SIZE(value) - (sizeof(*footer) /        \
						      sizeof(value[0]));       \
			     i++) {                                            \
				value[i] = data;                               \
			}                                                      \
			m0_format_footer_update(value);                        \
		} else if (crc == M0_BCT_BTREE_ENC_RAW_HASH) {                 \
			value[0] = data;                                       \
			value[1] = m0_byteorder_cpu_to_be64(vsize);            \
			for (i = 2; i < (vsize / sizeof(value[0])); i++)       \
				value[i] = data;                               \
		}                                                              \
	} while (0);

#define GET_RANDOM_KEYSIZE(karray, kfirst, kiter_start, kincr)                 \
	({                                                                     \
		uint64_t random_size;                                          \
		random_size = (((kfirst - kiter_start) / kincr) %              \
			       (KEY_ARR_ELE_COUNT - 1)) + 2;                   \
		random_size *= sizeof(karray[0]);                              \
		random_size;                                                   \
	})

#define GET_RANDOM_VALSIZE(varray, kfirst, kiter_start, kincr, crc)            \
	({                                                                     \
		uint64_t random_size = 0;                                      \
		if (crc == M0_BCT_NO_CRC || crc == M0_BCT_BTREE_ENC_RAW_HASH ) \
			random_size = (((kfirst - kiter_start) / kincr) %      \
					(VAL_ARR_ELE_COUNT - 1)) + 2;          \
		else if (crc == M0_BCT_USER_ENC_RAW_HASH)                      \
			random_size = (((kfirst - kiter_start) / kincr) %      \
					(VAL_ARR_ELE_COUNT - 2)) + 3;          \
		else if (crc == M0_BCT_USER_ENC_FORMAT_FOOTER)                 \
			random_size = (((kfirst - kiter_start) / kincr) %      \
					(VAL_ARR_ELE_COUNT - 4)) + 5;          \
		random_size *= sizeof(varray[0]);                              \
		random_size;                                                   \
	})

/**
 * This routine is a thread handler which launches PUT, GET, ITER, SLANT and DEL
 * operations on the btree passed as parameter.
 */
static void btree_ut_kv_oper_thread_handler(struct btree_ut_thread_info *ti)
{
	uint64_t                key[KEY_ARR_ELE_COUNT];
	uint64_t                value[VAL_ARR_ELE_COUNT];
	void                   *k_ptr         = &key;
	void                   *v_ptr         = &value;
	m0_bcount_t             ksize         = ti->ti_key_size;
	m0_bcount_t             vsize         = ti->ti_value_size;
	struct m0_btree_rec     rec;
	struct m0_btree_cb      ut_cb;
	struct ut_cb_data       data;

	uint64_t                get_key[ARRAY_SIZE(key)];
	uint64_t                get_value[ARRAY_SIZE(value)];
	m0_bcount_t             get_ksize;
	m0_bcount_t             get_vsize;
	void                   *get_k_ptr     = &get_key;
	void                   *get_v_ptr     = &get_value;
	struct m0_btree_rec     get_rec;
	struct m0_btree_cb      ut_get_cb;
	struct ut_cb_data       get_data;

	uint64_t                key_iter_start;
	uint64_t                key_end;
	struct m0_btree_op      kv_op        = {};
	struct m0_btree        *tree;
	bool                    ksize_random =
					ti->ti_key_size == RANDOM_KEY_SIZE;
	bool                    vsize_random =
					ti->ti_value_size == RANDOM_VALUE_SIZE;
	struct m0_be_tx         tx_data;
	struct m0_be_tx        *tx           = &tx_data;
	struct m0_be_tx_credit  put_cred     = {};
	struct m0_be_tx_credit  update_cred  = {};
	struct m0_be_tx_credit  del_cred     = {};
	enum m0_btree_crc_type  crc          = ti->ti_crc_type;

	/**
	 *  Currently our thread routine only supports Keys and Values which are
	 *  a multiple of 8 bytes.
	 */
	M0_ASSERT(ti->ti_key_size == RANDOM_KEY_SIZE ||
		  (ti->ti_key_size % sizeof(uint64_t) == 0 &&
		   ti->ti_key_size > sizeof(key[0]) &&
		   ti->ti_key_size <= sizeof(key)));
	M0_ASSERT(ti->ti_value_size == RANDOM_VALUE_SIZE ||
		  (ti->ti_value_size % sizeof(uint64_t) == 0 &&
		   ((crc == M0_BCT_USER_ENC_RAW_HASH &&
		     ti->ti_value_size > sizeof(value[0]) * 2) ||
		    (crc == M0_BCT_NO_CRC &&
		     ti->ti_value_size > sizeof(value[0])) ||
		    (crc == M0_BCT_BTREE_ENC_RAW_HASH &&
		     ti->ti_value_size > sizeof(value[0]))) &&
		   ti->ti_value_size <= sizeof(value)));

	M0_CASSERT(KEY_ARR_ELE_COUNT > 1 && VAL_ARR_ELE_COUNT > 1);

	key_iter_start = ti->ti_key_first;
	key_end        = ti->ti_key_first +
			 (ti->ti_key_count * ti->ti_key_incr) - ti->ti_key_incr;

	rec.r_key.k_data   = M0_BUFVEC_INIT_BUF(&k_ptr, &ksize);
	rec.r_val          = M0_BUFVEC_INIT_BUF(&v_ptr, &vsize);

	data.key           = &rec.r_key;
	data.value         = &rec.r_val;

	ut_cb.c_act        = ut_btree_kv_put_cb;
	ut_cb.c_datum      = &data;

	get_rec.r_key.k_data   = M0_BUFVEC_INIT_BUF(&get_k_ptr, &get_ksize);
	get_rec.r_val          = M0_BUFVEC_INIT_BUF(&get_v_ptr, &get_vsize);

	get_data.key            = &get_rec.r_key;
	get_data.value          = &get_rec.r_val;
	get_data.check_value    = true;
	get_data.crc            = crc;
	get_data.embedded_ksize = true;
	get_data.embedded_vsize = true;

	ut_get_cb.c_act        = ut_btree_kv_get_cb;
	ut_get_cb.c_datum      = &get_data;

	tree                   = ti->ti_tree;

	/** Wait till all the threads have been initialised. */
	UT_THREAD_WAIT();
	m0_atomic64_inc(&threads_running);

	put_cred = M0_BE_TX_CB_CREDIT(0, 0, 0);
	m0_btree_put_credit(tree, 1, ksize, vsize, &put_cred);

	update_cred = M0_BE_TX_CB_CREDIT(0, 0, 0);
	m0_btree_update_credit(tree, 1, ksize, vsize, &update_cred);

	del_cred = M0_BE_TX_CB_CREDIT(0, 0, 0);
	m0_btree_del_credit(tree, 1, ksize, -1, &del_cred);

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
		uint64_t  kdata;

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

		ut_cb.c_act   = ut_btree_kv_put_cb;
		ut_cb.c_datum = &data;

		while (key_first <= key_last - ti->ti_key_incr) {
			/**
			 * for variable key/value size, the size will increment
			 * in multiple of 8 after each iteration. The size will
			 * wrap around on reaching MAX_KEY_SIZE. To make sure
			 * there is atleast one key and size, arr_size is
			 * incremented by 1.
			 */
			ksize = ksize_random ?
				GET_RANDOM_KEYSIZE(key, key_first,
						   key_iter_start,
						   ti->ti_key_incr):
				ti->ti_key_size;
			vsize = vsize_random ?
				GET_RANDOM_VALSIZE(value, key_first,
						   key_iter_start,
						   ti->ti_key_incr, crc) :
				ti->ti_value_size;
			/**
			 *  Embed the thread-id in LSB so that different threads
			 *  will target the same node thus causing race
			 *  conditions useful to mimic and test btree operations
			 *  in a loaded system.
			 */
			kdata = (key_first << (sizeof(ti->ti_thread_id) * 8)) +
				ti->ti_thread_id;
			kdata = m0_byteorder_cpu_to_be64(kdata);

			FILL_KEY(key, ksize, kdata);
			FILL_VALUE(value, vsize, kdata, crc);

			m0_be_ut_tx_init(tx, ut_be);
			m0_be_tx_prep(tx, &put_cred);
			rc = m0_be_tx_open_sync(tx);
			M0_ASSERT(rc == 0);

			rc = M0_BTREE_OP_SYNC_WITH_RC(&kv_op,
						      m0_btree_put(tree, &rec,
								   &ut_cb,
								   &kv_op, tx));
			M0_ASSERT(rc == 0 && data.flags == M0_BSC_SUCCESS);
			m0_be_tx_close_sync(tx);
			m0_be_tx_fini(tx);

			keys_put_count++;
			key_first += ti->ti_key_incr;

			UT_THREAD_QUIESCE_IF_REQUESTED();
		}

		/** Verify btree_update with BOF_INSERT_IF_NOT_FOUND flag.
		 * 1. call update operation for non-existing record, which
		 *    should return -ENOENT.
		 * 2. call update operation for non-existing record with,
		 *    BOF_INSERT_IF_NOT_FOUND flag which should insert record
		 *    and return success.
		 */
		ut_cb.c_datum = &data;

		ksize = ksize_random ?
			GET_RANDOM_KEYSIZE(key, key_first, key_iter_start,
					   ti->ti_key_incr):
			ti->ti_key_size;
		vsize = vsize_random ?
			GET_RANDOM_VALSIZE(value, key_first, key_iter_start,
					   ti->ti_key_incr, crc) :
			ti->ti_value_size;

		kdata = (key_first << (sizeof(ti->ti_thread_id) * 8)) +
			ti->ti_thread_id;
		kdata = m0_byteorder_cpu_to_be64(kdata);

		FILL_KEY(key, ksize, kdata);
		FILL_VALUE(value, vsize, kdata, crc);

		m0_be_ut_tx_init(tx, ut_be);
		m0_be_tx_prep(tx, &put_cred);
		rc = m0_be_tx_open_sync(tx);
		M0_ASSERT(rc == 0);

		ut_cb.c_act = ut_btree_kv_update_cb;
		rc = M0_BTREE_OP_SYNC_WITH_RC(&kv_op,
					      m0_btree_update(tree, &rec,
							      &ut_cb, 0,
							      &kv_op, tx));
		M0_ASSERT(rc == M0_ERR(-ENOENT));

		ut_cb.c_act = ut_btree_kv_put_cb;
		rc = M0_BTREE_OP_SYNC_WITH_RC(&kv_op,
					m0_btree_update(tree, &rec,
							&ut_cb,
							BOF_INSERT_IF_NOT_FOUND,
							&kv_op, tx));

		M0_ASSERT(rc == 0 && data.flags == M0_BSC_SUCCESS);
		m0_be_tx_close_sync(tx);
		m0_be_tx_fini(tx);

		keys_put_count++;
		key_first += ti->ti_key_incr;

		/** Verify btree_update for value size increase/decrease. */

		key_first     = key_iter_start;
		ut_cb.c_act   = ut_btree_kv_update_cb;
		ut_cb.c_datum = &data;
		while (vsize_random && key_first <= key_last) {
			vsize = GET_RANDOM_VALSIZE(value, key_first,
						   key_iter_start,
						   ti->ti_key_incr, crc);
			/**
			 * Skip updating value size for max val size as
			 * it can create array outbound for val[]
			 */
			if (vsize >= (VAL_ARR_ELE_COUNT * sizeof(value[0]))) {
				key_first += (ti->ti_key_incr * 5);
				continue;
			}
			/** Test value size increase case. */
			vsize += sizeof(value[0]);
			ksize = ksize_random ?
				GET_RANDOM_KEYSIZE(key, key_first,
						    key_iter_start,
						    ti->ti_key_incr):
				ti->ti_key_size;

			kdata = (key_first << (sizeof(ti->ti_thread_id) * 8)) +
				 ti->ti_thread_id;
			kdata = m0_byteorder_cpu_to_be64(kdata);

			FILL_KEY(key, ksize, kdata);
			FILL_VALUE(value, vsize, kdata, crc);

			m0_be_ut_tx_init(tx, ut_be);
			m0_be_tx_prep(tx, &update_cred);
			rc = m0_be_tx_open_sync(tx);
			M0_ASSERT(rc == 0);

			rc = M0_BTREE_OP_SYNC_WITH_RC(&kv_op,
						      m0_btree_update(tree,
								      &rec,
								      &ut_cb, 0,
								      &kv_op,
								      tx));
			M0_ASSERT(rc == 0 && data.flags == M0_BSC_SUCCESS);

			m0_be_tx_close_sync(tx);
			m0_be_tx_fini(tx);

			/** Test value size decrease case. */
			vsize -= sizeof(value[0]);
			FILL_VALUE(value, vsize, kdata, crc);

			m0_be_ut_tx_init(tx, ut_be);
			m0_be_tx_prep(tx, &update_cred);
			rc = m0_be_tx_open_sync(tx);
			M0_ASSERT(rc == 0);

			rc = M0_BTREE_OP_SYNC_WITH_RC(&kv_op,
						      m0_btree_update(tree,
								      &rec,
								      &ut_cb, 0,
								      &kv_op,
								      tx));
			M0_ASSERT(rc == 0 && data.flags == M0_BSC_SUCCESS);

			m0_be_tx_close_sync(tx);
			m0_be_tx_fini(tx);

			key_first += (ti->ti_key_incr * 5);
		}

		/**
		 * Execute one error case where we PUT a key which already
		 * exists in the btree.
		 */
		key_first = key_iter_start;
		if ((key_last - key_first) > (ti->ti_key_incr * 2))
			key_first += ti->ti_key_incr;

		ksize = ksize_random ?
			GET_RANDOM_KEYSIZE(key, key_first, key_iter_start,
					   ti->ti_key_incr):
			ti->ti_key_size;

		kdata = (key_first << (sizeof(ti->ti_thread_id) * 8)) +
			ti->ti_thread_id;
		kdata = m0_byteorder_cpu_to_be64(kdata);

		FILL_KEY(key, ksize, kdata);

		/** Skip initializing the value as this is an error case */

		m0_be_ut_tx_init(tx, ut_be);
		m0_be_tx_prep(tx, &put_cred);
		rc = m0_be_tx_open_sync(tx);
		M0_ASSERT(rc == 0);

		rc = M0_BTREE_OP_SYNC_WITH_RC(&kv_op,
					      m0_btree_put(tree, &rec, &ut_cb,
							   &kv_op, tx));
		M0_ASSERT(rc == M0_ERR(-EEXIST));
		m0_be_tx_close_sync(tx);
		m0_be_tx_fini(tx);

		/** Modify 20% of the values which have been inserted. */

		key_first     = key_iter_start;
		ut_cb.c_act   = ut_btree_kv_update_cb;
		ut_cb.c_datum = &data;

		while (key_first <= key_last) {
			/**
			 *  Embed the thread-id in LSB so that different threads
			 *  will target the same node thus causing race
			 *  conditions useful to mimic and test btree operations
			 *  in a loaded system.
			 */
			ksize = ksize_random ?
				GET_RANDOM_KEYSIZE(key, key_first,
						   key_iter_start,
						   ti->ti_key_incr):
				ti->ti_key_size;
			vsize = vsize_random ?
				GET_RANDOM_VALSIZE(value, key_first,
						   key_iter_start,
						   ti->ti_key_incr, crc) :
				ti->ti_value_size;

			kdata = (key_first << (sizeof(ti->ti_thread_id) * 8)) +
				 ti->ti_thread_id;
			kdata = m0_byteorder_cpu_to_be64(kdata);

			FILL_KEY(key, ksize, kdata);
			kdata = ~kdata;
			FILL_VALUE(value, vsize, kdata, crc);

			m0_be_ut_tx_init(tx, ut_be);
			m0_be_tx_prep(tx, &update_cred);
			rc = m0_be_tx_open_sync(tx);
			M0_ASSERT(rc == 0);

			rc = M0_BTREE_OP_SYNC_WITH_RC(&kv_op,
						      m0_btree_update(tree,
								      &rec,
								      &ut_cb, 0,
								      &kv_op,
								      tx));
			M0_ASSERT(rc == 0 && data.flags == M0_BSC_SUCCESS);
			m0_be_tx_close_sync(tx);
			m0_be_tx_fini(tx);

			key_first += (ti->ti_key_incr * 5);
		}

		/**
		 * Execute one error case where we UPDATE a key that does not
		 * exist in the btree.
		 */
		key_first = key_iter_start;
		ksize = ksize_random ?
			GET_RANDOM_KEYSIZE(key, key_first, key_iter_start,
					   ti->ti_key_incr) :
			ti->ti_key_size;

		kdata = (key_first << (sizeof(ti->ti_thread_id) * 8)) +
			 (typeof(ti->ti_thread_id))-1;
		kdata = m0_byteorder_cpu_to_be64(kdata);
		FILL_KEY(key, ksize, kdata);

		/** Skip initializing the value as this is an error case */

		m0_be_ut_tx_init(tx, ut_be);
		m0_be_tx_prep(tx, &update_cred);
		rc = m0_be_tx_open_sync(tx);
		M0_ASSERT(rc == 0);

		rc = M0_BTREE_OP_SYNC_WITH_RC(&kv_op,
					      m0_btree_update(tree, &rec,
							      &ut_cb, 0,
							      &kv_op, tx));
		M0_ASSERT(rc == M0_ERR(-ENOENT));
		m0_be_tx_close_sync(tx);
		m0_be_tx_fini(tx);

		/** GET and ITERATE over the keys which we inserted above. */

		/**  Randomly decide the iteration direction. */
		random_r(&ti->ti_random_buf, &r);

		key_first = key_iter_start;
		if (r % 2) {
			/** Iterate forward. */
			iter_dir = BOF_NEXT;
			ksize = ksize_random ?
				GET_RANDOM_KEYSIZE(key, key_first,
						   key_iter_start,
						   ti->ti_key_incr) :
				ti->ti_key_size;

			kdata = (key_first <<
				  (sizeof(ti->ti_thread_id) * 8)) +
				 ti->ti_thread_id;
			kdata = m0_byteorder_cpu_to_be64(kdata);
			FILL_KEY(key, ksize, kdata);
		} else {
			/** Iterate backward. */
			iter_dir = BOF_PREV;
			ksize = ksize_random ?
				GET_RANDOM_KEYSIZE(key, key_last,
						   key_iter_start,
						   ti->ti_key_incr) :
				ti->ti_key_size;

			kdata = (key_last <<
				  (sizeof(ti->ti_thread_id) * 8)) +
				 ti->ti_thread_id;
			kdata = m0_byteorder_cpu_to_be64(kdata);
			FILL_KEY(key, ksize, kdata);
		}
		get_ksize = ksize;
		get_vsize = ti->ti_value_size;
		get_data.check_value = true; /** Compare value with key */
		get_data.crc         = crc;

		rc = M0_BTREE_OP_SYNC_WITH_RC(&kv_op,
					      m0_btree_get(tree, &rec.r_key,
							   &ut_get_cb,
							   BOF_EQUAL, &kv_op));
		M0_ASSERT(rc == 0);

		keys_found_count++;

		while (1) {
			rc = M0_BTREE_OP_SYNC_WITH_RC(&kv_op,
						      m0_btree_iter(tree,
								    &rec.r_key,
								    &ut_get_cb,
								    iter_dir,
								    &kv_op));
			if (rc == -ENOENT)
				break;

			keys_found_count++;

			/** Copy over the gotten key for the next search. */
			ksize = get_ksize;
			for (i = 0; i < ksize / sizeof(get_key[0]); i++) {
				key[i] = get_key[i];
			}

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

		/** Fill a value in the buffer which cannot be the MIN key */
		ksize = ksize_random ?
			GET_RANDOM_KEYSIZE(key, key_first, key_iter_start,
					   ti->ti_key_incr) :
			ti->ti_key_size;
		kdata = ((key_last + 1) << (sizeof(ti->ti_thread_id) * 8)) +
			ti->ti_thread_id;
		kdata = m0_byteorder_cpu_to_be64(kdata);
		FILL_KEY(get_key, ksize, kdata);

		rc = M0_BTREE_OP_SYNC_WITH_RC(&kv_op,
					      m0_btree_minkey(tree, &ut_get_cb,
							      0, &kv_op));
		M0_ASSERT(rc == 0);

		ksize = get_ksize;
		for (i = 0; i < get_ksize / sizeof(get_key[0]); i++)
			key[i] = get_key[i];

		rc = M0_BTREE_OP_SYNC_WITH_RC(&kv_op,
					      m0_btree_iter(tree, &rec.r_key,
							    &ut_get_cb,
							    BOF_NEXT, &kv_op));
		M0_ASSERT((rc == 0) ||
			  (rc == -ENOENT && key_iter_start == key_last));
		/**
		 * The second condition in the above assert is rare but can
		 * happen if only one Key is present in the btree. We presume
		 * that no Keys from other other threads are currently present
		 * in the btree and also the current thread  added just one
		 * key in this iteration.
		 */

		for (i = 0; i < ksize / sizeof(key[1]); i++)
			get_key[i] = key[i];

		rc = M0_BTREE_OP_SYNC_WITH_RC(&kv_op,
					      m0_btree_iter(tree, &rec.r_key,
							    &ut_get_cb,
							    BOF_PREV, &kv_op));
		M0_ASSERT(rc == -ENOENT);

		kdata = ((key_iter_start - 1) <<
			 (sizeof(ti->ti_thread_id) * 8)) + ti->ti_thread_id;
		kdata = m0_byteorder_cpu_to_be64(kdata);
		FILL_KEY(get_key, ksize, kdata);

		rc = M0_BTREE_OP_SYNC_WITH_RC(&kv_op,
					      m0_btree_maxkey(tree, &ut_get_cb,
							      0, &kv_op));
		M0_ASSERT(rc == 0);

		ksize = get_ksize;
		for (i = 0; i < get_ksize / sizeof(get_key[0]); i++)
			key[i] = get_key[i];

		rc = M0_BTREE_OP_SYNC_WITH_RC(&kv_op,
					      m0_btree_iter(tree, &rec.r_key,
							    &ut_get_cb,
							    BOF_PREV, &kv_op));
		M0_ASSERT((rc == 0) ||
			  (rc == -ENOENT && key_iter_start == key_last));

		for (i = 0; i < ARRAY_SIZE(key); i += sizeof(key[0]))
			get_key[i] = key[i];

		rc = M0_BTREE_OP_SYNC_WITH_RC(&kv_op,
					      m0_btree_iter(tree, &rec.r_key,
							    &ut_get_cb,
							    BOF_NEXT, &kv_op));
		M0_ASSERT(rc == -ENOENT);

		UT_START_THREADS();

		/**
		 *  Test slant only if possible. If the increment counter is
		 *  more than 1 we can provide the intermediate value to be got
		 *  in slant mode.
		 */

		if (ti->ti_key_incr > 1) {
			uint64_t            slant_key;
			uint64_t            got_key;
			struct m0_btree_rec r;
			struct m0_btree_cb  cb;

			M0_ASSERT(key_first >= 1);

			slant_key = key_first;
			get_data.check_value = false;
			get_data.crc         = crc;

			/**
			 *  The following short named variables are used just
			 *  to maintain the code decorum by limiting code lines
			 *  within 80 chars..
			 */
			r = rec;
			cb = ut_get_cb;

			do {
				ksize = ksize_random ?
					GET_RANDOM_KEYSIZE(key, slant_key,
							   key_iter_start,
							   ti->ti_key_incr) :
					ti->ti_key_size;

				/**
				 *  Alternate between using the exact number as
				 *  Key for slant and a previous number as Key
				 *  for slant to test for both scenarios.
				 */
				kdata = (slant_key % 2) ? slant_key - 1 :
							   slant_key;
				kdata = (kdata <<
					  (sizeof(ti->ti_thread_id) * 8)) +
					 ti->ti_thread_id;
				kdata = m0_byteorder_cpu_to_be64(kdata);
				FILL_KEY(key, ksize, kdata);

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
				M0_ASSERT(got_key == slant_key);

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

		ut_cb.c_act   = ut_btree_kv_del_cb;
		ut_cb.c_datum = &data;
		while (keys_put_count) {
			ksize = ksize_random ?
				GET_RANDOM_KEYSIZE(key, del_key, key_iter_start,
						   ti->ti_key_incr) :
				ti->ti_key_size;

			kdata = (del_key << (sizeof(ti->ti_thread_id) * 8)) +
				 ti->ti_thread_id;
			kdata = m0_byteorder_cpu_to_be64(kdata);
			FILL_KEY(key, ksize, kdata);

			m0_be_ut_tx_init(tx, ut_be);
			m0_be_tx_prep(tx, &del_cred);
			rc = m0_be_tx_open_sync(tx);
			M0_ASSERT(rc == 0);

			M0_BTREE_OP_SYNC_WITH_RC(&kv_op,
						 m0_btree_del(tree, &rec.r_key,
							      &ut_cb,
							      &kv_op, tx));
			M0_ASSERT(rc == 0 && data.flags == M0_BSC_SUCCESS);
			m0_be_tx_close_sync(tx);
			m0_be_tx_fini(tx);

			del_key = (r % 2 == 0) ? del_key + ti->ti_key_incr :
						 del_key - ti->ti_key_incr;
			keys_put_count--;

			UT_THREAD_QUIESCE_IF_REQUESTED();
		}

		/**
		 *  Verify deleted Keys are not 'visible'.
		 *  We try to read the first key and last key added in this
		 *  iteration from the btree and make sure ENOENT error is
		 *  returned for each of the Keys.
		 */
		key_first = key_iter_start;

		ksize = ksize_random ?
			GET_RANDOM_KEYSIZE(key, key_first, key_iter_start,
					   ti->ti_key_incr) :
			ti->ti_key_size;

		kdata = (key_first << (sizeof(ti->ti_thread_id) * 8)) +
			ti->ti_thread_id;
		kdata = m0_byteorder_cpu_to_be64(kdata);
		FILL_KEY(key, ksize, kdata);

		rc = M0_BTREE_OP_SYNC_WITH_RC(&kv_op,
					      m0_btree_get(tree,
							   &rec.r_key,
							   &ut_get_cb,
							   BOF_EQUAL, &kv_op));
		M0_ASSERT(rc == -ENOENT);

		if (key_first != key_last) {
			ksize = ksize_random ?
				GET_RANDOM_KEYSIZE(key, key_last,
						   key_iter_start,
						   ti->ti_key_incr) :
				ti->ti_key_size;

			kdata = (key_last << (sizeof(ti->ti_thread_id) * 8)) +
				ti->ti_thread_id;
			kdata = m0_byteorder_cpu_to_be64(kdata);
			FILL_KEY(key, ksize, kdata);

			rc = M0_BTREE_OP_SYNC_WITH_RC(&kv_op,
						      m0_btree_get(tree,
								   &rec.r_key,
								   &ut_get_cb,
								   BOF_EQUAL,
								   &kv_op));
			M0_ASSERT(rc == -ENOENT);
		}

		/**
		 *  Try to delete the first key and last key added in this
		 *  iteration from the btree and make sure ENOENT error is
		 *  returned for each of the Keys.
		 */
		key_first = key_iter_start;

		ksize = ksize_random ?
			GET_RANDOM_KEYSIZE(key, key_first, key_iter_start,
					   ti->ti_key_incr) :
			ti->ti_key_size;

		kdata = (key_first << (sizeof(ti->ti_thread_id) * 8)) +
			 ti->ti_thread_id;
		kdata = m0_byteorder_cpu_to_be64(kdata);
		FILL_KEY(key, ksize, kdata);

		m0_be_ut_tx_init(tx, ut_be);
		m0_be_tx_prep(tx, &del_cred);
		rc = m0_be_tx_open_sync(tx);
		M0_ASSERT(rc == 0);

		rc = M0_BTREE_OP_SYNC_WITH_RC(&kv_op,
					      m0_btree_del(tree, &rec.r_key,
							   &ut_cb, &kv_op,
							   tx));
		M0_ASSERT(rc == -ENOENT);
		m0_be_tx_close_sync(tx);
		m0_be_tx_fini(tx);

		if (key_first != key_last) {
			ksize = ksize_random ?
				GET_RANDOM_KEYSIZE(key, key_last,
						   key_iter_start,
						   ti->ti_key_incr) :
				ti->ti_key_size;

			kdata = (key_last << (sizeof(ti->ti_thread_id) * 8)) +
				ti->ti_thread_id;
			kdata = m0_byteorder_cpu_to_be64(kdata);
			FILL_KEY(key, ksize, kdata);

			m0_be_ut_tx_init(tx, ut_be);
			m0_be_tx_prep(tx, &del_cred);
			rc = m0_be_tx_open_sync(tx);
			M0_ASSERT(rc == 0);

			rc = M0_BTREE_OP_SYNC_WITH_RC(&kv_op,
						      m0_btree_del(tree,
								   &rec.r_key,
								   &ut_cb,
								   &kv_op, tx));
			M0_ASSERT(rc == -ENOENT);
			m0_be_tx_close_sync(tx);
			m0_be_tx_fini(tx);
		}

		key_iter_start = key_last + ti->ti_key_incr;

		UT_THREAD_QUIESCE_IF_REQUESTED();
	}

	m0_atomic64_dec(&threads_running);
	/** Free resources. */
	m0_free(ti->ti_rnd_state_ptr);

	m0_be_ut_backend_thread_exit(ut_be);
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

	m0_bitmap_fini(&map_cpu_online);
}

static void btree_ut_kv_size_get(enum btree_node_type bnt, int *ksize,
				 int *vsize)
{
	uint32_t ksize_to_use = 2 * sizeof(uint64_t);

	ksize_to_use += sizeof(uint64_t); /** To accomodate size within the Key. */

	switch (bnt) {
	case BNT_FIXED_FORMAT:
		*ksize = ksize_to_use;
		*vsize = ksize_to_use;
		break;
	case BNT_FIXED_KEYSIZE_VARIABLE_VALUESIZE:
		*ksize = ksize_to_use;
		*vsize = RANDOM_VALUE_SIZE;
		break;
	case BNT_VARIABLE_KEYSIZE_FIXED_VALUESIZE:
		*ksize = RANDOM_KEY_SIZE;
		*vsize = ksize_to_use;
		break;
	case BNT_VARIABLE_KEYSIZE_VARIABLE_VALUESIZE:
		*ksize = RANDOM_KEY_SIZE;
		*vsize = RANDOM_VALUE_SIZE;
		break;
	}
}

/**
 * This test launches multiple threads which launch KV operations against one
 * btree in parallel. If thread_count is passed as '0' then one thread per core
 * is launched. If tree_count is passed as '0' then one tree per thread is
 * created.
 */
static void btree_ut_kv_oper(int32_t thread_count, int32_t tree_count,
			     enum btree_node_type bnt)
{
	int                           rc;
	struct btree_ut_thread_info  *ti;
	int                           i;
	struct m0_btree             **ut_trees;
	uint16_t                      cpu;
	void                         *rnode;
	struct m0_btree_op            b_op         = {};
	int                           ksize = 0;
	int                           vsize = 0;
	struct m0_btree_type          btree_type;
	struct m0_be_tx_credit        cred;
	struct m0_be_tx               tx_data      = {};
	struct m0_be_tx              *tx           = &tx_data;
	struct m0_fid                 fid          = M0_FID_TINIT('b', 0, 1);
	struct m0_buf                 buf;
	uint16_t                     *cpuid_ptr;
	uint16_t                      cpu_count;
	size_t                        cpu_max;
	time_t                        curr_time;
	uint32_t                      rnode_sz        = m0_pagesize_get();
	uint32_t                      rnode_sz_shift;

	M0_ENTRY();

	btree_ut_kv_size_get(bnt, &ksize, &vsize);
	btree_type.tt_id = M0_BT_UT_KV_OPS;
	btree_type.ksize = ksize;
	btree_type.vsize = vsize;

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
	else if (tree_count == RANDOM_TREE_COUNT) {
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
	struct m0_btree btree[tree_count];

	/**
	 *  Add credits count needed for allocating node space and creating node
	 *  layout on it.
	 */
	M0_ASSERT(rnode_sz != 0 && m0_is_po2(rnode_sz));
	rnode_sz_shift = __builtin_ffsl(rnode_sz) - 1;
	cred = M0_BE_TX_CB_CREDIT(0, 0, 0);
	m0_be_allocator_credit(NULL, M0_BAO_ALLOC_ALIGNED, rnode_sz,
			       rnode_sz_shift, &cred);
	m0_btree_create_credit(&btree_type, &cred, 1);
	for (i = 0; i < tree_count; i++) {
		M0_SET0(&b_op);

		m0_be_ut_tx_init(tx, ut_be);
		m0_be_tx_prep(tx, &cred);
		rc = m0_be_tx_open_sync(tx);
		M0_ASSERT(rc == 0);

		/** Create temp node space and use it as root node for btree */
		buf = M0_BUF_INIT(rnode_sz, NULL);
		M0_BE_ALLOC_ALIGN_BUF_SYNC(&buf, rnode_sz_shift, seg, tx);
		rnode = buf.b_addr;

		M0_BTREE_OP_SYNC_WITH_RC(&b_op,
					 m0_btree_create(rnode, rnode_sz,
							 &btree_type,
							 M0_BCT_NO_CRC, &b_op,
							 &btree[i], seg, &fid,
							 tx, NULL));
		M0_ASSERT(rc == M0_BSC_SUCCESS);

		m0_be_tx_close_sync(tx);
		m0_be_tx_fini(tx);
		ut_trees[i] = b_op.bo_arbor;
	}

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

	cred = M0_BE_TX_CB_CREDIT(0, 0, 0);
	m0_be_allocator_credit(NULL, M0_BAO_FREE_ALIGNED, rnode_sz,
			       rnode_sz_shift, &cred);
	m0_btree_destroy_credit(ut_trees[0], NULL, &cred, 1);
	for (i = 0; i < tree_count; i++) {
		m0_be_ut_tx_init(tx, ut_be);
		m0_be_tx_prep(tx, &cred);
		rc = m0_be_tx_open_sync(tx);
		M0_ASSERT(rc == 0);

		rnode = segaddr_addr(&ut_trees[i]->t_desc->t_root->n_addr);
		rc = M0_BTREE_OP_SYNC_WITH_RC(&b_op,
					      m0_btree_destroy(ut_trees[i],
							       &b_op, tx));
		M0_ASSERT(rc == 0);
		M0_SET0(&btree[i]);
		buf = M0_BUF_INIT(rnode_sz, rnode);
		M0_BE_FREE_ALIGN_BUF_SYNC(&buf, rnode_sz_shift, seg, tx);
		m0_be_tx_close_sync(tx);
		m0_be_tx_fini(tx);
	}

	m0_free0(&cpuid_ptr);
	m0_free(ut_trees);
	m0_free(ti);
	btree_ut_fini();

	M0_LEAVE();
}

static void ut_st_st_kv_oper(void)
{
	int i;
	for (i = 1; i <= BNT_VARIABLE_KEYSIZE_VARIABLE_VALUESIZE; i++)
	{
		if (btree_node_format[i] != NULL)
			btree_ut_kv_oper(1, 1, i);
	}
}

static void ut_mt_st_kv_oper(void)
{
	int i;
	for (i = 1; i <= BNT_VARIABLE_KEYSIZE_VARIABLE_VALUESIZE; i++)
	{
		if (btree_node_format[i] != NULL)
			btree_ut_kv_oper(0, 1, i);
	}
}

static void ut_mt_mt_kv_oper(void)
{
	int i;
	for (i = 1; i <= BNT_VARIABLE_KEYSIZE_VARIABLE_VALUESIZE; i++)
	{
		if (btree_node_format[i] != NULL)
			btree_ut_kv_oper(0, 0, i);
	}
}

static void ut_rt_rt_kv_oper(void)
{
	int i;
	for (i = 1; i <= BNT_VARIABLE_KEYSIZE_VARIABLE_VALUESIZE; i++)
	{
		if (btree_node_format[i] != NULL)
			btree_ut_kv_oper(RANDOM_THREAD_COUNT, RANDOM_TREE_COUNT,
					 i);
	}
}

/**
 * This routine is a thread handler primarily involved in creating, opening,
 * closing and destroying btree. To run out-of-sync with other threads it also
 * launches PUT, GET, ITER and DEL operations on the btree for a random count.
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
	struct ut_cb_data      data  = {
					.key            = &rec.r_key,
					.value          = &rec.r_val,
					.check_value    = false,
					.crc            = M0_BCT_NO_CRC,
					.flags          = 0,
					.embedded_ksize = false,
					.embedded_vsize = false,
				       };
	struct m0_btree_cb     ut_cb   = {
					  .c_act       = ut_btree_kv_put_cb,
					  .c_datum     = &data,
					 };
	int32_t                loop_count;
	struct m0_btree_op     kv_op           = {};
	void                  *rnode;
	struct m0_btree_type   btree_type      = {.tt_id = M0_BT_UT_KV_OPS,
					          .ksize = sizeof(key),
					          .vsize = sizeof(value),
					         };
	struct m0_be_tx         tx_data         = {};
	struct m0_be_tx        *tx             = &tx_data;
	struct m0_be_tx_credit  cred           = {};
	struct m0_fid           fid            = M0_FID_TINIT('b', 0, 1);
	int                     rc;
	uint32_t                rnode_sz       = m0_pagesize_get();
	uint32_t                rnode_sz_shift;
	struct m0_buf           buf;

	random_r(&ti->ti_random_buf, &loop_count);
	loop_count %= (MAX_TREE_LOOPS - MIN_TREE_LOOPS);
	loop_count += MIN_TREE_LOOPS;

	UT_THREAD_WAIT();
	m0_atomic64_inc(&threads_running);

	M0_ASSERT(rnode_sz != 0 && m0_is_po2(rnode_sz));
	rnode_sz_shift = __builtin_ffsl(rnode_sz) - 1;
	cred = M0_BE_TX_CB_CREDIT(0, 0, 0);
	m0_be_allocator_credit(NULL, M0_BAO_ALLOC_ALIGNED, rnode_sz,
			       rnode_sz_shift, &cred);

	/** Prepare transaction to capture tree operations. */
	m0_be_ut_tx_init(tx, ut_be);
	m0_be_tx_prep(tx, &cred);
	rc = m0_be_tx_open_sync(tx);
	M0_ASSERT(rc == 0);

	/** Create temp node space and use it as root node for btree */
	buf = M0_BUF_INIT(rnode_sz, NULL);
	M0_BE_ALLOC_ALIGN_BUF_SYNC(&buf, rnode_sz_shift, seg, tx);
	rnode = buf.b_addr;

	m0_be_tx_close_sync(tx);
	m0_be_tx_fini(tx);

	while (loop_count--) {
		struct m0_btree_op b_op      = {};
		struct m0_btree    *tree;
		struct m0_btree    btree;
		int32_t            rec_count;
		uint32_t           i;

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
		cred = M0_BE_TX_CB_CREDIT(0, 0, 0);
		m0_btree_create_credit(&btree_type, &cred, 1);

		m0_be_ut_tx_init(tx, ut_be);
		m0_be_tx_prep(tx, &cred);
		rc = m0_be_tx_open_sync(tx);
		M0_ASSERT(rc == 0);
		rc = M0_BTREE_OP_SYNC_WITH_RC(&b_op,
					      m0_btree_create(rnode, rnode_sz,
							      &btree_type,
							      M0_BCT_NO_CRC,
							      &b_op, &btree,
							      seg, &fid, tx,
							      NULL));
		M0_ASSERT(rc == 0);
		m0_be_tx_close_sync(tx);
		m0_be_tx_fini(tx);
		tree = b_op.bo_arbor;

		random_r(&ti->ti_random_buf, &rec_count);
		rec_count %= MAX_RECS_FOR_TREE_TEST;
		rec_count = rec_count ? : (MAX_RECS_FOR_TREE_TEST / 2);

		ut_cb.c_act = ut_btree_kv_put_cb;

		cred = M0_BE_TX_CB_CREDIT(0, 0, 0);
		m0_btree_put_credit(tree, 1, ksize, vsize, &cred);
		for (i = 1; i <= rec_count; i++) {
			value = key = i;

			m0_be_ut_tx_init(tx, ut_be);
			m0_be_tx_prep(tx, &cred);
			rc = m0_be_tx_open_sync(tx);
			M0_ASSERT(rc == 0);

			rc = M0_BTREE_OP_SYNC_WITH_RC(&kv_op,
						      m0_btree_put(tree, &rec,
								   &ut_cb,
								   &kv_op, tx));
			M0_ASSERT(rc == 0 && data.flags == M0_BSC_SUCCESS);
			m0_be_tx_close_sync(tx);
			m0_be_tx_fini(tx);
		}

		rc = M0_BTREE_OP_SYNC_WITH_RC(&b_op,
					      m0_btree_close(tree, &b_op));
		M0_ASSERT(rc == 0);
		M0_SET0(&btree);

		rc = M0_BTREE_OP_SYNC_WITH_RC(&b_op,
					      m0_btree_open(rnode, rnode_sz,
							    tree, seg, &b_op,
							    NULL));
		M0_ASSERT(rc == 0);

		ut_cb.c_act = ut_btree_kv_get_cb;
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
		M0_SET0(&btree);

		rc = M0_BTREE_OP_SYNC_WITH_RC(&b_op,
					      m0_btree_open(rnode, rnode_sz,
							    tree, seg, &b_op,
							    NULL));
		M0_ASSERT(rc == 0);

		ut_cb.c_act = ut_btree_kv_del_cb;

		cred = M0_BE_TX_CREDIT(0, 0);
		m0_btree_del_credit(tree, 1, ksize, -1, &cred);
		for (i = 1; i <= rec_count; i++) {
			value = key = i;

			m0_be_ut_tx_init(tx, ut_be);
			m0_be_tx_prep(tx, &cred);
			rc = m0_be_tx_open_sync(tx);
			M0_ASSERT(rc == 0);

			rc = M0_BTREE_OP_SYNC_WITH_RC(&kv_op,
						      m0_btree_del(tree,
								   &rec.r_key,
								   &ut_cb,
								   &kv_op, tx));
			M0_ASSERT(data.flags == M0_BSC_SUCCESS && rc == 0);
			m0_be_tx_close_sync(tx);
			m0_be_tx_fini(tx);
		}

		cred = M0_BE_TX_CREDIT(0, 0);
		m0_btree_destroy_credit(tree, NULL, &cred, 1);

		m0_be_ut_tx_init(tx, ut_be);
		m0_be_tx_prep(tx, &cred);
		rc = m0_be_tx_open_sync(tx);
		M0_ASSERT(rc == 0);

		rc = M0_BTREE_OP_SYNC_WITH_RC(&b_op,
					      m0_btree_destroy(tree,
							       &b_op, tx));
		M0_ASSERT(rc == 0);
		M0_SET0(&btree);

		m0_be_tx_close_sync(tx);
		m0_be_tx_fini(tx);

		/** Error Case */
		rc = M0_BTREE_OP_SYNC_WITH_RC(&b_op,
					      m0_btree_open(rnode, rnode_sz,
							    tree, seg, &b_op,
							    NULL));
		M0_ASSERT(rc == -EINVAL);
	}

	m0_atomic64_dec(&threads_running);
	cred = M0_BE_TX_CREDIT(0, 0);
	m0_be_allocator_credit(NULL, M0_BAO_FREE_ALIGNED, rnode_sz,
			       rnode_sz_shift, &cred);

	m0_be_ut_tx_init(tx, ut_be);
	m0_be_tx_prep(tx, &cred);
	rc = m0_be_tx_open_sync(tx);
	M0_ASSERT(rc == 0);

	/** Delete temp node space which was used as root node for the tree. */
	buf = M0_BUF_INIT(rnode_sz, rnode);
	M0_BE_FREE_ALIGN_BUF_SYNC(&buf, rnode_sz_shift, seg, tx);

	m0_be_tx_close_sync(tx);
	m0_be_tx_fini(tx);

	m0_be_ut_backend_thread_exit(ut_be);
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
	time_t                       curr_time;

	M0_ENTRY();

	time(&curr_time);
	M0_LOG(M0_INFO, "Using seed %lu", curr_time);
	srandom(curr_time);

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

	m0_free0(&cpuid_ptr);
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
 * Note that tree is ASSUMED to be closed before calling this function.
 */
static bool validate_nodes_on_be_segment(struct segaddr *rnode_segaddr)
{
	const struct node_type     *nt;
	struct {
		struct segaddr node_segaddr;
		uint16_t       rec_idx;
		}                   stack[MAX_TREE_HEIGHT + 1];
	uint32_t                    stack_level = 0;
	struct nd                   n;
	struct slot                 s       = { .s_node = &n };
	uint16_t                    rec_idx = 0;

	nt = btree_node_format[segaddr_ntype_get(rnode_segaddr)];
	n.n_addr = *rnode_segaddr;
	#if (AVOID_BE_SEGMENT == 0)
		M0_ASSERT(nt->nt_opaque_get(&n.n_addr) == NULL);
	#endif

	while (true) {
		/**
		 * Move down towards leaf node only if we have child nodes still
		 * to traverse.
		 */
		if (nt->nt_level(&n) > 0 &&
		    rec_idx < nt->nt_count_rec(&n)) {
			stack[stack_level].node_segaddr = n.n_addr,
			stack[stack_level].rec_idx      = rec_idx,
			stack_level++;
			s.s_idx = rec_idx;
			nt->nt_child(&s, &n.n_addr);
			n.n_addr.as_core = (uint64_t)segaddr_addr(&n.n_addr);
			#if (AVOID_BE_SEGMENT == 0)
				M0_ASSERT(nt->nt_opaque_get(&n.n_addr) == NULL);
			#endif
			rec_idx = 0;
			continue;
		}

		/**
		 * We are at the leaf node. Validate it before going back to
		 * parent.
		 */

		M0_ASSERT(nt->nt_isvalid(&n.n_addr));
#if (AVOID_BE_SEGMENT == 0)
		M0_ASSERT(nt->nt_opaque_get(&n.n_addr) == NULL);
#endif
		if (stack_level == 0)
			break;

		/**
		 * Start moving up towards parent (or grand-parent) till we find
		 * an ancestor whose child nodes are still to be traversed. If
		 * we find an ancestor whose child nodes are still to be
		 * traversed then we pick the next child node on the right.
		 */
		do {
			stack_level--;
			rec_idx  = stack[stack_level].rec_idx + 1;
			n.n_addr = stack[stack_level].node_segaddr;
		} while (rec_idx >= nt->nt_count_rec(&n) &&
			 stack_level > 0);
	}

	return true;
}

/**
 * This unit test exercises different KV operations and confirms the changes
 * persist across cluster reboots.
 */
static void ut_btree_persistence(void)
{
	void                        *rnode;
	int                          i;
	struct m0_btree_cb           ut_cb;
	struct m0_be_tx             tx_data         = {};
	struct m0_be_tx            *tx              = &tx_data;
	struct m0_be_tx_credit      cred            = {};
	struct m0_btree_op          b_op            = {};
	uint64_t                    rec_count       = MAX_RECS_PER_STREAM;
	struct m0_btree_op          kv_op           = {};
	struct m0_btree            *tree;
	struct m0_btree             btree;
	const struct m0_btree_type  bt              = {
						     .tt_id = M0_BT_UT_KV_OPS,
						     .ksize = sizeof(uint64_t),
						     .vsize = bt.ksize * 2,
						};
	const struct node_type     *nt;
	struct segaddr              rnode_segaddr;
	uint64_t                    key;
	uint64_t                    value[bt.vsize / sizeof(uint64_t)];
	m0_bcount_t                 ksize           = sizeof key;
	m0_bcount_t                 vsize           = sizeof value;
	void                       *k_ptr           = &key;
	void                       *v_ptr           = &value;
	int                         rc;
	struct m0_buf               buf;
	uint32_t                    rnode_sz        = m0_pagesize_get();
	struct m0_fid               fid             = M0_FID_TINIT('b', 0, 1);
	uint32_t                    rnode_sz_shift;
	struct m0_btree_rec         rec             = {
			    .r_key.k_data = M0_BUFVEC_INIT_BUF(&k_ptr, &ksize),
			    .r_val        = M0_BUFVEC_INIT_BUF(&v_ptr, &vsize),
			    .r_crc_type   = M0_BCT_NO_CRC,
			};
	struct ut_cb_data           put_data;
	struct ut_cb_data           get_data;

	M0_ENTRY();

	rec_count = (rec_count / 2) * 2; /** Make rec_count a multiple of 2 */

	btree_ut_init();
	/**
	 *  Run the following scenario:
	 *  1) Create a btree
	 *  2) Add records in the created tree.
	 *  3) Reload the BE segment.
	 *  4) Confirm all the records are present in the tree.
	 *  5) Delete records with EVEN numbered Keys.
	 *  6) Reload the BE segment.
	 *  7) Confirm all the records with EVEN numbered Keys are missing while
	 *     the records with ODD numbered Keys are present in the tree.
	 *  8) Now add back records with EVEN numbered Keys (the same records
	 *     which were deleted in step 6)
	 *  9) Delete records with ODD numbered Keys from the btree.
	 * 10) Reload the BE segment.
	 * 11) Confirm records with EVEN numbered Keys are present while the
	 *     records with ODD numbered Keys are missing from the tree.
	 * 12) Delete all records with EVEN numbered Keys from the tree.
	 * 13) Reload the BE segment.
	 * 14) Search for the records with all the EVEN and ODD numbered Keys,
	 *     no record should be found in the tree.
	 * 15) Destroy the btree
	 * 16) Reload the BE segment.
	 * 17) Try to open the destroyed tree. This should fail.
	 *
	 *  Capture each operation in a separate transaction.
	 */

	M0_ASSERT(rnode_sz != 0 && m0_is_po2(rnode_sz));
	rnode_sz_shift = __builtin_ffsl(rnode_sz) - 1;
	cred = M0_BE_TX_CB_CREDIT(0, 0, 0);
	m0_be_allocator_credit(NULL, M0_BAO_ALLOC_ALIGNED, rnode_sz,
			       rnode_sz_shift, &cred);
	m0_btree_create_credit(&bt, &cred, 1);

	/** Prepare transaction to capture tree operations. */
	m0_be_ut_tx_init(tx, ut_be);
	m0_be_tx_prep(tx, &cred);
	rc = m0_be_tx_open_sync(tx);
	M0_ASSERT(rc == 0);

	/** Create temp node space and use it as root node for btree */
	buf = M0_BUF_INIT(rnode_sz, NULL);
	M0_BE_ALLOC_ALIGN_BUF_SYNC(&buf, rnode_sz_shift, seg, tx);
	rnode = buf.b_addr;

	rc = M0_BTREE_OP_SYNC_WITH_RC(&b_op, m0_btree_create(rnode, rnode_sz,
							     &bt,
							     M0_BCT_NO_CRC,
							     &b_op, &btree,seg,
							     &fid, tx, NULL));
	M0_ASSERT(rc == M0_BSC_SUCCESS);
	m0_be_tx_close_sync(tx);
	m0_be_tx_fini(tx);

	tree = b_op.bo_arbor;

	cred = M0_BE_TX_CB_CREDIT(0, 0, 0);
	m0_btree_put_credit(tree, 1, ksize, vsize, &cred);

	put_data.key       = &rec.r_key;
	put_data.value     = &rec.r_val;

	ut_cb.c_act        = ut_btree_kv_put_cb;
	ut_cb.c_datum      = &put_data;

	for (i = 1; i <= rec_count; i++) {
		int      k;

		key = m0_byteorder_cpu_to_be64(i);
		for (k = 0; k < ARRAY_SIZE(value); k++)
			value[k] = key;

		m0_be_ut_tx_init(tx, ut_be);
		m0_be_tx_prep(tx, &cred);
		rc = m0_be_tx_open_sync(tx);
		M0_ASSERT(rc == 0);

		rc = M0_BTREE_OP_SYNC_WITH_RC(&kv_op,
					      m0_btree_put(tree, &rec,
							   &ut_cb,
							   &kv_op, tx));
		M0_ASSERT(rc == 0 && put_data.flags == M0_BSC_SUCCESS);
		m0_be_tx_close_sync(tx);
		m0_be_tx_fini(tx);
	}

	rc = M0_BTREE_OP_SYNC_WITH_RC(&b_op, m0_btree_close(tree, &b_op));
	M0_ASSERT(rc == 0);
	M0_SET0(&btree);

	/**
	 * Confirm root node on BE segment is valid and the opaque pointer in
	 * the node is NULL.
	 */
	rnode_segaddr.as_core = (uint64_t)rnode;
	nt = btree_node_format[segaddr_ntype_get(&rnode_segaddr)];
	M0_ASSERT(nt->nt_isvalid(&rnode_segaddr) &&
		  nt->nt_opaque_get(&rnode_segaddr) == NULL);


	/** Re-map the BE segment.*/
	m0_be_seg_close(ut_seg->bus_seg);
	rc = madvise(rnode, rnode_sz, MADV_NORMAL);
	M0_ASSERT(rc == -1 && errno == ENOMEM); /** Assert BE segment unmapped*/
	m0_be_seg_open(ut_seg->bus_seg);

	/** Confirm nodes on BE segment are still valid. */
	rnode_segaddr.as_core = (uint64_t)rnode;
	validate_nodes_on_be_segment(&rnode_segaddr);

	rc = M0_BTREE_OP_SYNC_WITH_RC(&b_op,
				      m0_btree_open(rnode, rnode_sz, tree, seg,
						    &b_op, NULL));
	M0_ASSERT(rc == 0);

	get_data.key            = &rec.r_key;
	get_data.value          = &rec.r_val;
	get_data.check_value    = true;
	get_data.crc            = M0_BCT_NO_CRC;
	get_data.embedded_ksize = false;
	get_data.embedded_vsize = false;

	cred = M0_BE_TX_CB_CREDIT(0, 0, 0);
	m0_btree_del_credit(tree, 1, ksize, vsize, &cred);

	for (i = 1; i <= rec_count; i++) {
		uint64_t             f_key;
		void                *f_key_ptr  = &f_key;
		m0_bcount_t          f_key_size  = sizeof f_key;
		struct m0_btree_key  key_in_tree;

		f_key = m0_byteorder_cpu_to_be64(i);
		key_in_tree.k_data =
				    M0_BUFVEC_INIT_BUF(&f_key_ptr, &f_key_size);
		ut_cb.c_act             = ut_btree_kv_get_cb;
		ut_cb.c_datum           = &get_data;

		rc = M0_BTREE_OP_SYNC_WITH_RC(&kv_op,
					      m0_btree_get(tree,
							   &key_in_tree,
							   &ut_cb, BOF_EQUAL,
							   &kv_op));
		M0_ASSERT(rc == M0_BSC_SUCCESS &&
			  i == m0_byteorder_be64_to_cpu(key));

		if (i % 2 == 0) {
			m0_be_ut_tx_init(tx, ut_be);
			m0_be_tx_prep(tx, &cred);
			rc = m0_be_tx_open_sync(tx);
			M0_ASSERT(rc == 0);

			ut_cb.c_act             = ut_btree_kv_del_cb;
			ut_cb.c_datum           = &get_data;

			rc = M0_BTREE_OP_SYNC_WITH_RC(&kv_op,
						      m0_btree_del(tree,
								   &key_in_tree,
								   &ut_cb,
								   &kv_op, tx));
			M0_ASSERT(rc == 0);
			m0_be_tx_close_sync(tx);
			m0_be_tx_fini(tx);
		}
	}

	rc = M0_BTREE_OP_SYNC_WITH_RC(&b_op, m0_btree_close(tree, &b_op));
	M0_ASSERT(rc == 0);
	M0_SET0(&btree);

	/**
	 * Confirm root node on BE segment is valid and the opaque pointer in
	 * the node is NULL.
	 */
	rnode_segaddr.as_core = (uint64_t)rnode;
	nt = btree_node_format[segaddr_ntype_get(&rnode_segaddr)];
	M0_ASSERT(nt->nt_isvalid(&rnode_segaddr) &&
		  nt->nt_opaque_get(&rnode_segaddr) == NULL);

	/** Re-map the BE segment.*/
	m0_be_seg_close(ut_seg->bus_seg);
	rc = madvise(rnode, rnode_sz, MADV_NORMAL);
	M0_ASSERT(rc == -1 && errno == ENOMEM); /** Assert BE segment unmapped*/
	m0_be_seg_open(ut_seg->bus_seg);

	/** Confirm nodes on BE segment are still valid. */
	rnode_segaddr.as_core = (uint64_t)rnode;
	validate_nodes_on_be_segment(&rnode_segaddr);

	rc = M0_BTREE_OP_SYNC_WITH_RC(&b_op,
				      m0_btree_open(rnode, rnode_sz, tree, seg,
						    &b_op, NULL));
	M0_ASSERT(rc == 0);

	get_data.key            = &rec.r_key;
	get_data.value          = &rec.r_val;
	get_data.check_value    = true;
	get_data.crc            = M0_BCT_NO_CRC;
	get_data.embedded_ksize = false;
	get_data.embedded_vsize = false;

	for (i = 1; i <= rec_count; i++) {
		int                  k;
		uint64_t             f_key;
		void                *f_key_ptr  = &f_key;
		m0_bcount_t          f_key_size  = sizeof f_key;
		struct m0_btree_key  key_in_tree;

		f_key = m0_byteorder_cpu_to_be64(i);
		key_in_tree.k_data =
				    M0_BUFVEC_INIT_BUF(&f_key_ptr, &f_key_size);
		ut_cb.c_act             = ut_btree_kv_get_cb;
		ut_cb.c_datum           = &get_data;

		rc = M0_BTREE_OP_SYNC_WITH_RC(&kv_op,
					      m0_btree_get(tree,
							   &key_in_tree,
							   &ut_cb, BOF_EQUAL,
							   &kv_op));
		M0_ASSERT((i % 2 != 0 && rc == M0_BSC_SUCCESS) ||
			  (i % 2 == 0 && rc == M0_ERR(-ENOENT)));

		if (i % 2 != 0) {
			cred = M0_BE_TX_CB_CREDIT(0, 0, 0);
			m0_btree_del_credit(tree, 1, ksize, vsize, &cred);

			m0_be_ut_tx_init(tx, ut_be);
			m0_be_tx_prep(tx, &cred);
			rc = m0_be_tx_open_sync(tx);
			M0_ASSERT(rc == 0);

			ut_cb.c_act             = ut_btree_kv_del_cb;
			ut_cb.c_datum           = &get_data;

			rc = M0_BTREE_OP_SYNC_WITH_RC(&kv_op,
						      m0_btree_del(tree,
								   &key_in_tree,
								   &ut_cb,
								   &kv_op, tx));
			M0_ASSERT(rc == 0);
			m0_be_tx_close_sync(tx);
			m0_be_tx_fini(tx);
		} else {
			cred = M0_BE_TX_CB_CREDIT(0, 0, 0);
			m0_btree_put_credit(tree, 1, ksize, vsize, &cred);

			m0_be_ut_tx_init(tx, ut_be);
			m0_be_tx_prep(tx, &cred);
			rc = m0_be_tx_open_sync(tx);
			M0_ASSERT(rc == 0);

			key = m0_byteorder_cpu_to_be64(i);
			for (k = 0; k < ARRAY_SIZE(value); k++)
				value[k] = key;

			ut_cb.c_act        = ut_btree_kv_put_cb;
			ut_cb.c_datum      = &put_data;

			rc = M0_BTREE_OP_SYNC_WITH_RC(&kv_op,
						      m0_btree_put(tree, &rec,
								   &ut_cb,
								   &kv_op, tx));
			M0_ASSERT(rc == 0 && put_data.flags == M0_BSC_SUCCESS);
			m0_be_tx_close_sync(tx);
			m0_be_tx_fini(tx);
		}
	}

	rc = M0_BTREE_OP_SYNC_WITH_RC(&b_op, m0_btree_close(tree, &b_op));
	M0_ASSERT(rc == 0);
	M0_SET0(&btree);

	/**
	 * Confirm root node on BE segment is valid and the opaque pointer in
	 * the node is NULL.
	 */
	rnode_segaddr.as_core = (uint64_t)rnode;
	nt = btree_node_format[segaddr_ntype_get(&rnode_segaddr)];
	M0_ASSERT(nt->nt_isvalid(&rnode_segaddr) &&
		  nt->nt_opaque_get(&rnode_segaddr) == NULL);

	/** Re-map the BE segment.*/
	m0_be_seg_close(ut_seg->bus_seg);
	rc = madvise(rnode, rnode_sz, MADV_NORMAL);
	M0_ASSERT(rc == -1 && errno == ENOMEM); /** Assert BE segment unmapped*/
	m0_be_seg_open(ut_seg->bus_seg);

	/** Confirm nodes on BE segment are still valid. */
	rnode_segaddr.as_core = (uint64_t)rnode;
	validate_nodes_on_be_segment(&rnode_segaddr);

	rc = M0_BTREE_OP_SYNC_WITH_RC(&b_op,
				      m0_btree_open(rnode, rnode_sz, tree, seg,
						    &b_op, NULL));
	M0_ASSERT(rc == 0);

	get_data.key            = &rec.r_key;
	get_data.value          = &rec.r_val;
	get_data.check_value    = true;
	get_data.embedded_ksize = false;
	get_data.embedded_vsize = false;

	cred = M0_BE_TX_CB_CREDIT(0, 0, 0);
	m0_btree_del_credit(tree, 1, ksize, vsize, &cred);

	for (i = 1; i <= rec_count; i++) {
		uint64_t             f_key;
		void                *f_key_ptr  = &f_key;
		m0_bcount_t          f_key_size  = sizeof f_key;
		struct m0_btree_key  key_in_tree;

		f_key = m0_byteorder_cpu_to_be64(i);
		key_in_tree.k_data =
				    M0_BUFVEC_INIT_BUF(&f_key_ptr, &f_key_size);
		ut_cb.c_act        = ut_btree_kv_get_cb;
		ut_cb.c_datum      = &get_data;

		rc = M0_BTREE_OP_SYNC_WITH_RC(&kv_op,
					      m0_btree_get(tree,
							   &key_in_tree,
							   &ut_cb, BOF_EQUAL,
							   &kv_op));
		M0_ASSERT((i % 2 == 0 && rc == M0_BSC_SUCCESS) ||
			  (i % 2 != 0 && rc == M0_ERR(-ENOENT)));

		if (i % 2 == 0) {
			m0_be_ut_tx_init(tx, ut_be);
			m0_be_tx_prep(tx, &cred);
			rc = m0_be_tx_open_sync(tx);
			M0_ASSERT(rc == 0);

			ut_cb.c_act             = ut_btree_kv_del_cb;
			ut_cb.c_datum           = &get_data;

			rc = M0_BTREE_OP_SYNC_WITH_RC(&kv_op,
						      m0_btree_del(tree,
								   &key_in_tree,
								   &ut_cb,
								   &kv_op, tx));
			M0_ASSERT(rc == 0);
			m0_be_tx_close_sync(tx);
			m0_be_tx_fini(tx);
		}
	}

	rc = M0_BTREE_OP_SYNC_WITH_RC(&b_op, m0_btree_close(tree, &b_op));
	M0_ASSERT(rc == 0);
	M0_SET0(&btree);
	/**
	 * Confirm root node on BE segment is valid and the opaque pointer in
	 * the node is NULL.
	 */
	rnode_segaddr.as_core = (uint64_t)rnode;
	nt = btree_node_format[segaddr_ntype_get(&rnode_segaddr)];
	M0_ASSERT(nt->nt_isvalid(&rnode_segaddr) &&
		  nt->nt_opaque_get(&rnode_segaddr) == NULL);

	/** Re-map the BE segment.*/
	m0_be_seg_close(ut_seg->bus_seg);
	rc = madvise(rnode, rnode_sz, MADV_NORMAL);
	M0_ASSERT(rc == -1 && errno == ENOMEM); /** Assert BE segment unmapped*/
	m0_be_seg_open(ut_seg->bus_seg);

	/** Confirm nodes on BE segment are still valid. */
	rnode_segaddr.as_core = (uint64_t)rnode;
	validate_nodes_on_be_segment(&rnode_segaddr);

	rc = M0_BTREE_OP_SYNC_WITH_RC(&b_op,
				      m0_btree_open(rnode, rnode_sz, tree, seg,
						    &b_op, NULL));
	M0_ASSERT(rc == 0);

	get_data.key            = &rec.r_key;
	get_data.value          = &rec.r_val;
	get_data.check_value    = true;
	get_data.embedded_ksize = false;
	get_data.embedded_vsize = false;

	for (i = 1; i <= rec_count; i++) {
		uint64_t            f_key;
		void                *f_key_ptr  = &f_key;
		m0_bcount_t         f_key_size  = sizeof f_key;
		struct m0_btree_key key_in_tree;

		f_key = m0_byteorder_cpu_to_be64(i);
		key_in_tree.k_data =
				     M0_BUFVEC_INIT_BUF(&f_key_ptr, &f_key_size);
		ut_cb.c_act        = ut_btree_kv_get_cb;
		ut_cb.c_datum      = &get_data;

		rc = M0_BTREE_OP_SYNC_WITH_RC(&kv_op,
					      m0_btree_get(tree,
							   &key_in_tree,
							   &ut_cb, BOF_EQUAL,
							   &kv_op));
		M0_ASSERT(rc == M0_ERR(-ENOENT));
	}

	cred = M0_BE_TX_CREDIT(0, 0);
	m0_btree_destroy_credit(tree, NULL, &cred, 1);

	m0_be_ut_tx_init(tx, ut_be);
	m0_be_tx_prep(tx, &cred);
	rc = m0_be_tx_open_sync(tx);
	M0_ASSERT(rc == 0);

	rc = M0_BTREE_OP_SYNC_WITH_RC(&b_op, m0_btree_destroy(tree, &b_op, tx));
	M0_ASSERT(rc == 0);
	M0_SET0(&btree);

	m0_be_tx_close_sync(tx);
	m0_be_tx_fini(tx);

	/** Re-map the BE segment.*/
	m0_be_seg_close(ut_seg->bus_seg);
	rc = madvise(rnode, rnode_sz, MADV_NORMAL);
	M0_ASSERT(rc == -1 && errno == ENOMEM); /** Assert BE segment unmapped*/
	m0_be_seg_open(ut_seg->bus_seg);


	/**
	 * Confirm root node on BE segment is not valid and the opaque
	 * pointer in the node is NULL.
	 */
	rnode_segaddr.as_core = (uint64_t)rnode;
	nt = btree_node_format[segaddr_ntype_get(&rnode_segaddr)];
	M0_ASSERT(!nt->nt_isvalid(&rnode_segaddr) &&
		  nt->nt_opaque_get(&rnode_segaddr) == NULL);

	rc = M0_BTREE_OP_SYNC_WITH_RC(&b_op,
				      m0_btree_open(rnode, rnode_sz, tree, seg,
						    &b_op, NULL));
	M0_ASSERT(rc == -EINVAL);


	/** Delete temp node space which was used as root node for the tree. */
	cred = M0_BE_TX_CREDIT(0, 0);
	m0_be_allocator_credit(NULL, M0_BAO_FREE_ALIGNED, rnode_sz,
			       rnode_sz_shift, &cred);

	m0_be_ut_tx_init(tx, ut_be);
	m0_be_tx_prep(tx, &cred);
	rc = m0_be_tx_open_sync(tx);
	M0_ASSERT(rc == 0);

	buf = M0_BUF_INIT(rnode_sz, rnode);
	M0_BE_FREE_ALIGN_BUF_SYNC(&buf, rnode_sz_shift, seg, tx);

	m0_be_tx_close_sync(tx);
	m0_be_tx_fini(tx);

	btree_ut_fini();
}

static void ut_btree_truncate(void)
{
	void                        *rnode;
	int                          i;
	struct m0_btree_cb           ut_cb;
	struct m0_be_tx             tx_data         = {};
	struct m0_be_tx            *tx              = &tx_data;
	struct m0_be_tx_credit      cred            = {};
	struct m0_btree_op          b_op            = {};
	uint64_t                    rec_count       = MAX_RECS_PER_STREAM;
	struct m0_btree_op          kv_op           = {};
	struct m0_btree            *tree;
	struct m0_btree             btree;
	const struct m0_btree_type  bt              = {
						     .tt_id = M0_BT_UT_KV_OPS,
						     .ksize = sizeof(uint64_t),
						     .vsize = bt.ksize * 2,
						};
	uint64_t                    key;
	uint64_t                    value[bt.vsize / sizeof(uint64_t)];
	m0_bcount_t                 ksize           = sizeof key;
	m0_bcount_t                 vsize           = sizeof value;
	void                       *k_ptr           = &key;
	void                       *v_ptr           = &value;
	int                         rc;
	struct m0_buf               buf;
	uint32_t                    rnode_sz        = m0_pagesize_get();
	struct m0_fid               fid             = M0_FID_TINIT('b', 0, 1);
	uint32_t                    rnode_sz_shift;
	struct m0_btree_rec         rec             = {
			    .r_key.k_data = M0_BUFVEC_INIT_BUF(&k_ptr, &ksize),
			    .r_val        = M0_BUFVEC_INIT_BUF(&v_ptr, &vsize),
			    .r_crc_type   = M0_BCT_NO_CRC,
			};
	struct ut_cb_data           put_data;
	m0_bcount_t                 limit;

	M0_ENTRY();

	btree_ut_init();

	M0_ASSERT(rnode_sz != 0 && m0_is_po2(rnode_sz));
	rnode_sz_shift = __builtin_ffsl(rnode_sz) - 1;
	cred = M0_BE_TX_CB_CREDIT(0, 0, 0);
	m0_be_allocator_credit(NULL, M0_BAO_ALLOC_ALIGNED, rnode_sz,
			       rnode_sz_shift, &cred);
	m0_btree_create_credit(&bt, &cred, 1);

	/** Prepare transaction to capture tree operations. */
	m0_be_ut_tx_init(tx, ut_be);
	m0_be_tx_prep(tx, &cred);
	rc = m0_be_tx_open_sync(tx);
	M0_ASSERT(rc == 0);

	/** Create temp node space and use it as root node for btree */
	buf = M0_BUF_INIT(rnode_sz, NULL);
	M0_BE_ALLOC_ALIGN_BUF_SYNC(&buf, rnode_sz_shift, seg, tx);
	rnode = buf.b_addr;

	rc = M0_BTREE_OP_SYNC_WITH_RC(&b_op, m0_btree_create(rnode, rnode_sz,
							     &bt,
							     M0_BCT_NO_CRC,
							     &b_op, &btree, seg,
							     &fid, tx, NULL));
	M0_ASSERT(rc == M0_BSC_SUCCESS);
	m0_be_tx_close_sync(tx);
	m0_be_tx_fini(tx);

	tree = b_op.bo_arbor;

	cred = M0_BE_TX_CB_CREDIT(0, 0, 0);
	m0_btree_put_credit(tree, 1, ksize, vsize, &cred);

	put_data.key       = &rec.r_key;
	put_data.value     = &rec.r_val;

	ut_cb.c_act        = ut_btree_kv_put_cb;
	ut_cb.c_datum      = &put_data;

	for (i = 1; i <= rec_count; i++) {
		int      k;

		key = m0_byteorder_cpu_to_be64(i);
		for (k = 0; k < ARRAY_SIZE(value); k++)
			value[k] = key;

		m0_be_ut_tx_init(tx, ut_be);
		m0_be_tx_prep(tx, &cred);
		rc = m0_be_tx_open_sync(tx);
		M0_ASSERT(rc == 0);

		rc = M0_BTREE_OP_SYNC_WITH_RC(&kv_op,
					      m0_btree_put(tree, &rec,
							   &ut_cb,
							   &kv_op, tx));
		M0_ASSERT(rc == 0 && put_data.flags == M0_BSC_SUCCESS);
		m0_be_tx_close_sync(tx);
		m0_be_tx_fini(tx);
	}

	/**
	 * The test assumes that "limit" will be more than the total number of
	 * nodes present in the record. Hence only one function call to
	 * m0_btree_truncate is sufficient.
	 */
	cred = M0_BE_TX_CREDIT(0, 0);
	m0_btree_truncate_credit(tx, tree, &cred, &limit);
	m0_be_ut_tx_init(tx, ut_be);
	m0_be_tx_prep(tx, &cred);
	rc = m0_be_tx_open_sync(tx);
	M0_ASSERT(rc == 0);

	rc = M0_BTREE_OP_SYNC_WITH_RC(&kv_op,
				      m0_btree_truncate(tree, limit, tx,
							&kv_op));
	m0_be_tx_close_sync(tx);
	m0_be_tx_fini(tx);

	/** Verify the tree is empty */
	M0_ASSERT(m0_btree_is_empty(tree));

	cred = M0_BE_TX_CREDIT(0, 0);
	m0_be_allocator_credit(NULL, M0_BAO_FREE_ALIGNED, rnode_sz,
			       rnode_sz_shift, &cred);
	m0_btree_destroy_credit(tree, NULL, &cred, 1);

	m0_be_ut_tx_init(tx, ut_be);
	m0_be_tx_prep(tx, &cred);
	rc = m0_be_tx_open_sync(tx);
	M0_ASSERT(rc == 0);

	rc = M0_BTREE_OP_SYNC_WITH_RC(&b_op, m0_btree_destroy(tree, &b_op, tx));
	M0_ASSERT(rc == 0);
	M0_SET0(&btree);

	/** Delete temp node space which was used as root node for the tree. */
	buf = M0_BUF_INIT(rnode_sz, rnode);
	M0_BE_FREE_ALIGN_BUF_SYNC(&buf, rnode_sz_shift, seg, tx);

	m0_be_tx_close_sync(tx);
	m0_be_tx_fini(tx);

	btree_ut_fini();
}

static void ut_lru_test(void)
{
	void                       *rnode;
	int                         i;
	int64_t                     mem_after_alloc;
	int64_t                     mem_init;
	int64_t                     mem_increased;
	int64_t                     mem_freed;
	int64_t                     mem_after_free;
	struct m0_btree_cb          ut_cb;
	struct m0_be_tx             tx_data         = {};
	struct m0_be_tx            *tx              = &tx_data;
	struct m0_be_tx_credit      cred            = {};
	struct m0_btree_op          b_op            = {};
	uint64_t                    rec_count       = MAX_RECS_PER_STREAM*50;
	struct m0_btree_op          kv_op           = {};
	struct m0_btree            *tree;
	struct m0_btree             btree;
	const struct m0_btree_type  bt              = {
						     .tt_id = M0_BT_UT_KV_OPS,
						     .ksize = sizeof(uint64_t),
						     .vsize = bt.ksize * 2,
						};
	uint64_t                    key;
	uint64_t                    value[bt.vsize / sizeof(uint64_t)];
	m0_bcount_t                 ksize           = sizeof key;
	m0_bcount_t                 vsize           = sizeof value;
	void                       *k_ptr           = &key;
	void                       *v_ptr           = &value;
	int                         rc;
	struct m0_buf               buf;
	uint32_t                    rnode_sz        = m0_pagesize_get();
	struct m0_fid               fid             = M0_FID_TINIT('b', 0, 1);
	uint32_t                    rnode_sz_shift;
	struct m0_btree_rec         rec             = {
			    .r_key.k_data = M0_BUFVEC_INIT_BUF(&k_ptr, &ksize),
			    .r_val        = M0_BUFVEC_INIT_BUF(&v_ptr, &vsize),
			};
	struct ut_cb_data           put_data;
	/**
	 * In this UT, we are testing the functionality of LRU list purge and
	 * be-allocator with chunk align parameter.
	 *
	 * 1. Allocate and fill up the btree with multiple records.
	 * 2. Verify the size increase in memory.
	 * 3. Use the m0_btree_lrulist_purge() to reduce the size by freeing up
	 *    the unused nodes present in LRU list.
	 * 4. Verify the reduction in size.
	 */
	M0_ENTRY();

	btree_ut_init();
	mem_init = sysconf(_SC_AVPHYS_PAGES) * sysconf(_SC_PAGESIZE);
	M0_LOG(M0_INFO,"Mem Init (%"PRId64").\n",mem_init);

	M0_ASSERT(rnode_sz != 0 && m0_is_po2(rnode_sz));
	rnode_sz_shift = __builtin_ffsl(rnode_sz) - 1;
	cred = M0_BE_TX_CB_CREDIT(0, 0, 0);
	m0_be_allocator_credit(NULL, M0_BAO_ALLOC_ALIGNED, rnode_sz,
			       rnode_sz_shift, &cred);
	m0_btree_create_credit(&bt, &cred, 1);

	/** Prepare transaction to capture tree operations. */
	m0_be_ut_tx_init(tx, ut_be);
	m0_be_tx_prep(tx, &cred);
	rc = m0_be_tx_open_sync(tx);
	M0_ASSERT(rc == 0);

	/** Create temp node space and use it as root node for btree */
	buf = M0_BUF_INIT(rnode_sz, NULL);
	M0_BE_ALLOC_ALIGN_BUF_SYNC(&buf, rnode_sz_shift, seg, tx);
	rnode = buf.b_addr;

	rc = M0_BTREE_OP_SYNC_WITH_RC(&b_op, m0_btree_create(rnode, rnode_sz,
							     &bt,
							     M0_BCT_NO_CRC,
							     &b_op, &btree, seg,
							     &fid, tx, NULL));
	M0_ASSERT(rc == M0_BSC_SUCCESS);
	m0_be_tx_close_sync(tx);
	m0_be_tx_fini(tx);

	tree = b_op.bo_arbor;

	cred = M0_BE_TX_CB_CREDIT(0, 0, 0);
	m0_btree_put_credit(tree, 1, ksize, vsize, &cred);

	put_data.key       = &rec.r_key;
	put_data.value     = &rec.r_val;

	ut_cb.c_act        = ut_btree_kv_put_cb;
	ut_cb.c_datum      = &put_data;

	for (i = 1; i <= rec_count; i++) {
		int      k;

		key = m0_byteorder_cpu_to_be64(i);
		for (k = 0; k < ARRAY_SIZE(value); k++)
			value[k] = key;

		m0_be_ut_tx_init(tx, ut_be);
		m0_be_tx_prep(tx, &cred);
		rc = m0_be_tx_open_sync(tx);
		M0_ASSERT(rc == 0);

		rc = M0_BTREE_OP_SYNC_WITH_RC(&kv_op,
					      m0_btree_put(tree, &rec,
							   &ut_cb,
							   &kv_op, tx));
		M0_ASSERT(rc == 0 && put_data.flags == M0_BSC_SUCCESS);
		m0_be_tx_close_sync(tx);
		m0_be_tx_fini(tx);
	}

	mem_after_alloc = sysconf(_SC_AVPHYS_PAGES) * sysconf(_SC_PAGESIZE);
	mem_increased   = mem_init - mem_after_alloc;
	M0_LOG(M0_INFO, "Mem After Alloc (%"PRId64") || Mem Increase (%"PRId64").\n",
	       mem_after_alloc, mem_increased);

	M0_ASSERT(ndlist_tlist_length(&btree_lru_nds) > 0);

	mem_freed      = m0_btree_lrulist_purge(mem_increased/2);
	mem_after_free = sysconf(_SC_AVPHYS_PAGES) * sysconf(_SC_PAGESIZE);
	M0_LOG(M0_INFO, "Mem After Free (%"PRId64") || Mem freed (%"PRId64").\n",
	       mem_after_free, mem_freed);

	btree_ut_fini();
}

/**
 * This test launches multiple threads each of of which create different btree's
 * which host records with different CRC type embedded within them.
 */
static void ut_btree_crc_test(void)
{
	int                         rc;
	struct btree_ut_thread_info *ti;
	int                         i;
	uint16_t                    cpu;
	void                        *rnode;
	struct m0_btree_op          b_op           = {};
	struct btree_crc_data {
				struct m0_btree_type    bcr_btree_type;
				enum m0_btree_crc_type  bcr_crc_type;
			      } btrees_with_crc[] = {
			{
				{
					BNT_FIXED_FORMAT, 2 * sizeof(uint64_t),
					2 * sizeof(uint64_t)
				},
				M0_BCT_NO_CRC,
			},
			{
				{
					BNT_FIXED_KEYSIZE_VARIABLE_VALUESIZE,
					2 * sizeof(uint64_t), RANDOM_VALUE_SIZE
				},
				M0_BCT_NO_CRC,
			},
			{
				{
					BNT_VARIABLE_KEYSIZE_VARIABLE_VALUESIZE,
					RANDOM_KEY_SIZE, RANDOM_VALUE_SIZE
				},
				M0_BCT_NO_CRC,
			},
			{
				{
					BNT_FIXED_FORMAT, 2 * sizeof(uint64_t),
					3 * sizeof(uint64_t)
				},
				M0_BCT_USER_ENC_RAW_HASH,
			},
			{
				{
					BNT_FIXED_KEYSIZE_VARIABLE_VALUESIZE,
					2 * sizeof(uint64_t), RANDOM_VALUE_SIZE
				},
				M0_BCT_USER_ENC_RAW_HASH,
			},
			{
				{
					BNT_VARIABLE_KEYSIZE_VARIABLE_VALUESIZE,
					RANDOM_KEY_SIZE, RANDOM_VALUE_SIZE
				},
				M0_BCT_USER_ENC_RAW_HASH,
			},
			{
				{
					BNT_FIXED_FORMAT, 2 * sizeof(uint64_t),
					2 * sizeof(uint64_t)
				},
				M0_BCT_BTREE_ENC_RAW_HASH,
			},
			{
				{
					BNT_FIXED_KEYSIZE_VARIABLE_VALUESIZE,
					2 * sizeof(uint64_t), RANDOM_VALUE_SIZE
				},
				M0_BCT_BTREE_ENC_RAW_HASH,
			},
			{
				{
					BNT_VARIABLE_KEYSIZE_VARIABLE_VALUESIZE,
					RANDOM_KEY_SIZE, RANDOM_VALUE_SIZE
				},
				M0_BCT_BTREE_ENC_RAW_HASH,
			},
		};
	uint16_t                    thread_count = ARRAY_SIZE(btrees_with_crc);
	struct m0_be_tx_credit      cred;
	struct m0_be_tx             tx_data        = {};
	struct m0_be_tx             *tx            = &tx_data;
	struct m0_fid               fid            = M0_FID_TINIT('b', 0, 1);
	struct m0_buf               buf;
	uint16_t                    *cpuid_ptr;
	uint16_t                    cpu_count;
	size_t                      cpu_max;
	time_t                      curr_time;
	uint32_t                    rnode_sz       = sysconf(_SC_PAGESIZE);
	uint32_t                    rnode_sz_shift;
	struct m0_btree             btree[thread_count];

	M0_ENTRY();

	M0_ASSERT(rnode_sz != 0 && m0_is_po2(rnode_sz));
	rnode_sz_shift = __builtin_ffsl(rnode_sz) - 1;

	M0_ALLOC_ARR(ti, thread_count);
	M0_ASSERT(ti != NULL);

	time(&curr_time);
	M0_LOG(M0_INFO, "Using seed %lu", curr_time);
	srandom(curr_time);

	/**
	 *  1) Assign CPU cores to the threads.
	 *  2) Create btree(s) to be used by all the threads.
	 *  3) Init and Start the threads which do KV operations.
	 *  4) Wait till all the threads are done.
	 *  5) Close the btree
	 *  6) Destroy the btree
	 */

	btree_ut_init();

	online_cpu_id_get(&cpuid_ptr, &cpu_count);

	UT_STOP_THREADS();
	m0_atomic64_set(&threads_running, 0);
	m0_atomic64_set(&threads_quiesced, 0);

	M0_ALLOC_ARR(ti, thread_count);
	M0_ASSERT(ti != NULL);

	cpu_max = m0_processor_nr_max();

	cpu = 1; /** We skip Core-0 for Linux kernel and other processes. */
	for (i = 0; i < thread_count; i++) {
		enum m0_btree_crc_type  crc_type;
		struct m0_btree_type    btree_type;

		crc_type = btrees_with_crc[i].bcr_crc_type;
		btree_type = btrees_with_crc[i].bcr_btree_type;

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
		ti[i].ti_key_size   = btree_type.ksize;
		ti[i].ti_value_size = btree_type.vsize;
		ti[i].ti_crc_type   = crc_type;
		ti[i].ti_random_bursts = (thread_count > 1);
		do {
			ti[i].ti_rng_seed_base = random();
		} while (ti[i].ti_rng_seed_base == 0);

		M0_SET0(&b_op);

		cred = M0_BE_TX_CB_CREDIT(0, 0, 0);
		m0_be_allocator_credit(NULL, M0_BAO_ALLOC_ALIGNED, rnode_sz,
				       rnode_sz_shift, &cred);
		m0_btree_create_credit(&btree_type, &cred, 1);

		m0_be_ut_tx_init(tx, ut_be);
		m0_be_tx_prep(tx, &cred);
		rc = m0_be_tx_open_sync(tx);
		M0_ASSERT(rc == 0);

		/** Create temp node space and use it as root node for btree */
		buf = M0_BUF_INIT(rnode_sz, NULL);
		M0_BE_ALLOC_ALIGN_BUF_SYNC(&buf, rnode_sz_shift, seg, tx);
		rnode = buf.b_addr;

		rc = M0_BTREE_OP_SYNC_WITH_RC(&b_op,
					      m0_btree_create(rnode, rnode_sz,
							      &btree_type,
							      crc_type, &b_op,
							      &btree[i], seg,
							      &fid, tx, NULL));
		M0_ASSERT(rc == M0_BSC_SUCCESS);

		m0_be_tx_close_sync(tx);
		m0_be_tx_fini(tx);

		ti[i].ti_tree = &btree[i];

		rc = M0_THREAD_INIT(&ti[i].ti_q, struct btree_ut_thread_info *,
				    btree_ut_thread_init,
				    &btree_ut_kv_oper_thread_handler, &ti[i],
				    "Thread-%d", i);
		M0_ASSERT(rc == 0);
	}

	/** Initialized all the threads. Now start the test ... */
	UT_START_THREADS();

	for (i = 0; i < thread_count; i++) {
		m0_thread_join(&ti[i].ti_q);
		m0_thread_fini(&ti[i].ti_q);
	}

	cred = M0_BE_TX_CB_CREDIT(0, 0, 0);
	m0_be_allocator_credit(NULL, M0_BAO_FREE_ALIGNED, rnode_sz,
			       rnode_sz_shift, &cred);
	m0_btree_destroy_credit(&btree[0], NULL, &cred, 1);
	for (i = 0; i < thread_count; i++) {
		m0_be_ut_tx_init(tx, ut_be);
		m0_be_tx_prep(tx, &cred);
		rc = m0_be_tx_open_sync(tx);
		M0_ASSERT(rc == 0);

		rnode = segaddr_addr(&btree[i].t_desc->t_root->n_addr);
		rc = M0_BTREE_OP_SYNC_WITH_RC(&b_op,
					      m0_btree_destroy(&btree[i],
							       &b_op, tx));
		M0_ASSERT(rc == 0);
		M0_SET0(&btree[i]);
		buf = M0_BUF_INIT(rnode_sz, rnode);
		M0_BE_FREE_ALIGN_BUF_SYNC(&buf, rnode_sz_shift, seg, tx);
		m0_be_tx_close_sync(tx);
		m0_be_tx_fini(tx);
	}

	m0_free0(&cpuid_ptr);
	m0_free(ti);
	btree_ut_fini();

	M0_LEAVE();
}

/**
 * This unit test creates records in the nodes and then simulates the loading
 * of those nodes across cluster reboots.
 */
static void ut_btree_crc_persist_test_internal(struct m0_btree_type   *bt,
					       enum m0_btree_crc_type  crc_type)
{
	void                       *rnode;
	int                         i;
	struct m0_btree_cb          ut_cb;
	struct m0_be_tx             tx_data        = {};
	struct m0_be_tx            *tx             = &tx_data;
	struct m0_be_tx_credit      cred           = {};
	struct m0_btree_op          b_op           = {};
	uint64_t                    rec_count      = MAX_RECS_PER_STREAM;
	struct m0_btree_op          kv_op          = {};
	struct m0_btree            *tree;
	struct m0_btree             btree;
	uint64_t                    key;
	uint64_t                    value[VAL_ARR_ELE_COUNT];
	uint64_t                    kdata;
	m0_bcount_t                 ksize          = sizeof(key);
	m0_bcount_t                 vsize          = sizeof(value);
	void                       *k_ptr          = &key;
	void                       *v_ptr          = &value;
	int                         rc;
	struct m0_buf               buf;
	const struct node_type     *nt;
	struct segaddr              rnode_segaddr;
	uint32_t                    rnode_sz       = m0_pagesize_get();
	struct m0_fid               fid            = M0_FID_TINIT('b', 0, 1);
	uint32_t                    rnode_sz_shift;
	struct m0_btree_rec         rec            = {
				    .r_key.k_data  = M0_BUFVEC_INIT_BUF(&k_ptr,
								       &ksize),
				    .r_val         = M0_BUFVEC_INIT_BUF(&v_ptr,
								       &vsize),
				    .r_crc_type    = crc_type,
						    };
	struct ut_cb_data           put_data;
	struct ut_cb_data           get_data;
	m0_bcount_t                 limit;

	M0_ASSERT(bt->ksize == RANDOM_VALUE_SIZE || bt->ksize == sizeof(key));
	M0_ASSERT(bt->vsize == RANDOM_VALUE_SIZE ||
		  (bt->vsize % sizeof(uint64_t) == 0 &&
		   (((crc_type == M0_BCT_NO_CRC &&
		     bt->vsize > sizeof(value[0])) ||
		    (crc_type == M0_BCT_USER_ENC_RAW_HASH &&
		     bt->vsize > sizeof(value[0]) * 2) ||
		    (crc_type == M0_BCT_USER_ENC_FORMAT_FOOTER &&
		     bt->vsize > (sizeof(value[0]) +
				  sizeof(struct m0_format_header) +
				  sizeof(struct m0_format_footer))) ||
		    (crc_type == M0_BCT_BTREE_ENC_RAW_HASH &&
		     bt->vsize > sizeof(value[0]))) &&
		    bt->vsize <= sizeof(value))));

	M0_CASSERT(VAL_ARR_ELE_COUNT > 1);

	M0_ENTRY();

	rec_count = (rec_count / 2) * 2; /** Make rec_count a multiple of 2 */

	btree_ut_init();
	/**
	 *  Run the following scenario:
	 *  1) Create a btree
	 *  2) Add records in the created tree.
	 *  3) Reload the BE segment.
	 *  4) Confirm all the records are present in the tree and CRC matches.
	 *  5) Modify the Value for all the records.
	 *  6) Reload the BE segment.
	 *  7) Confirm all the records are present in the tree and CRC matches.
	 *
	 *  Capture each operation in a separate transaction.
	 */

	M0_ASSERT(rnode_sz != 0 && m0_is_po2(rnode_sz));
	rnode_sz_shift = __builtin_ffsl(rnode_sz) - 1;
	cred = M0_BE_TX_CB_CREDIT(0, 0, 0);
	m0_be_allocator_credit(NULL, M0_BAO_ALLOC_ALIGNED, rnode_sz,
			       rnode_sz_shift, &cred);
	m0_btree_create_credit(bt, &cred, 1);

	/** Prepare transaction to capture tree operations. */
	m0_be_ut_tx_init(tx, ut_be);
	m0_be_tx_prep(tx, &cred);
	rc = m0_be_tx_open_sync(tx);
	M0_ASSERT(rc == 0);

	/** Create temp node space and use it as root node for btree */
	buf = M0_BUF_INIT(rnode_sz, NULL);
	M0_BE_ALLOC_ALIGN_BUF_SYNC(&buf, rnode_sz_shift, seg, tx);
	rnode = buf.b_addr;

	rc = M0_BTREE_OP_SYNC_WITH_RC(&b_op,
				      m0_btree_create(rnode, rnode_sz, bt,
						      crc_type, &b_op, &btree,
						      seg, &fid, tx, NULL));
	M0_ASSERT(rc == M0_BSC_SUCCESS);
	m0_be_tx_close_sync(tx);
	m0_be_tx_fini(tx);

	tree = b_op.bo_arbor;

	cred = M0_BE_TX_CB_CREDIT(0, 0, 0);
	m0_btree_put_credit(tree, 1, ksize, vsize, &cred);

	put_data.key       = &rec.r_key;
	put_data.value     = &rec.r_val;

	ut_cb.c_act        = ut_btree_kv_put_cb;
	ut_cb.c_datum      = &put_data;

	for (i = 1; i <= rec_count; i++) {
		key = m0_byteorder_cpu_to_be64(i);

		vsize = bt->vsize == RANDOM_VALUE_SIZE ?
			GET_RANDOM_VALSIZE(value, i, 1, 1, crc_type) :
			bt->vsize;

		FILL_VALUE(value, vsize, key, crc_type);

		m0_be_ut_tx_init(tx, ut_be);
		m0_be_tx_prep(tx, &cred);
		rc = m0_be_tx_open_sync(tx);
		M0_ASSERT(rc == 0);

		rc = M0_BTREE_OP_SYNC_WITH_RC(&kv_op,
					      m0_btree_put(tree, &rec,
							   &ut_cb,
							   &kv_op, tx));
		M0_ASSERT(rc == 0 && put_data.flags == M0_BSC_SUCCESS);
		m0_be_tx_close_sync(tx);
		m0_be_tx_fini(tx);
	}

	rc = M0_BTREE_OP_SYNC_WITH_RC(&b_op, m0_btree_close(tree, &b_op));
	M0_ASSERT(rc == 0);
	M0_SET0(&btree);

	/**
	 * Confirm root node on BE segment is valid and the opaque pointer in
	 * the node is NULL.
	 */
	rnode_segaddr.as_core = (uint64_t)rnode;
	nt = btree_node_format[segaddr_ntype_get(&rnode_segaddr)];
	M0_ASSERT(nt->nt_isvalid(&rnode_segaddr) &&
		  nt->nt_opaque_get(&rnode_segaddr) == NULL);

	/** Re-map the BE segment.*/
	m0_be_seg_close(ut_seg->bus_seg);
	rc = madvise(rnode, rnode_sz, MADV_NORMAL);
	M0_ASSERT(rc == -1 && errno == ENOMEM); /** Assert BE segment unmapped*/
	m0_be_seg_open(ut_seg->bus_seg);

	rc = M0_BTREE_OP_SYNC_WITH_RC(&b_op,
				      m0_btree_open(rnode, rnode_sz, tree, seg,
						    &b_op, NULL));
	M0_ASSERT(rc == 0);

	get_data.key            = &rec.r_key;
	get_data.value          = &rec.r_val;
	get_data.check_value    = false;
	get_data.crc            = crc_type;
	get_data.embedded_ksize = false;
	get_data.embedded_vsize = false;

	for (i = 1; i <= rec_count; i++) {
		uint64_t             f_key;
		void                *f_kptr  = &f_key;
		m0_bcount_t          f_ksize = sizeof f_key;
		struct m0_btree_key  key_in_tree;

		f_key              = m0_byteorder_cpu_to_be64(i);
		key_in_tree.k_data = M0_BUFVEC_INIT_BUF(&f_kptr, &f_ksize);
		vsize              = sizeof(value);
		ut_cb.c_act        = ut_btree_kv_get_cb;
		ut_cb.c_datum      = &get_data;

		rc = M0_BTREE_OP_SYNC_WITH_RC(&kv_op,
					      m0_btree_get(tree, &key_in_tree,
							   &ut_cb, BOF_EQUAL,
							   &kv_op));
		M0_ASSERT(rc == M0_BSC_SUCCESS &&
			  i == m0_byteorder_be64_to_cpu(key));

		/** Update the Value of this Record. */
		key = m0_byteorder_cpu_to_be64(i);

		vsize = bt->vsize == RANDOM_VALUE_SIZE ?
			GET_RANDOM_VALSIZE(value, i, 1, 1, crc_type) :
			bt->vsize;

		/** Modify Value for the above Key. */
		kdata = m0_byteorder_cpu_to_be64(rec_count + i);
		FILL_VALUE(value, vsize, kdata, crc_type);

		m0_be_ut_tx_init(tx, ut_be);
		m0_be_tx_prep(tx, &cred);
		rc = m0_be_tx_open_sync(tx);
		M0_ASSERT(rc == 0);

		ut_cb.c_act        = ut_btree_kv_update_cb;
		ut_cb.c_datum      = &put_data;

		rc = M0_BTREE_OP_SYNC_WITH_RC(&kv_op,
					      m0_btree_update(tree, &rec,
							      &ut_cb, 0,
							      &kv_op, tx));
		M0_ASSERT(rc == 0 && put_data.flags == M0_BSC_SUCCESS);
		m0_be_tx_close_sync(tx);
		m0_be_tx_fini(tx);
	}

	rc = M0_BTREE_OP_SYNC_WITH_RC(&b_op, m0_btree_close(tree, &b_op));
	M0_ASSERT(rc == 0);
	M0_SET0(&btree);

	/**
	 * Confirm root node on BE segment is valid and the opaque pointer in
	 * the node is NULL.
	 */
	rnode_segaddr.as_core = (uint64_t)rnode;
	nt = btree_node_format[segaddr_ntype_get(&rnode_segaddr)];
	M0_ASSERT(nt->nt_isvalid(&rnode_segaddr) &&
		  nt->nt_opaque_get(&rnode_segaddr) == NULL);

	/** Re-map the BE segment.*/
	m0_be_seg_close(ut_seg->bus_seg);
	rc = madvise(rnode, rnode_sz, MADV_NORMAL);
	M0_ASSERT(rc == -1 && errno == ENOMEM); /** Assert BE segment unmapped*/
	m0_be_seg_open(ut_seg->bus_seg);

	rc = M0_BTREE_OP_SYNC_WITH_RC(&b_op,
				      m0_btree_open(rnode, rnode_sz, tree, seg,
						    &b_op, NULL));
	M0_ASSERT(rc == 0);

	get_data.key            = &rec.r_key;
	get_data.value          = &rec.r_val;
	get_data.check_value    = false;
	get_data.crc            = crc_type;
	get_data.embedded_ksize = false;
	get_data.embedded_vsize = false;

	for (i = 1; i <= rec_count; i++) {
		uint64_t             f_key;
		void                *f_kptr  = &f_key;
		m0_bcount_t          f_ksize = sizeof f_key;
		struct m0_btree_key  key_in_tree;

		f_key              = m0_byteorder_cpu_to_be64(i);
		key_in_tree.k_data = M0_BUFVEC_INIT_BUF(&f_kptr, &f_ksize);
		ut_cb.c_act        = ut_btree_kv_get_cb;
		ut_cb.c_datum      = &get_data;
		vsize              = sizeof(value);

		rc = M0_BTREE_OP_SYNC_WITH_RC(&kv_op,
					      m0_btree_get(tree, &key_in_tree,
							   &ut_cb, BOF_EQUAL,
							   &kv_op));
		M0_ASSERT(rc == M0_BSC_SUCCESS &&
			  i == m0_byteorder_be64_to_cpu(key));
	}

	cred = M0_BE_TX_CREDIT(0, 0);
	m0_btree_truncate_credit(tx, tree, &cred, &limit);
	m0_be_ut_tx_init(tx, ut_be);
	m0_be_tx_prep(tx, &cred);
	rc = m0_be_tx_open_sync(tx);
	M0_ASSERT(rc == 0);

	rc = M0_BTREE_OP_SYNC_WITH_RC(&kv_op,
				      m0_btree_truncate(tree, limit, tx,
							&kv_op));
	m0_be_tx_close_sync(tx);
	m0_be_tx_fini(tx);

	/** Verify the tree is empty */
	M0_ASSERT(m0_btree_is_empty(tree));

	cred = M0_BE_TX_CREDIT(0, 0);
	m0_btree_destroy_credit(tree, NULL, &cred, 1);

	m0_be_ut_tx_init(tx, ut_be);
	m0_be_tx_prep(tx, &cred);
	rc = m0_be_tx_open_sync(tx);
	M0_ASSERT(rc == 0);

	rc = M0_BTREE_OP_SYNC_WITH_RC(&b_op, m0_btree_destroy(tree, &b_op, tx));
	M0_ASSERT(rc == 0);
	M0_SET0(&btree);

	m0_be_tx_close_sync(tx);
	m0_be_tx_fini(tx);

	/** Delete temp node space which was used as root node for the tree. */
	cred = M0_BE_TX_CREDIT(0, 0);
	m0_be_allocator_credit(NULL, M0_BAO_FREE_ALIGNED, rnode_sz,
			       rnode_sz_shift, &cred);

	m0_be_ut_tx_init(tx, ut_be);
	m0_be_tx_prep(tx, &cred);
	rc = m0_be_tx_open_sync(tx);
	M0_ASSERT(rc == 0);

	buf = M0_BUF_INIT(rnode_sz, rnode);
	M0_BE_FREE_ALIGN_BUF_SYNC(&buf, rnode_sz_shift, seg, tx);

	m0_be_tx_close_sync(tx);
	m0_be_tx_fini(tx);

	btree_ut_fini();
}

static void ut_btree_crc_persist_test(void)
{
	struct btree_crc_data {
		struct m0_btree_type    bcr_btree_type;
		enum m0_btree_crc_type  bcr_crc_type;
	} btrees_with_crc[] = {
		{
			{
				BNT_FIXED_FORMAT, sizeof(uint64_t),
				2 * sizeof(uint64_t)
			},
			M0_BCT_NO_CRC,
		},
		{
			{
				BNT_FIXED_KEYSIZE_VARIABLE_VALUESIZE,
				sizeof(uint64_t), RANDOM_VALUE_SIZE
			},
			M0_BCT_NO_CRC,
		},
		{
			{
				BNT_VARIABLE_KEYSIZE_VARIABLE_VALUESIZE,
				sizeof(uint64_t), RANDOM_VALUE_SIZE
			},
			M0_BCT_NO_CRC,
		},
		{
			{
				BNT_FIXED_FORMAT, sizeof(uint64_t),
				3 * sizeof(uint64_t)
			},
			M0_BCT_USER_ENC_RAW_HASH,
		},
		{
			{
				BNT_FIXED_KEYSIZE_VARIABLE_VALUESIZE,
				sizeof(uint64_t), RANDOM_VALUE_SIZE
			},
			M0_BCT_USER_ENC_RAW_HASH,
		},
		{
			{
				BNT_VARIABLE_KEYSIZE_VARIABLE_VALUESIZE,
				sizeof(uint64_t), RANDOM_VALUE_SIZE
			},
			M0_BCT_USER_ENC_RAW_HASH,
		},
		{
			{
				BNT_FIXED_FORMAT, sizeof(uint64_t),
				6 * sizeof(uint64_t)
			},
			M0_BCT_USER_ENC_FORMAT_FOOTER,
		},
		{
			{
				BNT_FIXED_KEYSIZE_VARIABLE_VALUESIZE,
				sizeof(uint64_t), RANDOM_VALUE_SIZE
			},
			M0_BCT_USER_ENC_FORMAT_FOOTER,
		},
		{
			{
				BNT_VARIABLE_KEYSIZE_VARIABLE_VALUESIZE,
				sizeof(uint64_t), RANDOM_VALUE_SIZE
			},
			M0_BCT_USER_ENC_FORMAT_FOOTER,
		},
		{
			{
				BNT_FIXED_FORMAT, sizeof(uint64_t),
				2 * sizeof(uint64_t)
			},
			M0_BCT_BTREE_ENC_RAW_HASH,
		},
		{
			{
				BNT_FIXED_KEYSIZE_VARIABLE_VALUESIZE,
				sizeof(uint64_t), RANDOM_VALUE_SIZE
			},
			M0_BCT_BTREE_ENC_RAW_HASH,
		},
		{
			{
				BNT_VARIABLE_KEYSIZE_VARIABLE_VALUESIZE,
				RANDOM_VALUE_SIZE, RANDOM_VALUE_SIZE
			},
			M0_BCT_BTREE_ENC_RAW_HASH,
		},
	};
	uint32_t test_count = ARRAY_SIZE(btrees_with_crc);
	uint32_t i;


	for (i = 0; i < test_count; i++) {
		struct m0_btree_type   *bt = &btrees_with_crc[i].bcr_btree_type;
		enum m0_btree_crc_type  crc = btrees_with_crc[i].bcr_crc_type;
		ut_btree_crc_persist_test_internal(bt, crc);
	}

}

static int ut_btree_suite_init(void)
{
	M0_ENTRY();

	M0_ALLOC_PTR(ut_be);
	M0_ASSERT(ut_be != NULL);

	M0_ALLOC_PTR(ut_seg);
	M0_ASSERT(ut_seg != NULL);
	/* Init BE */
	m0_be_ut_backend_init(ut_be);
	m0_be_ut_seg_init(ut_seg, ut_be, BE_UT_SEG_SIZE);
	seg = ut_seg->bus_seg;

	g_process_fid = g_process_fid;

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
		{"basic_tree_op_icp",               ut_basic_tree_oper_icp},
		{"lru_test",                        ut_lru_test},
		{"multi_stream_kv_op",              ut_multi_stream_kv_oper},
		{"single_thread_single_tree_kv_op", ut_st_st_kv_oper},
		{"single_thread_tree_op",           ut_st_tree_oper},
		{"multi_thread_single_tree_kv_op",  ut_mt_st_kv_oper},
		{"multi_thread_multi_tree_kv_op",   ut_mt_mt_kv_oper},
		{"random_threads_and_trees_kv_op",  ut_rt_rt_kv_oper},
		{"multi_thread_tree_op",            ut_mt_tree_oper},
		{"btree_persistence",               ut_btree_persistence},
		{"btree_truncate",                  ut_btree_truncate},
		{"btree_crc_test",                  ut_btree_crc_test},
		{"btree_crc_persist_test",          ut_btree_crc_persist_test},
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
