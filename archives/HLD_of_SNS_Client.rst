==================================================
High-Level Design of an SNS Client Module for M0
==================================================

This document provides a High-Level Design (HLD) of an SNS Client Module from M0. The main purposes of this document are:

- To be inspected by M0 architects and peer designers to ensure that HLD is aligned with M0 architecture and other designs and contains no defects

- To be a source of material for Active Reviews of Intermediate Design (ARID) and Detailed Level Design (DLD) of the same component

- To be served as a design reference document

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

- [r.sns-client.nocache]: Motr client does not cache data because:

  - This simplifies its structure and implementation
  
  - Motr exports block device interface and block device users, including file systems, do caching internally.

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
=====

Trivial caching policy is used: fops are accumulated[7] in the staging area while IO operation is being processed by the request handler and IO layout engine. Once operation is processed, staged area is un-plugged[8] fops are converted into rpcs[9] and rpcs are transferred to their respective target servers. If IO operation extent is larger than parity group, multiple sibling updates on a given target server are batched together[10].

 Conformance
=============

- 1[u.fop] st

- 2[u.layout.parametrized] st 

- 3[u.fop.sns] st 

- 4[u.layout.data] st 

- 5[u.layout.pools] st 

- 6[u.lib.allocate-page] 

- 7[u.fop.cache.add] 

- 8[u.fop.cache.unplug] 

- 9[u.fop.rpc.to] 

- 10[u.fop.batching] st

- [r.sns-client.nocache]: holds per caching policy described in the rpc sub-section.

- [r.sns-client.norecovery]: holds obviously;

- [r.m0.io.direct]: no-caching and 0-copy for data together implement direct-io.

 Dependencies
===============

- layout:

  - [u.layout.sns] st: server network striping can be expressed as a layout

  - [u.layout.data] st: layouts for data are supported

  - [u.layout.pools] st: layouts use server and device pools for object allocation, location, and identification

  - [u.layout.parametrized] st: layouts have parameters that are substituted to perform actual mapping

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
  
  Security Model
================
 
 Security is outside of scope of the present design.
 
 Refinement
============
 
 Detailed level design specification should address the following:
 
 - concurrency control and liveness rules for fops and layouts;

 - data structures for mapping between layout and target objects in the pool;

 - instantiation of a layout formula;

 - relationship between fop and its sub-fops: concurrency control, liveness, ownership
 
********
 State
********

State diagrams are part of the detailed level design specification.

***********
 Use Cases
***********

 Scenarios
===========

+------------------------------+--------------------------------------------------+
|Scenario                      |[usecase.sns-client-read]                         |
+------------------------------+--------------------------------------------------+
|Relevant quality attributes   | usability                                        |
+------------------------------+--------------------------------------------------+
|Stimulus                      |an incoming read operation request from a user    |
|                              |space operation                                   |
+------------------------------+--------------------------------------------------+
|Stimulus source               |a user space application, potentially meditated by| 
|                              |a loop-back device driver                         |
+------------------------------+--------------------------------------------------+
|Environment                   |normal client operation                           |
+------------------------------+--------------------------------------------------+
|Artifact                      |call to VFS ->read() entry point                  |
+------------------------------+--------------------------------------------------+
|Response                      |a fop is created, network transmission of         |
|                              |operation parameters to all involved data servers |
|                              |is started as specified by the file layout,       |
|                              |servers place retrieved data directly in user     |
|                              |buffers, once transmission completes, the fop is  |
|                              |destroyed.                                        |
+------------------------------+--------------------------------------------------+
|Response measure              |no data copying in the process                    |
+------------------------------+--------------------------------------------------+


+------------------------------+--------------------------------------------------+
|Scenario                      |[usecase.sns-client-write]                        |
+------------------------------+--------------------------------------------------+
|Relevant quality attributes   | usability                                        |
+------------------------------+--------------------------------------------------+
|Stimulus                      |an incoming write operation request from a user   |
|                              |space operation                                   |
+------------------------------+--------------------------------------------------+
|Stimulus source               |a user space application, potentially meditated by| 
|                              |a loop-back device driver                         |
+------------------------------+--------------------------------------------------+
|Environment                   |normal client operation                           |
+------------------------------+--------------------------------------------------+
|Artifact                      |call to VFS ->write() entry point                 |
+------------------------------+--------------------------------------------------+
|Response                      |a fop is created, network transmission of         |
|                              |operation parameters to all involved data servers |
|                              |is started as specified by the file layout,       |
|                              |servers place retrieved data directly in user     |
|                              |buffers, once transmission completes, the fop is  |
|                              |destroyed.                                        |
+------------------------------+--------------------------------------------------+
|Response measure              |no data copying in the process                    |
+------------------------------+--------------------------------------------------+


**********
 Analysis
**********

 Scalability
=============

No scalability issues are expected in this component. Relatively little resources (processor cycles, memory) are consumed per byte of processed data. With large number of concurrent IO operation requests, scalability of layout, pool and fop data structures might become a bottleneck (in the case of small file IO initially).

*************
 Deployment
*************

 Compatability
===============

 Network
---------

No issues at this point.

 Persistent storage
--------------------

The design is not concerned with persistent storage manipulation.

 Core
------

No issues at this point. No additional external interfaces are introduced.

 Installation
================

The SNS client module is a part of m0t1fs.ko kernel module and requires no additional installation. System testing scripts in m0t1fs/st must be updated.

************
 References
************

- [0] Outline of M0 core conceptual design 

- [1] Summary requirements table 

- [2] Request Handler HLD 

- [3] Parity De-clustering Algorithm HLD 

- [4] High level design inspection trail of SNS client 

- [5] SNS server HLD
