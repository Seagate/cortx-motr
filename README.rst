=====
Motr
=====
Motr is a distributed storage system, targeting `exascale <https://en.wikipedia.org/wiki/Exascale_computing>`_
configurations. Its main roots are Lustre file system, NFSv4 and database technology. It must be noted that traditional file system properties (hierarchical directory namespace, strong POSIX consistency guarantees, &c.) are no longer desirable or achievable at exascale. Instead, Motr is a more general storage system that provides an optional file system interface. This allows wider range of deployments, including cloud.

Get to know
===========

- `Quick Start Guide <https://github.com/Seagate/cortx-motr/blob/main/doc/Quick-Start-Guide.rst>`_

- `Source Structure <https://github.com/Seagate/cortx-motr/blob/main/doc/source-structure.md>`_

- `Coding Style <https://github.com/Seagate/cortx-motr/blob/main/doc/coding-style.md>`_

Surfing
=======
Refer `Reading - list <https://github.com/Seagate/cortx-motr/blob/main/doc/reading-list.md>`_ for complete information.

- $ make doc

- $ x-www-browser doc/html/index.html
