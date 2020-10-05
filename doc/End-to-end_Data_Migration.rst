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


 .. image:: images/Write.PNG
