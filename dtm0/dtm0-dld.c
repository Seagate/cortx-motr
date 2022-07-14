/* -*- C -*- */
/*
 * Copyright (c) 2012-2020 Seagate Technology LLC and/or its Affiliates
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 * For any questions about this software or licensing,
 * please email opensource@seagate.com or cortx-questions@seagate.com.
 *
 */


/**
   @page DLD Motr DLD Template

   - @ref DLD-ovw (todo)
   - @ref DLD-def (done)
   - @ref DLD-req
   - @ref DLD-depends
   - @ref DLD-highlights
   - @subpage DLD-fspec "Functional Specification" <!-- Note @subpage -->
   - @ref DLD-lspec
      - @ref DLD-lspec-comps
      - @ref DLD-lspec-sub
      - @ref DLD-lspec-state
      - @ref DLD-lspec-thread
      - @ref DLD-lspec-numa
   - @ref DLD-conformance
   - @ref DLD-ut
   - @ref DLD-st
   - @ref DLD-O
   - @ref DLD-ref
   - @ref DLD-impl-plan

   <hr>
   @section DLD-ovw Overview
   <i>All specifications must start with an Overview section that
   briefly describes the document and provides any additional
   instructions or hints on how to best read the specification.</i>

   TODO: write overview; add to the index in dld-index.c.

   <hr>
   @section DLD-def Definitions
   <i>Mandatory.
   The DLD shall provide definitions of the terms and concepts
   introduced by the design, as well as the relevant terms used by the
   specification but described elsewhere.  References to the
   M0 Glossary and the component's HLD are permitted and encouraged.
   Agreed upon terminology should be incorporated in the glossary.</i>

   Previously defined terms:
   - <b>Storage device</b> A configuration object that corresponds to a physical
   device where Motr keeps data. In this document, this term is used as synonym
   for "persistent participant": Motr stores its persistent data on persistent
   participants.
   - <b>Unit</b> Unit of data. In this document, this term is used
   primary to describe DIX records (DIX/CAS) and data units (IO/IOS).
   New terms:
   - <b>Participant</b> A member of a distributed transaction. Participants
   comprise originators and storage devices.
   - <b>Originator</b> The initiator of a distributed transaction. Originators
   has no persistent storage.
   - <b>PERSISTENT message, Pmsg, Pmgs (plural)</b> A message that indicates
   that a certain information (transaction, log record) became persistent
   on a certain storage device.
   <b>Log record has/is All-P</b>: P messages were received about all non-FAILED
   storage devices that are participants of this log record's dtx.
   <b>Local participant</b> -- participant which is handled by the current DTM0
   domain.
   <b>Remote participant</b> -- participant which is not local participant.
   - <b>Availability</b> This term could be used in the following cases:

   <i>Read availability of participant</i> if it can successfully serve READ
   requests.  Participant is available for reads in ONLINE state only.  Write
   availability of participant: it can successfully serve WRITE requests.
   Participant is available for writes in ONLINE and RECOVERING states.

   <i>Unit is READ-available</i> if at least read-quorum of replicas are on
   READ-available participants, i.e. on ONLINE storage devices.

   <i>Unit is WRITE-available</i> if at least write-quorum of replicas are on
   WRITE-available participants, i.e. on ONLINE or RECOVERING storage devices.

   <i>Read availability for pool</i>: every possible object in the pool is
   READ-available, i.e. at least (pool_width - (number-of-replicas -
   read-quorum)) storage devices are READ-available, i.e. in ONLINE state.

   <i>Write availability for pool<i>: every possible object in the pool is
   WRITE-available, i.e. at least
   (pool_width - (number-of-replicas - write-quorum))
   storage devices are WRITE-available, i.e. in ONLINE or RECOVERING state.
   Explanation: (number-of-replicas - x-quorum) corresponds to the maximal
   number of devices in the "wrong" state among the pool.

   XXX: [Difficult task] Consider failure domains for READ and WRITE pool
   availability.

   <hr>
   @section DLD-req Requirements
   <i>Mandatory.
   The DLD shall state the requirements that it attempts to meet.</i>

   The key words "MUST", "MUST NOT", "REQUIRED", "SHALL", "SHALL NOT",
   "SHOULD", "SHOULD NOT", "RECOMMENDED",  "MAY", and "OPTIONAL" in this
   document are to be interpreted as described in RFC 2119.

   Considerations/assumptions (TODO: move it into the respective secions):
   - Client has limited capacity (timeout for operation).
     Conclusion: Sliding window is always moving on.
   - Duration of TRANSIENT failure - 1 month.
   - We want to be able to clean up the log in a way that this operation
     is not limited by the time when someone entered TRANSIENT.
   - N% of performance degradation is allowed during dtm0 recovery.

   Requirements:

   - dtm0 MUST maximize A, D and performance of the system.
	   - R.dtm0.maximize.availability
	   - R.dtm0.maximize.durability
	   - R.dtm0.maximize.performance

   - dtm0 MUST restore missing replicas in minimal time.
   See REDO-without-RECOVERING for the details.

   - dtm0 MUST handle at least the folloing kinds of failures:
     + replicas MAY become missing due to process crash/restart or because of
     unreliable network.

   - dtm0 MUST restore missing replicas even without process restarts.

   - dtm0 MUST NOT restore missing replicas in case of permanent failures.

   - dtm0 MUST NOT introduce bottlenecks in the system.

   - [R.dtm0.limited-ram] dtm0 memory usage MUST be limited;
	Comment: either we do recovery from the client (mem is not limited?) or
	         we do not do that (cancel) or a mix of that.

   - dtm0 MAY perform recovery from the originator side XXX.

   - dtm0 MUST NOT introduce unnecessary delays.

   - dtm0 SHOULD NOT support transaction dependencies.

   - dtm0 consistency model MUST be configurable.

   - dtm0 performance/D tradeoff MUST be configurable.

   - configuration options:
        1. Read quorum, write quorum.
        2. Number of replicas.
        3. XXX: Read A, WRITE A.

   - dtm0 MUST handle out-of-disk-space and out-of-memory conditions.

   - dtm0 MUST minimize the use of storage for the transactions that are
   replicated on all non-failed participants.
      Comment: no need to prune dtm0 log/FOL all the time.

   They should be expressed in a list, thusly:
   - @b R.DLD.Structured The DLD shall be decomposed into a standard
   set of section.  Sub-sections may be used to further decompose the
   material of a section into logically disjoint units.
   - @b R.DLD.What The DLD shall describe the externally visible
   data structures and interfaces of the component through a
   functional specification section.
   - @b R.DLD.How The DLD shall explain its inner algorithms through
   a logical specification section.
   - @b R.DLD.Maintainable The DLD shall be easily maintainable during
   the lifetime of the code.

   <hr>
   @section DLD-depends Dependencies
   <i>Mandatory. Identify other components on which this specification
   depends.</i>


   - dtm0 relies on Motr HA (Hare) to provide state of Motr configuration
   objects in the cluster.

   - DIX relies dtm0 to restore missing replicas.



   The DLD specification style guide depends on the HLD and AR
   specifications as they identify requirements, use cases, @a \&c.

   <hr>
   @section DLD-highlights-clocks Design Highlights: Clocks

   Originator keeps uint64_t clock for itself and for every storage device.
   The clocks are initialized with zero when the originator starts.
   When a new transaction is created then the originator clock and the
   corresponding clocks of the storage devices are getting incremented,
   and then they all are added to the transaction descriptor:

   @verbatim
   XXX: o.sdev1,sdev2,..sdev6 are used solve the problem log holes.

   [o.originator] [o.sdev1] [o.sdev2] [o.sdev3] [o.sdev4] [o.sdev5] [o.sdev6]
      |               |         |         |         |         |         |
      0               0         0         0         0         0         0
      |               |         |         |         |         |         |
    +----------------------------------------+      |         |         |
  T1| 1               1         1         1  |      0         0         0
    +----------------------------------------+      |         |         |
      |               |         |         |         |         |         |
      1               1         1         1         0         0         0
      |               |         |         |         |         |         |
    +----------------------------------------------------+    |         |
  T2| 2                         2         2         1    |    0         0
    +----------------------------------------------------+    |         |
      |               |         |         |         |         |         |
      2               1         2         2         1         0         0
   @endverbatim

   The picture represents volatile counters on the originator.
   We are sending a window. The window is range [o.BEGIN, o.END) where:
   - o.END is max(o.originator)+1 for any transactions ever created
   on the originator (sinec the process started) or 1 there was not any.
   - o.BEGIN is min(o.originator) for all non-finalised transactions or
   o.END if there are no non-finalised transactions.

   DTM0 log on a persistent participant maintains the following structure
   for every originator:

   @verbatim
   struct originator {
     map<sdev-id, u64/o.originator> max_all_p;
   };
   @endverbatim

   In this structure we store the information passed with P message: the
   most recent (max) o.originator value among the transactions received
   from that originator that have become All-P.

   <hr>
   @section DLD-highlights-holes Design Highlights: Holes

   For every sdev we have per-originator lists ordered by originator's clocks.
   If we have no holes then the updates in the history will be advancing with
   +1.
   We may have holes, so we have to be able deal with them.
   Let's say we have the following kinds of holes:
   - permanent hole -- transaction with such sdev clock value will never come;
   - temporary hole -- transaction has no REDO and transaction with such sdev
   clock will eventually come either from the originator or from another sdev;
   Recovery machine is not interested in the missing record (holes) because:
   - permanent hole is, essentionally, a replicated empty space -- it just does
   not exist anywhere in the system.
   - termporary hole will be eventually replicated by the participants that have
   it (by the definition of the temporary hole).

   <hr>
   @section DLD-highlights-clocks Design Highlights: REDO-without-RECOVERING

   Idea: we have window (o.BEGIN, o.END) on the client. We have "recent"
   transactions from the client such as:

   @verbatim
   w1 [BEGIN=1 END=11]
     w2 [BEGIN=2 END=12]
     ...
                        w13 [BEGIN=13 END=14]
                        ...
                                               w14 [BEGIN=14 END=34]

   ------------------------------------------------------------------>
                                                          o.originator
   @endverbatim

   @verbatim

   [All-P / almost-All-P] [REDO-without-RECOVERING] [N-txns] [current-window]
   ------------------------------------------------------------------>

   Almost-All-P -- transactions that does not have P messages from TRANSIENT
   or FAILED participants but all other participants have sent Pmsgs.

   N-txnds -- a group of transactions for which we are not going to send
   out REDO-without-RECOVERING because it is possible that the requests
   are still somewhere in the incoming queue on the server side, so that
   they could be executed (or any similiar situation).

   current-window -- [o.BEGIN, o.END).

   @endverbatim

   We maintain two kinds of vectors for every originator: max-all-p and
   min-nall-p.

   @verbatim
   for each t in originator.allp:
     for each sdev in t.sdevs:
       max-all-p[originator][sdev] max= t.sdev.clock_value

   for each t in originator.records filtered by "t is not all-p":
     for each sdev in t.sdevs:
       min-nall-p[originator][sdev] min= t.sdev.clock_value
   @endverbatim

   (max-all-p, min-nall-p) pair is added to every Pmsg:

   @verbatim
   struct pmsg {
     fid source;
     fid destination;

     u64(o.originator) timestamp;
     fid originator;

     u64(o.originator) max-all-p;
     u64(o.originator) min-nall-p;
   };
   @endverbatim

   Let's start with the case where we have no TRANSIENT failures of storage
   devices in the pool. In this case, the diagram would look like the following
   picture:

   @verbatim
        (IV)                  (III)             (II)       (I)
   [    All-P   ]   [REDO-without-RECOVERING] [N-txns] [current-window]
   ------------------------------------------------------------------>
                    [ <- may have temporary and permanent holes --->  ] (2)
   [ may have
     permanent but
     not temporary
     holes      ] (1)
   @endverbatim

   We need volatile structure to keep track of the range (2).
   We make a tree (for example, rb) such as:
   @verbatim
     key = o.originator;
     value = {
       ptr to log record in BE seg,
       fid participant_array[] (participant_index -> fids),
       bool pmsg_array[] (participant_index -> has Pmsg or not),
       bool is_locally_persistent (locally persistent or not),
       be_op *executed;
       be_op *persistent;
     };
   @endverbatim

   The tree is owned by the log.

   B -> A: min-nall-p;
   txA, txB;
   txA \in A; txB \in B;
   txA == min-nall-p(on A, contains B); // == next(max-all-p(A, B))
   txB == min-nall-p(on B, contains A);

   txB.clock < txA.clock; // send A -> B: REDO-without-RECOVERING

   \E A.tx: B.min-nall-p(A) < A.min-nall-p(B)

   <hr>
   @section DLD-highlights-clocks Design Highlights: With T

   struct log { ... preserved_max_all_p[originator -> o.originator]; };

   <hr>
   @section DLD-highlights-clocks Design Highlights: HA

   HA must be able to detect slow servers and deal with them one way or another.
   DTM will send information to HA that will help to detect slow servers.
   It allows to limit the size of REDO-with-RECOVERING range
   (to satisfy R.dtm0.limited-ram).

   <hr>
   @section DLD-highlights-clocks Design Highlights: Single clock

   Client has its own logical clock (o.originator).
   Transactions in all server lists (per originator, per sdev) are ordered.
   Transactions in the originator's list are ordered by o.originator.
   For each originator the participant keeps per storage device arrays.
   First array points to Max-All-P, second array points to the lowest Non-All-P.
   With every Pmsg (src sdev, dst sdev, originator fid, dtxid), the min
   Non-All-P for ...

   Let's assume we have one originator. It will help us to define the algorithm
   for one single originator, and then we can just extend it to the multiple
   originators case (because originators are independent).
   Let's start with the alrogithm that figures out holes in the
   REDO-without-recovery case. For each local storage device we keep an array
   with elements for each storage device, and the elements of the array would
   define the min-nall-p for a transaction on this originator which includes
   both storage devices (local and remote).
   In each Pmsg we send this min-nall-p value for src (local) and dst (remote).
   On the receiving end we compare this min-nall-p with the value we have for
   the same pair of storage devices. If remote value min-nall-p > local then
   it means we should send REDO to that participant.
   Each Pmsg also updates max-all-p and min-nall-p.
   all-p can be moved forward if remote min-nall-p > local all-p?

   TODO: describe persistent iterators for TRANSIENT failures (Almost-All-P).

   <hr>
   @section DLD-highlights-clocks Design Highlights: ADDB counters
   TODO.

   <hr>
   @section DLD-highlights-holes Design Highlights: RECOVERING is not needed

   We assume that REDO-with-RECOVERING may happen any time.
   Moreover, any operation can be canceled at any moment, including the time
   when write-quorum has not been reached.
   If record was written with write-quorum then consequent read-quorum read
   will return the recent data. In any other case, consistency is not
   guaranteed.
   Therfore, there is no need to have a separate RECOVERING state.

   <hr>
   @section DLD-highlights-holes Design Highlights: Simple recovery

   The goal: move Max-All-P as far as possible.
   For that, we have to solve two tasks:
   - fill in the missing remote temporary holes;
   - figure out if there are local temporary holes right after the current
   Max-All-P.

   At first, let's take a look once again the intervals:

   @verbatim
        (IV)                  (III)             (II)       (I)
   [ Seq-All-P  ]   [REDO-without-RECOVERING] [N-txns] [current-window]
   x------------x-----------------------------x------x---------------> (originator's clock)
   ^            ^                             ^
   |            | Max-All-P                   |
   |                                          | Last-non-r-w-r-able-dtx
   | Last non-pruned dtx

   Intervals:
     IV:  [last non-pruned dtx, Max-All-P]
     III: (Max-All-P, Non-Rwr]
   @endverbatim

   We use Min-Non-All-P to determine if remote side requires
   REDO-without-RECOVERING (r-w-r).
   For every remote storage device we keep volatile Min-Non-All-P:
   - initialized with zero;
   - updated when Pmsg is received;

   Note, transactions on the originator start with 1.
   Note, Min-Non-All-P is sent as a part of Pmsg.

   Whenever the R-w-R interval becomes non-empty we start sending REDOs to
   the corresponding participants. By definition, the interval may contain
   non-All-P transactions, temporary holes and All-P records.
   There are 3 possible cases for the log record after Max-All-P:
   - next record is Non-All-P (1);
   - next record is a temporary hole (2);
   - next record is All-P (3);
   The first case is a reason to send REDO to the corresponding participants.
   We send REDO message for next(Max-All-P) log record.
   The participants will send us Pmsgs, which, will eventually lead us to
   the third case.
   The second and third cases are handled with help of remote min-nall-p values:
   - we check if the next log record clock value is less than the minimum of the
   set of remote min-nall-p and check if All-P was reached for the record. We
   set Max-All-P to point to next record if both conditions (min, All-P) are
   satisfied:
   @verbatim
     Let's say min-min-nall-p = min(p.min-nall-p) for all participants p in the
     cluster).
     Then, we have the following condition and action:
     if next(Max-All-P) < min-min-nall-p and next(Max-All-P) is All-P then
       then we move Max-All-P = next(Max-All-P)
     endif.
   @endverbatim

   <hr>
   @section DLD-highlights-holes Design Highlights: Improvements for simple
   recovery

   Now, let's take a look at the intervals when one participant is in transient.
   In this case, All-P interval will not be able to advance because we are
   expecting that all participants will be able to execute REDOs and send
   their min-nall-p. This will lead as to the situation where DTM0 log
   is cannot be pruned. There are way to avoid this.
   TODO

   The basic algorithm requies O(N^2) memory for per-sdev data (lists, counters,
   etc.). To alleviate the problem, we may keep volatile data on the client side.
   In this case, the client will tell storage devices about min-nall-p.
   TODO

   The basic algorithm requires O(N^2) P messages (per sdev). To alleviate the
   problem, the client may redistribute Pmsgs: server may send Pmsg to the
   client as a part of EXECUTED message (for example, inside CAS reply). The
   client will send other server's Pmsgs as a part of EXECUTE message
   (for example, CAS request).
   TODO

   The basic algorithm requiers O(N^2) REDO messages (per sdev). To alleviate
   the problem, we send REDOs from the first non-failed participant in the
   participant list of the transaction.
   TODO

   <hr>
   @section DLD-highlights Design Highlights
   <i>Mandatory. This section briefly summarizes the key design
   decisions that are important for understanding the functional and
   logical specifications, and enumerates topics that need special
   attention.</i>

   DTM0 "information" needed for ordering (two timestamps):
       - txid;
       - txid previously sent to this participant.

   Log record states:
   - was never added to dtm0 log;
   - is in dtm0 log;
     + does not have REDO message;
     + has REDO message;
       * does not have All-P;
       * has All-P;
   - was added to dtm0 log and was removed from dtm0 log;

   [INPROGRESS, EXECUTED, STABLE, DONE]

   INPROGRESS -> EXECUTED: enough CAS replies
   INPROGRESS -> DONE: canceled before it was executed
   EXECUTED -> STABLE: enough Pmsgs
   EXECUTED -> DONE: canceled after it was executed
   STABLE -> DONE: successfully completed.

   [ INPROGRESS ]  [ EXECUTED ] [ STABLE ] [ DONE ]

   [ head  ... tail] [ to_be_assigned/current ]

   Sliding window: all transactions on the client:

   - min timestamp (the "head" of the list or "current" if the list is empty);
   - "current" timestamp;

   Invatiant: for the given [min, current) interval, its boundaries are never
   decreasing (TODO: look up term for "monotonically non-decreasing").

   Min xid sent to servers.
   Client concurrency * timeout value == window size.
   Server-side window size is limited by performance of server.
   We maintain ordered list on each client; they are ordered by client-side
   timestamp.
   3 cat of tx for a client:
     has not left the client
     did not arrive to servers
     not related.
   Sliding window is updated whenver client is not idle.
   When client is idle then we rely on local detection: if there is no io
   from that client while there is io from others then the client is idle.
   Sliding window allows us to prune records at the right time.

   - dtm0 uses persistent dtm0 log on participans other than originator,
   and it uses volatile dtm0 long on originator.

   - persistent dtm0 log uses BE as storage for dtm0 log records.

   - dtm0 uses m0 network for communication.

   - dtm0 saves information in dtm0 log that may be needed for dtm0 recovery up
   until the transaction is replicated on all non-failed participants.

   - to restore a missing replica, dtm0 participant sends dtm0 log record to the
   participant where the replica is missing.

   - dtm0 propagates back pressure (memory, IO, etc) across the system, thus
   helping us to avoid overload due to "too many operations are in-progress".

   [Cat of fids]
   Originator is service.
   Storage device is persistent participant.
   Participant is a service or a storage device.

   <hr>
   @section DLD-lspec Logical Specification
   <i>Mandatory.  This section describes the internal design of the component,
   explaining how the functional specification is met.  Sub-components and
   diagrams of their interaction should go into this section.  The section has
   mandatory subsections created using the Doxygen @@subsection command.  The
   designer should feel free to use additional sub-sectioning if needed, though
   if there is significant additional sub-sectioning, provide a table of
   contents here.</i>

   - @ref DLD-lspec-comps
   - @ref DLD-lspec-sub
      - @ref DLD-lspec-ds-log
      - @ref DLD-lspec-sub1
      - @ref DLDDFSInternal  <!-- Note link -->
   - @ref DLD-lspec-state
   - @ref DLD-lspec-thread
   - @ref DLD-lspec-numa


   @subsection DLD-lspec-comps Component Overview
   <i>Mandatory.
   This section describes the internal logical decomposition.
   A diagram of the interaction between internal components and
   between external consumers and the internal components is useful.</i>

   - TODO: add the component diagram of "the R dtm0".

   - dtm0 consists of dtm0 log, pruner, recovery machine, persistent machine,
   net, HA, dtx0 modules.

   @subsection DLD-lspec-sub Subcomponent design
   <i>Such sections briefly describes the purpose and design of each
   sub-component. Feel free to add multiple such sections, and any additional
   sub-sectioning within.</i>

   - DTM0 log:
   DTM0 log uses BE to store data.
   It uses BTree, key are txids, values are records.
   BTrees are "threaded" trees: each record is linked into several lists in
   adddtion to being in the tree.
   DTM0 log provides an API that allows the user to add or remove log records.
   Also, there are log iterators: API that allows the user to traverse over
   specific kinds of records (for example, "all-p" records).
   @verbatim

   struct m0_dtx0_id {
   	uint64_t      dti_timestamp;
   	struct m0_fid dti_originator_sdev_fid;
   } M0_XCA_RECORD M0_XCA_DOMAIN(rpc|be);

   struct m0_dtx0_participants {
   	uint64_t       dtpa_participants_nr;
   	struct m0_fid *dtpa_participants;
   } M0_XCA_SEQUENCE M0_XCA_DOMAIN(rpc|be);

   struct m0_dtx0_descriptor {
   	struct m0_dtx0_id           dtd_id;
   	struct m0_dtx0_participants dtd_participants;
   } M0_XCA_RECORD M0_XCA_DOMAIN(rpc|be);

   struct m0_dtx0_payload {
   	uint32_t         dtp_type M0_XCA_FENUM(m0_dtx0_payload_type);
   	struct m0_bufs   dtp_data;
   } M0_XCA_RECORD M0_XCA_DOMAIN(rpc|be);

   struct m0_dtm0_redo {
   	struct m0_dtx0_descriptor dtr_descriptor;
   	struct m0_dtx0_payload    dtr_payload;
   };

   struct pmsg {
	   dtx0_id;
	   uint64_t nr_participants;
	   fid source; // sdev_fid
	   fid destination; // sdev|service fid
   };

   struct log_record {
	   dtx0_id   id;
	   dtm0_redo redo;
	   be_list_link allp;
	   be_list_link redo;
	   be_list_link participants[]; // [0] == originator, rest - sdevs
	   fid          fids[];
	   uint64_t     nr_participants;
   };
   // states:
   //   REDO message is not here:
   //      first incoming Pmsg -- create a new log record with empty REDO,
   //                             msg.source is added to log_record.fids.
   //      next incoming Pmsgs -- add msg.source to log_record.fids.
   //   when REDO message finaly arrives:
   //      take all non-zero fids from log_record.fids and ignore these
   //      participants when we link the log_record to the
   //      log_record.participants lists.
   // for a local participant log record is added to the list when the operation
   // is executed on the local participant.
   // for a remote participant: TODO

   struct dtm0_log_originator {
	   fid;
	   be_list records; // ordered by o.originator
	   u64 o.BEGIN; // max received from originator
	   u64 o.END;   // max received from originator

	   //XXX
	   sdev -> u64: max(allp) for sdev
   };

   struct dtm0_log_sdev {
	   fid;
	   be_list current; // move items current -> redo when sdev leaves T state
	   be_list redo;

	   //XXX
	   u64 max_allp;
   };

   struct dtm0_log {
	   btree tree(key=dtxid, value=log_record);
	   be_list<dtm0_log_originator> originators;
	   be_list<dtm0_log_sdev> sdevs;
   };

   @endverbatim

   @verbatim
   Per originator lists are ordered by originator's clocks.
   Per sdev lists are not ordered.
   All-p list is not ordered.
   Record is moved to All-p only ...



   @endverbatim

   - DTM0 net:
   DTM0 net uses Motr RPC.
   It provides a queue-like API: a message could be posted into the network,
   and the user may subscribe to particular kind of incoming messages.
   It has no persistence state.
   The API provides asynchronous completion: the user may wait until message was
   acknoweledged by the remote side (RPC reply was sent).
   It cancels all outgoing/incoming messages when the HA tells us that
   participant goes to FAILED/TRANSIENT state.
   Optimizations:
   - Do not use network if the destination is in the same Motr process.

   - DTX0:
   It is the facade [todo: c` instead of just c] of dtm0 for dtm0 users.
   Duplicates (CAS requests, REDO messages, etc.) are checked at the DTM0 log
   level.

   - Persistent machine:
   It solves two tasks. It sends out a Pmsg when a local transction becomes
   persistent (1).
   It updates the log when a Pmsg is received from a remote participant (2).
   Persistent machine contains a set of local participants. For each participant,
   there is a set of iterators that correspond to the remote participants:

   @verbatim
   [ local-sdev-1, local-sdev-2]   [remote-sdev-1, remote-sdev-2]

   local-sdev-1 has the following iterators:
        remote-sdev-1
        remote-sdev-2
   local-sdev-2 has the same set of iterators.
   @endverbatim

   However, we want to minimize the latency of user operation. In Pmach, we can
   prioritize sending P messages to clients over sending of P messages to other
   participants. Clients are interested in Pmsgs only for in-progress
   transactions. Because of that, we can create an in-memory queue for each
   client which will be used as the source of Pmsgs to the client. If we keep
   this queue sorted and remove Pmsgs for transactions with T < T.client.min
   then this queue will not grow much more than the total amount of all
   in-progress transactions for the client, which will satisfy
   R.dtm0.limited-ram.
   The queue does not have to be persistent, so we keep it in RAM, thus
   satisfying R.dtm0.maximize-performance.

   Alternatives:
   1. In-memory queue (for outgoing Pmsgs). Cons: the queue is not bounded; it
   can grow quickly if the remote is slow. It is not acceptable (see
   R.dtm0.limited-ram).
   2. One iterator per local participant (send Pmsgs, advance one single iter,
   send again and so on). Cons: unecessary delays (for example when one remote
   is in TRANSIENT). The delays have negative impact on reliability, durability,
   and increases storage consumption.

   Pmach algorithm for outgoing Pmsgs:
   1. Take a Pmsg from any iterator mentioned above (for example,
   local-sdev-1.remote-sdev-2).
   2. Send the Pmsg to the remote (for example, remote-sdev-2).

   Pmach algorithm for incoming Pmsgs:
   1. Take an incoming Pmsg.
   2. Apply it to the log.

   Pmach optimizations:
   For outgoing: coalescing is done at the net level; wait until all local
   transactions are persistent before sending Pmsg about local participants.
   XXX: persistent on all local participants which are ONLINE, RECOVERING,
   during rebalance/direct rebalance.
   For incoming:
   Wait until Pmsgs about a dtx received from all non-failed (XXX: ONLINE,
   etc. like before) participants before persisting those Pmsgs. This
   will improve BE seg locality of reference.

   - Recovery machine:
   It solves two tasks. It sends out REDO messages (1).
   It applies incoming REDO messages sent from other recovery machines (2).
   REDO messages are read out from the log directly. They are applied through
   a callback (for example, posted as CAS FOMs).

   @verbatim
   [ remote participant restarted]
      -> [ merge REDO-list with ongoing-list of the participant ]

   [ remote participant: * -> RECOVERING ]
      -> [ local -> remote: REDO ]
   REDO: XXX: Contains Pmsgs for all local participants that are persistent.
   @endverbatim

   For each local participant, we iterate over the REDO-list, send out REDOs,
   thus recovering the corresponding remote participant. The recovery machine
   sends the (REDO, local participant Pmsg) tuple to the remote participant.
   Once all the mesages were sent, it sends End-of-log (EOL) message for the
   local participant.
   On the recovering side: recovery machine awaits EOL from a particular set
   of participants (see below).
   Local storage device recovery is considered as complete
   ("recovery-stop-condition") when the following sets are READ-available:
     - Set1: local storage device + set of remote storage devices that sent EOL.
     - Set2: local storage device + set of remote storage devices to which EOL
     was sent.

   Note, recovery machine may need to recover a local storage device
   (inter-process recovery). It is done in the same way as with remote storage
   devices, except DTM0 net will not be sending messages over network, instead
   they will be loopback-ed.

   When a REDO message is received, recovery machine calls the corresponding
   callback that will apply the message. For example, the callback submits a CAS
   FOM and then recovery machine awaits until the data gets landed on disk.
   Then, the machine sends reply. The remote recovery machine receives the reply
   and then sends another REDO. It allows to propagate the back pressure from
   the recovering participant to the remote.

   Duplicates are not checked by recovery machine. Instead, DTX0 and DTM0 log
   do that.

   Aside from recovering of TRANSIENT failures, recovery machine reacts to
   FAILED state: in case of originator it causes "client eviction"; in case of
   storage device, the machine does nothing -- Motr
   repair/rebalance/direct-rebalance will take care of such device.

   Moreover, recovery machine is also taking care of restoring missing replicas
   for XXX:DixRecords outside of the usual recovery process caused by the state
   transition of a storage device. This may happen due to losses in the network
   or due to user operation cancelation.

   Optimizations:
      - send one REDO message for multiple participants that exist in the same
      process.

   - Pruner:
   It removes records when their respective transactions become persistent on
   all non-failed participants.
   It removes records of FAILED participants (eviction). After storage device
   goes to FAILED state, pruner assumes that the records which have this storage
   device as a participant no longer have this participant without P message.
   If there are no other missing P messages then the pruner assumes that this
   log record has All-P.
   Pruner is only interested in only log records that are in the log and that
   have All-P.

   - HA (dtm0 ha):
   It provides interface to Motr HA tailored for dtm0 needs.
   What is needed from HA
     - Pmach wants to know states and transitions for remote participants,
     history does not matter.
     - Remach: states and transitions of all participants, history matters.
     Also, it allows motr/setup to send RECOVERED to HA (through domain).
     - Net: states and transitions of all participants, history does not matter.
     - Pruner: TRANSIENT and FAILED states, history matters.

   [subscription to transitions]
   DTM0 HA allows its user to subscribe to storage device states or service
   states updates.

   [persistent ha history]
   DTM0 HA uses BE to keep the persistent history of state trasitions of
   participants. The history is garbadge-collected by DTM0 HA itself:
   for example, FAILED participants are removed when eviction is complete.
   The other components may use the history.

   - domain:
   DTM0 domain is a container for DTM0 log, pruner, recovery machine,
   persistent machine, network. It serves as an entry point for any other
   component that wants to interact with DTM0. For example, distributed
   transactions are created within the scope of DTM0 domain.
   TODO: remove it from domain.h.
   To initialize DTM0, the user has to initialize DTM0 domain which will
   initialize all internal DTM0 subsystems.
   There may be more than one DTM0 domain per Motr process.


   @subsubsection DLD-lspec-ds-log Subcomponent Data Structures: DTM0 log
   <i>This section briefly describes the internal data structures that are
   significant to the design of the sub-component. These should not be a part
   of the Functional Specification.</i>

   XXX
   DTM0 log record is linked to one BTree and many BE lists.
   DTM0 log BTree: key is txid, value is a pointer to log record.
   BE list links in record: one for originator, many for participants.
   DTM0 log contains the BTree and a data structure that holds the
   heads of BE lists and related information.
   DTM0 log also contains a hashtable that keeps the local in-flight DTM0
   transactions and the corresponding BE txs. Usecases:
        - incoming REDO: find by dtxid;
        - find dtx0 by be_tx?;
   Hashtable: (key=txid, value=(ptr to dtx, ptr to be_tx))

   [ BTree (p) | Participants+Originators ds? (p) | in-flight tx (v) ]

   @section m0_dtm0_remach interface

   Note: log iter here does not care about holes in the log.

   - m0_dtm0_log_iter_init() - initializes log record iterator for a
     sdev participant. It iterates over all records that were in the log during
     last local process restart or during last remote process restart for the
     process that handles that sdev.
   - m0_dtm0_log_iter_next() - gives next log record for the sdev participant.
   - m0_dtm0_log_iter_fini() - finalises the iterator. It MUST be done for every
     call of m0_dtm0_log_iter_init().
   - m0_dtm0_log_participant_restarted() - notifies the log that the participant
     has restarted. All iterators for the participant MUST be finalized at the
     time of the call. Any record that doesn't have P from the participant at
     the time of the call will be returned during the next iteration for the
     participant.

   @section pmach interface

   - m0_dtm0_log_p_get_local() - returns the next P message that becomes local.
     Returns M0_FID0 during m0_dtm0_log_stop() call. After M0_FID0 is returned
     new calls to the log MUST NOT be made.
   - m0_dtm0_log_p_put() - records that P message was received for the sdev
     participant.

   @section pruner interface

   - m0_dtm0_log_p_get_none_left() - returns dtx0 id for the dtx which has all
     participants (except originator) reported P for the dtx0. Also returns all
     dtx0 which were cancelled.
   - m0_dtm0_log_prune() - remove the REDO message about dtx0 from the log

   dtx0 interface, client & server

   - bool m0_dtm0_log_redo_add_intent() - function to check if the transaction
     has to be applied or not, and reserves a slot in the log for that
     record (in case if it has to be applied).

   - m0_dtm0_log_redo_add() - adds a REDO message and, optionally, P message, to
     the log.

   @section dtx0 interface, client only

   - m0_dtm0_log_redo_p_wait() - returns the number of P messages for the dtx
     and waits until either the number increases or m0_dtm0_log_redo_cancel() is
     called.
   - m0_dtm0_log_redo_cancel() - notification that the client doesn't need the
     dtx anymore. Before the function returns the op
   - m0_dtm0_log_redo_end() - notifies dtx0 that the operation dtx0 is a part of
     is complete. This function MUST be called for every m0_dtm0_log_redo_add().


   Describe @a briefly the internal data structures that are significant to
   the design.  These should not be described in the Functional Specification
   as they are not part of the external interface.  It is <b>not necessary</b>
   to describe all the internal data structures here.  They should, however, be
   documented in Detailed Functional Specifications, though separate from the
   external interfaces.  See @ref DLDDFSInternal for example.

   - dld_sample_internal

   @subsubsection DLD-lspec-sub1 Subcomponent Subroutines
   <i>This section briefly describes the interfaces of the sub-component that
   are of significance to the design.</i>

   Describe @a briefly the internal subroutines that are significant to the
   design.  These should not be described in the Functional Specification as
   they are not part of the external interface.  It is <b>not necessary</b> to
   describe all the internal subroutines here.  They should, however, be
   documented in Detailed Functional Specifications, though separate from the
   external interfaces.  See @ref DLDDFSInternal for example.

   - dld_sample_internal_invariant()

   @subsection DLD-lspec-state State Specification
   <i>Mandatory.
   This section describes any formal state models used by the component,
   whether externally exposed or purely internal.</i>

   Diagrams are almost essential here. The @@dot tool is the easiest way to
   create state diagrams, and is very readable in text form too.  Here, for
   example, is a @@dot version of a figure from the "rpc/session.h" file:
   @dot
   digraph example {
       size = "5,6"
       label = "RPC Session States"
       node [shape=record, fontname=Helvetica, fontsize=10]
       S0 [label="", shape="plaintext", layer=""]
       S1 [label="Uninitialized"]
       S2 [label="Initialized"]
       S3 [label="Connecting"]
       S4 [label="Active"]
       S5 [label="Terminating"]
       S6 [label="Terminated"]
       S7 [label="Uninitialized"]
       S8 [label="Failed"]
       S0 -> S1 [label="allocate"]
       S1 -> S2 [label="m0_rpc_conn_init()"]
       S2 -> S3 [label="m0_rpc_conn_established()"]
       S3 -> S4 [label="m0_rpc_conn_establish_reply_received()"]
       S4 -> S5 [label="m0_rpc_conn_terminate()"]
       S5 -> S6 [label="m0_rpc_conn_terminate_reply_received()"]
       S6 -> S7 [label="m0_rpc_conn_fini()"]
       S2 -> S8 [label="failed"]
       S3 -> S8 [label="timeout or failed"]
       S5 -> S8 [label="timeout or failed"]
       S8 -> S7 [label="m0_rpc_conn_fini()"]
   }
   @enddot
   The @c dot program is part of the Scientific Linux DevVM.

   @subsection DLD-lspec-thread Threading and Concurrency Model
   <i>Mandatory.
   This section describes the threading and concurrency model.
   It describes the various asynchronous threads of operation, identifies
   the critical sections and synchronization primitives used
   (such as semaphores, locks, mutexes and condition variables).</i>

   This section must explain all aspects of synchronization, including locking
   order protocols, existential protection of objects by their state, etc.
   A diagram illustrating lock scope would be very useful here.
   For example, here is a @@dot illustration of the scope and locking order
   of the mutexes in the Networking Layer:
   @dot
   digraph {
      node [shape=plaintext];
      subgraph cluster_m1 { // represents mutex scope
         // sorted R-L so put mutex name last to align on the left
         rank = same;
	 n1_2 [label="dom_fini()"];  // procedure using mutex
	 n1_1 [label="dom_init()"];
         n1_0 [label="m0_net_mutex"];// mutex name
      }
      subgraph cluster_m2 {
         rank = same;
	 n2_2 [label="tm_fini()"];
         n2_1 [label="tm_init()"];
         n2_4 [label="buf_deregister()"];
	 n2_3 [label="buf_register()"];
         n2_0 [label="nd_mutex"];
      }
      subgraph cluster_m3 {
         rank = same;
	 n3_2 [label="tm_stop()"];
         n3_1 [label="tm_start()"];
	 n3_6 [label="ep_put()"];
	 n3_5 [label="ep_create()"];
	 n3_4 [label="buf_del()"];
	 n3_3 [label="buf_add()"];
         n3_0 [label="ntm_mutex"];
      }
      label="Mutex usage and locking order in the Network Layer";
      n1_0 -> n2_0;  // locking order
      n2_0 -> n3_0;
   }
   @enddot

   @subsection DLD-lspec-numa NUMA optimizations
   <i>Mandatory for components with programmatic interfaces.
   This section describes if optimal behavior can be supported by
   associating the utilizing thread to a single processor.</i>

   Conversely, it can describe if sub-optimal behavior arises due
   to contention for shared component resources by multiple processors.

   The section is marked mandatory because it forces the designer to
   consider these aspects of concurrency.

   <hr>
   @section DLD-conformance Conformance
   <i>Mandatory.
   This section cites each requirement in the @ref DLD-req section,
   and explains briefly how the DLD meets the requirement.</i>

   Note the subtle difference in that @b I tags are used instead of
   the @b R  tags of the requirements section.  The @b I of course,
   stands for "implements":

   - @b I.DLD.Structured The DLD specification provides a structural
   breakdown along the lines of the HLD specification.  This makes it
   easy to understand and analyze the various facets of the design.
   - @b I.DLD.What The DLD style guide requires that a
   DLD contain a Functional Specification section.
   - @b I.DLD.How The DLD style guide requires that a
   DLD contain a Logical Specification section.
   - @b I.DLD.Maintainable The DLD style guide requires that the
   DLD be written in the main header file of the component.
   It can be maintained along with the code, without
   requiring one to resort to other documents and tools.  The only
   exception to this would be for images referenced by the DLD specification,
   as Doxygen does not provide sufficient support for this purpose.

   This section is meant as a cross check for the DLD writer to ensure
   that all requirements have been addressed.  It is recommended that you
   fill it in as part of the DLD review.

   <hr>
   @section DLD-ut Unit Tests
   <i>Mandatory. This section describes the unit tests that will be designed.
   </i>

   Unit tests should be planned for all interfaces exposed by the
   component.  Testing should not just include correctness tests, but
   should also test failure situations.  This includes testing of
   @a expected return error codes when presented with invalid
   input or when encountering unexpected data or state.  Note that
   assertions are not testable - the unit test program terminates!

   Another area of focus is boundary value tests, where variable
   values are equal to but do not exceed their maximum or minimum
   possible values.

   As a further refinement and a plug for Test Driven Development, it
   would be nice if the designer can plan the order of development of
   the interfaces and their corresponding unit tests.  Code inspection
   could overlap development in such a model.

   Testing should relate to specific use cases described in the HLD if
   possible.

   It is acceptable that this section be located in a separate @@subpage like
   along the lines of the Functional Specification.  This can be deferred
   to the UT phase where additional details on the unit tests are available.

   Use the Doxygen @@test tag to identify each test.  Doxygen collects these
   and displays them on a "Test List" page.

   <hr>
   @section DLD-st System Tests
   <i>Mandatory.
   This section describes the system testing done, if applicable.</i>

   Testing should relate to specific use cases described in the HLD if
   possible.

   It is acceptable that this section be located in a separate @@subpage like
   along the lines of the Functional Specification.  This can be deferred
   to the ST phase where additional details on the system tests are available.


   <hr>
   @section DLD-O Analysis
   <i>This section estimates the performance of the component, in terms of
   resource (memory, processor, locks, messages, etc.) consumption,
   ideally described in big-O notation.</i>

   <hr>
   @section DLD-ref References
   <i>Mandatory. Provide references to other documents and components that
   are cited or used in the design.
   In particular a link to the HLD for the DLD should be provided.</i>

   For documentation links, please refer to this file :
   doc/motr-design-doc-list.rst

   Detailed level design HOWTO</a>,
   an older document on which this style guide is partially based.
   - <a href="http://www.stack.nl/~dimitri/doxygen/manual.html">Doxygen
   Manual</a>
   - <a href="http://www.graphviz.org">Graphviz - Graph Visualization
   Software</a> for documentation on the @c dot command.
   - <a href="http://www.mcternan.me.uk/mscgen">Mscgen home page</a>

   <hr>
   @section DLD-impl-plan-components Implementation Plan: Components

   On the client side:
   - dtx0: init/fini tx, cancel tx, STABLE callback;
   - log: add redo, cancel, add p, ...;
   - pmach: recv(net) and apply them to the log;
   - pruner: removes STABLE and canceled transaction after a delay;
   - net: send and recv;
   - ha: subscription to new states;
   - remach: sends REDOs;

   On the server side:
   - dtx0: init/fini tx with be, persistent callback;
   - log: add redo, add p, ...;
   - pmach: set of FOMs that recv(net) and add_p(log), also they
   are awaiting on log and send(net);
   - pruner: awaits on log for new Max-All-P, awaits on ha for new FAILED;
   - net: send, recv;
   - ha: persistent log, delivered(), subscription to new states;
   - remach: set of FOMs that await on log records for which we should send
   REDO, and receives incoming REDOs from net, and applies incoming REDOs.

   unordered:
   - optimizations: persistent iterators, client-based persistence-related
   coordination, first-non-falied participant sends REDO;

   V1:
   - DTM0 log:
     + Static list of participants, each item is a dtx for which we have not
     received Pmsgs;
     + No client lists;
   - Pmach (simple shim between log and net);
   - Pruner (removes all-p records);
   - no recovery machine;
   - DTX0 (simple shim between user and log);

   V2:
   - V1;
   - add recovery after restart;
   - adpter for client (pmsg old -> new);
   - remach that sends REDO from lists and merges lists;

   V3:
   - V2;
   - Replace the old one with the new DTM0;

   Features (starts with V3):
   - Originators lists, unordered, client evition;
     + Add when client appears;
     + Remove when client eviction is done;
   - Dtx0, Pmach, pruner, volatile log for client:
     + Pmach: receives Pmsg, marks them in the log;
     + Log, dtx0: sends STABLE to user;
     + User may cancel a dtx0 at any time;
     + Canceled dtx0 is removed from the log;
   - Logical clock on originators:
     + increment when added to the log;
     + send the window to the server side (juts send);
   - Remove RECOVERING state;
   - DTM0 net based on drlink (could be neede earlier for V1,V2);
   - DTM0 net based on queues and Motr RPC;
   - DTM0 HA;
   - in-memory queue for pmach;
   - independent (per-participant) iterators for pmach;
   - DTM0 log: tracking of real local p:
     + a data structure (htable+list) to keep track of in-progress be tx.
   - Redo-without-RECOVERING:
     + Start after a delay (physical T timeout == N CAS timeouts);
   - R-w-R: add new criteria -- starts after N local txns or T timeout.
   - Client eviction is replaced by R-W-R;
   - Ordinary recovery is replaced by R-W-R;
   - R-W-R, ordering, min-nall-p/max-all-p, pruner by max-all-p, sending of
   min-nall-p;
   - Optimize R-W-R: route messages through the client;
   - Optimize: Delay pruning until V amount of space is consumed or N tnxs;
   - Optimize: Send REDO from the first non-failed;
   - Optimize: Send REDO from the client log (cache);
   - Optimize: Pmach, batching of Pmsgs for the same tx, for the same sdev;
   - Optimize: Partition of DTM0 log by dtx0id;
   - Optimize: Persistent per-participant list for FOL purposes;


   <hr>
   @section DLD-impl-plan Implementation Plan
   <i>Mandatory.  Describe the steps that should be taken to implement this
   design.</i>

   The plan should take into account:
   - The need for early exposure of new interfaces to potential consumers.
   Should parts of the interfaces be landed early with stub support?
   Does consumer code have to be updated with such stubs?
   - Modular and functional decomposition.  Identify pieces that can be
   developed (coded and unit tested) in smaller sub-tasks.  Smaller tasks
   demonstrate progress to management, as well as reduce the inspection
   overhead.  It may be necessary to plan on re-factoring pieces of code
   to support this incremental development approach.
   - Understand how long it will take to implement the design. If it is
   significantly long (more than a few weeks), then task decomposition
   becomes even more essential, because it allows for merging of changes
   from dev into your feature branch, if necessary.
   Task decomposition should be reflected in the GSP task plan for the sprint.
   - Determine how to maximize the overlap of inspection and ongoing
   development.
   The smaller the inspection task the faster it could complete.
   Good planning can reduce programmer idle time during inspection;
   being able to overlap development of the next coding sub-task while the
   current one is being inspected is ideal!
   It is useful to anticipate how you would need organize your GIT code
   source branches to handle this efficiently.
   Remember that you should only present modified code for inspection,
   and not the changes you picked up with periodic merges from another branch.
   - The software development process used by Motr provides sufficient
   flexibility to decompose tasks, consolidate phases, etc.  For example,
   you may prefer to develop code and UT together, and present both for
   inspection at the same time.  This would require consolidation of the
   CINSP-PREUT phase into the CINSP-POSTUT phase.
   Involve your inspector in such decisions.
   Document such changes in this plan, update the task spreadsheet for both
   yourself and your inspector.

   The implementation plan should be deleted from the DLD when the feature
   is landed into dev.


   Unsolved questions:
   - Let's say we have a hole in the log. How to understand that the hole is
   valid (i.e., no redo will be received) or non-valid (a missing record)?
   - DTM0 service FIDs, user-service FIDs, storage device FIDs, process FID --
   what is the relation between them? see/grep [Cat of fids].

   Q: Isolation?
   A: RM/locks/etc. Executed state is required.
   It allows us not to wait on STABLE in certain cases.
     Counter point: but still there are some guarantees that dtm0 must provide.
     A: Nikita will provide an answer to the question about READ availablity.

   Q: Cancel really needed?
   A: no requirement for an explicit cancel().


   NOTES
   Holes in the log
   ----------------

   Kinds of holes:
   - missing or canceled record
   - missing Pmsg

   Client sends:
   - earliest non-stable
   - latest non-stable

   Server (to server) sends:
   - lastest allp
   - earliest non-allp

   Log
   ---

   originator list is ordered by its clock.

   TODO: add client cache of REDO message that are not linked with
   dtx'es (not needed for the client).

   TODO: right now we assume one DTM0 log per pool. Later on, we need a
   a single DTM0 log.

   TODO: consider almost "immutable" list links in log records (for FOL).



   @verbatim
        (IV)                  (III)             (II)       (I)
   [ Seq-All-P  ]   [REDO-without-RECOVERING] [N-txns] [current-window]

   (sdev1.self)
   x------------x-------------O1-m1----------------x------x--------------->
   (sdev1.sdev2)
   x------------x--------m2-m3------------------x------x--------------->


   (sdev2.self)
   x------------x--------m4-O2-------------------x------x--------------->
   (sdev2.sdev1)
   x------------x----------------m5--------------------x------x--------------->
   ^            ^                             ^
   |            | Max-All-P                   |
   |                                          | Last-non-r-w-r-able-dtx
   | Last non-pruned dtx

   Intervals:
     IV:  [last non-pruned dtx, Max-All-P]
     III: (Max-All-P, Non-Rwr]

   m - min-nall-p
   @endverbatim

   @verbatim
                                                              CAS REQ
							      (will be executed)
                                                                |
                                                               \|/

        (IV)                  (III)             (II)       (I)
   [ Seq-All-P  ]   [REDO-without-RECOVERING] [N-txns] [current-window]
   x------------x-----------------------------x--------x--------------->
                ^                             ^
		| Max-All-P                   | Last-non-r-w-r-able-dtx

                                         CAS REQ (will be droped)
                                           |
                                          \|/

        (IV)                  (III)             (II)       (I)
   [ Seq-All-P  ][                    ] [     ] [current-window]
   x------------x----------------------x------x--------------->
   ^            ^                      ^
   |            | Max-All-P            |
   |                                   | Last-non-r-w-r-able-dtx
   | Last non-pruned dtx

   Intervals:
     IV:  [last non-pruned dtx, Max-All-P]
     III: (Max-All-P, Non-Rwr]

   m - min-nall-p
   @endverbatim

   New conclusions:
   - we can (must?) reject CAS requests which txid is less than min-non-all-p
   (local).
   - but redo will be ignored when their txid is less than min-min-non-all-p
   (global).

   redo_lists[by_participants] - what are they?

   Cas request from the III-rd (REDO-without-RECOVERING) interval might be
   present in at least one the redo_lists[by_participant], so that the
   recovery process can send the redo msgs to the online participants from
   which there was no pmsg yet. Therefore, every log record must have
   redo_links[i], where i < K. (This, in turn, requires the btree log record
   to be the pointer to the actual log record with the redo_links, so that
   the lists pointers are not broken on btree rebalance.)

   Why do we need redo_lists[] exactly?

   Consider the case when some participant is down for a while, like days.
   There will be huge amount of redo records in the log for it. But we still
   need to be able to find quickly the redo records to be sent to online
   participants from which we don't have any pmsgs (for some reason) -
   this is part of the normal online recovery work. If we don't have such
   redo_lists[i], we will need to traverse the btree-log from the start (from
   the olders records) every time and skip a huge amount of records for the
   participant which is down. redo_lists[i] allow us to ignore offline
   participant(s) easily and work only with online ones.

   How do we add redo records to the redo_lists[]?

   There is no strict requirement for redo msgs to be ordered, but it's good
   to have them in order by txn_id in the redo_lists[]. In the initial
   implementation we can just append the records at the tail of the lists.

   Upon receiving a new cas request or redo msg, the 1st thing we must
   check whether it is in the right interval: if it's older than the min-nall-p,
   it should be dropped. (This might be a request which got stuck for a while
   in the network somewhere which is stale by now.)

   After inserting the record to the log-btree, we should add it to redo_lists[]
   for each participant the transaction belongs to.

   In most cases, the records will be sorted if we just add them to the end of
   the redo_lists[], and in a very rare cases when it is not (for example, when
   some cas request was delayed in the network for some reason so that it
   immediately falls into the III-rd interval) - the right place can be easily
   found by searching from the end of the list. As it was mentioned above, this
   optimisation can be implemented later.

   How the redo_lists[] are cleaned up?

   Upon receival of pmsg from the participant p, we find the record in the log-
   btree by its txn_id and remove it from the correspondent redo_lists[p]. If
   it was the last non-empty redo_list for this log record (which means we've
   got pmsgs from all participant for it and it becomes all-p), and this log
   record is min-nall-p - we can move min-nall-p pointer to the right until
   we find the next nall-p log record.

   What if we get pmsg for which there is no log record yet?

   Such situations may happen, indeed, when, for example, one server processes
   requests faster than the other or due to some network delays. In any case,
   we should record such pmsgs to avoid sending needless redo msgs later, which
   will only aggravate the situation on a busy networks and systems.

   One way to record such pmsgs is to create a placeholder records in the log
   with the correspondent flag in the payload structure. On the 1st such pmsg
   arrival, we should add the placeholder record to all redo_lists[] of the
   correspondent participants, except the one from which the pmsg arrived. On
   subsequent arrival of pmsgs, we can just remove the placeholder record from
   the correspondent redo_list.  On actual request arrival, we can just update
   the placeholder with the payload and don't touch the redo_lists[].

   This method has several drawbacks: 1) it generates additional transactions
   (when they are not strictly necessary); 2) it requires to add information
   about all participants into the pmsgs (additional network load). So here is
   another, more lightweight approach: collect such pmsgs in a volatile hash
   table (key - txn_id, value - list of participants we've got pmsgs from), and
   consult this table each time we create a new log record: if there were pmsgs
   for it already, don't add this log to the correspondent redo_lists[].

   */


