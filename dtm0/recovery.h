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


#pragma once

#ifndef __MOTR_DTM0_RECOVERY_H__
#define __MOTR_DTM0_RECOVERY_H__

#include "lib/tlist.h" /* m0_tl */
#include "ha/note.h"   /* m0_ha_obj_state */
#include "conf/ha.h"   /* m0_conf_ha_process_event */
#include "sm/sm.h"     /* m0_sm, m0_sm_group */

/* TODO: figure good literals and it move to the satchel */
#define M0_DTM0_RMACH_MAGIC 0x3AB0DBED
#define M0_DTM0_RMACH_HEAD_MAGIC 0xB0EB0D11

/* imports */
struct m0_dtm0_service;
struct m0_conf_process;
struct dtm0_req_fop;
struct m0_be_dtm0_log_iter;
struct m0_dtm0_log_rec;
struct m0_be_op;
struct m0_fom;

/* exports */
struct m0_dtm0_recovery_machine_ops;
struct m0_dtm0_recovery_machine;
struct recovery_fom;

enum m0_dtm0_recovery_machine_states {
	M0_DRMS_INIT,
	M0_DRMS_STARTED,
	M0_DRMS_STOPPED,
	M0_DRMS_NR,
};

struct m0_dtm0_recovery_machine_ops {
	/**
	 * Post a REDO message to the target DTM0 service.
	 */
	void (*redo_post)(struct m0_dtm0_recovery_machine *m,
			  struct m0_fom                   *fom,
			  const struct m0_fid             *tgt_proc,
			  const struct m0_fid             *tgt_svc,
			  struct dtm0_req_fop             *redo,
			  struct m0_be_op                 *op);

	/**
	 * Post a conf ha process event.
	 */
	void (*ha_event_post)(struct m0_dtm0_recovery_machine *m,
			      const struct m0_fid             *tgt_proc,
			      const struct m0_fid             *tgt_svc,
			      enum m0_conf_ha_process_event    event);

	int (*log_iter_init)(struct m0_dtm0_recovery_machine *m,
			     struct m0_be_dtm0_log_iter      *iter);

	/**
	 * Get next log record (or -ENOENT) from the local DTM0 log.
	 * @param[in] tgt_svc DTM0 service this REDO shall be sent to.
	 * @param[in,opt] origin_svc DTM0 service to be selected. When
	 *                           this parameter is set to non-NULL,
	 *                           the iter is supposed to select only
	 *                           the log records that were originated
	 *                           on this particular service.
	 */
	int (*log_iter_next)(struct m0_dtm0_recovery_machine   *m,
			     struct m0_be_dtm0_log_iter        *iter,
			     const struct m0_fid               *tgt_svc,
			     const struct m0_fid               *origin_svc,
			     struct m0_dtm0_log_rec            *record);

	void (*log_iter_fini)(struct m0_dtm0_recovery_machine *m,
			      struct m0_be_dtm0_log_iter      *iter);
};


struct m0_dtm0_recovery_machine {
	struct m0_dtm0_service                    *rm_svc;
	struct m0_tl                               rm_rfoms;
	struct m0_dtm0_recovery_machine_ops        rm_ops;
	struct recovery_fom                       *rm_local_rfom;

	struct m0_sm_group                         rm_sm_group;
	struct m0_sm                               rm_sm;
};

M0_INTERNAL int m0_drm_domain_init(void);

M0_INTERNAL void m0_drm_domain_fini(void);

/**
 * Initialise a recovery machine.
 * @param ops Recovery machine operations (such as sending of REDOs and some
 *            others) that have to be used instead of the default ops.
 *            These ops may be used to alter the effects of recovery machine
 *            decisions on the system (for example, in UTs).
 */
M0_INTERNAL int
m0_dtm0_recovery_machine_init(struct m0_dtm0_recovery_machine           *m,
			      const struct m0_dtm0_recovery_machine_ops *ops,
			      struct m0_dtm0_service                    *svc);

M0_INTERNAL void
m0_dtm0_recovery_machine_start(struct m0_dtm0_recovery_machine *m);

M0_INTERNAL void
m0_dtm0_recovery_machine_stop(struct m0_dtm0_recovery_machine *m);

M0_INTERNAL void
m0_dtm0_recovery_machine_fini(struct m0_dtm0_recovery_machine *m);

M0_INTERNAL void
m0_dtm0_recovery_machine_redo_post(struct m0_dtm0_recovery_machine *m,
				   struct dtm0_req_fop             *redo,
				   struct m0_be_op                 *op);

/* UT-related API */
M0_INTERNAL void
m0_ut_remach_heq_post(struct m0_dtm0_recovery_machine *m,
		      const struct m0_fid             *tgt_svc,
		      enum m0_ha_obj_state             state);
M0_INTERNAL void
m0_ut_remach_populate(struct m0_dtm0_recovery_machine *m,
		      struct m0_conf_process          *procs,
		      const struct m0_fid             *svcs,
		      bool                            *is_volatile,
		      uint64_t                         objs_nr);

#endif /* __MOTR_DTM0_RECOVERY_H__ */

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
