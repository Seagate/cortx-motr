============================================
High level design of a file operations log
============================================

This document presents a high level design (HLD) of a file operations log (a fol) of Motr M0 core. The main purposes of this document are: (i) to be inspected by M0 architects and peer designers to ascertain that high level design is aligned with M0 architecture and other designs, and contains no defects, (ii) to be a source of material for Active Reviews of Intermediate Design (ARID) and detailed level design (DLD) of the same component, (iii) to serve as a design reference document.

The intended audience of this document consists of M0 customers, architects, designers and developers.

*************
Introduction
*************

A fol is a central M0 data-structure, maintained by every node where M0 core is deployed and serving multiple goals:

- it is used by a node data-base component to implement local transactions through WAL logging;

- it is used by DTM to implement distributed transactions. DTM uses fol for multiple purposes internally:

  - on a node where a transaction originates: for replay;

  - on a node where a transaction update is executed: for undo, redo and for replay (sending redo requests to recovering nodes);

  - to determine when a transaction becomes stable;

- it is used by a cache pressure handler to determine what cached updates have to be re-integrated into upward caches;

- it is used by FDML to feed file system updates to fol consumers asynchronously;

- more generally, a fol is used by various components (snapshots, addb, etc.) to consistently reconstruct the system state as at a certain moment in the (logical) past.

Roughly speaking, a fol is a partially ordered collection of fol records, each corresponding to (part of) a consistent modification of file system state. A fol record contains information determining durability of the modification (how many volatile and persistent copies it has and where, etc.) and dependencies between modifications, among other things. When a client node has to modify a file system state to serve a system call from a user, it places a record it its (possibly volatile) fol. The record keeps track of operation state: has it been re-integrated to servers, has it been committed on the servers, etc. A server, on receiving a request to execute an update on a client behalf, inserts a record, describing the request into its fol. Eventually, fol is purged to reclaim storage, culling some of the records.

*************
Definitions
*************

- a (file system) operation is a modification of a file system state preserving file system consistency (i.e., when applied to a file system in a consistent state it produces consistent state). There is a limited repertoire of operation types: mkdir, link, create, write, truncate, etc. M0 core maintains serializability of operation execution;

- an update (of an operation) is a sub-modification of a file system state that modifies state on a single node only. For example, a typical write operation against a RAID-6 striped file includes updates which modify data blocks on a server A and updates which modify parity blocks on a server B;

- an operation or update undo is a reversal of state modification, restoring original state. An operation can be undone only when the parts of state it modifies are compatible with the operation having been executed. Similarly, an operation or update redo is modifying state in the "forward" direction, possibly after undo;

- the recovery is a distributed process of restoring file system consistency after a failure. M0 core implements recovery in terms of undoing and redoing individual updates in a coherent way;

- an update (or more generally, a piece of a file system state) is persistent when it is recorded on a persistent storage, where persistent storage is defined as one whose contents survives a reset;

- an operation (or more generally, a piece of a file system state) is durable when it has enough persistent copies to survive any failure. Note that even as a record of a durable operation exists after a failure, the recovery might decide to undo the operation to preserve overall system consistency. Also note that the notion of failure is configuration dependent;

- an operation is stable when it is guaranteed that the system state would be consistent with this operation having been executed. A stable operation is always durable. Additionally, M0 core guarantees that the recovery would never undo the operation. The system can renege stability guarantee only in the face of a catastrophe or a system failure;

- updates U and V are conflicting or non-commutative if the final system state after U and W are executed depends on the relative order of their execution (note that system state includes information and result codes returned to the user applications); otherwise the updates are commutative. The later of two non-commutative updates depends on the earlier one. The earlier one is a pre-requisite of a later one (this ordering is well-defined due to serializability);

- to maintain operation serializability, all conflicting updates of a given file system object must be serialized. An object version is a number associated with a storage object with a property that for any two conflicting updates U (a pre-requisite) and V (a dependent update) modifying the object, the object version at the moment of U execution is less than at the moment of V execution;

- a FOL of a node is a sequence of records, each describing an update carried out on the node, together with information identifying operations these updates are parts of, file system objects that were updated and their versions, and containing enough data to undo or redo the updates and to determine operation dependencies;

- a record in a node fol is uniquely identified by a log sequence number (lsn). Log sequence numbers have two crucial properties:

  - a fol record can be found efficiently (i.e., without fol scanning) given its lsn, and

  - for any pair of conflicting updates recorded in the fol, the lsn of the pre-requisite is less than that of the dependent update (Note: clearly this property implies that lsn     data-type has infinite range and, hence, is unimplementable in practice. What is in fact required is that this property holds for any two conflicting updates sufficiently     close in logical time, where precise closeness condition is defined by the fol pruning algorithm. The same applies to object versions.);

