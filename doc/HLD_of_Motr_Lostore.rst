==============================================
High level design of a Motr lostore module 
==============================================

This document presents a high level design (HLD) of a lower store (lostore) module of Motr core. The main purposes of this document are: (i) to be inspected by M0 architects and peer designers to ascertain that high level design is aligned with M0 architecture and other designs, and contains no defects, (ii) to be a source of material for Active Reviews of Intermediate Design (ARID) and detailed level design (DLD) of the same component, (iii) to serve as a design reference document. 

The intended audience of this document consists of M0 customers, architects, designers and developers. 

**************
Introduction
**************

- A table is a collection of pairs, each consisting of a key and a record. Keys and records of pairs in a given table have the same structure as defined by the table type. "Container" might be a better term for collection of pairs (compare with various "container libraries"), but this term is already used by Motr; 

- records consist of fields and some of fields can be pointers to pairs in (the same or other) table; 

- a table is ordered, if a total ordering is defined on possible keys of its records. For an ordered table an interface is defined to iterate over existing keys in order; 

- a sack is a special type of table, where key carries no semantic information and acts purely as an opaque record identifier, called record address. Addresses are assigned to records by the lostore module; 

- a table is persistent, if its contents survives a certain class of failures, viz. "power failures". A table can be created persistent or non-persistent (volatile); 

- tables are stored in segments. A segment is an array of pages, backed by either persistent or volatile storage. Each page can be assigned to a particular table. A table can be stored in the multiple segments and a segment can store pages belonging to the multiple table. Assignment of pages to tables is done by the lostore module. 

- updates to one of more tables can be grouped in a transaction, which is a set of updates atomic with respect to a failure (from the same class as used in the definition of persistent table). By abuse of terminology, an update of a volatile table can also be said to belong to a transaction; 

- after a transaction is opened, updates can be added to the transaction, until the transaction is closed by either committing or aborting. Updates added to an aborted transaction are reverted (rolled-back or undone). In the absence of failures, a committed transaction eventually becomes persistent, which means that its updates will survive any further power failures. On a recovery from a power failure, committed, but not yet persistent transactions are rolled back by the lostore implementation;

- a function call is blocking, if before return it waits for

  - a completion of a network communication, or 

  - a completion of a storage transfer operation, or 

  - a long-term synchronisation event, where the class of long-term events is to be defined later.   

Otherwise a function call is called non-blocking.

****************
Requirements
****************

- R.M0.MDSTORE.BACKEND.VARIABILITY: Supports various implementations: db5 and RVM 

- R.M0.MDSTORE.SCHEMA.EXPLICIT: Entities and their relations are explicitly documented. "Foreign key" following access functions provided. 

- R.M0.MDSTORE.SCHEMA.STABLE: Resistant against upgrades and interoperable 

- R.M0.MDSTORE.SCHEMA.EXTENSIBLE: Schema can be gradually updated over time without breaking interoperability. 

- R.M0.MDSTORE.SCHEMA.EFFICIENT: Locality of reference translates into storage locality. Within blocks (for flash) and across larger extents (for rotating drives) 

- R.M0.MDSTORE.PERSISTENT-VOLATILE: Both volatile and persistent entities are supported 

- R.M0.MDSTORE.ADVANCED-FEATURES: renaming symlinks, changelog, parent pointers 

- R.M0.REQH.DEPENDENCIES	mkdir a; touch a/b 

- R.M0.DTM.LOCAL-DISTRIBUTED: the same mechanism is used for distributed transactions and local transactions on multiple cores 

- R.M0.MDSTORE.PARTIAL-TXN-WRITEOUT: transactions can be written out partially (requires optional undo logging support) 

- R.M0.MDSTORE.NUMA: allocator respects NUMA topology 

- R.M0.REQH.10M: performance goal of 10M transactions per second on a 16 core system with a battery backed memory. 

- R.M0.MDSTORE.LOOKUP: Lookup of a value by key is supported 

- R.M0.MDSTORE.ITERATE: Iteration through records is supported. 

- R.M0.MDSTORE.CAN-GROW: The linear size of the address space can grow dynamically 

- R.M0.MDSTORE.SPARSE-PROVISIONING: including pre-allocation 

- R.M0.MDSTORE.COMPACT, R.M0.MDSTORE.DEFRAGMENT: used container space can be compacted and de-fragmented 

- R.M0.MDSTORE.FSCK: scavenger is supported 

- R.M0.MDSTORE.PERSISTENT-MEMORY: The log and dirty pages are (optionally) in a persistent memory 

- R.M0.MDSTORE.SEGMENT-SERVER-REMOTE: backing containers can be either local or remote	 

