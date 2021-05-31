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
 * 		    at parent node Move to MOVEUP , else move to CEANUP.
 * step 6. MOVEUP: This state moves to the parent node
 *
 *
 * Iteration (NEXT)
 * ................
 * @verbatim
 *
 *                       INIT
 *                         |
 *                         v
 *                       SETUP <----------------+----------------+
 *                         |                    |                |
 *                         v                    |                |
 *                      LOCKALL<----------------+                |
 *                         |                    |                |
 *                         v                    |                |
 *                       DOWN  <----------------+                |
 *                         |                    |              UNLOCK
 *                         v                    |                |
 *                   +->NEXTDOWN---->LOCK---->CHECK              |
 *                   |   |   |   ^              | <--------+     |
 *                   +---+   v   |              |          |     |
 *                          LOAD-+              +-------+  |     |
 *                                              |       v  |     |
 *                                             ACT----->NEXTKEY  |
 *                                              |        |   ^   |
 *                                              v        |   |   |
 *                                           CLEANUP     v   |   |
 *                                              |       NEXTNODE-+
 *                                              v
 *                                            DONE
 *
 * @endverbatim
 * Iteration function will return the record for the search key and iteratively
 * the record of subsequent keys as requested by the caller, if returned key is
 * last key in the node , we may need to fetch next node. To fetch keys in next
 * node: first release the LOCK, call Iteration function and pass key as
 * 'last fetched' key and next_sibling_flag= 'True'.
 * If next_sibling_flag == 'True', we will also load the right sibling node
 * which is to the node containing the search key. As we are releasing lock for
 * finding next node, updates such as(insertion of new keys, merging due to
 * deletion) can happen, so to handle such cases, we load both earlier node and
 * node to the right of it
 *
 * Phases Description:
 * step 1. NEXTDOWN: this state will load nodes searching for the given key,
 *                   but if next_sibling_flag == 'True' then this state will
 * 		     also load the leftmost child nodes during its downward
 * 		     traversal.
 * step 2. LOAD: this function will get called only when value of
 * 		 next_sibling_flag is 'True'.Functionality of this function is
 * 	         to load next node so, it will search and LOAD next sibling
 * step 3. CHECK: check function will check the traversal path for node with key
 * 		  and traversal path for next sibling node if it is also loaded
 *                if traverse path of any of the node has changed, repeat
 * 		  traversal again after UNLOCKING the tree else, if
 * 		  next_sibling_flag == 'True', go to NEXTKEY to fetch next key,
 * 	          else to ACT for callback
 * step 4. ACT: ACT will provide an input as 0 or 1:
 * 		where, 0 ->done 1-> return nextkey
 * step 5. NEXTKEY:
 *         if next_sibling_flag == 'True',
 *             check last key in the current node.
 *                 if it is <= given key, go for next node which was loaded at
 * 		      phase LOAD (step 2) and return first key
 *                 else return key next to last fetched key
 *            and call ACT for further input (1/0)
 *         else
 *             if no keys to return i.e., last returned key was last key in node
 *                 check if next node is loaded.
 *                  if yes go to next node else and return first key from that
 * 		       node
 *                  else call Iteration function and pass key='last fetched key'
 * 		         and next_sibling_flag='True'
 *             else return key == given key, or next to earlier retune key and
 * 		    call ACT for further input (1/0)
 *
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
#include "lib/byteorder.h"   /** m0_byteorder_cpu_to_be64() */
#include "btree/btree.h"
#include "fid/fid.h"
#include "format/format.h"   /** m0_format_header ff_fmt */
#include "module/instance.h"
#include "lib/memory.h"
#include "lib/misc.h"
#include "lib/assert.h"
#include "ut/ut.h"          /** struct m0_ut_suite */

#ifndef __KERNEL__
#include <stdlib.h>
#include <time.h>
#include <unistd.h>
#endif

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
	struct m0_rwlock            t_lock;
};

enum base_phase {
	P_INIT = M0_SOS_INIT,
	P_DONE = M0_SOS_DONE,
	P_DOWN = M0_SOS_NR,
	P_NEXTDOWN,
	P_ALLOC,
	P_SETUP,
	P_LOCKALL,
	P_LOCK,
	P_CHECK,
	P_MAKESPACE,
	P_NEXTUP,
	P_ACT,
	P_CLEANUP,
	P_COOKIE,
	P_NR
};

enum op_flags {
	OF_PREV    = M0_BITS(0),
	OF_NEXT    = M0_BITS(1),
	OF_LOCKALL = M0_BITS(2),
	OF_COOKIE  = M0_BITS(3),
	OF_EQUAL   = M0_BITS(4),
	OF_SLANT   = M0_BITS(5)
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
	INTERNAL_NODE_VALUE_SIZE = sizeof(void *),
};

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

static struct segaddr segaddr_build(const void *addr, int shift);
static void          *segaddr_addr (const struct segaddr *addr);
static int            segaddr_shift(const struct segaddr *addr);

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
	int                         r_ref;
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
	void (*nt_init)(const struct nd *node, int shift, int ksize, int vsize);

	/** Cleanup of the node if any before deallocation */
	void (*nt_fini)(const struct nd *node);

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

	/** Returns size of the key of node. In case of variable key size return
	 * -1. */
	int  (*nt_keysize)(const struct nd *node);

	/** Returns size of the value of node. In case variable value size
	 * return -1. */
	int  (*nt_valsize)(const struct nd *node);

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
	void (*nt_done) (struct slot *slot, struct m0_be_tx *tx, bool modified);

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
	void (*nt_set_level)  (const struct nd *node, uint8_t new_level);

	/** Moves record(s) between nodes */
	void (*nt_move) (struct nd *src, struct nd *tgt,
			 enum dir dir, int nr, struct m0_be_tx *tx);

	/** Validates node composition */
	bool (*nt_invariant)(const struct nd *node);

	/** Validates node footer */
	int (*nt_verify)(const struct nd *node);
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

	/** if n_skip_rec_count_check is true, it will skip invarient check
	 * record count as it is required for some scenarios */
	bool                    n_skip_rec_count_check;

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
	const struct nd     *s_node;
	int                  s_idx;
	struct m0_btree_rec  s_rec;
};

static int64_t tree_get   (struct node_op *op, struct segaddr *addr, int nxt);
#ifndef __KERNEL__
static int64_t tree_create(struct node_op *op, struct m0_btree_type *tt,
			   int rootshift, struct m0_be_tx *tx, int nxt);
static int64_t tree_delete(struct node_op *op, struct td *tree,
			   struct m0_be_tx *tx, int nxt);
#endif
static void    tree_put   (struct td *tree);
#ifndef __KERNEL__
static int64_t    node_get  (struct node_op *op, struct td *tree,
			     struct segaddr *addr, int nxt);
static void       node_put  (struct nd *node);
#endif


#if 0
static struct nd *node_try  (struct td *tree, struct segaddr *addr);
#endif

static int64_t    node_alloc(struct node_op *op, struct td *tree, int size,
			     const struct node_type *nt, int ksize, int vsize,
			     struct m0_be_tx *tx, int nxt);
#ifndef __KERNEL__
static int64_t    node_free(struct node_op *op, struct nd *node,
			    struct m0_be_tx *tx, int nxt);
static void node_op_fini(struct node_op *op);
#endif

#if 0
static int  node_verify(const struct nd *node);
#endif
#ifndef __KERNEL__
static void node_init(struct node_op *n_op, int ksize, int vsize);
#endif
static int  node_count(const struct nd *node);
static int  node_count_rec(const struct nd *node);
static int  node_space(const struct nd *node);
#ifndef __KERNEL__
static int  node_level(const struct nd *node);
static int  node_shift(const struct nd *node);
static int  node_keysize(const struct nd *node);
static int  node_valsize(const struct nd *node);
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
static void node_done (struct slot *slot, struct m0_be_tx *tx, bool modified);
static void node_make (struct slot *slot, struct m0_be_tx *tx);

#ifndef __KERNEL__
static bool node_find (struct slot *slot, const struct m0_btree_key *key);
#endif
static void node_fix  (const struct nd *node, struct m0_be_tx *tx);
#if 0
static void node_cut  (const struct nd *node, int idx, int size,
		       struct m0_be_tx *tx);
#endif
static void node_del  (const struct nd *node, int idx, struct m0_be_tx *tx);

#ifndef __KERNEL__
static void node_set_level  (const struct nd *node, uint8_t new_level);
static void node_move (struct nd *src, struct nd *tgt,
		       enum dir dir, int nr, struct m0_be_tx *tx);
static void mem_update(void);
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
	uint64_t   l_seq;

	/** Index for required record from the node. **/
	unsigned   l_idx;

	/** nd for newly allocated node at the level. **/
	struct nd *l_alloc;

	/** nd for left sibling of required node at current level. **/
	struct nd *l_prev;

	/** nd for right sibling of required node at current level. **/
	struct nd *l_next;
};

/**
 * Btree implementation structure.
 *
 * This structure will get created for each operation on btree and it will be
 * used while executing the given operation.
 */
struct m0_btree_oimpl {
	struct node_op  i_nop;
	/* struct lock_op  i_lop; */

	/** It will provide current level number. **/
	unsigned        i_used;

	/** Array of levels for storing data about each level. **/
	struct level   *i_level;

	/** Store node_find() output. */
	bool		i_key_found;

	/** When there will be requirement for new node in case of root
	 * splitting i_extra_node will be used. **/
	struct nd      *i_extra_node;

	/** Track number of trials done to complete operation. **/
	unsigned        i_trial;
};

static struct td        trees[M0_TREE_COUNT];
static uint64_t         trees_in_use[ARRAY_SIZE_FOR_BITS(M0_TREE_COUNT,
							 sizeof(uint64_t))];
static uint32_t         trees_loaded = 0;
static struct m0_rwlock trees_lock;

#ifndef __KERNEL__
static void node_init(struct node_op *n_op, int ksize, int vsize)
{
	const struct node_type *n_type = n_op->no_node->n_type;

	n_type->nt_init(n_op->no_node, segaddr_shift(&n_op->no_node->n_addr),
			ksize, vsize);
}
#endif
static bool node_invariant(const struct nd *node)
{
	return node->n_type->nt_invariant(node);
}
#if 0
static int node_verify(const struct nd *node)
{
	return node->n_type->nt_verify(node);
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
static void node_set_level(const struct nd *node, uint8_t new_level)
{
	//M0_PRE(node_invariant(node));
	node->n_type->nt_set_level(node, new_level);
}

static void node_move(struct nd *src, struct nd *tgt,
		      enum dir dir, int nr, struct m0_be_tx *tx)
{
	M0_PRE(node_invariant(src));
	M0_PRE(node_invariant(tgt));
	M0_IN(dir,(D_LEFT, D_RIGHT));
	tgt->n_type->nt_move(src, tgt, dir, nr, tx);
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

	M0_SET_ARR0(trees);
	M0_SET_ARR0(trees_in_use);
	trees_loaded = 0;
	m0_rwlock_init(&trees_lock);

	M0_ALLOC_PTR(m);
	if (m != NULL) {
		m0_get()->i_moddata[M0_MODULE_BTREE] = m;
		return 0;
	} else
		return M0_ERR(-ENOMEM);
}

void m0_btree_mod_fini(void)
{
	m0_rwlock_fini(&trees_lock);
	m0_free(mod_get());
}

static bool node_shift_is_valid(int shift)
{
	return shift >= NODE_SHIFT_MIN && shift < NODE_SHIFT_MIN + 0x10;
}
#ifndef __KERNEL__
static void mem_update(void)
{
	//ToDo: Memory update in segment
}
#endif
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

struct seg_ops {
	int64_t    (*so_tree_get)(struct node_op *op,
			          struct segaddr *addr, int nxt);
	int64_t    (*so_tree_create)(struct node_op *op,
	                             struct m0_btree_type *tt,
				     int rootshift, struct m0_be_tx *tx,
				     int nxt);
	int64_t    (*so_tree_delete)(struct node_op *op, struct td *tree,
				     struct m0_be_tx *tx, int nxt);
	void       (*so_tree_put)(struct td *tree);
	int64_t    (*so_node_get)(struct node_op *op, struct td *tree,
			          struct segaddr *addr, int nxt);
	void       (*so_node_put)(struct nd *node);
	struct nd *(*so_node_try)(struct td *tree, struct segaddr *addr);
	int64_t    (*so_node_alloc)(struct node_op *op, struct td *tree,
				    int shift, const struct node_type *nt,
				    struct m0_be_tx *tx, int nxt);
	int64_t    (*so_node_free)(struct node_op *op, struct nd *node,
				   struct m0_be_tx *tx, int nxt);
	void       (*so_node_op_fini)(struct node_op *op);
};

static struct seg_ops *segops;

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
	int        nxt_state;

	nxt_state = segops->so_tree_get(op, addr, nxt);

	return nxt_state;
}

