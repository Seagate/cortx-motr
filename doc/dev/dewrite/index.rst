==============
Degraded write
==============

:author: Nikita Danilov <nikita.danilov@seagate.com>
:state: ANALYSIS
:copyright: Seagate
:distribution: unlimited
:address: https://github.com/Seagate/cortx-motr/blob/documentation/doc/dev/dewrite/index.rst

:abstract: This document describes development of degraded write support in motr.

Stakeholders
============

.. list-table:: 
   :header-rows: 1

   * - alias
     - email
     - rôle

   * - andriy
     - andriy.tkachuk@seagate.com
     - 

   * - nikita
     - nikita.danilov@seagate.com
     - motr architect

   * - sining
     - sining.wu@seagate.com
     - 

   * - ?
     - ?
     - QA contact

..
   Overview
   ========

   motr and, more generally, CORTX is deployed as a collection of processes running
   on multiple nodes in a cluster. Wihin each process there is a number of
   sub-system interacting with each other, other processes, network and
   storage. Sub-systems create and maintain state in form of structures in volatile
   memory and on persistent store. State is accessed concurrently from multiple
   threads.

   Development is any modification of the Project, which is complex enough to
   warrant tracking its internal states. For example, elimination of the spelling
   errors within a documentation file is too simple to be covered by the processes
   described in this document, whereas development of a new major feature is not.

   Examples of types of development are:

   * new feature;
   * bug fix;
   * technical debt elimination;
   * documentation creation or update;
   * refactoring.

   Process
   =======

   The overall development process structure is the following:

Initiation (INIT)
=================

..
   The modification is proposed. The origin of modification request can be:

     - marketing or sales;
     - feature request from a user (internal or external to Seagate);
     - bug report;
     - report of a defect in or an inconsistency between process, architecture,
       design, code, documentation, tests, *etc*.;
     - change in requirements;
     - change in timelines, deadlines, available development resources or
       schedules;

   At the initiation state, the modification can be described imprecisely or
   indirectly. For example, a bug report "the system crashes while executing
   operation X in environment Y" is implicitly a request to "modify the system so
   that it doesn't crash while executing the operation X in environment Y".

   The modification is always associated with a group of *initiators*. As the
   outcome of initiation state, an *owner* is assigned to the modification.

   **Owner assignment process**: to be defined. Depends on the modification type.

The purpose of this development is to add degraded write support to cortx IO
stack. This development is part of Data-Path Stabilisation (DPS).

The first DPS milestone is the support for a `single process restart
<https://seagate-systems.atlassian.net/wiki/spaces/PRIVATECOR/pages/962691111/single-process-restart>`_. While
a process is unavailable, client IO should continue in degraded mode. (There
will be a separate development for degraded read.)

.. note:: Put in the reference to the degraded write tracking file, when it is
          available.

After the process re-joins the cluster, writes resume in normal mode, but reads
continue in degraded mode.

Clarification (CLARIFY)
=======================

..
   At this state, the scope and intent of the modification are clarified between
   the initiators and the owner. This is an iterative process, that completes when
   the owner has enough data to start analysis. The data include descriptions of
   features, informal requirements, informal use cases, bug reproducibility
   conditions, *etc*.

Some definitions:

read
  a motr client object read operation. This operation reads a number of
  extents in the object and places the extents contents in the client-supplied
  buffers. Objects are read in units of blocks.

write
  a motr client object read operation. This operation takes data from
  client-supplied buffers and places them in a number of extents in the
  object. Objects are written in units of blocks.

normal mode, normal operation
  non-degraded mode, non-degraded operation. Normal
  read fetches data units from the object and places them in the buffers. Normal
  write either updates data and parity units (in full-group write case), or
  involves read-modify-write (q.v.) in case of partial group write.

degraded mode, degraded operation
  degraded mode is used from normal mode cannot complete.

  Degraded write omits writing the unavailable units. In read-modify-write case,
  degraded write additionally avoid reading the unavailable units.

  Degraded read, fetches the parity units and reconstructs unavailable units.

The scope of degraded-write development is defined by the following informal
requirements (refined in Requirements section):

* [**r.dewrite.trigger.ha**] degraded mode is triggered by HA. When it is known
  in advance (from the HA state notifications), that certain units will be
  unavailable, the client should write in degraded mode;

