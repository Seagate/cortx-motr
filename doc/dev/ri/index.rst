====================
Run-time instruments
====================

:author: Nikita Danilov <nikita.danilov@seagate.com>
:state: CLARIFY
:copyright: Seagate
:distribution: unlimited

:abstract: This document describes run-time instruments for Motr (née Eos core,
	 née Colibri), hereunder referred to as "the Project" (TP).

Stakeholders
============

.. list-table::
   :header-rows: 1

   * - alias
     - full name
     - email
     - rôle
   * - nikita
     - Nikita Danilov
     - nikita.danilov@seagate.com
     - author, architect

Initiation (INIT)
-----------------

It is proposed to add to TP a mechanism for run-time inspection and modification
of in-memory and persistent state. This mechanism will be used by developers to
analyse bugs and to fix corrupted system state during experiments. The same
mechanism will be used in the field by support personnel or on-site developers
to analyse problems with the system (crashes, performance issues, *etc*.) and to
restore system in case of a catastrophic failure.


Clarification (CLARIFY)
-----------------------

.. note::
    At this state, the scope and intent of the modification are clarified
    between the initiators and the owner. This is an iterative process, that
    completes when the owner has enough data to start analysis. The data include
    descriptions of features, informal requirements, informal use cases, bug
    reproducibility conditions, *etc*.

TP and, more generally, CORTX is deployed as a collection of processes running
on multiple nodes in a cluster. Wihin each process there is a number of
sub-system interacting with each other, other processes, network and
storage. Sub-systems create and maintain state in the form of structures in
volatile memory and on persistent store. State is accessed concurrently from
multiple threads.

Analysis of a problem in such distributed and concurrent system is difficult. TP
already provides some mechanisms to help with analysis:

- tracing. lib/trace.h provides a fast tracing module that produces logs
  (m0trace.$PID files) with the record of last few seconds of system activity.
  Tracing is useful for debugging, but it does not allow examination or
  modification of system state;

- addb contains records describing system behaviour. These records are
  cross-referenced in a way that allows tracking execution of top-level request
  (*e.g.*, S3 GET request) through multiple nodes and multiple layers. addb is
  useful for large-scale analysis of system behaviour (performance, workload
  characteristics, efficiency of algorithms, *etc*.).

Neither of these mechanisms is suitable for interactive inspection of
modification of system in a development or customer environment.

The only available mechanisms to fix problems in a deployed system are total
re-initialisation (mkfs with loss of all data) and recovery tool (beck,
extremely heavyweight tool of last resort).

An additional mechanism, called RI (*run-time instruments*) will be introduced
that would allow developers and support people to inspect and fix a system
on-line without bringing it down.

RI would allow its user to do, for example, the following on a live system:

- map S3 object name to motr fid;

- list component objects (cobs) for a particular s3 object;

- map a cob to a server and a device on this server;

- find device blocks allocated to a cob;

- stop processing of all incoming requests in a given motr server process;

- print out the contents of a particular b-tree node;

- update a meta-data record transactionally;

- reset network connection between a given client and a given server;

- repair a particular block in a particular object from parity;

- list all requests queued in the request handler.

RI will be usable in the following situations:

- a developer wants to analyse a crash or experiment with a live system in the
  lab;

- a system fails on a customer site. A support person analyses the live system
  and tries to fix it.

RI should provide multiple access interfaces:

- interactive connection to a local TP process (client or server);

- interactive connection to a remote TP process;

- scriptable access to a local or remote processes.

Analysis (ANALYSIS)
-------------------

