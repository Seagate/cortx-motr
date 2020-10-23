This document describes motr port to Darwin (aka osx, aka macos, aka xnu)
platform.

Motr on darwin is not officially supported by Seagate. This port is maintained
by developers in their spare time. To use darwin port one should be ready to
edit build scripts, debug and fix bugs.

==============
Current status
==============

Motr builds. Some unit tests run. Some unit tests fail. Many things are much
slower than on Linux. Network works over net/sock/sock.[ch]. Storage io (stob)
works, but is single threaded and very inefficient.

====================
Tested configuration
====================

macOS Sierra (10.12.6).

clang: Apple LLVM version 9.0.0 (clang-900.0.39.2).

The port was built and tested with command line tools (cc, lldb, etc.).

=====
Goals
=====

Darwin port is not supposed to be a production ready port any time soon. It was
created because:

- clang is a good compiler, quite different from gcc, producing different set
  of warnings, using different optimisations, etc.;

- different configuration (e.g., sizeof(long) != sizeof(long long)) improve
  code portability;

- Darwin run-time, if slow, is pedantic and check many conditions that Linux
  libc ignores. For examples, objects, like semaphores and mutices must be
  properly finalised, cannot be finalised while busy, etc.

- Darwin has a set of nice tools for program analysis, like memory leak and race
  detectors.

=======
Install
=======

Execute scripts/install-build-deps-macos.

Make sure that gnu versions of grep, awk, sed and getopt (installed by
scripts/install-build-deps-macos) are first in your PATH.

Make sure that castxml has version at least 0.3.3 and export CXXXML to point to
the appropriate castxml binary (brew might install an older version, in this
case download the package from kitware.com).

Run ./scripts/m0 rebuild
