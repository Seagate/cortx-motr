/* -*- C -*- */
/*
 * Copyright (c) 2016-2021 Seagate Technology LLC and/or its Affiliates
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
 * Original author: Nikita Danilov <nikita.danilov@seagate.com>
 * Original creation date: 07-Jun-2016
 */

#pragma once

#ifndef __MOTR_LAYOUT_PLAN_H__
#define __MOTR_LAYOUT_PLAN_H__

/**
 * @defgroup layout
 *
 * Layout access plan, theory of operation
 * =======================================
 *
 * Overview
 * --------
 *
 * Layout access plan is an abstraction, which describes how a particular
 * operation is to be executed on an object or index.
 *
 * An access plan can be thought of as a directed graph, where vertices are
 * "plan operations" (plops) and arcs correspond to dependencies between plops.
 *
 * Take a network parity de-clustered layout as an example. To read an extent
 * from an object with this layout, one has to read data from the cobs of the
 * object. The plan for the object read would, therefore, contain a plop for
 * each cob read operation, each having an outgoing arc to the "operation
 * complete" plop.
 *
 * In case of a write to an object with parity de-clustered layout, the plan
 * contains additional plops corresponding to parity calculations. In case of
 * partial group write, the plan, in addition, contains read-plops describing
 * the logic of read-modify-write cycle. Degraded mode introduces more plops and
 * so on.
 *
 * Access plans are an intermediate abstraction designed for two main users:
 *
 *   - IO code in client API, including key-value indices and data objects, and
 *   - in-storage compute (ISC, aka Function Shipping).
 *
 * Interface
 * ---------
 *
 * In the following, "implementation" means generic layout code and particular
 * layout type code, while "user" denotes a caller of the layout plan interface
 * (e.g., IO code).
 *
 * An access plan is represented by m0_layout_plan.
 *
 * The directed graph is not presented as a first-class entity. Instead, a plan
 * produces, via calls to m0_layout_plan_get(), plops as they become "ready",
 * in the sense that all their dependencies are satisfied.
 *
 * Interaction between the implementation and the user takes form of the loop
 * (which is, basically, a graph traversal):
 *
 *   - the user calls m0_layout_plan_get() to obtain a ready plop. The
 *     implementation returns a plop, represented by a "sub-class" of
 *     m0_layout_plop. Some plop fields are set up by the implementation
 *     before the plop is handed to the user;
 *
 *   - the user processes the plop in the appropriate manner, e.g., executes
 *     read or write operation (which, in turn, might involve layout plans)
 *     and sets plop fields accordingly to the results of the execution (e.g.,
 *     points plop to the data read from a cob);
 *
 *   - the user calls m0_layout_plop_done() to signal the implementation that
 *     the plop has been executed. This might make more plops ready, so the
 *     loop repeats.
 *
 * All plops should be executed and m0_layout_plop_done() should be called
 * on them by user in the depencency order indicated by the
 * m0_layout_plop::pl_deps list. Before starting the plop execution user should
 * call m0_layout_plop_start() which would verify whether the dependencies are
 * met and the plop is still actual (i.e. that it was not cancelled already).
 *
 * A plop is not necessarily immediately destroyed by the implementation after
 * the user completes it. The implementation might keep plop alive for some
 * time, for example, data read from a cob might be needed for a future parity
 * calculation. When the implementation is done with the plop, it invokes
 * m0_layout_plop_ops::po_fini() call-back that the user might optionally set
 * before calling m0_layout_plop_done(). Between the call to
 * m0_layout_plop_done() and invocation of ->po_fini() call-back the plop (and
 * the user data buffers referenced by the plop fields) are shared between the
 * user and the application and the user should neither modify nor free the plop
 * or its data buffers.
 *
 * As an optimisation, implementation deterministically colours plops, in such a
 * way that a user can improve locality of reference by assigning colours to
 * processor cores. Colouring is optional and does not affect correctness.
 *
 * Let's consider, for example, the i/o read operation. The user calls
 * m0_layout_plan_build() passing m0_op as argument describing the operation.
 * After this we can enter the "loop" traversing the graph. m0_layout_plan_get()
 * is called and returns M0_LAT_READ plop. In this plop user gets cob fid
 * (at m0_layout_plop::pl_ent) and m0_layout_io_plop::iop_ext describing where
 * the unit to be read is located at the cob. Now, having this information the
 * user is ready to send the fop the ioservice.
 *
 * @verbatim
 *  m0_layout_plan_build() ->
 *    u0: m0_layout_plan_get() -> M0_LAT_READ -> user may start reading...
 *    u1: m0_layout_plan_get() -> M0_LAT_READ -> in parallel...
 *    u3: m0_layout_plan_get() -> M0_LAT_READ -> ...
 *    ...
 *    u0: m0_layout_plan_get() -> M0_LAT_OUT_READ  -> user should wait
 *    u0: The data for M0_LAT_READ is ready        -> m0_layout_plop_done()
 *    u0: Now the data in M0_LAT_OUT_READ is ready -> m0_layout_plop_done()
 *    ...
 *    m0_layout_plan_get() -> M0_LAT_FUN -> user must check pl_deps:
 *        if all plops in the list are M0_LPS_DONE -> call the function and
 *                                                 -> m0_layout_plop_done()
 *    ...
 *    m0_layout_plan_get() -> M0_LAT_DONE -> ... -> m0_layout_plop_done()
 *  m0_layout_plan_fini()
 * @endverbatim
 *
 * The user gets the data from ioservice, puts it at m0_layout_io_plop::iop_data
 * and calls m0_layout_plop_done(). The next m0_layout_plan_get() call might
 * return M0_LAT_OUT_READ indicating that the read data of the object is ready
 * for the user. One iteration of the graph traversing loop is done.
 *
 * Now, if the object consists only of a single unit, the next call to
 * m0_layout_plan_get() would return M0_LAT_DONE indicating that the i/o m0_op
 * is done and the plan is finished. But usually objects consist of
 * many units and all of them should be read in parallel. So the user
 * might call m0_layout_plan_get() many times and getting a number of
 * M0_LAT_READ plops before calling the first m0_layout_plop_done().
 *
 * @note M0_LAT_OUT_READ plop can be returned by the plan for the unit
 * even before m0_layout_plop_done() is called for its M0_LAT_READ plop.
 * It is user's responsibility to track the dependencies between plops
 * (see m0_layout_plop::pl_deps), execute and call m0_layout_plop_done()
 * on them in order.
 *
 * The picture becomes a bit more complicated when some disk is failed and
 * we are doing the degraded read. Or we are working in the read-verify mode.
 * In this case the plan would involve reading of the parity units also,
 * as well as running the parity calculation functions (M0_LAT_FUN plop)
 * for degraded groups or for every group (in read-verify mode).
 * The function would restore (or verify) data in a synchronous way.
 *
 * @note in case of the read-verify mode the verification status is indicated
 * by the return code from the m0_layout_fun_plop::fp_fun() call.
 *
 * The plan may change and adapt according to the situation. For example,
 * let's consider the case when some data unit in a parity group
 * cannot be read for some reason. The user should indicate the error to the
 * implementation via pl_rc at plop_done() for the M0_LAT_READ plop. The
 * implementation will change the plan and cancel all the plops that are no
 * longer relevant (like M0_LAT_OUT_READ for the failed M0_LAT_READ in this
 * case). On the next plan_get() calls the implementation will return
 * M0_LAT_READ plop(s) for the group parity unit(s), followed by M0_LAT_FUN
 * which would restore the failed data unit using the parity data, followed
 * by the new M0_LAT_OUT_READ plop for this data unit. Here is the diagram:
 *
 * @dot
 * digraph plopfail {
 * 	subgraph cluster_0 {
 * 		style=filled;
 * 		color=lightgrey;
 * 		start;
 * 		getlayout [label="layout-index.GET(fid)"];
 * 		getlayout -> start;
 * 	}
 * 	subgraph cluster_1 {
 * 		style=filled;
 * 		color=lightgrey;
 * 		read0 [label="cob0.read(ext0)" color="red"];
 * 		read1 [label="cob1.read(ext1)"];
 * 		read2 [label="cob2.read(ext2)"];
 * 		readout0 [label="buf0.ready" color="blue"];
 * 		readout1 [label="buf1.ready"];
 * 		readout2 [label="buf2.ready"];
 * 		readout0 -> read0;
 * 		readout1 -> read1;
 * 		readout2 -> read2;
 * 		read0 -> getlayout [style=invis];
 * 		read1 -> getlayout [style=invis];
 * 		read2 -> getlayout [style=invis];
 * 	}
 * 	subgraph cluster_degraded {
 * 		style=filled;
 * 		color=lightgrey;
 * 		readparity [label="cob-parity.read(ext-parity)"];
 * 		readparity -> read0 [style=invis];
 * 		compute [label="erasure(buf1, buf2, buf-parity)"];
 * 		readout0p [label="buf0.ready"];
 * 		compute -> readparity;
 * 		compute -> read1;
 * 		compute -> read2;
 * 		readout0p -> compute;
 * 	}
 * 	subgraph cluster_2 {
 * 		style=filled;
 * 		color=lightgrey;
 * 		done
 * 		done -> readout0p [style=invis];
 * 		done -> readout1 [style=invis];
 * 		done -> readout2 [style=invis];
 * 	}
 * }
 * @enddot
 *
 * ISC user should analyse the plops and their dependencies in oder to
 * figure out whether the user data can be obtained at the server side.
 * If so the computation can be delegated to the server. In some cases,
 * the data may not be accessible at the server. For example, in degraded
 * mode some data units in some parity groups are not available at the
 * server and have to be recovered at the client side. In this case the
 * computation on these units has to be done at the client side too.
 *
 * ISC should also be able to distinguish between reads of data units and
 * reads of parity units. (Because the computation makes sense only on the
 * data units.) To figure this out ISC have to analyse the dependencies
 * between plops: if there is a resulting M0_LAT_OUT_READ plop and it depends
 * only on the M0_LAT_READ (and possibly some FUNctions that can be called at
 * the server side, like decompression or checksumming) - it means that the
 * user data can be obtained at the server side independently from the other
 * servers (there are no M0_LAT_READs or parity calculation FUN-plops in
 * the dependencies). So the execution of such chain of plops can be
 * delegated to the server side along with the ISC computation. In all
 * other cases it should be done at the client side.
 *
 * @{
 */

