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
 *                             INIT
 *                               |
 *                               v
 *                             SETUP<----------------+
 *                               |                   |
 *                               v                   |
 *                             LOCKALL<------------+ |
 *                               |                 | |
 *                               v                 | |
 *                             DOWN<-------------+ | |
 *                               |               | | |
 *                               v               | | |
 *                        +--->NEXTDOWN-->LOCK-->CHECK
 *                        |     |                 |       +->MOVEUP
 *                        |     |                 v       |      |
 *                        +LOAD-+               ACT---->RESOLVE<-+
 *                                               |              |
 *                                               v              |
 *                                             CLEANUP<----------+
 *                                                |
 *                                                v
 *                                              DONE
 * @endverbatim
 *
 * Phases Description:
 * step 1. NEXTDOWN: traverse down the tree searching for given key till we reach leaf node containing that key
 * step 2. LOAD : load left and/or, right only if there are chances of underflow at the node (i.e. number of keys == min or any other conditions defined for underflow can be used)
 * step 3. CHECK : check if any of the nodes referenced (or loaded) during the traversal have changed
 *                 if the nodes have changed then repeat traversal again after UNLOCKING the tree
 *                 if the nodes have not changed then check will call ACT
 * step 4. ACT: This state will find the key and delete it.
 *              If there is no underflow, move to CLEANUP, otherwise move to RESOLVE.
 * step 5. RESOLVE: This state will resolve underflow,
 *                  it will get sibling and perform merging or rebalancing with sibling.
 *                  Once the underflow is resolved at the node move to its parent node using MOVEUP.
 * step 6. MOVEUP: This state is responsible with moving up to the parent node of the current node and 
 *                 checking whether there is an underflow in that node. 
 *                 If there is an underflow move to RESOLVE, else move to CLEANUP.
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
 * Iteration function will return the record for the search key and iteratively the record of subsequent keys as requested by the caller,
 * if returned key is last key in the node , we may need to fetch next node,
 * To fetch keys in next node: first release the LOCK, call Iteration function and pass key='last fetched' key and next_sibling_flag= 'True'
 * If next_sibling_flag == 'True' we will also load the right sibling node which is to the node containing the search key
 * As we are releasing lock for finding next node, updates such as(insertion of new keys, merging due to deletion) can happen,
 * so to handle such cases, we load both earlier node and node to the right of it
 *
 * Phases Description:
 * step 1. NEXTDOWN: this state will load nodes searching for the given key,
 *                   but if next_sibling_flag == 'True' then this state will also load the leftmost child nodes during its downward traversal.
 * step 2. LOAD: this function will get called only when next_sibling_flag == 'True',
 *               and functionality of this function is to load next node so, it will search and LOAD next sibling
 * step 3. CHECK: check function will check the traversal path for node with key and traversal path for next sibling node if it is also loaded
 *                if traverse path of any of the node has changed, repeat traversal again after UNLOCKING the tree
 *                else, if next_sibling_flag == 'True', go to NEXTKEY to fetch next key, else to ACT for callback
 * step 4. ACT: ACT will provide an input as 0 or 1: where 0 ->done 1-> return nextkey
 * step 5. NEXTKEY:
 *         if next_sibling_flag == 'True',
 *             check last key in the current node.
 *                 if it is <= given key, go for next node which was loaded at phase LOAD (step 2) and return first key
 *                 else return key next to last fetched key
 *            and call ACT for further input (1/0)
 *         else
 *             if no keys to return i.e., last returned key was last key in node.
 *                 check if next node is loaded.
 *                   if yes go to next node else and return first key from that node
 *                   else call Iteration function and pass key='last fetched key' and next_sibling_flag='True'
 *             else return key == given key, or next to earlier retune key and call ACT for further input (1/0)
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
#include "btree/btree.h"
#include "fid/fid.h"
#include "format/format.h"
#include "module/instance.h"
#include "lib/memory.h"
#include "lib/assert.h"

struct m0_btree {
	const struct m0_btree_type *t_type;
	unsigned                    t_height;
	struct tree                *t_addr;
	struct m0_rwlock            t_lock;
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
	struct segaddr              t_root;
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
	D_LEFT,
	/** Move (from left to) right. */
	D_RIGHT
};