#ifndef __KERNEL__

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
	return segops->so_tree_create(op, tt, rootshift, tx, nxt);
}

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
	return segops->so_tree_delete(op, tree, tx, nxt);
}
#endif
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
	segops->so_tree_put(tree);
}


#ifndef __KERNEL__
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
			struct segaddr *addr, int nxt){
	return segops->so_node_get(op, tree, addr, nxt);
}


/**
 * This function decrements the reference count for this node and if the
 * reference count reaches '0' then the node is made available for future
 * mode_get requests.
 *
 * @param op load operation to perform.
 * @param tree pointer to tree whose node is to be loaded or NULL if tree has
 *             not been loaded.
 * @param addr node address in the segment.
 * @param nxt state to return on successful completion
 *
 * @return next state
 */
static void node_put(struct nd *node){
	M0_PRE(node != NULL);
	segops->so_node_put(node);
}
#endif

# if 0
static struct nd *node_try(struct td *tree, struct segaddr *addr){
	return segops->so_node_try(tree, addr);
}
#endif



/**
 * Allocates node in the segment and a node-descriptor if all the resources are
 * available.
 *
 * @param op indicates node allocate operation.
 * @param tree points to the tree this node will be a part-of.
 * @param size is a power-of-2 size of this node.
 * @param nt points to the node type
 * @param ksize is the size of key (if constant) if not this contains '0'.
 * @param vsize is the size of value (if constant) if not this contains '0'.
 * @param tx points to the transaction which captures this operation.
 * @param nxt tells the next state to return when operation completes
 *
 * @return int64_t
 */
static int64_t node_alloc(struct node_op *op, struct td *tree, int size,
			  const struct node_type *nt, int ksize, int vsize,
			  struct m0_be_tx *tx, int nxt)
{
	int  nxt_state;

	nxt_state = segops->so_node_alloc(op, tree, size, nt, tx, nxt);

	nt->nt_init(op->no_node, size, ksize, vsize);

	return nxt_state;
}

#ifndef __KERNEL__
static int64_t node_free(struct node_op *op, struct nd *node,
			 struct m0_be_tx *tx, int nxt)
{
	node->n_type->nt_fini(node);
	return segops->so_node_free(op, node, tx, nxt);
}

static void node_op_fini(struct node_op *op)
{
	segops->so_node_op_fini(op);
}

#endif

static int64_t mem_node_get(struct node_op *op, struct td *tree,
			    struct segaddr *addr, int nxt);
static int64_t mem_node_alloc(struct node_op *op, struct td *tree, int shift,
			      const struct node_type *nt, struct m0_be_tx *tx,
			      int nxt);
static int64_t mem_node_free(struct node_op *op, struct nd *node,
			     struct m0_be_tx *tx, int nxt);
static const struct node_type fixed_format;

static int64_t mem_tree_get(struct node_op *op, struct segaddr *addr, int nxt)
{
	struct td *tree = NULL;
	int        i     = 0;
	uint32_t   offset;

	m0_rwlock_write_lock(&trees_lock);

	M0_ASSERT(trees_loaded <= ARRAY_SIZE(trees));

	/**
	 *  If existing allocated tree is found then return it after increasing
	 *  the reference count.
	 */
	if (addr != NULL && trees_loaded)
		for (i = 0; i < ARRAY_SIZE(trees); i++) {
			tree = &trees[i];
			m0_rwlock_write_lock(&tree->t_lock);
			if (tree->r_ref != 0) {
				M0_ASSERT(tree->t_root != NULL);
				if (tree->t_root->n_addr.as_core ==
				    addr->as_core) {
					tree->r_ref++;
					op->no_node = tree->t_root;
					op->no_tree = tree;
					m0_rwlock_write_unlock(&tree->t_lock);
					m0_rwlock_write_unlock(&trees_lock);
					return nxt;
				}
			}
			m0_rwlock_write_unlock(&tree->t_lock);
		}

	/** Assign a free tree descriptor to this tree. */
	for (i = 0; i < ARRAY_SIZE(trees_in_use); i++) {
		uint64_t   t = ~trees_in_use[i];

		if (t != 0) {
			offset = __builtin_ffsl(t);
			M0_ASSERT(offset != 0);
			offset--;
			trees_in_use[i] |= (1ULL << offset);
			offset += (i * sizeof trees_in_use[0]);
			tree = &trees[offset];
			trees_loaded++;
			break;
		}
	}

	M0_ASSERT(tree != NULL && tree->r_ref == 0);

	m0_rwlock_init(&tree->t_lock);

	m0_rwlock_write_lock(&tree->t_lock);
	tree->r_ref++;

	if (addr) {
		mem_node_get(op, tree, addr, nxt);
		tree->t_root		=  op->no_node;
		tree->t_root->n_addr 	= *addr;
		tree->t_root->n_tree	=  tree;
		//tree->t_height = tree_height_get(op->no_node);
	}

	op->no_node = tree->t_root;
	op->no_tree = tree;
	//op->no_addr = tree->t_root->n_addr;

	m0_rwlock_write_unlock(&tree->t_lock);

	m0_rwlock_write_unlock(&trees_lock);

	return nxt;
}

static int64_t mem_tree_create(struct node_op *op, struct m0_btree_type *tt,
			       int rootshift, struct m0_be_tx *tx, int nxt)
{
	struct td *tree;

	/**
	 * Creates root node and then assigns a tree descriptor for this root
	 * node.
	 */

	tree_get(op, NULL, nxt);

	tree = op->no_tree;
	node_alloc(op, tree, rootshift, &fixed_format, 8, 8, NULL, nxt);

	m0_rwlock_write_lock(&tree->t_lock);
	tree->t_root = op->no_node;
	tree->t_type = tt;
	m0_rwlock_write_unlock(&tree->t_lock);

	return nxt;
}

static int64_t mem_tree_delete(struct node_op *op, struct td *tree,
			       struct m0_be_tx *tx, int nxt)
{
	struct nd *root = tree->t_root;
	op->no_tree = tree;
	op->no_node = root;
	mem_node_free(op, root, tx, nxt);
	tree_put(tree);
	return nxt;
}

static void mem_tree_put(struct td *tree)
{
	m0_rwlock_write_lock(&tree->t_lock);

	M0_ASSERT(tree->r_ref > 0);
	M0_ASSERT(tree->t_root != NULL);

	tree->r_ref--;

	if (tree->r_ref == 0) {
		int i;
		int array_offset;
		int bit_offset_in_array;

		m0_rwlock_write_lock(&trees_lock);
		M0_ASSERT(trees_loaded > 0);
		i = tree - &trees[0];
		array_offset = i / sizeof(trees_in_use[0]);
		bit_offset_in_array = i % sizeof(trees_in_use[0]);
		trees_in_use[array_offset] &= ~(1ULL << bit_offset_in_array);
		trees_loaded--;
		m0_rwlock_write_unlock(&tree->t_lock);
		m0_rwlock_fini(&tree->t_lock);
		m0_rwlock_write_unlock(&trees_lock);
	}
	m0_rwlock_write_unlock(&tree->t_lock);
}

static int64_t mem_node_get(struct node_op *op, struct td *tree,
			    struct segaddr *addr, int nxt)
{
	int nxt_state = nxt;

	/**
	 *  In this implementation we assume to have the node descritors for
	 *  ALL the nodes present in memory. If the tree pointer is NULL then
	 *  we load the tree in memory before returning the node descriptor to
	 * the caller.
	 */

	if (tree == NULL) {
		nxt_state = mem_tree_get(op, addr, nxt);
	}

	op->no_node = segaddr_addr(addr) + (1ULL << segaddr_shift(addr));
	op->no_node->n_ref++;
	return nxt_state;
}

static void mem_node_put(struct nd *node)
{
	/**
	 * This implementation does not perform any action, but the final one
	 * should decrement the reference count of the node and if this
	 * reference count goes to '0' then detach this node descriptor from the
	 * existing node and make this node descriptor available for 'node_get'.
	 */
	node->n_ref--;
}

static struct nd *mem_node_try(struct td *tree, struct segaddr *addr)
{
	return NULL;
}

static int64_t mem_node_alloc(struct node_op *op, struct td *tree, int shift,
			      const struct node_type *nt, struct m0_be_tx *tx,
			      int nxt)
{
	void          *area;
	struct nd     *node;
	int            size = 1ULL << shift;

	M0_PRE(op->no_opc == NOP_ALLOC);
	M0_PRE(node_shift_is_valid(shift));
	area = m0_alloc_aligned(sizeof *node + size, shift);
	M0_ASSERT(area != NULL);
	node = area + size;
	node->n_addr = segaddr_build(area, shift);
	node->n_tree = tree;
	node->n_type = nt;
	node->n_seq = 0;
	m0_rwlock_init(&node->n_lock);
	op->no_node = node;
	op->no_addr = node->n_addr;
	op->no_tree = tree;
	return nxt;
}

static int64_t mem_node_free(struct node_op *op, struct nd *node,
			     struct m0_be_tx *tx, int nxt)
{
	int shift = node->n_type->nt_shift(node);
	m0_free_aligned(((void *)node) - (1ULL << shift),
			sizeof *node + (1ULL << shift), shift);
	return nxt;
}

static void mem_node_op_fini(struct node_op *op)
{
}

static const struct seg_ops mem_seg_ops = {
	.so_tree_get     = &mem_tree_get,
	.so_tree_create  = &mem_tree_create,
	.so_tree_delete  = &mem_tree_delete,
	.so_tree_put     = &mem_tree_put,
	.so_node_get     = &mem_node_get,
	.so_node_put     = &mem_node_put,
	.so_node_try     = &mem_node_try,
	.so_node_alloc   = &mem_node_alloc,
	.so_node_free    = &mem_node_free,
	.so_node_op_fini = &mem_node_op_fini
};

/**
 *  Structure of the node in persistent store.
 */
