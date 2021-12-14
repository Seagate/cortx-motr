# CORTX Motr Developer Guide

This document describes how to develop a simple application with Motr APIs.  
This document also list links to more useful Motr applications with richer  
features.

The first document developers should read is: [Motr Client API](/motr/client.h).  
It explains basic notations, terminologies, and data structures in CORTX Motr.  
Example motr client applications can be found in our [motr client apps repo](https://github.com/Seagate/cortx-motr-apps)  
and an alternative higher-level interface to motr can be found in our [Maestro repo](https://github.com/Seagate/cortx-mio).  
Developers are also assumed having a running CORTX Motr system: please refer to  
[Cluster Setup](https://github.com/Seagate/Cortx/blob/main/doc/Cluster_Setup.md) and [Quick Start Guide](/doc/Quick-Start-Guide.rst). For debugging, ADDB can be  
very useful and is described in these [two](ADDB.rst) [documents](addb2-primer).

For your convenience to build and develop motr, we offer a  
[CentOS-7.9-based Development VM](https://github.com/Seagate/cortx-motr/releases/tag/ova-centos79)  
with all dependencies installed ready to build and start single-node Motr cluster.

CORTX Motr provides object based operations and index (a.k.a key/value) based operations as well as FDMI interface.  
For architectural documents, please refer to our [reading list](reading-list.md).

## A simple Cortx Motr Object Application

[Object Application](motr-object-app.md)

## A simple Cortx Motr Index/Key-Value Application

[Key-Value Application](motr-kv-app.md)

## A simple Cortx Motr FDMI application

[FDMI Application](/fdmi/plugins/motr-fdmi-app.md)

## How to compile & build the application examples

One way is to use the Motr building framework. This example is compiled and built  
within the Motr building framework. If you have a new application, add it to the  
top level Makefile.am and add a Makefile.sub in your directory.

You can also treat this example as a standalone application, and build it out of  
More building framework. Please refer to the comments in the source code.  
If Motr-devel source code or RPM is already installed, you may find the header  
files in "/usr/include/motr " dir and binary libraries in "/lib".

## How to run the examples for Object and Key-Value

The first way is to run application against a running Cortx Motr system. Please  
refer to [Cluster Setup](https://github.com/Seagate/Cortx/blob/main/doc/Cluster_Setup.md)  
and [Quick Start Guide](/doc/Quick-Start-Guide.rst). `hctl status` will display Motr service  
configuration parameters.

The second way is to run the "motr/examples/setup\_a\_running\_motr\_system.sh" in a singlenode mode.  
Motr service configuration parameters will be shown there. Then run this example application  
from another terminal.

Cortx Motr object is identified by a 128-bit unsigned integer. An id should be provided  
as the last argument to the object program. In the object example, we will only use this id as the lower  
64-bit of an object identification. This id should be larger than 0x100000ULL, that is 1048576 in decimal.


## More examples, utilities, and applications

*   More Cortx Motr examples (object) and utilities are:
    *   [m0cp](/motr/st/utils/copy.c)
    *   [m0cat](/motr/st/utils/cat.c)
    *   [m0touch](/motr/st/utils/touch.c)
    *   [m0unlink](/motr/st/utils/unlink.c)
    *   [m0client](/motr/st/utils/client.c)
*   More Cortx Motr examples (index) and utilities are:
    *   [m0kv](/motr/m0kv/)
*   A Cortx Motr performance utility:
    *   [m0crate](/motr/m0crate/)
*   A Cortx Motr GO bindings:
    *   [bindings/go/](/bindings/go)
*   A Cortx Motr HSM utility and application:
    *   [hsm/](/hsm/)
*   A Cortx Motr Python wrapper:
    *   [spiel/](/spiel)
*   A S3Server using Motr client APIs to access Cortx Motr services:
    *   https://github.com/Seagate/cortx-s3server  
        This is one of the components of Cortx project.
*   A library which uses Motr client APIs to access Cortx Motr services, to provide file system accessibility:
    *   https://github.com/Seagate/cortx-posix  
        This is one of the components of Cortx project.
*   In-Store Computation (aka Function Shipping) demo:
    *   [iscservice/demo/](/iscservice/demo)

