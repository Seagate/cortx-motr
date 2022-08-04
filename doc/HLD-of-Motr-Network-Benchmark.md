# High level design of Motr Network Benchmark     
This document presents a high level design **(HLD)** of Motr Network Benchmark.  
 The main purposes of this document are:   
1. To be inspected by M0 architects and peer designers to ascertain that high level design is aligned with M0 architecture and other designs, and contains no defects.  
2. To be a source of material for Active Reviews of Intermediate Design **(ARID)** and detailed level design **(DLD)** of the same component.  
3. To serve as a design reference document.  

The intended audience of this document consists of M0 customers, architects, designers, and developers.  

## Introduction  
Motr network benchmark is designed to test the network subsystem of Motr and network connections between nodes that are running Motr.  

## Definitions
- m0_net_test: a network benchmark framework that uses the Motr API;  
- test client: active side of a test, e.g. when A pings B, A is the client;  
- test server: passive side of a test, e.g. when A pings B, B is the server;
- test node: a test client or server;  
- test group: a set of test nodes;  
- test console: a node where test commands are issued;  
- test message: unit of data exchange between test nodes;  
- test statistics: summary information about test messages for some time;  
- bulk test: a test that uses bulk data transfer;  
- ping test: a test that uses short message transfer;  
- command line: command line parameters for a user-mode program or kernel module parameters for a kernel module.

## Requirements
- `[r.m0.net.self-test.statistics]`: should be able to gather statistics from all nodes;  
- `[r.m0.net.self-test.statistics.live]`: display live aggregate statistics when the test is still running;
- `[r.m0.net.self-test.test.ping]`: simple ping/pong to test connectivity, and measure latency;  
- `[r.m0.net.self-test.test.bulk]`: bulk message read/write  
- `[r.m0.net.self-test.test.bulk.integrity.no-check]`: for pure link saturation tests, or stress tests. This should be the default.  
- `[r.m0.net.self-test.test.duration.simple]`: end-user should be able to specify how long a test should run, by loop;  
- `[r.m0.net.self-test.kernel]`: test nodes must be able to run in kernel space.  


## Design Highlights
- Bootstrapping Before the test console can issue commands to a test node, m0_net_test must be running on that node, as a kernel module pdsh can be used to load a kernel module.  
- Statistics collecting Kernel module creates a file in /proc filesystem, which can be accessed from the user space. This file contains aggregate statistics for a node.
- Live statistics collecting pdsh used for live statistics.    


## Functional Specification
- `[r.m0.net.self-test.test.ping]`   
  - Test client sends the desired number of test messages to a test server with desired size, type, etc. Test client waits for replies (test messages) from the test server and collects statistics for send/received messages;  
  - Test server waits for messages from a test client, then sends messages back to the test client. Test server collects messages statistics too;  
  - Messages RTT need to be measured;
- `[r.m0.net.self-test.test.bulk]`   
  - Test client is a passive bulk sender/receiver;
  - Test server is an active bulk sender/receiver;
  - Messages RTT and bandwidth from each test client to each corresponding test server in both directions need to be measured;  
- Test console sends commands to load/unload the kernel module (implementation of **m0_net_test** test client/test server), obtains statistics from every node, and generates aggregate statistics.  

## Logical specification  

### Kernel module specification   

#### start/finish   
After the kernel module is loaded all statistics are reset. Then the module determines is it a test client or a test server and acts according to this. Test client starts sending test messages immediately. All statistical data remain accessible until the kernel module is unloaded.  

#### test console
Test console uses pdsh to load/unload kernel module and gather statistics from every node's procfs. pdsh can be configured to use a different network if needed.  

#### test duration
End-user should be able to specify how long a test should run, by the loop. Test client checks command line parameters to determine the number of test messages.  

### Test Message  

#### message structure
Every message contains a timestamp and sequence number, which is set and checked on the test client and the test server and must be preserved on the test server. The timestamp is used to measure latency and the sequence number is used to identify message loss.  

#### message integrity
Currently, test message integrity is not checked either on the test client nor the test server.  

#### measuring message latency
Every test message will include a timestamp, which is set and tested on the test client. When the test client receives test message reply, it will update round-trip time statistics: minimum, maximum, average, and standard deviation. Lost messages aren’t included in these statistics.   

Test client will keep statistics for all test servers with which communication was.

