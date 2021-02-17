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


