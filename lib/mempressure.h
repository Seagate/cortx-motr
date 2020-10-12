/* -*- C -*- */
/*
 * Copyright (c) 2011-2020 Seagate Technology LLC and/or its Affiliates
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
#ifndef __MOTR_LIB_MEMPRESSURE_H__

#define __MOTR_LIB_MEMPRESSURE_H__

#include "lib/thread.h"         /* m0_thread */
#include "lib/mutex.h"          /* m0_mutex */

#ifdef __KERNEL__
#define PATH_MAX        4096
#endif

enum m0_mempressure_level {
	M0_MPL_LOW,
	M0_MPL_MEDIUM,
	M0_MPL_CRITICAL,
	M0_MPL_NR,
};

/**
 * A call-back that can be registered to what memory pressure changes.
 */
struct m0_mempressure_cb {
	uint64_t        mp_magic;
	/* linkage of all callbacks. */
	struct m0_tlink  mc_linkage;
	/* subscriber callback, that will execute during event occurs. */
	void            (*mc_pressure)(enum m0_mempressure_level level);
	/* level [M0_MPL_LOW, M0_MPL_MEDIUM, M0_MPL_CRITICAL]. */
	int             level;
};

/**
 * Per level memory presure object.
 */
struct mempressure_obj {
	/* All opened file descripter. */
	int                    mo_c_fd;
	int                    mo_e_fd[M0_MPL_NR];
	int                    mo_p_ec[M0_MPL_NR];
	/* evenent control path :cgroup.event_control,
 	  example: /sys/fs/cgroup/memory/foo/cgroup.event_control. */
	char                   mo_ec_path[PATH_MAX];
	/* reference to parent mempressure object. */
	struct m0_mempressure *mo_p_ref;
	/* Listener thread. */
	struct m0_thread       mo_listener_t;
};

/**
 * memory subsystem pressure publish/subscribe object.
 */
struct m0_mempressure {
	/* current object state. */
	bool                      mp_active;
	/* current memory pressure level. */
	int                       mp_cur_level;
	/* All suscriber's callback list. */
	struct m0_tl              mp_runmq;
	/* group lock for suscribers. */
	struct m0_mutex           mp_gr_lock;
	/* All three Listener objects. */
	struct mempressure_obj    mp_listener_obj;
};

/**
 * mempressure load module for cgroup.
 */
M0_INTERNAL int  m0_mempressure_mod_init(void);

/**
 * mempressure unload module
 */
M0_INTERNAL void m0_mempressure_mod_fini(void);

/**
 * Returns the current memory pressure level.
 */
M0_INTERNAL enum m0_mempressure_level m0_mempressure_get(void);

/**
 * Subscribe your callback with mempressure listener
 */
M0_INTERNAL int m0_mempressure_cb_add(struct m0_mempressure_cb *cb);

/**
 * unsubscribe your callback with mempressure listener.
 */
M0_INTERNAL void m0_mempressure_cb_del(struct m0_mempressure_cb *cb);

#endif

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