#include "lib/types.h"        /* uint64_t */
#include "lib/tlist.h"
#include "lib/vec.h"

struct m0_layout;
struct m0_layout_plan;
struct m0_layout_plop;
struct m0_layout_plop_ops;

/**
 * Types of plops.
 *
 * A value of this enum is placed by the implementation in
 * m0_layout_plop::pl_type field of a plop, before it is returned by
 * m0_layout_plan_get().
 */
enum m0_layout_plop_type {
	/**
	 * Input plop, m0_layout_inout_plop.
	 *
	 * Input plop has no dependencies. It is used to signal the
	 * implementation that a part of input data is available.
	 *
	 * An input plop is ready immediately after the plan for a write
	 * operation is created. The user handles it by setting
	 * m0_layout_in_object_plop::iob_ready to the set of available data
	 * extents. More input plops are generated by the implementation until
	 * the entire range of data is available.
	 *
	 * This plop is useful in situations when the entire input data-set is
	 * not available at once.
	 *
	 * @see m0_layout_inout_plop
	 */
	M0_LAT_IN_WRITE,
	/**
	 * Read plop, m0_layout_io_plop.
	 *
	 * This plop instructs the user to read data from the specified object.
	 *
	 * @see m0_layout_io_plop
	 */
	M0_LAT_READ,
	/**
	 * Write plop, m0_layout_io_plop.
	 *
	 * This plop instructs the user to write data to the specified object.
	 *
	 * @see m0_layout_io_plop
	 */
	M0_LAT_WRITE,
	/**
	 * Get plop, m0_layout_index_plop.
	 *
	 * This plop instructs the user to look values up in the specified
	 * index.
	 *
	 * @see m0_layout_index_plop
	 */
	M0_LAT_GET,
	/**
	 * Put plop, m0_layout_index_plop.
	 *
	 * This plop instructs the user to insert values into the specified
	 * index.
	 *
	 * @see m0_layout_index_plop
	 */
	M0_LAT_PUT,
	/**
	 * Fun plop, m0_layout_fun_plop.
	 *
	 * This plop instructs the user to execute a non-blocking function
	 * provided by the implementation.
	 *
	 * @see m0_layout_fun_plop
	 */
	M0_LAT_FUN,
	/**
	 * Output plop, m0_layout_inout_plop.
	 *
	 * This plop signals the user that a part of the read data is available.
	 *
	 * @note this plop can be returned by the plan for the unit even before
	 * m0_layout_plop_done() is called for its M0_LAT_READ plop. It is user
	 * responsibility to track the dependencies between plops
	 * (see m0_layout_plop::pl_deps), execute and call m0_layout_plop_done()
	 * on them in order.
	 *
	 * @see m0_layout_inout_plop
	 */
	M0_LAT_OUT_READ,
	/**
	 * Done plop, m0_layout_plop.
	 *
	 * This plop signals the final completion of the plan execution. The
	 * value of m0_layout_plop::pl_rc field of this plop contains the return
	 * code of the entire operation.
	 *
	 * @see m0_layout_plop
	 */
	M0_LAT_DONE,

