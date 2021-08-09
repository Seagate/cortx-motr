/* -*- C -*- */
/*
 * Copyright (c) 2021 Seagate Technology LLC and/or its Affiliates
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
   @page dtm0br-dld DLD of DTM0 basic recovery

   - @ref DTM0BR-ovw
   - @ref DTM0BR-def
   - @ref DTM0BR-req
   - @ref DTM0BR-depends
   - @ref DTM0BR-highlights
   - @subpage DTM0BR-fspec "Functional Specification"
   - @ref DTM0BR-lspec
      - @ref DTM0BR-lspec-comps
      - @ref DTM0BR-lspec-rem-fom
      - @ref DTM0BR-lspec-loc-fom
      - @ref DTM0BR-lspec-evc-fom
      - @ref DTM0BR-lspec-state
      - @ref DTM0BR-lspec-thread
      - @ref DTM0BR-lspec-numa
   - @ref DTM0BR-conformance
   - @ref DTM0BR-usecases
   - @ref DTM0BR-ref
   - @ref DTM0BR-impl-plan
--->>>
Max: [* defect *] ut and st sections are missing.
IvanA: Agree. I will add it once we are more-or-less confident in use-cases and
       requirements.
<<<---
--->>>
Max: [* style *] usecases section should go to .h file, along with the rest of
     the sections in .h in DLD template.
<<<---


   <hr>
   @section DTM0BR-ovw Overview
   <i>All specifications must start with an Overview section that
   briefly describes the document and provides any additional
   instructions or hints on how to best read the specification.</i>

   The document describes the way how DTM0 service is supposed to restore
   consistency of the data replicated across a certain kind of Motr-based
   cluster. The term "basic" implies that consistency is resored only
   for a limited number of use-cases. In the rest of the document,
   the term "basic" is omitted for simplicity (there are no other kinds of
   DTM0-based recovery at the moment).

--->>>
Max: [* defect *] This is an obvious definition, which has very small value.
     Please make an overview which would bring as much useful information to the
     reader as it can in just a few lines.
IvanA: [fixed] I tried to stick to the point, and avoid any "new" terms in
       the paragraph.
<<<---


   <hr>
   @section DTM0BR-def Definitions
   <i>Mandatory.
   The DLD shall provide definitions of the terms and concepts
   introduced by the design, as well as the relevant terms used by the
   specification but described elsewhere.  References to the
   M0 Glossary and the component's HLD are permitted and encouraged.
   Agreed upon terminology should be incorporated in the glossary.</i>

--->>>
Max: [* question *] Are those all the terms that are used here? If no, please
     add the rest of the terms. Please also consider adding terms that are being
     used, but which are not in DTM0 HLD.
IvanA: [fixed] No, it seems like they are not used. I removed this part.
       I'll populate the "new terms" first, and if there are any redundant
       information, I'll add references to the HLD.
<<<---

   Terms and definitions used in the document:
   - <b>User service</b> is a Motr service that is capable of replicating
     of its data ("user data") across the cluster. The document is focused on
     CAS (see @ref cas-dld) but this term could be applied to any Motr service
     that supports CRDT[2] and has behavior similar to CAS.
     Note, this document does not differentiate actual "clients" and "servers".
     For example, the Motr client (including DIX) is also considered to be a
     "User service".
   - <b>DTM0 service</b> is a Motr-service that helps a user service restore
     consistency of its replicated data.
   - <b>Recoverable process</b> is Motr process that has exactly one user
     service and exactly one DTM0 service in it. Note that in UT environment,
     a single OS process may have several user/DTM0 services, thus it may
     have multiple recoverable services.
--->>>
IvanA: [* question *] @Max, "Recoverable process" does not sound good to me.
       What do you think about "DTM0 process"? or "DTM0-backed process"?
       I just want to avoid confusion with confd and any other Motr process that
       does not have a DTM0 service in it. Any ideas on that?
<<<---
   - <b>Recovery procedures</b> is a broad term used to point at DTM0 services'
     reactions to HA notifications about states of DTM0 services. In other
     words, it references to actions performed by DTM0 services that
     help to restore consistency of replicated data.
   - <b>Recovery machine</b> is a state machine running with a DTM0 service
     that performs recovery procedures. Each recoverable process
     has a single instance of recovery machine.
   - <b>Recovery FOM</b> is a long-lived FOM responsible for reaction to
      HA-provided state changes of a recoverable process. The FOM knows the
      id (FID) of this process (<b>Recovery FOM id</b>). If the recovery
      FOM id matches with the id of the process it is running on, then
      this FOM is called "local". Otherwise, it is called "remote".
      If a Motr cluster has N recoverable processes in the configuration then
      each recoverable process has N recovery FOMs (one per remote counterpart
      plus one local FOM).
   - <b>Recovery FOM role</b> is a sub-set of states of a recovery FOM. Each
      recovery FOM may have one of the following roles: remote recovery,
      local recovery, eviction. The term "role" is often omitted.
   - <b>Recovery FOM reincarnation</b> is a transition between the roles
      of a recovery FOM. Reincarnation happens as a reaction to HA notifications
      about state changes of the corresponding process.
   - <b>Remote recovery role</b> defines a sub-set of states of a recovery FOM
      that performs recovery of a remote process (by sending REDO messages).
   - <b>Local recovery role </b> defines a sub-set of states of a recovery FOM
      that is responsible for starting and stopping of recovery procedures on
      the local recoverable process.
   - <b>Eviction role</b> defines a sub-set of states of a recovery FOM that is
      restores consistency of up to N-1 (N is number of recoverable processes
      in the configuration) recoverable processes after one of the recoverable
      processes of the cluster experienced a permanent failure (see HLD[1]).
   - <b>W-request</b> is a FOP sent from one user service to another that
      causes modifications of persistent storage. Such a request always
      contains DTM0-related meta-data (transaction descriptor). W-requests
      are used to replicate data across the cluster. They are stored in DTM0
      log, and replayed by DTM0 service in form of REDO messages. In case of
      CAS, W-requests are PUT and DEL operations
   - <b>R-request</b> is a user service FOP that does not modify persistent
      storage. R-requires are allowed to be sent only to the processes that
      have ONLINE state. In case of CAS, R-requests are GET and NEXT operations.
--->>>
Max: [* defect *] The terms are not defined. Please define them.
IvanA: I populated the list. I'll update the rest of the document
       in a separate round of fixes.
<<<---

   <hr>
   @section DTM0BR-req Requirements
   <i>Mandatory.
   The DLD shall state the requirements that it attempts to meet.</i>

   Recovery machine shall meet the requirements defined in the DTM0 HLD[1].
--->>>
Max: [* question *] All of them? If no, please put a list here. If so, please
     put the list anyway.
<<<---
   Additionally, it has the following list of low-level requirements:
--->>>
Max: [* question *] Why do we have those requirements? Please provide a way to
     track them to the top-level requirements or requriements for other DTM0
     subcomponents.
<<<---

   - @b R.DTM0BR.HA-Aligned State transitions of the recovery SM shall be
   aligned with the state transitions of Motr processes delivered by
   the HA subsystem to the recovery machine.
   - @b R.DTM0BR.Basic Recovery machine shall support only a subset of
   possible use-cases. The subset is defined in @ref DTM0BR-usecases.
--->>>
Max: [* defect *] Usecases are not defined.
<<<---
   - @b R.DTM0BR.No-Batching Recovery machine shall replay logs in
   a synchronous manner one-by-one.
--->>>
Max: [* question *] One-by-one log or one-by-one log record? In the latter case:
     what about records from logs on different nodes?
<<<---
--->>>
Max: [* question *] How those logs or records are ordered (to be able to process
     them one-by-one)?
<<<---
--->>>
Max: [* question *] What about performance: is it possible for this design to
     meet performance requirements? In any case please clearly state if it is
     and why.
<<<---

   <hr>
   @section DTM0BR-depends Dependencies
   <i>Mandatory. Identify other components on which this specification
   depends.</i>

   The basic recovery machine depends on the following components:
   - Hare. This component is supposed to own an ordered log of all HA events
     in the system. Note, this dependency is optional for a certain set
     of use-cases where events are delivered in the right way even without
     beign backed up by a log.
--->>>
Max: [* defect *] This is not true at the moment. Please specify if it's
     possible to implement this design without this being true or what is the
     plan to deal with this.
IvanA: [fixed] I added a note that this thing is optional for some cases.
<<<---
   - DTM0 service. The service provides access to DTM0 log and DTM0 RPC link.
   - DTM0 log. The log is used to fill in REDO messages.
   - DTM0 RPC link. The link is used as transport for REDO messages.
   - Motr HA/conf. HA states of Motr conf objects provide the information
   about state transitions of local and remote Motr processes.
   - Motr conf. The conf module provides the list of remote DTM0 services.

   <hr>
   @section DTM0BR-highlights Design Highlights
   <i>Mandatory. This section briefly summarizes the key design
   decisions that are important for understanding the functional and
   logical specifications, and enumerates topics that need special
   attention.</i>

   Each DTM0 service has a recovery machine. Each recovery machine contains
   N recovery FOMs (one per counterpart + one local). Recovery FOMs start
   listen to various events (HA events, DTM0 log events, RPC replies) and
   act accordingly (sending out M0_CONF_HA_PROCESS_* events and REDO messages).

   @verbatim

   +-------------------------------------------+
   | Recoverable process, process fid = 1      |
   | +---------------------------------------+ |
   | | DTM0 Service                          | |
   | |                                       | |
   | | +-----------------------------------+ | |
   | | | Recovery machine                  | | |
   | | |                                   | | |
   | | | +--------+ +--------+ +--------+  | | |
   | | | | R_FOM1 | | R_FOM2 | | R_FOM3 |  | | |
   | | | | FID=1  | | FID=2  | | FID=3  |  | | |
   | | | +--------+ +--------+ +--------+  | | |
   | | |                                   | | |
   | | +-----------------------------------+ | |
   | +---------------------------------------+ |
   +-------------------------------------------+

       Recovery machines of one of the processes of a 3-process cluster.
     The other two processes have the same set of FOMs. FID=N means that
     the id of this recovery FOM is N. The first one (R_FOM1) is the local
     recovery FOM of this DTM0 service. The other two (R_FOM2, R_FOM3) are
     remote recovery FOMs that would send REDOs to the processes 2 and 3.

   @endverbatim

   Each recovery FOM may await on external events:

   @verbatim

   +-----------------------------------------------------+
   |  Recovery FOM inputs                                |
   |                                                     |
   |                      Pollling                       |
   |   ----------------------------------------------    |
   |   |  HA queue |  | DTM0 log |  | RPC replies   |    |
   |   |           |  | watcher  |  |               |    |
   |   |    /|\    |  |          |  |     /|\       |    |
   |   |     |     |  |   /|\    |  |      |        |    |
   |   |     |     |  |    |     |  |      |        |    |
   |   |  conf obj |  | DTM0 log |  | DTM0 RPC link |    |
   +-----------------------------------------------------+

   @endverbatim

   For example, when a recovery FOM performs recovery of a remote
   process, it may await on HA queue (to halt sending of REDOs), on
   DTM0 log watcher (to learn if there are new entries in the log),
   or on DTM0 RPC link (to learn if next REDO message can be sent).

   Whenever a recovery FOM receives an event from the HA queue, it gets
   "reincarnated" to serve its particular role in recovery procedures
   (see "Recovery FOM reincarnation" definition).
   Incarnation of a recovery FOM is just a sub-state-machine of the FOM.
   For example, here is the full state machine of R_FOM2:

   @verbatim

   INIT -> AWAIT_HEQ (HA event queue)

   AWAIT_HEQ -> DECIDE_WHAT_TO_DO
   DECIDE_WHAT_TO_DO -> AWAIT_HEQ
   DECIDE_WHAT_TO_DO -> RESET
   DECIDE_WHAT_TO_DO -> FINAL

   RESET -> SEND_REDO

   SEND_REDO -> AWAIT_HEQ_OR_LOG
   SEND_REDO -> AWAIT_HEQ_OR_RPC

   AWAIT_HEQ_OR_LOG -> SEND_REDO
   AWAIT_HEQ_OR_LOG -> HEQ

   AWAIT_HEQ_OR_RPC -> SEND_REDO
   AWAIT_HEQ_OR_RPC -> HEQ

   INIT -------------->  AWAIT_HEQ
                         |     /|\
			\|/     |
                        DECIDE_WHAT_TO_DO --------------> FINAL
                         |    /|\
			 |     |               (remote recovery
                         |     |                 sub-states)
   //====================|=====|===================================//
   //  RESET <-----------+     +---< HEQ      (Transitions that    //
   //    |                                     may happen as       //
   //    |                                     a reaction to       //
   //    |    +------------------------+       RECOVERING HA event)//
   //   \|/  \|/                       |                           //
   //  SEND_REDO ---------------> AWAIT_ON_HEQ_OR_RPC ---> HEQ     //
   //      |  /|\                                                  //
   //     \|/  |                                                   //
   //     AWAIT_ON_HEQ_OR_LOG -----> HEQ                           //
   //==============================================================//

                        ...   ...
			 |    |             (eviction sub-states)
   //====================|====|====================================//
   //   RESET <----------+    +----< HEQ     (Transitions that     //
   //    |                                    may happen as a      //
   //    |    +------------------+            a reaction to FAILED //
   //   \|/  \|/                 |            HA event)            //
   //   SEND_REDO -----> AWAIT_ON_HEQ_OR_RPC ----> HEQ             //
   //==============================================================//
   @endverbatim

   The first frame highlighted with "===//" shows the sub-state machine of
   a recovery FOM that are used to recover a participant that entered
   RECOVERING HA state. If the same participant enters FAILED state
   (permanent failure) then another set of sub-states would be involved
   in recovery procedures (the second frame).

   On the diagram above, the transition DECIDE_WHAT_TO_DO -> RESET
   marks the point where recovery FOM enters a new incarnation,
   the transtion HEQ -> DECIDE_WHAT_TO_DO marks the end of the live of this
   incaration. The set of sub-states is called "role".
   For example, if a recovery FOM is somewhere inside the first frame
   (remote recovery) then we say that it has "remote recovery role" and it
   performs "remote recovery". Correspondingly, the second frame describes
   "eviction role" or just "eviction".

   The notion of incarnation is used to emphasise that a recovery FOM must
   always "clean up" its volatile state before processing next HA event.
   For example, it may await on a pending RPC reply or it may have to reset
   DTM0 log iterator.

   The local recovery FOM (R_FOM1 in the example), is just a volatile
   state that captures information about certain points in the log and sends
   out ::M0_CONF_HA_PROCESS_RECOVERED when it believes that the local process
   has been fully recovered (see the recovery stop condition below).
   The local recovery FOM cannot change its role (it cannot recover itself
   from transient or permanent failures).


   <hr>
   @section DTM0BR-lspec Logical Specification
   <i>Mandatory.  This section describes the internal design of the component,
   explaining how the functional specification is met.  Sub-components and
   diagrams of their interaction should go into this section.  The section has
   mandatory subsections created using the Doxygen @@subsection command.  The
   designer should feel free to use additional sub-sectioning if needed, though
   if there is significant additional sub-sectioning, provide a table of
   contents here.</i>

   - @ref DTM0BR-lspec-comps
   - @ref DTM0BR-lspec-rem-fom
   - @ref DTM0BR-lspec-loc-fom
   - @ref DTM0BR-lspec-evc-fom
   - @ref DTM0BR-lspec-state
   - @ref DTM0BR-lspec-thread
   - @ref DTM0BR-lspec-numa

   @subsection DLD-lspec-comps Component Overview
   <i>Mandatory.
   This section describes the internal logical decomposition.
   A diagram of the interaction between internal components and
   between external consumers and the internal components is useful.</i>

   The following diagram shows connections between recovery machine and
   the other components of the system:

   @dot
	digraph {
		rankdir = LR;
		bgcolor="lightblue"
		node[shape=box];
		label = "DTM0 Basic recovery machine components and dependencies";
		subgraph cluster_recovery_sm {
			label = "Recovery SM";
			bgcolor="lightyellow"
			node[shape=box];
			rfom[label="Recovery FOM"];
			E1[shape=none  label="&#8942;" fontsize=30]
			rfom_another[label="Another Recovery FOM"];
		}

		subgraph cluster_ha_conf {
			node[shape=box];
			label ="HA and Conf";
			conf_obj[label="Conf object"];
			ha[label="Motr HA"];
		}

		drlink[label="DTM0 RPC link"];
		dlog[label="DTM0 log"];

		conf_obj -> rfom[label="HA decisions"];
		rfom -> ha [label="Process events"];
		rfom -> drlink [label="REDO msgs"];
		dlog -> rfom[label="records"];
	}
   @enddot
--->>>
Max: [* defect *] Communication with other recovery foms in the same process and
     in different processes are missing. Please add.
<<<---

   The following sequence diagram represents an example of linerisation of
   HA decisions processing:
   @msc
   ha,rfom, remote;
   ha -> rfom [label="RECOVERING"];
   rfom -> remote [label="REDO(1)"];
   ha -> rfom [label="TRANSIENT"];
   remote -> rfom [label="REDO(1).reply(-ETIMEOUT)"];
   ha -> rfom [label="RECOVERING"];
   rfom -> rfom [label="Cancel recovery"];
   rfom -> remote [label="REDO(1)"];
   remote -> rfom [label="REDO(1).reply(ok)"];
   @endmsc

   In this example the recovery machine receives a sequence of events
   (TRANSIENT, RECOVERING, TRANSIENT), and reacts on them:
   - Starts replay the log by sending REDO(1).
   - Awaits on the reply.
   - Cancels recovery.
   - Re-starts replay by sending REDO(1) again.
   - Get the reply.
--->>>
Max: [* defect *] One example is not enough to describe the algorithm. Please
     explain how it's done exactly. Maybe not in this section, but somewhere in DLD.
<<<---

   @subsection DTM0BR-lspec-rem-fom Remote recovery FOM
   <i>Such sections briefly describes the purpose and design of each
   sub-component.</i>
--->>>
Max: [* defect *] State machine is there, but what the fom actually does is
     missing. Please add.
<<<---

   When a recovery FOM detects a state transition to RECOVERING of a remote
   participant, it transits to the sub-SM called "Remote recovery FOM".
   In other words, it re-incarnates as "Remote recovery FOM" as soon as
   the previous incarnation is ready to reach its "terminal" state.

   Remote recovery FOM acts as a reactor on the following kinds of events:
   - Process state transitions (from HA subsystem);
   - DTM0 log getting new records;
   - RPC replies (from DTM0 RPC link component).

   Remote recovery FOM has the following states and transitions:

   @verbatim
   REMOTE_RECOVERY(initial) -> WAITING_ON_HA_OR_RPC
   REMOTE_RECOVERY(initial) -> WAITING_ON_HA_OR_LOG
   WAITING_ON_HA_OR_RPC     -> WAITING_ON_HA_OR_LOG
   WAITING_ON_HA_OR_LOG     -> WAITING_ON_HA_OR_RPC
   WAITING_ON_HA_OR_RPC     -> NEXT_HA_STATE (terminal)
   WAITING_ON_HA_OR_LOG     -> NEXT_HA_STATE (terminal)
   WAITING_ON_HA_OR_RPC     -> SHUTDOWN (terminal)
   WAITING_ON_HA_OR_LOG     -> SHUTDOWN (terminal)

       Where:
       REMOTE_RECOVERY is a local initial state (local to the sub-SM);
       NEXT_HA_STATE is a local terminal state;
       SHUTDOWN is a global terminal state.
   @endverbatim

   @subsection DTM0BR-lspec-loc-fom Local recovery FOM
   <i>Such sections briefly describes the purpose and design of each
   sub-component.</i>

   Local recovery is used to ensure fairness of overall recovery procedure.
--->>>
Max: [* defect *] Definition of "fairness" in this context is missing. Please
     add.
<<<---
--->>>
Max: [* defect *] Definition of the "fairness" is "ensured" is missing. Please
     add.
<<<---
   Whenever a participant learns that it got all missed log records, it sends
   M0_CONF_HA_PROCESS_RECOVERED process event to the HA subsystem.

   The point where this event has to be sent is called "recovery stop
   condition". The local participant (i.e., the one that is beign in
--->>>
Max: [* typo *] being
<<<---
   the RECOVERING state) uses the information about new incoming W-requests,
   the information about the most recent (txid-wise) log entries on the other
   participants to make a decision whether recovery shall be stopped or not.

   TODO: describe the details on when and how it should stop.
--->>>
Max: [* defect *] You're right, the details are missing.
<<<---


   @subsection DTM0BR-lspec-evc-fom Eviction FOM
   <i>Such sections briefly describes the purpose and design of each
   sub-component.</i>
--->>>
Max: [* defect *] The purpose of the eviction fom is not described.
<<<---

   A recovery FOM re-incarnates as eviction FOM (i.e., enters the initial
--->>>
Max: [* defect *] A definition of "re-incarnation" is missing.
<<<---
   of the corresponding sub-SM) when the HA subsystem notifies about
--->>>
Max: [* defect *] A definition of sub-SM is missing.
<<<---
   permanent failure (FAILED) on the corresponding participant.

   The local DTM0 log shall be scanned for any record where the FAILED
--->>>
Max: [* doc *] Please add a reference to the component which does the scanning.
<<<---
--->>>
Max: [* doc *] Please explain how to scanning is done:
     - sequentially?
     - from least recent to most recent?
     - one fom/thread/whatever or multiple? How the work is distributed?
     - locks also have to be described somewhere in DLD.
<<<---
   participant participated. Such a record shall be replayed to the
   other participants that are capable of receiving REDO messages.
--->>>
Max: [* defect *] What to do with the message if a participant is in TRANSIENT
     state is not described.
<<<---

   When the log has been replayed completely, the eviction FOM notifies
--->>>
Max: [* defect *] Criteria of "log has been replayed completely" is missing.
     Please add.
<<<---
   the HA subsystem about completion and leaves the sub-SM.
--->>>
Max: [* question *] How? Please also describe what is expected from HA.
<<<---

   TODO: GC of FAILED processes and handling of clients restart.

   @subsection DTM0BR-lspec-state State Specification
   <i>Mandatory.
   This section describes any formal state models used by the component,
   whether externally exposed or purely internal.</i>

   The states of a recovery FOM is defined as a collection of the sub-SM
   states, and a few global states.

   TODO: state diagram for overall recovery FOM.
--->>>
Max: [* doc *] Also distributed state diagram.
<<<---

   @subsection DTM0BR-lspec-thread Threading and Concurrency Model
   <i>Mandatory.
   This section describes the threading and concurrency model.
   It describes the various asynchronous threads of operation, identifies
   the critical sections and synchronization primitives used
   (such as semaphores, locks, mutexes and condition variables).</i>

   Recovery machine revolves around the following kinds of threads:
--->>>
Max: [* defect *] Definition of "revolves" is missing.
<<<---
   - Locality threads (Recovery FOMs, DTM0 log updates).
   - RPC machine thread (RPC replies).

   Each recovery FOM is implemented using Motr coroutines library.
   Within the coroutine ::m0_be_op and/or ::m0_co_op is used to await
--->>>
Max: [* question *] RE: "the coroutine": which one?
<<<---
   on external events.
--->>>
Max: [* suggestion *] A list of such events would help to understand why be/co
     ops are needed.
<<<---

   DTM0 log is locked using a mutex (TBD: the log mutex or a separate mutex?)
--->>>
Max: [* question *] Entire DTM0 log is locked using a mutex? If no, please add
     unambiguous sentence. Example: "<pointer to a watcher ot whatever> is
     protected with a mutex" or "a separate mutex protects concurrent access to
     <a field>".
<<<---
   whenever recovery machine sets up or cleans up its watcher.
--->>>
Max: [* defect *] The watcher description/workflow/etc. is missing. Please add.
<<<---

   Interraction with RPC machine is wrapped by Motr RPC library and DTM0
--->>>
Max: [* typo *] s/Interraction/Interaction/
<<<---
   RPC link component. It helps to unify the type of completion objects
   across the FOM code.

   TODO: describe be/co op poll/select.

   @subsection DTM0BR-lspec-numa NUMA optimizations
   <i>Mandatory for components with programmatic interfaces.
   This section describes if optimal behavior can be supported by
   associating the utilizing thread to a single processor.</i>

   There is no so much shared resources outside of DTM0 log and
   the localities. Scaling and batching is outside of the scope
--->>>
Max: [* defect *] The number of localities is equal to the number of CPU cores
in our default configuration, so whatever uses more than one locality has to
have some kind of synchronisation.
<<<---
--->>>
Max: [* defect *] It's not clear what "scaling" and "batching" mean in this
     context. Please explain and/or add a reference where they are explained.
<<<---
   of this document.
--->>>
Max: [* defect *] FOM locality assignment is missing.
<<<---

   <hr>
   @section DTM0BR-conformance Conformance
   <i>Mandatory.
   This section cites each requirement in the @ref DTM0BR-req section,
   and explains briefly how the DLD meets the requirement.</i>
--->>>
Max: [* defect *] Top-level requirements from HLD are missing.
<<<---

   - @b I.DTM0BR.HA-Aligned Recovery machine provides an event queue that
   is beign consumed orderly by the corresponding FOM.
--->>>
Max: [* defect *] The word "queue" is present in this document only here. Please
     describe the event queue and events somewhere in this DLD.
<<<---
--->>>
Max: [* defect *] It's not clear how "HA subsystem" from the requirement is met
     by the implementation.
<<<---
   - @b I.DTM0BR.Basic Recovery machine supports only basic log replay defined
   by the remote recovery FOM and its counterpart.
--->>>
Max: [* defect *] "basic" term is not defined. Please define.
<<<---
   - @b I.DTM0BR.No-Batching Recovery machine achieves it by awaiting on
   RPC replies for every REDO message sent.
--->>>
Max: [* defect *] Maybe I should wait for I.* for performance and availability
     requirements, but as of now it's not clear how DTM recovery would catch up
     with the rest of the nodes creating new DTM0 transactions in parallel with
     DTM recovery.
<<<---

   <hr>
   @section DTM0BR-usecases
   <i>Mandatory. This section describes use-cases for recovery machine.
   </i>

   TODO: Add use-cases.
--->>>
Max: [* defect *] Right, they are missing.
<<<---


   <hr>
   @section DLD-O Analysis
   <i>This section estimates the performance of the component, in terms of
   resource (memory, processor, locks, messages, etc.) consumption,
   ideally described in big-O notation.</i>
--->>>
Max: [* doc *] Please fill this section with references to the requirements.
<<<---

   <hr>
   @section DLD-ref References
   <i>Mandatory. Provide references to other documents and components that
   are cited or used in the design.
   In particular a link to the HLD for the DLD should be provided.</i>

   - [1] <a href="https://github.com/Seagate/cortx-motr/blob/documentation/doc/dev/dtm/dtm-hld.org">
   DTM0 HLD</a>
   - [2] <a href="https://en.wikipedia.org/wiki/Conflict-free_replicated_data_type">

   <hr>
   @section DLD-impl-plan Implementation Plan
   <i>Mandatory.  Describe the steps that should be taken to implement this
   design.</i>

   - Develop pseudocode-based description of recovery acivities.
   - Identify and add missing concurrency primitives (select/poll for be op).
   - Use the pseudocode and new primitives to implement a skeleton of the FOMs.
   - Incorporate RECOVERING and TRANSIENT states with their
   new meaning as soon as they are available in the upstream code.
   - Populate the list of use-cases based on available tools.
   - Populate the list of system tests.
   - Improve the stop condition based on the use-cases and tests.
   - Add implementation of eviction FOM.
--->>>
Max: [* defect *] No mention of tests being done. Please clearly state when and
     how the tests are going to be done or where and when it would be possible
     to find this information.
<<<---
 */

