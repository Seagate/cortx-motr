================
Motr Epochs HLD
================
Motr services may, in general, depend on global state that changes over time. Some of this global state changes only as a result of failures, such as changing the striping formula across storage pools. Since these changes are failure driven, it is the HA subsystem that coordinates state transition as well as broadcasts of state updates across the cluster.

Because failures can happen concurrently, broadcasting state updates across the cluster can never be reliable. In general, some arbitrarily large portion of the cluster will be in a crashed state or otherwise unavailable during the broadcast of a new state. It is therefore unreasonable to expect that all nodes will share the same view of the global state at all times. For example, following a storage pool layout change, some nodes may still assume an old layout. Epochs are a Motr-specific mechanism to ensure that operations from one node to another are done with respect to a shared understanding of what the global state is. The idea is to assign a number (called the epoch number) to each global state (or item thereof), and communicate this number with every operation. A node should only accept operations from other nodes if the associated epoch number is the same same as that of the current epoch of the node. In a sense, the epoch number acts as a proxy to the shared state, since epoch numbers uniquely identify states.

This document describes the design of the HA subsystem in relation to coordinating epoch transitions and recovery of nodes that have not yet joined the latest epoch.

***************
Definitions
***************   

- Shared state item: a datum, copies of which are stored on multiple in the cluster. While a set S of nodes are in a given epoch, they all agree on the value of the shared state. 

- State transition: a node is said to transition to a state when it mutates the value of a datum to match that state. 

- Epoch failure: a class of failures whose recovery entails a global state transition. It is safe to include all failures in this class, but in practice some failures might be excluded if doing so does not affect correctness but improves performance.  

- Epoch: the maximal interval in the system history through which no epoch failures are agreed upon. In terms of HA components, this means that an epoch is the interval of time spanning between two recovery sequences by the Recovery Coordinator, or between the last recovery sequence and infinity. 

- Epoch number: an epoch is identified by an epoch number, which is a 64-bit integer. A later epoch has a greater number. Epoch numbers are totally ordered in the usual way. 

- HA domain: an epoch is always an attribute of a domain. Each domain has its own epoch. Nodes can be members of multiple domains so each node may be tracking not just one epoch but in fact multiple epochs. A domain also has a set of epoch handlers associated with it.

***************
Requirements
***************

- [R.MOTR.EPOCH.BROADCAST] When the HA subsystem decides to transition to a new epoch, a message is broadcast to all nodes in the cluster to notify them of an epoch transition. This message may include (recovery) instructions to transition a new shared state. 

- [R.MOTR.EPOCH.DOMAIN] There can be multiple concurrent HA domains in the cluster. Typically, there is one epoch domain associated with each Motr request handler. 

- [R.MOTR.EPOCH.MONOTONE] The epoch for any given domain on any given node changes monotonically. That is, a node can only transition to a later epoch in a domain, not an older one. 

- [R.MOTR.EPOCH.CATCH-UP] A node A that is that is told by another node B that a newer epoch exists, either in response to a message B or because B sent a message to A mentioning a later epoch, can send to failure event to the HA subsystem to request instructions on how to reach the latest epoch.

*******************
Design Highlights
*******************

- Upon transitioning to a new epoch, the HA subsystem broadcasts the new epoch number to the entire cluster. 

- The HA subsystem does not wait for acknowledgements from individual nodes that they have indeed transitioned to the new epoch. Aggregation of acknowledgements is therefore not required. 

- Communication of HA subsystem to nodes is done using Cloud Haskell messages, to make communication with services uniform within the HA subsystem, and to circumscribe the Motr-specific parts of the epoch functionality to the services themselves. 