struct nd;
struct slot;
struct node_type {
	uint32_t                   nt_id;
	const char                *nt_name;
	const struct m0_format_tag nt_tag;
	int  (*nt_count)(const struct nd *node);
	int  (*nt_space)(const struct nd *node);
	int  (*nt_level)(const struct nd *node);
	int  (*nt_shift)(const struct nd *node);
	void (*nt_fid)  (const struct nd *node, struct m0_fid *fid);
	void (*nt_rec)  (struct slot *slot);
	void (*nt_key)  (struct slot *slot);
	void (*nt_child)(struct slot *slot, struct segaddr *addr);
	bool (*nt_isfit)(struct slot *slot);
	void (*nt_done) (struct slot *slot, struct m0_be_tx *tx, bool modified);
	void (*nt_make) (struct slot *slot, struct m0_be_tx *tx);
	void (*nt_find) (struct slot *slot, const struct m0_btree_key *key);
	void (*nt_fix)  (const struct nd *node, struct m0_be_tx *tx);
	void (*nt_cut)  (const struct nd *node, int idx, int size,
			 struct m0_be_tx *tx);
	void (*nt_del)  (const struct nd *node, int idx, struct m0_be_tx *tx);
	void (*nt_move) (struct nd *src, struct nd *tgt,
			 enum dir dir, int nr, struct m0_be_tx *tx);
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

static int64_t tree_get   (struct node_op *op, struct segaddr *addr, int nxt);
static int64_t tree_create(struct node_op *op, struct m0_btree_type *tt,
			   int rootshift, struct m0_be_tx *tx, int nxt);
static int64_t tree_delete(struct node_op *op, struct td *tree,
			   struct m0_be_tx *tx, int nxt);
static void    tree_put   (struct td *tree);

static int64_t    node_get  (struct node_op *op, struct td *tree,
			     struct segaddr *addr, int nxt);
static void       node_put  (struct nd *node);
static struct nd *node_try  (struct td *tree, struct segaddr *addr);
static int64_t    node_alloc(struct node_op *op, struct td *tree, int shift,
			     struct node_type *nt, struct m0_be_tx *tx, int nxt);
static int64_t    node_free (struct node_op *op, struct nd *node,
			     struct m0_be_tx *tx, int nxt);
static void node_op_fini(struct node_op *op);

static int  node_count(const struct nd *node);
static int  node_space(const struct nd *node);
static int  node_level(const struct nd *node);
static int  node_shift(const struct nd *node);
static void node_fid  (const struct nd *node, struct m0_fid *fid);
static void node_rec  (struct slot *slot);
static void node_key  (struct slot *slot);
static void node_child(struct slot *slot, struct segaddr *addr);
static bool node_isfit(struct slot *slot);
static void node_done (struct slot *slot, struct m0_be_tx *tx, bool modified);
static void node_make (struct slot *slot, struct m0_be_tx *tx);
static void node_find (struct slot *slot, const struct m0_btree_key *key);
static void node_fix  (const struct nd *node, struct m0_be_tx *tx);
static void node_cut  (const struct nd *node, int idx, int size,
		       struct m0_be_tx *tx);
static void node_del  (const struct nd *node, int idx, struct m0_be_tx *tx);
static void node_move (struct nd *src, struct nd *tgt,
		       enum dir dir, int nr, struct m0_be_tx *tx);

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
	/* struct lock_op  i_lop; */
	unsigned        i_used;
	struct level    i_level[0];
};

static int node_count(const struct nd *node)
{
	return node->n_type->nt_count(node);
}

static int node_space(const struct nd *node)
{
	return node->n_type->nt_space(node);
}

static int node_level(const struct nd *node)
{
	return node->n_type->nt_level(node);
}

static int node_shift(const struct nd *node)
{
	return node->n_type->nt_shift(node);
}

static void node_fid(const struct nd *node, struct m0_fid *fid)
{
	node->n_type->nt_fid(node, fid);
}

