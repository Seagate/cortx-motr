=========================================
High-Level Design of a Motr Object Index
=========================================

This document provides a High-Level Design (HLD) of an Object Index for the Motr M0 core. The main purposes of this document are:

- To be inspected by M0 architects and peer designers to ensure that HLD is aligned with M0 architecture and other designs and contains no defects

- To be a source of material for Active Reviews of Intermediate Design (ARID) and Detailed Level Design (DLD) of the same component

- To be served as a design reference document

The intended audience of this document consists of M0 customers, architects, designers and developers.

**************
Introduction
**************

The Object Index performs the function of a metadata layer on top of the M0 storage objects. The M0 storage object (stob) is a flat address space where one can read or write with block size granularity. The stobs have no metadata associated with them. Additionally, metadata must be associated with the stobs to use as files or components of files, aka stripes.

- namespace information: parent object id, name, links

- file attributes: owner/mode/group, size, m/a/ctime, acls

- fol reference information: log sequence number (lsn), version counter

Metadata must be associated with both component objects (cobs), and global objects. Global objects would be files striped across multiple component objects. Ideally global objects and component objects should reuse the same metadata design (a cob can be treated as a gob with a local layout).

*************
Definitions
*************

- A storage object (stob) is a basic M0 data structure containing raw data.

- A component object (cob) is a component (stripe) of a file, referencing a single storage object and containing metadata describing the object.

- A global object (gob) is an object describing a striped file, by referring to a collection of component objects.

**************
Requirements
**************

- [R.M0.BACK-END.OBJECT-INDEX]: an object index allows back-end to locate an object by its fid

- [R.M0.BACK-END.INDEXING]: back-end has mechanisms to build metadata indices

- [R.M0.LAYOUT.BY-REFERENCE]: file layouts are stored in file attributes by reference

- [R.M0.BACK-END.FAST-STAT]: back-end data-structures are optimized to make stat(2) call fast

- [R.M0.DIR.READDIR.ATTR]: readdir should be able to return file attributes without additional IO

- [R.M0.FOL.UNDO]: FOL can be used for undo-redo recovery

- [R.M0.CACHE.MD]: metadata caching is supported

******************
Design Highlights
******************

- The file operation log will reference particular versions of cobs (or gobs). The version information enables undo and redo of file operations.

- cob metadata will be stored in database tables.

- The database tables will be stored persistently in a metadata container.

- There may be multiple cob domains with distinct tables inside a single container.

*************************
Functional Specification
*************************

Cob code:

- provides access to file metadata via fid lookup

- provides access to file metadata via namespace lookup

- organizes metadata for efficient filesystem usage (esp. stat() calls)

- allows creation and destruction of component objects

- facilitates metadata modification under a user-provided transaction

************************
Logical Specification
************************

Structures
===========

Three database tables are used to capture cob metadata:

- object-index table

  - key is {child_fid, link_index} pair

  - record is {parent_fid, filename}

- namespace table

  - key is {parent_fid, filename}

  - record is {child_fid, nlink, attrs}

  - if nlink > 0, attrs = {size, mactime, omg_ref, nlink}; else attrs = {}

  - multiple keys may point to the same record for hardlinks, if the database can support this. Otherwise, we store the attrs in one of the records only (link number 0). This    leads to a long sequence of operations to delete a hardlink, but straightforward.

- fileattr_basic table

  - key is {child_fid}

  - record is {layout_ref, version, lsn, acl} (version and lsn are updated at every fop involving this fid)

link_index is an ordinal number distinguishing between hardlinks of the same fid. E.g. file a/b with fid 3 has a hardlink c/d. In the object index table, the key {3,0} refers to a/b, and {3,1} refers to c/d.

omg_ref and layout_ref refer to common owner/mode/group settings and layout definitions; these will frequently be cached in-memory and referenced by cobs in a many-to-one manner. Exact specification of these is beyond the scope of this document.

References to the database tables are stored in a cob_domain in-memory structure. The database contents are stored persistently in a metadata container.

There may be multiple cob_domains within a metadata container, but the usual case will be 1 cob_domain per container. A cob_domain may be identified by an ordinal index inside a container. The list of domains will be created at container ingest.

.. code-block:: C

    struct m0_cob_domain {

                     cob_domain_id cd_id /* domain identifier */

                     m0_list_link cd_domain_linkage

                     m0_dbenv *cd_dbenv

                     m0_table *cd_obj_idx_table

                     m0_table *cd_namespace_table

                     m0_table *cd_file-attr-basic_table

                     m0_addb_ctx cd_addb

    }
        
A m0_cob is an in-memory structure, instantiated by the method cob_find and populated as needed from the above database tables. The m0_cob may be cached and should be protected by a lock.

.. code-block:: C

    struct m0_cob {

                    fid co_fid;

                    m0_ref co_ref; /* refcounter for caching cobs */

                    struct m0_stob *co_stob; /* underlying storage object */

                    struct m0_rwlock co_guard; /* lock on cob manipulation */

                    m0_fol_obj_ref co_lsn;

                    u64 co_version

                    struct namespace_rec *co_ns_rec;

                    struct fileattr_basic_rec *co_fab_rec;

                    struct object_index_rec *co_oi_rec; /* pfid, filename */

    };

The `*_rec` members are pointers to the records from the database tables. These records may or may not be populated at various stages in cob life.

The co_stob reference is also likely to remain unset, as metadata operations will not frequently affect the underlying storage object and, indeed, the storage object is likely to live on a different node.

