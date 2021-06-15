Motr Reading List
=================

For non-hyperlinked documentation links, please refer to this file : [doc/motr-design-doc-list.rst](motr-design-doc-list.rst).

* this document
* Motr data organization
* Motr architecture 2-pager
* Summary of M0 architecture
   * [Motr in Prose](motr-in-prose.md)
* M0 Architecture Documentation
* FAQ
* Glossary

Motr Clients
------------
[Client API](../motr/client.h) is the direct interface to Motr; our [developer guide](motr-developer-guide.md) shows how to build client applications using this API.  [m0cp](../motr/st/utils/copy.c), [m0cat](../motr/st/utils/cat.c), and [m0kv](../motr/m0kv) are example applications using this API.  In our CORTX parent repo, the [Cluster Setup guide](https://github.com/Seagate/cortx/blob/main/doc/Cluster_Setup.md) has useful information about using these tools.

[Go bindings](../bindings/go) allow to write Motr client applications in Go language quickly and efficiently.

[HSM Tool](../hsm) reference application utilizes Motr's Composite Objects Layout API, which allows to build Hierarchical Storage Management (HSM) in a multiple pools/tiers configuration clusters.

Containers
----------

* containers 1-pager

Resource Management
-------------------

* 1-pager
* High level design of resource management interfaces

DTM
---

* 1-pager
* overview
* High level design of version numbers
* [Jim Gray's publications](http://research.microsoft.com/en-us/um/people/gray/)
* [Leslie Lamport's publications](http://research.microsoft.com/en-us/um/people/lamport/pubs/pubs.html)
    - [The Part-Time Parliament (Paxos)](http://research.microsoft.com/en-us/um/people/lamport/pubs/pubs.html#lamport-paxos)
    - [Paxos made simple](http://research.microsoft.com/en-us/um/people/lamport/pubs/pubs.html#paxos-simple)
    - [Time, Clocks and the Ordering of Events in a Distributed System](http://research.microsoft.com/en-us/um/people/lamport/pubs/pubs.html#time-clocks)
* [Aries: A transaction recovery method supporting fine-granularity locking and partial rollbacks using write-ahead logging](http://citeseerx.ist.psu.edu/viewdoc/summary?doi=10.1.1.109.2480)
* Echo File System
    - [The Echo distributed file system](http://www.hpl.hp.com/techreports/Compaq-DEC/SRC-RR-111.html)
    - [Availability in the echo file system](http://www.hpl.hp.com/techreports/Compaq-DEC/SRC-RR-112.html)
    - [New value logging in the Echo replicated file system](http://www.hpl.hp.com/techreports/Compaq-DEC/SRC-RR-104.html)
* [Reliable Communication in the Presence of Failures](http://ksuseer1.ist.psu.edu/viewdoc/summary?doi=10.1.1.106.6258)
* [The Two-Phase Commit Protocol](http://ei.cs.vt.edu/~cs5204/sp99/distributedDBMS/duckett/tpcp.html)
* Paxos overview

Request Handler
---------------

* request handler 1-pager
* High level design of fop state machine
* High level design of M0 request handler
* Non-blocking server and locality of reference

IO
--

* sns 1-pager
* sns overview
* High level design of a parity de-clustering algorithm

Layouts
-------

* On layouts

MD-Store
--------

* NA

RPC
---

* AR of rpc layer

Function Shipping
---

* Function shipping (In-storage-compute) usage in motr: [PDF](PDF/motr_function_shipping.pdf).


Network
-------

* [LNET: Lustre Networking](http://wiki.lustre.org/lid/ulfi/ulfi_lnet.html)


ADDB
----

* addb 1-pager

Concurrency
-----------

* [Multi-core programming](http://www.cl.cam.ac.uk/~mgk25/u../teaching/1112/R204/slides-tharris.pdf)
* _The art of multiprocess programming_, M. Herlihy, N. Shavit.

Other
-----

* [Network File System (NFS) Version 4 Minor Version 1 Protocol](http://tools.ietf.org/html/rfc5661)
