============================================
High level design of the catalogue service
============================================

This document presents a High Level Design (HLD) of the Motr catalogue service. The main purposes of this document are: (i) to be inspected by the Motr architects and peer designers to ascertain that high level design is aligned with Motr architecture and other designs, and contains no defects, (ii) to be a source of material for Active Reviews of Intermediate Design (ARID) and detailed level design (DLD) of the same component, (iii) to serve as a design reference document.

The intended audience of this document consists of Motr customers, architects, designers and developers.

****************
Introduction
****************

Catalogue service (cas) is a Motr service exporting key-value catalogues (indices). Users can access catalogues by sending appropriate fops to an instance of the catalogue service. Externally, a catalogue is a collection of key-value pairs, called records. A user can insert and delete records, lookup records by key and iterate through records in a certain order. A catalogue service does not interpret keys or values, (except that keys are ordered as bit-strings)—semantics are left to users.

Catalogues are used by other Motr sub-systems to store and access meta-data. Distributed meta-data storage is implemented on top of cas.

**************
Definitions
**************

- catalogue: a container for records. A catalogue is explicitly created and deleted by a user and has an identifier, assigned by the user;

- record: a key-value pair;

- key: an arbitrary sequence of bytes, used to identify a record in a catalogue;

- value: an arbitrary sequence of bytes, associated with a key; 

- key order: total order, defined on keys within a given container. Iterating through the container, returns keys in this order. The order is defined as lexicographical order of keys, interpreted as bit-strings.

- user: any Motr component or external application using a cas instance by sending fops to it.


****************
Requirements
****************

- [r.cas.persistency]: modifications to catalogues are stored persistently;

- [r.cas.atomicity]: operations executed as part of particular cas fop are atomic w.r.t. service failures. If the service crashes and restarts, either all or none modifications are visible to the future queries;

- [r.cas.efficiency]: complexity of catalogue query and modification is logarithmic in the number of records in the catalogue;

- [r.cas.vectored]: cas operations are vectored. Multiple records can be queried or updated by the same operation;

- [r.cas.scatter-gather-scatter]: input and output parameters (keys and values) of an operation are provided by the user in arbitrary vectored buffers to avoid data-copy;

- [r.cas.creation]: there is an operation to create and delete a catalogue with user provided identifier;

- [r.cas.identification]: a catalogue is uniquely identified by a non-reusable 120-bit identifier;

- [r.cas.fid]: catalogue identifier, together with a fixed 8-bit prefix form the catalogue fid;

- [r.cas.unique-keys]: record keys are unique within a given catalogue;

- [r.cas.variable-size-keys]: keys with different sizes are supported;

- [r.cas.variable-size-values]: values with different sizes are supported

- [r.cas.locality]: the implementation guarantees that spatial (in key order) together with temporal locality of accesses to the same catalogue is statistically optimal. That is, consecutive access to records with close keys is more efficient than random access;

- [r.cas.cookies]: a service returns to the user an opaque cookie together with every returned record, plus a cookie for a newly inserted record. This cookie is optionally passed by the user (along with the key) to later access the same record. The cookie might speed up the access.
