=======================================================
High level design of an SNS server server for Motr
=======================================================

This document presents a high level design (HLD) of an SNS server module from Motr. The main purposes of this document are: (i) to be inspected by M0 architects and peer designers to ascertain that high level design is aligned with M0 architecture and other designs, and contains no defects, (ii) to be a source of material for Active Reviews of Intermediate Design (ARID) and detailed level design (DLD) of the same component, (iii) to serve as a design reference document.

The intended audience of this document consists of M0 customers, architects, designers and developers.

*************
Introduction
*************

See companion SNS client high level design specification [4].

*************
Definitions
*************

Definitions from the Parity De-clustering Algorithm HLD [3] and SNS client HLD [4] are incorporated by reference.

**************
Requirements
**************

- [r.sns-server.0-copy]: read and write IO operations with sufficiently large and properly aligned buffers incur no data copying in the complete IO path;

- [r.sns-server.conceptual-design]: sns server code is designed to be upward compatible with the M0 core conceptual design [5].

Refer to the SNS client HLD [4] for the list of topics currently omitted from SNS.

*******************
Design Highlights
*******************

The design of M0 server will eventually conform to the Conceptual Design [0]. As indicated in the Requirements section, only a small part of complete conceptual design is considered at the moment. Yet the structure of client code is chosen so that it can be extended in the future to conform to the conceptual design.

An important feature of proposed design is a concurrency model based on few threads and asynchronous request execution.

**************************
Functional Specification
**************************

Server SNS component externally interacts with rpc sub-system and storage objects. A fop is constructed by fop builder from incoming rpc data. The fop is queued to the network request scheduler. A trivial scheduling policy is currently implemented: a queued fop is immediately forwarded to the request handler.

Request handler issues asynchronous storage transfer request. On completion, reply fop is constructed and queued. In the current design, replies are immediately sent back to client.

***********************
Logical Specification
***********************

FOP Builder and NRS
======================

A fop [1], representing IO operation is de-serialized from an incoming RPC [2]. The fop [3] is then passed to the dummy NRS [4] , that immediately passes it down to the request handler. The request handler uses file meta-data to identify the layout and calls layout IO engine to proceed with IO operation.

Request Handler
=================

Request handler uses a small pool of threads (e.g., a thread per processor) to handle incoming fops. A fop is not bound to a thread. Instead, fop executes on an available thread until it has to wait (e.g., to block waiting for storage transfer to complete). At that moment, fop places itself into a request handler supplied wait queue and de-schedules itself, freeing request handler thread for processing of another fop.

When event for which the fop is waiting happens, request handler retrieves the fop from the waiting queue and schedules it on some of its threads again. Roughly speaking, the request handler implements a logic of a simple thread scheduler, using a small number of threads as processors to run fop execution on.

RPC
====

Rpc uses rdma to transfer data pages. Trivial reply caching policy is used: a reply fop is immediately serialized into rpc [5] and sent to the client.

Conformance
============

- [r.sns-server.0-copy]: use of rdma by rpc layer plus direct-io used by stob adieu provide 0-copy data path;

- [r.sns-server.conceptual-design]: the code structure outlines in the functional and logical specifications above can be easily extended to include other conceptual design components: resource framework, FOL, memory pressure handler, etc.

Dependencies
==============

- fop:

  - [u.fop] ST: M0 uses File Operation Packets (FOPs)

  - [u.fop.rpc.from]: a fop can be buil

  - [u.fop.nrs] ST: FOPs can be used by NRS

  - [u.fop.sns] ST: FOP supports SNS

- NRS:

  - [u.nrs] ST: Network Request Scheduler optimizes processing order globally

Security Model
===============

Security is outside of scope of the present design.

State
======

State diagrams are part of the detailed level design specification.