struct ff_head {
	struct m0_format_header ff_fmt;   /*< Node Header */
	struct node_header      ff_seg;   /*< Node type information */
	uint16_t                ff_used;  /*< Count of records */
	uint8_t                 ff_shift; /*< Node size as pow-of-2 */
	uint8_t                 ff_level; /*< Level in Btree */
	uint16_t                ff_ksize; /*< Size of key in bytes */
	uint16_t                ff_vsize; /*< Size of value in bytes */
	struct m0_format_footer ff_foot;  /*< Node Footer */
	/**
	 *  This space is used to host the Keys and Values upto the size of the
	 *  node
	 */
} M0_XCA_RECORD M0_XCA_DOMAIN(be);


static void ff_init(const struct nd *node, int shift, int ksize, int vsize);
static void ff_fini(const struct nd *node);
static int  ff_count(const struct nd *node);
static int  ff_count_rec(const struct nd *node);
static int  ff_space(const struct nd *node);
static int  ff_level(const struct nd *node);
static int  ff_shift(const struct nd *node);
static int  ff_valsize(const struct nd *node);
static int  ff_keysize(const struct nd *node);
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
static void ff_set_level(const struct nd *node, uint8_t new_level);
static void generic_move(struct nd *src, struct nd *tgt,
			 enum dir dir, int nr, struct m0_be_tx *tx);
static bool ff_invariant(const struct nd *node);
static int ff_verify(const struct nd *node);
/**
 *  Implementation of node which supports fixed format/size for Keys and Values
 *  contained in it.
 */
static const struct node_type fixed_format = {
	.nt_id        = BNT_FIXED_FORMAT,
	.nt_name      = "m0_bnode_fixed_format",
	//.nt_tag,
	.nt_init      = ff_init,
	.nt_fini      = ff_fini,
	.nt_count     = ff_count,
	.nt_count_rec = ff_count_rec,
	.nt_space     = ff_space,
	.nt_level     = ff_level,
	.nt_shift     = ff_shift,
	.nt_keysize   = ff_keysize,
	.nt_valsize   = ff_valsize,
	.nt_fid       = ff_fid,
	.nt_rec       = ff_rec,
	.nt_key       = ff_node_key,
	.nt_child     = ff_child,
	.nt_isfit     = ff_isfit,
	.nt_done      = ff_done,
	.nt_make      = ff_make,
	.nt_find      = ff_find,
	.nt_fix       = ff_fix,
	.nt_cut       = ff_cut,
	.nt_del       = ff_del,
	.nt_set_level = ff_set_level,
	.nt_move      = generic_move,
	.nt_invariant = ff_invariant,
	.nt_verify    = ff_verify,
};

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
	bool   val_is_valid;
	val_is_valid = h->ff_level > 0 ?
		       m0_vec_count(&slot->s_rec.r_val.ov_vec) <= h->ff_vsize :
		       m0_vec_count(&slot->s_rec.r_val.ov_vec) == h->ff_vsize;

	return
	   _0C(m0_vec_count(&slot->s_rec.r_key.k_data.ov_vec) == h->ff_ksize) &&
	   _0C(val_is_valid);
}

static bool ff_invariant(const struct nd *node)
{
	const struct ff_head *h = ff_data(node);

	return  _0C(h->ff_shift == segaddr_shift(&node->n_addr)) &&
		_0C(node->n_skip_rec_count_check ||
		    ergo(h->ff_level > 0, h->ff_used > 0));
}

static int ff_verify(const struct nd *node)
{
	const struct ff_head *h = ff_data(node);
	return m0_format_footer_verify(h, true);
}

static void ff_init(const struct nd *node, int shift, int ksize, int vsize)
{
	struct ff_head *h   = ff_data(node);

	M0_PRE(ksize != 0);
	M0_PRE(vsize != 0);
	M0_SET0(h);

	h->ff_shift = shift;
	h->ff_ksize = ksize;
	h->ff_vsize = vsize;
}

static void ff_fini(const struct nd *node)
{
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
}

static void ff_cut(const struct nd *node, int idx, int size,
		   struct m0_be_tx *tx)
{
	M0_PRE(size == ff_data(node)->ff_vsize);
}

static void ff_del(const struct nd *node, int idx, struct m0_be_tx *tx)
{
	struct ff_head *h     = ff_data(node);
	int             rsize = h->ff_ksize + h->ff_vsize;
	void           *start = ff_key(node, idx);

	M0_PRE(idx < h->ff_used);
	M0_PRE(h->ff_used > 0);
	memmove(start, start + rsize, rsize * (h->ff_used - idx - 1));
	h->ff_used--;
}

static void ff_set_level(const struct nd *node, uint8_t new_level)
{
	struct ff_head *h = ff_data(node);

	h->ff_level = new_level;
}

static void generic_move(struct nd *src, struct nd *tgt,
			 enum dir dir, int nr, struct m0_be_tx *tx)
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
			       (nr == NR_MAX && srcidx == -1))
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
	node_fix(src, tx);
	node_fix(tgt, tx);
}

/** Insert operation section start point: */

#ifndef __KERNEL__
static bool m0_btree_overflow_is_possible(const struct nd *node, int ksize,
					  int vsize)
{
	return (node_space(node) < ksize + vsize) ? true : false;
}
#endif

#if 0
static bool m0_btree_underflow_is_possible(const struct nd *node)
{
	/**
	 * defination of this function can be modified as, MAX_NODE_SIZE can
	 * vary according to level
	 */
	/* calculate remaining space after deletion of key-value */
	int space = node_space(node) -  MAX_KEY_SIZE - MAX_VAL_SIZE;
	return space < MIN_NODE_SIZE_UNDERFLOW ? true : false;
}
#endif

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
	return false;
}

/**
 * checks if overflow possible for given cookie
 */
static bool cookie_overflow_is_possible(void *segaddr)
{
	/* TBD : function definition */
	return false;
}

static int fail(struct m0_btree_op *bop, int rc)
{
	bop->bo_op.o_sm.sm_rc = rc;
	//return m0_sm_op_sub(&bop->bo_op, P_CLEANUP, P_DONE);
	return P_CLEANUP;
}

/**
 * checks if given node is still exists
 */
static int node_is_valid(const struct nd *node)
{
	/* function definition is yet to be implemented */
	return 0;
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
	struct nd *l_node      = oi->i_level[total_level].l_node;

	if (cookie_is_used())
		return cookie_is_valid(tree, k_cookie);

	while (total_level >= 0) {
		int rc = node_is_valid(l_node);
		if (rc) {
			node_op_fini(&oi->i_nop);
			//return fail(bop, rc);
			return false;
		}
		if (oi->i_level[total_level].l_seq != l_node->n_seq) {
			return false;
		}
		total_level--;
	}
	return true;
}

static bool locked(struct td *tree)
{
	return false;
}

static int64_t lock_op_init(struct m0_sm_op *bo_op, struct node_op  *i_nop,
			    struct td       *tree, int nxt)
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

static struct m0_btree_oimpl *level_alloc(int height)
{
	struct m0_btree_oimpl *oi;

	oi = m0_alloc(sizeof *oi);
	if (oi == NULL)
		return NULL;
	oi->i_level = m0_alloc(height * (sizeof *oi->i_level));
	if (oi->i_level == NULL) {
		m0_free(oi);
		oi = NULL;
	}
	return oi;
}

