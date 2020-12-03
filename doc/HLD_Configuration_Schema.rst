=========================================================
High level design of Motr configuration database schema 
=========================================================

This document presents a high level design (HLD) of Motr’s configuration database schema The main purposes of this document are: (i) to be inspected by M0 architects and peer designers to ascertain that high level design is aligned with M0 architecture and other designs, and contains no defects, (ii) to be a source of material for Active Reviews of Intermediate Design (ARID) and detailed level design (DLD) of the same component, (iii) to serve as a design reference document. 

The intended audience of this document consists of M0 customers, architects, designers and developers.

*************
Introduction
*************

Motr maintains all the configuration information in some central repository from where it is served to all other nodes interested in the information. This HLD is about database schema used to maintain all the configuration.


*************
Definitions
*************

- Motr configuration is part of meta-data that is updated by management components, as opposed to meta-data updated as part of executing file system operations. 

- Configuration database is a central repository of cluster configuration. 

- Configuration cache is configuration data being stored in node’s memory. 

- Configuration client (confc) is a software module that manages node’s configuration cache. 

- Configuration server (confd) is a software module that mediates access to the configuration database. Also, the server node on which this module runs. 

- Trinity is a set of utilities and GUI used by system administrators to monitor and influence operation of a cluster.


*************
Requirements
*************

- [R.M0.CONFIGURATION.SCHEMA.CENTRALISED]

  - Must store global configuration in a centralized location.
  
  
*************************
Functional Specification
*************************

Configuration database is created and maintained by confd. Configuration database contains information about following entities: 

- Node 

- storage devices 

- disk-partitions 

- nics 

- file-systems 

- services 

- profiles 

- storage pools


Other possible objects whose information can be kept in configuration database are: 

- Motr tuning parameters 

- Routers 

- Containers 

- snapshots 

- software versions 

- enterprise user data-base 

- security, keys

**********************
Logical Specification
**********************

Configuration database is kept at some well-known location in persistent store on confd node. Configuration database is presented as set of tables where each table is a set of <key, value> pairs. Configuration database is implemented using m0_db_* interfaces. confd accesses configuration database using m0_db APIs.

When confd service starts, it opens the configuration database or creates it if it does not already exists.

Configuration database maintains separate table for each of following entities:

- nodes 

- storage devices 

- storage-device-partitions 

- nics 

- file-systems 

- services 

- profiles 

- storage pools

Relationships between these entities in configuration database: 

.. image:: Images/config.PNG
















