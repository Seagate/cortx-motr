=======================================
High level design of fop state machine 
=======================================

This document presents a high level design (HLD) of file operation packet (fop) state machine component of Motr M0 core. The main purposes of this document are: (i) to be inspected by M0 architects and peer designers to ascertain that high level design is aligned with M0 architecture and other designs, and contains no defects, (ii) to be a source of material for Active Reviews of Intermediate Design (ARID) and detailed level design (DLD) of the same component, (iii) to serve as a design reference document.

The intended audience of this document consists of M0 customers, architects, designers and developers.

*****************
Introduction
*****************

Motr uses non-blocking (also known as event- or state machine based) processing model on a server (and to a lesser degree on a client) side. The essence of this model is that instead of serving each incoming request on a thread taken from a thread pool and dedicated exclusively to this request for the whole duration or its processing, a larger number of concurrently processed requests is multiplexed over a small number of threads (typically a few threads per processor). 

Whereas in a traditional thread-per-request model a thread blocks when request processing must wait for a certain event (e.g., storage IO completion, availability of a remote resource [3]), non-blocking server, instead, saves the current request processing context and switches to the next ready request. Effectively, the functionality of saving and restoring request state and of switching to the next ready request is conceptually very similar to the functionality of a typical thread scheduler, with the exception that instead of native hardware stacks certain explicit data-structures are used to store computation state. 

This design document describes how these data-structures are organized and maintained.

See [0], [1] and [2] for overview of thread- versus event- based servers.

************
Definitions
************

See [4] and [5] for the description of fop architecture.

- fop state machine (fom) is a state machine [6] that represents current state of the fop's [r.fop]ST execution on a node. fom is associated with the particular fop and implicitly includes this fop as part of its state;

- a fom state transition is executed by a handler thread[r.lib.threads]. The association between the fom and the handler thread is short-living: a different handler thread can be selected to execute next state transition;

*************
Requirements
*************

- [r.non-blocking.few-threads]: Motr service should use a relatively small number of threads: a few per processor [r.lib.processors]. 

- [r.non-blocking.easy]: non-blocking infrastructure should be easy to use and non-intrusive. 

- [r.non-blocking.extensibility]: addition of new "cross-cut" functionality (e.g., logging, reporting) potentially including blocking points and affecting multiple fop types should not require extensive changes to the data-structures for each fop type involved. 

- [r.non-blocking.network]: network communication must not block handler threads. 

- [r.non-blocking.storage]: storage transfers must not block handler threads. 

- [r.non-blocking.resources]: resource acquisition and release [3] must not block handler threads. 

- [r.non-blocking.other-block]: other potentially blocking conditions (page faults, memory allocations, writing trace records, etc.) must never block all service threads.

******************
Design Highlights
******************

A set of data-structures similar to one maintained by a typical thread or process scheduler in an operating system kernel (or a user level library thread package) is used for non-blocking fop processing: prioritized run-queues of fom-s ready for the next state transition and wait-queues of fom-s parked waiting for events to happen.

*************************
Functional Specification
*************************

A fop belongs to a fop type. Similarly, a fom belongs to a fom type. The latter is part of the corresponding fop type. fom type specifies machine states as well as its transition function. A mandatory part of fom state is phase, indicating how far the fop processing progressed. Each fom goes through standard phases, described in [7], as well as some fop-type specific phases.

Fop-type implementation provides an enumeration of non-standard phases and state-transition function for the fom.

A care is taken to guarantee that at least one handler thread is runnable, i.e., not blocked in the kernel at any time. Typically, a state transition is triggered by some event, e.g., an arrival of an incoming fop, availability of a resource, completion of a network or storage communication. When a fom is about to wait for an event to happen, the source of future event is registered with the fom infrastructure. When event happens, the appropriate state transition function is invoked.

***********************
Logical Specification
***********************

Locality
==========

For the purposes of the present design, server computational resources are partitioned into localities. A typical locality includes a sub-set of available processors [r.lib.cores] and a collection of allocated memory areas[r.lib.memory-partitioning]. fom scheduling algorithm tries to confine processing of a particular fom to a specific locality (called a home locality of the fom) establishing affinity of resources and optimizing cache hierarchy utilization. For example, inclusion of all cores sharing processor caches in the same locality, allows fom to be processed on any of said cores without incurring a penalty of cache misses. 

Run-queue
==========

A run-queue is a per-locality list of fom-s ready for a next state transition. A fom is placed into a run-queue in the following situations:

- when the fom is initially created for incoming fop. Selection of a locality to which the fom is assigned is a subtle question: 

  - locality of reference: it is advantageous to bind objects which fom-s manipulate to localities. E.g., by processing all requests for operations in a particular directory to the same locality, processor cache utilization can be improved; 

  - load balancing: it is also advantageous to avoid overloading some localities while others are underloaded; 

- when an event occurs that triggers next state transition for the fom. At the event occurrence, the fom is moved from a wait-queue to the run-queue of its home locality.

A run-queue is maintained in the FIFO order. 