/* XXX
Implementation plan
-------------------

Areas/responsibilities:
- fom-groundwork (Ivan).
- be-op-set-or (Max).
- log-watcher (Ivan).
- test-stubs (Ivan).
- drlink-completion (vacancy).
  A be op is needed to be DONE when drlink gets reply.
- REDO2CAS-replay (Sergey).
- EOL-notice (Ivan).
- RECOVER-ED-ING-groundwork (Anatoliy).
- all2all-integration-v3 (vacancy).
  It will be needed only after P9 is ready for landing.
- log-iter (vacancy).
  Extend DTM0 log API to support iteration.
- dtx-integration-v3 (Ivan).
- recovery-stop-ha-polling-v2 (vacancy).
  V2 should support a stop condition where it watcher for
  HA states of the other participants.
- versioned-cas (Ivan).

Atomic patches for upstream:
- P1 DTM0 log iterator.
- P2 BE op set or.
- P3 New HA states/events -- RECOVERED/RECOVERING.
- P4 DTM0 log watcher.
- P5 DTM0 log iterator.
- P6 A dummy REDO FOM.
- P7 REDO-fom-to-CAS-fom re-play.
- P8 Recovery machine V1.
- P9 Recovery machine V2.
- P10 Recovery machine V3.
- P11 Versioned CAS.

Dependencies between patches:
- P7 depends on P6.
- P8 depends on P3.
- P9 depends on P8, P1, P2, P5, P7
- P10 depends on P9 and P11.

Deviverables:
- Recovery machine V1: unit-tested recovery machine with a small set of cases.
- Recovery machine V2: integration-tested machine (m0crate, Hare).
- Recovery machine V3: extended use-cases (failures during recovery and so on).
*/



