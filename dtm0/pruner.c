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

/**
 * @addtogroup dtm0
 *
 * @{
 */

#define M0_TRACE_SUBSYSTEM M0_TRACE_SUBSYS_DTM0
#include "lib/trace.h"

#include "dtm0/pruner.h"
#include "dtm0/service.h" /* m0_co_fom_service */
#include "lib/memory.h" /* M0_ALLOC_PTR */
#include "lib/errno.h"  /* ENOMEM */
#include "rpc/rpc_opcodes.h" /* M0_DTM0_PRUNER_OPCODE */

/*
 *
 *           -------------------------------------------+
 *           |                                          |
 *           v                                          |
 * INIT -> WAIT -> TX_OPEN -> REMOVE -> TX_CLOSE -> ... +
 *           |
 *           v
 *         FINAL
 */

struct m0_dtm0_pruner_fom {
	struct m0_fom       dpf_gen;
	struct m0_semaphore dpf_start_sem;
	struct m0_semaphore dpf_stop_sem;
};

enum pruner_fom_state {
	PFS_INIT   = M0_FOM_PHASE_INIT,
	PFS_FINISH = M0_FOM_PHASE_FINISH,
	PFS_NR,
};

static struct m0_sm_state_descr pruner_fom_states[PFS_NR] = {
#define _S(name, flags, allowed)      \
	[name] = {                    \
		.sd_flags   = flags,  \
		.sd_name    = #name,  \
		.sd_allowed = allowed \
	}

	_S(PFS_INIT,   M0_SDF_INITIAL, M0_BITS(PFS_FINISH)),
	_S(PFS_FINISH, M0_SDF_TERMINAL, 0),
#undef _S
};

static struct m0_dtm0_pruner_fom *fom2pruner_fom(const struct m0_fom *fom)
{
	/* XXX TODO bob_of() */
	return container_of(fom, struct m0_dtm0_pruner_fom, dpf_gen);
}

static struct m0_fom *pruner_fom2fom(struct m0_dtm0_pruner_fom *dpf)
{
	return &dpf->dpf_gen;
}

const static struct m0_sm_conf pruner_fom_sm_conf = {
	.scf_name      = "m0_dtm0_pruner",
	.scf_nr_states = ARRAY_SIZE(pruner_fom_states),
	.scf_state     = pruner_fom_states,
};

static struct m0_fom_type pruner_fom_type;

static const struct m0_fom_type_ops pruner_fom_type_ops = {
	.fto_create = NULL
};

static void pruner_fom_fini(struct m0_fom *fom)
{
	struct m0_dtm0_pruner_fom *dpf = fom2pruner_fom(fom);

	m0_fom_fini(fom);
	m0_semaphore_up(&dpf->dpf_stop_sem);
}

static size_t pruner_fom_locality(const struct m0_fom *fom)
{
	(void)fom;
	return 0;
}

static int pruner_fom_tick(struct m0_fom *fom)
{
	struct m0_dtm0_pruner_fom *dpf = fom2pruner_fom(fom);

	m0_semaphore_up(&dpf->dpf_start_sem);
	m0_fom_phase_set(fom, PFS_FINISH);
	return M0_FSO_WAIT;
}

static const struct m0_fom_ops pruner_fom_ops = {
	.fo_fini          = pruner_fom_fini,
	.fo_tick          = pruner_fom_tick,
	.fo_home_locality = pruner_fom_locality
};

M0_INTERNAL int m0_dtm0_pruner_init(struct m0_dtm0_pruner     *dp,
				    struct m0_dtm0_pruner_cfg *dp_cfg)
{
	struct m0_dtm0_pruner_fom   *dpf;

	M0_PRE(dp_cfg->dpc_cfs->cfs_reqh_service->rs_type == &m0_cfs_stype);

	m0_fom_type_init(&pruner_fom_type, M0_DTM0_PRUNER_OPCODE,
			 &pruner_fom_type_ops, &m0_cfs_stype,
			 &pruner_fom_sm_conf);

	M0_ALLOC_PTR(dpf);
	if (dpf == NULL)
		return M0_ERR(-ENOMEM);

	m0_semaphore_init(&dpf->dpf_start_sem, 0);
	m0_semaphore_init(&dpf->dpf_stop_sem, 0);

	dp->dp_cfg = *dp_cfg;
	dp->dp_pruner_fom = dpf;
	return 0;
}

M0_INTERNAL void m0_dtm0_pruner_fini(struct m0_dtm0_pruner *dp)
{
	struct m0_dtm0_pruner_fom *dpf = dp->dp_pruner_fom;

	m0_semaphore_fini(&dpf->dpf_start_sem);
	m0_semaphore_fini(&dpf->dpf_stop_sem);
	m0_free(dpf);
	dp->dp_pruner_fom = NULL;
}

M0_INTERNAL void m0_dtm0_pruner_start(struct m0_dtm0_pruner *dp)
{
	struct m0_dtm0_pruner_fom *dpf = dp->dp_pruner_fom;
	struct m0_reqh            *reqh =
		dp->dp_cfg.dpc_cfs->cfs_reqh_service->rs_reqh;

	m0_fom_init(pruner_fom2fom(dpf), &pruner_fom_type,
		    &pruner_fom_ops, NULL, NULL, reqh);
	m0_fom_queue(pruner_fom2fom(dpf));
	m0_semaphore_down(&dpf->dpf_start_sem);
}

M0_INTERNAL void m0_dtm0_pruner_stop(struct m0_dtm0_pruner *dp)
{
	struct m0_dtm0_pruner_fom *dpf = dp->dp_pruner_fom;

	m0_semaphore_down(&dpf->dpf_stop_sem);
}

#undef M0_TRACE_SUBSYSTEM

/** @} end of dtm0 group */

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
