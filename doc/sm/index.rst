===========================
State machine documentation
===========================

:author: Nikita Danilov <nikita.danilov@seagate.com>
:state: INIT
:copyright: Seagate
:distribution: unlimited

:abstract: This document describes how and why state machines are used by motr.

Stakeholders
============

+----------+----------------------+----------------------------+----------------+
| alias    | full name            | email                      | rôle           |
+==========+======================+============================+================+
| nikita   | Nikita Danilov       | nikita.danilov@seagate.com | author,        |
|          |                      |                            | architect      |
+----------+----------------------+----------------------------+----------------+

Introduction
============

Non-blocking state machines are one of the most unusual parts motr. State
machine based programming model is not as widely known as stack based, this
introduces a barrier to entry and complicates understanding of existing motr
code based.

Let's start with the description of traditional programming model that can be
called *stack* or *thread* based.

Any function call like

.. highlight:: C
.. code-block:: C

    int foo(...) {
            ...
	    bar(...);
	    ...
    }

produces at run-time a new *stack frame*:

.. image:: call-stack.png

A sequence of these stack frames is called *a call stack* or simply a stack. In
a multi-threading environment one tends to identify a thread and a
stack. Technically, a thread is a set of processor registers. When operating
system together with the processor switch from one thread to another, all they
really do is saving of the current state of hardware registers and installing
the registers associated with the target thread. At any moment in time, there
are many threads, some running on available processors, some *blocked* (that is,
not running anywhere). In addition to the usual call stack (which is actually *a
user-space stack*), each thread also has *a kernel stack* that is used whenever
execution enters code mode for whatever reason.

.. image:: kernel-stack.png

Kernel stack is used when the thread enters kernel voluntarily (usually by
invoking a system call) or involuntarily (because of interrupt, signal, page
fault, *etc*.). The kernel stack exists even when the thread is running in user
space, but it is empty in this case.

When a thread is running in kernel mode, the kernel might make a decision to
block the thread. When a thread blocks, it stops running and another thread is
selected to run on the processor. Later the kernel will *resume* the blocked
thread and select it to run instead of another blocked thread.

Blocking can be voluntary or involuntary. A thread blocks voluntarily when it
cannot proceed further without waiting for some event, for example:

    - wait for completion of IO initiated by the thread,

    - wait until free memory is available,

    - wait until other process terminates (waitpid(2) system call).

Involuntary blocking happens, among other reasons, when:

    - there is a higher-priority thread that can be ran on the processor,

    - the thread exhausted its processor time quantum.

Thread-per-request
==================

Consider a server process, like motr m0d, that handles incoming requests from
clients across the network. To handle a request, the server, typically, takes
some clocks, initiates and waits for some storage and network IO, *etc*. The
simplest way to implement such a server is to create a dedicated thread for each
incoming request. In such server, under load there will be a large number of
threads (equal to the number of concurrently processed requests), executing
requests. Because memory, storage and network are significantly slower than
processor most of the request processing thread will be blocked waiting for some
form of IO to complete.

.. image:: thread-per-request.png

This model has a number of advantages. The most important is its simplicity:
dedicated per-request threads map well onto standard system abstractions such as
threads, standard synchronisation primitives (mutices, condition variables,
*etc*.) and simple blocking system calls like read and write. It also provides
reasonable performance, while concurrency levels are not too high.

As number of concurrent threads increases, the model starts breaking up. One
reason is immediately obvious from the description above. At high concurrency
levels, the kernel has to schedule a large number of threads (in tens of
thousands) over a much smaller number of processors (less than
hundred). Scheduling decisions impact user-visible performance characteristics,
such as request processing latency. Which data are available for the kernel to
make informed scheduling decisions? The kernel does not know semantics of the
requests, because this semantics is in user-space code, which is opaque to the
kernel. The scheduler uses information about cpu consumption and IO frequency by
the thread, but for the short-living per-request threads this is not a good
predictor of the future thread behaviour.