Usage
======

m0_cob_domain methods locate the database tables associated with a container. These methods are called at container discovery/setup.

m0_cob methods are used to create, find, and destroy in-memory and on-disk cobs. These might be:

- cob_locate: find an object via a fid using the object_index table.

- cob_lookup: find an object via a namespace lookup (namespace table).

- cob_create: add a new cob to the cob_domain namespace

- cob_remove: remove the object from the namespace

- cob_get/put: take references on the cob. At last put cob may be destroyed.
        
m0_cob_domain methods are limited to initial setup and cleanup functions, and are called during container setup/cleanup.

Simple mapping functions from the fid to stob:so_id and to the cob_domain:cd_id are assumed to be available.

Conformance
============

- [I.M0.BACK-END.OBJECT-INDEX]: object-index table facilitates lookup by fid

- [I.M0.BACK-END.INDEXING]: new namespace entries are added to the db table

- [I.M0.LAYOUT.BY-REFERENCE]: layouts are referenced by layout ID in fileattr_basic table.

- [I.M0.BACK-END.FAST-STAT]: stat data is stored adjacent to namespace record in namespace table.

- [I.M0.DIR.READDIR.ATTR]: namespace table contains attrs

- [I.M0.FOL.UNDO]: versions and lsn's are stored with metadata for recovery

- [I.M0.CACHE.MD]: m0_cob is refcounted and locked

Dependencies
==============

- [R.M0.FID.UNIQUE]: uses; fids can be used to uniquely identify a stob

- [R.M0.CONTAINER.FID]: uses; fids indentify the cob_domain via the container

- [R.M0.LAYOUT.LAYID]: uses; reference stored in fileattr_basic table

**********
Use Cases
**********

Scenarios
==========

.. list-table::
   :header-rows: 1
   
   * - Scenario 1
     - QA.schema.op
   * - Relevant quality attributes
     - variability, re-usability, flexibility, modifiability
   * - Stimulus
     - a Request Handler invokes back-end as part of file system operation processing
   * - Stimulus source
     - a file system operation request originating from protocol translator, native M0 client or storage application
   * - Environment
     - normal operation
   * - Artifact
     - a series of Schema accesses
   * - Response
     - Metadata back-end contains enough information to handle file system operation request. This information includes the below mentioned aspects:
     
       - tandard file attributes as defined by POSIX, including access control related information; 
       
       - description of file system name-space, including directory structure, hard-links and symbolic links; 
       
       - references to remote parts of file-system namespace; 
       
       - file data allocation information
       
   * - Response Measure
     - 
   * - Questions and issues
     - 



.. list-table::
   :header-rows: 1
   
   * - Scenario 2
     - QA.schema.stat
   * - Relevant quality attributes
     - usability
   * - Stimulus
     - a stat(2) request arrives to a Request Handler
   * - Stimulus source
     - a user application
   * - Environment
     - normal operation
   * - Artifact
     - a back-end query to locate the file and fetch its basic attributes
   * - Response
     - Schema must be structured so that stat(2) processing can be done quickly without extract index lookups and associated storage accesses
   * - Response Measure
     - 
       - an average number of schema operations necessary to complete stat(2) processing; 
       - an average number of storage accesses during stat(2) processing
   * - Questions and issues
     - 



.. list-table::
   :header-rows: 1
   
   * - Scenario 3
     - QA.schema.duplicates
   * - Relevant quality attributes
     - usability
   * - Stimulus
     - a new file is created
   * - Stimulus source
     - protocol translator, native C2 client or storage application
   * - Environment
     - normal operation
   * - Artifact
     - a records, describing new file are inserted in various schema indices
   * - Response
     - records must be small. Schema must exploit the fact that in a typical file system, certain sets of file attributes 
       have much fewer different values than combinatorially possible. Such sets of attributes are stored by reference, 
       rather than by duplicating the same values in multiple records. Examples of such sets of attributes are: 

       - {file owner, file group, permission bits} 

       - {access control list} 

       - {file layout formula}

   * - Response Measure
     - 

        - average size of data that is added to the indices as a result of file creation

        - attribute and attribute set sharing ratio
   * - Questions and issues
     - 


.. list-table::
   :header-rows: 1
   
   * - Scenario 4
     - QA.schema.simple
   * - Relevant quality attributes
     - re-usability, variability
   * - Stimulus
     - a new file is created
   * - Stimulus source
     - protocol translator, native M0 client or storage application
   * - Environment
     - normal operation
   * - Artifact
     - a records, describing new file are inserted in various schema indices
   * - Response
     - Schema can be described and implemented in terms of a limited repertoire of standard operations: 

       - index lookup 

       - index modification

      - index iteration Assuming fairly standard transactional capabilities and usual locking primitives
      
   * - Response Measure
     - 

        - average size of data that is added to the indices as a result of file creation

        - attribute and attribute set sharing ratio
   * - Questions and issues
     - 


.. list-table::
   :header-rows: 1
   
   * - Scenario 5
     - QA.schema.index
   * - Relevant quality attributes
     - variability, extensibility, re-usability
   * - Stimulus
     - storage application wants to maintain additional metadata index
   * - Stimulus source
     - storage application
   * - Environment
     - normal operation
   * - Artifact
     - index creation operation
   * - Response
     - schema allows dynamic index creation
   * - Response Measure
     - 
   * - Questions and issues
     - 
     
This is OBSOLETED content.
