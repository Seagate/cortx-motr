=====================================================
High level design of the distributed indexing
=====================================================

This document presents a high level design (HLD) of the Motr distributed indexing. The main purposes of this document are: (i) to be inspected by the Motr architects and peer designers to ascertain that high level design is aligned with Motr architecture and other designs, and contains no defects, (ii) to be a source of material for Active Reviews of Intermediate Design (ARID) and detailed level design (DLD) of the same component, (iii) to serve as a design reference document.

The intended audience of this document consists of Motr customers, architects, designers and developers.

**************
Introduction
**************

Distributed indexing is a Motr component that provides key-value indices distributed over multiple storage devices and network nodes in the cluster for performance, scalability and fault tolerance.

Distributed indices are exported via interface and provide applications with a method to store application-level meta-data. Distributed indices are also used internally by Motr to store certain internal meta-data. Distributed indexing is implemented on top of "non-distributed indexing" provided by the catalogue service (cas).

************
Definitions
************

- definitions from the high level design of catalogue service are included: catalogue, identifier, record, key, value, key order, user; 

- definitions from the high level design of parity de-clustered algorithm are included: pool, P, N, K, spare slot, failure vector;

- a distributed index, or simply index is an ordered container of key-value records;

- a component catalogue of a distributed index is a non-distributed catalogue of key-value record, provided by the catalogue service, in which the records of the distributed index are stored.

**************
Requirements
**************

- [r.idx.entity]: an index is a Clovis entity with a fid. There is a fid type for distributed indices;

- [r.idx.layout]: an index has a layout attribute, which determines how the index is stored in non-distributed catalogues;

- [r.idx.pdclust]: an index is stored according to a parity de-clustered layout with N = 1, i.e., some form of replication. The existing parity de-clustered code is re-used;

- [r.idx.hash]: partition of index records into parity groups is done via key hashing. The hash of a key determines the parity group (in the sense of parity de-clustered layout algorithm) and, therefore, the location of all replicas and spare spaces;

- [r.idx.hash-tune]: the layout of an index can specify one of the pre-determined hash functions and specify the part of the key used as the input for the hash function. This provides an application with some degree of control over locality;

- [r.idx.cas]: indices are built on top of catalogues and use appropriately extended catalogue service;

- [r.idx.repair]: distributed indexing sub-system has a mechanism of background repair. In case of a permanent storage failure, index repair restores redundancy by generating more replicas in spare space;

- [r.idx.re-balance]: distributed indexing sub-system has a mechanism of background re-balance. When a replacement hardware element (a device, a node, a rack) in added to the system, re-balance copies appropriate replicas from the spare space to the replacement unit;

- [r.idx.repair.reuse]: index repair and re-balance, if possible, are built on top of copy machine abstraction used by the SNS repair and re-balance;

- [r.idx.degraded-mode]: access to indices is possible during repair and re-balance;

- [r.idx.root-index]: a root index is provided, which has a known built-in layout and, hence, can be accessed without learning its layout first. The root index is stored in a pre-determined pool, specified in the configuration data-base. The root index contains a small number of global records;

- [r.idx.layout-index]: a layout index is provided, which contains (key: index-fid, value: index-layout-id) records for all indices except the root index, itself and other indices mentioned in the root index. The layout of the layout index is stored in the root index. Multiple indices can use the same layout-id;

- [r.idx.layout-descr]: a layout descriptor index is provided, which contains (key: index-layout-id, value: index-layout-descriptor) records for all indices;

Relevant requirements from the Motr Summary Requirements table:

- [r.m0.cmd]: clustered meta-data are supported;

- [r.m0.layout.layid]: a layout is uniquely identified by a layout id (layid);

- [r.m0.layout.meta-data]: layouts for meta-data are supported.

