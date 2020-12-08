=======================
HLD of Metadata Backend
=======================

This document presents a high level design (HLD) of meta-data back-end for Motr. The main purposes of this document are: (i) to be inspected by Motr architects and peer designers to ascertain that high level design is aligned with Motr architecture and other designs, and contains no defects, (ii) to be a source of material for Active Reviews of Intermediate Design (ARID) and detailed level design (DLD) of the same component, (iii) to serve as a design reference document. The intended audience of this document consists of Motr customers, architects, designers and developers. 

*********************
Introduction
*********************

Meta-data back-end (BE) is a module presenting an interface for a transactional local meta-data storage. BE users manipulate and access meta-data structures in memory. BE maps this memory to persistent storage. User groups meta-data updates in transactions. BE guarantees that transactions are atomic in the face of process failures. BE provides support for a few frequently used data-structures: double linked list, B-tree and extmap.

*********************
Dependencies
*********************

- a storage object (stob) is a container for unstructured data, accessible through m0_stob interface. BE uses stobs to store meta-data on persistent store. BE accesses persistent store only through m0_stob interface and assumes that every completed stob write survives any node failure. It is up to a stob implementation to guarantee this;

- a segment is a stob mapped to an extent in process address space. Each address in the extent uniquely corresponds to the offset in the stob and vice versa. Stob is divided into blocks of fixed size. Memory extent is divided into pages of fixed size. Page size is a multiple of block size (it follows that stob size is a multiple of page size). At a given moment in time, some pages are up-to-date (their contents is the same as of the corresponding stob blocks) and some are dirty (their contents was modified relative to the stob blocks). In the initial implementation all pages are up-to-date, when the segment is opened. In the later versions, pages will be loaded dynamically on demand. The memory extent to which a segment is mapped is called segment memory; 

- a region is an extent within segment memory. A (meta-data) update is a modification of some region;

- a transaction is a collection of updates. User adds an update to a transaction by capturing the update's region. User explicitly closes a transaction. BE guarantees that a closed transaction is atomic with respect to process crashes that happen after transaction close call returns. That is, after such a crash, either all or none of transaction updates will be present in the segment memory when the segment is opened next time. If a process crashes before a transaction closes, BE guarantees that none of transaction updates will be present in the segment memory;

- a credit is a measure of a group of updates. A credit is a pair (nr, size), where nr is the number of updates and size is total size in bytes of modified regions.

*********************
Requirements
*********************

- **R.M0.MDSTORE.NUMA**: allocator respects NUMA topology 

- **R.MO.REQH.10M**: performance goal of 10M transactions per second on a 16 core system with a battery backed memory. 

- **R.M0.MDSTORE.LOOKUP**: Lookup of a value by key is supported 

- **R.M0.MDSTORE.ITERATE**: Iteration through records is supported. 

- **R.M0.MDSTORE.CAN-GROW**: The linear size of the address space can grow dynamically 

- **R.M0.MDSTORE.SPARSE-PROVISIONING**: including pre-allocation 

- **R.M0.MDSTORE.COMPACT, R.M0.MDSTORE.DEFRAGMENT**: used container space can be compacted and de-fragmented 

- **R.M0.MDSTORE.FSCK**: scavenger is supported 

- **R.M0.MDSTORE.PERSISTENT-MEMORY**: The log and dirty pages are (optionally) in a persistent memory 

- **R.M0.MDSTORE.SEGMENT-SERVER-REMOTE**: backing containers can be either local or remote 

- **R.M0.MDSTORE.ADDRESS-MAPPING-OFFSETS**: offset structure friendly to container migration and merging 

- **R.M0.MDSTORE.SNAPSHOTS**: snapshots are supported 

- **R.M0.MDSTORE.SLABS-ON-VOLUMES**: slab-based space allocator 

- **R.M0.MDSTORE.SEGMENT-LAYOUT** Any object layout for a meta-data segment is supported 

- **R.M0.MDSTORE.DATA.MDKEY**: Data objects carry meta-data key for sorting (like reiser4 key assignment does). 

- **R.M0.MDSTORE.RECOVERY-SIMPLER**: There is a possibility of doing a recovery twice. There is also a possibility to use either object level mirroring or a logical transaction mirroring. 

- **R.M0.MDSTORE.CRYPTOGRAPHY**: optionally meta-data records are encrypted 

- **R.M0.MDSTORE.PROXY**: proxy meta-data server is supported. A client and a server are almost identical. 

*********************
Design Highlights
*********************

BE transaction engine uses write-ahead redo-only logging. Concurrency control is delegated to BE users.

*************************
Functional Specification
*************************

BE provides interface to make in-memory structures transactionally persistent. A user opens a (previously created) segment. An area of virtual address space is allocated to the segment. The user then reads and writes the memory in this area, by using BE provided interfaces together with normal memory access operations. When memory address is read for the first time, its contents is loaded from the segment (initial BE implementation loads the entire segment stob in memory when the segment is opened). Modifications to segment memory are grouped in transactions. After a transaction is closed, BE asynchronusly writes updated memory to the segment stob.  

When a segment is closed (perhaps implicitly as a result of a failure) and re-opened again, the same virtual address space area is allocated to it. This guarantees that it is safe to store pointers to segment memory in segment memory. Because of this property, a user can place in segment memory in-memory structures, relying on pointers: linked lists, trees, hash-tables, strings, etc. Some in-memory structures, notably locks, are meaningless on storage, but for simplicity (to avoid allocation and maintenance of a separate set of volatile-only objects), can nevertheless be placed in the segment. When such a structure is modified (e.g., a lock is taken or released), the modification is not captured in any transaction and, hence, is not written to the segment stob.

BE-exported objects (domain, segment, region, transaction, linked list and b-tree) support Motr non-blocking server architecture.

*************************
Use Cases
*************************

Scenarios
==========

+-----------------------------+----------------------------------------------------------------+
| Scenario                    | [usecase.component.name]                                       |
+-----------------------------+----------------------------------------------------------------+
|Relevant quality attributes  |[e.g., fault tolerance, scalability, usability, re-usability]   |
+-----------------------------+----------------------------------------------------------------+
|Stimulus                     |[an incoming event that triggers the use case]                  |
+-----------------------------+----------------------------------------------------------------+
|Stimulus source              |[system or external world entity that caused the stimulus]      |
+-----------------------------+----------------------------------------------------------------+
|Environment                  | [part of the system involved in the scenario]                  |
+-----------------------------+----------------------------------------------------------------+
|Artifact                     |[change to the system produced by the stimulus]                 |
+-----------------------------+----------------------------------------------------------------+
|Response                     |[how the component responds to the system change]               |
+-----------------------------+----------------------------------------------------------------+
|Response measure             |[qualitative and (preferably) quantitative measures of response | 
|                             |that must be maintained]                                        |
+-----------------------------+----------------------------------------------------------------+
|Questions and Answers        |                                                                |
+-----------------------------+----------------------------------------------------------------+

