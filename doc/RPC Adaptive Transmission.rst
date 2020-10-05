==============================
RPC Adaptive Transmission
==============================

This document outlines requirements for the Motr RPC Adaptive Transmission (AT) feature. 

AT provides an interface to attach memory buffers to an rpc item on the sender side. AT implementation transfers the contents of the buffer to the receiver by either of the following methods.

- Inline: By including the contents of the buffer into the rpc item body.

- Inbulk: By constructing a network buffer representing the memory buffer, transmitting the network buffer descriptor in the rpc item and using bulk (RDMA) to transfer the contents of the buffer.  

Irrespective of the transfer method, the receiver uses the same interface to access the contents of the buffer.

***************
Requirements
***************  

- re-use m0_rpc_bulk for inbulk transfer mode

- the decision to use inline vs. inbulk is based on 

  - buffer alignment. Only page-aligned buffers are suitable for inbulk, 

  - buffer size and total rpc size. Rpc initialization takes a new parameter that determines the cutoff size after which inbulk is used.

- receiver side is fom-oriented, i.e., can be easily used within a tick function. 
