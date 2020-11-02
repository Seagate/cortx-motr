============
Repair Auxdb
============

***************
Objective
***************

To implement any additional meta-data tables that are not needed during normal io service operations, but are only required for SNS repair.


***************
Keywords
***************

- storage devices - devices attached to data servers 

-  storage objects (stob) - provide access to storage device contents by means of a linear name-space associated with an object

-  component object (cob) - refers to a stob and contains its metadata

-  global object (gob) - is an object describing a striped file, by referring to a collection of cobs

-  containers - capable of storing other objects

**********************
SNS Repair and Aux: DB
**********************

- SNS repair proceeds in gob fid order

- A single table is necessary which maps the gob fid to cob fid for every device.

- This information is used by storage agents to iterate over gob-fid->cob-fid mapping for its device, selecting the next cob to process.

***************
COB - FID Map
***************

- C2 database tables are key-value associations

- A typical record in the cobfid_map will have,

  Key   : (device_id, fid)
  
  Value: cob_fid

where

- device/container id  : uint64_t

- fid  (gob fid)       : struct c2_fid

- cob_fid              : struct c2_uint128

**Note**: Tuple of {device_id, fid, cob_fid} is always unique.

***************
Interfaces
***************

#. Insert record to cobfid_map - executed on every ioservice when a file gets created.

#. Delete record from cobfid_map - executed on every ioservice when a file gets deleted.

#. Enumerate devices/containers (In future, device id will begeneralized to container id) - Used by storage agents (typically storage-in agent to iterate over fid-cob_fid mapping to select nextcob for repair).

#. Enumerate map (devices + fid).

***************
Enumeration
***************

Enumeration is implemented using the c2_db_cursor interfaces. The sequence of operation is as follows:

#. Create a cursor using c2_db_cursor_init().

#. Create an initial positioning key value with the desired device-id and a file fid value of 0, and invoke c2_db_cursor_get() to set the initial position and get the first key/value record. This works because this subroutine sets the DB_SET_RANGE flag internally, which causes a greater-than-equal-to comparison of the key value when positioning the cursor.

#. Subsequent records are fetched using c2_db_cursor_next().

#. Traversal ends if at any time the device-id component of the returned key changes from the desired device-id, or we’ve exhausted all records in the database 