To understand another class of issues with thread-per-request scalability,
recall that on a multi-processor machine a memory access from one processor to a
word cached on another processor incurs the cost of a memory coherency protocol,
that includes multiple cross-processor bus round-trips. A result of this is that
sub-optimal thread migrations between processors and sub-optimal interleaving
executions of threads accessing shared data can significantly affect system
performance.

Finally, most modern servers are NUMA under the hood: the cost of memory access
depends on which part of the memory is addressed. Each processor has some
*local* memory, which can be accessed much faster than *remote* memory (which is
local to other processors). Again, the kernel scheduler has no information that
would allow it to pair threads and memory areas.

The underlying reason for the problems with thread-per-request is that the
kernel is not aware of the semantics of request processing. While it is possible
to imagine an extended interface that the server can use to affect decisions of
the kernel scheduler, such interface would be prohibitively complex and
non-portable.

Localities
==========

Instead motr chose to implement its own user-space scheduler that would be able
to execute a large number of concurrent requests efficiently. Once this decision
is made, it is easy to realise that there is no need for this scheduler to
schedule threads, it can directly schedule requests.

Enter the *locality architecture*.

*A locality* consists of:

    - a thread, called locality *handler thread*,

    - a list of requests ready for execution (*run list*),

    - a list of requests waiting for some event to happen (*wait list*).

The handler thread executes the following loop (more details will be filled
later):

.. highlight:: C
.. code-block:: C

    int handler(struct locality *loc) {
            lock(&loc->l_lock);
	    while (true) {
	            while (!empty(&loc->run_list)) {
		            fom = head(&loc->run_list);
			    exec(fom);
			    if (blocked(fom))
			            add_tail(&loc->wait_list, fom);
		            else
			            add_tail(&loc->run_list, fom);
		    }
		    wait_not_empty(&loc->run_list);
	    }
            unlock(&loc->l_lock);
    }

Locality handler takes a fom from the ready queue.

.. image:: locality-get.png

It tries to advance the request processing. Let's say this requires reading
something from the storage. Request processing function will initiate
asynchronous storage IO.

.. image:: locality-exec.png

Handler places the request on the wait list and takes the next request from the
ready list for execution.

.. image:: locality-block.png

When the event for which a request is waiting occurs (for example, previously
launched IO operation completes), the request is moved from the wait list to the
ready list.

.. image:: locality-wakeup.png

Few immediate comments:

    - this loop is (of course) very similar to a prototypical kernel scheduling
      loop: maintain a list of threads ready for execution and a list of blocked
      threads; take a ready thread; execute it until it blocks. But instead of
      threads, locality handler schedules requests;

    - all locality data-structures are protected by a single per-locality lock;

    - execution of requests within locality is serialised.

motr creates a separate locality for each processor (cpu core) used by the motr
process. Each locality has its own wait and ready lists. An incoming request is
associated with a certain locality. Memory, necessary for request processing, is
allocated locally (NUMA-wise) to the request locality.

.. image:: locality-overall.png

This architecture addresses the issues mentioned above:

    - it uses only a small number of operating system threads (1 thread per
      core) and these threads are permanently bound to their cores. This
      minimises the amount of guessing that the kernel scheduler has to do;

    - locality handler can inspect request objects and schedule them optimally;

    - memory can be allocated locally;

    - programming model is simplified by avoiding any concurrency within a
      locality.

It is clear that locality model can be efficient only if handler threads never
block. Indeed, if a handler thread blocks, no request processing will be done by
the locality core, until the handler thread unblocks.

