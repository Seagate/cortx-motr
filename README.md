# Motr

[![Codacy Badge](https://app.codacy.com/project/badge/Grade/a3d60ecc5d8942c9a4b04bcf4b60bf20)](https://www.codacy.com/gh/Seagate/cortx/dashboard?utm_source=github.com&amp;utm_medium=referral&amp;utm_content=Seagate/cortx&amp;utm_campaign=Badge_Grade)
[![license](https://img.shields.io/badge/License-Apache%202.0-blue.svg)](https://github.com/Seagate/cortx/blob/main/LICENSE)
[![Slack](https://img.shields.io/badge/chat-on%20Slack-blue")](https://cortx.link/join-slack)
[![YouTube](https://img.shields.io/badge/Video-YouTube-red)](https://cortx.link/videos)
[![Latest Release](https://img.shields.io/github/v/release/Seagate/cortx?label=Latest%20Release)](https://github.com/seagate/cortx/releases/latest)
[![GitHub contributors](https://img.shields.io/github/contributors/Seagate/cortx-motr)](https://github.com/Seagate/cortx-motr/graphs/contributors/)
[![SODA Eco project](https://img.shields.io/badge/SODA-ECO%20Project-9cf)](./doc/Soda-welcome-page.md)

At the core of [CORTX](https://github.com/Seagate/cortx) lies Motr.  Motr is a distributed object storage system, targeting [mass capacity storage](https://www.seagate.com/products/storage/object-storage-software/)
configurations. To ensure the most efficient storage utilization, Motr interacts directly with block devices (i.e. it does not _layer_ on a local file system).  The Motr design was heavily influenced by the Lustre file system, NFSv4 and database technology. It must be noted that traditional file system properties (hierarchical directory namespace, strong POSIX consistency guarantees, etc.) are no longer desirable or achievable at mass capacity. Instead, Motr is a more general storage system that provides an optional file system interface. This allows wider range of deployments, including cloud.

Following are the features of CORTX Motr:

-   Scalable: 
    -   Horizontal scalability: grow your system by adding more nodes. The Motr submodule is designed for horizontal scalability with no meta-data hotspots, shared-nothing IO paths and extensions running on additional nodes.
    -   Vertical scalability: with more memory and CPU on the nodes.
  
-   Fault-tolerant: with flexible erasure coding that takes hardware and network topology into account.

-   Fast network raid repairs.

-   Observable: with built-in monitoring that collects detailed information about the system behavior.

-   Extensible.

-   Extension interface.

-   Flexible transactions.

-   Open source.

-   Portable: runs in user space and can be easily ported to any version of Linux.

## Get to know

-   [Quick Start Guide](/doc/Quick-Start-Guide.rst)
-   [Architectural Summary](/doc/motr-in-prose.md)
-   [Example Cluster Setup](https://github.com/Seagate/cortx-motr/discussions/285)
-   [Source Structure](/doc/source-structure.md)
-   [Coding Style](/doc/coding-style.md)
-   [Developer Guide](/doc/motr-developer-guide.md)
-   [Provisioning Guide for Building and Testing Environment](/scripts/provisioning/README.md)
-   [Motr Performance Tuning](https://github.com/Seagate/cortx-motr/wiki/Motr-Performance-Tuning)

## Surfing

Refer to [Reading - list](/doc/reading-list.md) for complete information.
-   $ make doc
-   $ x-www-browser doc/html/index.html
