/* -*- C -*- */
/*
 * Copyright (c) 2013-2020 Seagate Technology LLC and/or its Affiliates
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


#pragma once

#ifndef __MOTR_FOP_FOM_INTERPOSE_H__
#define __MOTR_FOP_FOM_INTERPOSE_H__

#include "fop/fom.h"

struct m0_fom_interpose_ops;

/**
 * A structure to modify the behaviour of a fom dynamically.
 *
 * Fom interposition allows fom phase transition logic, implemented in
 * m0_fom_ops::fo_tick() function, to be dynamically adjusted.
 *
 * This can be used to implement monitoring, profiling or structural
 * relationships between foms (e.g., leader-follower).
 *
 * Multiple levels of interception can be applied to the same fom.
 */
struct m0_fom_interpose {
	struct m0_fom_ops                  fi_shim;
	const struct m0_fom_ops           *fi_orig;
	const struct m0_fom_interpose_ops *fi_ops;
};

/**
 * Functions to be executed around interposed fom tick.
 */
struct m0_fom_interpose_ops {
	/**
	 * Functions from this array are executed before the original fom tick
	 * is executed.
	 *
	 * A pre-function can return either normal tick return value (i.e.,
	 * FSO_AGAIN or FSO_WAIT) or a special INTERPOSE_CONT value. In the
	 * former case, the returned value is immediately returned as the tick
	 * result. Otherwise, original fom tick function is called.
	 */
	int (*io_pre [64])(struct m0_fom *fom, struct m0_fom_interpose *proxy);
	/**
	 * Functions from this array are executed after the original fom tick is
	 * executed.
	 *
	 * Post-functions take the result of the original tick as an additional
	 * parameter. Whatever is returned by the post-function is returned as
	 * the result of the tick.
	 */
	int (*io_post[64])(struct m0_fom *fom, struct m0_fom_interpose *proxy,
			   int result);
};

/**
 * A structure to implement the leader-follower fom execution on top of
 * interposition.
 */
struct m0_fom_thralldom {
	/** Leader fom, which calls follower fom and waits for its completion. */
	struct m0_fom           *ft_leader;
	/**
	 * A structure which modifies the behaviour of a follower fom so that it
	 * wakes up leader fom when finish phase is reached.
	 */
	struct m0_fom_interpose  ft_fief;
	/** Called when follower fom reaches finish state. */
	void                   (*ft_end)(struct m0_fom_thralldom *thrall,
					 struct m0_fom           *serf);
};

/**
 * Activates the interposition by substitution of original fom tick function
 * with interposition tick.
 */
M0_INTERNAL void m0_fom_interpose_enter(struct m0_fom           *fom,
					struct m0_fom_interpose *proxy);

/** Disables the interposition by restoring of original fom tick function. */
M0_INTERNAL void m0_fom_interpose_leave(struct m0_fom           *fom,
					struct m0_fom_interpose *proxy);


/**
 * Arranges for the leader to be woken up once the serf reaches
 * M0_FOM_PHASE_FINISH, should be called by leader fom.
 * It is up to the caller to make sure the serf is actually executing and that
 * the leader goes to sleep.
 *
 * @note It is not required to initialise thrall context before call, all
 * context preparation is done inside of this function.
 */
M0_INTERNAL void m0_fom_enthrall(struct m0_fom *leader, struct m0_fom *serf,
				 struct m0_fom_thralldom *thrall,
				 void (*end)(struct m0_fom_thralldom *thrall,
					     struct m0_fom           *serf));

#endif /* __MOTR_FOP_FOM_INTERPOSE_H__ */

/*
 *  Local variables:
 *  c-indentation-style: "K&R"
 *  c-basic-offset: 8
 *  tab-width: 8
 *  fill-column: 80
 *  scroll-step: 1
 *  End:
 */
