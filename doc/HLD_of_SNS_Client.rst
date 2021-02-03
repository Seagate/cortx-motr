==================================================
High level design of an SNS client module for M0
==================================================

This document presents a high level design (HLD) of an SNS client module from M0. The main purposes of this document are: (i) to be inspected by M0 architects and peer designers to ascertain that high level design is aligned with M0 architecture and other designs, and contains no defects, (ii) to be a source of material for Active Reviews of Intermediate Design (ARID) and detailed level design (DLD) of the same component, (iii) to serve as a design reference document.

The intended audience of this document consists of M0 customers, architects, designers and developers.

**************
Introductiom
**************

SNS client component interacts with Linux kernel VFS and VM to translate user application IO operation requests into IO FOPs sent to the servers according to SNS layout.

************
Definitions
************

The following terms are used to discuss and describe SNS client:

- IO operation is a read or write call issued by a user space application (or any other entity invoking M0 client services, e.g., loop block device driver or NFS server). Truncate is also, technically, IO operation, but truncates are beyond the scope of this document;

- network transfer is a process of transmitting operation code and operation parameters, potentially including data pages, to the corresponding data server, and waiting for server reply. Network transfer is implemented as a synchronous call to M0 rpc service;

- an IO operation consists of (IO) updates, each accessing or modifying file system state on a single server. Updates of the same IO operation are siblings.

*************
Requirements
*************

- [r.sns-client.nocache]: Motr client does not cache data because (i) this simplifies its structure and implementation, (ii) Motr exports block device interface and block device users, including file systems, do caching internally;

- [r.sns-client.norecovery]: recovery from node or network failure is beyond the scope of the present design;

- [R.M0.IO.DIRECT]: direct-IO is supported.

  - For completeness and to avoid confusion, below are listed non-requirements (i.e., things that are out of scope of this specification):
  
- resource management, including distributed cache coherency and locking;

- distributed transactions;

- recovery, replay, resend;

- advanced layout functionality:

  - layout formulae;

  - layouts on file extents;

  - layout revocation;

- meta-data operations, including file creation and deletion;

- security of IO;

- data integrity, fsck;

- availability, NBA.

******************
Design Highlights
******************

The design of M0 client will eventually conform to the Conceptual Design [0]. As indicated in the Requirements section, only a small part of complete conceptual design is considered at the moment. Yet the structure of client code is chosen so that it can be extended in the future to conform to the conceptual design.

**************************
Functional Specification
**************************

External SNS client interfaces are standard Linux file_operations and address_space_operations methods. VFS and VM entry points create fops describing the operation to be performed. The fop is forwarded to the request handler. At the present moment, request handler functionality on a client is trivial, in the future, request handler will be expanded with generic functionality, see Request Handler HLD [2]. A fop passes through a sequence of state transitions, guided by availability of resources (such as memory and, in the future, locks) and layout io engine, and staged in the fop cache until rpcs are formed. Current rpc formation logic is very simple due to no-cache requirement. rpcs are sent to their respective destinations. Once replies are received, fop is destroyed and results are returned to the caller.

************************
Logical Specification
************************

fop builder, NRS and request handler
========================================

A fop, representing IO operation is created at the VFS or VM entry point1. The fop is then passed to the dummy NRS(23), that immediately passes it down to the request handler. The request handler uses file meta-data to identify the layout and calls layout IO engine to proceed with IO operation.

Layout Schema
==============

Layout formula generates a parity de-clustered file layout for a particular file, using file id (fid) as an identifier[2]. See Parity De-clustering Algorithm HLD [3] for details. At the moment, m0t1fs supports single file with fid supplied as a mount option.

Layout IO Engine
==================

Layout IO engine takes as an input layout and a fop (including operation parameters such as file offsets and user data pages). It creates sub-fops3 for individual updates using layout[67] for data[4] , based on pool objects[5]. Sub-fops corresponding to the parity units reference temporarily allocated pages[6], released (under no-cache policy) when processing completes.

RPC
====

Trivial caching policy is used: fops are accumulated[7] in the staging area while IO operation is being processed by the request handler and IO layout engine. Once operation is processed, staged area is un-plugged[8] fops are converted into rpcs[9] and rpcs are transferred to their respective target servers. If IO operation extent is larger than parity group, multiple sibling updates on a given target server are batched together[10].

Conformance
============

- 1[u.FOP] ST

- 2[u.LAYOUT.PARAMETRIZED] ST 

- 3[u.FOP.SNS] ST 

- 4[u.LAYOUT.DATA] ST 

- 5[u.LAYOUT.POOLS] ST 

- 6[u.lib.allocate-page] 

- 7[u.fop.cache.add] 

- 8[u.fop.cache.unplug] 

- 9[u.fop.rpc.to] 

- 10[u.FOP.BATCHING] ST

- [r.sns-client.nocache]: holds per caching policy described in the rpc sub-section.

- [r.sns-client.norecovery]: holds obviously;

- [R.M0.IO.DIRECT]: no-caching and 0-copy for data together implement direct-io.

Dependencies
=============

- layout:

  - [u.LAYOUT.SNS] ST: server network striping can be expressed as a layout

  - [u.LAYOUT.DATA] ST: layouts for data are supported

  - [u.LAYOUT.POOLS] ST: layouts use server and device pools for object allocation, location, and identification

  - [u.LAYOUT.PARAMETRIZED] ST: layouts have parameters that are substituted to perform actual mapping

- fop:

  - [u.fop] ST: M0 uses File Operation Packets (FOPs)

  - [u.fop.rpc.to]: a fop can be serialized into an rpc

  - [u.fop.nrs] ST: FOPs can be used by NRS

  - [u.fop.sns] ST: FOP supports SNS

  - [u.fop.batching] ST: FOPs can be batched in a batch-FOP

  - [u.fop.cache.put]: fops can be cached

  - [u.fop.cache.unplug]: fop cache can be de-staged forcibly

- NRS:

  - [u.nrs] ST: Network Request Scheduler optimizes processing order globally

- misc:

  - [u.io.sns] ST: server network striping is supported
 
  - [u.lib.allocate-page]: page allocation interface is present in the library