#### measuring messages bandwidth
- messages bandwidth can be measured only in the bulk test. It is assumed that all messages in the ping test have zero sizes, and all messages in the bulk test have specified sizes;  
- messages bandwidth statistics are kept separately from node/to node directions on the test server and total bandwidth only is measured on the test client;  
- messages bandwidth is the ratio of the total messages size (in the corresponding direction) and time from the start time to the finish time in the corresponding direction;  
- on the test client start time is time just before sending the network buffer descriptor to the test server (for the first bulk transfer);  
- on the test server start time in the “to node” direction is the time, when the first bulk transfer request was received from the test client, and in the “from node” direction it is time just before the bulk transfer request will be sent to the test client for the first time;  
- finish time is the time when the last bulk transfer (in the corresponding direction) is finished or it is considered that there was a message loss;  
- ideally, time in the “from test client to test server” direction must be measured from Tc0 to Ts4, and time in the “from a test server to test client”   direction must be measured from Ts5 to Tc8. But in the real world, we can only measure the time between Tc(i) and Tc(j) or between Ts(i) and Ts(j). Therefore always will be some errors and differences between test client statistics and test server statistics;  
- absolute value of the error is O(Ts1 - Tc0)(for the first message) + O(abs(Tc8 - Ts8))(for the last message);  
- with the message number increasing the relative error will tend to zero.

#### messages concurrency
Messages concurrency looks like the test client has a semaphore, which has several concurrent operations as its initial value. One thread will down this semaphore and send a message to the test client in a loop, and the other thread will up this semaphore when the reply message is received or the message is considered lost.

#### messages loss   
Message loss is determined using timeouts.  

#### message frequency  
Measure how many messages can be sent in a time interval.

### Bulk Test   
#### test client
The test client allocates a set of network buffers, used to receive replies from test servers. Then test client sends bulk data messages to all test servers (as a passive bulk sender) from the command line. After that, the test client will wait for the bulk transfer (as a passive bulk receiver) from the test server. Test clients can perform more than one concurrent send/receive to the same server.  

#### test server
Test server allocates a set of network buffers and then waits for a message from clients as an active bulk receiver. When the bulk data arrives, the test server will send it back to the test client as an active bulk sender.  

### Ping test   
#### test server
Test server waits for incoming test messages and simply sends them back.   

#### test client
Test client sends test messages to the server and waits for reply messages. If reply message isn't received within a timeout, then it is considered that the message is lost.  

### Conformance
- `[i.m0.net.self-test.statistics]` statistics from all nodes can be collected on the test console;  
- `[i.m0.net.self-test.statistics.live]`: statistics from all nodes can be collected on the test console at any time during the test;  
- `[i.m0.net.self-test.test.ping]`: latency is automatically measured for all messages;  
- `[i.m0.net.self-test.test.bulk]`: used messages with additional data;  
- `[i.m0.net.self-test.test.bulk.integrity.no-check]`: bulk messages additional data isn't checked;  
- `[i.m0.net.self-test.test.duration.simple]`: end-user should be able to specify how long a test should run, by loop;  
- `[i.m0.net.self-test.kernel]`: test client/server is implemented as a kernel module.  

## Use Cases  

### Scenarios
Scenario 1  

|Scenario 	| Description |  
|---------|-------------------------|  
|Scenario	| [usecase.net-test.test]  |
|Relevant |quality attributes	usability|
|Stimulus	|user starts the test|
|Stimulus source |	user |
|Environment	|network benchmark |
|Artifact	|test started and completed|
|Response	|benchmark statistics produced|
|Response measure|	statistics are consistent|  


### Failures  

#### network failure
Message loss is determined by timeout. If the message wasn't expected and if it did come, it is rejected. If some node isn't accessible from the console, it is assumed that all messages, associated with this node have been lost.  

#### test node failure
If the test node isn't accessible at the beginning of the test, it is assumed a network failure. Otherwise, the test console will try to reach it every time it uses other nodes.  

## Analysis  
### Scalability  

#### network buffer sharing
Single buffer (except timestamp/sequence number fields in the test message) can be shared between all bulk send/receive operations.  

#### statistics gathering
For a few tens of nodes pdsh can be used - scalability issues do not exists on this scale.  

### Rationale
pdsh was chosen as an instrument for starting/stopping/gathering purposes because of a few tens of nodes in the test. At a large scale, something else must be used.

## References
- [0] Motr Summary Requirements Table  
- [1] HLD of Motr LNet Transport  
- [2] Parallel Distributed Shell  
- [3] Round-trip time (RTT)  