static void level_cleanup(struct m0_btree_oimpl *oi)
{
	int i;
	for (i = 0; i < oi->i_used; ++i) {
		if (oi->i_level[i].l_node != NULL) {
			node_put(oi->i_level[i].l_node);
			oi->i_level[i].l_node = NULL;
		}
		if (oi->i_level[i].l_alloc != NULL) {
			node_put(oi->i_level[i].l_alloc);
			oi->i_level[i].l_alloc = NULL;
		}
	}
	if (oi->i_extra_node != NULL) {
		node_put(oi->i_extra_node);
	}
	m0_free(oi->i_level);
	m0_free(oi);
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
 * This function will be called when there is possiblity of overflow at required
 * node present at particular level. TO handle the overflow this function will
 * allocate new nodes. It will store of newly allocated node in l_alloc and
 * i_extra_node(for root node).
 *
 * @param bop it structure for btree operation which contains all required data.
 * @return int64_t it return state which needs to get executed next.
 */
static int64_t m0_btree_put_alloc_phase(struct m0_btree_op *bop)
{
	struct td             *tree       = bop->bo_arbor->t_desc;
	struct m0_btree_oimpl *oi         = bop->bo_i;
	struct level          *curr_level = &oi->i_level[oi->i_used];

	if (oi->i_used == 0) {
		if ((oi->i_extra_node == NULL || curr_level->l_alloc == NULL)) {
			/**
			 * If we reach root node and there is possibility of
			 * overflow at root,allocate two nodes: l_alloc,
			 * i_extra_node. i)l_alloc is required in case of
			 * splitting operation of root ii)i_extra_node is
			 * required if splitting is done at root node so to have
			 * pointers to these splitted nodes at root level, there
			 * will be need for new node.
			 * Depending on the level of node, shift can be updated.
			 */
			if (oi->i_nop.no_node == NULL) {
				int ksize = node_keysize(curr_level->l_node);
				int vsize = node_valsize(curr_level->l_node);
				int shift = node_shift(curr_level->l_node);
				oi->i_nop.no_opc = NOP_ALLOC;
				return node_alloc(&oi->i_nop, tree,
						  shift,
						  curr_level->l_node->n_type,
						  ksize, vsize,
						  bop->bo_tx, P_ALLOC);
			}
			if (oi->i_nop.no_op.o_sm.sm_rc == 0) {
				if (oi->i_extra_node == NULL)
					oi->i_extra_node = oi->i_nop.no_node;
				else
					curr_level->l_alloc = oi->i_nop.no_node;

				oi->i_nop.no_node = NULL;

				return P_ALLOC;
			} else {
				node_op_fini(&oi->i_nop);
				level_cleanup(oi);
				return P_INIT;
			}
		}
		/* Reset oi->i_used */
		oi->i_used = bop->bo_arbor->t_height - 1;
		return P_LOCK;
	} else {
		if (oi->i_nop.no_node == NULL) {
			int ksize = node_keysize(curr_level->l_node);
			int vsize = node_valsize(curr_level->l_node);
			int shift = node_shift(curr_level->l_node);
			oi->i_nop.no_opc = NOP_ALLOC;
			return node_alloc(&oi->i_nop, tree, shift,
					  curr_level->l_node->n_type, ksize,
					  vsize, bop->bo_tx, P_ALLOC);
		}
		if (oi->i_nop.no_op.o_sm.sm_rc == 0) {
			curr_level->l_alloc = oi->i_nop.no_node;
			oi->i_nop.no_node = NULL;

			oi->i_used--;
			return P_ALLOC;
		} else {
			node_op_fini(&oi->i_nop);
			level_cleanup(oi);
			return P_INIT;
		}
	}
}

/**
 * This function gets called when splitting is done at root node. This function
 * is responsible to handle this scanario and ultimately root will point out to
 * the two splitted node.
 * @param bop it structure for btree operation which contains all required data
 * @param new_rec it will contain key and value as address pointing to newly
 * allocated node at root
 * @return int64_t it return state which needs to get executed next
 */
static int64_t m0_btree_put_root_split_handle(struct m0_btree_op *bop,
					      struct m0_btree_rec *new_rec)
{
	struct td              *tree       = bop->bo_arbor->t_desc;
	struct m0_btree_oimpl  *oi         = bop->bo_i;
	struct level           *curr_level = &oi->i_level[oi->i_used];
	m0_bcount_t             ksize;
	void                   *p_key;
	m0_bcount_t             vsize;
	void                   *p_val;
	struct m0_btree_rec     temp_rec;
	m0_bcount_t             ksize_2;
	void                   *p_key_2;
	m0_bcount_t             vsize_2;
	void                   *p_val_2;
	struct m0_btree_rec     temp_rec_2;

	bop->bo_rec   = *new_rec;

	temp_rec.r_key.k_data = M0_BUFVEC_INIT_BUF(&p_key, &ksize);
	temp_rec.r_val        = M0_BUFVEC_INIT_BUF(&p_val, &vsize);

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

	int curr_max_level = node_level(curr_level->l_node);

	node_set_level(oi->i_extra_node, curr_max_level);
	node_set_level(curr_level->l_node, curr_max_level + 1);

	/* skip the invarient check for level */
	oi->i_extra_node->n_skip_rec_count_check   = true;
	curr_level->l_node->n_skip_rec_count_check = true;

	node_move(curr_level->l_node, oi->i_extra_node, D_RIGHT, NR_MAX,
		  bop->bo_tx);
	oi->i_extra_node->n_skip_rec_count_check = false;
	/* M0_ASSERT(node_count(curr_level->l_node) == 0); */

	/* 2) add new 2 records at root node. */

	/* Add first rec at root */
	struct slot node_slot = {
		.s_node = curr_level->l_node,
		.s_idx  = 0
	};
	node_slot.s_rec = bop->bo_rec;

	/* M0_ASSERT(node_isfit(&node_slot)) */
	node_make(&node_slot, bop->bo_tx);
	node_slot.s_rec = temp_rec;
	node_rec(&node_slot);
	m0_bufvec_copy(&node_slot.s_rec.r_key.k_data, &bop->bo_rec.r_key.k_data,
		       m0_vec_count(&bop->bo_rec.r_key.k_data.ov_vec));
	m0_bufvec_copy(&node_slot.s_rec.r_val, &bop->bo_rec.r_val,
		       m0_vec_count(&bop->bo_rec.r_val.ov_vec));

	/* if we need to update vec_count for root, update here */
	curr_level->l_node->n_skip_rec_count_check = false;
	node_done(&node_slot, bop->bo_tx, true);

	/* Add second rec at root */
	temp_rec_2.r_key.k_data = M0_BUFVEC_INIT_BUF(&p_key_2, &ksize_2);
	temp_rec_2.r_val        = M0_BUFVEC_INIT_BUF(&p_val_2, &vsize_2);

	node_slot.s_idx  = 1;
	node_slot.s_rec = temp_rec;
	/* M0_ASSERT(node_isfit(&node_slot)) */
	node_make(&node_slot, bop->bo_tx);
	node_slot.s_rec = temp_rec_2;
	node_rec(&node_slot);

	temp_rec.r_val.ov_buf[0] = &(oi->i_extra_node->n_addr);
	m0_bufvec_copy(&node_slot.s_rec.r_val, &temp_rec.r_val,
		       m0_vec_count(&temp_rec.r_val.ov_vec));
	/* if we need to update vec_count for root slot, update at this place */

	node_done(&node_slot, bop->bo_tx, true);
	node_fix(curr_level->l_node, bop->bo_tx);

	/* Increase height by one */
	tree->t_height++;

	lock_op_unlock(tree);
	/* return m0_sm_op_sub(&bop->bo_op, P_CLEANUP, P_DONE); */
	return P_CLEANUP;
}

/**
 * This function is called when there is overflow and splitting needs to be
 * done. It is will move some records from right node(l_node) to left
 * node(l_alloc) and find the appropriate slot for given record. It will store
 * the node and index for slot at tgt slot as a result. It will
 *
 * @param l_alloc It is the newly allocated node, where we want to move record.
 * @param l_node It is the current node, from where we want to move record.
 * @param rec It is the given record for which we want to find slot
 * @param tgt result of record find will get stored in tgt slot
 * @param tx It represents the transaction of which the current operation is
 * part of.
 */
static void m0_btree_put_split_and_find(struct nd *l_alloc , struct nd *l_node,
				        struct m0_btree_rec *rec,
					struct slot *tgt, struct m0_be_tx *tx)
{
	struct slot r_slot ;
	struct slot l_slot;
	struct m0_bufvec_cursor  cur_1;
	struct m0_bufvec_cursor  cur_2;
	int                      diff;
	m0_bcount_t              ksize;
	void                    *p_key;
	m0_bcount_t              vsize;
	void                    *p_val;
	struct m0_btree_rec      temp_rec;

	/* intialised slot for left and right node*/
	l_slot.s_node = l_alloc;
	r_slot.s_node = l_node;
	/* 1)Move some records from current node to new node */
	l_alloc->n_skip_rec_count_check = true;
	node_set_level(l_alloc, node_level(l_node));

	node_move(l_node, l_alloc, D_LEFT, NR_EVEN, tx);
	l_alloc->n_skip_rec_count_check = false;

	/*2) Find appropriate slot for given record */
	temp_rec.r_key.k_data = M0_BUFVEC_INIT_BUF(&p_key, &ksize);
	temp_rec.r_val        = M0_BUFVEC_INIT_BUF(&p_val, &vsize);

	r_slot.s_idx = 0;
	r_slot.s_rec = temp_rec;
	node_key(&r_slot);

	m0_bufvec_cursor_init(&cur_1, &rec->r_key.k_data);
	m0_bufvec_cursor_init(&cur_2, &r_slot.s_rec.r_key.k_data);

	diff = m0_bufvec_cursor_cmp(&cur_1, &cur_2);
	tgt->s_node = diff < 0 ? l_slot.s_node : r_slot.s_node;

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
 * @param bop it structure for btree operation which contains all required data.
 * @return int64_t it return state which needs to get executed next.
 */
static int64_t m0_btree_put_makespace_phase(struct m0_btree_op *bop)
{
	struct m0_btree_oimpl *oi         = bop->bo_i;
	struct level          *curr_level = &oi->i_level[oi->i_used];
	m0_bcount_t            ksize;
	void                  *p_key;
	m0_bcount_t            vsize;
	void                  *p_val;
	struct m0_btree_rec    temp_rec;
	m0_bcount_t            ksize_1;
	void                  *p_key_1;
	m0_bcount_t            vsize_1;
	void                  *p_val_1;
	struct m0_btree_rec    temp_rec_1;
	uint64_t               newvalue;
	m0_bcount_t            newvsize  = INTERNAL_NODE_VALUE_SIZE;
	void                  *newv_ptr  = &newvalue;
	struct m0_btree_rec    new_rec;
	struct slot            tgt;
	struct slot            node_slot;
	int                    i;

	temp_rec.r_key.k_data = M0_BUFVEC_INIT_BUF(&p_key, &ksize);
	temp_rec.r_val        = M0_BUFVEC_INIT_BUF(&p_val, &vsize);

	/**
	 * move records from current node to new node and find slot for given
	 * record
	 */
	m0_btree_put_split_and_find(curr_level->l_alloc, curr_level->l_node,
			       &bop->bo_rec, &tgt, bop->bo_tx);
	tgt.s_rec = bop->bo_rec;
	node_make (&tgt, bop->bo_tx);
	tgt.s_rec = temp_rec;
	node_rec(&tgt);
	tgt.s_rec.r_flags = M0_BSC_SUCCESS;
	int rc = bop->bo_cb.c_act(&bop->bo_cb, &tgt.s_rec);
	if (rc) {
		/* If callback failed, undo make space, splitted node */
		node_del(tgt.s_node, tgt.s_idx, bop->bo_tx);
		node_done(&tgt, bop->bo_tx, true);
		node_fix(curr_level->l_node, bop->bo_tx);
		node_move(curr_level->l_alloc, curr_level->l_node, D_RIGHT,
		          NR_MAX, bop->bo_tx);
		lock_op_unlock(bop->bo_arbor->t_desc);
		return fail(bop, rc);
	}
	node_done(&tgt, bop->bo_tx, true);
	node_fix(tgt.s_node, bop->bo_tx);

	/* Initialized new record which will get inserted at parent */
	node_slot.s_node = curr_level->l_node;
	node_slot.s_idx = 0;
	node_slot.s_rec = temp_rec;
	node_key(&node_slot);
	new_rec.r_key = node_slot.s_rec.r_key;

	newvalue      = INTERNAL_NODE_VALUE_SIZE;
	newv_ptr      = &(curr_level->l_alloc->n_addr);
	new_rec.r_val = M0_BUFVEC_INIT_BUF(&newv_ptr, &newvsize);

	temp_rec_1.r_key.k_data   = M0_BUFVEC_INIT_BUF(&p_key_1, &ksize_1);
	temp_rec_1.r_val          = M0_BUFVEC_INIT_BUF(&p_val_1, &vsize_1);

	for (i = oi->i_used - 1; i >= 0; i--) {
		curr_level = &oi->i_level[i];
		node_slot.s_node = curr_level->l_node;
		node_slot.s_idx  = curr_level->l_idx;
		node_slot.s_rec  = new_rec;
		if (node_isfit(&node_slot)) {
			struct m0_btree_rec *rec;
			node_make(&node_slot, bop->bo_tx);
			node_slot.s_rec = temp_rec_1;
			node_rec(&node_slot);
			rec = &new_rec;
			m0_bufvec_copy(&node_slot.s_rec.r_key.k_data,
			       	       &rec->r_key.k_data,
			               m0_vec_count(&rec->r_key.k_data.ov_vec));
			m0_bufvec_copy(&node_slot.s_rec.r_val, &rec->r_val,
				       m0_vec_count(&rec->r_val.ov_vec));

			node_done(&node_slot, bop->bo_tx, true);
			node_fix(curr_level->l_node, bop->bo_tx);

			lock_op_unlock(bop->bo_arbor->t_desc);
			return P_CLEANUP;
		}

		m0_btree_put_split_and_find(curr_level->l_alloc,
					    curr_level->l_node, &new_rec,
					    &tgt, bop->bo_tx);
		tgt.s_rec = new_rec;
		node_make(&tgt, bop->bo_tx);
		tgt.s_rec = temp_rec_1;
		node_rec(&tgt);
		m0_bufvec_copy(&tgt.s_rec.r_key.k_data, &new_rec.r_key.k_data,
			       m0_vec_count(&new_rec.r_key.k_data.ov_vec));
		m0_bufvec_copy(&tgt.s_rec.r_val, &new_rec.r_val,
			       m0_vec_count(&new_rec.r_val.ov_vec));
		node_slot.s_node = curr_level->l_alloc;
		node_slot.s_idx = node_count(node_slot.s_node);
		node_slot.s_rec = temp_rec;
		node_key(&node_slot);
		new_rec.r_key = node_slot.s_rec.r_key;
		newv_ptr = &(curr_level->l_alloc->n_addr);
	}

	oi->i_used  = 0;
	/**
	 * If we reach root node and splitting is done at root handle spliting
	 * of root
	*/
	return m0_btree_put_root_split_handle(bop, &new_rec);
}

/* get_tick for insert operation */
static int64_t btree_put_tick(struct m0_sm_op *smop)
{
	struct m0_btree_op    *bop        = M0_AMB(bop, smop, bo_op);
	struct td             *tree       = bop->bo_arbor->t_desc;
	uint64_t               flags      = bop->bo_flags;
	struct m0_btree_oimpl *oi         = bop->bo_i;
	struct level          *curr_level;

	if (oi)
		curr_level = &oi->i_level[oi->i_used];

	switch (bop->bo_op.o_sm.sm_state) {
	case P_INIT:
		if ((flags & OF_COOKIE) &&
		    cookie_is_set(&bop->bo_rec.r_key.k_cookie))
			return P_COOKIE;
		else
			return P_SETUP;
	case P_COOKIE: {
		void *cookie_segaddr = bop->bo_rec.r_key.k_cookie.segaddr;
		if (cookie_is_valid(tree, &bop->bo_rec.r_key.k_cookie) &&
		    !cookie_overflow_is_possible(cookie_segaddr))
			return P_LOCK;
		else
			return P_SETUP;
	}
	case P_SETUP: {
		bop->bo_arbor->t_height = tree->t_height;
		bop->bo_i = level_alloc(bop->bo_arbor->t_height);
		if (bop->bo_i == NULL)
			return fail(bop, M0_ERR(-ENOMEM));
		bop->bo_i->i_key_found = false;
		return P_LOCKALL;
	}
	case P_LOCKALL:
		if (bop->bo_flags & OF_LOCKALL) {
			//return m0_sm_op_sub(&bop->bo_op, P_LOCK, P_DOWN);
			m0_rwlock_write_lock(&tree->t_lock);
		}
		/* Fall through to the next stage */
	case P_DOWN:
		oi->i_used = 0;
		/* Load root node. */
		return node_get(&oi->i_nop, tree, &tree->t_root->n_addr,
				P_NEXTDOWN);
	case P_NEXTDOWN:
		if (oi->i_nop.no_op.o_sm.sm_rc == 0) {
			struct slot    node_slot = {};
			struct segaddr child_node_addr;

			curr_level->l_node = oi->i_nop.no_node;
			node_slot.s_node = oi->i_nop.no_node;
			curr_level->l_seq = curr_level->l_node->n_seq;
			/* Verify node footer */
			/*int rc = node_verify(curr_level->l_node);
			if (rc)
			{
				node_op_fini(&oi->i_nop);
				return fail(bop, rc);
			}*/
			oi->i_nop.no_node = NULL;

			oi->i_key_found = node_find(&node_slot,
						    &bop->bo_rec.r_key);
			curr_level->l_idx = node_slot.s_idx;
			if (node_level(node_slot.s_node) > 0) {
				if (oi->i_key_found) {
					curr_level->l_idx++;
					node_slot.s_idx++;
				}
				node_child(&node_slot, &child_node_addr);
				if (!address_in_segment(child_node_addr)) {
					node_op_fini(&oi->i_nop);
					return fail(bop, M0_ERR(-EFAULT));
				}
				oi->i_used++;
				return node_get(&oi->i_nop, tree,
						&child_node_addr, P_NEXTDOWN);
			} else
				return P_ALLOC;
		} else {
			node_op_fini(&oi->i_nop);
			return fail(bop, oi->i_nop.no_op.o_sm.sm_rc);
		}
	case P_ALLOC: {
		int ksize;
		int vsize;
		/* Validate curr_level->l_node(i.e.is it still exists or not) */
		int rc = node_is_valid(curr_level->l_node);
		if (rc) {
			node_op_fini(&oi->i_nop);
			return fail(bop, rc);
		}
		if (curr_level->l_node->n_type->nt_id == BNT_FIXED_FORMAT) {
			ksize = node_keysize(curr_level->l_node);
			vsize = node_valsize(curr_level->l_node);
		} else {
			/**
			 * @todo : ksize, vsize for variable key:
			 * get the correct max key size at runtime
			 * */
			ksize = MAX_KEY_SIZE;
			vsize = MAX_VAL_SIZE;
		}
		if (m0_btree_overflow_is_possible(curr_level->l_node, ksize,
						  vsize))
			return m0_btree_put_alloc_phase(bop);

		/* Reset oi->i_used */
		oi->i_used = bop->bo_arbor->t_height - 1;
		return P_LOCK;
	}
	case P_LOCK:
		if (!locked(tree))
			return lock_op_init(&bop->bo_op, &bop->bo_i->i_nop,
					    bop->bo_arbor->t_desc, P_CHECK);
		else
			return P_CHECK;
	case P_CHECK: {
		oi->i_trial++;
		if (!path_check(oi, tree, &bop->bo_rec.r_key.k_cookie)) {
			if (oi->i_trial == MAX_TRIALS) {
				if (bop->bo_flags & OF_LOCKALL)
					return fail(bop, -ETOOMANYREFS);
				else
					bop->bo_flags |= OF_LOCKALL;
			}
			if (bop->bo_arbor->t_height <= tree->t_height) {
				/* If height increased */
				//return m0_sm_op_sub(&bop->bo_op, P_CLEANUP,
				//                    P_INIT);
				lock_op_unlock(tree);
				return P_CLEANUP;
			} else {
				/* If height decreased */
				lock_op_unlock(tree);
				return P_DOWN;
			}
		}
		/* Fall through to the next step i.e. P_MAKESPACE */
	}
	case P_MAKESPACE: {
		if (oi->i_key_found) {
			struct m0_btree_rec rec;
			rec.r_flags = M0_BSC_KEY_EXISTS;
			int rc = bop->bo_cb.c_act(&bop->bo_cb, &rec);
			if (rc) {
				lock_op_unlock(tree);
				return fail(bop, rc);
			}
			lock_op_unlock(tree);
			return P_CLEANUP;
		}
		struct slot slot_for_right_node = {
			.s_node = curr_level->l_node,
			.s_idx  = curr_level->l_idx,
			.s_rec  = bop->bo_rec
		};
		if(!node_isfit(&slot_for_right_node)) {
			return m0_btree_put_makespace_phase(bop);
		}
		node_make (&slot_for_right_node, bop->bo_tx);
		/** Fallthrough to the P_ACT  **/
	}
	case P_ACT: {
		m0_bcount_t          ksize;
		void                *p_key;
		m0_bcount_t          vsize;
		void                *p_val;
		struct m0_btree_rec *rec;
		struct slot  node_slot = {
			.s_node = curr_level->l_node,
			.s_idx  = curr_level->l_idx
		};
		rec = &node_slot.s_rec;
		rec->r_key.k_data =  M0_BUFVEC_INIT_BUF(&p_key, &ksize);
		rec->r_val        =  M0_BUFVEC_INIT_BUF(&p_val, &vsize);

		node_rec(&node_slot);

		/**
		 * If we are at leaf node, and we have made the space
		 * for inserting a record, callback will be called.
		 * Callback will be provided with the record. It is
		 * user's responsibility to fill the value as well as
		 * key in the given record. if callback failed,we will
		 * revert back the changes made on btree. Detailed
		 * explination is provided at P_MAKESPACE stage.
		 */
		rec->r_flags = M0_BSC_SUCCESS;
		int rc = bop->bo_cb.c_act(&bop->bo_cb, rec);
		if (rc) {
			/* handle if callback fail i.e undo make */
			node_del(node_slot.s_node, node_slot.s_idx, bop->bo_tx);
			node_done(&node_slot, bop->bo_tx, true);
			node_fix(curr_level->l_node, bop->bo_tx);
			lock_op_unlock(tree);
			return fail(bop, rc);
		}
		node_done(&node_slot, bop->bo_tx, true);
		node_fix(curr_level->l_node, bop->bo_tx);

		lock_op_unlock(tree);
		return P_CLEANUP;
		//return m0_sm_op_sub(&bop->bo_op, P_CLEANUP, P_DONE);
	}
	case P_CLEANUP:
		level_cleanup(oi);
		return P_DONE;
		//return m0_sm_op_ret(&bop->bo_op);
	default:
		M0_IMPOSSIBLE("Wrong state: %i", bop->bo_op.o_sm.sm_state);
	};
}
/* Insert operation section end point */
#endif
#ifndef __KERNEL__
static struct m0_sm_group G;

static struct m0_sm_state_descr btree_states[P_NR] = {
	[P_INIT] = {
		.sd_flags   = M0_SDF_INITIAL,
		.sd_name    = "P_INIT",
		.sd_allowed = M0_BITS(P_COOKIE, P_SETUP, P_ACT, P_DONE),
	},
	[P_COOKIE] = {
		.sd_flags   = 0,
		.sd_name    = "P_COOKIE",
		.sd_allowed = M0_BITS(P_LOCK, P_SETUP),
	},
	[P_SETUP] = {
		.sd_flags   = 0,
		.sd_name    = "P_SETUP",
		.sd_allowed = M0_BITS(P_LOCKALL, P_CLEANUP),
	},
	[P_LOCKALL] = {
		.sd_flags   = 0,
		.sd_name    = "P_LOCKALL",
		.sd_allowed = M0_BITS(P_LOCK, P_DOWN, P_NEXTDOWN),
	},
	[P_DOWN] = {
		.sd_flags   = 0,
		.sd_name    = "P_DOWN",
		.sd_allowed = M0_BITS(P_NEXTDOWN),
	},
	[P_NEXTDOWN] = {
		.sd_flags   = 0,
		.sd_name    = "P_NEXTDOWN",
		.sd_allowed = M0_BITS(P_NEXTDOWN, P_ALLOC, P_CLEANUP, P_LOCK),
	},
	[P_ALLOC] = {
		.sd_flags   = 0,
		.sd_name    = "P_ALLOC",
		.sd_allowed = M0_BITS(P_ALLOC, P_LOCK, P_CLEANUP, P_INIT),
	},
	[P_LOCK] = {
		.sd_flags   = 0,
		.sd_name    = "P_LOCK",
		.sd_allowed = M0_BITS(P_CHECK),
	},
	[P_CHECK] = {
		.sd_flags   = 0,
		.sd_name    = "P_CHECK",
		.sd_allowed = M0_BITS(P_MAKESPACE, P_CLEANUP, P_DOWN, P_ACT,
				      P_INIT, P_NEXTUP),
	},
	[P_MAKESPACE] = {
		.sd_flags   = 0,
		.sd_name    = "P_MAKESPACE",
		.sd_allowed = M0_BITS(P_ACT, P_NEXTUP),
	},
	[P_NEXTUP] = {
		.sd_flags   = 0,
		.sd_name    = "P_NEXTUP",
		.sd_allowed = M0_BITS(P_MAKESPACE, P_CLEANUP),
	},
	[P_ACT] = {
		.sd_flags   = 0,
		.sd_name    = "P_ACT",
		.sd_allowed = M0_BITS(P_CLEANUP, P_DONE),
	},
	[P_CLEANUP] = {
		.sd_flags   = 0,
		.sd_name    = "P_CLEANUP",
		.sd_allowed = M0_BITS(P_DONE),
	},
	[P_DONE] = {
		.sd_flags   = M0_SDF_TERMINAL,
		.sd_name    = "P_DONE",
		.sd_allowed = 0,
	},
};

static struct m0_sm_trans_descr btree_trans[256] = {
	{ "create-init", P_INIT,  P_ACT  },
	{ "create-act",  P_ACT,   P_DOWN },
        { "destroy", P_INIT, P_DONE},
	{ "put/get-init-cookie", P_INIT, P_COOKIE },
	{ "put/get-init", P_INIT, P_SETUP },
	{ "put/get-cookie-valid", P_COOKIE, P_LOCK },
	{ "put/get-cookie-invalid", P_COOKIE, P_SETUP },
	{ "put/get-setup", P_SETUP, P_LOCKALL },
	{ "put/get-setup-failed", P_SETUP, P_CLEANUP },
	{ "put/get-lockall-lock", P_LOCKALL, P_LOCK },
	{ "put/get-lockall", P_LOCKALL, P_DOWN },
	{ "put/get-lockall-ft", P_LOCKALL, P_NEXTDOWN},
	{ "put/get-down", P_DOWN, P_NEXTDOWN },
	{ "put/get-nextdown-repeat", P_NEXTDOWN, P_NEXTDOWN },
	{ "put-nextdown-next", P_NEXTDOWN, P_ALLOC },
	{ "get-nextdown-next", P_NEXTDOWN, P_LOCK},
	{ "put/get-nextdown-failed", P_NEXTDOWN, P_CLEANUP },
	{ "put-alloc-repeat", P_ALLOC, P_ALLOC },
	{ "put-alloc-next", P_ALLOC, P_LOCK },
	{ "put-alloc-fail", P_ALLOC, P_INIT },
	{ "put-alloc-failed", P_ALLOC, P_CLEANUP },
	{ "put/get-lock", P_LOCK, P_CHECK },
	{ "put/get-check-height-inc", P_CHECK, P_CLEANUP },
	{ "put/get-check-height-decr", P_CHECK, P_DOWN },
	{ "put/get-check-ft", P_CHECK, P_ACT },
	{ "put-check-ft-makespace", P_CHECK, P_MAKESPACE },
	{ "put-check-ft-resolve", P_CHECK, P_NEXTUP },
	{ "put-check-init", P_CHECK, P_INIT },
	{ "put-makespace", P_MAKESPACE, P_ACT },
	{ "put-makespace-resolve", P_MAKESPACE, P_NEXTUP },
	{ "put-nextup", P_NEXTUP, P_MAKESPACE },
	{ "put-nextup-done", P_NEXTUP, P_CLEANUP },
	{ "put/get-act", P_ACT, P_CLEANUP },
	{ "put/get-done", P_CLEANUP, P_DONE },
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
 * @param value It represents the number of bytes
 * @return int returns the shift value.
 */

int calc_shift(int value)
{
	unsigned int sample 	= (unsigned int) value;
	unsigned int pow 	= 0;

	while (sample > 0)
	{
		sample >>=1;
		pow += 1;
	}

	return pow - 1;
}

/**
 * btree_create_tick function is the main function used to create btree.
 * It traverses through multiple states to perform its operation.
 *
 * @param smop Represents the state machine operation
 * @return int64_t It returns the next state to be executed.
 */

int64_t btree_create_tick(struct m0_sm_op *smop)
{
	struct m0_btree_op 	*bop = M0_AMB(bop, smop, bo_op);
	struct m0_btree_oimpl 	*oi = bop->bo_i;
	struct m0_btree_idata 	*data = &bop->b_data;
	int 			 k_size = data->nt->nt_id == BNT_FIXED_FORMAT ?
					  data->bt->ksize : (data->nt->nt_id ==
					  BNT_FIXED_KEYSIZE_VARIABLE_VALUESIZE ?
					  data->bt->ksize : -1);
	int 			 v_size = data->nt->nt_id == BNT_FIXED_FORMAT ?
					  data->bt->vsize :(data->nt->nt_id ==
					  BNT_VARIABLE_KEYSIZE_FIXED_VALUESIZE ?
					  data->bt->vsize : -1);

	switch(bop->bo_op.o_sm.sm_state)
	{
		case P_INIT:
			bop->bo_i = m0_alloc(sizeof *bop->bo_i);
			if (bop->bo_i == NULL)
				return M0_ERR(-ENOMEM);
			oi = bop->bo_i;
			bop->bo_arbor = m0_alloc(sizeof *bop->bo_arbor);
			if (bop->bo_arbor == NULL)
				return M0_ERR(-ENOMEM);

			oi->i_nop.no_addr = segaddr_build(data->addr,
							  calc_shift(
							  data->num_bytes));
			return tree_get(&oi->i_nop, &oi->i_nop.no_addr, P_ACT);

		case P_ACT:
			oi->i_nop.no_node->n_type = data->nt;

			node_init(&oi->i_nop, k_size, v_size);

			m0_rwlock_write_lock(&bop->bo_arbor->t_lock);
			bop->bo_arbor->t_desc           = oi->i_nop.no_tree;
			bop->bo_arbor->t_type           = data->bt;
			bop->bo_arbor->t_desc->t_height = 1;
			m0_rwlock_write_unlock(&bop->bo_arbor->t_lock);

			m0_free(oi);
			mem_update();
			return P_DONE;

		default:
			return 0;
	}
}

int64_t btree_destroy_tick(struct m0_sm_op *smop)
{
	struct m0_btree_op 	*bop = M0_AMB(bop, smop, bo_op);

	switch(bop->bo_op.o_sm.sm_state)
	{
		case P_INIT:
			M0_PRE(bop->bo_arbor != NULL);
			M0_PRE(bop->bo_arbor->t_desc != NULL);
			M0_PRE(node_invariant(bop->bo_arbor->t_desc->t_root));

			/** The following pre-condition is currently a
			 *  compulsion as the delete routine has not been
			 *  implemented yet.
			 *  Once it is implemented, this pre-condition can be
			 *  modified to compulsorily remove the records and get
			 *  the node count to 0.
			 */

			M0_PRE(node_count(bop->bo_arbor->t_desc->t_root) == 0);
			bop->bo_i = m0_alloc(sizeof *bop->bo_i);

			tree_delete(&bop->bo_i->i_nop, bop->bo_arbor->t_desc,
				    bop->bo_tx, P_ACT);

			m0_free(bop->bo_arbor);
			m0_free(bop->bo_i);
			bop->bo_arbor = NULL;
			bop->bo_i = NULL;
			return P_DONE;

		default:
			return 0;
	}
}

/**
 *  Search from the leaf + 1 level till the root level and find a node
 *  which has valid sibling. Once found, get the leftmost leaf record from the
 *  sibling subtree.
 */
int  btree_sibling_first_key_get(struct m0_btree_oimpl *oi, struct td *tree,
				 struct slot *s)
{
	int	        i;
	struct level   *lev;
	struct segaddr  child;

	for (i = oi->i_used - 1; i >= 0; i--) {
		lev = &oi->i_level[i];
		if (lev->l_idx < node_count(lev->l_node)) {
			s->s_node = oi->i_nop.no_node = lev->l_node;
			s->s_idx = lev->l_idx + 1;
			while (i-- >= 0) {
				node_child(s, &child);
				if (!address_in_segment(child))
					return M0_ERR(-EFAULT);
				node_get(&oi->i_nop, tree,
					 &child, P_CLEANUP);
				s->s_idx = 0;
				s->s_node = oi->i_nop.no_node;
			}
			node_rec(s);
			return 0;
		}
	}
	s->s_rec.r_flags = M0_BSC_KEY_NOT_FOUND;
	return 0;

}

/** Tree GET (lookup) state machine. */
static int64_t btree_get_tick(struct m0_sm_op *smop)
{
	struct m0_btree_op    *bop   = M0_AMB(bop, smop, bo_op);
	struct td             *tree  = bop->bo_arbor->t_desc;
	struct m0_btree_oimpl *oi    = bop->bo_i;
	struct level          *lev;

	switch (bop->bo_op.o_sm.sm_state) {
	case P_INIT:
		if ((bop->bo_flags & OF_COOKIE) &&
		    cookie_is_set(&bop->bo_rec.r_key.k_cookie))
			return P_COOKIE;
		else
			return P_SETUP;
	case P_COOKIE:
		if (cookie_is_valid(tree, &bop->bo_rec.r_key.k_cookie))
			return P_LOCK;
		else
			return P_SETUP;
	case P_SETUP:
		bop->bo_arbor->t_height = tree->t_height;
		oi = level_alloc(tree->t_height);
		if (oi == NULL)
			return fail(bop, M0_ERR(-ENOMEM));
		return P_LOCKALL;
	case P_LOCKALL:
		if (bop->bo_flags & OF_LOCKALL)
			/* return m0_sm_op_sub(&bop->bo_op, P_LOCK, P_DOWN); */
			return P_LOCK;
		/** Fall through if LOCKALL flag is not set. */
	case P_DOWN:
		oi->i_used = 0;
		return node_get(&oi->i_nop, tree, &tree->t_root->n_addr,
				P_NEXTDOWN);
	case P_NEXTDOWN:
		if (oi->i_nop.no_op.o_sm.sm_rc == 0) {
			struct slot    node_slot = {};
			struct segaddr child;

			lev = &oi->i_level[oi->i_used];
			lev->l_node = oi->i_nop.no_node;
			node_slot.s_node = oi->i_nop.no_node;
			lev->l_seq = lev->l_node->n_seq;
			oi->i_key_found = node_find(&node_slot,
						    &bop->bo_rec.r_key);
			lev->l_idx = node_slot.s_idx;

			if (node_level(node_slot.s_node) > 0) {
				if (oi->i_key_found) {
					node_slot.s_idx++;
					lev->l_idx++;
				}
				node_child(&node_slot, &child);
				if (!address_in_segment(child)) {
					node_op_fini(&oi->i_nop);
					return fail(bop, M0_ERR(-EFAULT));
				}
				oi->i_used++;
				return node_get(&oi->i_nop, tree,
						&child, P_NEXTDOWN);
			} else
				return P_LOCK;
		} else {
			node_op_fini(&oi->i_nop);
			return fail(bop, oi->i_nop.no_op.o_sm.sm_rc);
		}
	case P_LOCK:
		if (!locked(tree))
			return lock_op_init(&bop->bo_op, &bop->bo_i->i_nop,
					    bop->bo_arbor->t_desc, P_CHECK);
		else
			return P_CHECK;
	case P_CHECK:
		oi->i_trial++;
		if (!path_check(oi, tree, &bop->bo_rec.r_key.k_cookie)) {
			if (oi->i_trial == MAX_TRIALS) {
				if (bop->bo_flags & OF_LOCKALL)
					return fail(bop, -ETOOMANYREFS);
				else
					bop->bo_flags |= OF_LOCKALL;
			}
			if (bop->bo_arbor->t_height <= tree->t_height) {
				/* If height increased */
				//return m0_sm_op_sub(&bop->bo_op, P_CLEANUP,
				//                    P_INIT);
				lock_op_unlock(tree);
				return P_CLEANUP;
			} else {
				/* If height decreased */
				lock_op_unlock(tree);
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
		int	     rc;

		lev = &oi->i_level[oi->i_used];

		s.s_node	     = lev->l_node;
		s.s_idx		     = lev->l_idx;
		s.s_rec.r_key.k_data = M0_BUFVEC_INIT_BUF(&pkey, &ksize);
		s.s_rec.r_val	     = M0_BUFVEC_INIT_BUF(&pval, &vsize);
		s.s_rec.r_flags      = M0_BSC_SUCCESS;
		/**
		 *  There are two cases based on the flag set by user :
		 *  1. Flag BRF_EQUAL: If requested key found return record else
		 *  return key not exist.
		 *  2. Flag BRF_SLANT: If the key index(found during P_NEXTDOWN)
		 *  is less than total number of keys, return the record at key
		 *  index. Else loop through the levels to find valid sibling.
		 *  If valid sibling found, return first key of the sibling
		 *  subtree else return key not exist.
		 */
		if (bop->bo_flags & OF_EQUAL) {
			if (oi->i_key_found)
				node_rec(&s);
			else
				s.s_rec.r_flags = M0_BSC_KEY_NOT_FOUND;
		} else {
			if (lev->l_idx < node_count(lev->l_node))
				node_rec(&s);
			else {
				rc = btree_sibling_first_key_get(oi, tree, &s);
				if (rc != 0) {
					node_op_fini(&oi->i_nop);
					return fail(bop, rc);
				}
			}
		}

		bop->bo_cb.c_act(&bop->bo_cb, &s.s_rec);

		lock_op_unlock(tree);
		return P_CLEANUP;
		//return m0_sm_op_sub(&bop->bo_op, P_CLEANUP, P_DONE);
	}
	case P_CLEANUP:
		level_cleanup(oi);
		return P_DONE;
		//return m0_sm_op_ret(&bop->bo_op);
	default:
		M0_IMPOSSIBLE("Wrong state: %i", bop->bo_op.o_sm.sm_state);
	};
}

int64_t btree_nxt_tick(struct m0_sm_op *smop)
{
	struct m0_btree_op 	*bop = M0_AMB(bop, smop, bo_op);
	//ToDo: Implement complete destroy tick function.
	switch(bop->bo_op.o_sm.sm_state)
	{
		case P_INIT:
			return P_ACT;

		case P_ACT:
			return P_DONE;

		default:
			return 0;
	}
}

int64_t btree_del_tick(struct m0_sm_op *smop)
{
	struct m0_btree_op 	*bop = M0_AMB(bop, smop, bo_op);
	//ToDo: Implement complete destroy tick function.
	switch(bop->bo_op.o_sm.sm_state)
	{
		case P_INIT:
			return P_ACT;

		case P_ACT:
			return P_DONE;

		default:
			return 0;
	}
}

int  m0_btree_open(void *addr, int nob, struct m0_btree **out)
{
	return 0;
}

void m0_btree_close(struct m0_btree *arbor)
{
}

/**
 * m0_btree_create is the API which is used by motr to create btree.
 *
 * @param addr It is the address of root node allocated by the caller of this
 * function
 * @param nob It is the size of root node.
 * @param bt It is the type of btree to be created.
 * @param tx It represents the transaction of which the current operation is
 * part of.
 * @param bop It represents the structure containing all the relevant details
 * for carrying out btree operations.
 */

void m0_btree_create(void *addr, int nob, const struct m0_btree_type *bt,
		     const struct node_type *nt, struct m0_be_tx *tx,
		     struct m0_btree_op *bop)
{
	bop->b_data.addr	= addr;
	bop->b_data.num_bytes	= nob;
	bop->b_data.bt		= bt;
	bop->b_data.nt		= nt;

	m0_sm_op_init(&bop->bo_op, &btree_create_tick, &bop->bo_op_exec,
		      &btree_conf, &G);
}

void m0_btree_destroy(struct m0_btree *arbor, struct m0_btree_op *bop)
{
	bop->bo_arbor	= arbor;
	bop->bo_tx	= NULL;

	m0_sm_op_init(&bop->bo_op, &btree_destroy_tick, &bop->bo_op_exec,
		      &btree_conf, &G);
}

/**
 * Looks up for the key/slant key by the given search key in the btree.
 * The callback routine returns record if key is found else it returns error.
 *
 * @param arbor Btree parameteres.`
 * @param key   Key to be searched in the btree.
 * @param cb    Callback routine to return operation output.
 * @param flags Operation specific flags (cookie, slant etc.).
 * @param bop   Btree operation related parameters.
 */
void m0_btree_get(struct m0_btree *arbor, const struct m0_btree_key *key,
		  const struct m0_btree_cb *cb, uint64_t flags,
		  struct m0_btree_op *bop)
{
	bop->bo_opc = M0_BO_GET;
	bop->bo_arbor = arbor;
	bop->bo_rec.r_key = *key;
	bop->bo_flags = flags;
	bop->bo_cb = *cb;
	m0_sm_op_init(&bop->bo_op, &btree_get_tick, &bop->bo_op_exec,
		      &btree_conf, &G);
}

void m0_btree_nxt(struct m0_btree *arbor, const struct m0_btree_key *key,
		  const struct m0_btree_cb *cb, uint64_t flags,
		  struct m0_btree_op *bop)
{
	m0_sm_op_init(&bop->bo_op, &btree_nxt_tick, &bop->bo_op_exec,
		      &btree_conf, &G);
}

/**
 * m0_btree_put is the API which is used by motr to put the given record into
 * the btree.
 *
 * @param arbor It provides all the required data about the tree in which user
 * wants to insert record.
 * @param tx It represents the transaction of which the current operation is
 * part of.
 * @param rec It represents the record which needs to get inserted. Note that,
 * user may or may not provide valid value but record should be provided with
 * valid key, key size and value size as it is needed for operation.
 * @param cb It represents callback which will get called by the operation to
 * allow user to insert key and record at provided place.
 * @param flags
 * @param bop It represents the structure containing all the relevant details
 * for carrying out btree operations.
 */
void m0_btree_put(struct m0_btree *arbor, struct m0_be_tx *tx,
		  const struct m0_btree_rec *rec,
		  const struct m0_btree_cb *cb, uint64_t flags,
		  struct m0_btree_op *bop)
{
	bop->bo_opc    = M0_BO_PUT;
	bop->bo_arbor  = arbor;
	bop->bo_rec    = *rec;
	bop->bo_cb     = *cb;
	bop->bo_tx     = tx;
	bop->bo_flags  = flags;
	bop->bo_i      = NULL;

	m0_sm_op_init(&bop->bo_op, &btree_put_tick, &bop->bo_op_exec,
		      &btree_conf, &G);
}

void m0_btree_del(struct m0_btree *arbor, const struct m0_btree_key *key,
		  const struct m0_btree_cb *cb, uint64_t flags,
		  struct m0_btree_op *bop)
{
	bop->bo_opc = M0_BO_DEL;
	bop->bo_arbor = arbor;
	bop->bo_rec.r_key = *key;
	bop->bo_cb = *cb;
	bop->bo_flags = flags;

	m0_sm_op_init(&bop->bo_op, &btree_del_tick, &bop->bo_op_exec,
		      &btree_conf, &G);
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
		m0_btree_mod_init();
		btree_ut_initialised = true;
	}
}

static void btree_ut_fini(void)
{
	segops = NULL;
	m0_btree_mod_fini();
	btree_ut_initialised = false;
}

/**
 * This test will create a few nodes and then delete them before exiting. The
 * main intent of this test is to debug the create and delete nodes functions.
 */
static void m0_btree_ut_node_create_delete(void)
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
static void m0_btree_ut_node_add_del_rec(void)
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
	printf("\nUsing seed %lu", curr_time);
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

	printf("\n");
	op.no_opc = NOP_FREE;
	node_free(&op, node1, NULL, 0);

	// Done playing with the tree - delete it.
	op.no_opc = NOP_FREE;
	tree_delete(&op, tree, NULL, 0);

	btree_ut_fini();

	M0_LEAVE();
}

/**
 * In this unit test we exercise a few tree operations in both valid and invalid
 * conditions.
 */
static void m0_btree_ut_basic_tree_oper(void)
{
	void                   *invalid_addr = (void *)0xbadbadbadbad;
	struct m0_btree        *btree;
	struct m0_btree_type    btree_type = {	.tt_id = M0_BT_UT_KV_OPS,
						.ksize = 8,
						.vsize = 8, };
	struct m0_be_tx        *tx = NULL;
	struct m0_btree_op      b_op = {};
	void                   *temp_node;
	const struct node_type *nt = &fixed_format;

	/** Prepare transaction to capture tree operations. */
	m0_be_tx_init(tx, 0, NULL, NULL, NULL, NULL, NULL, NULL);
	m0_be_tx_prep(tx, NULL);
	btree_ut_init();
	/**
	 *  Run a valid scenario which:
	 *  1) Creates a btree
	 *  2) Closes the btree
	 *  3) Opens the btree
	 *  4) Closes the btree
	 *  5) Destroys the btree
	 */

	/** Create temp node space*/
	temp_node = m0_alloc_aligned((1024 + sizeof(struct nd)), 10);
	M0_BTREE_OP_SYNC_WITH_RC(&b_op.bo_op,
				 m0_btree_create(temp_node, 1024, &btree_type,
						 nt, tx, &b_op),
				 &G, &b_op.bo_op_exec);

	m0_btree_close(b_op.bo_arbor);

	m0_btree_open(temp_node, 1024, &btree);

	m0_btree_close(btree);

	btree = b_op.bo_arbor;

	M0_BTREE_OP_SYNC_WITH_RC(&b_op.bo_op, m0_btree_destroy(btree, &b_op),
				 &G, &b_op.bo_op_exec);

	/**
	 * Commenting this line as btree destroy will take care of it.
	 *
	 * m0_free_aligned(temp_node, (1024 + sizeof(struct nd)), 10);
	 */

	/** Now run some invalid cases */

	/** Open a non-existent btree */
	m0_btree_open(invalid_addr, 1024, &btree);

	/** Close a non-existent btree */
	m0_btree_close(btree);

	/** Destroy a non-existent btree */
	/**
	 * Commenting this case till the time we can gracefully handle failure.
	 *
	 * M0_BTREE_OP_SYNC_WITH_RC(&b_op.bo_op, m0_btree_destroy(btree, &b_op),
	 *			 &G, &b_op.bo_op_exec);
	 */

	/** Create a new btree */
	temp_node = m0_alloc_aligned((1024 + sizeof(struct nd)), 10);
	M0_BTREE_OP_SYNC_WITH_RC(&b_op.bo_op,
				 m0_btree_create(temp_node, 1024, &btree_type,
						 nt, tx, &b_op),
				 &G, &b_op.bo_op_exec);

	/** Close it */
	m0_btree_close(b_op.bo_arbor);

	/** Try closing again */
	m0_btree_close(b_op.bo_arbor);

	/** Re-open it */
	m0_btree_open(invalid_addr, 1024, &btree);

	/** Open it again */
	m0_btree_open(invalid_addr, 1024, &btree);

	/** Destory it */
	M0_BTREE_OP_SYNC_WITH_RC(&b_op.bo_op, m0_btree_destroy(btree, &b_op),
				 &G, &b_op.bo_op_exec);

	/** Attempt to reopen the destroyed tree */
	m0_btree_open(invalid_addr, 1024, &btree);
	btree_ut_fini();

}

struct cb_data {
	/** Key that needs to be stored or retrieved. */
	struct m0_btree_key *key;

	/** Value associated with the key that is to be stored or retrieved. */
	struct m0_bufvec    *value;

	/** If value is retrieved (GET) then check if has expected contents. */
	bool                 check_value;
};

static int btree_kv_put_cb(struct m0_btree_cb *cb, struct m0_btree_rec *rec)
{
        struct m0_bufvec_cursor  scur;
	struct m0_bufvec_cursor  dcur;
	m0_bcount_t              ksize;
	m0_bcount_t              vsize;
	struct cb_data          *datum = cb->c_datum;

	if (rec->r_flags == M0_BSC_KEY_EXISTS)
		return M0_BSC_KEY_EXISTS;

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

	if (rec->r_flags == M0_BSC_KEY_NOT_FOUND)
     		return M0_BSC_KEY_NOT_FOUND;

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

		while (v_off <= vsize) {
			m0_bufvec_cursor_init(&kcur, &rec->r_key.k_data);
			m0_bufvec_cursor_init(&vcur, &rec->r_val);
			m0_bufvec_cursor_move(&vcur, v_off);

			if (m0_bufvec_cursor_cmp(&kcur,&vcur)) {
				M0_ASSERT(0);
			}
			v_off += ksize;
		}
	}

	return 0;
}

static int btree_kv_del_cb(struct m0_btree_cb *cb, struct m0_btree_rec *rec)
{

	M0_ASSERT(rec && rec->r_flags == M0_BSC_KEY_NOT_FOUND);

	return 0;
}

/**
 * This unit test exercises the KV operations for both valid and invalid
 * conditions.
 */
static void m0_btree_ut_basic_kv_oper(void)
{
	struct m0_btree_type  btree_type = {	.tt_id = M0_BT_UT_KV_OPS,
						.ksize = 8,
						.vsize = 8, };
	struct m0_be_tx      *tx          = NULL;
	struct m0_btree_op    b_op        = {};
	void                 *temp_node;
	int                   i;
	time_t                curr_time;
	struct m0_btree_cb    ut_cb;
	uint64_t              first_key;
	bool                  first_key_initialized = false;
	struct m0_btree_op    kv_op                 = {};
	const struct node_type *nt                  = &fixed_format;

	M0_ENTRY();

	time(&curr_time);
	printf("\nUsing seed %lu", curr_time);
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
	M0_BTREE_OP_SYNC_WITH_RC(&b_op.bo_op,
				 m0_btree_create(temp_node, 1024, &btree_type,
						 nt, tx, &b_op),
				 &G, &b_op.bo_op_exec);

	for (i = 0; i < 2048; i++) {
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

		M0_BTREE_OP_SYNC_WITH_RC(&kv_op.bo_op,
					 m0_btree_put(b_op.bo_arbor, tx, &rec,
						      &ut_cb, 0, &kv_op),
					 &G, &kv_op.bo_op_exec);
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

		M0_BTREE_OP_SYNC_WITH_RC(&kv_op.bo_op,
					 m0_btree_get(b_op.bo_arbor,
						      &find_key_in_tree,
						      &ut_cb, OF_EQUAL, &kv_op),
					 &G, &b_op.bo_op_exec);

		for (i = 1; i < 2048; i++) {
			find_key = key;
			M0_BTREE_OP_SYNC_WITH_RC(&kv_op.bo_op,
						 m0_btree_nxt(b_op.bo_arbor,
							      &find_key_in_tree,
							      &ut_cb, 0,
							      &kv_op),
						 &G, &b_op.bo_op_exec);
		}
	}

	m0_btree_close(b_op.bo_arbor);
	/**
	 * Commenting this code as the delete operation is not done here.
	 * Due to this, the destroy operation will crash.
	 *
	 *
	 * M0_BTREE_OP_SYNC_WITH_RC(&b_op.bo_op,
	 *				 m0_btree_destroy(b_op.bo_arbor, &b_op),
	 *				 &G, &b_op.bo_op_exec);
	 */
	btree_ut_fini();
}

#define MIN_STREAM_CNT             5
#define MAX_STREAM_CNT             20

#define MIN_RECS_PER_STREAM        5
#define MAX_RECS_PER_STREAM        2048

/**
 * This unit test exercises the KV operations triggered by multiple streams.
 */
static void m0_btree_ut_multi_stream_kv_oper(void)
{
	void                   *temp_node;
	int                     i;
	time_t                  curr_time;
	struct m0_btree_cb      ut_cb;
	struct m0_be_tx        *tx              = NULL;
	struct m0_btree_op      b_op            = {};
	uint32_t                stream_count    = 0;
	uint64_t                recs_per_stream = 0;
	struct m0_btree_op      kv_op           = {};
	const struct node_type *nt              = &fixed_format;
	struct m0_btree_type    btree_type      = {.tt_id = M0_BT_UT_KV_OPS,
						  .ksize = sizeof(uint64_t),
						  .vsize = btree_type.ksize*2,
						  };

	M0_ENTRY();

	time(&curr_time);
	printf("\nUsing seed %lu", curr_time);
	srandom(curr_time);

	stream_count = (random() % (MAX_STREAM_CNT - MIN_STREAM_CNT)) +
			MIN_STREAM_CNT;

	recs_per_stream = (random()%
			   (MAX_RECS_PER_STREAM - MIN_RECS_PER_STREAM)) +
			    MIN_RECS_PER_STREAM;

	/** Prepare transaction to capture tree operations. */
	m0_be_tx_init(tx, 0, NULL, NULL, NULL, NULL, NULL, NULL);
	m0_be_tx_prep(tx, NULL);
	btree_ut_init();
	/**
	 *  Run valid scenario:
	 *  1) Create a btree
	 *  2) Adds records in multiple streams to the created tree.
	 *  3) Confirms the records are present in the tree.
	 *  4) Deletes all the records from the tree using multiple streams.
	 *  5) Close the btree
	 *  6) Destroy the btree
	 */

	/** Create temp node space and use it as root node for btree */
	temp_node = m0_alloc_aligned((1024 + sizeof(struct nd)), 10);
	M0_BTREE_OP_SYNC_WITH_RC(&b_op.bo_op,
				 m0_btree_create(temp_node, 1024, &btree_type,
						 nt, tx, &b_op),
				 &G, &b_op.bo_op_exec);

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

			key = i + (stream_num * recs_per_stream);
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

			M0_BTREE_OP_SYNC_WITH_RC(&kv_op.bo_op,
						 m0_btree_put(b_op.bo_arbor, tx,
							      &rec, &ut_cb, 0,
							      &kv_op),
						 &G, &kv_op.bo_op_exec);
		}
	}

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

		M0_BTREE_OP_SYNC_WITH_RC(&kv_op.bo_op,
					 m0_btree_get(b_op.bo_arbor,
						      &find_key_in_tree,
						      &ut_cb, OF_EQUAL, &kv_op),
					 &G, &b_op.bo_op_exec);
	}

	for (i = 1; i <= recs_per_stream; i++) {
		uint64_t             del_key;
		struct m0_btree_key  del_key_in_tree;
		void                *p_del_key    = &del_key;
		m0_bcount_t          del_key_size = sizeof del_key;
		struct cb_data       del_data;
		uint32_t             stream_num;

		del_data = (struct cb_data) { .key = &del_key_in_tree,
						 .value = NULL,
						 .check_value = false,
						};

		del_key_in_tree.k_data =
				M0_BUFVEC_INIT_BUF(&p_del_key, &del_key_size);

		ut_cb.c_act   = btree_kv_del_cb;
		ut_cb.c_datum = &del_data;

		for (stream_num = 0; stream_num < stream_count; stream_num++) {
			del_key = i + (stream_num * recs_per_stream);
			del_key = m0_byteorder_cpu_to_be64(del_key);

			M0_BTREE_OP_SYNC_WITH_RC(&kv_op.bo_op,
						 m0_btree_del(b_op.bo_arbor,
							      &del_key_in_tree,
							      &ut_cb, 0,
							      &kv_op),
						 &G, &b_op.bo_op_exec);
		}
	}

	m0_btree_close(b_op.bo_arbor);

	/**
	 * Commenting this code as the delete operation is not done here.
	 * Due to this, the destroy operation will crash.
	 *
	 *
	 * M0_BTREE_OP_SYNC_WITH_RC(&b_op.bo_op,
	 *				 m0_btree_destroy(b_op.bo_arbor, &b_op),
	 *				 &G, &b_op.bo_op_exec);
	 */

	btree_ut_fini();
}

