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

***********************
Encoding-Decoding FOP
***********************

Rather than sending individual items between Colibri services as separate RPCs, as in traditional RPC mechanisms, multiple items are batched and sent as a single RPC. Batching allows larger messages to be sent, allowing the cost of message passing to be amortized among the multiple items. The upper layers of the RPC module, specifically, the Formation module, select which items are to be batched into a single RPC. Once items are selected, the RPC Formation module then creates that single in-core RPC object. This object is then encoded/serialized into an on wire rpc object and copied into a network buffer using the exported interfaces ( m0_rpc_encode () and m0_rpc_decode()). Each onwire rpc object includes a header with common information, followed by a sequence of items. This RPC is sent to the receiver stored and decoded into individual items. The items are queued on the appropriate queues for processing.

***************
Sending a FOP
***************

A fop can be sent as a request FOP or a reply FOP. A fop is sent across using the various rpc interfaces. Every fop has an rpc item embedded into it.

::

 struct m0_fop {

 ...

       /**

          RPC item for this FOP

        */

       struct m0_rpc_item      f_item;

 ...
 
Sending a fop involves initializing various fop and rpc item structures and then invoking the m0_rpc_post routines. The steps for the same are described below with few code examples.

Define and initialize the fop_type ops
======================================

::

 const struct m0_fop_type_ops m0_rpc_fop_conn_establish_ops = {

       .fto_fom_init = &m0_rpc_fop_conn_establish_fom_init


 };

Define and initialize the rpc item_type ops
============================================

::

 static struct m0_rpc_item_type_ops default_item_type_ops = { 

       .rito_encode = m0_rpc_fop_item_type_default_encode, 

       .rito_decode = m0_rpc_fop_item_type_default_decode, 

       .rito_payload_size = m0_fop_item_type_default_onwire_size, 

 };
 
Define and initialize the rpc item type
========================================

::

 m0_RPC_ITEM_TYPE_DEF(m0_rpc_item_conn_establish,

                  m0_RPC_FOP_CONN_ESTABLISH_OPCODE,

                  m0_RPC_ITEM_TYPE_REQUEST | m0_RPC_ITEM_TYPE_MUTABO,

                  &default_item_type_ops);

Define and initialize the fop type for the new fop and associate the corresponding item type
==============================================================================================

::

 struct m0_fop_type m0_rpc_fop_conn_establish_fopt; 

 /* In module’s init function */ 

 foo_subsystem_init() 

 { 

         m0_xc_foo_subsystem_xc_init() /* Provided by ff2c compiler */

         m0_FOP_TYPE_INIT(&m0_rpc_fop_conn_establish_fopt, 

                   .name = "rpc conn establish", 

                   .opcode = m0_RPC_FOP_CONN_ESTABLISH_REP_OPCODE, 

                   /* Provided by ff2c */ 

                   .xt = m0_rpc_fop_conn_establish_xt, 

                   .fop_ops = m0_rpc_fop_conn_establish_ops); 

 }
 
A request FOP is sent by invoking a rpc routine m0_rpc_post(), and its corresponding reply can be sent by invoking m0_rpc_reply_post() (as per new rpc layer).

- Client side

  Every request fop should be submitted to request handler for processing (both at the client as well as at the server side) which is then forwarded by the request handler    itself, although currently (for “november” demo) we do not have request handler at the client side. Thus sending a FOP from the client side just involves submitting it to rpc layer by invoking m0_rpc_post(). So, this may look something similar to this:
  
  ::
  
   system_call()->m0t1fs_sys_call()
   
   m0t2fs_sys_call() {
   
        /* create fop */
        
        m0_rpc_post();
        
   }
   
- Server Side
 
  At server side a fop should be submitted to request handler for processing, invoking m0_reqh_fop_handle() and the reply is then sent by one of the standard/generic phases of the request handler.
















