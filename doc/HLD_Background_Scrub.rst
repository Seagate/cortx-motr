========================
HLD of Background Scrub
========================

Motr, with the help of DI (data integrity) feature using block checksums, shall be able to work with less reliable drives. Although DI provides required data integrity through checksums for unreliable hardware, it alone is not enough to ensure data availability.

Background scrub will reconstruct a failed block reported by DI subsystem on detecting a checksum mismatch without major impact on the product systems. This document will present the high-level design of background scrub. 

***************
Definitions
***************

Although background scrub follow similar principles to SNS repair [2], it is relatively a much lighter operation and works on a very small subset of data.

Following terms are used to discuss and describe background scrub:

- background scrub: subsystem to identify and repair corrupted motr data blocks. 

- DI: motr data integrity component. 

- scrubbing: process of identifying and repairing a small subset of corrupted data. 

- scanner: continuously scans motr data for any corruption and notifies background subsystem. 

- scrub machine: receives and executes scrub requests from DI and background scrub scanner. 

- scrub request: request submitted by DI or scanner in-order to repair the corrupt motr data block. 

- scrubber: worker created by scrub machine to serve a scrub request. 

- scrub group: a group of relevant data blocks typically distributed across storage devices in a cluster, e.g. a parity group. 

- transformation: reconstruction performed by background scrub worker in-order to recover the corrupted data.    
