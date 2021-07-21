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
 * See also a quick introduction presentation at doc/PDF/layout-access-plan.pdf.
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
 * The directed graph is not presented as a static data structure. Instead,
 * a plan produces, via calls to m0_layout_plan_get(), plops as they become
 * "ready", in the sense that all their dependencies are satisfied.
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
 * All plops should be executed and m0_layout_plop_done() on them should be
 * called by the user in the dependency order indicated by the
 * m0_layout_plop::pl_deps list. Before starting the plop execution user should
 * call m0_layout_plop_start() which verifies whether the dependencies are
 * met and the plop is still actual (i.e. that it was not cancelled).
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
 * Use cases
 * ---------
 *
 * Let's consider, for example, the i/o read operation. The user calls
 * m0_layout_plan_build() passing m0_op as argument describing the operation.
 * After this we can enter the "loop" traversing the graph. m0_layout_plan_get()
 * is called and returns M0_LAT_READ plop. In this plop user gets cob fid
 * (at m0_layout_plop::pl_ent) and m0_layout_io_plop::iop_ext indexvec
 * describing where the unit to be read is located at the cob. Now, having this
 * information the user is ready to send the io fop to the ioservice.
 *
 * \image html layout-plan-simple-read.svg "Simple read graph"
 *
 * The next m0_layout_plan_get() call might return M0_LAT_OUT_READ indicating
 * the object data is ready for the user. But the data will be actually ready
 * only after the M0_LAT_READ plop on which M0_LAT_OUT_READ depends on is done.
 *
 * On receiving the reply from ioservice, the user puts the received io data
 * at m0_layout_io_plop::iop_data bufvec and calls m0_layout_plop_done(). Now
 * the user can access the data using M0_LAT_OUT_READ plop and call plop_done()
 * on it. One iteration of the graph traversing loop is done.
 *
 * Now, if the object is small and the bufvec at m0_op operation specified
 * by user spans only a single unit, the next call to m0_layout_plan_get()
 * might return M0_LAT_DONE indicating that the plan is done. But usually
 * objects consist of many units and a single m0_op might span many units
 * which, preferrably, should be read from the ioservice(s) in parallel. So
 * the user might call m0_layout_plan_get() many times and get a number of
 * M0_LAT_READ plops before calling the first m0_layout_plop_done().
 *
 * Let's look at the calls flow example below where u0, u1, u2 are the units
 * for which the plops are returned by the plan:
 *
 * @verbatim
    m0_layout_plan_build() ->
      u0: m0_layout_plan_get() -> M0_LAT_READ -> user may start reading...
      u1: m0_layout_plan_get() -> M0_LAT_READ -> in parallel...
      u2: m0_layout_plan_get() -> M0_LAT_READ -> ...
      ...
      u0: m0_layout_plan_get() -> M0_LAT_OUT_READ  -> wait for M0_LAT_READ...
      u0: the data for M0_LAT_READ is ready        -> m0_layout_plop_done()
      u0: now the data in M0_LAT_OUT_READ is ready -> m0_layout_plop_done()
      ...
      m0_layout_plan_get() -> M0_LAT_FUN -> user must check pl_deps:
          if all plops in the list are M0_LPS_DONE -> call the function and
                                                   -> m0_layout_plop_done()
      ...
      m0_layout_plan_get() -> +1 -> there are no more plops do to yet,
                                    user should complete some previous plops.
      ...
      m0_layout_plan_get() -> M0_LAT_DONE -> ... -> m0_layout_plop_done()
    m0_layout_plan_fini()
 * @endverbatim
 *
 * @note M0_LAT_OUT_READ plop can be returned by the plan for the unit even
 * before m0_layout_plop_done() is called for its M0_LAT_READ plop. It is
 * user responsibility to track the dependencies between plops (via pl_deps),
 * execute and call m0_layout_plop_done() on them in order.
 *
 * The picture becomes a bit more complicated when some disk is failed and
 * we are doing the degraded read. Or we are working in the read-verify mode.
 * In this case the plan would involve reading of the parity units also, as
 * well as running the parity calculation functions (M0_LAT_FUN plop) for
 * degraded groups or for every group (in read-verify mode). The function
 * would restore (or verify) data in a synchronous way.
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
 * \image html layout-plan-read-fail.svg "Read failure handling"
 *
 * ISC use case
 * ------------
 *
 * ISC user should analyse the plops and their dependencies in order to
 * figure out whether the user data can be obtained at the server side so
 * that the computation could be delegated there. In some cases, however,
 * the data may not be accessible at the server. For example, in degraded
 * mode some data units in some parity groups have to be recovered from
 * other data and parity units which may be located on different servers.
 * In this case the computation on the recovered units is better to do at
 * the client side.
 *
 * \image html layout-plan-isc-usage.svg "ISC usage example"
 *
 * If the graph dependency looks like a linear chain starting from M0_LAT_READ
 * and finishing with M0_LAT_OUT_READ, this might be a good canditate for ISC:
 *
 * @verbatim
      M0_LAT_READ <-- [ M0_LAT_FUN <-- ... ] <-- M0_LAT_OUT_READ
 * @endverbatim
 *
 * it means that the user data can be obtained at the server side
 * independently from the other servers (i.e. there are no several M0_LAT_READ
 * or parity calculation (M0_LAT_FUN) plops in the dependencies) and the
 * execution of such a chain of plops can be delegated to the server side
 * along with the ISC computation.
 *
 * ISC code should only make sure that the functions which may happen in
 * between the chain links can be run at the server side.
 *
 * In all other cases the user data should be retrieved at the client side
 * and the ISC computation on it should be done at the client side as well.
 *
 * @{
 */

#include "lib/types.h"        /* uint64_t */
#include "lib/tlist.h"
#include "lib/vec.h"

struct m0_op;
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
	/** target_ioreq this plop corresponds to. */
	struct target_ioreq             *pl_ti;
	/**
	 * Plop colour.
	 *
	 * The implementation assigns colours to plops to improve locality of
	 * reference. The caller can optionally map colours to processor cores
	 * via m0_locality_get() and execute plops on cores corresponding to
	 * their colours.
	 */
	uint64_t                         pl_colour;
	/** Plops list link magic. */
	uint64_t                         pl_magix;
	/**
	 * Linkage in the ::lp_plops list of all plops in the plan.
	 */
	struct m0_tlink                  pl_linkage;
	/**
	 * List of plops this plop depends on,
	 * linked via m0_layout_plop_rel::plr_dep_linkage.
	 *
	 * User can use this list to track the dependencies between plops
	 * to execute and call m0_layout_plop_done() on them in the
	 * correspondent order and also to check whether there are still
	 * pending undone plops we depend on.
	 */
	struct m0_tl                     pl_deps;
	/**
	 * List of plops that depend on this plop (dependants),
	 * linked via m0_layout_plop_rel::plr_rdep_linkage.
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
struct m0_layout_plop_rel {
	/** Target dependency in this relation (see pl_deps list). */
	struct m0_layout_plop *plr_dep;
	/** Dependant plop in this relation (see pl_rdeps list). */
	struct m0_layout_plop *plr_rdep;
	/** Deps lists links magic. */
	uint64_t               plr_magix;
	/** pl_deps list linkage. */
	struct m0_tlink        plr_dep_linkage;
	/** pl_rdeps list linkage. */
	struct m0_tlink        plr_rdep_linkage;
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
	struct m0_layout_plop  iop_base;

	/** Where to send the io-request. */
	struct m0_rpc_session *iop_session;
	/**
	 * The set of extents to be read or written.
	 */
	struct m0_indexvec     iop_ext;

	/** Global object offset. */
	m0_bindex_t            iop_goff;

	/**
	 * Data buffers provided by the implementation.
	 *
	 * In case of write, the buffers contain the data to be written.
	 *
	 * In case of read, the buffers should be populated with the data
	 * read by the user before calling m0_layout_plop_done().
	 */
	struct m0_bufvec       iop_data;
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

M0_TL_DESCR_DECLARE(pldeps, M0_EXTERN);
M0_TL_DECLARE(pldeps, M0_EXTERN, struct m0_layout_plop_rel);

M0_TL_DESCR_DECLARE(plrdeps, M0_EXTERN);
M0_TL_DECLARE(plrdeps, M0_EXTERN, struct m0_layout_plop_rel);

/**
 * Constructs the plan describing how the given @op is to be executed.
 */
M0_INTERNAL struct m0_layout_plan * m0_layout_plan_build(struct m0_op *op);

/**
 * Finalises the plan.
 *
 * This causes invocation of m0_layout_plop_ops::po_fini() for all still
 * existing plops.
 */
M0_INTERNAL void m0_layout_plan_fini(struct m0_layout_plan *plan);

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
M0_INTERNAL int m0_layout_plan_get(struct m0_layout_plan *plan, uint64_t colour,
				   struct m0_layout_plop **out);

/**
 * Instructs the implementation that the user starts processing of the plop.
 *
 * @retval -EINVAL if the plop cannot be processed anymore for some reason.
 *                 For example, if it was cancelled by the plan already.
 */
M0_INTERNAL int m0_layout_plop_start(struct m0_layout_plop *plop);

/**
 * Instructs the implementation that the user completed processing of the plop.
 *
 * This might make more plop ready to be returned from m0_layout_plan_get().
 *
 * If plop->pl_rc is non 0, the implementation might attempt to update the plan
 * to mask or correct the failure.
 */
M0_INTERNAL void m0_layout_plop_done(struct m0_layout_plop *plop);

/**
 * Signals the implementation that operation execution should be aborted.
 *
 * Operation abort might affect access plan. The user still has to drain the
 * plan and execute all received plops until a DONE plop is produced.
 */
M0_INTERNAL void m0_layout_plan_abort(struct m0_layout_plan *plan);

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