#define M0_TRACE_SUBSYSTEM M0_TRACE_SUBSYS_DTM0
#include "lib/trace.h"
#include "dtm0/recovery.h"    /* m0_dtm0_recovery_machine */
#include "dtm0/service.h"     /* m0_dtm0_service */
#include "dtm0/drlink.h"      /* m0_dtm0_req_post */
#include "dtm0/fop.h"         /* dtm0_req_fop */
#include "be/op.h"            /* m0_be_op */
#include "be/queue.h"         /* m0_be_queue */
#include "conf/diter.h"       /* diter */
#include "conf/helpers.h"     /* m0_confc_root_open */
#include "conf/obj.h"         /* m0_conf_obj */
#include "fop/fom.h"          /* m0_fom */
#include "lib/coroutine.h"    /* m0_co_context */
#include "lib/memory.h"       /* M0_ALLOC_PTR */
#include "reqh/reqh.h"        /* m0_reqh2confc */
#include "rpc/rpc_opcodes.h"  /* M0_DTM0_RECOVERY_FOM_OPCODE */
#include "lib/string.h"       /* m0_streq */
#include "be/dtm0_log.h"      /* m0_dtm0_log_rec */
#include "motr/setup.h"       /* m0_cs_reqh_context */
#include "addb2/identifier.h" /* M0_AVI_FOM_TO_TX */
#include "dtm0/addb2.h"       /* M0_AVI_DORM_SM_STATE */

