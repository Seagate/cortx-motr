==============
RPC Formation
==============

RPC formation component is part of RPC core component [0], which determines when an RPC can be created and what its contents will be. This helps in optimal use of network bandwidth and helps in maximizing the IO throughput.

RPC formation component acts as a state machine in the RPC core layer.

***************
Definitions
*************** 

- fop, file operation packet, a description of file operation suitable for sending over network and storing on a storage device. File operation packet (FOP) identifies file operation type and operation parameters; 

- rpc item is an entity containing FOP or other auxiliary data which is grouped into RPC object after formation. 

- rpc, is a collection of rpc items. 

- endpoint (not very good term, has connotations of lower levels) is a host on which service is being executed. 

- session, corresponds to network connection between two services. 

- max_rpcs_in_flight, is the number of RPC objects that can be in-flight (on-wire) per endpoint. This parameter will be handled as a resource by resource manager.  

- max_message_size, is the largest possible size of RPC object. 

- max_message_fragments, is the number of maximum disjoint buffer an RPC object can contain. 

- urgent rpc item, is rpc item with zero deadline value.  
