=========================================
Architecture review of Motr RPC layer 
=========================================

Motr rpc layer comprises the higher level part of the network stack. Its purpose is to provide higher layers with an interface for communication with remote parts of the system that:

- is flexible enough: 

  - supports synchronous and asynchronous communication; 

  - supports priorities; 

  - supports sending various auxiliary information, like addb records; 

- is efficient enough: 

  - the network bandwidth is utilized fully by sending concurrent messages; 

  - the messaging overhead is amortised by issuing larger compound message; 

  - 0-copy, if provided by the underlying network transport, is utilised. 

In addition, rpc layer should support ordered exactly once semantics of delivery, but this functionality is beyond the scope of the present document (it belongs to the sessions component).

*******
Cache 
*******

As described in the Motr networking 1-pager, Motr rpc layer maintains a cache of items (a better name is needed. Element?) ready to be sent to the remote end-points. The layer contains a logic to place these items into containers called rpcs that are sent over the network. Cached items are directed at particular end-points, but rpcs are sent to services (network addresses). The mapping from end-points to services can change over time (see 1-pager). 

***************
Formation 
***************

The rpc formation algorithm is a subtle and critical part of an efficient IO system. It should take the following considerations into account: 

- underlying network transport performs optimally when certain conditions are met. This includes: 

   - a certain number of messages are in flight (between a pair of nodes). The algorithm tries to keep max-in-flight number of rpc to a given server in flight at any time. To see examples of this, search for cl_max_rpcs_in_flight in the Lustre sources; 

   - messages should be large to amortise the message overhead (the overhead consists of  networking hardware setup to send the message, various interrupt-related overheads at the receiver, &c.). The algorithm should try to form messages as large as possible, but no larger than max-message-size parameter. To see examples of this search for cl_max_pages_per_rpc in the Lustre sources; 

   - rdma may impose restrictions on a total number of fragments in the message. That is, the message cannot contain more than max-message-fragments disjoint memory buffers. See comment about "fragmented" page array in osc_request.c:osc_send_oap_rpc() in Lustre; 

- remote services perform optimally when certain conditions are met. For example: 

   - a data service (handling data IO in a form of read and write fops) favours large rpcs for contiguous operations, with well-aligned starting position. See ending_offset and comments on it in osc_request.c:osc_send_oap_rpc() in Lustre; 

   - meta-data service would benefit from an rpc where component fops are sorted by (say) fid of file, because this would result in a more sequential storage IO. No examples to look at, because Lustre doesn't have meta-data batching; 

- upper layers can specify priorities of items; 

- upper layers can specify caching properties of items. E.g., for some items caching is write-through: the item is not cached and instead is immediately placed into an rpc, even is the latter would be sub-optimal. Other caching parameters, like maximal time the item can linger in the cache can also be specified. 

Generally, the rpc formation algorithm is specified per (target end-point, network transport) pair, which implies that it should be carefully split into two cooperating policies. 

Note, that at the moment Motr doesn't need a very sophisticated rpc formation algorithm, because there are no data caches on Motr clients (all IO goes in a cache-through mode), but the implementation should be extensible so that a Lustre-like algorithm could be plugged in. 