	M0_LAT_NR
};

/**
 * States of plops.
 */
enum m0_layout_plop_state {
	M0_LPS_INIT,
	M0_LPS_STARTED,
	M0_LPS_DONE,
};

/**
 * Common plop structure, shared by all "sub-classes".
 *
 * A call to m0_layout_plop_get() returns a structure starting with
 * m0_layout_plop. The user should use m0_layout_plop::pl_type to determine
 * actual sub-class.
 */
struct m0_layout_plop {
	/** Plop type. */
	enum m0_layout_plop_type         pl_type;
	/** Plop state. */
	enum m0_layout_plop_state        pl_state;
	/** Plan this plop is part of. */
	struct m0_layout_plan           *pl_plan;
	/**
	 * Plop colour.
	 *
	 * The implementation assigns colours to plops to improve locality of
	 * reference. The caller can optionally map colours to processor cores
	 * via m0_locality_get() and execute plops on cores corresponding to
	 * their colours.
	 */
	uint64_t                         pl_colour;
	/**
	 * Linkage in the list of all plops in the plan.
	 */
	struct m0_tlink                  pl_all_link;
	/**
	 * List of plops this plop depends on,
	 * linked via m0_layout_plop_relation::plr_dep_link.
	 *
	 * User can use this list to track the dependencies between plops
	 * to execute and call m0_layout_plop_done() on them in the
	 * correspondent order and also to check whether there are still
	 * pending undone plops we depend on.
	 */
	struct m0_tl                     pl_deps;
	/**
	 * List of plops that depend on this plop (dependants),
	 * linked via m0_layout_plop_relation::plr_rdep_link.
	 */
	struct m0_tl                     pl_rdeps;
	/**
	 * Fid of the entity this plop operates on. Meaning of this field
	 * depends on plop type.
	 */
	struct m0_fid                    pl_ent;
	/**
	 * Plop execution result. This field is set by the user before calling
	 * m0_layout_plop_done().
	 */
	int32_t                          pl_rc;
	/**
	 * User call-backs optionally set by the user before calling
	 * m0_layout_plop_done().
	 */
	const struct m0_layout_plop_ops *pl_ops;
};

