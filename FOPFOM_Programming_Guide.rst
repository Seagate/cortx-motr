============
FOPFOM Guide
============

FOP stands for File Operation Packet. In a network or a distributed file-system, a file operation on a client may result in sending a message to a server to carry out that operation. FOP is a generic mechanism to define a application specific protocol within Motr file-system. The application here means any sub-system (within the file-system) above networking layer. FOP is not necessarily restricted to the file operations - unlike the name suggests. Thus it provides more generic and flexible framework to develop a protocol. In this aspect, FOP is different from traditional network file-system protocols messages.

******************************
FDL (FOP declaration language)
******************************

A FOP structure can comprise of following user defined and native types:

User defined:

- record (a structure in c programming language)

- union

- sequence (an array in programming languages)

Native types:

- u32

- u64

- u8

- void

A FOP can be declared as follows

::

 record { 

         u64 f_seq; 

         u64 f_oid }
 
 m0_fop_file_fid; 

 sequence { 

         u32 f_count;

         u8 f_buf
 } m0_fop_io_buf;     
