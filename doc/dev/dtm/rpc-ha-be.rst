=========
DTM model
=========

:author: Nikita Danilov <nikita.danilov@seagate.com>
:state: INIT
:copyright: Seagate
:distribution: unlimited
:url: https://github.com/Seagate/cortx-motr/blob/documentation/doc/dev/dtm/rpc-ha-be.rst

Overview
========

To be able to model, test and verify dtm, one has to specify its algorithms and
protocols abstractly. Because dtm interacts with other TP components, these
components should be specified abstractly. This document provides basic
specification of main components with which dtm interacts: rpc, ha and BE
transaction engine.

Assumptions about components are specified in the form of axioms.

Model
=====

The model consists of *processes* (TP instances), which exchange rpc *messages*
(one-way or request-reply). Some processes have *persistent store* and use local
BE transaction engines to update the store.

The system experiences *faults*: minor network loss (packet loss, delays,
partially masked by rpc), major network loss (partitions), minor storage loss
(bit rot, misdirected writes, ghost writes), major storage loss (persistent
store loses its contents), process crash (node reboot, abnormal process
termination). When a process recovers after a crash, it loses its volatile store
(memory), but persistent store is intact up to a transaction boundary (see
below).

Faults are events that actually happen. HA sub-system maps faults to
*failures*. The rest of the system trusts HA. HA can miss some faults and can
introduce spurious failures not corresponding to any fault.

Processes, messages, stores and events have *attributes*.

Notation
========

Not very formal predicate calculus is used to express various conditions. ``A``
and ``E`` are used as the universal and existential quantifiers
respectively. ``^``, ``|`` and ``~`` denote conjunction, disjunction and
negation respectively.

Method invocation notation ``x.f(y, z, ...)`` is used along with functional
notation ``f(x, y, z, ...)`` for events and attributes to stress their main
parameter.

If ``P(x, y, ...)`` is any condition then ``P(x, y, ...) @t`` means condition
"``P(x, y, ...)`` was or is or will-be true at time ``t``", where ``t`` is is
physical time (global and non-relativistic). Physical time is not observable in
the actual system and only exists in the model.

If ``V(a, b, ...)`` is an event, then ``[V(a, b, ...)]`` denotes the condition
"``V(a, b, ...)`` happened". For example,::

    E dt < TIMEOUT . [tx.committed] @(t0 + dt)

is true iff ``tx`` is committed before ``TIMEOUT`` elapses after ``t0``. It is
assumed that exactly one event, denoted ``at(t)`` happens at a given moment in
time (this assumption does not lead to a loss of generality, because time has
infinite precision). If nothing interesting happened at ``t``, then ``at(t)`` is
an idle event. Therefore::

    *E0*: ([e] @t) = (e = at(t))

(Or, equivalently, ``[at(t)] @t``.)

Symmetrically, ``e.time`` is the time when event ``e`` happened (or nil if
never), so that::

    *E1*: at(t).time = t

``bolt`` denotes current physical time so that ``P @t`` is the same as ``P[bolt
:= t]``, where ``[v := exp]`` is the substitution operator, replacing all
occurrences of ``v`` with ``exp``.

An event is either observable (the event is actually a call-back provided by a
component to which dtm can subscribe) or un-observable.

An event is either triggerable (there is an interface entry-point that dtm can
call to cause the event) or not.

Processes
=========

Attributes
----------

* ``p.gen`` : integer, "boot counter" incremented on each process restart

* ``p.store`` : process' persistent store, or nil if none

Events
------

* ``p.crash`` : ``p`` crashed, fault, triggerable, un-observable

* ``p.start`` : ``p`` restarts, fault, un-triggerable, observable

Message
=======

Attributes
----------

* ``m.src`` : process, sender process

* ``m.dst`` : process, receiver process

* ``m.reply`` : message, the reply to the request ``m``

* ``m.isreq`` : bool, the message is a request requiring a reply

* ``m.isrep`` : bool, the message is a reply

* ``m.isone`` : bool, the message is one way

Events
------

If ``p`` is a process and ``m`` is a message, then ``p.send(m)`` denotes the
event of sending ``m`` and ``p.recv(m)`` denotes the event of receiving ``m``.

* ``p.send(m)`` : observable, triggerable
* ``p.recv(m)`` : observable, non-triggerable

Axioms
------

