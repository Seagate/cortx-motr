==================================
HLD Resource Management Interface
==================================

This document presents a high level design (HLD) of scalable resource management interfaces for Mero. The main purposes of this document are: (i) to be inspected by M0 architects and peer designers to ascertain that high level design is aligned with M0 architecture and other designs, and contains no defects, (ii) to be a source of material for Active Reviews of Intermediate Design (ARID) and detailed level design (DLD) of the same component, (iii) to serve as a design reference document.

The intended audience of this document consists of M0 customers, architects, designers and developers.

***************
Introduction
***************

Motr functionality, both internal and external, is often specified in terms of resources. A resource is part of system or its environment for which a notion of ownership is well-defined.

***************
Definitions
***************

- A resource is part of system or its environment for which a notion of ownership is well-defined. Resource ownership is used for two purposes:

  - concurrency control. Only resource owner can manipulate the resource and ownership transfer protocol assures that owners do not step on each other. That is, resources provide traditional distributed locking mechanism;

  - replication control. Resource owner can create a (local) copy of a resource. The ownership transfer protocol with the help of version numbers guarantees that multiple replicas are re-integrated correctly. That is, resources provide a cache coherency mechanism. Global cluster-wide cache management policy can be implemented on top of resources.

- A resource owner uses the resource via a usage credit (also called resource credit or simply credit as context permits). E.g., a client might have a credit of a read-only or write-only or read-write access to a certain extent in a file. An owner is granted a credit to use a resource.

- A usage credit granted to an owner is held (or pinned) when its existence is necessary for the correctness of ongoing resource usage. For example, a lock on a data extent must be held while IO operation is going on and a meta-data lock on a directory must be held while a new file is created in the directory. Otherwise, the granted credit is cached.

- A resource belongs to a specific resource type, which determines resource semantics.

- A conflict occurs at an attempt to use a resource with a credit incompatible with already granted credit. Conflicts are resolved by a conflict resolution policy specific to the resource type in question.

- To acquire a resource usage credit, a prospective owner enqueues a resource acquisition request to a resource owner.

- An owner can relinquish its usage credits by sending a resource cancel request to another resource owner, which assumes relinquished credits.

- A usage credit can be associated with a lease, which is a time interval for which the credit is granted. The usage credit automatically cancels at the end of the lease. A lease can be renewed.

- One possible conflict resolution policy would revoke all already granted conflicting credits before granting the new credit. Revocation is effected by sending conflict call-backs to the credits owners. The owners are expected to react by cancelling their cached credits.