enum {
	/*
	 * Number of HA event that could be submitted by the HA subsystem
	 * at once; where:
	 *   "HA event" means state tranistion of a single DTM0 process;
	 *   "at once" means the maximal duration of a tick of any FOM
	 *     running on the same locality as the recovery FOM that
	 *     handles HA events.
	 * XXX: The number was chosen randomly. Update the previous sentence
	 *      if you want to change the number.
	 */
	HEQ_MAX_LEN = 32,

	/*
	 * Number of HA events and EOL messages that could be submitted
	 * at once.
	 * TODO: Modify this comment once we stop putting HA events in this
	 * queue (with help of be-op-or-set). It shall be EOL-only queue.
	 */
	EOLQ_MAX_LEN = 100,
};

struct recovery_fom {
	struct m0_fom                    rf_base;

	/* Recovery machine instance that owns this FOM. */
	struct m0_dtm0_recovery_machine *rf_m;

	/** Subscription to conf obj HA state. */
	struct m0_clink                  rf_ha_clink;

	/** HA event queue populated by the clink and consumed by the FOM. */
	struct m0_be_queue               rf_heq;

	struct m0_co_context             rf_coro;

	/** Linkage for m0_dtm0_recovery_machine::rm_foms */
	struct m0_tlink                  rf_linkage;

	/** Magic for rfom tlist entry. */
	uint64_t                         rf_magic;

	struct m0_be_queue               rf_eolq;

	struct m0_be_dtm0_log_iter       rf_log_iter;

	/* Target DTM0 service FID (id of this FOM within the machine). */
	struct m0_fid                    rf_tgt_svc;

	struct m0_fid                    rf_tgt_proc;

	/** Is target DTM0 service the service ::rf_m belongs to? */
	bool                             rf_is_local;

	/** Is target DTM0 volatile? */
	bool                            rf_is_volatile;


	/** The most recent HA state of this remote DTM0 service. */
	enum m0_ha_obj_state            rf_last_known_ha_state;

	/**
	 * The most recent known state of the log on a remote DTM0 service.
	 * Note, it is impossible to guarantee that this stat is "in-sync"
	 * with ::rf_last_known_ha_state unless we have HA epochs.
	 */
	bool                            rf_last_known_eol;
};

enum eolq_item_type {
	EIT_EOL,
	EIT_HA,
	EIT_END,
};

struct eolq_item {
	enum eolq_item_type  ei_type;
	struct m0_fid        ei_source;
	enum m0_ha_obj_state ei_ha_state;
};

/*
 * A global variable to set off parts of the code that were added
 * specifically for the integration (all2all) test script.
 * Later on, they need to be removed (once we get a better
 * way of testing).
 */
const bool ALL2ALL = true;

M0_TL_DESCR_DEFINE(rfom, "recovery_fom",
		   static, struct recovery_fom, rf_linkage,
		   rf_magic, M0_DTM0_RMACH_MAGIC, M0_DTM0_RMACH_HEAD_MAGIC);
M0_TL_DEFINE(rfom, static, struct recovery_fom);

static int   populate_foms (struct m0_dtm0_recovery_machine *m);
static void unpopulate_foms(struct m0_dtm0_recovery_machine *m);

static bool recovery_fom_ha_clink_cb(struct m0_clink *clink);

static void recovery_fom_self_fini(struct m0_fom *fom);
static int  recovery_fom_tick(struct m0_fom *fom);

static void recovery_machine_lock(struct m0_dtm0_recovery_machine *m);
static void recovery_machine_unlock(struct m0_dtm0_recovery_machine *m);
static struct recovery_fom *
recovery_fom_local(struct m0_dtm0_recovery_machine *m);

static const struct m0_dtm0_recovery_machine_ops default_ops;

static bool ha_event_invariant(uint64_t event)
{
	return event < M0_NC_NR;
}

static void addb2_relate(const struct m0_sm *left, const struct m0_sm *right)
{
	M0_ADDB2_ADD(M0_AVI_FOM_TO_TX, m0_sm_id_get(left), m0_sm_id_get(right));
}


static struct m0_sm_state_descr recovery_machine_states[] = {
	[M0_DRMS_INIT] = {
		.sd_name      = "M0_DRMS_INIT",
		.sd_allowed   = M0_BITS(M0_DRMS_STOPPED, M0_DRMS_STARTED),
		.sd_flags     = M0_SDF_INITIAL,
	},
	[M0_DRMS_STARTED] = {
		.sd_name      = "M0_DRMS_STARTED",
		.sd_allowed   = M0_BITS(M0_DRMS_STOPPED),
		.sd_flags     = 0,
	},
	[M0_DRMS_STOPPED] = {
		.sd_name      = "M0_DRMS_STOPPED",
		.sd_allowed   = 0,
		.sd_flags     = M0_SDF_TERMINAL,
	},
};

static struct m0_sm_trans_descr recovery_machine_trans[] = {
	{ "started",      M0_DRMS_INIT,    M0_DRMS_STARTED },
	{ "stop-running", M0_DRMS_STARTED, M0_DRMS_STOPPED },
	{ "stop-idle",    M0_DRMS_INIT,    M0_DRMS_STOPPED },
};

struct m0_sm_conf m0_drm_sm_conf = {
	.scf_name      = "recovery_machine",
	.scf_nr_states = ARRAY_SIZE(recovery_machine_states),
	.scf_state     = recovery_machine_states,
	.scf_trans_nr  = ARRAY_SIZE(recovery_machine_trans),
	.scf_trans     = recovery_machine_trans,
};