Let ``last(P)`` be the last moment in time when the predicate ``P`` holds or
beginning of time ``START`` is not such moment exists:

::

    (P @last(P) ^ (A t > last(P) . ~P)) | (last(P) = START ^ ~P @START)

(Note: this is not always well-defined, but do not worry.)

Let ``in(start, end, P)`` mean that ``P`` holds sometime in the time interval
``[start, end)``:

::

    in(start, end, P) = E t. ((start <= t) ^ (t < end) ^ (P @t))

Then

::

    *M0*: [p.recv(m)] @t0 -> (m.dst = p) ^ E t1 < t0 . ([m.src.send(m)] @t1)

(If a message is received, it was sent earlier and is received at the correct
receiver.)

::

    *M1*: ([p.send(m.reply)] @t0) -> ([p.recv(m)] @t1) ^ (t1 < t0)

(Reply can be sent only after the request message was received.)

::

    *M2*: m.isreq XOR m.isrep XOR m.isone

(A message is either request or reply or one-way.)

::

    *M3*: (m.reply != nil) = m.isreq

::

    *M4*: (m.reply != nil) -> m.reply.isrep ^ (m.dst = m.reply.src) ^
                                              (m.src = m.reply.dst)

::

    *M5*: r.isrep = E m . m.reply = r

::

    *M6*: [p.send(m)] -> (m.src = p)

::

    *M7*: ([p.send(m0)] @t0) ^ ([p.send(m1)] @t1) ^
          ([q.recv(m0)] @q0) ^ ([q.recv(m1)] @q1) ^
	  m0.isreq ^ m1.isreq ^ (t0 < t1) -> (q0 < q1)

(Requests between a pair of processes are delivered in order.)

::

   *M8*: ([p.recv(m)] @t0) ^ ([p.recv(m)] @t1)  ^ m.isreq ->
                                       (t0 = t1) | in(t0, t1, [p.crash])

(In the absence of crashes, duplicate requests are suppressed.)

A good rpc module would provide a stronger axiom:

::

   *M8a*: ([p.recv(m)] @t0) ^ ([p.recv(m)] @t1) ^ m.isreq -> (t0 = t1)

(Duplicate requests are suppressed.)

Let's see whether *M8a* is needed.

::

   *M9*: ([p.send(m)] @t0) ^ ([q.send(m)] @t1) -> (t0 = t1) ^ (p = q)

(A message can be sent only once.)

HA
==

Events
------

* ``state(p, mode)`` : HA declares that process ``p`` state is ``mode``, where
  ``mode`` is either ``TRANSIENT``, ``PERMANENT`` or ``ONLINE``. This
  un-observable, non-triggerable event means that HA made a decision about ``p``
  state and made this decision persistent within HA. For brevity,

* ``transient(p) = state(p, TRANSIENT)``

* ``permanent(p) = state(p, PERMANENT)``

* ``online(p)    = state(p, ONLINE)``

Axioms
------

::

    *H0*: [p.start] @t0 -> (E g. ((p.gen = g) @last(t < t0 ^ [p.crash] @t) ^
                                  (p.gen = g + 1) @t0)) ^
                           last(t < t0 ^ [p.crash] @t) != START

(``last(t < t0 ^ [p.crash] @t)`` is the time of the last crash preceding
``t0``. The axiom guarantees that re-boot counter increases between crash and
restart.)

The following 5 axioms describe the internal HA state machine, un-observable
outside of HA.

::

   *H1*: [p.transient] @t -> in(t, t + TIMEOUT, [p.permanent] | [p.online])

(A transiently failed process either goes back online or dies permanently within
a certain timeout.)

::

   *H2*: [p.permanent] @t -> E t0 < t . (([p.transient] @t0) ^
                                         ~in(t0, t, [p.online]))

(A permanent failure is always preceded by a transient failure.)

::

   *H3*: [p.transient] @t -> E t0 < t . (([p.online] @t0) ^
                                         ~in(t0, t, [p.transient]))

(A transient failure is always preceded by online state.)

::

   *H4*: [p.permanent] @t -> ~in(t, END, [p.transient] | [p.online])

(A permanent failure is final: no further state transitions.)

::

   *H5*: ([p.state(m0)] @t0) ^ ([p.state(m1)] @t1) ^ (t0 < t1) ^
           ~in(t0, t1, [p.state(m)]) -> (m0 != m1)

(Process state changes in state change events.)

