=====================================
High level design of a layout schema
=====================================

This document presents a high level design (HLD) of a layout schema of Motr M0 core. The main purposes of this document are: (i) to be inspected by M0 architects and peer designers to ascertain that high level design is aligned with M0 architecture and other designs, and contains no defects, (ii) to be a source of material for Active Reviews of Intermediate Design (ARID) and detailed level design (DLD) of the same component, (iii) to serve as a design reference document.

The intended audience of this document consists of M0 customers, architects, designers and developers.


*************
Introduction
*************

Very broadly, a layout is something that determines where a particular piece of data or meta-data is located in a storage system, including location of data and meta-data that are about to be added to the system.

The layout is by itself a piece of meta-data and has to be stored somewhere. This HLD of layout schema is to design the organization of the layout meta-data stored in database.

*************
Definitions
*************

- A layout is a map determining where file data and meta-data are located. The layout is by itself a piece of meta-data and has to be stored somewhere.

- A layout is identified by layout identifier uniquely.

- This map is composed of sub-maps, and a sub-map may still be composed of sub-maps, till direct mapping to some known underlying location, i.e. block number on physical device.

- A layout schema is a way to store the layout information in data base. The schema describes the organization for the layout meta-data.

*************
Requirements
*************

- [r.layout.schema.layid] layout identifiers. Layout identifiers are unique globally in the system, and persistent in the life cycle.

- [r.layout.schema.types] multiple layout types. There are multiple layout types for different purposes: SNS, block map, local raid, de-dup, encryption, compression, etc.

- [r.layout.schema.formulae] layout formulae (future)

  - parameters. Layout may contain sub-map information. Layout may contain some formula, and its parameters and real mapping information should be calculated from the formula and its parameters.

  - garbage collection. If some objects are deleted from the system, their associated layout may still be left in the system, with zero reference count. This layout can be re-used, or be garbage collected in some time.
  
- [r.layout.schema.sub-layouts] sub-layouts (future).