static void ops_apply(struct m0_dtm0_recovery_machine_ops *out,
		      const struct m0_dtm0_recovery_machine_ops *def,
		      const struct m0_dtm0_recovery_machine_ops *over)
{
#define OVERWRITE_IF_SET(op) \
	out->op = over != NULL && over->op != NULL ? over->op : def->op; \
	M0_ASSERT_INFO(out->op != NULL, "Op %s is not set", #op)
	OVERWRITE_IF_SET(log_iter_init);
	OVERWRITE_IF_SET(log_iter_fini);
	OVERWRITE_IF_SET(log_iter_next);
	OVERWRITE_IF_SET(redo_post);
	OVERWRITE_IF_SET(ha_event_post);
#undef OVERWRITE_IF_SET
}

M0_INTERNAL int
m0_dtm0_recovery_machine_init(struct m0_dtm0_recovery_machine           *m,
			      const struct m0_dtm0_recovery_machine_ops *ops,
			      struct m0_dtm0_service                    *svc)
{
	M0_PRE(m != NULL);
	M0_ENTRY("m=%p, svc=%p", m, svc);
	M0_PRE(m0_sm_conf_is_initialized(&m0_drm_sm_conf));

	/* TODO: Skip initialisation of recovery foms during mkfs. */

	m->rm_svc = svc;
	ops_apply(&m->rm_ops, &default_ops, ops);
	rfom_tlist_init(&m->rm_rfoms);
	m0_sm_group_init(&m->rm_sm_group);
	m0_sm_init(&m->rm_sm, &m0_drm_sm_conf,
		   M0_DRMS_INIT, &m->rm_sm_group);
	m0_sm_addb2_counter_init(&m->rm_sm);
	return M0_RC(populate_foms(m));
}

M0_INTERNAL void
m0_dtm0_recovery_machine_fini(struct m0_dtm0_recovery_machine *m)
{
	M0_ENTRY("m=%p", m);
	unpopulate_foms(m);
	recovery_machine_lock(m);
	if (m->rm_sm.sm_state == M0_DRMS_INIT)
		m0_sm_state_set(&m->rm_sm, M0_DRMS_STOPPED);
	else
		M0_ASSERT(m->rm_sm.sm_state == M0_DRMS_STOPPED);
	m0_sm_fini(&m->rm_sm);
	recovery_machine_unlock(m);
	m0_sm_group_fini(&m->rm_sm_group);
	M0_ASSERT(rfom_tlist_is_empty(&m->rm_rfoms));
	rfom_tlist_fini(&m->rm_rfoms);
	M0_LEAVE();
}

M0_INTERNAL void
m0_dtm0_recovery_machine_start(struct m0_dtm0_recovery_machine *m)
{
	struct recovery_fom *rf;

	M0_PRE(equi(rfom_tlist_is_empty(&m->rm_rfoms),
		    m->rm_local_rfom == NULL));

	m0_tl_for(rfom, &m->rm_rfoms, rf) {
		m0_fom_queue(&rf->rf_base);
	}
	m0_tlist_endfor;

	if (m->rm_local_rfom != NULL) {
		recovery_machine_lock(m);
		m0_sm_state_set(&m->rm_sm, M0_DRMS_STARTED);
		recovery_machine_unlock(m);
	}

	if (ALL2ALL)
		M0_LOG(M0_DEBUG, "ALL2ALL_STARTED");
}

M0_INTERNAL void
m0_dtm0_recovery_machine_stop(struct m0_dtm0_recovery_machine *m)
{
	int                  rc;
	struct recovery_fom *rf;

	m0_tl_for(rfom, &m->rm_rfoms, rf) {
		m0_be_queue_lock(&rf->rf_heq);
		M0_LOG(M0_DEBUG, "heq_end " FID_F, FID_P(&rf->rf_tgt_svc));
		m0_be_queue_end(&rf->rf_heq);
		m0_be_queue_unlock(&rf->rf_heq);
	}
	m0_tlist_endfor;

	recovery_machine_lock(m);
	rc = m0_sm_timedwait(&m->rm_sm,
			     M0_BITS(M0_DRMS_INIT, M0_DRMS_STOPPED),
			     M0_TIME_NEVER);
	M0_ASSERT_INFO(rc == 0, "rc=%d", rc);
	recovery_machine_unlock(m);
}

static struct m0_reqh *
m0_dtm0_recovery_machine_reqh(struct m0_dtm0_recovery_machine *m)
{
	return m->rm_svc->dos_generic.rs_reqh;
}

static const struct m0_fid *
recovery_machine_local_id(const struct m0_dtm0_recovery_machine *m)
{
	return &m->rm_svc->dos_generic.rs_service_fid;
}

static int recovery_machine_log_iter_next(struct m0_dtm0_recovery_machine *m,
					  struct m0_be_dtm0_log_iter *iter,
					  const struct m0_fid *tgt_svc,
					  const struct m0_fid *origin_svc,
					  struct m0_dtm0_log_rec *record)
{
	M0_PRE(m->rm_ops.log_iter_next != NULL);
	return m->rm_ops.log_iter_next(m, iter, tgt_svc, origin_svc, record);
}

static int recovery_machine_log_iter_init(struct m0_dtm0_recovery_machine *m,
					  struct m0_be_dtm0_log_iter      *iter)
{
	M0_PRE(m->rm_ops.log_iter_init != NULL);
	return m->rm_ops.log_iter_init(m, iter);

}

static void recovery_machine_log_iter_fini(struct m0_dtm0_recovery_machine *m,
					   struct m0_be_dtm0_log_iter      *iter)
{
	M0_PRE(m->rm_ops.log_iter_fini != NULL);
	m->rm_ops.log_iter_fini(m, iter);
}

static void recovery_machine_redo_post(struct m0_dtm0_recovery_machine *m,
				       struct m0_fom                   *fom,
				       const struct m0_fid *tgt_proc,
				       const struct m0_fid *tgt_svc,
				       struct dtm0_req_fop *redo,
				       struct m0_be_op *op)
{
	M0_PRE(m->rm_ops.redo_post != NULL);
	m->rm_ops.redo_post(m, fom, tgt_proc, tgt_svc, redo, op);
}

static void recovery_machine_recovered(struct m0_dtm0_recovery_machine *m,
				       const struct m0_fid *tgt_proc,
				       const struct m0_fid *tgt_svc)
{
	M0_PRE(m->rm_ops.ha_event_post != NULL);
	m->rm_ops.ha_event_post(m, tgt_proc, tgt_svc,
				M0_CONF_HA_PROCESS_DTM_RECOVERED);
}

static void recovery_machine_lock(struct m0_dtm0_recovery_machine *m)
{
	m0_sm_group_lock(&m->rm_sm_group);
}

static void recovery_machine_unlock(struct m0_dtm0_recovery_machine *m)
{
	m0_sm_group_unlock(&m->rm_sm_group);
}

enum recovery_fom_state {
	RFS_INIT = M0_FOM_PHASE_INIT,
	RFS_DONE = M0_FOM_PHASE_FINISH,
	RFS_WAITING,
	RFS_FAILED,
	RFS_NR,
};

static struct m0_sm_state_descr recovery_fom_states[] = {
	[RFS_INIT] = {
		.sd_name      = "RFS_INIT",
		.sd_allowed   = M0_BITS(RFS_WAITING, RFS_DONE, RFS_FAILED),
		.sd_flags     = M0_SDF_INITIAL,
	},
	/* terminal states */
	[RFS_DONE] = {
		.sd_name      = "RFS_DONE",
		.sd_allowed   = 0,
		.sd_flags     = M0_SDF_TERMINAL,
	},
	/* failure states */
	[RFS_FAILED] = {
		.sd_name      = "RFS_FAILED",
		.sd_allowed   = M0_BITS(RFS_DONE),
		.sd_flags     = M0_SDF_FAILURE,
	},

	/* intermediate states */
#define _ST(name, allowed)            \
	[name] = {                    \
		.sd_name    = #name,  \
		.sd_allowed = allowed \
	}
	_ST(RFS_WAITING,           M0_BITS(RFS_DONE,
					   RFS_FAILED, RFS_WAITING)),
#undef _ST
};

const static struct m0_sm_conf recovery_fom_conf = {
	.scf_name      = "recovery_fom",
	.scf_nr_states = ARRAY_SIZE(recovery_fom_states),
	.scf_state     = recovery_fom_states,
};

static size_t recovery_fom_locality(const struct m0_fom *fom)
{
	return 1;
}

static const struct m0_fom_ops recovery_fom_ops = {
	.fo_fini          = recovery_fom_self_fini,
	.fo_tick          = recovery_fom_tick,
	.fo_home_locality = recovery_fom_locality
};

static struct m0_fom_type recovery_fom_type;
static const struct m0_fom_type_ops recovery_fom_type_ops = {};

M0_INTERNAL int m0_drm_domain_init(void)
{
	int         rc = 0;

	if (!m0_sm_conf_is_initialized(&m0_drm_sm_conf)) {
		m0_fom_type_init(&recovery_fom_type,
				 M0_DTM0_RECOVERY_FOM_OPCODE,
				 &recovery_fom_type_ops,
				 &dtm0_service_type,
				 &recovery_fom_conf);

		m0_sm_conf_init(&m0_drm_sm_conf);
		rc = m0_sm_addb2_init(&m0_drm_sm_conf,
				      M0_AVI_DRM_SM_STATE,
				      M0_AVI_DRM_SM_COUNTER);

		M0_POST(m0_sm_conf_is_initialized(&m0_drm_sm_conf));
	}

	return M0_RC(rc);
}

M0_INTERNAL void m0_drm_domain_fini(void)
{
	if (m0_sm_conf_is_initialized(&m0_drm_sm_conf)) {
		m0_sm_addb2_fini(&m0_drm_sm_conf);
		m0_sm_conf_fini(&m0_drm_sm_conf);
		M0_POST(!m0_sm_conf_is_initialized(&m0_drm_sm_conf));
	}
}

static void eolq_post(struct m0_dtm0_recovery_machine *m,
		      struct eolq_item                *item)
{
	struct recovery_fom *rf = recovery_fom_local(m);
	m0_be_queue_lock(&rf->rf_eolq);
	/*
	 * Do not post any events if we have already recovered this DTM0
	 * service (eoq is an indicator here).
	 */
	if (!rf->rf_eolq.bq_the_end) {
		/*
		 * Assumption: the queue never gets full.
		 * XXX: We could panic in this case. Right now, the HA/REDO FOM
		 * should get stuck in the tick. Prorably, panic is
		 * a better solution here.
		 */
		M0_BE_OP_SYNC(op, M0_BE_QUEUE_PUT(&rf->rf_eolq, &op, item));
	}
	m0_be_queue_unlock(&rf->rf_eolq);
}

static void heq_post(struct recovery_fom *rf, enum m0_ha_obj_state state)
{
	uint64_t event = state;
	m0_be_queue_lock(&rf->rf_heq);
	/*
	 * Assumption: the queue never gets full.
	 * XXX: We could panic in this case. Right now, the HA FOM should get
	 *      stuck in the tick. Prorably, panic is a better solution
	 *      here.
	 */
	M0_BE_OP_SYNC(op, M0_BE_QUEUE_PUT(&rf->rf_heq, &op, &event));
	m0_be_queue_unlock(&rf->rf_heq);

	M0_LOG(M0_DEBUG, "heq_enq " FID_F " %s ",
	       FID_P(&rf->rf_tgt_svc),
	       m0_ha_state2str(state));

	if (!rf->rf_is_local)
		eolq_post(rf->rf_m,
			  &(struct eolq_item) {
			  .ei_type = EIT_HA,
			  .ei_ha_state = state,
			  .ei_source = rf->rf_tgt_svc, });
}

static bool recovery_fom_ha_clink_cb(struct m0_clink *clink)
{
	struct recovery_fom *rf        = M0_AMB(rf, clink, rf_ha_clink);
	struct m0_conf_obj  *proc_conf = container_of(clink->cl_chan,
						       struct m0_conf_obj,
						       co_ha_chan);
	heq_post(rf, proc_conf->co_ha_state);
	return false;
}

static int recovery_fom_init(struct recovery_fom             *rf,
			     struct m0_dtm0_recovery_machine *m,
			     struct m0_conf_process          *proc_conf,
			     const struct m0_fid             *target,
			     bool                             is_volatile)
{
	int rc;
	bool is_local = m0_fid_eq(recovery_machine_local_id(m), target);

	M0_ENTRY("m=%p, rf=%p, tgt=" FID_F ", is_vol=%d, is_local=%d",
		 m, rf, FID_P(target), !!is_volatile, !!is_local);

	M0_PRE(ergo(is_local, m->rm_local_rfom == NULL));

	rc = m0_be_queue_init(&rf->rf_heq, &(struct m0_be_queue_cfg){
		.bqc_q_size_max       = HEQ_MAX_LEN,
		/* Conf-obj (1) and stop-and-wait-when-finalising (2) */
		.bqc_producers_nr_max = 2, /* XXX */
		.bqc_consumers_nr_max = 1, /* XXX */
		.bqc_item_length      = sizeof(uint64_t),
	});
	if (rc != 0)
		return M0_ERR(rc);

	if (is_local) {
		rc = m0_be_queue_init(&rf->rf_eolq, &(struct m0_be_queue_cfg){
				.bqc_q_size_max       = EOLQ_MAX_LEN,
				.bqc_producers_nr_max = 2,   /* XXX */
				.bqc_consumers_nr_max = 1,   /* XXX */
				.bqc_item_length      = sizeof(struct eolq_item),
		});
		if (rc != 0) {
			m0_be_queue_fini(&rf->rf_heq);
			return M0_ERR(rc);
		}
		M0_ASSERT(!rf->rf_eolq.bq_the_end);
		m->rm_local_rfom = rf;
	}

	rf->rf_m = m;
	rf->rf_tgt_svc = *target;
	rf->rf_is_local = is_local;
	rf->rf_is_volatile = is_volatile;

	rfom_tlink_init(rf);
	m0_co_context_init(&rf->rf_coro);

	m0_clink_init(&rf->rf_ha_clink, recovery_fom_ha_clink_cb);
	m0_clink_add_lock(&proc_conf->pc_obj.co_ha_chan, &rf->rf_ha_clink);

	m0_fom_init(&rf->rf_base, &recovery_fom_type,
		    &recovery_fom_ops, NULL, NULL,
		    m0_dtm0_recovery_machine_reqh(m));

	rfom_tlist_add_tail(&m->rm_rfoms, rf);
	return M0_RC(rc);
}

/*
 * Mark the queue as ended and drain it until the end.
 */
static void m0_be_queue__finish(struct m0_be_queue *bq, struct m0_buf *item)
{
	bool got = true;

	m0_be_queue_lock(bq);
	if (!bq->bq_the_end) {
		m0_be_queue_end(bq);
		while (got)
			M0_BE_OP_SYNC(op, m0_be_queue_get(bq, &op, item, &got));
	}
	M0_POST(bq->bq_the_end);
	m0_be_queue_unlock(bq);
}
#define M0_BE_QUEUE__FINISH(bq, item_type) ({                \
	item_type item;                                      \
	m0_be_queue__finish(bq, &M0_BUF_INIT_PTR(&item));    \
})

