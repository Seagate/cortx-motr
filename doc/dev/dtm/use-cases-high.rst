Agenda
======

High-level description of dtm use-cases for 2020/12.

Included:

* layout

* failure model

* hare

* spares

* dix, ios

Not included:

* versions

* recovery algorithm

* fol pruning, stability

Layout
======

Consider parity de-clustered network RAID (pdclust, sns). Each object and index
has a layout, specified by 3 parameters:

* N: number of data units in a parity group,

* K: number of parity units in a parity group,

* S: number of spare units in a parity group.

The layout (also known as striping pattern) is denoted N+K+S.

N is the number of units in a group necessary to reconstruct the entire group.

At most N+K units are actually used (allocated) at any moment in time.

At most S repairs can be completed before a replacement is needed.

Repair
------

Repair is a process where lost data are reconstructed and stored in distributed
spare space. Repair is invoked when a device, or controller, or enclosure or
site is lost. Repair is fast and becomes faster as the cluster grows larger,
because during repair all surviving devices do (mostly sequential) reads and
writes.

Re-balance
----------

Re-balance is a process where lost data are reconstructed and stored in a
replacement (device, controller, enclosure, *etc*.). Re-balance is slower than
repair, because writes are done only by the replacement target.

After a failure, one can first execute repair, then, when the replacement is
installed, re-balance (from distributed spare space to the replacement
target). Alternatively, repair can be skipped, and re-balance, called direct
re-balance in this case, is done directly from surviving data to the replacement
target.
  
Failure model
=============

States

* online,

* transient (failure),

* permanent (failure).

Software
--------

A motr instance (m0d, motr.ko or libmotr) experiences a transient failure when
it stops servicing incoming network requests. This can be because of a
dead-lock, process crash, network partition, bug in the network stack, *etc*.

After some known timeout, a transiently failed instance either recovers (is
again able to serve incoming requests), or becomes permanently failed.

An instance can be recovered on a different hardware, i.e., a process can be
migrated to a different server node having access to the same storage.

Hardware
--------

Similarly, a storage hardware (disk, controller, enclosure, *etc*.) can
experience transient and permanent failures. When storage fails permanently, its
contents is considered lost.

Assumptions
-----------

A cluster is divided into pools. An object is stored within a pool. N+K+S
parameters are associated with a pool and used by all objects in the pool.

Due to definition of these parameters, a pool can survive up to K simultaneous
permanent storage failures, where a failure ends when repair completes (if
repair is used) or direct re-balance completes (when repair is not used).

Use-cases
=========

Dix
---

1+1+0
~~~~~

Failure free

