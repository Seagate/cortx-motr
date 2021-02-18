==================================================
High level design of Motr Network Benchmark 
==================================================

This document presents a high level design (HLD) of Motr Network Benchmark. The main purposes of this document are: (i) to be inspected by M0 architects and peer designers to ascertain that high level design is aligned with M0 architecture and other designs, and contains no defects, (ii) to be a source of material for Active Reviews of Intermediate Design (ARID) and detailed level design (DLD) of the same component, (iii) to serve as a design reference document. 

The intended audience of this document consists of M0 customers, architects, designers and developers. 

*****************
Introduction
*****************

Motr network benchmark is designed to test network subsystem of Motr and network connections between nodes that are running Motr. 

**************
Definitions
**************

- m0_net_test: a network benchmark framework that uses the Motr API; 

- test client: active side of a test, e.g. when A pings B, A is the client; 

- test server: passive side of a test, e.g. when A pings B, B is the server; 

- test node: a test client or server; 

- test group: a set of test nodes; 

- test console: a node where test commands are issued; 

- test message: unit of data exchange between test nodes; 

- test statistics: summary information about test messages for a some period of time; 

- bulk test: a test that uses bulk data transfer; 

- ping test: a test that uses short message transfer; 

- command line: command line parameters for an user mode program or kernel module parameters for a kernel module; 

**************
Requirements
**************

- [r.m0.net.self-test.statistics]: should be able to gather statistics from all nodes; 

- [r.m0.net.self-test.statistics.live]: display live aggregate statistics when the test is still running; 

- [r.m0.net.self-test.test.ping]: simple ping/pong to test connectivity, and measure latency; 

- [r.m0.net.self-test.test.bulk]: bulk message read/write

- [r.m0.net.self-test.test.bulk.integrity.no-check]: for pure link saturation tests, or stress tests. This should be the default.

- [r.m0.net.self-test.test.duration.simple]: end user should be able to specify how long a test should run, by loop; 

- [r.m0.net.self-test.kernel]: test nodes must be able to run in kernel space; 

*******************
Design Highlights
*******************

- Bootstrapping Before the test console can issue commands to a test node, m0_net_test must be running on that node, as a kernel module pdsh can be used to load a kernel module. 

-Statistics collecting Kernel module creates file in /proc filesystem, which can be accessed from the user space. This file contains aggregate statistics for a node.

- Live statistics collecting pdsh used for live statistics. 
