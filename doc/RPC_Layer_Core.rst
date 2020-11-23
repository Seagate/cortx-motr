==================
RPC Layer Core
==================

This document presents a high level design (HLD) of rpc layer core of Colibri C2 core. The main purposes of this document are: (i) to be inspected by C2 architects and peer designers to ascertain that high level design is aligned with C2 architecture and other designs, and contains no defects, (ii) to be a source of material for Active Reviews of Intermediate Design (ARID) and detailed level design (DLD) of the same component, (iii) to serve as a design reference document. The intended audience of this document consists of C2 customers, architects, designers and developers.

**************
Introduction
**************

RPC layer is a high level part of C2 network stack. It contains operations allowing user to send RPC items, containing File Operation Packets (FOP) to network destinations. RPC layer speaks about FOPs being sent from and to end-points and RPCs, which are containers for FOPs, being sent to services. RPC layer is a part of a C2 server as well as a C2 client.

**************
Definitions
**************

- fop, file operation packet, a description of file operation suitable for sending over network and storing on a storage device. File operation packet (FOP) identifies file operation type and operation parameters;

- rpc, is a container for fops and other auxiliary data. For example, addb records are placed in rpcs alongside with fops

- service, process, running on an endpoint, allowing to execute user requests.

- endpoint (not very good term, has connotations of lower levels), host on which service is being executed.

- message, communication mechanism, allowing to send requests and receive replies.

- network address, IP address.

- session, corresponds to network connection between two services.

- (update) stream, is established between two end-points and fops are associated with the update streams.

- slot, context of update stream.

- replay

- resend

- action (forward fop, ylper?), operation of RPC item transition from client to service.

- reply (backward fop), operation of RPC item transition from client to service with results of "ylper" operation.

- RPC-item, item is being processed and sent by RPC layer.

- fop group








