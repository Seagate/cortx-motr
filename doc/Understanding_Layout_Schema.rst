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
 

   
