
=====
Motr
=====

.. Slack Badge  - https://img.shields.io/badge/chat-on%20Slack-blu

.. image:: https://img.shields.io/badge/chat-on%20Slack-blue
        :target: https://join.slack.com/t/cortxcommunity/shared_invite/zt-femhm3zm-yiCs5V9NBxh89a_709FFXQ?
        :alt: Slack chat badge

.. Codacy Badge - https://api.codacy.com/project/badge

.. image:: https://api.codacy.com/project/badge/Grade/e047436e66e54d67b911294ad7fe8b4a
        :target: https://app.codacy.com/gh/Seagate/cortx-motr?utm_source=github.com&utm_medium=referral&utm_content=Seagate/cortx-motr&utm_campaign=Badge_Grade
         :alt: Codacy Badge 
        
.. License Badge - https://img.shields.io/badge/License-Apache%202.0-blue.svg

.. image:: https://img.shields.io/badge/License-Apache%202.0-blue.svg
        :target: https://github.com/pujamudaliar/cortx-motr/blob/main/LICENCE
        :alt: License Badge

At the core of `CORTX <https://github.com/Seagate/cortx>`_ lies Motr.  Motr is a distributed object storage system, targeting `mass capacity storage <https://www.seagate.com/products/storage/object-storage-software/>`_
configurations. To ensure the most efficient storage utilization, Motr interacts directly with block devices (i.e. it does not _layer_ on a local file system).  The Motr design was heavily influenced by the Lustre file system, NFSv4 and database technology. It must be noted that traditional file system properties (hierarchical directory namespace, strong POSIX consistency guarantees, &c.) are no longer desirable or achievable at mass capacity. Instead, Motr is a more general storage system that provides an optional file system interface. This allows wider range of deployments, including cloud.

Following are the features of CORTX Motr:

- Scalable:

        - Horizontal scalability: grow your system by adding more nodes. The Motr submodule is designed for horizontal scalability with no meta-data hotspots, shared-nothing IO paths and extensions running on additional nodes.
        - Vertical scalability: with more memory and CPU on the nodes.
- Fault-tolerant: with flexible erasure coding that takes hardware and network topology into account.
- Fast network raid repairs.
- Observable: with built-in monitoring that collects detailed information about the system behavior.
- Extensible.
- Extension interface.
- Flexible transactions.
- Open source.
- Portable: runs in user space and can be easily ported to any version of Linux.

Get to know
===========

- `Quick Start Guide </doc/Quick-Start-Guide.rst>`_

- `Architectural Summary </doc/motr-in-prose.md>`_

- `Example Cluster Setup <https://github.com/Seagate/cortx-motr/discussions/285>`_

- `Source Structure </doc/source-structure.md>`_

- `Coding Style </doc/coding-style.md>`_

- `Developer Guide </doc/motr-developer-guide.md>`_

- `Provisioning Guide for Building and Testing Environment </scripts/provisioning/README.md>`_

- `Motr Performance Tuning <https://github.com/Seagate/cortx-motr/wiki/Motr-Performance-Tuning>`_

Surfing
=======
Refer to `Reading - list </doc/reading-list.md>`_ for complete information.

- $ make doc

- $ x-www-browser doc/html/index.html