``state(p, mode)`` events are un-observable. After this HA-internal event
happens, HA notifies processes about the state change. This notification is
modelled by introducing a fictitious HA process ``hap``.

::

   *H6*: [p.state(mode)] @t = in(t, t + TIMEOUT,
                                 [hap.send(REQ(hap, q, <STATE, p, mode>))])

(If HA decides that a process changes state, it sends notifications about this
to all processes (including the failed one and ``hap``) within a certain
timeout.)

Here ``REQ(src, dst, payload)`` means a request message with given source,
destination and payload.

*H5* implies that if ``p`` experiences next failure before all processes were
notified about the previous one, the processes are still notified about both
failures.

::

   *H7*: ~[hap.transient]

(HA process never fails.)

It is also implicitly assumed that at the beginning of system history
``p.online`` happens for all processes and notifications about this are
successfully delivered.

Let ``thinks(p, q, mode)`` mean that as far as ``p`` knows, ``q`` has HA-state
``mode``:

::

   p.thinks(q, mode) @t = E t0 < t. ([p.recv(REQ(hap, p, <STATE, q, mode>)] @t0) ^
                            ~in(t0, t, [p.recv(REQ(hap, p, <STATE, q, *>))]))

(The latest state update notification for ``q`` that ``p`` received from ``hap``
was ``mode``.)

Now, an important axiom can be formulated:

::

    *H8*: [p.send(m)] @t ^ m.isreq -> in(t, t + TIMEOUT, [p.recv(m.reply)] |
					 hap.thinks(p, TRANSIENT) |
                                         p.thinks(m.dst, PERMANENT))

(If a request was sent, then within a certain timeout, the reply is received, or
the sender learns that the receiver failed permanently, or the sender fails.)

::

   *H9*: ([p.transient] @t0) ^ ([p.online] @t1) ^ (t0 < t1) ->
             in(t0, t1, [p.recv(REQ(hap, p, <STATE, p, TRANSIENT>))])

(Before a process goes back online, it is informed by HA that it was considered
TRANSIENT.)

Persistent store
================

Process' persistent store is modelled as a set of key-value pairs, updated via
transactions.

Attributes
----------

* ``tx.id`` : integer, a unique transaction identifier.

* ``tx.store`` : persistent store. Persistent store of the transaction.

* ``s.get(k)`` : value associated with the key.

* ``s.process`` : the process to which the persistent store is attached.

Events
------

* ``tx.state(s)`` : changes transaction state. State can be ``OPEN``, ``CLOSE``,
  ``LOGGED`` or ``COMMITTED``. The states are consecutive integers in the order
  indicated. Observability and triggerability of state change depends on the
  state:

  * ``OPEN`` : opens a new transaction. Observable, triggerable.

  * ``CLOSE`` : closes a transaction. Observable, triggerable.

  * ``LOGGED`` : transaction becomes persistently logged and will survive
    process crashes. Un-observable, non-triggerable.

  * ``COMMITTED`` : notification about transaction becoming
    persistent. Observable, non-triggerable.

* ``tx.set(k, v)`` : set key's value as part of transaction tx. Observable,
  triggerable.

Axioms
------

Let's define a few auxiliary predicates:

::

   tx.is(s) @t = E t0 < t . ([tx.state(s)] @t0) ^ ~in(t0, t, [tx.state(s1)])

(Last transaction state is ``s``).

::

   tx.atleast(s) = E p >= s . tx.is(p)

(Transaction reached at least state ``s``).

::

   tx.start = [tx.state(OPEN)].time

(The time when the transaction opens.)

::

   tx.lost @t = in(tx.start, t, [tx.store.process.crash] ^ ~tx.atleast(LOGGED))

(An un-logged transaction is lost in a process crash.)

::

   *P0*: (s0.process = s1.process) = (s0 = s1)

(Different processes have different stores.)

::

   *P1*: E t . ([tx.state(OPEN)] @t)

(A transaction has to be opened first.)

::

   *P2*: ([tx.state(s0)] @t0) ^ (s0 > OPEN) -> E t1 < t0 .
             ([tx.state(s0 - 1)] @t1) ^ ~in(t0, t1, [tx.state(s)])

(Transaction state increases monotonically without gaps.)

There is also a progress axiom::

   *P2.a*: (tx.is(CLOSED) @t) ^ ~in(t, t + TIMEOUT, [tx.store.process.crash]) ->
             (tx.atleast(COMMITTED) @(t + TIMEOUT))

