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

***************
Requirements
***************

- [r.background.scrub.triggers] Background scrub can be triggered by an i/o failure due to DI checksum mismatch of a data block or by scanner. 

- [r.background.scrub.scan] Scanner can be running continuously in the background. 

- [r.background.scrub.scan.efficient] Scanner should efficiently scan disks for corruptions in a  non-blocking fashion. 

- [r.background.scrub.scan.progress] Scanner should provide an interface that allows to query its progress. 

- [r.background.scrub.io] Scrubber reads and writes relevant data from and to the disk. Reconstructed data is written in a newly allocated data block on the same disk having the failed block. 

- [r.background.scrub.net] Scrubber sends and receives data from remote nodes over the network. 

- [r.background.scrub.xform] Scrubber transforms the data read in-order to reconstruct the lost data. 

- [r.background.scrub.failures] Background scrub should handle failures during its progress, accordingly suspend (for SNS repair to take over) or restart. 

- [r.background.scrub.DI.interface] Background scrub should provide an interface to interact with DI. 

- [r.background.scrub.sns.repair] It should be possible to suspend an ongoing background scrub operation if SNS repair is triggered, possibly due to a device failure. 

- [r.background.scrub.repair.code.reuse] Background scrub should try to reuse code parts from SNS repair. 

- [r.background.scrub.halon] Background scrub should be able to notify halon in case the number of failures are more than K.

*******************
Design Highlights
*******************

Background scrub is implemented as a Motr service with its corresponding foms. Unlike SNS repair, scrubbing involves reconstruction of a small subset of data, e.g. a parity group and does not have to be a distributed operation. Although being a pull model (compared to push model of SNS repair), it reuses selected parts of i/o, transformation and network communication as in SNS repair. Data scrubbing is performed as a continuous operation and also on-demand basis. It is started as a background process, thus it is necessary to give a higher priority to normal i/o operations in order to make the data available to user all the time. Failures during data scrubbing must be accounted appropriately and reported to the relevant entity. System resources, e.g. memory, cpu and network bandwidth, must be used judiciously. Background scrub subsystem may have to interact with other Motr subsystems, e.g. i/o, SNS repair, Halon, etc. for concurrency control and failure handling. Scrubbing must be halted in case SNS repair takes over due to a device failure (e.g. disk, controller, Enclosure, rack, etc.).
