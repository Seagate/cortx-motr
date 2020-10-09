=======
Paxos
=======

Paxos algorithm implements "state machine" approach where a service is made fault tolerant by running multiple copies (replicas) of it on independently failing nodes and assuring that all replicas receive exactly the same sequence of inputs and pass through exactly the same sequence of internal states. To reach this goal, Paxos uses a distributed non-blocking consensus protocol---Synod protocol.

***************
Introduction
***************

Lamport [0] and Schneider [4] popularized a "state machine" approach as a unified method to add fault-tolerance to a distributed service. In this method a service running on a node has to be represented as an automaton accepting a sequence of "commands" from the clients across the network and moving from one state to another as a result of command processing. It's important that automaton state transitions depend on nothing but the sequence of input commands (they do not depend on the absolute time measurements).

To add fault tolerance to such service it is replicated:

- identical instances of service state machines are ran on multiple nodes;

- clients send their commands to the elected leader and the leader broadcasts commands to all instances;

- because state depends only on the sequence of input commands, all instances execute precisely the same state transitions;

- voting among instances is used by clients to determine the outcome of the commands.

For this to work, broadcasting has to satisfy following conditions:

- all non-faulty instances receive all the commands, and

- all instances receive commands in the same order.

At the heart of the Paxos algorithm is a distributed fault-tolerant consensus algorithm ("the Synod algorithm", described below) that allows instances to agree on a common value proposed by the leader. This algorithm is executed by the leader for every incoming command to reach an agreement on what command to execute next. That is, Synod algorithm uses (N, COMMAND) pair as a value, and agreement on this value means that all service instances promise to execute COMMAND as N-th command in the input sequence.

It might look that the description above is contradictory, because it assumes the existence of a leader and to select a leader participants have to agree on a common value (leader identity). Indeed, selection of a _unique_ leader is equivalent to distributed consensus problem, but Synod algorithm guarantees correctness even in the presence of multiple concurrent leaders. If multiple nodes claim to be leaders they can live-lock Synod progress for some transition time, until single leader is eventually elected, but they cannot violate its correctness. This is the crucial difference between the Synod algorithm and other would-be consensus algorithms like Three Phase Commit protocol that break in the face of multiple leaders. Note, that Paxos doesn't specify how the leader is elected, any existing algorithm, e.g., [2], can be used.

It must be added that Paxos alone is not sufficient to implement a fault tolerant distributed server as it doesn't deal with recovery of the failed replica.