(If the process does not crash, a closed transaction eventually commits.)

::

   *P3*: [tx.set(k, v)] -> tx.is(OPEN)

(Only opened transactions can mutate the store.)

::

   *P4*: tx0.atleast(LOGGED) ^ ([tx0.set(k, v0)] @t0) ^ ([tx1.set(k, v1)] @t1) ^
         t1 < t0 -> tx1.atleast(LOGGED)

(Transactions respect dependencies: if 2 transactions modify the same key, they
are logged in order.)

::

   *P5*: ([tx.state(s)] | [tx.set(k, v)]) @t ->
                                 ~in(tx.start, t, [tx.store.process.crash])

(A transaction cannot be manipulated after a process failure.)

The following defines persistent store key-value semantics:

::

   *P6*: (s.get(k) = v) @t = E t0 < t, tx0 .
           (([tx0.set(k, v)] @t0) ^ (tx0.store = s) ^ ~(tx0.lost @t) ^
	    (A t1, tx1, v1 . (((t0 < t1) ^ (t1 < t) ^ ([tx1.set(k, v1)] @t1)) ->
               tx1.lost @t)))

(The value associated with a key, is the value assigned by the last non-lost
transaction.)

Model limitations
=================

The model ignores certain aspects of the system.

* It is assumed that a process has at most 1 persistent store attached to it. In
  an actual system, a server process can manage multiple devices. The difference
  is important when *spare devices* are considered, but it is felt that this
  difference is not essential to the analysis of the core dtm algorithms.

* In the actual system communication between a client and a server takes a form
  of an rpc followed by a *bulk transfer*. The model abstracts this 2 phase
  protocol into single request send-receive. While bulk transfers affect
  life-time requirements of buffers in a critical way, they can be ignored
  during analysis of the basic transaction algorithms.

DTM
===

DTM is modelled as a collection of state machines that react to observable
events and invoke triggerable events.

.. list-table:: observable events
   :header-rows: 1

   * - event
     - description
     - observable where
   * - ``p.start``
     - process restarts after a crash
     - ``p``
   * - ``p.send(m)``
     - process sends a message
     - ``p``
   * - ``p.recv(m)``
     - process receives a message
     - ``p``
   * - ``tx.state(OPEN)``
     - transaction opens
     - ``tx.store.process``
   * - ``tx.state(CLOSE)``
     - transaction closes
     - ``tx.store.process``
   * - ``tx.state(COMMITTED)``
     - transaction has been logged
     - ``tx.store.process``
   * - ``tx.set(k, v)``
     - transaction sets a key to a value
     - ``tx.store.process``

   
.. list-table:: triggerable events
   :header-rows: 1

   * - event
     - description
     - triggerable where
   * - ``p.crash``
     - crash a process
     - ``p``
   * - ``p.send(m)``
     - process sends a message
     - ``p``
   * - ``tx.state(OPEN)``
     - transaction opens
     - ``tx.store.process``
   * - ``tx.state(CLOSE)``
     - transaction closes
     - ``tx.store.process``
   * - ``tx.set(k, v)``
     - transaction sets a key to a value
     - ``tx.store.process``


.. list-table:: observable attributes
   :header-rows: 1

   * - attribute
     - description
     - observable where
   * - ``p.gen``
     - process boot counter
     - ``p``
   * - ``p.store``
     - process' persistent store
     - ``p``
   * - ``m.{src,dst,isreq,isrep,isone}``
     - message attributes
     - ``m.src``. ``m.dst`` after ``[m.dst.recv(m)]``
   * - ``m.reply``
     - reply
     - ``m.dst`` after ``[m.dst.recv(m)]``. ``m.src`` after
       ``[m.src.recv(m.reply)]``
   * - ``tx.id``
     - transaction identifier
     - ``tx.store.process``
   * - ``tx.store``
     - persistent store of a transaction
     - ``tx.store.process``
   * - ``store.process``
     - process to which a persistent store is attached
     - ``store.process``
   * - ``store.get(k)``
     - value associated with a key
     - ``store.process``


Example
-------

Below is a very simple example or a client and server that can set and get a
specific key.

