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
#include "dtm0/dtm0.h" /* m0_dtx0_id */
#include "fop/fom_generic.h" /* m0_generic_phases_trans */
#include "dtm0/log.h" /* m0_dtm0_log_p_get_none_left */

/*
 * INIT -> GET_NEXT -> WAIT_NEXT
 *
 *
 * INIT -> WAIT -> TXN_INIT -> TXN_OPEN -> ... -> SPECIFIC -> REMOVE
 *
 * REMOVE -> SUCCESS -> FOL_REC_ADD -> ... -> TXN_COMMIT -> ...
 *
 * ... -> TXN_DONE_WAIT -> WAIT
 *
 *
 *           +------------------------------------------+
 *           |                                          |
 *           v                                          |
 * INIT -> WAIT -> TX_OPEN -> REMOVE -> TX_CLOSE -> ... +
 *           |
 *           v
 *         FINAL
 */

struct m0_dtm0_pruner_fom {
	struct m0_fom          dpf_gen;

	struct m0_dtm0_pruner *dpf_pruner;
	struct m0_semaphore    dpf_start_sem;
	struct m0_semaphore    dpf_stop_sem;

	bool                   dpf_successful;
	struct m0_dtx0_id      dpf_dtx0_id;
	struct m0_be_op        dpf_op;
};

enum pruner_fom_state {
	PFS_INIT   = M0_FOM_PHASE_INIT,
	PFS_FINISH = M0_FOM_PHASE_FINISH,
	/* XXX: Use aliases for every gen fom state used here? */
	/*PFS_TXN_DONE = M0_FOPH_TXN_DONE_WAIT,*/
	PFS_TXN_OPENED = M0_FOPH_TYPE_SPECIFIC,
	PFS_GET_NEXT,
	PFS_WAIT_NEXT,
	PFS_REMOVE,
	PFS_NR,
};

static struct m0_sm_state_descr pruner_fom_states[] = {
#define __S(name, flags, allowed)      \
	[name] = {                    \
		.sd_flags   = flags,  \
		.sd_name    = #name,  \
		.sd_allowed = allowed \
	}
	__S(PFS_GET_NEXT, 0,              M0_BITS(PFS_WAIT_NEXT)),
	__S(PFS_WAIT_NEXT,0,              M0_BITS(PFS_FINISH, M0_FOPH_TXN_INIT)),
	__S(PFS_REMOVE,   0,              M0_BITS(M0_FOPH_SUCCESS)),
#undef __S
};

