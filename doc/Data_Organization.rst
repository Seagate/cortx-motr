=================
Data Organization
=================

***************
Storage Devices
***************

The Motr term Storage Device means one device that is attached to a server. For example, a disk, a PCI flash device, a RAID controller, main memory pool and a partition are all Storage Devices. Ideally, an individual storage device would contain either data or metadata, rather than being mixed use for both. As the size of clusters continues to scale upwards, we expect there to be perhaps 100,000's of storage devices in a large cluster. Storage devices are named and a location database is maintained to track them. A storage device knows its own name (self-identification) and can be physically moved across the system. The most likely way that storage devices are implemented is as block devices in the operating system, accessed using asynchronous, IOV-based, direct I/O (currently available in both Windows and Linux). 
    
*********
Objects
*********

An Object (or a Storage Object) is a fundamental Motr building block. An object contains a description of linear address space together with some other meta-data. Object address space describes allocated blocks which contain the object data.

In the context of a particular server, an object can be either local or not, depending on whether all object data are located on the server.

There are 5 kinds of local objects, described in more detail below:

- 3 types of container objects:

  - device container objects;

  - meta-data container objects;

  - data container objects

- objects that contain meta-data data-base tables

- objects that are sets of records of data locations
