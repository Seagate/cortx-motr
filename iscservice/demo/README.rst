Preparing computations library
==============================

Computations from an external library cannot be linked directly with
a Motr instance. The library is supposed to have an entry function named
``void motr_lib_init(void)``. All the computations in the library must
have the following signature::

  int comp(struct m0_buf *args, struct m0_buf *out,
           struct m0_isc_comp_private *comp_data, int *rc)

See demo/libdemo.c for examples.

Loading the library
===================

With ``spiel`` command (see spiel/spiel.h and demo/util.h) the library
can be loaded with any running Motr instance. A helper function
``m0_isc_lib_register`` takes the library path which is (IMPORTANT!)
expected to be the same across all the nodes running Motr.
``m0iscreg`` utility takes the path as an input and loads the library
into all the remote Motr instances.

On successful loading of the library, the output will look like this::

  $ m0iscreg -e 192.168.180.171@tcp:12345:4:1 \
             -x 192.168.180.171@tcp:12345:1:1 \
             -f 0x7200000000000001:0x2c \
             -p 0x7000000000000001:0x50 $PWD/libdemo.so
  m0iscreg success

The four options are standard ones to connect to Motr::

  $ m0iscreg -h

  Usage: m0iscreg OPTIONS libpath

     -e <addr>  endpoint address
     -x <addr>  ha-agent (hax) endpoint address
     -f <fid>   process fid
     -p <fid>   profile fid

The values for them can be taken from the output of ``hctl status``
command. (We'll refer to them as ``<motr-opts>`` below.)

Demo computations
=================

Currently, we demonstrate three simple computations: ``ping``, ``min`` and
``max``. ``m0iscdemo`` utility can be used to invoke the computations and
see the result::

  $ m0iscdemo -h

  Usage: m0iscdemo OPTIONS COMP OBJ_ID LEN

   Supported COMPutations: ping, min, max

   OBJ_ID is two uint64 numbers in hi:lo format (dec or hex)
   LEN    is the length of object (in KiB)

Following are the steps to run the demo.

ping
----

This functionality pings all the ISC services spanned by the object units.
For each unit a separate ping request is sent, so the utility prints
"Hello-World@<service-fid>" reply each of these requests.

Here is an example for the object with 1MB units::

  $ m0iscdemo <motr-opts> ping 123:12371 4096
  Hello-world @192.168.180.171@tcp:12345:2:2
  Hello-world @192.168.180.171@tcp:12345:2:2
  Hello-world @192.168.180.171@tcp:12345:2:2
  Hello-world @192.168.180.171@tcp:12345:2:2

Note: the object length (or the amount to read) must be specified, as Motr
does not store the objects lengths in their metadata.

min / max
---------

Write an object with a real numbers strings delimited by the newline.
The min/max in-storage computation can then be done on such object::

  $ m0iscdemo <motr-opts> max 123:12371 4096
  idx=132151 val=32767.627900
  $ m0iscdemo <motr-opts> min 123:12371 4096
  idx=180959 val=0.134330
