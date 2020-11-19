===========================
Understanding Layout Schema
===========================

***************
Overview
***************

The layouts of the files are relevant for a disk-based file system. This is one of the most important services (or functionality) provided by the disk-based file-systems. They manage the underlying storage and optimally utilize the storage using user-opaque layout of the file data blocks as well as meta-data. The user accesses the files using the system calls and does not have to bother about the disk block management. The file layout refers to the arrangement of the disk blocks that belong to a file. The layouts are designed by taking into consideration the optimum storage utilization and efficient access to the data.


Traditional file layout 
========================

Most of the disk-based file-systems access any file using meta-data for that file, famously known on UNIX as inode. Along with other attributes of the file, the inode stores the file layout. In a classic file-system, this file layout is also called as block map. A typical EXT2 block map (a file layout) comprises of:

- 10 direct block 

- 1 indirect block 

- 1 doubly indirect block 

- 1 triple indirect block 

The traditional block map in an inode would contain logical disk block numbers. A block map for direct blocks is simply an array of block numbers.   

Optimization using extents 
---------------------------

The traditional EXT2 file layout (block map) was evolved to store extents or block clusters for a file in EXT4. This provided more compact storage for the file layout and also provided better sequential access to the file data. This layout started storing <starting block number, len> instead of storing only a block number within the file layout.

File layout for snapshots
==========================

With advent of 24x7 availability of the applications and a requirement to back-up the on-line file-system created a need for creating the snapshots of the file system. A snapshot of a file-system creates a point in time copy of all the files in a file system. If a file is modified after the snapshot is created, there is need to store only the incremental changes. Duplicating the entire block map for the file would waste the storage space. This created a need to access the new modified blocks and old unmodified blocks through the file. The classical block map was again modified to create a block map chain that would point to old block map as necessary (Not an EXT4 feature).   

File Layout in distributed name-space
=======================================

In many organizations, file systems are deployed using NAS boxes. As the storage demands grow, more NAS boxes are added. This creates lot of administrative overhead. To provide storage scalability, a single unified namespace, and easier administration, many solutions such as clustered NAS, scale-out NAS, file switches and file virtualization products were developed. 

Many of these products developed their own layouts. The files were accessed using a head server with bunch of NAS boxes behind it. The files are accessed using a top-level layout within a single distributed name space. This layout would then map to the underlying file system. The top-level layout now points to the files on different underlying file-systems rather than a block map. These layouts are stored either as data or attributes of a traditional inode. The underlying (stand-alone) file system uses the classical block-map layout.

Lustre case
=============

Lustre has two levels of mapping. The first level of mapping is stored on MDS as a file EA. The layout is structured as {{ost1,fid1}, {ost2,fid2}, {ost3,fid3}, {ost4,fid4}, … {ost-n,fid-n}}, along with stripe size. So the logical offset in a file can be calculated into one of the object represented by {ost-index, fid}. With this info, lustre client communicates to that ost, accessing the object with that specified fid. Another mapping from object logical offset to disk block is done by a local disk file system, ldiskfs (the origin of ext4).

pNFS and file layouts
=====================

In a traditional NAS deployment, there is a NAS server and multiple NFS or CIFS clients. With the advent of SAN, even the client machines started having direct access to the storage. To boost the performance of overall system, pNFS  like systems were developed. The pNFS  clients started requesting file layouts so that they could perform direct I/O (this does not mean un buffered I/O) to the files. This created different kinds of requirements for the file-layouts. The layouts that are to be transported need to be  

- Compact (not occupying a lot of space/memory) 

- Possibly they need to be in network-byte order (pre-encoded) for easy transport 

- A part (region of a file) of the layout could be delivered if necessary 

***************
Design Goals
***************

Looking at the history of the file-layouts, it’s very clear that the file layouts evolve over a period of time. A host of copy services (snapshot, back-up, archiving, data de-duplication, replication etc) can also influence the file layout. In Motr the layout will be transported between two servers. The file layout will need a compact description so that it will consume minimal resources (memory, network bandwidth). Motr should support multiple layouts. It should allow accommodation of new (future) layouts. It should allow migration of file data from one layout to another. The design goals for the file layouts (and schema) are listed below:

- Support multiple layouts within the file-system 