static void recovery_fom_fini(struct recovery_fom *rf)
{
	M0_ENTRY("m=%p, rf= %p", rf->rf_m, rf);
	m0_clink_del_lock(&rf->rf_ha_clink);
	m0_clink_fini(&rf->rf_ha_clink);
	m0_co_context_fini(&rf->rf_coro);
	rfom_tlink_fini(rf);
	if (rf->rf_is_local) {
		rf->rf_m->rm_local_rfom = NULL;
		m0_be_queue_fini(&rf->rf_eolq);
	}
	m0_be_queue_fini(&rf->rf_heq);
	M0_LEAVE();
}

static void recovery_fom_self_fini(struct m0_fom *fom)
{
	struct recovery_fom *rf = M0_AMB(rf, fom, rf_base);
	struct m0_dtm0_recovery_machine *m = rf->rf_m;
	bool                 is_stopped;

	M0_ENTRY("fom=%p, m=%p, rf=%p", fom, m, rf);
	recovery_machine_lock(m);
	rfom_tlist_remove(rf);
	is_stopped = rfom_tlist_is_empty(&m->rm_rfoms);
	recovery_fom_fini(rf);
	m0_fom_fini(fom);
	m0_free(rf);
	if (is_stopped)
		m0_sm_move(&m->rm_sm, 0, M0_DRMS_STOPPED);
	recovery_machine_unlock(m);
	M0_LEAVE("is_stopped=%d", !!is_stopped);
}

static int recovery_fom_add(struct m0_dtm0_recovery_machine *m,
			    struct m0_conf_process          *proc_conf,
			    const struct m0_fid             *target,
			    bool                             is_volatile)
{
	struct recovery_fom *rf;
	int                  rc;

	M0_ALLOC_PTR(rf);
	if (rf != NULL) {
		recovery_machine_lock(m);
		rc = recovery_fom_init(rf, m, proc_conf, target, is_volatile);
		recovery_machine_unlock(m);
	} else
		rc = M0_ERR(-ENOMEM);

	return M0_RC(rc);
}

static struct recovery_fom *
recovery_fom_by_svc_find(struct m0_dtm0_recovery_machine *m,
			 const struct m0_fid             *tgt_svc)
{
	return m0_tl_find(rfom, rf, &m->rm_rfoms,
			  m0_fid_eq(tgt_svc, &rf->rf_tgt_svc));
}

static struct recovery_fom *
recovery_fom_by_svc_find_lock(struct m0_dtm0_recovery_machine *m,
			      const struct m0_fid             *tgt_svc)
{
	struct recovery_fom *rf;
	recovery_machine_lock(m);
	rf = recovery_fom_by_svc_find(m, tgt_svc);
	recovery_machine_unlock(m);
	return rf;
}

static struct recovery_fom *
recovery_fom_local(struct m0_dtm0_recovery_machine *m)
{
	return recovery_fom_by_svc_find_lock(m, recovery_machine_local_id(m));
}

static void unpopulate_foms(struct m0_dtm0_recovery_machine *m)
{
	struct recovery_fom *rf;

	M0_ENTRY("m=%p", m);

	recovery_machine_lock(m);
	m0_tl_teardown(rfom, &m->rm_rfoms, rf) {
		recovery_fom_fini(rf);
		m0_free(rf);
	}
	recovery_machine_unlock(m);

	M0_LEAVE();
}

static bool conf_obj_is_process(const struct m0_conf_obj *obj)
{
	return m0_conf_obj_type(obj) == &M0_CONF_PROCESS_TYPE;
}

static bool is_svc_volatile(const struct m0_confc        *confc,
			    const struct m0_fid          *svc_fid)
{
	struct m0_conf_service *svc;
	struct m0_conf_obj     *obj;
	const char            **param;

	obj = m0_conf_cache_lookup(&confc->cc_cache, svc_fid);
	M0_ASSERT(obj != NULL);

	svc = M0_CONF_CAST(obj, m0_conf_service);

	M0_ASSERT(svc->cs_params != NULL);

	for (param = svc->cs_params; *param != NULL; ++param) {
		if (m0_streq(*param, "origin:in-volatile"))
			return true;
		else if (m0_streq(*param, "origin:in-persistent"))
			return false;
	}

	M0_IMPOSSIBLE("Service origin is not defined in the config?");
}

static int populate_foms(struct m0_dtm0_recovery_machine *m)
{
	struct m0_reqh_service *service = &m->rm_svc->dos_generic;
	struct m0_confc        *confc = m0_reqh2confc(service->rs_reqh);
	struct m0_conf_obj     *obj;
	struct m0_conf_root    *root;
	struct m0_conf_diter    it;
	struct m0_conf_process *proc_conf;
	struct m0_fid           svc_fid;
	int                     rc;

	M0_ENTRY("recovery machine=%p", m);

	/** UT workaround */
	if (!m0_confc_is_inited(confc)) {
		M0_LOG(M0_WARN, "confc is not initiated!");
		return M0_RC(0);
	}

	rc = m0_confc_root_open(confc, &root) ?:
		m0_conf_diter_init(&it, confc,
				   &root->rt_obj,
				   M0_CONF_ROOT_NODES_FID,
				   M0_CONF_NODE_PROCESSES_FID);
	if (rc != 0)
		goto out;

	while ((rc = m0_conf_diter_next_sync(&it, conf_obj_is_process)) > 0) {
		obj = m0_conf_diter_result(&it);
		proc_conf = M0_CONF_CAST(obj, m0_conf_process);
		rc = m0_conf_process2service_get(confc,
						 &proc_conf->pc_obj.co_id,
						 M0_CST_DTM0, &svc_fid);
		if (rc != 0)
			continue;

		rc = recovery_fom_add(m, proc_conf, &svc_fid,
				      is_svc_volatile(confc, &svc_fid));
		if (rc != 0)
			break;
	}

	m0_conf_diter_fini(&it);
out:
	if (root != NULL)
		m0_confc_close(&root->rt_obj);
	if (rc != 0)
		unpopulate_foms(m);
	return M0_RC(rc);
}

static struct m0_co_context *CO(struct m0_fom *fom)
{
	struct recovery_fom *rf = M0_AMB(rf, fom, rf_base);
	return &rf->rf_coro;
}

#define F M0_CO_FRAME_DATA