Locality infrastructure takes care to avoid involuntary blocking (due to
preemption, for example). The request processing code should be structured in a
way that avoids voluntary blocking. This is achieved by representing request
execution as *a non-blocking state machine*, called *fom* (which stands for "FOp
Machine"). fom structures request processing as a collection of *phases*,
starting with the initial phase. When a particular phase is reached, some *phase
transition* code, associated with the phase is executed. Phase transition code
is non-blocking, which means it cannot execute potentially blocking system calls
or voluntarily block in any other way. When execution of phase transition code
terminates, the fom transitions to the next phase (as determined by the result
of phase transition). This next phase can be reachable immediately (fom remains
on the ready list), or after some event happens. In the latter case fom is
parked on the wait list and will be moved to the ready list by the specified
wakeup call-back.

Here is a simplified example of a fom phase transition diagram.

.. image:: phase-diagram.png

The blue arrows are blocking phase transitions.

Actual phase transition diagrams are much more complex. Take the diagram of cas
fom as an example.
      
.. image:: cas.png

State machine programming
=========================

State machine module (`sm/ directory
<https://github.com/Seagate/cortx-motr/tree/main/sm>`_) and fom (`fop/fom.h
<https://github.com/Seagate/cortx-motr/tree/main/fop/fom.h>`_) provide support
for non-blocking fom implementation. Phase transition code is kept in a *tick
function*. Return value of this function determines whether fom goes to the
ready or wait list.



===
AST
===

The discussion above glossed over fom wakeups. Suppose a fom is parked on the
locality wait list, waiting on some event. This event will typically happen
asynchronously with the handler thread execution:

    - if the event is timer expiration, timer call-back will be invoked as a
      signal handler (maybe on the handler thread stack, maybe in some other
      thread);

    - if the event is storage IO completion, completion call-back will be
      invoked by an IO thread;

    - if the event is a network message receipts, notification will be invoked
      on the stack of network management thread, and so on.

In any case, the fom has to be moved from the wait list to the ready list. The
problem is that because these lists are protected by the locality lock, which is
always held by the handler thread, it is unsafe to modify these lists outside of
the handler thread loop.

This, again, is a typical problem that must be solved by an operating system
kernel. For example, a file descriptor must somehow be marked readable when data
arrive with an interrupt. motr uses a method called *fork queue* from DEC
operating systems. The idea is that to modify some per-locality data-structure
from outside of the locality lock or, more generally, to execute some code under
the locality lock, a special data-structure called ast (*Asynchronous System
Trap*) is created. An ast contains the pointer to a function to be executed
within locality lock. Asts are placed on a per-locality list (called *fork
queue*) and locality handled thread periodically checks this list and executes
all asts on it. This of course begs the question: how to place an ast on the
fork queue list protected by the locality lock? Fortunately, there are lockless
lists that do not require locking. All together, fom wakeup looks like this:

.. highlight:: C
.. code-block:: C

    void m0_fom_wakeup(struct m0_fom *fom) {
            fom->f_ast.sa_cb = &readyit;
	    /* Magic function that does not require locality lock. */
            m0_sm_ast_post(fom->f_locality, &fom->f_ast);
    }

    static void readyit(...) {
            wait_list_del(fom);
	    run_list_add(fom);
    }

    int handler(struct locality *loc) {
            lock(&loc->l_lock);
	    while (true) {
	            while (!empty(&loc->run_list)) {
		            fom = head(&loc->run_list);
			    exec(fom);
			    if (blocked(fom))
			            add_tail(&loc->wait_list, fom);
		            else
			            add_tail(&loc->run_list, fom);
		    }
		    while (!empty(&loc->l_ast)) {    /* New code... */
		            ast = head(&loc->l_ast); /* Runs all pending ASTs... */
			    ast->sa_cb(...);         /* ... under locality lock. */
		    }
		    wait_not_empty(&loc->run_list);
	    }
            unlock(&loc->l_lock);
    }

    /** Lockless list addition. */
    void m0_sm_ast_post(struct m0_sm_group *grp, struct m0_sm_ast *ast) {
            do {
                    ast->sa_next = grp->s_forkq;
            } while (!compare_and_swap(&grp->s_forkq, ast->sa_next, ast));
            m0_clink_signal(&grp->s_clink);
    }

..  LocalWords:   waitpid mutices