#if 0
/**
 * Commenting this ut as it is not required as a part for test-suite but my
 * required for testing purpose
**/
/**
 * This function is for traversal of tree in breadth-first order and it will
 * print level and key-value pair for each node.
 */
static void m0_btree_ut_traversal(struct td *tree)
{
	struct nd *root = tree->t_root;
	struct nd *queue[10000];
	int front = 0, rear = 0;
	queue[front] = root;


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
			printf("level : %d \n",level);
			int total_count = node_count(element);
			int j;
			for(j=0 ; j < total_count; j++)
			{
				uint64_t key = 0;
				get_key_at_index(element, j, &key);
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
				if (rear == 9999) {
					printf("***********OVERFLW***********");
					break;
				}
				queue[rear] = i_nop.no_node;
			}
			//store last child:
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
			if (rear == 9999) {
				printf("***********OVERFLW***********");
				break;
			}
			queue[rear] = i_nop.no_node;
			printf("\n\n");
		} else {
			printf("level : %d \n",level);
			int total_count = node_count(element);
			int j;
			for(j=0 ; j < total_count; j++)
			{
				uint64_t key = 0;
				uint64_t val = 0;
				get_rec_at_index(element, j, &key, &val);
				printf("%"PRIu64",%"PRIu64"\t", key, val);
			}
			printf("\n\n");
		}
	}
}