- R.M0.MDSTORE.ADDRESS-MAPPING-OFFSETS: offset structure friendly to container migration and merging 

- R.M0.MDSTORE.SNAPSHOTS: snapshots are supported 

- R.M0.MDSTORE.SLABS-ON-VOLUMES: slab-based space allocator 

- R.M0.MDSTORE.SEGMENT-LAYOUT: Any object layout for a meta-data segment is supported 

- R.M0.MDSTORE.DATA.MDKEY: Data objects carry meta-data key for sorting (like reiser4 key assignment does). 

- R.M0.MDSTORE.RECOVERY-SIMPLER: There is a possibility of doing a recovery twice. There is also a possibility to use either object level mirroring or a logical transaction mirroring. 

- R.M0.MDSTORE.CRYPTOGRAPHY: optionally meta-data records are encrypted 

- R.M0.MDSTORE.PROXY: proxy meta-data server is supported. A client and a server are almost identical.

******************
Design highlights
******************

Key problem of lostore interface design is to accommodate for different implementations, viz.,  a db5-based one and RVM-based. To address this problem, keys, records, tables and their relationships are carefully defined in a way that allows different underlying implementations without impairing efficiency.

lostore transaction provide very weak guarantees, compared with the typical ACID transactions: 

- no isolation: lostore transaction engine does not guarantee transaction serialisability. User has to implement  any concurrency control measure necessary. The reason for this decision is that transaction used by Motr are much shorter and smaller than typical transactions in a general purpose RDBMS and a user is better equipped to implement a locking protocol that guarantees consistency and deadlock freedom. Note, however, that lostore transactions can be aborted and this restricts the class of usable locking protocols; 

- no durability: lostore transactions are made durable asynchronously, see the Definitions section above.

The requirement of non-blocking access to tables implies that access is implemented as a state machine, rather than a function call. The same state machine is used to iterate over tables. 

*************************
Functional Specification
*************************

mdstore and iostore use lostore to access a data-base, where meta-data are kept. Meta-data are orgianised according to a meta-data schema, which is defined as a set of lostore tables referencing each other together with consistency constraints.

lostore public interface consists of the following major types of entities: 

- table type: defines common characteristics of all table of this type, including:

  - structure of keys and records, 

  - optional key ordering, 

  - usage hints (e.g., how large is the table? Should it be implemented as a b-tree or a hash table?)  

- table: defines table attributes, such as persistence, name; 

- segment: segment attributes such as volatility or persistency. A segment can be local or remote; 

- transaction: transaction object can be created (opened) and closed (committed or aborted). Persistence notification can be optionally delivered, when the transaction becomes persistent; 

- table operation, table iterator: a state machine encoding the state of table operation; 

- domain: a collection of tables sharing underlying data-base implementation. A transaction is confined to a single domain. 

Tables
========

For a table type, a user has to define operations to encode and decode records and keys (unless the table is sack, in which case key encoding and decoding functions are provided automatically) and optional key comparison function. It's expected that encoding and decoding functions will often be generated automatically in a way similar to fop encoding and decoding functions. 

Following functions are defined on tables:

- create: create a new instance of a table type. The table created can be either persistent or volatile, as specified by the user. Backing segment can be optionally specified; 

- open: open an existing table by name; 

- destroy: destroy a table; 

- insert: insert a pair into a table; 

- lookup: find a pair given its key; 

- delete: delete a pair with a given key; 

- update: replace pair's record with a new value; 

- next: move to the pair with the next key; 

- follow: follow a pointer to a record.

Transactions
=============

The following operations are defined for transactions:

- open: start a new transaction. Transaction flags can be specified, e.g., whether the transaction can be aborted, whether persistence notification is needed. 

- add: add an update to the transaction. This is internally called as part of any table update operation (insert, update, delete); 

- commit: close the transaction; 

- abort: close the transaction and roll it back; 

- force: indicate that transaction should be made persistent as soon as possible.  

Segments
=============

The following operations are defined for segments:

- create a segment backed up by a storage object (note that because the meta-data describing the storage object are accessible through lostore, the boot-strapping issues have to be addressed); 

- create a segment backed up by a remote storage object; 

- destroy a segment, none of which pages are assigned to tables. 

***************************
Logical Specification
***************************

Internally, a lostore domain belongs to one of the following types:

- a db5 domain, where tables are implemented as db5 databases and transactions as db5 transactions; 

- an rvm domain, where tables are implemented as hash tables in the RVM segments and transactions as RVM transactions; 

- a light domain, where tables are implemented as linked lists in memory and transactions calls are ignored.  


