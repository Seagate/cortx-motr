====================================================
High level design of Motr configuration caching 
====================================================

This document presents a high level design (HLD) of Motr configuration caching. The main purposes of this document are: (i) to be inspected by M0 architects and peer designers to ascertain that high level design is aligned with M0 architecture and other designs, and contains no defects, (ii) to be a source of material for Active Reviews of Intermediate Design (ARID) and detailed level design (DLD) of the same component, (iii) to serve as a design reference document. 

The intended audience of this document consists of M0 customers, architects, designers and developers. 

*************
Introduction
*************

Configuration information of a Motr cluster (node data, device data, filesystem tuning parameters, etc.[1]) is stored in a database, which is maintained by a dedicated management service --- configuration server.  Other services and clients access configuration information by using API of configuration client library.

Configuration caching provides a higher level interface to the configuration database, convenient to use by upper layers. The implementation maintains data structures in memory, fetching them from the configuration server if necessary. Configuration caches are maintained in management-, metadata- and io- services, and in clients. 

*************
Definitions
************* 

- Motr configuration is part of M0 cluster meta-data. 

- Configuration database is a central repository of M0 configuration. 

- Confd (configuration server) is a management service that provides configuration clients with information obtained from configuration database. 

- Confc (configuration client library, configuration client) is a library that provides configuration consumers with interfaces to query M0 configuration. 

- Configuration consumer is any software that uses confc API to access M0 configuration. 

- Configuration cache is configuration data stored in node’s memory. Confc library maintains such a cache and provides configuration consumers with access to its data. Confd also uses configuration cache for faster retrieval of information requested by configuration clients. 

- Configuration object is a data structure that contains configuration information. There are several types of configuration objects: profile, service, node, etc. 

*****************
Requirements
*****************

- [r.conf.async]  Configuration querying should be a non-blocking operation. 

- [r.conf.cache]  Configuration information is stored (cached) in management-, metadata-, and io- services, and on clients. 

- [r.conf.cache.in-memory]  Implementation maintains configuration structures in memory, fetching them from the management service if necessary. 

- [r.conf.cache.resource-management]  Configuration caches should be integrated with the resource manager[4].

*******************
Design Highlights
*******************

This design assumes Motr configuration to be read-only. Implementation of writable configuration is postponed. 
 
A typical use case is when a client (confc) requests configuration from a server (confd). The latter most likely already has all configuration in memory: even large configuration data base is very small compared with other meta-data.

Simplistic variant of configuration server always load the entire database into the cache.  This considerably simplifies the locking model (reader-writer) and configuration request processing. 

*************************
Functional Specification
*************************

Configuration of a Motr cluster is stored in the configuration database. Motr services and filesystem clients --- configuration consumers --- have no access to this database. In order to work with configuration information, they use API and data structures provided by confc library. 

Confc's obtain configuration data from configuration server (confd); only the latter is supposed to work with configuration database directly.

Configuration consumer accesses configuration information, which is stored in the configuration cache. The cache is maintained by confc library. If the data needed by a consumer is not cached, confc will fetch this data from confd. 

Confd has its own configuration cache. If the data requested by a confc is missing from this cache, confd gets the information from the configuration database.

Configuration Data Model
==============================

Configuration database consists of tables, each table being a set of {key, value} pairs. The schema of configuration database is documented in [2]. Confd and confc organize configuration data as a directed acyclic graph (DAG) with vertices being configuration objects and edges being relations between objects.

Profile object is the root of configuration data provided by confc. To access other configuration objects, a consumer follows the links (relations), “descending” from the profile object. 

 ::

  profile 

    \_ filesystem 

        \_ service 

           \  _ node 

              \_ nic 

                 \_ storage device 

                    \_ partition 

Relation is a pointer from one configuration object to another configuration object or to a collection of objects. In the former case it is one-to-one relation, in the latter case it is one-to-many. 

Some relations are explicitly specified in corresponding records of the configuration database (e.g., a record of “profiles” table contains name of the filesystem associated with this profile).  Other relations are deduced by the confd (e.g., the list of services that belong given filesystem is obtained by scanning “services” table and selecting entries with particular value of ‘filesystem’ field).