::

   client {
        s : message;
        g : message;

        set(value) {
	        s = REQ(client, server, <SET, value>);
		send(m);
        }

        on(recv(s.reply)) {
	        done();
	}

        get() : value {
	        g = REQ(client, server, <GET>);
		send(g);
	}

        on(recv(g.reply)) {
	        assert g.reply matches REPLY(value);
		got(value);
	}
   };

   server {
        tx : transaction;
	s  : message;

        on(recv(m), m matches REQ(client, server, <SET, value>)) {
	        s = m;
	        tx = server.store.tx_new();
	        tx.open();
		tx.set(KEY, value);
	        tx.close();
	}
        on(tx.state(COMMITTED)) {
		send(s.reply);
	}
	
        on(recv(g), m matches REQ(client, server, <GET>)) {
		g.reply = REPLY(server.store.get(KEY));
		send(g.reply);
	}
   };

Client is used by calling client.set(v0), then waiting for client.done() to be
called, then calling client.get() and waiting for client.got(v1) to be
called. That is::

    ([client.set(v0)] @t0) ^                     /* *O1* */
    ([client.done()]  @t1) ^ (t0 < t1) ^         /* *O2* */
    ([client.get()]   @t2) ^ (t1 < t2) ^         /* *O3* */
    ([client.got(v1)] @t3) ^ (t2 < t3)           /* *O4* */

And no other client events occurred within ``[t0, t3]``, except for possible
crashes (*NO*).

Let's prove that ``v0`` equals ``v1``.

* ``[client.got(v1)] @t3``
* The only place where ``client.got()`` is called is ``on(recv(g.reply))``
* ``[client.recv(g.reply)] @t3.0 ^ (t3.0 < t3)``
* by *M0*
* ``(g.reply.dst = client) ^ E t3.1 < t3.0 . ([g.reply.src.send(g.reply)] @t3.1)``
* by *M4*
* ``(g.src = client) ^ E t3.1 < t3 . ([g.dst.send(g.reply)] @t3.1)`` [*X0*]
* by *M1*
* ``([g.dst.recv(g)] @t3.2) ^ (t3.2 < t3.1)``
* by *M0*
* ``(g.dst = g.dst) ^ E t3.3 < t3.2 . ([g.src.send(g)] @t3.3)``
* by *X0*
* ``E t3.3 < t3.2 . ([client.send(g)] @t3.3)``
* by *NO* and *O3* and code of ``client.get()``
* ``([client.get()] @t2) ^ (t1 < t2) ^ (t2 < t3.3)``
* by *O2*
* ``([client.done()]  @t1)``
* The only place where ``client.done()`` is invoked is ``on(recv(s.reply))``
* ``[client.recv(s.reply)] @t1.1 ^ t1.1 < t1``
* by *M0*
* ``(s.reply.dst = client) ^ E t1.2 < t1.1 . ([s.reply.src.send(s.reply)] @t1.2)``
* by *M4*
* ``(s.src = client) ^ E t1.2 < t1.1 . ([s.dst.send(s.reply)] @t1.2)`` [*X1*]
* by *M1*
* ``([s.dst.recv(s)] @t1.3) ^ (t1.3 < t1.2)``
* by *M0*
* ``(s.dst = s.dst) ^ E t1.4 < t1.3 . ([s.src.send(s)] @t1.4)``
* by *X1*
* ``E t1.4 < t1.3 . ([client.send(s)] @t1.4)``
* by *NO* and *O1* and *O2* code of ``client.set()``
* ``([client.set(v0)] @t0) ^ (t0 < t1.4)``

The following history of events has just been established::

  [client.set(v0)] @t0
  [client.send(s)] @t1.4
  [server.recv(s)] @t1.3
  [server.send(s.reply)] @t1.2
  [client.recv(s.reply)] @t1.1
  [client.done()]  @t1
  [client.get()] @t2
  [client.send(g)] @t3.3
  [server.recv(g)] @t3.2
  [server.send(g.reply)] @t3.1
  [client.recv(g.reply)] @t3.0
  [client.got(v1)] @t3

By using transaction axioms *P2* and *P3* we can include transaction events in
the history::

  [client.set(v0)] @t0
  [client.send(s)] @t1.4
  [server.recv(s)] @t1.3
      [tx.state(OPEN)] @t1.3.1
      [tx.set(KEY, v0)] @t1.3.2
      [tx.state(CLOSE)] @t1.3.3
      [tx.state(LOGGED)] @t1.3.4
      [tx.state(COMMITTED)] @t1.3.5
  [server.send(s.reply)] @t1.2
  [client.recv(s.reply)] @t1.1
  [client.done()]  @t1
  [client.get()] @t2
  [client.send(g)] @t3.3
  [server.recv(g)] @t3.2
      [server.store.get(KEY)] @t3.2.1
  [server.send(g.reply)] @t3.1
  [client.recv(g.reply)] @t3.0
  [client.got(v1)] @t3