#include "doc/dld/dld_template.h"

/**
   @defgroup DLDDFSInternal Motr Sample Module Internals
   @brief Detailed functional specification of the internals of the
   sample module.

   This example is part of the DLD Template and Style Guide. It illustrates
   how to keep internal documentation separate from external documentation
   by using multiple @@defgroup commands in different files.

   Please make sure that the module cross-reference the DLD, as shown below.

   @see @ref DLD and @ref DLD-lspec

   @{
 */

/** Structure used internally */
struct dld_sample_internal {
	int dsi_f1; /**< field to do blah */
};

/** Invariant for dld_sample_internal must be called holding the mutex */
static bool dld_sample_internal_invariant(const struct dld_sample_internal *dsi)
{
	if (dsi->dsi-f1 == 0)
		return false;
	return true;
}

/** @} */ /* end internal */

/**
   External documentation can be continued if need be - usually it should
   be fully documented in the header only.
   @addtogroup DLDFS
   @{
 */

/**
 * This is an example of bad documentation, where an external symbol is
 * not documented in the externally visible header in which it is declared.
 * This also results in Doxygen not being able to automatically reference
 * it in the Functional Specification.
 */
unsigned int dld_bad_example;

/** @} */ /* end-of-DLDFS */

/*
 *  Local variables:
 *  c-indentation-style: "K&R"
 *  c-basic-offset: 8
 *  tab-width: 8
 *  fill-column: 79
 *  scroll-step: 1
 *  End:
 */