+-----------------------------------+--------------------------------------------------+-----------------------------------------------+
|Configuration object               |Relations specified in the DB record              |Relations deduced by scanning other DB tables  |
+===================================+==================================================+===============================================+
|profile                            |.filesystem                                       |--                                             |
+-----------------------------------+--------------------------------------------------+-----------------------------------------------+
|filesystem                         |--                                                |.services                                      |
+-----------------------------------+--------------------------------------------------+-----------------------------------------------+
|service                            |.filesystem,  .node                               |--                                             |
+-----------------------------------+--------------------------------------------------+-----------------------------------------------+
|node                               |--                                                |.services,  .nics,  .sdevs                     |
+-----------------------------------+--------------------------------------------------+-----------------------------------------------+
|nic                                |.node                                             |--                                             |
+-----------------------------------+--------------------------------------------------+-----------------------------------------------+
|storage device (sdev)              |.node                                             |.partitions                                    |
+-----------------------------------+--------------------------------------------------+-----------------------------------------------+
|partition                          |.storage_device                                   |--                                             |
+-----------------------------------+--------------------------------------------------+-----------------------------------------------+

Relation is a downlink if its destination is located further from the root of configuration DAG than the origin. Relation is an uplink if its destination is closer to the root than the origin. Configuration object is a stub if its status (.*_obj.co_status subfield) is not equal to M0_CS_READY. Stubs contain no meaningful configuration data apart from object’s type and key. Configuration object is pinned if its reference counter (.*_obj.co_nrefs subfield) is non-zero.  When a configuration consumer wants to use an object, it pins it in order protect existence of the object in the cache. Pinning of an object makes confc library request a corresponding distributed lock (resource) from the resource manager. 

Path
=====

Imagine a sequence of downlinks and keys for which the following is true: 

- the first element (if any) is a downlink; 

- a one-to-many downlink is either followed by a key or is the last element; 

- a key is preceded by a one-to-many downlink. 

Such a sequence R and a configuration object X represent a path to configuration object Y if Y can be reached by starting at X and following all relations from R sequentially.  X object is called path origin, elements of R are path components. 

Confc library uses m0_confc_path data structure to represent a path. The members of this structure are: 

- p_origin --- path origin (struct m0_conf_obj*). NULL for absolute path; 

- p_comps --- array of components. A component is either a downlink, represented by a type of target object (enum m0_conf_objtype), or a key. 

Examples:

- { NULL, [FILESYSTEM, SERVICE, “foo”, NODE, NIC, “bar”] } --- absolute path (origin = NULL) to the NIC with key “bar” of the node that hosts service “foo”; 

- { node_obj, [SDEV, “baz”, PARTITION] } --- relative path to a list of partitions that belong “baz” storage device of a given node. 

Subroutines
==============

- m0_confc_init() 

  Initiates configuration client, creates the root configuration object. 

  Arguments: 

  - profile --- name of profile to be used by this confc; 

  - confd_addr --- address of confd end point; 

  - sm_group --- state machine group (struct m0_sm_group*) that will be associated with configuration cache.


- m0_confc_open() 

  Requests an asynchronous opening of a configuration object. Initiates retrieval of configuration data from the confd, if the data needed to fulfill this request is missing from configuration cache.  

  Arguments:

  - path --- path to configuration object. The caller must guarantee existence and immutability of path until the state machine, embedded in ctx argument, terminates or fails;

  - ctx --- fetch context (struct m0_confc_fetchctx*) containing:

     - state machine (struct m0_sm); 

     - FOP (struct m0_fop); 

     - asynchronous system trap (struct m0_sm_ast) that will be posted to confc’s state machine group when a response from confd arrives; 

     - resulting pointer (void*) that will be set to the address of requested configuration object iff the state machine terminates successfully. Otherwise the value is NULL; 

     - errno. 

- m0_confc_open_sync() 

  Synchronous variant of m0_confc_open(). Returns a pointer to requested configuration object or NULL in case of error. 

  Argument: path --- path to configuration object. 

- m0_confc_close() 

  Closes a configuration object opened with m0_confc_open() or m0_confc_open_sync().

- m0_confc_diropen() 

  Requests an asynchronous opening of a collection of configuration objects. Initiates retrieval of configuration data from the confd, if the data needed to fulfill this request is missing from configuration cache. 

  Arguments:

  - path --- path to collection of configuration objects. The caller must guarantee existence and immutability of path until the state machine, embedded in ctx argument, terminates or fails; 

  - ctx --- fetch context (the structure is described above). Its ‘resulting pointer’ member will be set to non-NULL opaque value iff the state machine terminates successfully. This value is an argument for m0_confc_dirnext() and m0_confc_dirclose() functions.

