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
#ifndef __MOTR_BE_TX_GROUP_FOM_H__
#define __MOTR_BE_TX_GROUP_FOM_H__

#include "lib/types.h"          /* bool */
#include "lib/semaphore.h"      /* m0_semaphore */

#include "fop/fom.h"            /* m0_fom */
#include "sm/sm.h"              /* m0_sm_ast */

#include "be/op.h"              /* m0_be_op */

struct m0_be_tx_group;
struct m0_reqh;

/**
 * @defgroup be Meta-data back-end
 * @{
 */

struct m0_be_tx_group_fom {
	/** generic fom */
	struct m0_fom          tgf_gen;
	struct m0_reqh        *tgf_reqh;
	/** group to handle */
	struct m0_be_tx_group *tgf_group;
	/** m0_be_op for I/O operations */
	struct m0_be_op        tgf_op;
	/** m0_be_op for tx GC after recovery */
	struct m0_be_op        tgf_op_gc;
	/**
	 * True iff all transactions of the group have reached M0_BTS_DONE
	 * state.
	 */
	bool                   tgf_stable;
	bool                   tgf_stopping;
	struct m0_sm_ast       tgf_ast_handle;
	struct m0_semaphore    tgf_start_sem;
	struct m0_semaphore    tgf_finish_sem;
	bool                   tgf_recovery_mode;
};

/** @todo XXX TODO s/gf/m/ in function parameters */
M0_INTERNAL void m0_be_tx_group_fom_init(struct m0_be_tx_group_fom *m,
					 struct m0_be_tx_group     *gr,
					 struct m0_reqh            *reqh);
M0_INTERNAL void m0_be_tx_group_fom_fini(struct m0_be_tx_group_fom *m);
M0_INTERNAL void m0_be_tx_group_fom_reset(struct m0_be_tx_group_fom *m);

M0_INTERNAL int m0_be_tx_group_fom_start(struct m0_be_tx_group_fom *gf);
M0_INTERNAL void m0_be_tx_group_fom_stop(struct m0_be_tx_group_fom *gf);

M0_INTERNAL void m0_be_tx_group_fom_handle(struct m0_be_tx_group_fom *m);
M0_INTERNAL void m0_be_tx_group_fom_stable(struct m0_be_tx_group_fom *gf);

M0_INTERNAL struct m0_sm_group *
m0_be_tx_group_fom__sm_group(struct m0_be_tx_group_fom *m);

M0_INTERNAL void
m0_be_tx_group_fom_recovery_prepare(struct m0_be_tx_group_fom *m);

M0_INTERNAL void m0_be_tx_group_fom_mod_init(void);
M0_INTERNAL void m0_be_tx_group_fom_mod_fini(void);

/** @} end of be group */
#endif /* __MOTR_BE_TX_GROUP_FOM_H__ */

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
