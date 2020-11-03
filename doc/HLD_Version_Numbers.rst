======================================
High Level Design of Version Numbers
======================================

This document presents a high level design (HLD) of version numbers in Motr M0 core. The main purposes of this document are: (i) to be inspected by M0 architects and peer designers to ascertain that high level design is aligned with M0 architecture and other designs, and contains no defects, (ii) to be a source of material for Active Reviews of Intermediate Design (ARID) and detailed level design (DLD) of the same component, (iii) to serve as a design reference document.

***************
Introduction
*************** 

Version numbers identify particular file system states and order updates thereof. They are used for distributed transaction management, concurrency control (both lock based and optimistic) and object history navigation.

A version number is stored together with the file system state whose version it identifies. Specifically, multiple cached replicas of the same state are all tagged with version numbers matching each other in the sense defined below. Version numbers are treated as a distributed resource. A piece of file system state to which a version number is assigned is called a unit. Unit granularity can be a file system object or a part of a file system object. 

***************
Definitions
*************** 

See the Glossary for general M0 definitions and HLD of FOL for the definitions of file system operation, update and lsn. The following additional definitions are required:

- for the purposes of the present design it is assumed that a file system update acts on units1. For example, a typical meta-data update acts on one or more "inodes" and a typical data update acts on inodes and data blocks. Inodes, data blocks, directory entries, etc. are all examples of units. It is further assumed that units involved in an update are unambiguously identified2 and that complete file system state is a disjoint union of states of comprising units. (Of course, there are consistency relationships between units, e.g., inode nlink counter must be consistent with contents of directories in the name-space).

- It is guaranteed that operations (updates and queries) against a given unit are serializable3 in the face of concurrent requests issued by the file system users. This means that observable (through query requests) unit state looks as if updates of the unit were executed serially in some order. Note that the ordering of updates is further constrained by the distributed transaction management considerations which are outside the scope of this document.

- A unit version number is an additional piece of information attached to the unit. A version number is drawn from some linearly ordered domain. A version number changes on every update of the unit state in such a way that ordering of unit states in the serial history can be deduced by comparing version numbers associated with the corresponding states.    

***************
Requirements
***************

- [r.verno.serial]: version numbers order unit states in the unit serial history; 

- [r.verno.resource]: version numbers are managed as a scalable distributed resource3: entities caching a given unit could also cache4 some range of version numbers and to generate version numbers for updates of local cached state locally; 

- [r.verno.dtm]: version numbers are usable for distributed transaction5 recovery: when cached update is replayed to restore lost unit state, unit version number is used to determine whether update is already present in the survived unit state; 
 
- [r.verno.optimistic-concurrency]: version numbers are usable for optimistic concurrency control in the spirit of NAMOS [1]; 

- [r.verno.update-streams]: version numbers are usable for implementation of update streams6 (see On file versions [0]); 

- [r.verno.fol]: a unit version number identifies the fol record that brought the unit into the state corresponding to the version number.

******************
Design Highlights
******************

In the presence of caching, requirements [r.verno.resource] and [r.verno.fol] are seemingly contradictory: if two caching client nodes assigned (as allowed by [r.verno.resource]) version numbers to two independent units, then after re-integration of units to their common master server, the version numbers must refer to the master's fol, but clients cannot produce such references without extremely inefficient serialization of all accesses to the units on the server. 

To deal with that, a version number is made compound: it consists of two components: 

- LSN7: a reference to a fol record, corresponding to the unit state;

- VC: a version counter, which is an ordinal number of update in the unit's serial history.

When a unit state is updated, a new version number is produced, with lsn referring to the fol local to the node where update is made. When unit state is re-integrated to another node, lsn part of version number is replaced with a reference to target node fol, resulting in a compound version number who's lsn matches the target node's fol. The vc remains unchanged; a monotonically increasing unit history independent of any node-specific relationship.  The unit state can be traced through the vc, the fol references can be traced through the lsn. See [0] for additional recovery related advantages of compound version numbers. 

Note that introduction of compound version numbers does not, by itself, require additional indices for fol records, because whenever one wants to refer to a particular unit version from some persistent of volatile data-structure (e.g., a snapshot of file-set descriptor), one uses a compound version number. This version number contains lsn, that can be used to locate required fol entry efficiently. This means that no additional indexing of fol records (e.g., indexing by vc) is needed. The before-version-numbers, stored in the fol record, provide for navigation of unit history in the direction of the things past.

************************
Functional Specification
************************

Other sub-systems use version numbers in a number of ways:

- [a version number in a fol record] a fol record stores version numbers8 that units modified by the update had before the update was applied. These are called before-version-numbers (similarly to a before-image stored by data-bases in a transactional log). The version number a unit would have after the update is applied (called after-version-number) can be constructed: its lsn is the lsn of the fol record in question and its version counter is one more than the version counter of before-version. The lsn parts of before-version-numbers constitute prev-lsn references as defined by the FOL HLD;

- [a version number update protocol] a unit has version number of its latest state as an attribute9. This attribute is modified with every update to the unit state. lsn part of this version number points to the fol record with the last update for the unit. When new update is performed, new fol record is transactionally10 appended to the log, pointing to the previous record through last-lsn, and in-unit version number is changed to point to the new record. Effectively, this maintains a single-linked list of records updating a given unit, except that a single record can belong to multiple lists. For example, initial configuration:  

.. image:: Images/rev1.PNG

becomes after open("foo") call: 

.. image:: Images/rev2.PNG

- [a version number applicability check] distributed transaction manager uses version numbers for recovery: when an update is replayed on a node, a modification to an unit should be replayed if and only if before-version shipped with an update is the same as before-version stored in the unit;

- [a version number in an update stream] cache re-integration uses version numbers to implement update streams. For each update stream between a client node C and a server node S, a special stream unit U is created. This unit is persistently stored on S and cached on C. Each update cached on C and targeted for re-integration on S modifies U. Therefore, each update sent from C to S contains U's version counter which is incremented by each update. This counter acts like a xid of the traditional Lustre protocol, and (U, U.VC) pair acts as a (session, slot) pair of the NFSv4 session protocol. The collection of these special units for all update streams is similar to last_rcvd file of Lustre and to EOS cache of NFSv4. 


 