.. uml::

   App -[#white]> c
   c   -[#white]> d0
   d0  -[#white]> d1
   App  -> c ++ : PUT(idx, k, v)
           c  -> d0        : PUT(ctg0, k, v)
 	   c         -> d1 : PUT(ctg1, k, v)
           c  <- d0        : RET(rc)
   App <-- c               : EXEC
           c        <-- d1 : RET(rc)
   App <-- c --            : STABLE

Transient

.. uml::

   App -[#white]> c
   c   -[#white]> d0
   d0  -[#white]> d1
   activate             d1 #a0a0a0
   App  -> c ++ : PUT(idx, k, v)
           c  -> d0        : PUT(ctg0, k, v)
           c  <- d0        : RET(rc)
   App <-- c               : EXEC
   deactivate           d1
 	   c         -> d1 : PUT(ctg1, k, v)
           c        <-- d1 : RET(rc)
   App <-- c --            : STABLE

Permanent

.. uml::

   App -[#white]> c
   c   -[#white]> d0
   d0  -[#white]> d1
   activate             d1 #606060
   App  -> c ++ : PUT(idx, k, v)
           c  -> d0        : PUT(ctg0, k, v)
           c  <- d0        : RET(rc)
   App <-- c               : EXEC
   App <-- c --            : STABLE
   deactivate           d1

Transient -> Permanent

.. uml::

   App -[#white]> c
   c   -[#white]> d0
   d0  -[#white]> d1
   activate             d1 #a0a0a0
   App  -> c ++ : PUT(idx, k, v)
           c  -> d0        : PUT(ctg0, k, v)
           c  <- d0        : RET(rc)
   App <-- c               : EXEC
   activate             d1 #606060
   App <-- c --            : STABLE

Originator failure

.. uml::

   App -[#white]> c
   c   -[#white]> d0
   d0  -[#white]> d1
   App  -> c ++ : PUT(idx, k, v)
           c  -> d0        : PUT(ctg0, k, v)
   activate c #a0a0a0
 	         d0  -> d1 : PUT(ctg1, k, v)
   deactivate c
           c  <- d0        : RET(rc)
   App <-- c               : EXEC
           c        <-- d1 : RET(rc)
   App <-- c --            : STABLE

Originator permanent failure

.. uml::

   App -[#white]> c
   c   -[#white]> d0
   d0  -[#white]> d1
   App  -> c ++ : PUT(idx, k, v)
           c  -> d0        : PUT(ctg0, k, v)
   activate c #a0a0a0
   activate c #606060
 	         d0  -> d1 : REDO(PUT, ctg1, k, v)



1+1+1
~~~~~

Failure free

.. uml::

   App -[#white]> c
   c   -[#white]> d0
   d0  -[#white]> d1
   d1  -[#white]> s
   App  -> c ++ : PUT(idx, k, v)
           c  -> d0        : PUT(ctg0, k, v)
 	   c         -> d1 : PUT(ctg1, k, v)
           c  <- d0        : RET(rc)
   App <-- c               : EXEC
           c        <-- d1 : RET(rc)
   App <-- c --            : STABLE

Transient

.. uml::

   App -[#white]> c
   c   -[#white]> d0
   d0  -[#white]> d1
   d1  -[#white]> s
   activate             d1 #a0a0a0
   App  -> c ++ : PUT(idx, k, v)
           c  -> d0        : PUT(ctg0, k, v)
           c  <- d0        : RET(rc)
   App <-- c               : EXEC
   deactivate           d1
 	   c         -> d1 : PUT(ctg1, k, v)
           c        <-- d1 : RET(rc)
   App <-- c --            : STABLE

Permanent

.. uml::

   App -[#white]> c
   c   -[#white]> d0
   d0  -[#white]> d1
   d1  -[#white]> s
   activate             d1 #606060
   App  -> c ++ : PUT(idx, k, v)
           c  -> d0        : PUT(ctg0, k, v)
	         d0            -> s : PUT(ctg1, k, v)
	   c                   -> s : PUT(ctg1, k, v)
	   c                  <-- s : RET(rc)
           c  <- d0        : RET(rc)
   App <-- c               : EXEC
   App <-- c --            : STABLE
   deactivate           d1

Transient -> Permanent

.. uml::

   App -[#white]> c
   c   -[#white]> d0
   d0  -[#white]> d1
   d1  -[#white]> s
   activate             d1 #a0a0a0
   App  -> c ++ : PUT(idx, k, v)
           c  -> d0        : PUT(ctg0, k, v)
           c  <- d0        : RET(rc)
   App <-- c               : EXEC
   activate             d1 #606060
	         d0            -> s : PUT(ctg1, k, v)
	   c                   -> s : PUT(ctg1, k, v)
	   c                  <-- s : RET(rc)
   App <-- c --            : STABLE


ios
---

2+1+0
~~~~~

2+1+1
~~~~~

Transaction record
==================

.. highlight:: C
.. code-block:: C

   struct participant {
       /* Immutable. */
       struct m0_fid p_service;
       struct m0_fid p_device;
       bool          p_authoritative;
       bool          p_originator;
       {int}         p_units_need;
       /* Mutable. */
       {int}         p_units_has;
       int           p_state;
   };

   struct txr {
       int t_N;
       int t_K;
       int t_nr_participants;
       struct t_participant[];
   };