static void node_rec(struct slot *slot)
{
	slot->s_node->n_type->nt_rec(slot);
}

static void node_key(struct slot *slot)
{
	slot->s_node->n_type->nt_key(slot);
}

static void node_child(struct slot *slot, struct segaddr *addr)
{
	slot->s_node->n_type->nt_child(slot, addr);
}

static bool node_isfit(struct slot *slot)
{
	return slot->s_node->n_type->nt_isfit(slot);
}

static void node_done(struct slot *slot, struct m0_be_tx *tx, bool modified)
{
	slot->s_node->n_type->nt_done(slot, tx, modified);
}

static void node_make(struct slot *slot, struct m0_be_tx *tx)
{
	slot->s_node->n_type->nt_make(slot, tx);
}

static void node_find(struct slot *slot, const struct m0_btree_key *key)
{
	slot->s_node->n_type->nt_find(slot, key);
}

static void node_fix(const struct nd *node, struct m0_be_tx *tx)
{
	node->n_type->nt_fix(node, tx);
}

static void node_cut(const struct nd *node, int idx, int size,
		    struct m0_be_tx *tx)
{
	node->n_type->nt_cut(node, idx, size, tx);
}

static void node_del(const struct nd *node, int idx, struct m0_be_tx *tx)
{
	node->n_type->nt_del(node, idx, tx);
}