* [**r.dewrite.trigger.timeout**] degraded mode is triggered by timeouts. If,
  while an operation is executing, certain operations (writes or reads) fail to
  complete within a certain timeout, the operation should be continued in
  degraded mode with the assumption that timed out units are unavailable. This
  should happen both in the case when original operation execution was in normal
  mode [normal -> degraded transition] and in the case when original operation
  execution was already degraded for whatever reason [degraded -> more degraded
  transition];

* [**r.dewrite.trigger.error**] similarly to timeouts, errors during unit reads
  or writes, that can be reasonably attributed to specific units, should
  transition operation execution to degraded mode. Possible errors are: out of
  memory condition in the receiving process, spurious io error (bad block), data
  integrity checksum mismatch, *etc*.;

* [**r.dewrite.report**] a client should report to HA all errors and timeouts
  used in the decision to transition to degraded mode;

* [**r.dewrite.stick**] if a client decides to transition to degraded mode
  because of a failure to do operations with a particular device (or node,
  *etc*.) the client, if necessary, should stick to this decision for some time
  and pre-suppose in advance that the units located on the device will be
  unavailable. *E.g.*, if a server is out of memory, it will remain out of
  memory for some time. On the other hand, a bad block on a particular device
  does not indicate that following operations with this device will
  fail. Perhaps a hint returned from the receiving process is needed;

Analysis (ANALYSIS)
===================

..
   The modification is analysed in terms of the Project software structure. An
   analysis produces:

   * a list of software components that have to be changed,
   * a high level description of changes, their intent, scope and interaction.

   At this point it is decided whether the modification falls under the development
   process described in this document. If it does, a unique meaningful *name* is
   assigned to it and a development *tracking file* is created in doc/dev/.

   The list of *stakeholders* is defined at this point and recorded in the tracking
   file. Stakeholders are peoples or groups interested in this development, their
   consent is required for state transitions of the development process. A
   stakeholder has a rôle (or rôles) with the development, for example,
   "architect", "designer", *etc*. The list of rôles and their responsibilities in
   the process is described **elsewhere**.

   All decisions, problems and artefacts associated with the development are
   recorded in the tracking file.

Some of the required functionality is already implemented. Motr supports
degraded IO in case of device failure. (What is exactly implemented should be
clarified.)

The bulk of additional functionality is in client IO path (and also, perhaps, a
bit on the server side, see [**r.dewrite.stick**]).

The test plan has to cover important use cases: combinations of HA events,
timeouts, errors together with full and partial group writes.

One possibility is to transition to degraded mode by abandoning the current
operation execution and starting it anew with the assumption that some
additional units are unavailable. This can be sub-optimal (already executed part
of the operation is re-executed), but simpler.

Requirements (REQS)
===================

..
   The formal list of requirements is defined and recorded in the tracking
   file. This list is formed and maintained according to the *requirements tracking
   process* (defined elsewhere). Requirements are used to systematically find
   dependencies or inconsistencies between the developments and the existing code
   base.

Blah...

**Basic operational requirements**

.. list-table:: 
   :widths: 10 80 10
   :header-rows: 1

   * - label
     - description
     - source

   * - [**r.dewrite**]
     - ...
     - [r.dewrite.]


**Performance related requirements**
     
.. list-table:: 
   :widths: 10 80 10
   :header-rows: 1

   * - label
     - description
     - source

   * - [**r.dewrite**]
     - ...
     - [r.dewrite.]

**Fault-tolerance related requirements**

.. list-table::
   :widths: 10 80 10
   :header-rows: 1

   * - label
     - description
     - source

   * - [**r.dewrite**]
     - ...
     - [r.dewrite.]

Architecture (ARCH)
===================

..
   If the analysis (or any other) stage determines that changes to the Project
   architecture are needed, the *architecture modification process* is
   invoked. This process determines which parts of the architecture need to be
   altered, added or removed; develops a version of the architecture including this
   modification and checks it for consistency.

   If changes to the architecture are needed, the designs (high and low level),
   code and documentation that have to be changed (to reflect changes in the
   architecture) are identified and listed in the tracking file.

   If changes to the architecture change assumptions about external dependencies
   (software, hardware and environment), these changes in assumptions are
   identified and listed in the tracking file.

   All changes to the internal and external entities have to be discussed with and
   agreed by the appropriate stakeholders. The outcomes of these discussions are
   recorded in the tracking file.

   The outputs of the architecture stage:

   * agreed modifications to the architecture (both as a new architecture document
     and as a "delta");

   * agreed modifications to the assumptions about external dependencies.