static void heq_await(struct m0_fom *fom, enum m0_ha_obj_state *out, bool *eoq)
{
	struct recovery_fom *rf = M0_AMB(rf, fom, rf_base);

	M0_CO_REENTER(CO(fom),
		      struct m0_be_op op;
		      bool            got;
		      uint64_t        state;);

	F(got) = false;
	F(state) = 0;
	M0_SET0(&F(op));
	m0_be_op_init(&F(op));
	m0_be_queue_lock(&rf->rf_heq);
	M0_BE_QUEUE_GET(&rf->rf_heq, &F(op), &F(state), &F(got));
	m0_be_queue_unlock(&rf->rf_heq);
	M0_CO_YIELD_RC(CO(fom), m0_be_op_tick_ret(&F(op), fom, RFS_WAITING));
	m0_be_op_fini(&F(op));


	if (F(got)) {
		M0_LOG(M0_DEBUG, "heq_deq " FID_F " %s" ,
		       FID_P(&rf->rf_tgt_svc),
		       m0_ha_state2str(F(state)));
		M0_ASSERT(ha_event_invariant(F(state)));
		*out = F(state);
	} else {
		M0_LOG(M0_DEBUG, "heq_deq " FID_F, FID_P(&rf->rf_tgt_svc));
		*eoq = true;
	}
}

static void eolq_await(struct m0_fom *fom, struct eolq_item *out)
{
	struct recovery_fom *rf = M0_AMB(rf, fom, rf_base);

	M0_CO_REENTER(CO(fom),
		      struct m0_be_op op;
		      bool            got;
		      struct eolq_item item;);

	F(got) = false;
	M0_SET0(&F(item));
	M0_SET0(&F(op));
	m0_be_op_init(&F(op));
	m0_be_queue_lock(&rf->rf_eolq);
	M0_BE_QUEUE_GET(&rf->rf_eolq, &F(op), &F(item), &F(got));
	m0_be_queue_unlock(&rf->rf_eolq);
	M0_CO_YIELD_RC(CO(fom), m0_be_op_tick_ret(&F(op), fom, RFS_WAITING));
	m0_be_op_fini(&F(op));

	*out = F(got) ? F(item) : (struct eolq_item) { .ei_type = EIT_END };
}

static void restore(struct m0_fom *fom,
		    enum m0_ha_obj_state *out, bool *eoq)
{
	struct recovery_fom   *rf = M0_AMB(rf, fom, rf_base);
	struct m0_dtm0_log_rec record;
	int                    rc = 0;

	M0_CO_REENTER(CO(fom),
		      struct dtm0_req_fop redo;
		      struct m0_fid       initiator;
		      struct m0_be_op     reply_op;
		      bool                next;
		      );

	recovery_machine_log_iter_init(rf->rf_m, &rf->rf_log_iter);

	/* XXX: race condition in the case where we are stopping the FOM. */
	F(initiator) = recovery_fom_local(rf->rf_m)->rf_tgt_svc;

	M0_SET0(&F(reply_op));
	m0_be_op_init(&F(reply_op));

	do {
		M0_SET0(&record);
		rc = recovery_machine_log_iter_next(rf->rf_m, &rf->rf_log_iter,
						    &rf->rf_tgt_svc, NULL,
						    &record);
		/* Any value except zero means that we should stop recovery. */
		F(next) = rc == 0;
		F(redo) = (struct dtm0_req_fop) {
			.dtr_msg       = DTM_REDO,
			.dtr_initiator = F(initiator),
			.dtr_payload   = record.dlr_payload,
			.dtr_txr       = record.dlr_txd,
			.dtr_flags     = F(next) ? 0 : M0_BITS(M0_DMF_EOL),
		};

		recovery_machine_redo_post(rf->rf_m, &rf->rf_base,
					   &rf->rf_tgt_proc, &rf->rf_tgt_svc,
					   &F(redo), &F(reply_op));

		M0_LOG(M0_DEBUG, "out-redo: (m=%p) " REDO_F,
		       rf->rf_m, REDO_P(&F(redo)));
		M0_CO_YIELD_RC(CO(fom), m0_be_op_tick_ret(&F(reply_op),
							  fom, RFS_WAITING));
		m0_be_op_reset(&F(reply_op));
	} while (F(next));

	m0_be_op_fini(&F(reply_op));

	recovery_machine_log_iter_fini(rf->rf_m, &rf->rf_log_iter);
	M0_SET0(&rf->rf_log_iter);

	M0_CO_FUN(CO(fom), heq_await(fom, out, eoq));
}

static void remote_recovery_fom_coro(struct m0_fom *fom)
{
	struct recovery_fom *rf = M0_AMB(rf, fom, rf_base);

	M0_CO_REENTER(CO(fom),
		      struct m0_be_op op;
		      bool            eoq;
		      enum m0_ha_obj_state state;
		      void (*action)(struct m0_fom *fom,
				     enum m0_ha_obj_state *out, bool *eoq);
		      );

	F(eoq) = false;
	F(state) = M0_NC_UNKNOWN;

	while (!F(eoq)) {
		M0_LOG(M0_DEBUG, "remote recovery fom=%p handles %d state.",
		       fom, F(state));

		switch (F(state)) {
		case M0_NC_DTM_RECOVERING:
			F(action) = rf->rf_is_volatile ? heq_await : restore;
			break;
		case M0_NC_FAILED:
			if (ALL2ALL)
				M0_LOG(M0_WARN, "Eviction is not supported.");
			else
				M0_IMPOSSIBLE("Eviction is not supported.");
		default:
			F(action) = heq_await;
			break;
		}

		M0_CO_FUN(CO(fom), F(action)(fom, &F(state), &F(eoq)));
	}

	m0_fom_phase_set(fom, RFS_DONE);
}

static bool was_log_replayed(struct recovery_fom *rf)
{
	/*
	 * We should ignore UNKNOWN and FAILED clients.
	 */
	bool is_na = rf->rf_is_volatile &&
		M0_IN(rf->rf_last_known_ha_state,
		      (M0_NC_UNKNOWN, M0_NC_FAILED));

	bool outcome = ergo(!rf->rf_is_local && !is_na,
			    M0_IN(rf->rf_last_known_ha_state,
				  (M0_NC_ONLINE, M0_NC_DTM_RECOVERING)) &&
			    rf->rf_last_known_eol);
	return outcome;
}

static void rec_cond_trace(struct m0_dtm0_recovery_machine *m)
{
	struct recovery_fom *rf;

	if (!m0_tl_exists(rfom, rf, &m->rm_rfoms, !rf->rf_is_local))
		M0_LOG(M0_WARN, "Recovery cannot be completed because there "
		       "are no remote DTM0 services.");

	m0_tl_for(rfom, &m->rm_rfoms, rf) {
		M0_LOG(M0_DEBUG,
		       "id=" FID_F ", is_volatile=%d, "
		       "is_local=%d, state=%s, got_eol=%d => %d",
		       FID_P(&rf->rf_tgt_svc),
		       (int) rf->rf_is_volatile,
		       (int) rf->rf_is_local,
		       m0_ha_state2str(rf->rf_last_known_ha_state),
		       (int) rf->rf_last_known_eol,
		       (int) was_log_replayed(rf));
	} m0_tl_endfor;
}

static bool is_local_recovery_completed(struct m0_dtm0_recovery_machine *m)
{
	M0_ENTRY();
	rec_cond_trace(m);
	return M0_RC(m0_tl_exists(rfom, r, &m->rm_rfoms, !r->rf_is_local) &&
		m0_tl_forall(rfom, r, &m->rm_rfoms, was_log_replayed(r)));
}

static void remote_state_update(struct recovery_fom    *rf,
				const struct eolq_item *item)
{
	switch (item->ei_type) {
	case EIT_HA:
		rf->rf_last_known_ha_state = item->ei_ha_state;
		/* Clear the EOL flag if the remote is dead. */
		if (M0_IN(item->ei_ha_state, (M0_NC_TRANSIENT, M0_NC_FAILED)))
			rf->rf_last_known_eol = false;
		break;
	case EIT_EOL:
		rf->rf_last_known_eol = true;
		break;
	default:
		M0_IMPOSSIBLE("Wrong eolq item type %d?", item->ei_type);
		break;
	}
}

static void local_recovery_fom_coro(struct m0_fom *fom)
{
	struct recovery_fom *rf = M0_AMB(rf, fom, rf_base);
	struct recovery_fom *remote_rf;

	M0_CO_REENTER(CO(fom),
		      struct m0_be_op      eolq_op;
		      bool                 eoq;
		      bool                 recovered;
		      struct eolq_item     item;
		      enum m0_ha_obj_state state;);

	F(eoq) = false;
	/*
	 * A DTM0 service without persistent storage does not need
	 * REDOs.
	 */
	F(recovered) = rf->rf_is_volatile;

	/* Wait until the moment where we should start recovery. */
	do {
		M0_CO_FUN(CO(fom), heq_await(fom, &F(state), &F(eoq)));
		if (F(eoq))
			goto out;

		if (F(state) == M0_NC_ONLINE && !ALL2ALL)
			break;
	} while (F(state) != M0_NC_DTM_RECOVERING);

	if (F(state) == M0_NC_ONLINE) {
		M0_LOG(M0_WARN, "HA told DTM0 service to skip recovery.");
		F(recovered) = true;
	}

	while (!F(recovered)) {
		M0_CO_FUN(CO(fom), eolq_await(fom, &F(item)));
		M0_ASSERT(F(item).ei_type != EIT_END);

		recovery_machine_lock(rf->rf_m);
		remote_rf = recovery_fom_by_svc_find(rf->rf_m,
						     &F(item).ei_source);
		if (remote_rf == NULL) {
			/* XXX: machine is stopping? */
			recovery_machine_unlock(rf->rf_m);
			break;
		}
		M0_ASSERT(remote_rf != rf);
		remote_state_update(remote_rf, &F(item));

		F(recovered) = is_local_recovery_completed(rf->rf_m);
		recovery_machine_unlock(rf->rf_m);
	}

	M0_BE_QUEUE__FINISH(&rf->rf_eolq, typeof(F(item)));

	/*
	 * Emit "RECOVERED". It shall cause HA to tell us to transit from
	 * RECOVERING to ONLINE.
	 */
	if (F(recovered)) {
		recovery_machine_recovered(rf->rf_m,
					   &rf->rf_tgt_proc, &rf->rf_tgt_svc);

		M0_CO_FUN(CO(fom), heq_await(fom, &F(state), &F(eoq)));
		M0_ASSERT(ergo(!F(eoq), F(state) == M0_NC_ONLINE));
		if (!F(eoq)) {
			M0_CO_FUN(CO(fom), heq_await(fom, &F(state),
						     &F(eoq)));
			M0_ASSERT(F(eoq));
		}
	}

out:
	M0_BE_QUEUE__FINISH(&rf->rf_eolq, typeof(F(item)));
	m0_fom_phase_set(fom, RFS_DONE);
}