- m0_confc_diropen_sync() 

  Synchronous variant of m0_confc_diropen(). Returns an opaque pointer to be passed to m0_confc_dirnext(). Returns NULL in case of error. 

- m0_confc_dirnext() 

  Returns next element in a collection of configuration objects. 

  Argument: dir --- opaque pointer obtained from a fetch context (see ctx argument of m0_confc_diropen()). 

- m0_confc_dirclose() 

  Closes a collection of configuration objects opened with m0_confc_diropen() or m0_confc_diropen_sync(). 

  Argument: dir --- opaque pointer obtained from a fetch context. 
    
- m0_confc_fini() 

  Terminating routine: destroys configuration cache, freeing allocated memory.

FOP Types
=================

Confc requests configuration information by sending m0_conf_fetch FOP to confd.  This FOP contains the path to the requested configuration object/directory.  Note that the path in FOP may be shorter then the path originally specified in m0_confc_*open*() call: if some of the objects are already present in confc cache, there is no reason to re-fetch them from confd.

Confd replies to m0_conf_fetch with m0_conf_fetch_resp FOP, containing: 

- status of the retrieval operation (0 = success, -Exxx = failure); 

- array (SEQUENCE in .ff terms) of configuration object descriptors.

If the last past component, specified in m0_conf_fetch, denotes a directory (i.e., a collection of configuration objects), then confd’s reply must include descriptors of all the configuration objects of this directory.  For example, if a path targets a collection of partitions, then m0_conf_fetch_resp should describe every partition of the targeted collection.   

Note that in the future, configuration data will be transferred from confd to confc using RPC bulk interfaces. Current implementation embeds configuration information in a response FOP and uses encoding and decoding functions generated by fop2c from .ff description. 

**************************
Logical Specification
**************************

Confd service is parametrized by address of the end point, associated with this service, and path to the configuration database. Confd state machine, created by the request handler[3], obtains requested configuration (either from configuration cache or from configuration database; in the latter case the data gets added to the cache), generates a response FOP (m0_conf_fetch_resp) populated with configuration information, and sends it back to confc.

Confc is parametrized by name of profile, address of confd end point, and state machine group.  When m0_confc_fetch() function is called, confc checks whether all of the requested configuration data is available in cache. If something is missing, confc sends request to the confd. When response arrives, confc updates the configuration cache. 

Conformance
==================

- [i.conf.async]: m0_confc_fetch() is an asynchronous non-blocking call. Confc state machine keeps the information about the progress of the query. 

- [i.conf.cache]: confc components, used by Motr services and filesystem clients, maintain the caches of configuration data. 

- [i.conf.cache.in-memory]: a confc obtains configuration from the confd and builds in-memory data structure in the address space of configuration consumer. 

- [r.conf.cache.resource-management]: prior to pinning configuration object or modifying configuration cache, configuration module (confd or confc) requests appropriate resource from the resource manager. 

Dependencies
=============

- [configuration.schema]  The confd is aware of the schema of configuration database. The actual schema is defined by another task[2] and is beyond the scope of this document. 

- [confc.startup]  Parameters, needed by confc startup procedure, are provided by calling code (presumably the request handler or the service startup logic). 

Security Model
===============

No security model is defined. 

Refinement
=============

- [r.conf.confc.kernel]  Confc library must be implemented for the kernel. 

- [r.conf.confc.user]  Confc library must be implement for user space. 

- [r.conf.confd]  Confd service must be implemented for user space. 

- [r.conf.cache.data-model]  The implementation should organize configuration information as outlined in sections above. The same data structures should be used for confc and confd caches, if possible. Configuration structures must be kept in memory. 

- [r.conf.cache.pinning]  Pinning of an object protects existence of this object in the cache. Pinned object can be moved from stub condition to “ready”. 

- [r.conf.cache.unique-objects]  Configuration cache must not contain multiple objects with the same identity (identity of a configuration object is a tuple of type and key). 

**********
State
**********

Confd and confc modules define state machines for asynchronous non-blocking processing of configuration requests.  State diagrams of such machines are shown below. 

States, events, transitions
============================

While a confd state machine is in CHECK_SERIALIZE state, it keeps configuration cache locked. LOAD state unlocks the cache, fetches missing objects from the configuration database, and falls back to CHECK_SERIALIZE. After configuration object is successfully loaded from the database, its status is set to M0_CS_READY and its channel (m0_conf_obj::co_chan) is broadcasted. 

