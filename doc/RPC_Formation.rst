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

***************
Requirements
***************

- [r.rpc_formation.network_optimization]: Formation component should make optimum use of network bandwidth. 

- [r.rpc_formation.maximize_io_throughput]: Form RPCs in such a manner that will help maximize the IO throughput. 

- [r.rpc_formation.extensible]: RPC Formation component should be extensible enough to accommodate further changes so that a Lustre like algorithm could be plugged in [1]. 

- [r.rpc_formation.send_policy]: Formation component will decide when to send an RPC to the output component in compliance with certain network parameters.

******************
Design Highlights
******************

RPC formation component will incorporate a formation algorithm which will create RPC objects from RPC items taking into consideration various parameters. 

The formation component will be implemented as a state machine with state transitions based on certain external events.

*************************
Functional Specification
*************************

RPC formation component will employ a formation algorithm which will act on a cache of RPC items. The algorithm will decide the items to be selected from the cache and it will put them together in an RPC object. The formation algorithm will make sure that maximum size of an RPC object is limited by max_message_size so that it makes optimal use of network bandwidth. The maximum number of disjoint buffers in an RPC object is limited by max_message_fragments. The max_message_fragments limit is enforced due to a limitation of RDMA to transfer only certain number of disjoint buffers in a request.

Multiple RPC items will be coalesced into one RPC item if intents are similar. This will be typically useful in case of vectored read/writes3. The individual rpc items being coalesced will be kept intact and an intermediate structure will be introduced to which all member rpc items will be tied. On receiving reply of the coalesced rpc item, callbacks to individual rpc items will be called which are part of the intermediate structure. The coalesced rpc item will put the constituent items in increasing order of file offset thus benefiting from sequential IO (reduced disk head seek). As far as possible, coalescing will be done within rpc groups. If not, coalescing can be done across groups. Coalescing is not done for items belonging to different update streams.

The RPC item cache could contain bounded and unbounded items in the sense that they may or may not have session information embedded within them. The formation algorithm queries and retrieves the session information for unbounded items after formation is complete and before sending the RPC object on wire.

The RPC Formation algorithm will be triggered only if current rpcs in flight per endpoint are less than max_rpcs_in_flight. It will also take care of distributed deadlocks.     