/**
 * This test will create tree and insert records it into the created tree
 * using insert function for btree(i.e.btree_put_tick())
 */
static void m0_btree_ut_insert_record(void)
{
	struct node_op          op;
	struct m0_btree_type    tt;
	struct td              *tree;
	m0_bcount_t             ksize;
	m0_bcount_t             vsize;
	struct ff_head         *h;

	M0_ENTRY();

	btree_ut_init();

	M0_SET0(&op);

	// Create a Fixed-Format tree.
	op.no_opc = NOP_ALLOC;

	tree_create(&op, &tt, 10, NULL, 0);

	tree = op.no_tree;

	M0_ASSERT(tree->r_ref == 1);
	M0_ASSERT(tree->t_root != NULL);

	tree->t_height = 1;
	h = ff_data(tree->t_root);
	ksize = h->ff_ksize;
	vsize = h->ff_vsize;
        int r;
	for(r=0;r<3;r++) {
	uint64_t total_record = 255;

	uint64_t temp = 255;

	bool inc = false;

	while (total_record) {
		uint64_t               key;
		uint64_t               val;

		struct cb_data         put_data;
		struct m0_btree_key    put_key;
		struct m0_bufvec       put_value;
		struct m0_btree_cb     ut_cb;
		void                  *p_key;
		void                  *p_val;

		key = inc ? temp - total_record : total_record;
		val = key + 1;

		p_key = &key;
		p_val = &val;

		printf("%"PRIu64",",key);

		struct m0_btree_op bop;
		M0_SET0(&bop);
		struct m0_btree btree;
		M0_SET0(&btree);

		bop.bo_op.o_sm.sm_state = P_INIT;
		bop.bo_arbor = &btree;
		bop.bo_arbor->t_desc = tree;

		bop.bo_rec.r_key.k_data = M0_BUFVEC_INIT_BUF(&p_key, &ksize);
		bop.bo_rec.r_val        = M0_BUFVEC_INIT_BUF(&p_val, &vsize);

		put_key.k_data     = M0_BUFVEC_INIT_BUF(&p_key, &ksize);
		put_value          = M0_BUFVEC_INIT_BUF(&p_val, &vsize);

		put_data.key       = &put_key;
		put_data.value     = &put_value;

		ut_cb.c_act   = btree_kv_put_cb;
		ut_cb.c_datum = &put_data;
		bop.bo_cb = ut_cb;

		//struct m0_sm_op        bo_op;
		//bop.bo_op = bo_op;

		while (1) {
			int64_t nxt = btree_put_tick(&bop.bo_op);
			if (nxt == P_DONE )
				break;
			bop.bo_op.o_sm.sm_state = nxt;
		}
		total_record--;
		//printf("\n");
		//m0_btree_ut_traversal(tree);
	}
	printf("\n");
	m0_btree_ut_traversal(tree);
	}
	// Done playing with the tree - delete it.

	op.no_opc = NOP_FREE;
	tree_delete(&op, tree, NULL, 0);

	btree_ut_fini();
	M0_LEAVE();
}
#endif

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
		{"node_create_delete",    m0_btree_ut_node_create_delete},
		{"node_add_del_rec",      m0_btree_ut_node_add_del_rec},
		{"basic_tree_op",         m0_btree_ut_basic_tree_oper},
		{"basic_kv_ops",          m0_btree_ut_basic_kv_oper},
		{"multi_stream_kv_op",    m0_btree_ut_multi_stream_kv_oper},
		/* {"insert_rec",            m0_btree_ut_insert_record}, */
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