static void node_move(struct nd *src, struct nd *tgt,
		      enum dir dir, int nr, struct m0_be_tx *tx)
{
	tgt->n_type->nt_move(src, tgt, dir, nr, tx);
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

int m0_btree_mod_init(void)
{
	struct mod *m;

	M0_ALLOC_PTR(m);
	if (m != NULL) {
		m0_get()->i_moddata[M0_MODULE_BTREE] = m;
		return 0;
	} else
		return M0_ERR(-ENOMEM);
}

void m0_btree_mod_fini(void)
{
	m0_free(mod_get());
}

static bool node_shift_is_valid(int shift)
{
	return shift >= NODE_SHIFT_MIN && shift < NODE_SHIFT_MIN + 0x10;
}

static bool segaddr_is_valid(const struct segaddr *addr)
{
	return (0xff000000000001f0ull & addr->as_core) == 0;
}

static struct segaddr segaddr_build(const void *addr, int shift)
{
	struct segaddr sa;
	M0_PRE(node_shift_is_valid(shift));
	sa.as_core = ((uint64_t)addr) << 9 | (shift - NODE_SHIFT_MIN);
	M0_POST(segaddr_is_valid(addr));
	M0_POST(segaddr_addr(addr) == addr);
	M0_POST(segaddr_shift(addr) == shift);
	return sa;
}

static void *segaddr_addr(const struct segaddr *addr)
{
	M0_PRE(segaddr_is_valid(addr));
	return (void *)(addr->as_core >> 9);
}

static int segaddr_shift(const struct segaddr *addr)
{
	M0_PRE(segaddr_is_valid(addr));
	return addr->as_core & 0xf;
}

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

struct seg_ops {
	int64_t (*so_tree_get)(struct node_op *op,
			       struct segaddr *addr, int nxt);
	int64_t (*so_tree_create)(struct node_op *op, struct m0_btree_type *tt,
				  int rootshift, struct m0_be_tx *tx, int nxt);
	int64_t (*so_tree_delete)(struct node_op *op, struct td *tree,
				  struct m0_be_tx *tx, int nxt);
	void    (*so_tree_put)(struct td *tree);
	int64_t (*so_node_get)(struct node_op *op, struct td *tree,
			       struct segaddr *addr, int nxt);
	void       (*so_node_put)(struct nd *node);
	struct nd *(*so_node_try)(struct td *tree, struct segaddr *addr);
	int64_t    (*so_node_alloc)(struct node_op *op, struct td *tree,
				    int shift, struct node_type *nt,
				    struct m0_be_tx *tx, int nxt);
	int64_t    (*so_node_free)(struct node_op *op, struct nd *node,
				   struct m0_be_tx *tx, int nxt);
	void (*so_node_op_fini)(struct node_op *op);
};

static struct seg_ops *segops;

static int64_t tree_get(struct node_op *op, struct segaddr *addr, int nxt)
{
	return segops->so_tree_get(op, addr, nxt);
}

static int64_t tree_create(struct node_op *op, struct m0_btree_type *tt,
			   int rootshift, struct m0_be_tx *tx, int nxt)
{
	return segops->so_tree_create(op, tt, rootshift, tx, nxt);
}

static int64_t tree_delete(struct node_op *op, struct td *tree,
			   struct m0_be_tx *tx, int nxt)
{
	return segops->so_tree_delete(op, tree, tx, nxt);
}

static void tree_put(struct td *tree)
{
	segops->so_tree_put(tree);
}


static int64_t node_get(struct node_op *op, struct td *tree,
			struct segaddr *addr, int nxt)
{
	return segops->so_node_get(op, tree, addr, nxt);
}

static void node_put(struct nd *node)
{
	segops->so_node_put(node);
}

static struct nd *node_try(struct td *tree, struct segaddr *addr)
{
	return segops->so_node_try(tree, addr);
}

static int64_t node_alloc(struct node_op *op, struct td *tree, int size,
			  struct node_type *nt, struct m0_be_tx *tx, int nxt)
{
	return segops->so_node_alloc(op, tree, size, nt, tx, nxt);
}

static int64_t node_free(struct node_op *op, struct nd *node,
			 struct m0_be_tx *tx, int nxt)
{
	return segops->so_node_free(op, node, tx, nxt);
}

static void node_op_fini(struct node_op *op)
{
	segops->so_node_op_fini(op);
}

static int64_t mem_tree_get(struct node_op *op, struct segaddr *addr, int nxt)
{
	return nxt;
}

static int64_t mem_node_alloc(struct node_op *op, struct td *tree, int shift,
			      struct node_type *nt, struct m0_be_tx *tx,
			      int nxt);
static int64_t mem_node_free(struct node_op *op, struct nd *node,
			     struct m0_be_tx *tx, int nxt);

static int64_t mem_tree_create(struct node_op *op, struct m0_btree_type *tt,
			       int rootshift, struct m0_be_tx *tx, int nxt)
{
	struct td *tree;

	M0_ALLOC_PTR(tree);
	M0_ASSERT(tree != NULL);
	mem_node_alloc(op, tree, rootshift, NULL, NULL, nxt);
	m0_rwlock_init(&tree->t_lock);
	tree->t_root = op->no_addr;
	tree->t_type = tt;
	return nxt;
}

static int64_t mem_tree_delete(struct node_op *op, struct td *tree,
			       struct m0_be_tx *tx, int nxt)
{
	struct nd *root = segaddr_addr(&tree->t_root) +
		(1ULL << segaddr_shift(&tree->t_root));
	op->no_tree = tree;
	op->no_node = root;
	mem_node_free(op, root, tx, nxt);
	return nxt;
}

static void mem_tree_put(struct td *tree)
{
}

static int64_t mem_node_get(struct node_op *op, struct td *tree,
			    struct segaddr *addr, int nxt)
{
	op->no_node = segaddr_addr(addr) + (1ULL << segaddr_shift(addr));
	return nxt;
}

static void mem_node_put(struct nd *node)
{
}

static struct nd *mem_node_try(struct td *tree, struct segaddr *addr)
{
	return NULL;
}

static int64_t mem_node_alloc(struct node_op *op, struct td *tree, int shift,
			      struct node_type *nt, struct m0_be_tx *tx,
			      int nxt)
{
	void          *area;
	struct nd     *node;
	struct segaddr addr;
	int            size = 1ULL << shift;

	M0_PRE(node_shift_is_valid(shift));
	area = m0_alloc_aligned(sizeof *node + size, shift);
	M0_ASSERT(node != NULL);
	node = area + size;
	node->n_addr = segaddr_build(area, shift);
	node->n_tree = tree;
	node->n_type = &ff_node_type;
	m0_rwlock_init(&node->n_lock);
	op->no_node = node;
	op->no_addr = addr;
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

struct ff_head {
	struct m0_format_header ff_fmt;
	struct node_header      ff_seg;
	uint16_t                ff_used;
	uint8_t                 ff_shift;
	uint8_t                 ff_level;
	uint16_t                ff_ksize;
	uint16_t                ff_vsize;
	struct m0_format_footer ff_foot;
};

static struct ff_head *ff_data(const struct nd *node)
{
	return segaddr_addr(&node->n_addr);
}

static void *ff_key(const struct nd *node, int idx)
{
	struct ff_head *h    = ff_data(node);
	void           *area = h + 1;

	M0_PRE(0 <= idx && idx < h->ff_used);
	return area + (h->ff_ksize + h->ff_vsize) * idx;
}

static void *ff_val(const struct nd *node, int idx)
{
	struct ff_head *h    = ff_data(node);
	void           *area = h + 1;

	M0_PRE(0 <= idx && idx < h->ff_used);
	return area + (h->ff_ksize + h->ff_vsize) * idx + h->ff_ksize;
}

static bool ff_rec_is_valid(const struct slot *slot)
{
	struct ff_head *h = ff_data(slot->s_node);

	return
	   _0C(m0_vec_count(&slot->s_rec.r_key.k_data.ov_vec) == h->ff_ksize) &&
	   _0C(m0_vec_count(&slot->s_rec.r_val.ov_vec) == h->ff_vsize);
}

static bool ff_invariant(const struct nd *node)
{
	const struct ff_head *h = ff_data(node);
	return  _0C(h->ff_shift == segaddr_shift(&node->n_addr)) &&
		_0C(ergo(h->ff_level > 0, h->ff_used > 0));
}

static int ff_count(const struct nd *node)
{
	int used = ff_data(node)->ff_used;
	M0_PRE(ff_invariant(node));
	if (ff_data(node)->ff_level > 0)
		used --;
	return used;
}

static int ff_space(const struct nd *node)
{
	struct ff_head *h = ff_data(node);
	M0_PRE(ff_invariant(node));
	return (1ULL << h->ff_shift) - sizeof *h -
		(h->ff_ksize + h->ff_vsize) * h->ff_used;
}

static int ff_level(const struct nd *node)
{
	M0_PRE(ff_invariant(node));
	return ff_data(node)->ff_level;
}

static int ff_shift(const struct nd *node)
{
	M0_PRE(ff_invariant(node));
	return ff_data(node)->ff_shift;
}

static void ff_fid(const struct nd *node, struct m0_fid *fid)
{
}

static void ff_node_key(struct slot *slot);

static void ff_rec(struct slot *slot)
{
	struct ff_head *h = ff_data(slot->s_node);

	M0_PRE(ff_invariant(slot->s_node));
	M0_PRE(slot->s_idx < h->ff_used);

	slot->s_rec.r_key.k_data.ov_vec.v_nr = 1;
	slot->s_rec.r_key.k_data.ov_vec.v_count[0] = h->ff_ksize;
	slot->s_rec.r_key.k_data.ov_buf[0] = ff_val(slot->s_node, slot->s_idx);
	ff_node_key(slot);
	M0_POST(ff_rec_is_valid(slot));
}

static void ff_node_key(struct slot *slot)
{
	struct nd      *node = slot->s_node;
	struct ff_head *h    = ff_data(node);

	M0_PRE(ff_invariant(node));
	M0_PRE(slot->s_idx < h->ff_used);

	slot->s_rec.r_val.ov_vec.v_nr = 1;
	slot->s_rec.r_val.ov_vec.v_count[0] = h->ff_ksize;
	slot->s_rec.r_val.ov_buf[0] = ff_key(node, slot->s_idx);
}

static void ff_child(struct slot *slot, struct segaddr *addr)
{
	struct nd      *node = slot->s_node;
	struct ff_head *h    = ff_data(node);

	M0_PRE(ff_invariant(node));
	M0_PRE(slot->s_idx < h->ff_used);
	*addr = *(struct segaddr *)ff_val(node, slot->s_idx);
}

static bool ff_isfit(struct slot *slot)
{
	struct ff_head *h = ff_data(slot->s_node);

	M0_PRE(ff_invariant(slot->s_node));
	M0_PRE(ff_rec_is_valid(slot));
	return h->ff_ksize + h->ff_vsize >= ff_space(slot->s_node);
}

static void ff_done(struct slot *slot, struct m0_be_tx *tx, bool modified)
{
	M0_PRE(ff_invariant(slot->s_node));
}

static void ff_make(struct slot *slot, struct m0_be_tx *tx)
{
	struct nd      *node  = slot->s_node;
	struct ff_head *h     = ff_data(node);
	int             rsize = h->ff_ksize + h->ff_vsize;
	void           *start = ff_key(node, slot->s_idx);

	M0_PRE(ff_invariant(node));
	M0_PRE(ff_rec_is_valid(slot));
	M0_PRE(ff_isfit(slot));
	memmove(start + rsize, start, rsize * (h->ff_used - slot->s_idx));
	h->ff_used++;
}

static void ff_find(struct slot *slot, const struct m0_btree_key *key)
{
	struct ff_head *h = ff_data(slot->s_node);
	int i = 0;
	int j = h->ff_used;
	M0_PRE(ff_invariant(slot->s_node));
	M0_PRE(key->k_data.ov_vec.v_count[0] == h->ff_ksize);
	M0_PRE(key->k_data.ov_vec.v_nr == 1);

	while (i + 1 < j) {
		int m    = (i + j) / 2;
		int diff = memcmp(ff_key(slot->s_node, m),
				  key->k_data.ov_buf[0], h->ff_ksize);
		M0_ASSERT(i < m && m < j);

		if (diff < 0)
			i = m;
		else if (diff > 0)
			j = m;
		else {
			i = m;
			break;
		}
	}
	slot->s_idx = i;
}

static void ff_fix(const struct nd *node, struct m0_be_tx *tx)
{
}

static void ff_cut(const struct nd *node, int idx, int size,
		   struct m0_be_tx *tx)
{
	M0_PRE(ff_invariant(node));
	M0_PRE(size == ff_data(node)->ff_vsize);
}

static void ff_del(const struct nd *node, int idx, struct m0_be_tx *tx)
{
	struct ff_head *h     = ff_data(node);
	int             rsize = h->ff_ksize + h->ff_vsize;
	void           *start = ff_key(node, idx);

	M0_PRE(ff_invariant(node));
	M0_PRE(idx < h->ff_used);
	M0_PRE(h->ff_used > 0);
	memmove(start, start + rsize, rsize * (h->ff_used - idx - 1));
	h->ff_used--;
}

static void generic_move(struct nd *src, struct nd *tgt,
			 enum dir dir, int nr, struct m0_be_tx *tx)
{
	M0_PRE(src != tgt);
	while (true) {
		struct slot rec;
		struct slot tmp;
		int         srcidx = dir == D_LEFT ? 0 : node_count(src) - 1;
		int         tgtidx = dir == D_LEFT ? node_count(tgt) : 0;

		if (nr == 0 || (nr == NR_EVEN &&
				(node_space(tgt) <= node_space(src))))
			break;
		rec.s_node = src;
		rec.s_idx  = srcidx;
		node_rec(&rec);
		rec.s_node = tgt;
		rec.s_idx  = tgtidx;
		if (!node_isfit(&rec))
			break;
		node_make(&rec, tx);
		tmp.s_node = tgt;
		tmp.s_idx  = tgtidx;
		node_rec(&tmp);
		rec.s_node = src;
		rec.s_idx  = srcidx;
		m0_bufvec_copy(&tmp.s_rec.r_key.k_data,
			       &rec.s_rec.r_key.k_data,
			       m0_vec_count(&rec.s_rec.r_key.k_data.ov_vec));
		m0_bufvec_copy(&tmp.s_rec.r_val, &rec.s_rec.r_val,
			       m0_vec_count(&rec.s_rec.r_val.ov_vec));
		node_del(src, srcidx, tx);
		if (nr > 0)
			nr--;
	}
	node_fix(src, tx);
	node_fix(tgt, tx);
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