/**
 * Helper structure to implement m:n relations between plops.
 */
struct m0_layout_plop_relation {
	/** Target dependency in this relation (see pl_deps list). */
	struct m0_layout_plop *plr_dep;
	/** Dependant plop in this relation (see pl_rdeps list). */
	struct m0_layout_plop *plr_rdep;
	/** pl_deps list link. */
	struct m0_tlink        plr_dep_link;
	/** pl_rdeps list link. */
	struct m0_tlink        plr_rdep_link;
};

/**
 * Set of user-supplied operations that the implementation invokes when an event
 * related to the plop happens asynchronously.
 */
struct m0_layout_plop_ops {
	/**
	 * The implementation invokes this for a still not complete plop, when
	 * its processing is no longer necessary, for example, because the plan
	 * changed due to a failure or plop execution was opportunistic in the
	 * first place.
	 *
	 * The user is expected to cancel plop processing if possible. It is
	 * always correct to ignore this call.
	 */
	void (*po_cancel)(struct m0_layout_plop *plop);
	/**
	 * The implementation invokes this when it no longer needs the plop. The
	 * user can free user data associated with the plop. The plop cannot be
	 * accessed by the user after this call returns.
	 *
	 * The plop structure will be freed by the implementation.
	 */
	void (*po_fini)(struct m0_layout_plop *plop);
};