- Provide support to add new layouts 

- Provide support for composite/mixed layouts (combining different layout types together) 

- Provide migration of file data from one layout to another 

- Provide compact description of the layouts 

- Provide pre-encoded layouts (where necessary) for easy transport 

*********************
Motr Layout Types  
*********************

HLD for layout schema lists the following layout types: 

- SNS 

- Local RAID 

- RAID levels 

- Future layouts such as de-duplication, encryption, compression 

Although these layout types are well classified from schema perspective, we believe there will be two types of layouts that will be supported for the planned demo: 

- Parity de-clustering layout (c2_pdclust_layout) (Conceptually this is ‘SNS layout with parity de-clustering as a property’ and is referred as pdclust layout.) 

- Composite layout (mixed layout for representing layout under repair)

*************************
Layout Schema Interfaces
*************************

Creating Layouts
================

Motr will support certain number of layout types (e.g. pdclust, raid, composite). Creating a layout of any of the supported types is an administrative task. (Note: Need to figure out what is the user interface to create a layout).

There are various parameters that will have to be considered while initializing any file layout. Some of the parameters are listed below:

- Policy (e.g. prealloc blocks?) 

- Layout Type 

- Layout type specific data (This may contain sub-maps) 

- Backing store

This interface will make following assumptions:

- While initializing a layout, parameters like the ones mentioned above will be considered by another encapsulating task viz. “Layout”. The task “Layout Schema” will not play any role into that. 

