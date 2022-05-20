===============================
Motr end-to-end Data Integrity
===============================

*******************
Design Highlights
*******************

- Data of each target is divided into blocks of 4096 bytes.

- Checksum and tags of 64-bit each for these blocks are computed at m0t1fs and sent over wire.

- Checksum for data blocks is computed based on checksum algorithm selected from configuration.

- Data integrity type and operations are initialized in m0_file.

- Using do_sum(), checksum values are computed for each block of data and using do_chk(), checksum values are verified.


  .. image:: Images/Write.PNG


  .. image:: Images/Read.PNG
  
  
  .. image:: Images/SNS.PNG
  
Current Status
===============

Completed
----------

- Di is computed at m0t1fs and sent over wire.

- After receiving write fop, checksum is recomputed and verified at the IO service.

In progress
------------

- In be segment block attributes m0_be_emap_seg:ee_battr is added. The m0_be_emap_seg:ee_val and ee_battr (When b_nob > 0) are stored in btree.

- Emap split for di data.

- Write di data to extents while storing the data in disks (uses be_emap_split and in place btree insert api’s).

- Read di data from extents while reading data from disks and verify checksum.

- In sns while reading data units, verify checksum and while writing, store di data.