/**
 * Input or output plop.
 *
 * In the case of object read operation, this plop is used to signal the user
 * that part of the output data are available.
 *
 * In the case of object write operation, this plop is used to signal the
 * implementation that part of the input data is available.
 *
 * @see M0_LAT_IN_WRITE, M0_LAT_OUT_READ
 */
struct m0_layout_inout_plop {
	struct m0_layout_plop iob_base;
	/** Available part of data. */
	struct m0_indexvec    iob_ready;
	/** Data buffers. */
	struct m0_bufvec      iob_data;
};

/**
 * IO plop instructs the user to perform read or write on some object.
 *
 * The object to read or write is specified as m0_layout_plop::pl_ent.
 *
 * @see M0_LAT_WRITE, M0_LAT_READ
 */
struct m0_layout_io_plop {
	struct m0_layout_plop iop_base;
	/**
	 * The set of extents to be read or written.
	 */
	struct m0_indexvec    iop_ext;
	/**
	 * Data buffers provided by the implementation.
	 *
	 * In case of write, the buffers contain the data to be written.
	 *
	 * In case of read, the buffers should be populated with the data
	 * read by the user before calling m0_layout_plop_done().
	 */
	struct m0_bufvec      iop_data;
};

/**
 * Index plop instructs the user to perform an operation on some index.
 *
 * The index is specified as m0_layout_plop::pl_ent.
 *
 * @see M0_LAT_PUT, M0_LAT_GET
 */
struct m0_layout_index_plop {
	struct m0_layout_plop rp_base;
};

/**
 * Fun plop instructs the user to call the given function.
 *
 * @see M0_LAT_FUN
 */
struct m0_layout_fun_plop {
	struct m0_layout_plop fp_base;
	int                 (*fp_fun)(struct m0_layout_fun_plop *plop);
	void                 *fp_datum;
};

/**
 * Constructs the plan describing how the given operation is to be executed for
 * the given layout.
 */
M0_EXTERN struct m0_layout_plan *
m0_layout_plan_build(struct m0_layout_instance *layout,
		     struct m0_op *op);
/**
 * Finalises the plan.
 *
 * This causes invocation of m0_layout_plop_ops::po_fini() for all still
 * existing plops.
 */
M0_EXTERN void m0_layout_plan_fini(struct m0_layout_plan *plan);

enum {
	M0_LAYOUT_PLOT_ANYCOLOUR = ~0ULL
};

/**
 * Allocates and returns a ready plop.
 *
 * If no plop is ready at the moment, +1 is returned.
 *
 * If colour is equal to M0_LAYOUT_PLOT_ANYCOLOUR, any ready plop is returned,
 * otherwise only a plop with the matching colour.
 */
M0_EXTERN int m0_layout_plan_get(struct m0_layout_plan *plan, uint64_t colour,
				 struct m0_layout_plop **out);

/**
 * Instructs the implementation that the user starts processing of the plop.
 *
 * @retval -EINVAL if the plop cannot be processed anymore for some reason.
 *                 For example, if it was cancelled by the plan already.
 */
M0_EXTERN int m0_layout_plop_start(struct m0_layout_plop *plop);

/**
 * Instructs the implementation that the user completed processing of the plop.
 *
 * This might make more plop ready to be returned from m0_layout_plan_get().
 *
 * If plop->pl_rc is non 0, the implementation might attempt to update the plan
 * to mask or correct the failure.
 */
M0_EXTERN void m0_layout_plop_done(struct m0_layout_plop *plop);

/**
 * Signals the implementation that operation execution should be aborted.
 *
 * Operation abort might affect access plan. The user still has to drain the
 * plan and execute all received plops until a DONE plop is produced.
 */
M0_EXTERN void m0_layout_plan_abort(struct m0_layout_plan *plan);

/** @} end of layout group */
#endif /* __MOTR_LAYOUT_PLAN_H__ */

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
