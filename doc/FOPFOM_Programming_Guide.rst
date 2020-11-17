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
 
 
***************
FDL Limitations
***************

Following limitations need to be considered while declaring FOP:

- Currently we cannot define FOPs dynamically, they need to be defined and built before they can be used.

***************
Compiling FDL
***************

A .ff file should be compiled using ff2c compiler.

After writing FOPs and creating .ff file for a particular module, we need to make an entry for the same in the module's Makefile.am file. This would automatically invoke ff2c on the .ff files and create corresponding “C” format structures.

**Simple FOP, containing native data types in file fom_io_xc.ff**:

::

 record {

         u64 f_seq; 

         u64 f_oid

 } reqh_ut_fom_fop_fid;

**Makefile.am entry:**

::

 UT_SRCDIR = @SRCDIR@/reqh/ut 

 noinst_LTLIBRARIES = libreqh-ut.la 

 INCLUDES = -I. -I$(top_srcdir)/include \ -I$(top_srcdir) 

 FOM_FOPS = fom_io_xc.h fom_io_xc.c 

 $(FOM_FOPS): fom_io_xc.ff \ 

         $(top_builddir)/xcode/ff2c/ff2c 

         $(top_builddir)/xcode/ff2c $< 

 libreqh_ut_la_SOURCES = $(FOM_FOPS) \ 

                         reqh_fom_ut.c fom_io_xc.ff 

 EXTRA_DIST = fom_io.ff 

 MOSTLYCLEANFILES = $(FOM_FOPS)

On compiling fom_io_xc.ff file using ff2c compiler, it creates corresponding fom_io_xc.h and fom_io_xc.c.
