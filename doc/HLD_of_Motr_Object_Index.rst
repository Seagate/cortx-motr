=========================================
High level design of a Motr Object Index
=========================================

This document presents a high level design (HLD) of an Object Index for Motr M0 core. The main purposes of this document are: (i) to be inspected by M0 architects and peer designers to ascertain that high level design is aligned with M0 architecture and other designs, and contains no defects, (ii) to be a source of material for Active Reviews of Intermediate Design (ARID) and detailed level design (DLD) of the same component, (iii) to serve as a design reference document.

The intended audience of this document consists of M0 customers, architects, designers and developers.

**************
Introduction
**************

The object index performs the function of a metadata layer on top of M0 storage objects. A M0 storage object (stob) is a flat address space where one can read from or write on, with block size granularity. Stobs have no metadata associated with them. To be used as files (or components of files, aka stripes), additional metadata must be associated with the stobs.

- namespace information: parent object id, name, links

- file attributes: owner/mode/group, size, m/a/ctime, acls

- fol reference information: log sequence number (lsn), version counter

Metadata must be associated with both component objects (cobs), and global objects. Global objects would be files striped across multiple component objects. Ideally global objects and component objects should reuse the same metadata design (a cob can be treated as a gob with a local layout).

*************
Definitions
*************

- A storage object (stob) is a basic M0 data structure containing raw data.

- A component object (cob) is a component (stripe) of a file, referencing a single storage object and containing metadata describing the object.

- A global object (gob) is an object describing a striped file, by referring to a collection of component objects.

**************
Requirements
**************

- [R.M0.BACK-END.OBJECT-INDEX]: an object index allows back-end to locate an object by its fid

- [R.M0.BACK-END.INDEXING]: back-end has mechanisms to build meta-data indices

- [R.M0.LAYOUT.BY-REFERENCE]: file layouts are stored in file attributes by reference

- [R.M0.BACK-END.FAST-STAT]: back-end data-structures are optimized to make stat(2) call fast

- [R.M0.DIR.READDIR.ATTR]: readdir should be able to return file attributes without additional IO

- [R.M0.FOL.UNDO]: FOL can be used for undo-redo recovery

- [R.M0.CACHE.MD]: meta-data caching is supported