static void recovery_fom_coro(struct m0_fom *fom)
{
	struct recovery_fom *rf = M0_AMB(rf, fom, rf_base);

	M0_CO_REENTER(CO(fom));

	addb2_relate(&rf->rf_m->rm_sm, &fom->fo_sm_phase);

	if (rf->rf_is_local)
		M0_CO_FUN(CO(fom), local_recovery_fom_coro(fom));
	else
		M0_CO_FUN(CO(fom), remote_recovery_fom_coro(fom));
}

static int recovery_fom_tick(struct m0_fom *fom)
{
	int rc;
	M0_CO_START(CO(fom));
	recovery_fom_coro(fom);
	rc = M0_CO_END(CO(fom));
	M0_POST(M0_IN(rc, (0, M0_FSO_AGAIN, M0_FSO_WAIT)));
	return rc ?: M0_FSO_WAIT;
}

#undef F

M0_INTERNAL void
m0_ut_remach_heq_post(struct m0_dtm0_recovery_machine *m,
		      const struct m0_fid             *tgt_svc,
		      enum m0_ha_obj_state             state)
{
	struct recovery_fom *rf = recovery_fom_by_svc_find_lock(m, tgt_svc);
	M0_ASSERT_INFO(rf != NULL,
		       "Trying to post HA event to a wrong service?");
	heq_post(rf, state);
}

M0_INTERNAL void
m0_ut_remach_populate(struct m0_dtm0_recovery_machine *m,
		      struct m0_conf_process          *procs,
		      const struct m0_fid             *svcs,
		      bool                            *is_volatile,
		      uint64_t                         objs_nr)
{
	uint64_t i;

	for (i = 0; i < objs_nr; ++i)
		M0_ASSERT(recovery_fom_add(m, procs + i,
					   svcs + i, is_volatile[i]) == 0);
}

M0_INTERNAL void
m0_dtm0_recovery_machine_redo_post(struct m0_dtm0_recovery_machine *m,
				   struct dtm0_req_fop             *redo,
				   struct m0_be_op                 *op)
{
	bool                 is_eol =
		!!(redo->dtr_flags & M0_BITS(M0_DMF_EOL));
	bool                 is_eviction =
		!!(redo->dtr_flags & M0_BITS(M0_DMF_EVICTION));
	const struct m0_fid *initiator = &redo->dtr_initiator;
	struct eolq_item     item = {};
	struct recovery_fom *rf;

	M0_ENTRY("in-redo (m=%p): " REDO_F, m, REDO_P(redo));

	if (is_eol) {
		M0_ASSERT_INFO(!is_eviction,
			       "TODO: Eviction is not handled yet.");

		rf = recovery_fom_local(m);
		if (rf != NULL) {
			M0_ASSERT_INFO(equi(is_eviction, !rf->rf_is_local),
				       "Participant cannot evict itself.");
			item = (struct eolq_item) {
				.ei_type = EIT_EOL,
				.ei_source = *initiator,
			};
			m0_be_queue_lock(&rf->rf_eolq);
			M0_ASSERT_INFO(!rf->rf_eolq.bq_the_end,
				       "REDOs are not allowed if local recovery"
				       " has already been finished.");
			M0_BE_QUEUE_PUT(&rf->rf_eolq, op, &item);
			m0_be_queue_unlock(&rf->rf_eolq);
		} else {
			M0_LOG(M0_WARN,
			       "REDO received but svc is not RECOVERING yet");
			m0_be_op_active(op);
			m0_be_op_done(op);
		}
	} else {
		M0_LOG(M0_DEBUG, "A non-EOL REDO was ignored.");
		m0_be_op_active(op);
		m0_be_op_done(op);
	}

	M0_LEAVE();
}

static int default_log_iter_init(struct m0_dtm0_recovery_machine *m,
				 struct m0_be_dtm0_log_iter      *iter)
{
	m0_be_dtm0_log_iter_init(iter, m->rm_svc->dos_log);
	return 0;
}

static void default_log_iter_fini(struct m0_dtm0_recovery_machine *m,
				  struct m0_be_dtm0_log_iter      *iter)
{
	struct recovery_fom *rf = M0_AMB(rf, iter, rf_log_iter);
	M0_ASSERT(rf->rf_m == m);
	m0_be_dtm0_log_iter_fini(iter);
}

static bool participated(const struct m0_dtm0_log_rec *record,
			 const struct m0_fid          *svc)
{
	return m0_exists(i, record->dlr_txd.dtd_ps.dtp_nr,
			 m0_fid_eq(&record->dlr_txd.dtd_ps.dtp_pa[i].p_fid,
				   svc));
}

static int default_log_iter_next(struct m0_dtm0_recovery_machine *m,
				 struct m0_be_dtm0_log_iter      *iter,
				 const struct m0_fid             *tgt_svc,
				 const struct m0_fid             *origin_svc,
				 struct m0_dtm0_log_rec          *record)
{
	struct m0_be_dtm0_log *log = m->rm_svc->dos_log;
	int rc;

	/* XXX: not supported yet */
	M0_ASSERT(origin_svc == NULL);

	m0_mutex_lock(&log->dl_lock);

	/* Filter out records where tgt_svc is not a participant. */
	do {
		M0_SET0(record);
		rc = m0_be_dtm0_log_iter_next(iter, record);
		if (rc == +1) {
			if (participated(record, tgt_svc))
				break;
			else
				m0_dtm0_log_iter_rec_fini(record);
		}
	} while (rc == +1);

	m0_mutex_unlock(&log->dl_lock);

	/* XXX: error codes will be adjusted separately. */
	switch (rc) {
	case +1:
		return 0;
	case 0:
		return -ENOENT;
	default:
		return M0_ERR(rc);
	}
}

/*
 * TODO: It was copy-pasted from setup.c!
 * Export cs_ha_process_event instead of using this thing.
 */
static void cs_ha_process_event(struct m0_motr                *cctx,
                                enum m0_conf_ha_process_event  event)
{
	enum m0_conf_ha_process_type type;

	type = cctx->cc_mkfs ? M0_CONF_HA_PROCESS_M0MKFS :
			       M0_CONF_HA_PROCESS_M0D;
	if (cctx->cc_ha_is_started && !cctx->cc_no_conf &&
	    cctx->cc_motr_ha.mh_link != NULL) {
		m0_conf_ha_process_event_post(&cctx->cc_motr_ha.mh_ha,
		                              cctx->cc_motr_ha.mh_link,
		                              &cctx->cc_reqh_ctx.rc_fid,
		                              m0_process(), event, type);
	}
}

static void default_ha_event_post(struct m0_dtm0_recovery_machine *m,
				  const struct m0_fid             *tgt_proc,
				  const struct m0_fid             *tgt_svc,
				  enum m0_conf_ha_process_event    event)
{
	struct m0_reqh *reqh;
	(void) tgt_proc;
	(void) tgt_svc;

	if (ALL2ALL) {
		M0_LOG(M0_DEBUG, "ALL2ALL_DTM_RECOVERED");
	}

	M0_ASSERT_INFO(m->rm_local_rfom != NULL,
		       "It is impossible to emit an HA event without local "
		       "recovery FOM up and running.");
	reqh = m0_fom_reqh(&m->rm_local_rfom->rf_base);
	M0_ASSERT_INFO(m0_cs_reqh_context(reqh) != NULL,
		       "A fully-functional motr process must have a reqh ctx.");
	cs_ha_process_event(m0_cs_ctx_get(reqh), event);
}

static void default_redo_post(struct m0_dtm0_recovery_machine *m,
			      struct m0_fom                   *fom,
			      const struct m0_fid             *tgt_proc,
			      const struct m0_fid             *tgt_svc,
			      struct dtm0_req_fop             *redo,
			      struct m0_be_op                 *op)
{
	int rc;

	rc = m0_dtm0_req_post(m->rm_svc, op, redo, tgt_svc, fom, true);
	/*
	 * We assume "perfect" links between ONLINE/RECOVERING processes.
	 * If the link is not perfect then let's just kill the process
	 * that is not able to send out REDOs.
	 */
	M0_ASSERT(rc == 0);
}

static const struct m0_dtm0_recovery_machine_ops default_ops = {
	.log_iter_init = default_log_iter_init,
	.log_iter_fini = default_log_iter_fini,
	.log_iter_next = default_log_iter_next,

	.redo_post     = default_redo_post,
	.ha_event_post = default_ha_event_post,
};

#undef M0_TRACE_SUBSYSTEM
/*
 *  Local variables:
 *  c-indentation-style: "K&R"
 *  c-basic-offset: 8
 *  tab-width: 8
 *  fill-column: 80
 *  scroll-step: 1
 *  End:
 */
/*
 * vim: tabstop=8 shiftwidth=8 noexpandtab textwidth=80 nowrap
 */