struct m0_sm_trans_descr pruner_fom_trans[] = {
	[ARRAY_SIZE(m0_generic_phases_trans)] =
	{ "todo1", PFS_INIT,              PFS_GET_NEXT       },
	{ "todo2", PFS_GET_NEXT,          PFS_WAIT_NEXT      },
	{ "todo3", PFS_WAIT_NEXT,         PFS_FINISH         },
	{ "todo4", M0_FOPH_TXN_DONE_WAIT, PFS_GET_NEXT       },
	{ "todo5", PFS_WAIT_NEXT,         M0_FOPH_TXN_INIT   },
	{ "todo6", PFS_TXN_OPENED,        PFS_REMOVE         },
	{ "todo7", PFS_REMOVE,            M0_FOPH_SUCCESS    },

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

static struct m0_sm_conf pruner_fom_sm_conf = {
	.scf_name      = "m0_dtm0_pruner",
	.scf_nr_states = ARRAY_SIZE(pruner_fom_states),
	.scf_state     = pruner_fom_states,
	.scf_trans_nr  = ARRAY_SIZE(pruner_fom_trans),
	.scf_trans     = pruner_fom_trans
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

/*
 * TODO: This function does not handle scenario where an attempt to open be_tx
 * fails. In case of E2BIG, it is a bug (add assert). If ENOMEM happened,
 * then we should wait a bit (it is fine to skip pruning), but if happens
 * frequently then we should write a warning into the trace; or even
 * notify the HA.
 */
static int pruner_fom_tick(struct m0_fom *fom)
{
	struct m0_dtm0_pruner_fom *dpf = fom2pruner_fom(fom);
	struct m0_dtm0_pruner     *dp  = dpf->dpf_pruner;
	struct m0_dtm0_log        *dol = dp->dp_cfg.dpc_dol;
	enum m0_fom_phase_outcome  result = M0_FSO_NR;

	M0_ENTRY("pruner_fom=%p pruner=%p phase=%s", dpf, dp,
		 m0_fom_phase_name(fom, m0_fom_phase(fom)));

	switch (m0_fom_phase(fom)) {
	case M0_FOPH_TXN_OPEN:
		m0_dtm0_log_prune_credit(dol, &fom->fo_tx.tx_betx_cred);
		break;

	case M0_FOPH_TXN_DONE_WAIT:
		if (m0_be_tx_state(m0_fom_tx(fom)) == M0_BTS_DONE) {
			m0_dtx_fini(&fom->fo_tx);
			m0_fom_phase_set(fom, PFS_GET_NEXT);
			M0_SET0(&fom->fo_tx);
		}
		break;

	default:
		break;
	}

	if (m0_fom_phase(fom) > M0_FOPH_INIT &&
	    m0_fom_phase(fom) < M0_FOPH_NR) {
		result = m0_fom_tick_generic(fom);
	}

	switch (m0_fom_phase(fom)) {
	case PFS_INIT:
		m0_fom_phase_set(fom, PFS_GET_NEXT);
		m0_semaphore_up(&dpf->dpf_start_sem);
		result = M0_FSO_AGAIN;
		break;
	case PFS_GET_NEXT:
		m0_be_op_init(&dpf->dpf_op);
		m0_dtm0_log_p_get_none_left(dol, &dpf->dpf_op,
					    &dpf->dpf_dtx0_id,
					    &dpf->dpf_successful);
		result = m0_be_op_tick_ret(&dpf->dpf_op, fom, PFS_WAIT_NEXT);
		break;
	case PFS_WAIT_NEXT:
		/* TODO: reset? */
		m0_be_op_fini(&dpf->dpf_op);
		M0_SET0(&dpf->dpf_op);
		if (dpf->dpf_successful) {
			m0_fom_phase_set(fom, M0_FOPH_TXN_INIT);
			result = M0_FSO_AGAIN;
		} else {
			m0_fom_phase_set(fom, PFS_FINISH);
			result = M0_FSO_WAIT;
		}
		break;
	case PFS_TXN_OPENED:
		m0_fom_phase_set(fom, PFS_REMOVE);
		result = M0_FSO_AGAIN;
		break;
	case PFS_REMOVE:
		m0_dtm0_log_prune(dol, &fom->fo_tx.tx_betx, &dpf->dpf_dtx0_id);
		/* TODO: Skip M0_FOPH_FOL_REC_ADD */
		m0_fom_phase_set(fom, M0_FOPH_SUCCESS);
		result = M0_FSO_AGAIN;
		break;
	default:
		break;
	}

	M0_LEAVE("result=%d", result);
	return result;
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

	M0_ALLOC_PTR(dpf);
	if (dpf == NULL)
		return M0_ERR(-ENOMEM);

	m0_semaphore_init(&dpf->dpf_start_sem, 0);
	m0_semaphore_init(&dpf->dpf_stop_sem, 0);

	dp->dp_cfg = *dp_cfg;
	dp->dp_pruner_fom = dpf;
	dpf->dpf_pruner = dp;
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
	struct m0_fom             *fom = pruner_fom2fom(dpf);

	m0_fom_init(fom, &pruner_fom_type,
		    &pruner_fom_ops, NULL, NULL, reqh);
	fom->fo_local = true;
	fom->fo_local_update = true;
	m0_fom_queue(fom);
	m0_semaphore_down(&dpf->dpf_start_sem);
}

M0_INTERNAL void m0_dtm0_pruner_stop(struct m0_dtm0_pruner *dp)
{
	struct m0_dtm0_pruner_fom *dpf = dp->dp_pruner_fom;

	m0_semaphore_down(&dpf->dpf_stop_sem);
}

M0_INTERNAL void m0_dtm0_pruner_mod_init(void)
{
	m0_sm_conf_extend(m0_generic_conf.scf_state, pruner_fom_states,
			  min_check(m0_generic_conf.scf_nr_states,
				    pruner_fom_sm_conf.scf_nr_states));
	m0_sm_conf_trans_extend(&m0_generic_conf, &pruner_fom_sm_conf);
	/*
	 * TODO: turn it into an array and move the array closer to the
	 * definition of pruner_fom_states.
	 */
	pruner_fom_states[M0_FOPH_INIT].sd_allowed |= M0_BITS(PFS_GET_NEXT);
	pruner_fom_states[M0_FOPH_TXN_DONE_WAIT].sd_allowed |=
		M0_BITS(PFS_GET_NEXT);

	/*
	 * TODO: Generic FOM sm does not have transitions from TYPE_SPECIFIC to
	 * FINISH, SUCCESS, FAILED. Because of that, we either should explicitly
	 * add them in our sm or remove them.  Here we are removing them.
	 * Consider adding the missing transitions right into the generic FOM
	 * sm.
	 */
	pruner_fom_states[M0_FOPH_TYPE_SPECIFIC].sd_allowed =
		M0_BITS(PFS_REMOVE);

	m0_sm_conf_init(&pruner_fom_sm_conf);

	m0_fom_type_init(&pruner_fom_type, M0_DTM0_PRUNER_OPCODE,
			 &pruner_fom_type_ops, &m0_cfs_stype,
			 &pruner_fom_sm_conf);
}

M0_INTERNAL void m0_dtm0_pruner_mod_fini(void)
{
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
