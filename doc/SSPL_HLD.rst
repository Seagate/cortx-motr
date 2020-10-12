============
HLD of SSPL
============

Platform functionality will be provided via the SSPL, a library of functions designed to provide a multi-tiered common interface to the storage platform.

SSPL consists of two layers of interfaces (verbs) that together provide an abstracted interface between all the components of the Motr solution.

These two layers are the High Level (SSPLHL) and Low Level (SSPLLL) sub components which work together to provide the layered abstraction of hardware components and software stack with the aim of providing an agnostic interface to the entire platform.

The High Level (HL) component provides abstraction of the underlying cluster to a top level consumer such as a generic management UI / CLI and the RAS system.

The Low Level (LL) component provides abstraction from the underlying hardware and software stack that comprises the storage platform. This will be but isn’t limited to the Core Motr software stack; the Puppet provisioning tool set and the RAS DCS collector subsystem.

*********************************
SSPL - High Level Verbs (SSPLHL) 
*********************************

The purpose of the HL component of the SSPL is to provide a management and configuration interface for carrying out such actions as on-the-fly Motr configuration changes; service and file system control and supply telemetry and platform metrics.

 
It is expected that most commands accessing the HL library will not be acted upon directly by the platform library but will in fact be sent to the HAlon High Availability subsystem for processing as a cluster wide event in order to ensure that the only commands executed are those that can be completed without detrimental impact of the operation of the system as a whole.

It is reasonable for us to expect that not all commands should flow via the HAlon subsystem and for those outside cases the requirement to utilize the HAlon subsystem will be relaxed such that those commands can progress immediately to the SSPL (Low Level) library. Examples of such a command could be the querying of a system metric not recorded or monitored by the HAlon subsystem for example Puppet manifest data or control of some other 3rd party application and data.

In the above cases of non-HAlon derived actuation of high level requests or commands the SSPLHL should be able to ascertain the programmatic path for command/control realization. This programmatic path discovery should not rely on HAlon’s notion of the ground truth of the system as it is highly likely HAlon will have no awareness at any level of the software or data being referenced. 

All activity through these HL verbs should be logged, this logging information should include both the verb called; the variables passed to that verb and the response(s) received.         
