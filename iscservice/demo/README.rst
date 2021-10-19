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

Note: ``m0iscreg`` utility can be used to load any future ISC-library
without modifications.

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
does not store the objects lengths in their metadata. In the example above,
4MB length was specified for the object with 1MB units, so 4 ping requests
were sent and, as result, 4 replies were printed as a result.

The cluster configuration in the above example consisted of a single node
only, so all the units were located on the same node. That's why the
endpoints addresses in the replies are identical.

min / max
---------

Write an object with a real numbers strings delimited by the newline.
The min/max in-storage computation can then be done on such object::

  $ m0iscdemo <motr-opts> max 123:12371 4096
  idx=132151 val=32767.627900
  $ m0iscdemo <motr-opts> min 123:12371 4096
  idx=180959 val=0.134330

Benchmark example
=================

This benchmark was conducted on the SAGE Prototype Cluster (located in
Jülich Computing Centre). SSD pool was used with 8+2 EC configuration,
shared among the 3 server nodes (with max 5 SSDs per node).

1GB object::

  $ \time m0iscdemo <motr-opts> min 0x3456023:0x87002803 $((1024*1024))
  idx=2845139 val=0.100200
  2.37user 0.75system 0:15.66elapsed 19%CPU (0avgtext+0avgdata 234728maxresident)k
  0inputs+231016outputs (0major+99487minor)pagefaults 0swaps
  $
  $ # Compare with the client computation performance on the same object:
  $
  $ mcp <motr-opts> -v -osz $((1024*1024)) 0x3456023:0x87002803 - | \time ~/minmax min
  2021/10/18 15:49:50 mio.go:614: R: off=0 len=33554432 bs=33554432 gs=33554432 speed=500 (Mbytes/sec)
  ...
  2021/10/18 15:50:15 mio.go:614: R: off=1040187392 len=33554432 bs=33554432 gs=33554432 speed=711 (Mbytes/sec)
  idx=2845139 val=0.100200
  23.36user 0.59system 0:31.45elapsed 76%CPU (0avgtext+0avgdata 588maxresident)k
  0inputs+0outputs (0major+224minor)pagefaults 0swaps

2GB object::

  $ \time m0iscdemo <motr-opts> min 0x3456023:0x87002805 $((2*1024*1024))
  idx=2845139 val=0.100200
  4.37user 1.01system 0:24.27elapsed 22%CPU (0avgtext+0avgdata 236728maxresident)k
  0inputs+262288outputs (0major+164358minor)pagefaults 0swaps
  $
  $ # Client computation:
  $
  $ mcp <motr-opts> -v -osz $((2*1024*1024)) 0x3456023:0x87002805 - | \time ~/minmax min
  2021/10/18 16:08:04 mio.go:614: R: off=0 len=33554432 bs=33554432 gs=33554432 speed=492 (Mbytes/sec)
  ...
  2021/10/18 16:08:54 mio.go:614: R: off=2113929216 len=33554432 bs=33554432 gs=33554432 speed=653 (Mbytes/sec)
  idx=2845139 val=0.100200
  46.35user 1.30system 0:56.97elapsed 83%CPU (0avgtext+0avgdata 588maxresident)k
  0inputs+0outputs (0major+225minor)pagefaults 0swaps

4GB object::

  $ \time m0iscdemo <motr-opts> min 0x3456023:0x87002806 $((4*1024*1024))
  idx=2845139 val=0.100200
  7.50user 1.05system 0:40.85elapsed 20%CPU (0avgtext+0avgdata 246840maxresident)k
  0inputs+362736outputs (0major+173574minor)pagefaults 0swaps
  $
  $ # Client computation:
  $
  $ mcp <motr-opts> -v -osz $((4*1024*1024)) 0x3456023:0x87002806 - | \time ~/minmax min
  2021/10/18 16:17:45 mio.go:614: R: off=0 len=33554432 bs=33554432 gs=33554432 speed=516 (Mbytes/sec)
  ...
  2021/10/18 16:19:27 mio.go:614: R: off=4261412864 len=33554432 bs=33554432 gs=33554432 speed=592 (Mbytes/sec)
  idx=2845139 val=0.100200
  93.48user 2.48system 1:48.59elapsed 88%CPU (0avgtext+0avgdata 584maxresident)k
  0inputs+0outputs (0major+231minor)pagefaults 0swaps

8GB object::

  $ \time m0iscdemo <motr-opts> min 0x3456023:0x87002807 $((8*1024*1024))
  idx=2845139 val=0.100200
  14.48user 1.57system 1:15.78elapsed 21%CPU (0avgtext+0avgdata 272176maxresident)k
  0inputs+1424720outputs (0major+360575minor)pagefaults 0swaps
  $
  $ # Client computation:
  $
  $ mcp <motr-opts> -v -osz $((8*1024*1024)) 0x3456023:0x87002807 - | \time ~/minmax min
  2021/10/18 17:33:54 mio.go:614: R: off=0 len=33554432 bs=33554432 gs=33554432 speed=500 (Mbytes/sec)
  ...
  2021/10/18 17:37:17 mio.go:614: R: off=8556380160 len=33554432 bs=33554432 gs=33554432 speed=615 (Mbytes/sec)
  idx=2845139 val=0.100200
  185.60user 4.82system 3:29.11elapsed 91%CPU (0avgtext+0avgdata 588maxresident)k
  0inputs+0outputs (0major+265minor)pagefaults 0swaps


We can clearly see that the computation with ISC performs more than 2 times faster,
than on the client node with the client utility (which runs exactly the same logic
to find min/max as the ISC library). And the bigger the object size, the faster it
performs, see the table below.

ISC Performance Comparison table:

+------------------+----------------------+-------------------------+--------------+
| Object size (GB) | ISC computation time | Client computation time | Times faster |
+==================+======================+=========================+==============+
|               1  |                15.66 |                   31.45 |         2.0  |
+------------------+----------------------+-------------------------+--------------+
|               2  |                24.27 |                   56.97 |         2.34 |
+------------------+----------------------+-------------------------+--------------+
|               4  |                40.85 |                 1:48.59 |         2.65 |
+------------------+----------------------+-------------------------+--------------+
|               8  |              1:15.78 |                 3:29.11 |         2.75 |
+------------------+----------------------+-------------------------+--------------+
