# HLD of New B-Tree

Motr implements B-Tree at the backend in order to handle metadata related operations. It can be internal metadata such as cob list (used by ios and repair), allocation data (used by ad stob), balloc (used by ios), global dix indices (used by dix), meta index (used by cas) or external metadata by applications like s3server using catalogues (used by cas, dix). This B-Tree code should support better performance and upgrade.

## Requirements

The scope of B-Tree development is defined by the following requirements:

- [r.btree.clean] develop B-Tree (B+ Tree) implementation from scratch;
- [r.btree.r2p0] B-Tree implementation has to be available in the first release (ldrr2p0) and is fully integrated with the rest of the code-base;
- [r.btree.future-proof] after the first release, online conversion to the later versions should be possible. Future versions of B-Tree can add more features;
- [r.btree.features] B-Tree format and algorithms should be ready to support advanced features that will be necessary in the future;
- [r.btree.performance] performance properties of the implementation (throughput, operations per second, concurrency levels, memory and io footprints) should be sufficient for fulfilment of the overall product performance requirements;
- [r.btree.quality] the implementation should be of high quality. Quality should be measured and attained by comprehensive testing and documentation;
- [r.btree.scale] the implementation should be sufficiently scalable to support the overall product scalability requirements.

## System Design

### Approach
--------------------------
This B-Tree code implements a flavor of B-Tree known as B+ Tree. The structure of B+ Tree gives it an advantage of improved performance and stability over a regular B-Tree. The varied meta-data structures will will need Keys and Values which can be either fixed in size or variable in size. Considering this, the B-Tree code supports 3-types of B-Tree node formats. Also to keep the Keys and Values closer to their host B-Tree node, we embed the Keys and Values within the node instead of maintaining a pointer to the Keys and Values.

### Node Types
--------------------------
Every B-Tree node has a header which stores important information such as Type of B-Tree node, Count of records, Key size, Value size, …
Below is the description of all the Node types which are supported by the B-Tree code.

a. Fixed Key Fixed Value Size (FF node type)

  The FF node type has fixed sized Keys and Values. Following trees use this type of node format: balloc group extents, balloc group desc, cob fileattr omg, cas meta, cas dead idx and cas ct idx.
We start populating Keys from the left side and the Values from the right side of free node space. In case of leaf node, since all the Keys are of same size and all the Values are also of same size (though the Keys and Values may not be of same size), we can calculate the address of a particular Key or Value based on its index.
In case of internal node, the structure remains the same but now, the Value field will contain the address of the child node instead of actual Value data associated with the Key. Due to this, the Value size is now sizeof(void *).

![image](https://user-images.githubusercontent.com/62925646/176011278-77295c51-84e2-4a0e-bce2-c61f98572903.png)

b. Fixed Key Variable Value Size (FKVV node type)

  The FKVV node type has fixed sized Keys but Values could be variable sized. Following trees use this type of node format: extmap, conf db, cob object index, and cob fileattr basic.
We start populating Keys from the left side and the Values from the right side of free node space. In case of leaf node, since Values are variable in size, we maintain an offset along with the Key. On subtracting the end address of the node with the offset, we get the actual address of the Value associated with the Key.

![image](https://user-images.githubusercontent.com/62925646/176011328-1a502029-90a2-4a73-9b3b-d39c13847ef1.png)

In case of internal node, the Value field will contain address of the child node. Hence, it is no more variable in size. This becomes the case of FF node type and a similar structure of FF node type is used for internal nodes of FKVV node type.

c. Variable Key Variable Value Size (VKVV node type)

  The VKVV node type has variable sized Keys and variable sized Values. Following trees use this type of node format: catalogue tree, and cob fileattr ea.
We start populating Keys from the left side and the Values from the right side of free node space. In case of leaf nodes, we maintain a floating directory at the center of the free node space. This directory contains offsets for Keys and Values. To get the starting address of Key, we add the offset of that Key from the directory to the start address of the free node space. Similarly, to get the starting address of Value, we subtract the offset of Value from the end address of the node.  Since the directory is floating in nature, we will hold the address of the directory in the header of the node.

![image](https://user-images.githubusercontent.com/62925646/176011373-0a6270ed-16f3-43c1-b305-7da5a0c9a149.png)

In case of internal nodes, as the Value field will contain address of child nodes, we will have fixed size Values but the Keys will be of variable size. In such a case, we will continue populating Keys from the left side and the Values from the right side of free node space but we will maintain an offset with the Value field to find the actual location of Key associated with it.

![image](https://user-images.githubusercontent.com/62925646/176011394-8e3b989f-593a-4b67-a136-6f5391f53e98.png)

### B+ Tree Operations
--------------------------
In B+ Tree, unlike B-Tree, the Values for the corresponding Keys are always present in the leaf node. So every Read and Write operation is required to traverse the path from root to the leaf node loading the intermediate (including the root and the leaf) nodes from BE segment to memory. Once the appropriate leaf node is loaded, the requested operation is executed. B-Tree consumers store the root node(s) as a part of their container data structure hence the root pointer will not be changed by any operations of the B+ Tree.

Unlike most B-Tree (and B+ Tree) implementations which lock the complete tree before performing any operation on the B-Tree nodes, the implemented B-Tree does not lock the complete tree while traversing from the root towards the leaf node. Not locking the B-Tree during the traversal makes this B-Tree available to execute operations in parallel from other threads.

To handle asynchronous B+ Tree operations, two locks, namely node lock and tree lock are used. For any Read or Write operation on a node, it is necessary to take node locks. While performing any modifications to the tree the tree lock needs to be taken. Tree lock is taken once all the traversed nodes are loaded and also other resources required for the operation have been acquired.

a. Insert operation

  As a part of this operation a new record is added to the leaf node of the tree. If sufficient space is not found in the existing B-Tree node then new node is allocated and records are moved between the old and the new nodes to create space for inserting the new record.

b. Delete operation

  As a part of this operation an existing record from the tree is deleted. If the node is found to be empty after deleting this record then the node is deleted and the tree is rebalanced. Unlike other B-Tree implementations we do not rebalance the nodes when half of the records have been deleted. Instead we opted for a lazy rebalance approach of not shuffling existing records between nodes to avoid record movement and also to transact minimal changes. This approach might have us a B-Tree with lot of sparsely populated nodes and we might change this in future to have active rebalancing.

c. Lookup operation

  As a part of this operation an existing record from the tree is returned back to the caller. If this record is not found in the tree but BOF_SLANT flag is set then the next record (whose Key is just higher than the Key which was sent for lookup) is returned back to the caller.

### Memory Limit Feature
--------------------------
The B-Tree module has code which helps control the usage of physical memory space for mmaped BE segment.

To support this the B-Tree nodes are allocated as a multiple of CPU page size and also the node start address is aligned at CPU page size. The B-Tree code maintains the loaded nodes in a LRU list based on their access pattern. When there is a need to release Physical memory then this LRU list is scanned and the nodes are unmapped from the memory thus releasing physical memory to kernel to be re-used for other activities.

Memory limit feature is a novel way to free main memory when the consumption of memory becomes very high. This feature was needed since the kernel cannot use swap space, when it runs out of physical memory, for Kubernetes container applications.

## References
https://github.com/Seagate/cortx-motr/blob/documentation/doc/dev/btree/index.rst