Configuration cache is associated with a state machine group (m0_sm_group). While a confc state machine is in CHECK state, it keeps the state machine group locked. 

GROW_CACHE state releases state machine lock and performs the following actions for every configuration descriptor decoded from confd’s response (m0_conf_fetch_resp): 

- lock the state machine group; 

- make sure that target object of every relation, mentioned by the descriptor, is present in cache (stubs are created for absent target objects); 

- if an object with described identity (type and key) already exists in cache and its status is M0_CS_READY then compare existing object with the one received from confd, reporting inequality by means of ADDB API; 

- if an object with described identify already exists and is a stub then fill it with the received configuration data, change its status to M0_CS_READY, and announce status update on object’s channel; 

- unlock the state machine group. 

State Variants
================

If a confc state machine in GROW_CACHE state, while trying to add an object, finds that the object with this key already exists, then either the existing object is a stub or new and existing objects are equal. 

This invariant is also applicable to LOAD state of a confd state machine.

Concurrency Control
=====================

Confd
------
Several confd state machines (FOMs) can work with configuration cache --- read from it and add new objects to it --- concurrently. 

A confd state machine keeps configuration cache locked while examining its completeness (i.e., checking whether it has enough data to fulfill confc’s request) and serializing configuration data into response FOP. Another confd state machine is unable to enter CHECK_SERIALIZE state until the mutex, embedded into configuration cache data structure (m0_conf_cache::cc_lock), is released.

The same lock is used to prevent concurrent modifications to configuration DAG.  A confd state machine must hold m0_conf_cache::cc_lock in order to add new configuration object to the cache or fill a stub object with configuration data.  

Confc
------

Configuration cache at confc side is shared by configuration consumers that read from it and confc state machines that traverse the cache and add new objects to it. Consumers and state machines can work with configuration cache concurrently.

Implementation of confc serializes state transitions by means of a state machine group, associated with configuration cache. It is up to upper layers to decide whether this group will be used exclusively for confc or for some other state machines as well. 

The state machine group is locked by a confc state machine running in CHECK state. The lock (m0_sm_group::s_lock) is also acquired by a confc state machine for time interval needed to append new configuration object to the cache. 

Configuration consumers must pin the objects they are using (and not to forget to unpin them afterwards, of course). Pinned configuration objects cannot be deleted by confc library but those that have zero references may be deleted at any moment (e.g., when another consumer thread calls m0_confc_fini()). 

***********
Analysis
***********

Scalability
=================

Present design assumes that there is exactly one configuration server per cluster. The problem is that however fast a confd is, the amount of data it can serve per fixed time is limited (the same goes for the number of requests it can process). Given big enough number of confc’s willing to obtain configuration simultaneously (e.g., when a cluster is starting), RPC timeouts are inevitable. 

One of the possible solutions (which will probably be employed anyway, in one form or another) is to make configuration consumers keep on re-fetching the configuration data until a request succeeds or a maximum number of retries is reached.  Another solution is to replicate the configuration database and have as many confd’s as there are replicas. Both solutions are beyond the scope of this document. 

Of course, once a confc has configuration data cached it will not need to fetch the same data from confd again (at least not until configuration is updated, which is not going to happen in this incarnation of M0 configuration system). 

Other
==========

In the presence of resource manager, the invariant mentioned in the above sections is maintained by the resource manager. In the absence of resource manager, the invariant is maintained by the “cache is read-append-only” property. 

Rationale
==========

Rejected: 

- m0_confc_lookup() function that returns configuration record given name of configuration database table and record’s key; 

- YAML for configuration data serialization. 

  (Rationale: YAML is not convenient to be parsed in the kernel. It is much simpler to use encoding and decoding functions generated by fop2c from .ff description.) 

Postponed: 

- writable configuration; 

- configuration auto-discovery; 

- using RPC bulk interfaces for transmitting configuration data. 

Pointer to the root configuration object cannot be embedded in m0_reqh structure, because filesystem clients do not have request handler (yet). 

Installation
===============

- Configuration database is created with ‘yaml2db’ utility (the product of ‘configuration.devenum’ component). 

- Confd is a management service, started by request handler. It is installed together with Motr. 

- Confc is a library that configuration consumers link with. 


**References**


- [1] Configuration one-pager 

- [2] HLD of configuration.schema 

- [3] HLD of request handler 

- [4] HLD of resource management interfaces 

- [5] configuration.caching drafts 