Blah...

Planning (PLAN)
===============

..
   During the planning phase, the development is sub-divided into a list of
   development *tasks*. Examples of tasks are:

   * detailed-level design inspection;
   * integration of the system tests for the new feature.

   Each task is assigned a meaningful name unique within the development. If
   necessary, a tracking file doc/dev/development.task is created to record
   progress of the task execution, otherwise task progress is recorded within the
   development tracking file.

   The outcome of planning, recorded in the tracking file, is:

   * a list of development tasks,
   * dependencies between tasks,
   * an integration plan, which specifies how the modifications will be merged in
     the Project,
   * QA plan, which specifies how the QA team will test the tasks,
   * deployment plan, which specifies how tasks are deployed in the field,
   * estimates for task phases (development, test, integration, QA and deployment),
   * assignment of task phase responsibilities to developers, architects and
     managers,
   * an execution schedule

Test:

- degraded write with s3

- degraded write with m0crate


Blah...

.. list-table::
   :widths: 10 80 10
   :header-rows: 1

   * - 
     - 
     - 

   * - 
     - 
     - 

Task dependencies
~~~~~~~~~~~~~~~~~

.. graphviz::

   digraph foo {
   }

Estimation
~~~~~~~~~~

.. list-table::
   :header-rows: 1

   * - task
     - subtask
     - owners
     - LOC/pages
     - effort (PD)
     - calendar (PD)

   * - 
     - 
     - 
     - 
     - 
     - 

Execution (EXEC)
================

..
   During execution phase, development tasks are executed concurrently, according
   to the task execution process specified below.

   Periodically, development stakeholders perform a *checkpoint* to assess
   alignment with the planned schedule and necessary adjustments to the tasks.

   A task execution process can got *stuck* at any stage. This happens when further
   task execution is impossible for any reason, for example:

   * during task requirement collection or design phase it becomes clear that the
     task would take significantly more effort to complete than originally
     estimated;

   * high or detailed level design uncovers an inconsistency in other design or
     architecture;

   * change in requirements requires significant change to task designs or code.

   When a task is stuck, and this cannot be fixed at the checkpoint level, the
   development process is reset to an earlier stage, *e.g.*, architecture,
   requirements or planning, to address the issue with the task.

   Task execution process for a typical task is the following.

Meetings
~~~~~~~~

.. list-table::
   :header-rows: 1

   * - date
     - type
     - participants
     - agenda
     - summary
     - action items
     - attachments

   * - 2021.04.11
     - sync up
     - andriy, nikita, sining
     - 
     - go through the high level requirements. Is degraded read-modify-write
       necessary? Do we have system tests for degraded mode?
     - andriy, sining: understand current state of the code.
     - 

Detailed-level design (DLD)
~~~~~~~~~~~~~~~~~~~~~~~~~~~

..
   A detailed-level design is created as a set of skeleton source files with embedded
   documentation (for C, Doxygen-formatted comments are used).

   A DLD describes

   * data-structures,
   * programming interfaces,
   * functions,
   * concurrency,
   * scope and ownership data objects,
   * data and control flow,
   * deployment procedures (install, upgrade, downgrade, removal, monitoring,
     logging, error reporting in the field, *etc*.).

   A DLD contains enough detail to start coding. A DLD contains a refinement of the
   testing and integration plans from the HLD.

Detailed-level design intermediate review (DLDIR)
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

..
   Creation of a complex DLD can be periodically informally reviewed (**by ...**)
   to assure that it goes in the right direction.

Detailed-level design inspection (DLDINSP)
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

..
   A DLD is inspected. Inspection comments are recorded. The DLD authors discuss
   and address the comments. The DLD is updated. The inspection process is repeated
   until all found issues are addressed.

   **Question**: how and where DLD inspection comments are recorded?

   **Proposal**: [nikita]: an inspection round is recorded as a separate commit,
    with questions directly embedded in the DLD. git diff will show the
    context. The answers and requests for clarification are added as a next
    commit. Then another next commit contains the new version of the DLD, with
    comments and answers removed (but preserved in the repository history).

Code (CODE)
~~~~~~~~~~~

..
   Coding populates the set of skeleton source files, created at the DLD stage with
   the implementation conforming to the design. The code contains the set of tests,
   according to the testing plan specified in the designs.

   At the completion of the code phase, the design is implemented to the
   satisfaction of the inspectors.