.. note::
    The modification is analysed in terms of the Project software structure. An
    analysis produces:

    * a list of software components that have to be changed,
    * a high level description of changes, their intent, scope and interaction.

    At this point it is decided whether the modification falls under the
    development process described in this document. If it does, a unique
    meaningful *name* is assigned to it and a development *tracking file* is
    created in doc/dev/.

    The list of *stakeholders* is defined at this point and recorded in the
    tracking file. Stakeholders are peoples or groups interested in this
    development, their consent is required for state transitions of the
    development process. A stakeholder has a rôle (or rôles) with the
    development, for example, "architect", "designer", *etc*. The list of rôles
    and their responsibilities in the process is described **elsewhere**.

    All decisions, problems and artefacts associated with the development are
    recorded in the tracking file.

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

   Requirements (REQS)
   -------------------

   The formal list of requirements is defined and recorded in the tracking
   file. This list is formed and maintained according to the *requirements tracking
   process* (defined elsewhere). Requirements are used to systematically find
   dependencies or inconsistencies between the developments and the existing code
   base.

   Architecture (ARCH)
   -------------------

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

   Planning (PLAN)
   ---------------

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

   Execution (EXEC)
   ----------------

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

   Task requirements (TREQ)
   ++++++++++++++++++++++++

   Task requirements are extracted from the development requirements and refined.

   High-level design (HLD)
   +++++++++++++++++++++++

   A high-level design is created and recorded in doc/hld. A link to the HLD file
   is placed in the tracking file.

   The resulting HLD should contain enough information to start the DLD. An HLD
   contains, among other things, functional and logical specification of the task,
   *testing*, *integration* and *deployment* plans (see
   doc/hld/hld-template.rst.in).

   High-level design intermediate review (HLDIR)
   +++++++++++++++++++++++++++++++++++++++++++++

   Creation of a complex HLD can be periodically informally reviewed (**by ...**)
   to assure that it goes in the right direction.

   High-level design inspection (HLDINSP)
   ++++++++++++++++++++++++++++++++++++++

   An HLD is inspected. Inspection comments are recorded. The HLD authors discuss
   and address the comments. The HLD is updated. The inspection process is repeated
   until all found issues are addressed.

   Detailed-level design (DLD)
   +++++++++++++++++++++++++++

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
   +++++++++++++++++++++++++++++++++++++++++++++++++

   Creation of a complex DLD can be periodically informally reviewed (**by ...**)
   to assure that it goes in the right direction.

   Detailed-level design inspection (DLDINSP)
   ++++++++++++++++++++++++++++++++++++++++++

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
   +++++++++++

   Coding populates the set of skeleton source files, created at the DLD stage with
   the implementation conforming to the design. The code contains the set of tests,
   according to the testing plan specified in the designs.

   At the completion of the code phase, the design is implemented to the
   satisfaction of the inspectors.

   Code intermediate review (CODEIR)
   +++++++++++++++++++++++++++++++++

   A long code phase can be periodically informally reviewed (**by ...**) to assure
   that it goes in the right direction.

   Dev testing (TEST)
   ++++++++++++++++++

   Developers execute tests, created as part of the code phase, and fix all the
   test failures.

   Test runs and failure analyses are recorded in the tracking file (either
   directly or by reference to a testing platform (jenkins, ci, *etc*.)).

   Code inspection (CODEINSP)
   ++++++++++++++++++++++++++

   Code is inspected. Inspection comments are recorded. Code authors discuss and
   address the comments. Code is updated. The inspection process is repeated until
   all found issues are addressed.

   Documentation (DOC)
   +++++++++++++++++++

   Necessary documentation is created, reviewed and inspected concurrently with the
   designs and code.

   Integration (INT)
   +++++++++++++++++

   Integration happens according to the integration plan developed at the planning
   phase and refined (for this task) at the design and coding phases.

   Integration includes execution of integration and end-to-end tests involving the
   task.

   Integration completes with landing the designs and the code onto the appropriate
   branch.

   A group of tasks can be integrated together (this should be specified in the
   integration plan).

   QA testing (QA)
   +++++++++++++++

   QA team tests the landed tasks according to the planned QA plan.

   A group of tasks can be QAed together (this should be specified in the
   QA plan).

   Deployment (DEPLOY)
   +++++++++++++++++++

   Once the task has been tested by QA it can be deployed in the field (as part of
   a product). Deployment phase includes necessary modifications to the product
   packages, manifests, BOMs, *etc*., installation procedures and responding to
   customer bug reports related to the task.

   A group of tasks can be deployed together (this should be specified in the
   deployment plan).

   Patents (PATENTS)
   +++++++++++++++++

   If any, IP disclosures are filed concurrently with the other task execution
   stages.

   Abandoned (ABANDON)
   -------------------

   A development can be retired when no longer needed. Its tracking file and
   artefacts are preserved.

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