- Backing store objects will be created before making this call. Appropriate heuristics will be used while creating these objects. For example, for a RAID-like layout the storage objects for the stripe units will not be located on the same disk [The term storage object used here is different from Motr storage object. The storage object referred here will map to component object of Motr. 

- Sub-maps (aka sub-layout) will be created before creating top level layout. 

- Layout creation operation will fill in type-specific data in the type tables (or the fields in a record that are specific to a layout).

Assigning Layout To A File
===========================

A file layout is one of the attributes of the file. A layoutid is assigned to a file when a file is created and it is then stored in the file attributes as one of the properties of the file. In Motr, a file will be created using a file create (or open) FOP. There are various parameters that may need to be considered while assigning layoutid to a file. Some of the parameters are listed below:

- Parent dir inheritance attribute 

- Policy (e.g. prealloc blocks?)

A few assumptions regarding file create operation: 

- If a policy such as storage pre-allocation is used, the block (blocks of backing store) reservation will happen first. 

- Creation of a file requires creation of its component objects and the creator (a client, usually) must assure that cobs can be created (i.e., that free identifiers exist). 

- File-id to layout-id mapping is stored by the fileattr_basic table (also called as fab).

Updating or Modifying Layouts 
==============================

The layouts will be modified under the following conditions: 

- The system administrator changes the layout properties 

- The underlying storage of the layout is affected

These events will lead to a composite layout until the file data migrates to the new layout completely. The layout schema should provide interface to update the existing layout. Modifying the layout may result in a new layout.

The modification to the layout opens up following design related queries:

- Should update to the layout change the layoutid? 

- Should this function generate a layout change notification? 

- Should the modified layout be marked as invalid till old layout is dropped by all the servers using it?

Searching Layouts
===================

The layout provides mapping of logical file block to corresponding logical storage block(s). In many circumstances identifying layout using fileid is useful. While in some other conditions the inverse (or reverse) mapping from storage to files is useful. Hence the search function should provide flexibility to obtain layoutid (key to the layout) using different parameters (or mechanisms).

The query interface of the layout will be used to obtain the details of the layout structure where as search interface will provide only the layoutid.

Search by storage object id 
----------------------------

When there is a back-end storage failure, it has to be marked into the database (Motr meta-data). The layouts affected by the storage device will also have to be updated. To provide this functionality, searching the layout by storage id (inverse mapping) is useful. This type of interface is also helpful for the recovery IO (Motr middleware).

Search by file id
------------------

This mapping will often be used by the client performing the IO on the file. When an IO is being performed on a file, a layout will be obtained by using fileid. 

Query / Lookup Layout
======================

A layout is queried using a layoutid. This function will fetch all the details of the layout. This query can either be covering the entire file or a region of the file. For compact layouts such as layout formulae, this will not matter. This interface will be used by the clients for performing IO against a file. Query for a file region or partial layout is out of the scope of layout schema.

Deleting a Layout
===================

The HLD talks about holding a reference to a layout and decrementing the reference to the layout when a file is deleted. Although this idea is quite appealing, it’s implementation in the schema is not clear at this point in time. Until this design and use-case becomes clear, we will simply delete a layout when the file (using this layout) is deleted. 

List Layout types
==================

Layouts are influenced by layout types. A block map style layout will contain array (tree) of all the storage blocks. A formula based layout will work with parameters (variables) of the formula. There will be one or more tables storing the layout type information. 

During the initialization of the layout module, it will be necessary to load all the known layout types from the database. This will in turn help to create a layout for the file of a desired layout type using the layout type operations.

***************
Layout Schema
***************

UML/ER Diagram
===============

This is yet to be done. But the following tables should give some idea.

Layout Tables
==============

File Id to Layout Id Mapping
-------------------------------

File id to layout id mapping is stored by the basic file attributes table (FAB). Hence, there is no table in the layout schema to store this mapping. Following file id to layout id mapping is shown for the completeness of the example in this section and is assumed to be part of FAB.

+----------------------+--------------------------------------+
|**File Id (c2_fid)**  | **Layout Id (c2_layout_id)**         |
+======================+======================================+
|fid1001               |L1                                    |
+----------------------+--------------------------------------+
|fid1002               |L1                                    |
+----------------------+--------------------------------------+
|fid1003               |L1                                    |
+----------------------+--------------------------------------+
|fid1004               |L2                                    |
+----------------------+--------------------------------------+
|fid1005               |L2                                    |
+----------------------+--------------------------------------+
|fid1006               |L3                                    |
+----------------------+--------------------------------------+
|fid1007               |L4                                    |
+----------------------+--------------------------------------+
|fid1008               |L5                                    |
+----------------------+--------------------------------------+
|...                   |...                                   |
+----------------------+--------------------------------------+
|fid....x              |L6                                    |
+----------------------+--------------------------------------+

The Table Layouts is mentioned below.


+----------------------+--------------------------------------------+
|Table Name            | layouts                                    |
+----------------------+--------------------------------------------+
|Key                   |layout_id (Type: c2_layout_id)              |
+----------------------+--------------------------------------------+
|Record                |- layout_type (pdclust, composite, coblist) |
|                      |                                            |
|                      |- reference_count                           |
|                      |                                            |
|                      |- byte_array                                |
|                      |                                            |
+----------------------+--------------------------------------------+
|Comments              |byte_array is parsed and                    |
|                      |composed by the layout_type specific        |
|                      |decoding and encoding methods.              |
|                      |                                            |
|                      |                                            |
|                      |For parity-declustered layout type with     | 
|                      |formula-type as “LINEAR”, the byte_array    |
|                      |contains the record with N (number of data  |
|                      |units in parity group) (Type: uint32_t),    |
|                      |K (number of parity units in parity groups) |
|                      |(Type: uint32_t) and unit size              |
|                      |(Type: uint32_t).                           |
|                      |                                            |
|                      |For parity-declustered layout type with     |
|                      |formula-type as “LIST”, the byte_array      |
|                      |contains list of cob ids.                   |
+----------------------+--------------------------------------------+

Tabular representation of “layouts” table with example 


+------------+--------------+-----------------------------+-----------------------+
|layout_id   | layout_type  | reference_count (c2_uint32) | byte_array            |
+============+==============+=============================+=======================+
|L1          |pdclust       | 3                           |LINEAR, U, N, K        |
+------------+--------------+-----------------------------+-----------------------+
|L2          |pdclust       | 2                           |LINEAR, U, N, K        |
+------------+--------------+-----------------------------+-----------------------+
|L3          |pdclust       | 1                           |LINEAR, U, N, K        |
+------------+--------------+-----------------------------+-----------------------+
|L4          |composite     | 1                           |                       |
+------------+--------------+-----------------------------+-----------------------+
|L5          |composite     | 1                           |                       |
+------------+--------------+-----------------------------+-----------------------+
|L6          |pdclust       | 1                           |LIST, cob1, cob2, cob3 |       
+------------+--------------+-----------------------------+-----------------------+
   