By *P5* there can be no ``server.crash`` in ``[t1.3.1, t1.3.5]``. Therefore,
``~tx.lost @t3.2.1`` (``tx`` was logged before any possible crash). Because of
this, ``tx`` was the last non-lost transaction that updated ``KEY`` and by *P6*
``v0 = v1``. QED.

DTM0
----

Next, consider a distributed transaction state machine:

* An originator mode initiates a transaction that must atomically replicate
  given value across ``K + 1`` participants (that is, the transaction has ``K +
  2`` participants total.).

* Participants other than originator has persistent store. Originator does not.

* Originator never recovers from a crash. But originator can recover from a
  transient failure, as observed by HA and other participants. These transient
  failures can be, for example, due to network partitions.
  
* Only a single transaction is implemented, the state machines terminate after
  it is processed.

* Redundancy model is replication (``N = 1`` in motr terms.).

* Transaction has no result.

::

    enum tstate { NONE, VOLATILE, PERSISTENT };
    participant {
            service    : process;  /* Service and device. */
	    state      : tstate;   /* Local transaction state. */
    };

    txrecord {
            cohort     : participant[];
	    val        : any;
    };

    node : process {
            txr : txrecord;
	    tx  : transaction;
    };

    K : int;
    n : node[K + 2];
    
    /* Global initialisation. */
    n[0].txr = {
            .cohort = {
	            [0]     = { n[0], true, false, VOLATILE }, /* Originator. */
		    [1]     = { n[1], false, true, NONE },
		    ...
		    [K + 1] = { n[K + 1], false, true, NONE }
	    }
	    .val = V;
    };
    n[0].balance();

    node.balance() {
	    if (coordinator() != self || txr == nil)
		    return;
            for (i : txr.cohort) {
		    participant p = txr.cohort[i];
	            if (p.service != self && p.state < PERSISTENT) {
			    send(REQ(self, p, <TXR, txr>));
			    p.state = VOLATILE;
		    }
	    }
    }

    node.isstable() : bool {
            for (i : txr.cohort) {
		    participant p = txr.cohort[i];
		    if (!failed(p) && p.state != PERSISTENT && i != 0)
		            return false;
            }
	    return true;
    }
    
    node.coordinator() : process {
            for (i : txr.cohort) {
	            if (!failed(txr.cohort[i].service) &&
		        txr.cohort[i].state >= PERSISTENT)
		            return txr.cohort[i].service;
	    }
	    /* If there are no (non-failed) persistent replicas, everybody leads. */
	    return self;
    }

    node.failed(process) {
            return self.think(process, PERMANENT);
    }

    node.on(recv(REQ(n[idx], self, <TXR, t>))) {
            if (txr == nil) {
	            txr = t;
		    if (self.store != nil) {
		            tx = self.store.tx_new();
		            tx.open();
		            tx.set(TXR, txr);
		            tx.close();
		    }
	    } else { /* Merge received txr. */
	            assert(t.val == txr.val);
		    txr.cohort[idx].state = t.cohort[idx].state;
	    }
	    balance();
    }

    node.on(recv(REQ(hap, self, <STATE, n[idx], PERMANENT>))) {
            balance();
    }

    node.on(recv(REQ(hap, self, <STATE, n[idx], ONLINE>))) {
            participant p = txr.cohort[idx];
	    if (p.state < PERSISTENT)
	            p.state = NONE;
            balance();
    }

    n[idx].on(tx.state(COMMITTED)) {
            for (i : txr.cohort) {
		    participant p = txr.cohort[i];
	            if (p.service != self)
			    send(REQ(self, p, <TXSTATE, txr.cohort[idx].state>));
		    else
		            p.state = PERSISTENT;
	    }
    }

    node.on(recv(REQ(n[idx], self, <TXSTATE, state>))) {
            txr.cohort[idx].state = state;
	    balance();
    }

    node.on(crash) {
            txr = nil;
    }

    n[idx].on(start) {
            assert self.store != nil;
            txr = self.store.get(TXR);
	    if (txr != nil)
	            txr.cohort[idx].state = PERSISTENT;
    }

    node.stable() {
    }


