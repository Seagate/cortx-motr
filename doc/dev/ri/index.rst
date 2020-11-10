====================
Run-time instruments
====================

:author: Nikita Danilov <nikita.danilov@seagate.com>
:state: CLARIFY
:copyright: Seagate
:distribution: unlimited

:abstract: This document describes run-time instruments for Motr.

Stakeholders
============

.. list-table::
   :header-rows: 1

   * - alias
     - full name
     - email
     - rôle
   * - nikita
     - Nikita Danilov
     - nikita.danilov@seagate.com
     - author, architect

Initiation (INIT)
-----------------

It is proposed to add to motr a mechanism for run-time inspection and modification
of in-memory and persistent state. This mechanism will be used by developers to
analyse bugs and to fix corrupted system state during experiments. The same
mechanism will be used in the field by support personnel or on-site developers
to analyse problems with the system (crashes, performance issues, *etc*.) and to
restore system in case of a catastrophic failure.


Clarification (CLARIFY)
-----------------------

motr and, more generally, CORTX is deployed as a collection of processes running
on multiple nodes in a cluster. Wihin each process there is a number of
sub-system interacting with each other, other processes, network and
storage. Sub-systems create and maintain state in the form of structures in
volatile memory and on persistent store. State is accessed concurrently from
multiple threads.

Analysis of a problem in such distributed and concurrent system is difficult. motr
already provides some mechanisms to help with analysis:

- tracing. lib/trace.h provides a fast tracing module that produces logs
  (m0trace.$PID files) with the record of last few seconds of system activity.
  Tracing is useful for debugging, but it does not allow examination or
  modification of system state;

- addb contains records describing system behaviour. These records are
  cross-referenced in a way that allows tracking execution of top-level request
  (*e.g.*, S3 GET request) through multiple nodes and multiple layers. addb is
  useful for large-scale analysis of system behaviour (performance, workload
  characteristics, efficiency of algorithms, *etc*.).

Neither of these mechanisms is suitable for interactive inspection of
modification of system in a development or customer environment.

The only available mechanisms to fix problems in a deployed system are total
re-initialisation (mkfs with loss of all data) and recovery tool (beck,
extremely heavyweight tool of last resort).

An additional mechanism, called RI (*run-time instruments*) will be introduced
that would allow developers and support people to inspect and fix a system
on-line without bringing it down.

RI would allow its user to do, for example, the following on a live system:

- map S3 object name to motr fid;

- list component objects (cobs) for a particular s3 object;

- map a cob to a server and a device on this server;

- find device blocks allocated to a cob;

- stop processing of all incoming requests in a given motr server process;

- print out the contents of a particular b-tree node;

- update a meta-data record transactionally;

- reset network connection between a given client and a given server;

- repair a particular block in a particular object from parity;

- list all requests queued in the request handler;

- change hare's state for a given service.

RI will be usable in the following situations:

- a developer wants to analyse a crash or experiment with a live system in the
  lab;

- a system fails on a customer site. A support person analyses the live system
  and tries to fix it.

RI should provide multiple access interfaces:

- interactive connection to a local motr process (client or server);

- interactive connection to a remote motr process;

- scriptable access to a local or remote processes.

- RI should be able to inspect and modify state outside of motr (s3, nfs,
  provisioner).


Analysis (ANALYSIS)
-------------------

Possible implementation strategies:

- RI as a service. RI provides an interface that allows components to register
  their instruments. There is a fop-based protocol to invoke instruments. RI
  runs as a normal motr service. In addition, in motr instance there is a
  special dedicated thread, listening for incoming tcp connections, accepting RI
  fops over tcp and processes them. This is for a case when motr instance
  experienced a failure in rpc, networking, request handling, *etc*. RI client
  connects to a motr instance over motr networking or over tcp. Command-line
  client can be scripted. Client can operate on multiple motr instances at once.

- RI as a collection of scripts. RI is implemented as a collection of gdb (or
  dtrace, or system tap) scripts. RI client invokes gdb, attaches to a running
  motr instance and executes scripts.

Requirements (REQS)
-------------------

Architecture (ARCH)
-------------------

Planning (PLAN)
---------------

Execution (EXEC)
----------------

Task requirements (TREQ)
++++++++++++++++++++++++

High-level design intermediate review (HLDIR)
+++++++++++++++++++++++++++++++++++++++++++++

High-level design inspection (HLDINSP)
++++++++++++++++++++++++++++++++++++++

Detailed-level design (DLD)
+++++++++++++++++++++++++++

Detailed-level design intermediate review (DLDIR)
+++++++++++++++++++++++++++++++++++++++++++++++++

Detailed-level design inspection (DLDINSP)
++++++++++++++++++++++++++++++++++++++++++

Code (CODE)
+++++++++++

Code intermediate review (CODEIR)
+++++++++++++++++++++++++++++++++

Dev testing (TEST)
++++++++++++++++++

Code inspection (CODEINSP)
++++++++++++++++++++++++++

Documentation (DOC)
+++++++++++++++++++

Integration (INT)
+++++++++++++++++

QA testing (QA)
+++++++++++++++

Deployment (DEPLOY)
+++++++++++++++++++

Patents (PATENTS)
+++++++++++++++++

Abandoned (ABANDON)
-------------------

