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

Synod Algorithm
===============

In the simplest form of Synod algorithm there is a set of "proposers"---nodes that might be leaders, and a set of "acceptors"---nodes that are to agree on a common value. Acceptors might coincide with the state machine instances, or can form a different set of nodes. The role of acceptors is to make consensus fault tolerant. That is, there are two "levels" of fault tolerance in the Paxos: replicated state machines make service fault tolerant and acceptors make state machine synchrony fault tolerant.

The algorithm is executed as a sequence of numbered "ballots", each ballot involving two phase communication between its originating proposer and some subset of acceptors. The ballot is either successful, in which case a common value is agreed upon, or it fails due to proposer failure, network failure, a failure of sufficiently large number of acceptors, or a conflict with another ballot, in which case a new ballot is eventually started. In the non-faulty case there is only one ballot.

Certain sub-sets of the set of all acceptors are designated as "quorums". It is required that any two quorums have a non-empty intersection. For example, one can define quorum as any set containing a majority of acceptors (any two majorities obviously intersect).

The idea of Synod algorithm is that a proposer first tries to propose its preferred value for some quorum of acceptors (this is called "phase 1" of the ballot). If acceptors and network are alive, the proposer receives enough responses from the acceptors and learns from them whether the proposal can be accepted without conflicting past and ongoing activity of other proposers. If there is a conflict, proposer abandons its own value and instead is forced to uphold certain value that it learns from the acceptor responses and that is not in conflict. In any case, proposer moves into "phase 2" where it sends messages to acceptors asking them to accept its value (either original or forced). Once a quorum of acceptors successfully replies to this proposal, the consensus on value has been reached, and proposer can broadcast this value to state machine instances.

For every instance of Synod algorithm each proposer maintains on its stable storage a record.

::

 struct proposer_state {

    /** maximal ballot number ever used by this proposer in

      this instance of Synod algorithm. */

    integer max_ballot;

 } proposer_state;
 
And every acceptor maintains a record

 ::
 
  struct acceptor_state {

     /** number of highest numbered ballot ACCEPTED by

       this acceptor. */

    integer max_accepted_ballot;

    /** value of highest numbered ballot ACCEPTED by

    this acceptor. */

    value_t max_accepted_value;

    /** number of highest numbered ballot PREPARED by

    this acceptor. */

   integer max_prepared_ballot;

  } acceptor_state;
  
 An operation of synchronously committing a record to the stable storage is denoted by fsync(record). Message M with arguments (A0, ..., AN) sent from node P to node Q is denoted as

- M(A0, ..., AN) : P -> Q

A statement "On M(A0, ..., AN) : P -> Q { ... }" is executed on Q when a matching message is received there.

For simplicity we assume that the number of proposers is fixed and known in advance. This allows to easily generate unique ballot numbers: i-th proposer generates numbers in the sequence.

- i, i + nr_proposers, i + 2 * nr_proposers, ...

It's assumed that proposer_state.max_ballot is initialized with a suitable "proposer number".

With these preliminaries Synod algorithm can be described as:

/** Synod algorithm for proposer P initially trying to

propose value our_value.*/

Proposer(node_t P, value_t our_value):

value_t chosen;

integer max_ballot;

chosen = our_value;

max_ballot = -1;

Phase1: { /* Here execution starts when Synod is invoked or

when a proposer fails and restarts.*/

/* select a unique ballot number*/

proposer_state.max_ballot += nr_proposers;

fsync(&proposer_state);

/* select some quorum of acceptors*/

Q0 = a_quorum(acceptors);

for_each(A in Q0)

PREPARE(proposer_state.max_ballot) : P -> A;

}

Phase 2 {

On PREPARE_ACK(ballot, value) : A -> P {

if (ballot != NIL) {

/*

* Acceptor A already accepted a ballot, find

* highest numbered ballot accepted anywhere

* in the quorum.



if (ballot > max_ballot) {

max_ballot = ballot;

chosen = value;

}

} else {

/* Acceptor A hasn't yet accepted any ballot.*/

}

}

On PREPARE_NACK : A -> P { restart };

On timeout for PREPARE { restart };

/*

* Proposer received successful replies from the quorum. If

* none of acceptors accepted a value in any other ballot,

* phase 2 proposes original value our_value; otherwise the value

* from the highest numbered accepted ballot is proposed.

* Propose the value.


Q1 = a_quorum(acceptors);/* This might be different from Q0*/

for_each(A in Q1)

ACCEPT(proposer_state.max_ballot, chosen) : P -> A;

On ACCEPT_ACK(ballot) : A -> P {;}

On PREPARE_NACK : A -> P { restart };

On timeout for PREPARE { restart };

/*

* ACCEPT_ACK messages were received from the

* quorum. Consensus has been reached.


}

Acceptor(node_t A):

On PREPARE(ballot) : P -> A {

if (ballot > acceptor_state.max_prepared_ballot) {

/*

* Acceptor received new highest numbered

* ballot. Remember this and reply. Acceptance of a

* ballot N extracts from acceptor a promise to not

* accept any ballot with number less than N.


acceptor_state.max_prepared_ballot = ballot;

fsync(&acceptor_state);

PREPARE_ACK(acceptor_state.max_accepted_ballot,

acceptor_state.max_accepted_value) : A -> P

} else

/*

* Be faithful to this node's previous promise to not

* accept lower numbered ballots.


PREPARE_NACK : A -> P

}

On ACCEPT(ballot, value) : P -> A {

if (ballot >= acceptor_state.max_prepared_ballot) {

acceptor_state.max_accepted_ballot = ballot;

acceptor_state.max_accepted_value = value;

fsync(&acceptor_state);

ACCEPT_ACK(ballot) : A -> P

} else

ACCEPT_NACK : A -> P

}


