=========================================================
High level design of Auxiliary Databases for SNS repair 
=========================================================

This document presents a high level design (HLD) of the auxiliary databases required for SNS repair. The main purposes of this document are: (i) to be inspected by M0 architects and peer designers to ascertain that high level design is aligned with M0 architecture and other designs, and contains no defects, (ii) to be a source of material for Active Reviews of Intermediate Design (ARID) and detailed level design (DLD) of the same component, (iii) to serve as a design reference document. 

The intended audience of this document consists of M0 customers, architects, designers and developers. 

*************
Introduction
*************

Formally, this task is to implement any additional meta-data tables that are not needed for normal ioservice operation, but required for SNS repair. At the moment it seems that a single table is necessary. SNS repair proceeds in the file fid (also referred to as global object fid) order and by a parity group number (that is, effectively in a logical file offset order) within a file. For its normal operation ioservice requires no knowledge of files at all, because it works with component objects (cob-s), the translation between user visible (file, offset) coördinates to (cob, offset) coördinates is performed by the client through layout mapping function.

The additional table should have (device id, file fid) as the key and cob fid as the corresponding record. The represented (device, file)->cob mapping is 1:1.

The task should define the table and provide wrapper functions to inserts and delete (device_id, file_fid, cob_fid) pairs, lookup cob-fid by device-id and file-fid, and iterate over the table in file-fid order for a given device. In the future, device-id will be generalised to container-id. 

 
This table is used by the object creation path, executed on every ioservice when a file is created. A new file->cob fid mapping is installed at this point. The mapping is deleted by file deletion path (not existing at the moment). During SNS repair it is used by storage agents associated with storage devices. Each agent iterates over file-fid->cob-fid mapping for its device, selecting the next cob to process.

*************
Definitions   
*************

A cobfid map is a persistent data structure that tracks the id of cobs and their associated file fid, contained within other containers, such as a storage object.

***************
Requirements
***************

- [r.container.enumerate]: it is possible to efficiently iterate through the containers stored (at the moment) on a given storage device. This requirement is mentioned as a dependency in [2]. 

- [r.container.enumerate.order.fid]: it is possible to efficiently iterate through the containers stored on a given storage device in priority order of global object fid. This is a special case of [r.container.enumerate] with a specific type of ordering. 

- [r.generic-container.enumerate.order.fid]: it is possible to efficiently iterate through the containers stored in another container in priority order of global object fid.   This extends the previous requirement, [r.container.enumerate.order.fid], to support enumeration of the contents of arbitrary types of containers, not just storage devices. 

********************
Design Highlights
********************

- A cobfid map is implemented with the M0 database interface which is actually based upon key-value tables implemented using the Oracle Berkeley Database, with one such “table” or “database” per file. 

- Interfaces to add and delete such entries are provided for both devices and generic containers.  

- Interfaces to iterate through the contents of a device or generic container are provided. 