The state machine above is intended to provide atomicity, that is to always end
up in a state, where the transaction is executed either everywhere, or nowhere::

    (E t . A p . (hastate(p) != PERMANENT ^ p != originator ->
                  p.store.get(TXR).val = V) @t) |     /* E t. EVERYWHERE(t) */
    (E t . A p . ((hastate(p) != PERMANENT ->         /* E t. NOWHERE(t) */
                  p.txr = nil ^ (p != originator -> p.store.get(TXR) = nil) ^
		  ~in(t, END, [p.recv(TXR)]) 

That is, either every non-failed process has the transaction value logged, or
none of the non-failed processes has the transaction in volatile or persistent
store and there are no pending messages.

Unfortunately, as it is, the state machine above cannot guarantee atomicity. It
cannot even guarantee termination: it is possible that due to an infinite series
of frequent restarts, processes ping-pong the transaction record between them,
but no process manages to live long enough without crashing to log the
transaction.

To deal with this an additional assumption (fairly common on the literature) is
made, that after a finite time (and a finite number of state transitions), there
are no further failures::

    *S0*: E t . (A p, s . ~in(t, END, [p.state(s)]))

Before embarking on the prof of atomicity, note, that atomicity is a safety
property and as such is not very useful in isolation. For example, a much
simpler state machine that immediately discards the transaction achieves
atomicity (specifically, it establishes ``NOWHERE``). In addition, one has to
prove some liveness property, *e.g.*, that in the absence of failures,
``EVERYWHERE`` is eventually established.

First, observe that if there are no failures, the algorithm obviously achieves
atomicity.

Let's assume that there was at least one failure and prove atomicity by
contradiction, that is assume that atomicity is violated:
``(A t. ~EVERYWHERE(t)) ^ (A t. ~NOWHERE(t))``.

First, observe that if a non-crashed participant has ``txr`` in its
persistent store, it also has it in its volatile store (that is, ``self.txr !=
nil``):

* when ``txr`` is placed in the persistent store (on receipt of ``TXR``
  message), it is already in the volatile store;

* the only place where the volatile store is lost is ``node.on(crash)``;

* after a process restarts, it fetches ``txr`` from the persistent store before
  doing anything else: ``n[idx].on(start)``.

Now, let ``t0`` be the time of the last HA process state change event, which
exists by *S0*.  Because there are no further failures, all HA state
notifications will be delivered before some ``t1 = t0 + TIMEOUT``.

By assumption, ``~NOWHERE(t1)``, that is, there is a non-faulty process ``p``
such that

::

    (p.txr != nil | (p != originator ^ p.store.get(TXR) != nil) |
		   in(t, END, [p.recv(TXR)])) @t1

If ``p.txr != nil | (p != originator ^ p.store.get(TXR) != nil)`` holds at
``t1`` for any process, let ``t2 = t1``. Otherwise (txr only exists in transit
over network at ``t1``), some ``p`` will receive a ``TXR`` message. Let ``t2``
be the time when the execution of ``p.on(recv(<TXR, t>))`` handler completes
(``t2`` is well-defined because there are no crashes after ``t0``). By that time
``p`` has txr in its volatile store.

In any case, at ``t2`` there is a process that has txr in its volatile
store. Let ``p`` be such processes with the smallest index.

If ``p`` had txr at ``t0``, it received the notification about the last failure
before ``t1``. This notification must be either ``ONLINE`` or ``PERMANENT``,
because a transient failure cannot be the last one. ``node.balance()`` was
executed by ``p`` as part of notification handler.

If ``p`` received txr via a ``TXR`` message after ``t0``, ``node.balance()`` was
executed as part of receipt handler.

In any case, ``p`` executed ``node.balance()`` when it has txr and after
``t0``. Because ``p`` has the smallest index of all non-faulty processes with
txr, ``node.coordinator()`` is true for ``p`` and ``node.balance()`` will send
``TXR`` messages to all participants that do not have persistent copies. Because
there are no further failures, these messages will be delivered by some time
``t3`` and by *P2.a*, txr-s will becomes persistent everywhere by ``t4``
violating ``~EVERYWHERE(t4)``. Contradiction. Therefore atomicity is
guaranteed. QED.

..  LocalWords:  triggerable dtm disjunction atomicity liveness