Code intermediate review (CODEIR)
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

..
   A long code phase can be periodically informally reviewed (**by ...**) to assure
   that it goes in the right direction.

Dev testing (TEST)
~~~~~~~~~~~~~~~~~~

..
   Developers execute tests, created as part of the code phase, and fix all the
   test failures.

   Test runs and failure analyses are recorded in the tracking file (either
   directly or by reference to a testing platform (jenkins, ci, *etc*.)).

Code inspection (CODEINSP)
~~~~~~~~~~~~~~~~~~~~~~~~~~

..
   Code is inspected. Inspection comments are recorded. Code authors discuss and
   address the comments. Code is updated. The inspection process is repeated until
   all found issues are addressed.

Documentation (DOC)
~~~~~~~~~~~~~~~~~~~

..
   Necessary documentation is created, reviewed and inspected concurrently with the
   designs and code.

Integration (INT)
~~~~~~~~~~~~~~~~~

..
   Integration happens according to the integration plan developed at the planning
   phase and refined (for this task) at the design and coding phases.

   Integration includes execution of integration and end-to-end tests involving the
   task.

   Integration completes with landing the designs and the code onto the appropriate
   branch.

   A group of tasks can be integrated together (this should be specified in the
   integration plan).

QA testing (QA)
~~~~~~~~~~~~~~~

..
   QA team tests the landed tasks according to the planned QA plan.

   A group of tasks can be QAed together (this should be specified in the
   QA plan).

Deployment (DEPLOY)
~~~~~~~~~~~~~~~~~~~

..
   Once the task has been tested by QA it can be deployed in the field (as part of
   a product). Deployment phase includes necessary modifications to the product
   packages, manifests, BOMs, *etc., installation procedures and responding to
   customer bug reports related to the task.

   A group of tasks can be deployed together (this should be specified in the
   deployment plan).

Patents (PATENTS)
~~~~~~~~~~~~~~~~~

..
   If any, IP disclosures are filed concurrently with the other task execution
   stages.

Abandoned (ABANDON)
~~~~~~~~~~~~~~~~~~~

..
   A development can be retired when no longer needed. Its tracking file and
   artefacts are preserved.

..
   Pseudo-code
   ===========

   The development process can be represented by the following pseudo-code:

   .. highlight:: C
   .. code-block:: C

      development(input) {
	   do {
		   input = clarify(input);
	   } while (!clarified);
	   development = analysis(input);
	   development.reqs = requirements(development);
	   arch = architecture(development, arch);
	   plan = planning(development);
	   for (task in plan) {
		   task_process(task) &;
	   }
      }

   .. highlight:: C
   .. code-block:: C

      task_process(task) {
	   task.reqs = task_requirements(task.development, task);
	   document(task) &;
	   patent(task) &;
	   do {
	      do {
		 task.hld = hld(task);
		 task.hld = hldir(task.hld);
	      } while (!complete(task.hld));
	      issues = hldinsp(task.hld);
	   } while (issues != nil);
	   do {
	      do {
		 task.dld = dld(task);
		 task.dld = dldir(task.dld);
	      } while (!complete(task.dld));
	      issues = dldinsp(task.dld);
	   } while (issues != nil);
	   do {
	      do {
		 task.code = code(task);
		 task.code = codeir(task.code);
	      } while (!complete(task.code));
	      task.code = devtests(task);
	      issues = codeinsp(task.code);
	   } while (issues != nil);
	   integration(task);
	   qa(task);
	   deploy(task);
      }


   Notes
   =====

   * This document is itself managed by the process it describes.

   * This development process can be adjusted as needed. States can be omitted,
     added, re-ordered, as necessary by the agreement of the stakeholders.

   * The development owner and the list of development stakeholders can be changed
     during development to accommodate for changes in circumstances or additional
     information.

   * Artefacts, created as part of this process (tracking files, design documents,
     and so on) are kept under version control in the Project repository. If
     possible, they are formatted as reStructured text files pre-processed by the
     Project build system with a common set of m4 macros (as this file is). If this
     format is not suitable, the artefacts should be in a format that allows easy
     search, meaningful version control and links to particular items within a
     document. Artefacts should be in the English language and follow standard
     conventions of the Project: British spelling, no Oxford comma, *etc*. (see
     doc/coding-style.md).

Literature
==========

