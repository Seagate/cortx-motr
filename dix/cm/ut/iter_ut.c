/* -*- C -*- */
/*
 * Copyright (c) 2016-2020 Seagate Technology LLC and/or its Affiliates
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


#define M0_TRACE_SUBSYSTEM M0_TRACE_SUBSYS_UT

#include <endian.h>            /* htobe64, betoh64 */
#include <unistd.h>
#include "ut/ut.h"
#include "be/ut/helper.h"
#include "be/tx.h"
#include "pool/pool.h"
#include "reqh/reqh.h"
#include "reqh/reqh_service.h"
#include "dix/cm/cm.h"
#include "rpc/rpc_opcodes.h"
#include "cas/cas.h"
#include "cas/ctg_store.h"
#include "dix/cm/iter.h"
#include "dix/fid_convert.h"
#include "lib/finject.h"
#include "lib/trace.h"

#define POOL_WIDTH    10
#define NODES         10
#define DATA_NR       1
#define FAILURES_NR   3
#define SPARE_NR      3
#define DEVS_ID_SHIFT 100
#define INVALID_IDX   0xFF

static struct m0_reqh           reqh;
static struct m0_be_ut_backend  be;
static struct m0_be_seg        *seg0;
static struct m0_reqh_service  *repair_svc;
static struct m0_reqh_service  *rebalance_svc;
static struct m0_fom_type       ut_fom_type;
static struct m0_fom_type       ut_fom_type_rep;
static struct m0_fom_type       ut_fom_type_reb;

static struct m0_pool         pool;
static struct m0_pools_common pc;
static struct m0_pool_version pv;

static struct m0_fid          pv_fid = M0_FID_TINIT('v', 1, 1);
static        int             spare_usage_pos = 0;

enum {
	RPC_CUTOFF = 4096
};

enum iter_ut_fom_op {
	ITER_UT_OP_META_INSERT,
	ITER_UT_OP_META_DELETE,
	ITER_UT_OP_META_LOOKUP,
	ITER_UT_OP_CTIDX_INSERT,
	ITER_UT_OP_CTIDX_DELETE,
	ITER_UT_OP_KV_INSERT,
	ITER_UT_OP_KV_DELETE
};

enum iter_ut_fom_phase {
	ITER_UT_FOM_INIT  = M0_FOM_PHASE_INIT,
	ITER_UT_FOM_FINAL = M0_FOM_PHASE_FINISH,
	ITER_UT_FOM_DONE  = M0_FOM_PHASE_NR,
	ITER_UT_FOM_INIT_WAIT,
	ITER_UT_FOM_CTIDX_LOCK,
	ITER_UT_FOM_META_LOCK,
	ITER_UT_FOM_EXEC,
	ITER_UT_FOM_TX_COMMIT
};

enum iter_ut_pool_nd_state {
	ITER_UT_ONLINE,
	ITER_UT_REPAIRING,
	ITER_UT_REPAIRED,
	ITER_UT_REBALANCING,
};

enum iter_ut_dev_type {
	ITER_UT_DATA,
	ITER_UT_PARITY,
	ITER_UT_SPARE,
};

struct iter_ut_fom {
	enum iter_ut_fom_op      iu_op;
	struct m0_fom            iu_fom;
	struct m0_ctg_op         iu_ctg_op;
	struct m0_fid            iu_cctg_fid;
	struct m0_buf            iu_key;
	struct m0_buf            iu_val;
	struct m0_be_tx_credit   iu_tx_cred;
	struct m0_cas_ctg       *iu_ctg;
	struct m0_cas_id         iu_cid;
	struct m0_semaphore     *iu_sem;
	struct m0_long_lock_link iu_lock_link;
	struct m0_long_lock_link iu_del_lock_link;
};

static struct m0_sm_state_descr iter_ut_fom_phases[] = {
	[ITER_UT_FOM_INIT] = {
		.sd_flags     = M0_SDF_INITIAL,
		.sd_name      = "init",
		.sd_allowed   = M0_BITS(ITER_UT_FOM_INIT_WAIT)
	},
	[ITER_UT_FOM_INIT_WAIT] = {
		.sd_name      = "init_wait",
		.sd_allowed   = M0_BITS(ITER_UT_FOM_CTIDX_LOCK,
					ITER_UT_FOM_META_LOCK,
					ITER_UT_FOM_EXEC)
	},
	[ITER_UT_FOM_CTIDX_LOCK] = {
		.sd_name      = "ctidx-lock",
		.sd_allowed   = M0_BITS(ITER_UT_FOM_EXEC)

	},
	[ITER_UT_FOM_META_LOCK] = {
		.sd_name      = "meta-lock",
		.sd_allowed   = M0_BITS(ITER_UT_FOM_EXEC)

	},
	[ITER_UT_FOM_EXEC] = {
		.sd_name      = "exec",
		.sd_allowed   = M0_BITS(ITER_UT_FOM_DONE)
	},
	[ITER_UT_FOM_DONE] = {
		.sd_name      = "in-progress",
		.sd_allowed   = M0_BITS(ITER_UT_FOM_FINAL,
					ITER_UT_FOM_TX_COMMIT)
	},
	[ITER_UT_FOM_TX_COMMIT] = {
		.sd_name      = "tx-commit-wait",
		.sd_allowed   = M0_BITS(ITER_UT_FOM_FINAL)
	},
	[ITER_UT_FOM_FINAL] = {
		.sd_name      = "final",
		.sd_flags     = M0_SDF_TERMINAL,
	},
};

static const struct m0_sm_conf iter_ut_fom_conf = {
	.scf_name      = "iter_ut_fom",
	.scf_nr_states = ARRAY_SIZE(iter_ut_fom_phases),
	.scf_state     = iter_ut_fom_phases,
};

static struct m0_dix_cm_iter* iter_ut_iter(struct m0_reqh_service *svc)
{
	struct m0_cm     *cm = container_of(svc, struct m0_cm,
					    cm_service);
	struct m0_dix_cm *dix_cm = container_of(cm, struct m0_dix_cm, dcm_base);

	return &dix_cm->dcm_it;
}

struct iter_ut_dev_id {
	uint32_t                   pool_dev_id;
	uint32_t                   global_dev_id;
	uint32_t                   spare_idx;
	enum iter_ut_dev_type      type;
	enum iter_ut_pool_nd_state state;
};

#define NUM_DATA_PARITY_DEV_IDS (sizeof(data_parity_dev_ids) / sizeof(data_parity_dev_ids[0]))
#define NUM_SPARE_DEV_IDS      (sizeof(spare_dev_ids) / sizeof(spare_dev_ids[0]))

#define DATA_PARITY_DEV_IDS \
	iter_ut_dev_ids(4, 104, ITER_UT_DATA,   ITER_UT_ONLINE) \
	iter_ut_dev_ids(9, 109, ITER_UT_PARITY, ITER_UT_ONLINE) \
	iter_ut_dev_ids(6, 106, ITER_UT_PARITY, ITER_UT_ONLINE) \
	iter_ut_dev_ids(5, 105, ITER_UT_PARITY, ITER_UT_ONLINE) \

#define SPARE_DEV_IDS \
	iter_ut_dev_ids(1, 101, ITER_UT_SPARE, ITER_UT_ONLINE) \
	iter_ut_dev_ids(0, 100, ITER_UT_SPARE, ITER_UT_ONLINE) \
	iter_ut_dev_ids(7, 107, ITER_UT_SPARE, ITER_UT_ONLINE) \

static struct iter_ut_dev_id data_parity_dev_ids[] = {
	#define iter_ut_dev_ids(_pool_dev_id, _global_dev_id, _type, _state) \
		{.pool_dev_id=_pool_dev_id, .global_dev_id=_global_dev_id , \
		 .spare_idx=INVALID_IDX,.type=_type, .state=_state},
	DATA_PARITY_DEV_IDS
	#undef iter_ut_dev_ids
};

static struct iter_ut_dev_id spare_dev_ids[] = {
	#define iter_ut_dev_ids(_pool_dev_id, _global_dev_id, _type, _state) \
		{.pool_dev_id=_pool_dev_id, .global_dev_id=_global_dev_id , \
		 .spare_idx=INVALID_IDX,.type=_type, .state=_state},
	SPARE_DEV_IDS
	#undef iter_ut_dev_ids
};

static uint32_t spare_idx = 0;

static void iter_ut_fom_fini(struct m0_fom *fom0)
{
	struct iter_ut_fom *fom = M0_AMB(fom, fom0, iu_fom);

	m0_long_lock_link_fini(&fom->iu_lock_link);
	m0_long_lock_link_fini(&fom->iu_del_lock_link);

	if (fom->iu_op == ITER_UT_OP_CTIDX_INSERT)
		m0_cas_id_fini(&fom->iu_cid);
	m0_fom_fini(fom0);
	m0_semaphore_up(fom->iu_sem);
}

static uint64_t iter_ut_fom_locality(const struct m0_fom *fom)
{
	return fom->fo_type->ft_id;
}

static void iter_ut_fom_init(struct iter_ut_fom *fom)
{
	struct m0_be_tx_credit *accum = &fom->iu_fom.fo_tx.tx_betx_cred;

	m0_long_lock_link_init(&fom->iu_lock_link, &fom->iu_fom, NULL);
	m0_long_lock_link_init(&fom->iu_del_lock_link, &fom->iu_fom, NULL);

	if (fom->iu_op == ITER_UT_OP_CTIDX_INSERT ||
	    fom->iu_op == ITER_UT_OP_CTIDX_DELETE) {
		struct m0_cas_id    *cid = &fom->iu_cid;
		struct m0_dix_ldesc *ldesc = &cid->ci_layout.u.dl_desc;

		cid->ci_fid = fom->iu_cctg_fid;
		cid->ci_layout.dl_type = DIX_LTYPE_DESCR;
		m0_dix_ldesc_init(ldesc, NULL, 0, HASH_FNC_NONE, &pv_fid);
	}

	if (fom->iu_op != ITER_UT_OP_META_LOOKUP)
		m0_dtx_init(&fom->iu_fom.fo_tx,
			    m0_fom_reqh(&fom->iu_fom)->rh_beseg->bs_domain,
			    &fom->iu_fom.fo_loc->fl_group);

	switch (fom->iu_op) {
	case ITER_UT_OP_META_INSERT:
		m0_ctg_create_credit(accum);
		break;
	case ITER_UT_OP_META_DELETE:
		m0_ctg_mark_deleted_credit(accum);
		break;
	case ITER_UT_OP_CTIDX_INSERT:
		m0_ctg_ctidx_insert_credits(&fom->iu_cid, accum);
		break;
	case ITER_UT_OP_CTIDX_DELETE:
		m0_ctg_ctidx_delete_credits(&fom->iu_cid, accum);
		break;
	case ITER_UT_OP_KV_INSERT:
		m0_ctg_insert_credit(fom->iu_ctg, fom->iu_key.b_nob,
				     fom->iu_val.b_nob, accum);
		break;
	case ITER_UT_OP_KV_DELETE:
		m0_ctg_delete_credit(fom->iu_ctg, fom->iu_key.b_nob,
				     fom->iu_val.b_nob, accum);
		break;
	case ITER_UT_OP_META_LOOKUP:
		/* Nothing to do. */
		break;
	}
}

static int iter_ut_fom_exec(struct iter_ut_fom *fom)
{
	struct m0_ctg_op      *ctg_op = &fom->iu_ctg_op;
	struct m0_be_tx       *be_tx  = &fom->iu_fom.fo_tx.tx_betx;
	int                    result;

	switch (fom->iu_op) {
	case ITER_UT_OP_META_INSERT:
		result = m0_ctg_meta_insert(ctg_op, &fom->iu_cctg_fid,
					    ITER_UT_FOM_DONE);
		M0_UT_ASSERT(result >= 0);
		break;
	case ITER_UT_OP_META_DELETE:
		result = m0_ctg_meta_delete(ctg_op, &fom->iu_cctg_fid,
					    ITER_UT_FOM_DONE);
		M0_UT_ASSERT(result >= 0);
		break;
	case ITER_UT_OP_META_LOOKUP:
		result = m0_ctg_meta_lookup(ctg_op, &fom->iu_cctg_fid,
					    ITER_UT_FOM_DONE);
		break;
	case ITER_UT_OP_CTIDX_INSERT:
		result = m0_ctg_ctidx_insert_sync(&fom->iu_cid, be_tx);
		M0_PRE(result == 0);
		m0_fom_phase_set(&fom->iu_fom, ITER_UT_FOM_DONE);
		result = M0_FSO_AGAIN;
		break;
	case ITER_UT_OP_CTIDX_DELETE:
		result = m0_ctg_ctidx_delete_sync(&fom->iu_cid, be_tx);
		M0_PRE(result == 0);
		m0_fom_phase_set(&fom->iu_fom, ITER_UT_FOM_DONE);
		result = M0_FSO_AGAIN;
		break;
	case ITER_UT_OP_KV_INSERT:
		result = m0_ctg_insert(ctg_op, fom->iu_ctg, &fom->iu_key,
				       &fom->iu_val, ITER_UT_FOM_DONE);
		break;
	case ITER_UT_OP_KV_DELETE:
		result = m0_ctg_delete(ctg_op, fom->iu_ctg, &fom->iu_key,
				       ITER_UT_FOM_DONE);
		break;
	default:
		M0_IMPOSSIBLE("Incorrect op %d", fom->iu_op);
	}
	return result;
}

static void iter_ut_fom_result(struct iter_ut_fom *fom)
{
	struct m0_ctg_op *ctg_op = &fom->iu_ctg_op;
	int               rc;

	rc = m0_ctg_op_rc(ctg_op);
	M0_UT_ASSERT(rc == 0);
	switch (fom->iu_op) {
	case ITER_UT_OP_META_INSERT:
	case ITER_UT_OP_META_DELETE:
	case ITER_UT_OP_KV_INSERT:
	case ITER_UT_OP_KV_DELETE:
	case ITER_UT_OP_CTIDX_INSERT:
	case ITER_UT_OP_CTIDX_DELETE:
		break;
	case ITER_UT_OP_META_LOOKUP:
		fom->iu_ctg = m0_ctg_meta_lookup_result(ctg_op);
	}
}

static int iter_ut_fom_tick(struct m0_fom *fom0)
{
	struct iter_ut_fom *fom = M0_AMB(fom, fom0, iu_fom);
	int                 result = M0_FSO_AGAIN;

	switch (m0_fom_phase(fom0)) {
	case ITER_UT_FOM_INIT:
		iter_ut_fom_init(fom);
		if (fom->iu_op != ITER_UT_OP_META_LOOKUP)
			m0_dtx_open(&fom0->fo_tx);
		m0_fom_phase_set(fom0, ITER_UT_FOM_INIT_WAIT);
		break;
	case ITER_UT_FOM_INIT_WAIT:
		if (fom->iu_op != ITER_UT_OP_META_LOOKUP) {
			/* XXX simplified version of fom_tx_wait() */
			M0_ASSERT(m0_be_tx_state(m0_fom_tx(fom0)) !=
				  M0_BTS_FAILED);
			if (M0_IN(m0_be_tx_state(m0_fom_tx(fom0)),
				  (M0_BTS_OPENING, M0_BTS_GROUPING))) {
				m0_fom_wait_on(fom0,
					       &m0_fom_tx(fom0)->t_sm.sm_chan,
					       &fom0->fo_cb);
				result = M0_FSO_WAIT;
				break;
			} else {
				m0_dtx_opened(&fom0->fo_tx);
			}
		}
		m0_ctg_op_init(&fom->iu_ctg_op, fom0, 0);
		if (fom->iu_op == ITER_UT_OP_CTIDX_DELETE)
			result = M0_FOM_LONG_LOCK_RETURN(m0_long_write_lock(
						 m0_ctg_del_lock(),
						 &fom->iu_del_lock_link,
						 ITER_UT_FOM_CTIDX_LOCK));
		else if (fom->iu_op == ITER_UT_OP_META_DELETE)
			result = M0_FOM_LONG_LOCK_RETURN(m0_long_write_lock(
						 m0_ctg_del_lock(),
						 &fom->iu_del_lock_link,
						 ITER_UT_FOM_META_LOCK));
		else
			m0_fom_phase_set(fom0, ITER_UT_FOM_EXEC);

		break;
	case ITER_UT_FOM_CTIDX_LOCK:
		result = M0_FOM_LONG_LOCK_RETURN(m0_long_write_lock(
						 m0_ctg_lock(m0_ctg_ctidx()),
						 &fom->iu_lock_link,
						 ITER_UT_FOM_EXEC));
		break;
	case ITER_UT_FOM_META_LOCK:
		result = M0_FOM_LONG_LOCK_RETURN(m0_long_write_lock(
						 m0_ctg_lock(m0_ctg_meta()),
						 &fom->iu_lock_link,
						 ITER_UT_FOM_EXEC));
		break;
	case ITER_UT_FOM_EXEC:
		result = iter_ut_fom_exec(fom);
		break;
	case ITER_UT_FOM_DONE:
		iter_ut_fom_result(fom);
		m0_ctg_op_fini(&fom->iu_ctg_op);
		if (fom->iu_op == ITER_UT_OP_CTIDX_DELETE) {
			m0_long_write_unlock(m0_ctg_del_lock(),
					     &fom->iu_del_lock_link);
			m0_long_write_unlock(m0_ctg_lock(m0_ctg_ctidx()),
					     &fom->iu_lock_link);
		} else if (fom->iu_op == ITER_UT_OP_META_DELETE) {
			m0_long_write_unlock(m0_ctg_del_lock(),
					     &fom->iu_del_lock_link);
			m0_long_write_unlock(m0_ctg_lock(m0_ctg_meta()),
					     &fom->iu_lock_link);
		}

		if (fom->iu_op != ITER_UT_OP_META_LOOKUP) {
			m0_dtx_done(&fom0->fo_tx);
			m0_fom_wait_on(fom0, &m0_fom_tx(fom0)->t_sm.sm_chan,
				       &fom0->fo_cb);
			m0_fom_phase_set(fom0, ITER_UT_FOM_TX_COMMIT);
		} else {
			m0_fom_phase_set(fom0, ITER_UT_FOM_FINAL);
		}
		result = M0_FSO_WAIT;
		break;
	case ITER_UT_FOM_TX_COMMIT:
		if (m0_be_tx_state(m0_fom_tx(fom0)) == M0_BTS_DONE) {
			m0_dtx_fini(&fom0->fo_tx);
			m0_fom_phase_set(fom0, ITER_UT_FOM_FINAL);
		} else {
			m0_fom_wait_on(fom0, &m0_fom_tx(fom0)->t_sm.sm_chan,
				       &fom0->fo_cb);
		}
		result = M0_FSO_WAIT;
		break;
	}
	return result;
}

static const struct m0_fom_ops iter_ut_fom_ops = {
	.fo_fini          = iter_ut_fom_fini,
	.fo_tick          = iter_ut_fom_tick,
	.fo_home_locality = iter_ut_fom_locality
};

static const struct m0_fom_type_ops iter_ut_fom_type_ops = {
	.fto_create = NULL
};

static void iter_ut_fom_op_sync(struct iter_ut_fom *fom)
{
	struct m0_semaphore sem;

	m0_semaphore_init(&sem, 0);
	m0_fom_init(&fom->iu_fom, &ut_fom_type, &iter_ut_fom_ops, NULL, NULL,
		    &reqh);
	fom->iu_sem = &sem;
	m0_fom_queue(&fom->iu_fom);
	m0_semaphore_down(&sem);
	m0_semaphore_fini(&sem);
}

static void iter_ut_fom_op(struct iter_ut_fom *fom, struct m0_semaphore *sem)
{
	m0_fom_init(&fom->iu_fom, &ut_fom_type, &iter_ut_fom_ops, NULL,
		    NULL, &reqh);
	fom->iu_sem = sem;
	m0_fom_queue(&fom->iu_fom);
}

static void iter_ut_meta_insert(struct m0_fid *cctg_fid)
{
	struct iter_ut_fom fom;

	M0_SET0(&fom);
	fom.iu_cctg_fid = *cctg_fid;
	fom.iu_op = ITER_UT_OP_META_INSERT;
	iter_ut_fom_op_sync(&fom);
}

static struct m0_cas_ctg *iter_ut_meta_lookup(struct m0_fid *cctg_fid)
{
	struct iter_ut_fom fom;

	M0_SET0(&fom);
	fom.iu_cctg_fid = *cctg_fid;
	fom.iu_op = ITER_UT_OP_META_LOOKUP;
	iter_ut_fom_op_sync(&fom);
	return fom.iu_ctg;
}

static void iter_ut_ctidx_insert(struct m0_fid *cctg_fid)
{
	struct iter_ut_fom fom;

	M0_SET0(&fom);
	fom.iu_cctg_fid = *cctg_fid;
	fom.iu_op = ITER_UT_OP_CTIDX_INSERT;
	iter_ut_fom_op_sync(&fom);
}

static void iter_ut_meta_delete_async(struct iter_ut_fom *fom,
				      struct m0_fid *cctg_fid,
				      struct m0_semaphore *sem)
{
	M0_SET0(fom);
	fom->iu_cctg_fid = *cctg_fid;
	fom->iu_op = ITER_UT_OP_META_DELETE;
	iter_ut_fom_op(fom, sem);
}

static void iter_ut_ctidx_delete_async(struct iter_ut_fom *fom,
				       struct m0_fid *cctg_fid,
				       struct m0_semaphore *sem)
{
	M0_SET0(fom);
	fom->iu_cctg_fid = *cctg_fid;
	fom->iu_op = ITER_UT_OP_CTIDX_DELETE;
	iter_ut_fom_op(fom, sem);
}
static void iter_ut_op(struct m0_cas_ctg *cctg, uint64_t key, uint64_t val, enum iter_ut_fom_op op)
{
	struct iter_ut_fom fom;
	struct m0_buf      kbuf = {};
	struct m0_buf      vbuf = {};
	int                rc;

	M0_SET0(&fom);
	rc = m0_buf_alloc(&kbuf, sizeof(key));
	     m0_buf_alloc(&vbuf, sizeof(val));
	M0_UT_ASSERT(rc == 0);
	*(uint64_t *)kbuf.b_addr = htobe64(key);
	*(uint64_t *)vbuf.b_addr = htobe64(val);
	fom.iu_ctg = cctg;
	fom.iu_key = kbuf;
	fom.iu_val = vbuf;
	fom.iu_op = op;
	iter_ut_fom_op_sync(&fom);
	m0_buf_free(&kbuf);
	m0_buf_free(&vbuf);
}

static void iter_ut_insert(struct m0_cas_ctg *cctg, uint64_t key, uint64_t val)
{
	iter_ut_op(cctg, key, val, ITER_UT_OP_KV_INSERT);
}

static void iter_ut_delete(struct m0_cas_ctg *cctg, uint64_t key, uint64_t val)
{
	iter_ut_op(cctg, key, val, ITER_UT_OP_KV_DELETE);
}

static void device_state_set(uint64_t pool_device_id,
			     int      dev_state)
{
	struct m0_pooldev        *pdev;
	struct m0_poolmach_state *state;

	state = pv.pv_mach.pm_state;
	pdev = &state->pst_devices_array[pool_device_id];
	pdev->pd_state = dev_state;
}

static void device_state_set_op(uint64_t pool_device_id,
				enum m0_pool_nd_state dev_state)
{
	struct m0_pool_spare_usage *spare_usage_array;
	struct m0_pool_spare_usage *spare_usage_item;

	if ( pool_device_id != POOL_PM_SPARE_SLOT_UNUSED)
		device_state_set(pool_device_id, dev_state);

	spare_usage_array = pv.pv_mach.pm_state->pst_spare_usage_array;
	spare_usage_item = &spare_usage_array[spare_usage_pos];
	spare_usage_item->psu_device_index = (uint32_t)pool_device_id;
	spare_usage_item->psu_device_state = dev_state;
	spare_usage_pos++;
}

static void device_repaired_set(uint64_t pool_device_id)
{
	device_state_set_op(pool_device_id, M0_PNDS_SNS_REPAIRED);
}

static void device_repairing_set(uint64_t pool_device_id)
{
	device_state_set_op(pool_device_id, M0_PNDS_SNS_REPAIRING);
}

static void device_rebalancing_set(uint64_t pool_device_id)
{
	device_state_set_op(pool_device_id, M0_PNDS_SNS_REBALANCING);
}

static void spare_slot_unused_set()
{
	device_state_set_op(POOL_PM_SPARE_SLOT_UNUSED, 0);
}

static void iter_ut_devs_setup()
{
	uint32_t                  i;
	struct m0_pooldev        *pdev;
	struct m0_poolmach_state *state;

	state = pv.pv_mach.pm_state;
	for (i = 0; i < state->pst_nr_devices; i++) {
		pdev = &state->pst_devices_array[i];
		pdev->pd_state = M0_PNDS_ONLINE;
		pdev->pd_id = M0_FID_TINIT('d', 1, i);
		pdev->pd_sdev_idx = i + DEVS_ID_SHIFT;
		pdev->pd_index = i;
	}
}

static void iter_ut_pool_init()
{
	int result;
	struct m0_fid p_fid  = { .f_key = 1 };

	result = m0_pool_init(&pool, &p_fid, 0);
	M0_UT_ASSERT(result == 0);
	result = m0_pool_version_init(&pv,
				      &pv_fid,
				      &pool,
				      POOL_WIDTH,
				      NODES,
				      DATA_NR,
				      FAILURES_NR,
				      SPARE_NR);
	M0_UT_ASSERT(result == 0);
	iter_ut_devs_setup();
	/** @todo Ugly workaround to do finalisation successfully. */
	m0_clink_init(&pv.pv_mach.pm_state->pst_conf_exp.bc_u.clink, NULL);
	m0_clink_init(&pv.pv_mach.pm_state->pst_conf_ready.bc_u.clink, NULL);
	pool_version_tlist_add(&pool.po_vers, &pv);
	pools_tlist_init(&pc.pc_pools);
	pools_tlink_init_at_tail(&pool, &pc.pc_pools);
}

static void iter_ut_reqh_init(void)
{
	int result;

	M0_SET0(&reqh);
	M0_SET0(&be);
	seg0 = m0_be_domain_seg0_get(&be.but_dom);
	iter_ut_pool_init();
	result = M0_REQH_INIT(&reqh,
			      .rhia_db      = seg0,
			      .rhia_mdstore = (void *)1,
			      .rhia_fid     = &g_process_fid,
			      .rhia_pc      = &pc
		);
	M0_UT_ASSERT(result == 0);
	result = m0_layout_init_by_pver(&reqh.rh_ldom, &pv, NULL);
	M0_UT_ASSERT(result == 0);
	be.but_dom_cfg.bc_engine.bec_reqh = &reqh;
	m0_be_ut_backend_init(&be);
}


static void iter_ut_init(struct m0_reqh_service      **svc,
			 struct m0_reqh_service_type  *stype)
{
	int result;

	spare_usage_pos = 0;

	m0_fi_enable("cas_in_ut", "ut");

	iter_ut_reqh_init();
	result = m0_reqh_service_allocate(svc, stype,
					  NULL);
	M0_UT_ASSERT(result == 0);
	m0_reqh_service_init(*svc, &reqh, NULL);
	m0_reqh_service_start(*svc);
	m0_reqh_start(&reqh);
	m0_ctg_store_init(&be.but_dom);

	if (stype == &dix_repair_cmt.ct_stype) {
		/* If a fom type called m0_fom_type_init() once,
		 * it has already registered. And another call
		 * won't change its type. So we have to use another
		 * variable to switch between different fom types.
		 */
		m0_fom_type_init(&ut_fom_type_rep,
				 M0_CM_DIX_REP_ITER_UT_OPCODE,
				 &iter_ut_fom_type_ops, stype,
				 &iter_ut_fom_conf);
		ut_fom_type = ut_fom_type_rep;
	} else {
		m0_fom_type_init(&ut_fom_type_reb,
				 M0_CM_DIX_REB_ITER_UT_OPCODE,
				 &iter_ut_fom_type_ops, stype,
				 &iter_ut_fom_conf);
		ut_fom_type = ut_fom_type_reb;
	}
}

static void iter_ut_pool_fini()
{
	pools_tlink_del_fini(&pool);
	pools_tlist_fini(&pc.pc_pools);
	pool_version_tlist_del(&pv);
	m0_pool_version_fini(&pv);
	m0_pool_fini(&pool);
}

static void iter_ut_reqh_fini()
{
	m0_reqh_layouts_cleanup(&reqh);
	m0_layout_domain_cleanup(&reqh.rh_ldom);
	iter_ut_pool_fini();
}

static void iter_ut_fini(struct m0_reqh_service *svc)
{
	m0_ctg_store_fini();
	m0_reqh_service_prepare_to_stop(svc);
	m0_reqh_idle_wait_for(&reqh, svc);
	m0_reqh_service_stop(svc);
	m0_reqh_service_fini(svc);
	iter_ut_reqh_fini();
	m0_be_ut_backend_fini(&be);
	m0_fi_disable("cas_in_ut", "ut");
}

static void start_stop(void)
{
	struct m0_dix_cm_iter *iter;
	int                    rc;

	iter_ut_init(&repair_svc, &dix_repair_cmt.ct_stype);
	iter = iter_ut_iter(repair_svc);
	M0_SET0(iter);
	rc = m0_dix_cm_iter_start(iter, &dix_repair_dcmt, &reqh, RPC_CUTOFF);
	M0_ASSERT(rc == 0);
	m0_dix_cm_iter_stop(iter);
	iter_ut_fini(repair_svc);
}

static int iter_ut_next_sync(struct m0_dix_cm_iter *iter,
			     struct m0_buf         *key,
			     struct m0_buf         *val,
			     uint32_t              *sdev_id)
{
	struct m0_clink clink;

	M0_SET0(key);
	M0_SET0(val);

	m0_clink_init(&clink, NULL);
	m0_clink_add_lock(&iter->di_completed, &clink);
	m0_dix_cm_iter_next(iter);
	m0_chan_wait(&clink);
	m0_clink_del_lock(&clink);
	m0_clink_fini(&clink);
	return m0_dix_cm_iter_get(iter, key, val, sdev_id);
}

static void empty_store(void)
{
	struct m0_dix_cm_iter *iter;
	struct m0_buf          key;
	struct m0_buf          val;
	uint32_t               sdev_id;
	int                    rc;

	iter_ut_init(&repair_svc, &dix_repair_cmt.ct_stype);
	iter = iter_ut_iter(repair_svc);
	M0_SET0(iter);
	rc = m0_dix_cm_iter_start(iter, &dix_repair_dcmt, &reqh, RPC_CUTOFF);
	M0_ASSERT(rc == 0);
	rc = iter_ut_next_sync(iter, &key, &val, &sdev_id);
	M0_ASSERT(rc == -ENODATA);
	m0_dix_cm_iter_stop(iter);
	iter_ut_fini(repair_svc);
}

static void empty_cctg(void)
{
	struct m0_dix_cm_iter *iter;
	struct m0_fid          cctg_fid0 = M0_FID_TINIT('T', 1, 0);
	struct m0_fid          cctg_fid1 = M0_FID_TINIT('T', 1, 1);
	struct m0_buf          key;
	struct m0_buf          val;
	uint32_t               sdev_id;
	int                    rc;

	iter_ut_init(&repair_svc, &dix_repair_cmt.ct_stype);
	iter_ut_meta_insert(&cctg_fid0);
	iter_ut_ctidx_insert(&cctg_fid0);
	iter_ut_meta_insert(&cctg_fid1);
	iter_ut_ctidx_insert(&cctg_fid1);
	iter = iter_ut_iter(repair_svc);
	M0_SET0(iter);
	rc = m0_dix_cm_iter_start(iter, &dix_repair_dcmt, &reqh, RPC_CUTOFF);
	M0_ASSERT(rc == 0);
	rc = iter_ut_next_sync(iter, &key, &val, &sdev_id);
	M0_ASSERT(rc == -ENODATA);
	m0_dix_cm_iter_stop(iter);
	iter_ut_fini(repair_svc);
}

static void cctg_not_found(void)
{
	struct m0_dix_cm_iter *iter;
	struct m0_fid          cctg_fid = M0_FID_TINIT('T', 1, 0);
	struct m0_buf          key;
	struct m0_buf          val;
	uint32_t               sdev_id;
	int                    rc;

	iter_ut_init(&repair_svc, &dix_repair_cmt.ct_stype);
	iter = iter_ut_iter(repair_svc);
	/*
	 * Don't insert component catalogue into meta.
	 * Iterator should return error.
	 */
	iter_ut_ctidx_insert(&cctg_fid);
	M0_SET0(iter);
	rc = m0_dix_cm_iter_start(iter, &dix_repair_dcmt, &reqh, RPC_CUTOFF);
	M0_ASSERT(rc == 0);
	rc = iter_ut_next_sync(iter, &key, &val, &sdev_id);
	M0_ASSERT(rc == -ENOENT);
	m0_dix_cm_iter_stop(iter);
	iter_ut_fini(repair_svc);
}

static uint64_t buf_value(const struct m0_buf *buf)
{
	return be64toh(*(uint64_t *)buf->b_addr);
}

static void check_dix_iter_key_val(struct m0_dix_cm_iter *iter, int exp_rc,
				   uint64_t exp_key, uint64_t exp_val,
				   uint32_t exp_sdev)
{
	struct m0_buf key;
	struct m0_buf val;
	uint32_t      sdev_id;
	int           rc;

	rc = iter_ut_next_sync(iter, &key, &val, &sdev_id);
	M0_ASSERT(rc == exp_rc);
	if (exp_rc == 0) {
		M0_UT_ASSERT(buf_value(&key) == exp_key);
		M0_UT_ASSERT(buf_value(&val) == exp_val);
		if (exp_sdev != 0)
			M0_UT_ASSERT(sdev_id == exp_sdev);
		m0_buf_free(&key);
		m0_buf_free(&val);
	}
}

static void test_dix_rec(uint32_t cctg_count, uint32_t rec_count)
{
	struct m0_dix_cm_iter *iter;
	struct m0_fid          cctg_fid;
	struct m0_cas_ctg     *cctg;
	int                    i;
	int                    j;
	int                    rc;

	iter_ut_init(&repair_svc, &dix_repair_cmt.ct_stype);
	iter = iter_ut_iter(repair_svc);
	for (i = 0; i < cctg_count; i++) {
		cctg_fid = M0_FID_TINIT('T', 1, i);
		iter_ut_ctidx_insert(&cctg_fid);
		iter_ut_meta_insert(&cctg_fid);
		cctg = iter_ut_meta_lookup(&cctg_fid);
		for (j = 0; j < rec_count; j++)
			iter_ut_insert(cctg, 100 * i + j, j * j);
	}
	M0_SET0(iter);
	rc = m0_dix_cm_iter_start(iter, &dix_repair_dcmt, &reqh, RPC_CUTOFF);
	M0_ASSERT(rc == 0);
	for (i = 0; i < cctg_count; i++) {
		for (j = 0; j < rec_count; j++) {
			m0_fi_enable_once("dix_cm_is_repair_coordinator",
					  "always_coordinator");
			m0_fi_enable_once("dix_cm_repair_tgts_get",
					  "single_target");
			check_dix_iter_key_val(iter, 0, (100 * i + j), (j * j), 0);
		}
	}
	m0_fi_enable_once("dix_cm_is_repair_coordinator", "always_coordinator");
	m0_fi_enable_once("dix_cm_repair_tgts_get", "single_target");
	check_dix_iter_key_val(iter, -ENODATA, 0, 0, 0);
	m0_dix_cm_iter_stop(iter);
	iter_ut_fini(repair_svc);
	m0_fi_disable("dix_cm_is_repair_coordinator", "always_coordinator");
	m0_fi_disable("dix_cm_repair_tgts_get", "single_target");
}

static void one_rec(void)
{
	test_dix_rec(1, 1);
}

static void multi_rec(void)
{
	test_dix_rec(10, 20);
}

static void iter_ut_insert_cctg_fid(struct m0_fid *cctg_fid,
				    uint32_t container,
				    uint64_t key,
				    uint32_t device_id)
{
	struct m0_fid dix_fid;

	m0_dix_fid_dix_make(&dix_fid, container, key);
	m0_dix_fid_convert_dix2cctg(&dix_fid,
				    cctg_fid,
				    device_id);
	iter_ut_ctidx_insert(cctg_fid);
	iter_ut_meta_insert(cctg_fid);
}

static struct m0_cas_ctg *iter_ut_insert_lookup_cctg(uint32_t container,
						     uint64_t key,
						     uint32_t device_id)
{
	struct m0_cas_ctg *cctg;
	struct m0_fid      cctg_fid;

	iter_ut_insert_cctg_fid(&cctg_fid, container, key, device_id);
	cctg = iter_ut_meta_lookup(&cctg_fid);
	return cctg;
}

static void rep_coordinator(void)
{
	struct m0_dix_cm_iter *iter;
	struct m0_cas_ctg     *cctg;
	int                    rc;

	m0_fi_enable("m0_dix_target", "pdcluster-map");
/*
 * Parity group info for key 10 (pool dev_id/global dev_id):
 * =============================================
 * 0: dev_id: 4/104,   data, online, (local)
 * 1: dev_id: 9/109, parity, online
 * 2: dev_id: 6/106, parity, online
 * 3: dev_id: 5/105, parity, online
 * 4: dev_id: 1/101,  spare, online
 * 5: dev_id: 0/100,  spare, online
 * 6: dev_id: 7/107,  spare, online
 * =============================================
 */
	/* A coordinator, the first in the parity group. */
	iter_ut_init(&repair_svc, &dix_repair_cmt.ct_stype);
	iter = iter_ut_iter(repair_svc);

	cctg = iter_ut_insert_lookup_cctg(1, 0, 104);
	iter_ut_insert(cctg, 10, 20);
	M0_SET0(iter);
	rc = m0_dix_cm_iter_start(iter, &dix_repair_dcmt, &reqh, RPC_CUTOFF);
	M0_ASSERT(rc == 0);
	m0_fi_enable_once("dix_cm_iter_next_key", "print_parity_group");
	m0_fi_enable_once("dix_cm_repair_tgts_get", "single_target");
	check_dix_iter_key_val(iter, 0, 10, 20, 0);
	check_dix_iter_key_val(iter, -ENODATA, 0, 0, 0);
	m0_dix_cm_iter_stop(iter);
	iter_ut_fini(repair_svc);

/*
 * Parity group info for key 10 (pool dev_id/global dev_id):
 * =============================================
 * 0: dev_id: 4/104,   data, failed
 * 1: dev_id: 9/109, parity, failed
 * 2: dev_id: 6/106, parity, online, (local)
 * 3: dev_id: 5/105, parity, online
 * 4: dev_id: 1/101,  spare, online
 * 5: dev_id: 0/100,  spare, online
 * 6: dev_id: 7/107,  spare, online
 * =============================================
 */
	/* A coordinator, not the first in the parity group. */
	iter_ut_init(&repair_svc, &dix_repair_cmt.ct_stype);
	iter = iter_ut_iter(repair_svc);

	cctg = iter_ut_insert_lookup_cctg(1, 0, 106);
	iter_ut_insert(cctg, 10, 20);
	device_state_set(4, M0_PNDS_FAILED);
	device_state_set(9, M0_PNDS_FAILED);
	M0_SET0(iter);
	rc = m0_dix_cm_iter_start(iter, &dix_repair_dcmt, &reqh, RPC_CUTOFF);
	M0_ASSERT(rc == 0);
	m0_fi_enable_once("dix_cm_iter_next_key", "print_parity_group");
	m0_fi_enable_once("dix_cm_repair_tgts_get", "single_target");
	check_dix_iter_key_val(iter, 0, 10, 20, 0);
	check_dix_iter_key_val(iter, -ENODATA, 0, 0, 0);
	m0_dix_cm_iter_stop(iter);
	iter_ut_fini(repair_svc);

/*
 * Parity group info for key 10 (pool dev_id/global dev_id):
 * =============================================
 * 0: dev_id: 4/104,   data, online
 * 1: dev_id: 9/109, parity, online, (local)
 * 2: dev_id: 6/106, parity, online
 * 3: dev_id: 5/105, parity, online
 * 4: dev_id: 1/101,  spare, online
 * 5: dev_id: 0/100,  spare, online
 * 6: dev_id: 7/107,  spare, online
 * =============================================
 */
	/* Not a coordinator, not the first in the parity group. */
	iter_ut_init(&repair_svc, &dix_repair_cmt.ct_stype);
	iter = iter_ut_iter(repair_svc);

	cctg = iter_ut_insert_lookup_cctg(1, 0, 109);
	iter_ut_insert(cctg, 10, 20);
	M0_SET0(iter);
	rc = m0_dix_cm_iter_start(iter, &dix_repair_dcmt, &reqh, RPC_CUTOFF);
	M0_ASSERT(rc == 0);
	m0_fi_enable_once("dix_cm_iter_next_key", "print_parity_group");
	m0_fi_enable_once("dix_cm_repair_tgts_get", "single_target");
	check_dix_iter_key_val(iter, -ENODATA, 0, 0, 0);
	m0_dix_cm_iter_stop(iter);
	iter_ut_fini(repair_svc);
	m0_fi_disable("dix_cm_iter_next_key", "print_parity_group");
	m0_fi_disable("dix_cm_repair_tgts_get", "single_target");

/*
 * Parity group info for key 10 (pool dev_id/global dev_id):
 * =============================================
 * 0: dev_id: 4/104,   data, failed
 * 1: dev_id: 9/109, parity, failed
 * 2: dev_id: 6/106, parity, failed
 * 3: dev_id: 5/105, parity, failed
 * 4: dev_id: 1/101,  spare, online, (local)
 * 5: dev_id: 0/100,  spare, online
 * 6: dev_id: 7/107,  spare, online
 * =============================================
 */
	/* Not a coordinator, serves spare unit. */
	iter_ut_init(&repair_svc, &dix_repair_cmt.ct_stype);
	iter = iter_ut_iter(repair_svc);

	cctg = iter_ut_insert_lookup_cctg(1, 0, 101);
	iter_ut_insert(cctg, 10, 20);
	device_state_set(4, M0_PNDS_FAILED);
	device_state_set(9, M0_PNDS_FAILED);
	device_state_set(6, M0_PNDS_FAILED);
	device_state_set(5, M0_PNDS_FAILED);
	M0_SET0(iter);
	rc = m0_dix_cm_iter_start(iter, &dix_repair_dcmt, &reqh, RPC_CUTOFF);
	M0_ASSERT(rc == 0);
	m0_fi_enable_once("dix_cm_iter_next_key", "print_parity_group");
	m0_fi_enable_once("dix_cm_repair_tgts_get", "single_target");
	check_dix_iter_key_val(iter, -ENODATA, 0, 0, 0);
	m0_dix_cm_iter_stop(iter);
	iter_ut_fini(repair_svc);
	m0_fi_disable("dix_cm_iter_next_key", "print_parity_group");
	m0_fi_disable("dix_cm_repair_tgts_get", "single_target");
}

static void reset_parity_grp_info(void)
{
	uint32_t i;

	for (i = 0; i < NUM_DATA_PARITY_DEV_IDS; i++)
		data_parity_dev_ids[i].state = ITER_UT_ONLINE;

	for (i = 0; i < NUM_SPARE_DEV_IDS; i++)
		spare_dev_ids[i].state = ITER_UT_ONLINE;
	spare_idx = 0;
}

static void iter_ut_set_drive_state(uint64_t pool_device_id,
				    enum iter_ut_pool_nd_state state)
{
	uint32_t i;

	for (i = 0; i < NUM_SPARE_DEV_IDS; i++) {
		if (spare_dev_ids[i].pool_dev_id == pool_device_id) {
			spare_dev_ids[i].state = state;
			spare_dev_ids[i].spare_idx = spare_idx;
			break;
		}
	}

	if (i == NUM_SPARE_DEV_IDS)
		for (i = 0; i < NUM_DATA_PARITY_DEV_IDS; i++) {
			if (data_parity_dev_ids[i].pool_dev_id == pool_device_id) {
				data_parity_dev_ids[i].state = state;
				data_parity_dev_ids[i].spare_idx = spare_idx;
				break;
			}
		}

	switch (state)
	{
		case ITER_UT_REPAIRING:
			device_repairing_set(pool_device_id);
			break;
		case ITER_UT_REPAIRED:
			device_repaired_set(pool_device_id);
			break;
		case ITER_UT_REBALANCING:
			device_rebalancing_set(pool_device_id);
			break;
		default:
			break;
	}
	spare_idx++;
}

static void iter_ut_set_drive_repairing(uint64_t pool_device_id)
{
	iter_ut_set_drive_state(pool_device_id, ITER_UT_REPAIRING);
}

static void iter_ut_set_drive_repaired(uint64_t pool_device_id)
{
	iter_ut_set_drive_state(pool_device_id, ITER_UT_REPAIRED);
}

// TODO: For future use
// static void iter_ut_set_drive_rebalancing(uint64_t pool_device_id)
// {
// 	iter_ut_set_drive_state(pool_device_id, ITER_UT_REBALANCING);
// }

static void iter_ut_rep_validate(struct m0_dix_cm_iter *iter,
				 uint64_t exp_key,
				 uint64_t exp_val)
{
	enum iter_ut_pool_nd_state  data_parity_dev_state;
	struct iter_ut_dev_id      *curr_dev_id;
	uint32_t                    spare_idx;
	uint32_t                    target_dev_id = INVALID_IDX;
	uint32_t                    i;
	int                         rc;

	M0_SET0(iter);
	rc = m0_dix_cm_iter_start(iter, &dix_repair_dcmt, &reqh, RPC_CUTOFF);
	M0_UT_ASSERT(rc == 0);
	m0_fi_enable_once("dix_cm_iter_next_key", "print_parity_group");
	m0_fi_enable_once("dix_cm_iter_next_key", "print_spare_usage");
	m0_fi_enable_once("dix_cm_iter_next_key", "print_targets");

	for (i = 0; i < NUM_DATA_PARITY_DEV_IDS; i++) {
		curr_dev_id = &data_parity_dev_ids[i];
		data_parity_dev_state = curr_dev_id->state;

		if (data_parity_dev_state != ITER_UT_ONLINE) {
			spare_idx = curr_dev_id->spare_idx;
			curr_dev_id = &spare_dev_ids[spare_idx];
			if ((data_parity_dev_state == ITER_UT_REPAIRED) &&
			    (curr_dev_id->state == ITER_UT_ONLINE))
				continue;
			else {
				while (curr_dev_id->state != ITER_UT_ONLINE) {
					spare_idx = curr_dev_id->spare_idx;
					curr_dev_id = &spare_dev_ids[spare_idx];
				}
				target_dev_id = spare_dev_ids[spare_idx].global_dev_id;
				M0_UT_ASSERT(target_dev_id != INVALID_IDX);
				check_dix_iter_key_val(iter, 0, exp_key,
						       exp_val, target_dev_id);
			}
		}
	}
	check_dix_iter_key_val(iter, -ENODATA, 0, 0, 0);
	m0_fi_disable("dix_cm_iter_next_key", "print_parity_group");
	m0_fi_disable("dix_cm_iter_next_key", "print_spare_usage");
	m0_fi_disable("dix_cm_iter_next_key", "print_targets");
}

static void one_dev_fail(void)
{
	struct m0_dix_cm_iter *iter;
	struct m0_cas_ctg     *cctg;

/*
 * Parity group info for key 10 (pool dev_id/global dev_id):
 * =============================================
 * 0: dev_id: 4/104,   data, repairing
 * 1: dev_id: 9/109, parity, online, (local)
 * 2: dev_id: 6/106, parity, online
 * 3: dev_id: 5/105, parity, online
 * 4: dev_id: 1/101,  spare, online
 * 5: dev_id: 0/100,  spare, online
 * 6: dev_id: 7/107,  spare, online
 * =============================================
 *
 * Spare usage array info (pool dev_id/global dev_id):
 * =============================================
 * 0: dev_id: 4/104, repairing, served by:
 *    dev_id: 1/101, online
 * 1: slot unused, served by:
 *    dev_id: 0/100, online
 * 2: slot unused, served by:
 *    dev_id: 7/107, online
 * =============================================
 *
 * Targets for key 10 (pool dev_id/global dev_id):
 * =============================================
 * 1: dev_id: 1/101
 * =============================================
 */

	iter_ut_init(&repair_svc, &dix_repair_cmt.ct_stype);
	iter = iter_ut_iter(repair_svc);
	cctg = iter_ut_insert_lookup_cctg(1, 0, 109);
	iter_ut_insert(cctg, 10, 20);
	reset_parity_grp_info();
	iter_ut_set_drive_repairing(4);
	iter_ut_rep_validate(iter, 10, 20);
	m0_dix_cm_iter_stop(iter);
	iter_ut_fini(repair_svc);
}

static void two_devs_fail(void)
{
	struct m0_dix_cm_iter *iter;
	struct m0_cas_ctg     *cctg;

/*
 * Parity group info for key 10 (pool dev_id/global dev_id):
 * =============================================
 * 0: dev_id: 4/104,   data, repairing
 * 1: dev_id: 9/109, parity, online, (local)
 * 2: dev_id: 6/106, parity, online
 * 3: dev_id: 5/105, parity, repairing
 * 4: dev_id: 1/101,  spare, online
 * 5: dev_id: 0/100,  spare, online
 * 6: dev_id: 7/107,  spare, online
 * =============================================
 *
 * Spare usage array info (pool dev_id/global dev_id):
 * =============================================
 * 0: dev_id: 4/104, repairing, served by:
 *    dev_id: 1/101, online
 * 1: dev_id: 5/105, repairing, served by:
 *    dev_id: 0/100, online
 * 2: slot unused, served by:
 *    dev_id: 7/107, online
 * =============================================
 *
 * Targets for key 10 (pool dev_id/global dev_id):
 * =============================================
 * 1: dev_id: 1/101
 * 2: dev_id: 0/100
 * =============================================
 */

	iter_ut_init(&repair_svc, &dix_repair_cmt.ct_stype);
	iter = iter_ut_iter(repair_svc);
	cctg = iter_ut_insert_lookup_cctg(1, 0, 109);
	iter_ut_insert(cctg, 10, 20);
	reset_parity_grp_info();
	iter_ut_set_drive_repairing(4);
	iter_ut_set_drive_repairing(5);
	iter_ut_rep_validate(iter, 10, 20);
	m0_dix_cm_iter_stop(iter);
	iter_ut_fini(repair_svc);
}

static void outside_dev_fail(void)
{
	struct m0_dix_cm_iter *iter;
	struct m0_cas_ctg     *cctg;

/*
 * Parity group info for key 10 (pool dev_id/global dev_id):
 * =============================================
 * 0: dev_id: 4/104,   data, online, (local)
 * 1: dev_id: 9/109, parity, online
 * 2: dev_id: 6/106, parity, online
 * 3: dev_id: 5/105, parity, online
 * 4: dev_id: 1/101,  spare, online
 * 5: dev_id: 0/100,  spare, online
 * 6: dev_id: 7/107,  spare, online
 * =============================================
 *
 * Spare usage array info (pool dev_id/global dev_id):
 * =============================================
 * 0: dev_id: 3/103, repairing, served by:
 *    dev_id: 1/101, online
 * 1: slot unused, served by:
 *    dev_id: 0/100, online
 * 2: slot unused, served by:
 *    dev_id: 7/107, online
 * =============================================
 */
	iter_ut_init(&repair_svc, &dix_repair_cmt.ct_stype);
	iter = iter_ut_iter(repair_svc);
	cctg = iter_ut_insert_lookup_cctg(1, 0, 104);
	iter_ut_insert(cctg, 10, 20);
	reset_parity_grp_info();
	iter_ut_set_drive_repairing(3);
	iter_ut_rep_validate(iter, 10, 20);
	m0_dix_cm_iter_stop(iter);
	iter_ut_fini(repair_svc);

/*
 * Parity group info for key 10 (pool dev_id/global dev_id):
 * =============================================
 * 0: dev_id: 4/104,   data, online, (local)
 * 1: dev_id: 9/109, parity, repairing
 * 2: dev_id: 6/106, parity, online
 * 3: dev_id: 5/105, parity, online
 * 4: dev_id: 1/101,  spare, online
 * 5: dev_id: 0/100,  spare, online
 * 6: dev_id: 7/107,  spare, online
 * =============================================
 *
 * Spare usage array info (pool dev_id/global dev_id):
 * =============================================
 * 0: dev_id: 3/103, repairing, served by:
 *    dev_id: 1/101, online
 * 1: dev_id: 9/109, repairing, served by:
 *    dev_id: 0/100, online
 * 2: slot unused, served by:
 *    dev_id: 7/107, online
 * =============================================
 *
 * Targets for key 10 (pool dev_id/global dev_id):
 * =============================================
 * 1: dev_id: 0/100
 * =============================================
 */
	iter_ut_init(&repair_svc, &dix_repair_cmt.ct_stype);
	iter = iter_ut_iter(repair_svc);
	cctg = iter_ut_insert_lookup_cctg(1, 0, 104);
	iter_ut_insert(cctg, 10, 20);
	reset_parity_grp_info();
	iter_ut_set_drive_repairing(3);
	iter_ut_set_drive_repairing(9);
	iter_ut_rep_validate(iter, 10, 20);
	m0_dix_cm_iter_stop(iter);
	iter_ut_fini(repair_svc);

/*
 * Parity group info for key 10 (pool dev_id/global dev_id):
 * =============================================
 * 0: dev_id: 4/104, data, online, (local)
 * 1: dev_id: 9/109, parity, repairing
 * 2: dev_id: 6/106, parity, online
 * 3: dev_id: 5/105, parity, online
 * 4: dev_id: 1/101, spare, repairing
 * 5: dev_id: 0/100, spare, online
 * 6: dev_id: 7/107, spare, online
 * =============================================
 *
 * Spare usage array info (pool dev_id/global dev_id):
 * =============================================
 * 0: dev_id: 3/103, repaired, served by:
 *    dev_id: 1/101, repairing
 * 1: dev_id: 1/101, repairing, served by:
 *    dev_id: 0/100, online
 * 2: dev_id: 9/109, repairing, served by:
 *    dev_id: 7/107, online
 * =============================================
 *
 * Targets for key 10 (pool dev_id/global dev_id):
 * =============================================
 * 1: dev_id: 7/107
 * =============================================
 */
	iter_ut_init(&repair_svc, &dix_repair_cmt.ct_stype);
	iter = iter_ut_iter(repair_svc);
	cctg = iter_ut_insert_lookup_cctg(1, 0, 104);
	iter_ut_insert(cctg, 10, 20);
	reset_parity_grp_info();
	iter_ut_set_drive_repaired(3);
	iter_ut_set_drive_repairing(1);
	iter_ut_set_drive_repairing(9);
	iter_ut_rep_validate(iter, 10, 20);
	m0_dix_cm_iter_stop(iter);
	iter_ut_fini(repair_svc);
}

static void empty_spare_fail(void)
{
	struct m0_dix_cm_iter *iter;
	struct m0_cas_ctg     *cctg;

/*
 * Parity group info for key 10 (pool dev_id/global dev_id):
 * =============================================
 * 0: dev_id: 4/104,   data, online, (local)
 * 1: dev_id: 9/109, parity, online
 * 2: dev_id: 6/106, parity, online
 * 3: dev_id: 5/105, parity, online
 * 4: dev_id: 1/101,  spare, online
 * 5: dev_id: 0/100,  spare, repairing
 * 6: dev_id: 7/107,  spare, online
 * =============================================
 *
 * Spare usage array info (pool dev_id/global dev_id):
 * =============================================
 * 0: dev_id: 0/100, repairing, served by:
 *    dev_id: 1/101, online
 * 1: slot unused, served by:
 *    dev_id: 0/100, repairing
 * 2: slot unused, served by:
 *    dev_id: 7/107, online
 * =============================================
 */
	iter_ut_init(&repair_svc, &dix_repair_cmt.ct_stype);
	iter = iter_ut_iter(repair_svc);
	cctg = iter_ut_insert_lookup_cctg(1, 0, 104);
	iter_ut_insert(cctg, 10, 20);
	reset_parity_grp_info();
	iter_ut_set_drive_repairing(0);
	iter_ut_rep_validate(iter, 10, 20);
	m0_dix_cm_iter_stop(iter);
	iter_ut_fini(repair_svc);

/*
 * Parity group info for key 10 (pool dev_id/global dev_id):
 * =============================================
 * 0: dev_id: 4/104,   data, online, (local)
 * 1: dev_id: 9/109, parity, online
 * 2: dev_id: 6/106, parity, online
 * 3: dev_id: 5/105, parity, online
 * 4: dev_id: 1/101,  spare, repaired
 * 5: dev_id: 0/100,  spare, repairing
 * 6: dev_id: 7/107,  spare, online
 * =============================================
 *
 * Spare usage array info (pool dev_id/global dev_id):
 * =============================================
 * 0: dev_id: 1/101, repaired, served by:
 *    dev_id: 1/101, repaired
 * 1: dev_id: 0/100, repairing, served by:
 *    dev_id: 0/100, repairing
 * 2: slot unused, served by:
 *    dev_id: 7/107, online
 * =============================================
 */
	iter_ut_init(&repair_svc, &dix_repair_cmt.ct_stype);
	iter = iter_ut_iter(repair_svc);
	cctg = iter_ut_insert_lookup_cctg(1, 0, 104);
	iter_ut_insert(cctg, 10, 20);
	reset_parity_grp_info();
	iter_ut_set_drive_repaired(1);
	iter_ut_set_drive_repairing(0);
	iter_ut_rep_validate(iter, 10, 20);
	m0_dix_cm_iter_stop(iter);
	iter_ut_fini(repair_svc);

/*
 * Parity group info for key 10 (pool dev_id/global dev_id):
 * =============================================
 * 0: dev_id: 4/104,   data, online, (local)
 * 1: dev_id: 9/109, parity, online
 * 2: dev_id: 6/106, parity, online
 * 3: dev_id: 5/105, parity, online
 * 4: dev_id: 1/101,  spare, repairing
 * 5: dev_id: 0/100,  spare, repaired
 * 6: dev_id: 7/107,  spare, online
 * =============================================
 *
 * Spare usage array info (pool dev_id/global dev_id):
 * =============================================
 * 0: dev_id: 0/100, repaired, served by:
 *    dev_id: 1/101, repairing
 * 1: dev_id: 1/101, repairing, served by:
 *    dev_id: 0/100, repaired
 * 2: slot unused, served by:
 *    dev_id: 7/107, online
 * =============================================
 */
	iter_ut_init(&repair_svc, &dix_repair_cmt.ct_stype);
	iter = iter_ut_iter(repair_svc);
	cctg = iter_ut_insert_lookup_cctg(1, 0, 104);
	iter_ut_insert(cctg, 10, 20);
	reset_parity_grp_info();
	iter_ut_set_drive_repaired(0);
	iter_ut_set_drive_repairing(1);
	iter_ut_rep_validate(iter, 10, 20);
	m0_dix_cm_iter_stop(iter);
	iter_ut_fini(repair_svc);

/*
 * Parity group info for key 10 (pool dev_id/global dev_id):
 * =============================================
 * 0: dev_id: 4/104,   data, online, (local)
 * 1: dev_id: 9/109, parity, online
 * 2: dev_id: 6/106, parity, online
 * 3: dev_id: 5/105, parity, online
 * 4: dev_id: 1/101,  spare, repaired
 * 5: dev_id: 0/100,  spare, repairing
 * 6: dev_id: 7/107,  spare, repaired
 * =============================================
 *
 * Spare usage array info (pool dev_id/global dev_id):
 * =============================================
 * 0: dev_id: 7/107, repaired, served by:
 *    dev_id: 1/101, repaired
 * 1: dev_id: 1/101, repaired, served by:
 *    dev_id: 0/100, repairing
 * 2: dev_id: 0/100, repairing, served by:
 *    dev_id: 7/107, repaired
 * =============================================
 */
	iter_ut_init(&repair_svc, &dix_repair_cmt.ct_stype);
	iter = iter_ut_iter(repair_svc);
	cctg = iter_ut_insert_lookup_cctg(1, 0, 104);
	iter_ut_insert(cctg, 10, 20);
	reset_parity_grp_info();
	iter_ut_set_drive_repaired(7);
	iter_ut_set_drive_repaired(1);
	iter_ut_set_drive_repairing(0);
	iter_ut_rep_validate(iter, 10, 20);
	m0_dix_cm_iter_stop(iter);
	iter_ut_fini(repair_svc);
}

static void filled_spare_fail(void)
{
	struct m0_dix_cm_iter *iter;
	struct m0_cas_ctg     *cctg;

/*
 * Parity group info for key 10 (pool dev_id/global dev_id):
 * =============================================
 * 0: dev_id: 4/104,   data, online, (local)
 * 1: dev_id: 9/109, parity, online
 * 2: dev_id: 6/106, parity, repaired
 * 3: dev_id: 5/105, parity, online
 * 4: dev_id: 1/101,  spare, repairing
 * 5: dev_id: 0/100,  spare, online
 * 6: dev_id: 7/107,  spare, online
 * =============================================
 *
 * Spare usage array info (pool dev_id/global dev_id):
 * =============================================
 * 0: dev_id: 6/106, repaired, served by:
 *    dev_id: 1/101, repairing
 * 1: dev_id: 1/101, repairing, served by:
 *    dev_id: 0/100, online
 * 2: slot unused, served by:
 *    dev_id: 7/107, online
 * =============================================
 *
 * Targets for key 10 (pool dev_id/global dev_id):
 * =============================================
 * 1: dev_id: 0/100
 * =============================================
 */
	iter_ut_init(&repair_svc, &dix_repair_cmt.ct_stype);
	iter = iter_ut_iter(repair_svc);
	cctg = iter_ut_insert_lookup_cctg(1, 0, 104);
	iter_ut_insert(cctg, 10, 20);
	reset_parity_grp_info();
	iter_ut_set_drive_repaired(6);
	iter_ut_set_drive_repairing(1);
	iter_ut_rep_validate(iter, 10, 20);
	m0_dix_cm_iter_stop(iter);
	iter_ut_fini(repair_svc);

/*
 * Parity group info for key 10 (pool dev_id/global dev_id):
 * =============================================
 * 0: dev_id: 4/104,   data, online, (local)
 * 1: dev_id: 9/109, parity, online
 * 2: dev_id: 6/106, parity, repaired
 * 3: dev_id: 5/105, parity, online
 * 4: dev_id: 1/101,  spare, repaired
 * 5: dev_id: 0/100,  spare, repairing
 * 6: dev_id: 7/107,  spare, online
 * =============================================
 *
 * Spare usage array info (pool dev_id/global dev_id):
 * =============================================
 * 0: dev_id: 6/106, repaired, served by:
 *    dev_id: 1/101, repaired
 * 1: dev_id: 1/101, repaired, served by:
 *    dev_id: 0/100, repairing
 * 2: dev_id: 0/100, repairing, served by:
 *    dev_id: 7/107, online
 * =============================================
 *
 * Targets for key 10 (pool dev_id/global dev_id):
 * =============================================
 * 1: dev_id: 7/107
 * =============================================
 */
	iter_ut_init(&repair_svc, &dix_repair_cmt.ct_stype);
	iter = iter_ut_iter(repair_svc);
	cctg = iter_ut_insert_lookup_cctg(1, 0, 104);
	iter_ut_insert(cctg, 10, 20);
	reset_parity_grp_info();
	iter_ut_set_drive_repaired(6);
	iter_ut_set_drive_repaired(1);
	iter_ut_set_drive_repairing(0);
	iter_ut_rep_validate(iter, 10, 20);
	m0_dix_cm_iter_stop(iter);
	iter_ut_fini(repair_svc);

/*
 * Parity group info for key 10 (pool dev_id/global dev_id):
 * =============================================
 * 0: dev_id: 4/104,   data, repaired
 * 1: dev_id: 9/109, parity, online, (local)
 * 2: dev_id: 6/106, parity, repaired
 * 3: dev_id: 5/105, parity, online
 * 4: dev_id: 1/101,  spare, repairing
 * 5: dev_id: 0/100,  spare, online
 * 6: dev_id: 7/107,  spare, online
 * =============================================
 *
 * Spare usage array info (pool dev_id/global dev_id):
 * =============================================
 * 0: dev_id: 4/104, repaired, served by:
 *    dev_id: 1/101, repairing
 * 1: dev_id: 6/106, repaired, served by:
 *    dev_id: 0/100, online
 * 2: dev_id: 1/101, repairing, served by:
 *    dev_id: 7/107, online
 * =============================================
 *
 * Targets for key 10 (pool dev_id/global dev_id):
 * =============================================
 * 1: dev_id: 7/107
 * =============================================
 */
	iter_ut_init(&repair_svc, &dix_repair_cmt.ct_stype);
	iter = iter_ut_iter(repair_svc);
	cctg = iter_ut_insert_lookup_cctg(1, 0, 109);
	iter_ut_insert(cctg, 10, 20);
	reset_parity_grp_info();
	iter_ut_set_drive_repaired(4);
	iter_ut_set_drive_repaired(6);
	iter_ut_set_drive_repairing(1);
	iter_ut_rep_validate(iter, 10, 20);
	m0_dix_cm_iter_stop(iter);
	iter_ut_fini(repair_svc);

/*
 * Parity group info for key 10 (pool dev_id/global dev_id):
 * =============================================
 * 0: dev_id: 4/104,   data, repaired
 * 1: dev_id: 9/109, parity, online, (local)
 * 2: dev_id: 6/106, parity, online
 * 3: dev_id: 5/105, parity, online
 * 4: dev_id: 1/101,  spare, repairing
 * 5: dev_id: 0/100,  spare, repairing
 * 6: dev_id: 7/107,  spare, online
 * =============================================
 *
 * Spare usage array info (pool dev_id/global dev_id):
 * =============================================
 * 0: dev_id: 4/104, repaired, served by:
 *    dev_id: 1/101, repairing
 * 1: dev_id: 1/101, repairing, served by:
 *    dev_id: 0/100, repairing
 * 2: dev_id: 0/100, repairing, served by:
 *    dev_id: 7/107, online
 * =============================================
 *
 * Targets for key 10 (pool dev_id/global dev_id):
 * =============================================
 * 1: dev_id: 7/107
 * =============================================
 */
	iter_ut_init(&repair_svc, &dix_repair_cmt.ct_stype);
	iter = iter_ut_iter(repair_svc);
	cctg = iter_ut_insert_lookup_cctg(1, 0, 109);
	iter_ut_insert(cctg, 10, 20);
	reset_parity_grp_info();
	iter_ut_set_drive_repaired(4);
	iter_ut_set_drive_repairing(1);
	iter_ut_set_drive_repairing(0);
	iter_ut_rep_validate(iter, 10, 20);
	m0_dix_cm_iter_stop(iter);
	iter_ut_fini(repair_svc);

/*
 * Parity group info for key 10 (pool dev_id/global dev_id):
 * =============================================
 * 0: dev_id: 4/104,   data, repaired
 * 1: dev_id: 9/109, parity, online, (local)
 * 2: dev_id: 6/106, parity, online
 * 3: dev_id: 5/105, parity, online
 * 4: dev_id: 1/101,  spare, repairing
 * 5: dev_id: 0/100,  spare, repairing
 * 6: dev_id: 7/107,  spare, online
 * =============================================
 *
 * Spare usage array info (pool dev_id/global dev_id):
 * =============================================
 * 0: dev_id: 4/104, repaired, served by:
 *    dev_id: 1/101, repairing
 * 1: dev_id: 0/100, repairing, served by:
 *    dev_id: 0/100, repairing
 * 2: dev_id: 1/101, repairing, served by:
 *    dev_id: 7/107, online
 * =============================================
 *
 * Targets for key 10 (pool dev_id/global dev_id):
 * =============================================
 * 1: dev_id: 7/107
 * =============================================
 */
	iter_ut_init(&repair_svc, &dix_repair_cmt.ct_stype);
	iter = iter_ut_iter(repair_svc);
	cctg = iter_ut_insert_lookup_cctg(1, 0, 109);
	iter_ut_insert(cctg, 10, 20);
	reset_parity_grp_info();
	iter_ut_set_drive_repaired(4);
	iter_ut_set_drive_repairing(0);
	iter_ut_set_drive_repairing(1);
	iter_ut_rep_validate(iter, 10, 20);
	m0_dix_cm_iter_stop(iter);
	iter_ut_fini(repair_svc);

/*
 * Parity group info for key 10 (pool dev_id/global dev_id):
 * =============================================
 * 0: dev_id: 4/104,   data, repaired
 * 1: dev_id: 9/109, parity, online, (local)
 * 2: dev_id: 6/106, parity, online
 * 3: dev_id: 5/105, parity, repairing
 * 4: dev_id: 1/101,  spare, repairing
 * 5: dev_id: 0/100,  spare, online
 * 6: dev_id: 7/107,  spare, online
 * =============================================
 *
 * Spare usage array info (pool dev_id/global dev_id):
 * =============================================
 * 0: dev_id: 4/104, repaired, served by:
 *    dev_id: 1/101, repairing
 * 1: dev_id: 1/101, repairing, served by:
 *    dev_id: 0/100, online
 * 2: dev_id: 5/105, repairing, served by:
 *    dev_id: 7/107, online
 *    =============================================
 *
 * Targets for key 10 (pool dev_id/global dev_id):
 * =============================================
 * 1: dev_id: 0/100
 * 2: dev_id: 7/107
 * =============================================
 */
	iter_ut_init(&repair_svc, &dix_repair_cmt.ct_stype);
	iter = iter_ut_iter(repair_svc);
	cctg = iter_ut_insert_lookup_cctg(1, 0, 109);
	iter_ut_insert(cctg, 10, 20);
	reset_parity_grp_info();
	iter_ut_set_drive_repaired(4);
	iter_ut_set_drive_repairing(1);
	iter_ut_set_drive_repairing(5);
	iter_ut_rep_validate(iter, 10, 20);
	m0_dix_cm_iter_stop(iter);
	iter_ut_fini(repair_svc);
}

static void many_keys_rep(void)
{
	struct m0_dix_cm_iter *iter;
	struct m0_cas_ctg     *cctg;
	int                    rc;

/*
 * Parity group info for keys 10/11/12 (pool dev_id/global dev_id):
 * =============================================
 * 0: dev_id: 4/104,   data, repairing
 * 1: dev_id: 9/109, parity, online, (local)
 * 2: dev_id: 6/106, parity, online
 * 3: dev_id: 5/105, parity, online
 * 4: dev_id: 1/101,  spare, online
 * 5: dev_id: 0/100,  spare, online
 * 6: dev_id: 7/107,  spare, online
 * =============================================
 *
 * Spare usage array info (pool dev_id/global dev_id):
 * =============================================
 * 0: dev_id: 4/104, repairing, served by:
 *    dev_id: 1/101, online
 * 1: slot unused, served by:
 *    dev_id: 0/100, online
 * 2: slot unused, served by:
 *    dev_id: 7/107, online
 * =============================================
 *
 * Targets for keys 10/11/12 (pool dev_id/global dev_id):
 * =============================================
 * 1: dev_id: 1/101
 * =============================================
 */

	iter_ut_init(&repair_svc, &dix_repair_cmt.ct_stype);
	iter = iter_ut_iter(repair_svc);
	cctg = iter_ut_insert_lookup_cctg(1, 0, 109);
	iter_ut_insert(cctg, 10, 20);
	iter_ut_insert(cctg, 11, 30);
	iter_ut_insert(cctg, 12, 40);
	device_repairing_set(4);
	M0_SET0(iter);
	rc = m0_dix_cm_iter_start(iter, &dix_repair_dcmt, &reqh, RPC_CUTOFF);
	M0_ASSERT(rc == 0);
	m0_fi_enable_once("dix_cm_iter_next_key", "print_parity_group");
	m0_fi_enable_once("dix_cm_iter_next_key", "print_spare_usage");
	m0_fi_enable_once("dix_cm_iter_next_key", "print_targets");
	check_dix_iter_key_val(iter, 0, 10, 20, 101);
	m0_fi_enable_once("dix_cm_iter_next_key", "print_parity_group");
	m0_fi_enable_once("dix_cm_iter_next_key", "print_spare_usage");
	m0_fi_enable_once("dix_cm_iter_next_key", "print_targets");
	check_dix_iter_key_val(iter, 0, 11, 30, 101);
	m0_fi_enable_once("dix_cm_iter_next_key", "print_parity_group");
	m0_fi_enable_once("dix_cm_iter_next_key", "print_spare_usage");
	m0_fi_enable_once("dix_cm_iter_next_key", "print_targets");
	check_dix_iter_key_val(iter, 0, 12, 40, 101);
	check_dix_iter_key_val(iter, -ENODATA, 0, 0, 0);
	m0_dix_cm_iter_stop(iter);
	iter_ut_fini(repair_svc);
	m0_fi_disable("dix_cm_iter_next_key", "print_parity_group");
	m0_fi_disable("dix_cm_iter_next_key", "print_spare_usage");
	m0_fi_disable("dix_cm_iter_next_key", "print_targets");
}

static void user_concur_rep(void)
{
	struct m0_dix_cm_iter *iter;
	struct m0_cas_ctg     *cctg;
	int                    rc;

/*
 * Parity group info for keys 10/11/12 (pool dev_id/global dev_id):
 * =============================================
 * 0: dev_id: 4/104,   data, repairing
 * 1: dev_id: 9/109, parity, online, (local)
 * 2: dev_id: 6/106, parity, online
 * 3: dev_id: 5/105, parity, online
 * 4: dev_id: 1/101,  spare, online
 * 5: dev_id: 0/100,  spare, online
 * 6: dev_id: 7/107,  spare, online
 * =============================================
 *
 * Spare usage array info (pool dev_id/global dev_id):
 * =============================================
 * 0: dev_id: 4/104, repairing, served by:
 *    dev_id: 1/101, online
 * 1: slot unused, served by:
 *    dev_id: 0/100, online
 * 2: slot unused, served by:
 *    dev_id: 7/107, online
 * =============================================
 *
 * Targets for keys 10/11/12 (pool dev_id/global dev_id):
 * =============================================
 * 1: dev_id: 1/101
 * =============================================
 */

	iter_ut_init(&repair_svc, &dix_repair_cmt.ct_stype);
	iter = iter_ut_iter(repair_svc);
	cctg = iter_ut_insert_lookup_cctg(1, 0, 109);
	iter_ut_insert(cctg, 10, 20);
	iter_ut_insert(cctg, 11, 30);
	iter_ut_insert(cctg, 12, 40);
	device_repairing_set(4);
	M0_SET0(iter);
	rc = m0_dix_cm_iter_start(iter, &dix_repair_dcmt, &reqh, RPC_CUTOFF);
	M0_ASSERT(rc == 0);

	/* Delete the first record. */
	iter_ut_delete(cctg, 10, 20);

	m0_fi_enable_once("dix_cm_iter_next_key", "print_parity_group");
	m0_fi_enable_once("dix_cm_iter_next_key", "print_spare_usage");
	m0_fi_enable_once("dix_cm_iter_next_key", "print_targets");
	check_dix_iter_key_val(iter, 0, 11, 30, 101);

	check_dix_iter_key_val(iter, 0, 12, 40, 101);

	check_dix_iter_key_val(iter, -ENODATA, 0, 0, 0);
	m0_dix_cm_iter_stop(iter);
	iter_ut_fini(repair_svc);


	/* NEXT TEST */
	iter_ut_init(&repair_svc, &dix_repair_cmt.ct_stype);
	iter = iter_ut_iter(repair_svc);
	cctg = iter_ut_insert_lookup_cctg(1, 0, 109);
	iter_ut_insert(cctg, 10, 20);
	iter_ut_insert(cctg, 11, 30);
	iter_ut_insert(cctg, 12, 40);
	device_repairing_set(4);
	M0_SET0(iter);
	rc = m0_dix_cm_iter_start(iter, &dix_repair_dcmt, &reqh, RPC_CUTOFF);
	M0_ASSERT(rc == 0);
	m0_fi_enable_once("dix_cm_iter_next_key", "print_parity_group");
	m0_fi_enable_once("dix_cm_iter_next_key", "print_spare_usage");
	m0_fi_enable_once("dix_cm_iter_next_key", "print_targets");
	check_dix_iter_key_val(iter, 0, 10, 20, 101);

	/* Delete current record. */
	iter_ut_delete(cctg, 10, 20);

	check_dix_iter_key_val(iter, 0, 11, 30, 101);

	check_dix_iter_key_val(iter, 0, 12, 40, 101);

	check_dix_iter_key_val(iter, -ENODATA, 0, 0, 0);
	m0_dix_cm_iter_stop(iter);
	iter_ut_fini(repair_svc);


	/* NEXT TEST */
	iter_ut_init(&repair_svc, &dix_repair_cmt.ct_stype);
	iter = iter_ut_iter(repair_svc);
	cctg = iter_ut_insert_lookup_cctg(1, 0, 109);
	iter_ut_insert(cctg, 10, 20);
	iter_ut_insert(cctg, 11, 30);
	iter_ut_insert(cctg, 12, 40);
	device_repairing_set(4);
	M0_SET0(iter);
	rc = m0_dix_cm_iter_start(iter, &dix_repair_dcmt, &reqh, RPC_CUTOFF);
	M0_ASSERT(rc == 0);
	m0_fi_enable_once("dix_cm_iter_next_key", "print_parity_group");
	m0_fi_enable_once("dix_cm_iter_next_key", "print_spare_usage");
	m0_fi_enable_once("dix_cm_iter_next_key", "print_targets");
	check_dix_iter_key_val(iter, 0, 10, 20, 101);

	/* Delete next record. */
	iter_ut_delete(cctg, 11, 30);

	check_dix_iter_key_val(iter, 0, 12, 40, 101);

	check_dix_iter_key_val(iter, -ENODATA, 0, 0, 0);
	m0_dix_cm_iter_stop(iter);
	iter_ut_fini(repair_svc);


	/* NEXT TEST */
	iter_ut_init(&repair_svc, &dix_repair_cmt.ct_stype);
	iter = iter_ut_iter(repair_svc);
	cctg = iter_ut_insert_lookup_cctg(1, 0, 109);
	iter_ut_insert(cctg, 10, 20);
	iter_ut_insert(cctg, 11, 30);
	iter_ut_insert(cctg, 12, 40);
	device_repairing_set(4);
	M0_SET0(iter);
	rc = m0_dix_cm_iter_start(iter, &dix_repair_dcmt, &reqh, RPC_CUTOFF);
	M0_ASSERT(rc == 0);
	m0_fi_enable_once("dix_cm_iter_next_key", "print_parity_group");
	m0_fi_enable_once("dix_cm_iter_next_key", "print_spare_usage");
	m0_fi_enable_once("dix_cm_iter_next_key", "print_targets");
	check_dix_iter_key_val(iter, 0, 10, 20, 101);

	check_dix_iter_key_val(iter, 0, 11, 30, 101);

	/* Delete the last record. */
	iter_ut_delete(cctg, 12, 40);

	check_dix_iter_key_val(iter, -ENODATA, 0, 0, 0);
	m0_dix_cm_iter_stop(iter);
	iter_ut_fini(repair_svc);
	m0_fi_disable("dix_cm_iter_next_key", "print_parity_group");
	m0_fi_disable("dix_cm_iter_next_key", "print_spare_usage");
	m0_fi_disable("dix_cm_iter_next_key", "print_targets");
}

/*
 * Deletes concurrently current catalogue of repair iterator.
 * Iterator should skip current catalogue and continues with the next one.
 */
static void ctg_del_concur_rep(uint8_t index)
{
	struct m0_dix_cm_iter *iter;
	struct m0_fid          cctg_fid1;
	struct m0_fid          cctg_fid2;
	struct m0_cas_ctg     *cctg;
	struct m0_semaphore    sem;
	struct iter_ut_fom     ctidx_fom;
	struct iter_ut_fom     meta_fom;
	int                    rc;

	iter_ut_init(&repair_svc, &dix_repair_cmt.ct_stype);
	iter = iter_ut_iter(repair_svc);

	iter_ut_insert_cctg_fid(&cctg_fid1, 1, 1, 104);

	iter_ut_insert_cctg_fid(&cctg_fid2, 2, 2, 104);

	cctg = iter_ut_meta_lookup(&cctg_fid1);
	iter_ut_insert(cctg, 10, 20);
	iter_ut_insert(cctg, 11, 30);
	iter_ut_insert(cctg, 12, 40);

	cctg = iter_ut_meta_lookup(&cctg_fid2);
	iter_ut_insert(cctg, 100, 200);
	iter_ut_insert(cctg, 110, 300);

	device_repairing_set(9);
	M0_SET0(iter);
	rc = m0_dix_cm_iter_start(iter, &dix_repair_dcmt, &reqh, RPC_CUTOFF);
	M0_ASSERT(rc == 0);

	m0_fi_enable_once("dix_cm_is_repair_coordinator", "always_coordinator");
	m0_fi_enable_once("dix_cm_repair_tgts_get", "single_target");
	check_dix_iter_key_val(iter, 0, 10, 20, 0);

	m0_semaphore_init(&sem, 0);
	if (index == 1) {
		iter_ut_meta_delete_async(&meta_fom, &cctg_fid1, &sem);
		iter_ut_ctidx_delete_async(&ctidx_fom, &cctg_fid1, &sem);
	} else {
		iter_ut_meta_delete_async(&meta_fom, &cctg_fid2, &sem);
		iter_ut_ctidx_delete_async(&ctidx_fom, &cctg_fid2, &sem);
	}
	m0_fom_timedwait(&ctidx_fom.iu_fom, M0_BITS(ITER_UT_FOM_CTIDX_LOCK),
			 M0_TIME_NEVER);
	m0_fom_timedwait(&meta_fom.iu_fom, M0_BITS(ITER_UT_FOM_META_LOCK),
			 M0_TIME_NEVER);

	m0_fi_enable_once("dix_cm_is_repair_coordinator", "always_coordinator");
	m0_fi_enable_once("dix_cm_repair_tgts_get", "single_target");
	if (index == 1)
		check_dix_iter_key_val(iter, 0, 100, 200, 0);
	else
		check_dix_iter_key_val(iter, 0, 11, 30, 0);

	m0_fi_enable_once("dix_cm_is_repair_coordinator", "always_coordinator");
	m0_fi_enable_once("dix_cm_repair_tgts_get", "single_target");
	if (index == 1)
		check_dix_iter_key_val(iter, 0, 110, 300, 0);
	else
		check_dix_iter_key_val(iter, 0, 12, 40, 0);

	m0_fi_enable_once("dix_cm_is_repair_coordinator", "always_coordinator");
	m0_fi_enable_once("dix_cm_repair_tgts_get", "single_target");
	check_dix_iter_key_val(iter, -ENODATA, 0, 0, 0);

	m0_semaphore_down(&sem);
	m0_semaphore_down(&sem);
	m0_semaphore_fini(&sem);

	m0_dix_cm_iter_stop(iter);
	iter_ut_fini(repair_svc);
	m0_fi_disable("dix_cm_is_repair_coordinator", "always_coordinator");
	m0_fi_disable("dix_cm_repair_tgts_get", "single_target");
}

/*
 * Deletes concurrently current catalogue of repair iterator.
 * Iterator should skip current catalogue and continues with the next one.
 */
static void ctg_del_concur_rep1(void)
{
	ctg_del_concur_rep(1);
}

/*
 * Deletes concurrently a catalogue, that is not processed by iterator at the
 * moment. Iterator should continue processing his current catalogue.
 */
static void ctg_del_concur_rep2(void)
{
	ctg_del_concur_rep(2);
}

static void one_dev_reb(void)
{
	struct m0_dix_cm_iter *iter;
	struct m0_cas_ctg     *cctg;
	int                    rc;

/*
 * Parity group info for key 10 (pool dev_id/global dev_id):
 * =============================================
 * 0: dev_id: 4/104,   data, online
 * 1: dev_id: 9/109, parity, rebalancing
 * 2: dev_id: 6/106, parity, online
 * 3: dev_id: 5/105, parity, online
 * 4: dev_id: 1/101,  spare, online, (local)
 * 5: dev_id: 0/100,  spare, online
 * 6: dev_id: 7/107,  spare, online
 * =============================================
 *
 * Spare usage array info (pool dev_id/global dev_id):
 * =============================================
 * 0: dev_id: 9/109, rebalancing, served by:
 *    dev_id: 1/101, online
 * 1: slot unused, served by:
 *    dev_id: 0/100, online
 * 2: slot unused, served by:
 *    dev_id: 7/107, online
 * =============================================
 *
 * Targets for key 10 (pool dev_id/global dev_id):
 * =============================================
 * 1: dev_id: 9/109
 * =============================================
 */
	iter_ut_init(&rebalance_svc, &dix_rebalance_cmt.ct_stype);
	iter = iter_ut_iter(rebalance_svc);
	cctg = iter_ut_insert_lookup_cctg(1, 0, 101);
	iter_ut_insert(cctg, 10, 20);
	device_rebalancing_set(9);
	M0_SET0(iter);
	rc = m0_dix_cm_iter_start(iter, &dix_rebalance_dcmt, &reqh, RPC_CUTOFF);
	M0_ASSERT(rc == 0);
	m0_fi_enable_once("dix_cm_iter_next_key", "print_parity_group");
	m0_fi_enable_once("dix_cm_iter_next_key", "print_spare_usage");
	m0_fi_enable_once("dix_cm_iter_next_key", "print_targets");
	check_dix_iter_key_val(iter, 0, 10, 20, 109);
	check_dix_iter_key_val(iter, -ENODATA, 0, 0, 0);
	m0_dix_cm_iter_stop(iter);
	iter_ut_fini(rebalance_svc);
	m0_fi_disable("dix_cm_iter_next_key", "print_parity_group");
	m0_fi_disable("dix_cm_iter_next_key", "print_spare_usage");
	m0_fi_disable("dix_cm_iter_next_key", "print_targets");
}

static void reb_coordinator(void)
{
	struct m0_dix_cm_iter *iter;
	struct m0_cas_ctg     *cctg;
	int                    rc;

/*
 * Parity group info for key 10 (pool dev_id/global dev_id):
 * =============================================
 * 0: dev_id: 4/104,   data, repaired
 * 1: dev_id: 9/109, parity, rebalancing
 * 2: dev_id: 6/106, parity, online
 * 3: dev_id: 5/105, parity, online
 * 4: dev_id: 1/101,  spare, online, (local)
 * 5: dev_id: 0/100,  spare, online
 * 6: dev_id: 7/107,  spare, online
 * =============================================
 *
 * Spare usage array info (pool dev_id/global dev_id):
 * =============================================
 * 0: dev_id: 4/104, repaired, served by:
 *    dev_id: 1/101, online
 * 1: dev_id: 9/109, rebalancing, served by:
 *    dev_id: 0/100, online
 * 2: slot unused, served by:
 *    dev_id: 7/107, online
 * =============================================
 */
	iter_ut_init(&rebalance_svc, &dix_rebalance_cmt.ct_stype);
	iter = iter_ut_iter(rebalance_svc);
	cctg = iter_ut_insert_lookup_cctg(1, 0, 101);
	iter_ut_insert(cctg, 10, 20);
	device_repaired_set(4);
	device_rebalancing_set(9);
	M0_SET0(iter);
	rc = m0_dix_cm_iter_start(iter, &dix_rebalance_dcmt, &reqh, RPC_CUTOFF);
	M0_ASSERT(rc == 0);
	m0_fi_enable_once("dix_cm_iter_next_key", "print_parity_group");
	m0_fi_enable_once("dix_cm_iter_next_key", "print_spare_usage");
	m0_fi_enable_once("dix_cm_iter_next_key", "print_targets");
	check_dix_iter_key_val(iter, -ENODATA, 0, 0, 0);
	m0_dix_cm_iter_stop(iter);
	iter_ut_fini(rebalance_svc);

/*
 * Parity group info for key 10 (pool dev_id/global dev_id):
 * =============================================
 * 0: dev_id: 4/104,   data, repaired
 * 1: dev_id: 9/109, parity, rebalancing
 * 2: dev_id: 6/106, parity, online
 * 3: dev_id: 5/105, parity, online
 * 4: dev_id: 1/101,  spare, online, (local)
 * 5: dev_id: 0/100,  spare, online
 * 6: dev_id: 7/107,  spare, online
 * =============================================
 *
 * Spare usage array info (pool dev_id/global dev_id):
 * =============================================
 * 0: dev_id: 4/104, repaired, served by:
 *    dev_id: 1/101, online
 * 1: dev_id: 9/109, rebalancing, served by:
 *    dev_id: 0/100, online
 * 2: slot unused, served by:
 *    dev_id: 7/107, online
 * =============================================
 */
	iter_ut_init(&rebalance_svc, &dix_rebalance_cmt.ct_stype);
	iter = iter_ut_iter(rebalance_svc);
	cctg = iter_ut_insert_lookup_cctg(1, 0, 100);
	iter_ut_insert(cctg, 10, 20);
	device_repaired_set(4);
	device_rebalancing_set(9);
	M0_SET0(iter);
	rc = m0_dix_cm_iter_start(iter, &dix_rebalance_dcmt, &reqh, RPC_CUTOFF);
	M0_ASSERT(rc == 0);
	m0_fi_enable_once("dix_cm_iter_next_key", "print_parity_group");
	m0_fi_enable_once("dix_cm_iter_next_key", "print_spare_usage");
	m0_fi_enable_once("dix_cm_iter_next_key", "print_targets");
	check_dix_iter_key_val(iter, 0, 10, 20, 109);
	check_dix_iter_key_val(iter, -ENODATA, 0, 0, 0);
	m0_dix_cm_iter_stop(iter);
	iter_ut_fini(rebalance_svc);

/*
 * Parity group info for key 10 (pool dev_id/global dev_id):
 * =============================================
 * 0: dev_id: 4/104,   data, repaired
 * 1: dev_id: 9/109, parity, rebalancing
 * 2: dev_id: 6/106, parity, online
 * 3: dev_id: 5/105, parity, online
 * 4: dev_id: 1/101,  spare, rebalancing
 * 5: dev_id: 0/100,  spare, online
 * 6: dev_id: 7/107,  spare, online, (local)
 * =============================================
 *
 * Spare usage array info (pool dev_id/global dev_id):
 * =============================================
 * 0: dev_id: 9/109, rebalancing, served by:
 *    dev_id: 1/101, rebalancing
 * 1: dev_id: 4/104, repaired, served by:
 *    dev_id: 0/100, online
 * 2: dev_id: 1/101, rebalancing, served by:
 *    dev_id: 7/107, online
 * =============================================
 *
 * Targets for key 10 (pool dev_id/global dev_id):
 * =============================================
 * 1: dev_id: 9/109
 * =============================================
 */
	iter_ut_init(&rebalance_svc, &dix_rebalance_cmt.ct_stype);
	iter = iter_ut_iter(rebalance_svc);
	cctg = iter_ut_insert_lookup_cctg(1, 0, 107);
	iter_ut_insert(cctg, 10, 20);
	device_rebalancing_set(9);
	device_repaired_set(4);
	device_rebalancing_set(1);
	M0_SET0(iter);
	rc = m0_dix_cm_iter_start(iter, &dix_rebalance_dcmt, &reqh, RPC_CUTOFF);
	M0_ASSERT(rc == 0);
	m0_fi_enable_once("dix_cm_iter_next_key", "print_parity_group");
	m0_fi_enable_once("dix_cm_iter_next_key", "print_spare_usage");
	m0_fi_enable_once("dix_cm_iter_next_key", "print_targets");
	check_dix_iter_key_val(iter, 0, 10, 20, 109);
	check_dix_iter_key_val(iter, -ENODATA, 0, 0, 0);
	m0_dix_cm_iter_stop(iter);
	iter_ut_fini(rebalance_svc);
	m0_fi_disable("dix_cm_iter_next_key", "print_parity_group");
	m0_fi_disable("dix_cm_iter_next_key", "print_spare_usage");
	m0_fi_disable("dix_cm_iter_next_key", "print_targets");
}

static void outside_dev_reb(void)
{
	struct m0_dix_cm_iter *iter;
	struct m0_cas_ctg     *cctg;
	int                    rc;

/*
 * Parity group info for key 10 (pool dev_id/global dev_id):
 * =============================================
 * 0: dev_id: 4/104, data, online
 * 1: dev_id: 9/109, parity, online
 * 2: dev_id: 6/106, parity, online
 * 3: dev_id: 5/105, parity, online
 * 4: dev_id: 1/101, spare, online, (local)
 * 5: dev_id: 0/100, spare, online
 * 6: dev_id: 7/107, spare, online
 * =============================================
 *
 * Spare usage array info (pool dev_id/global dev_id):
 * =============================================
 * 0: dev_id: 3/103, rebalancing, served by:
 *    dev_id: 1/101, online
 * 1: slot unused, served by:
 *    dev_id: 0/100, online
 * 2: slot unused, served by:
 *    dev_id: 7/107, online
 * =============================================
 */
	iter_ut_init(&rebalance_svc, &dix_rebalance_cmt.ct_stype);
	iter = iter_ut_iter(rebalance_svc);
	cctg = iter_ut_insert_lookup_cctg(1, 0, 101);
	iter_ut_insert(cctg, 10, 20);
	device_rebalancing_set(3);
	M0_SET0(iter);
	rc = m0_dix_cm_iter_start(iter, &dix_rebalance_dcmt, &reqh, RPC_CUTOFF);
	M0_ASSERT(rc == 0);
	m0_fi_enable_once("dix_cm_iter_next_key", "print_parity_group");
	m0_fi_enable_once("dix_cm_iter_next_key", "print_spare_usage");
	m0_fi_enable_once("dix_cm_iter_next_key", "print_targets");
	check_dix_iter_key_val(iter, -ENODATA, 0, 0, 0);
	m0_dix_cm_iter_stop(iter);
	iter_ut_fini(rebalance_svc);

/*
 * Parity group info for key 10 (pool dev_id/global dev_id):
 * =============================================
 * 0: dev_id: 4/104,   data, online
 * 1: dev_id: 9/109, parity, rebalancing
 * 2: dev_id: 6/106, parity, online
 * 3: dev_id: 5/105, parity, online
 * 4: dev_id: 1/101,  spare, online
 * 5: dev_id: 0/100,  spare, online, (local)
 * 6: dev_id: 7/107,  spare, online
 * =============================================
 *
 * Spare usage array info (pool dev_id/global dev_id):
 * =============================================
 * 0: dev_id: 3/103, rebalancing, served by:
 *    dev_id: 1/101, online
 * 1: dev_id: 9/109, rebalancing, served by:
 *    dev_id: 0/100, online
 * 2: slot unused, served by:
 *    dev_id: 7/107, online
 * =============================================
 *
 * Targets for key 10 (pool dev_id/global dev_id):
 * =============================================
 * 1: dev_id: 9/109
 * =============================================
 */
	iter_ut_init(&rebalance_svc, &dix_rebalance_cmt.ct_stype);
	iter = iter_ut_iter(rebalance_svc);
	cctg = iter_ut_insert_lookup_cctg(1, 0, 100);
	iter_ut_insert(cctg, 10, 20);
	device_rebalancing_set(3);
	device_rebalancing_set(9);
	M0_SET0(iter);
	rc = m0_dix_cm_iter_start(iter, &dix_rebalance_dcmt, &reqh, RPC_CUTOFF);
	M0_ASSERT(rc == 0);
	m0_fi_enable_once("dix_cm_iter_next_key", "print_parity_group");
	m0_fi_enable_once("dix_cm_iter_next_key", "print_spare_usage");
	m0_fi_enable_once("dix_cm_iter_next_key", "print_targets");
	check_dix_iter_key_val(iter, 0, 10, 20, 109);
	check_dix_iter_key_val(iter, -ENODATA, 0, 0, 0);
	m0_dix_cm_iter_stop(iter);
	iter_ut_fini(rebalance_svc);

/*
 * Parity group info for key 10 (pool dev_id/global dev_id):
 * =============================================
 * 0: dev_id: 4/104, data, online
 * 1: dev_id: 9/109, parity, rebalancing
 * 2: dev_id: 6/106, parity, online
 * 3: dev_id: 5/105, parity, online
 * 4: dev_id: 1/101, spare, online
 * 5: dev_id: 0/100, spare, rebalancing
 * 6: dev_id: 7/107, spare, online, (local)
 * =============================================
 *
 * Spare usage array info (pool dev_id/global dev_id):
 * =============================================
 * 0: dev_id: 3/103, rebalancing, served by:
 *    dev_id: 1/101, online
 * 1: dev_id: 9/109, rebalancing, served by:
 *    dev_id: 0/100, rebalancing
 * 2: dev_id: 0/100, rebalancing, served by:
 *    dev_id: 7/107, online
 * =============================================
 *
 * Targets for key 10 (pool dev_id/global dev_id):
 * =============================================
 * 1: dev_id: 9/109
 * =============================================
 */
	iter_ut_init(&rebalance_svc, &dix_rebalance_cmt.ct_stype);
	iter = iter_ut_iter(rebalance_svc);
	cctg = iter_ut_insert_lookup_cctg(1, 0, 107);
	iter_ut_insert(cctg, 10, 20);
	device_rebalancing_set(3);
	device_rebalancing_set(9);
	device_rebalancing_set(0);
	M0_SET0(iter);
	rc = m0_dix_cm_iter_start(iter, &dix_rebalance_dcmt, &reqh, RPC_CUTOFF);
	M0_ASSERT(rc == 0);
	m0_fi_enable_once("dix_cm_iter_next_key", "print_parity_group");
	m0_fi_enable_once("dix_cm_iter_next_key", "print_spare_usage");
	m0_fi_enable_once("dix_cm_iter_next_key", "print_targets");
	check_dix_iter_key_val(iter, 0, 10, 20, 109);
	check_dix_iter_key_val(iter, -ENODATA, 0, 0, 0);
	m0_dix_cm_iter_stop(iter);
	iter_ut_fini(rebalance_svc);
	m0_fi_disable("dix_cm_iter_next_key", "print_parity_group");
	m0_fi_disable("dix_cm_iter_next_key", "print_spare_usage");
	m0_fi_disable("dix_cm_iter_next_key", "print_targets");
}

static void reb_unused(void)
{
	struct m0_dix_cm_iter *iter;
	struct m0_cas_ctg     *cctg;
	int                    rc;

/*
 * Parity group info for key 10 (pool dev_id/global dev_id):
 * =============================================
 * 0: dev_id: 4/104,   data, online
 * 1: dev_id: 9/109, parity, rebalancing
 * 2: dev_id: 6/106, parity, online
 * 3: dev_id: 5/105, parity, online
 * 4: dev_id: 1/101,  spare, online, (local)
 * 5: dev_id: 0/100,  spare, online
 * 6: dev_id: 7/107,  spare, online
 * =============================================
 *
 * Spare usage array info (pool dev_id/global dev_id):
 * =============================================
 * 0: slot unused, served by:
 *    dev_id: 1/101, online
 * 1: dev_id: 9/109, rebalancing, served by:
 *    dev_id: 0/100, online
 * 2: slot unused, served by:
 *    dev_id: 7/107, online
 * =============================================
 */
	iter_ut_init(&rebalance_svc, &dix_rebalance_cmt.ct_stype);
	iter = iter_ut_iter(rebalance_svc);
	cctg = iter_ut_insert_lookup_cctg(1, 0, 101);
	iter_ut_insert(cctg, 10, 20);
	spare_slot_unused_set();
	device_rebalancing_set(9);
	M0_SET0(iter);
	rc = m0_dix_cm_iter_start(iter, &dix_rebalance_dcmt, &reqh, RPC_CUTOFF);
	M0_ASSERT(rc == 0);
	m0_fi_enable_once("dix_cm_iter_next_key", "print_parity_group");
	m0_fi_enable_once("dix_cm_iter_next_key", "print_spare_usage");
	m0_fi_enable_once("dix_cm_iter_next_key", "print_targets");
	check_dix_iter_key_val(iter, -ENODATA, 0, 0, 0);
	m0_dix_cm_iter_stop(iter);
	iter_ut_fini(rebalance_svc);

/*
 * Parity group info for key 10 (pool dev_id/global dev_id):
 * =============================================
 * 0: dev_id: 4/104,   data, online
 * 1: dev_id: 9/109, parity, rebalancing
 * 2: dev_id: 6/106, parity, online
 * 3: dev_id: 5/105, parity, online
 * 4: dev_id: 1/101,  spare, online
 * 5: dev_id: 0/100,  spare, online
 * 6: dev_id: 7/107,  spare, online, (local)
 * =============================================
 *
 * Spare usage array info (pool dev_id/global dev_id):
 * =============================================
 * 0: dev_id: 9/109, rebalancing, served by:
 *    dev_id: 1/101, online
 * 1: slot unused, served by:
 *    dev_id: 0/100, online
 * 2: slot unused, served by:
 *    dev_id: 7/107, online
 * =============================================
 */
	iter_ut_init(&rebalance_svc, &dix_rebalance_cmt.ct_stype);
	iter = iter_ut_iter(rebalance_svc);
	cctg = iter_ut_insert_lookup_cctg(1, 0, 107);
	iter_ut_insert(cctg, 10, 20);
	device_rebalancing_set(9);
	M0_SET0(iter);
	rc = m0_dix_cm_iter_start(iter, &dix_rebalance_dcmt, &reqh, RPC_CUTOFF);
	M0_ASSERT(rc == 0);
	m0_fi_enable_once("dix_cm_iter_next_key", "print_parity_group");
	m0_fi_enable_once("dix_cm_iter_next_key", "print_spare_usage");
	m0_fi_enable_once("dix_cm_iter_next_key", "print_targets");
	check_dix_iter_key_val(iter, -ENODATA, 0, 0, 0);
	m0_dix_cm_iter_stop(iter);
	iter_ut_fini(rebalance_svc);

/*
 * Parity group info for key 10 (pool dev_id/global dev_id):
 * =============================================
 * 0: dev_id: 4/104,   data, online
 * 1: dev_id: 9/109, parity, rebalancing
 * 2: dev_id: 6/106, parity, online
 * 3: dev_id: 5/105, parity, online
 * 4: dev_id: 1/101,  spare, rebalancing
 * 5: dev_id: 0/100,  spare, online
 * 6: dev_id: 7/107,  spare, online, (local)
 * =============================================
 *
 * Spare usage array info (pool dev_id/global dev_id):
 * =============================================
 * 0: dev_id: 9/109, rebalancing, served by:
 *    dev_id: 1/101, rebalancing
 * 1: slot unused, served by:
 *    dev_id: 0/100, online
 * 2: dev_id: 1/101, rebalancing, served by:
 *    dev_id: 7/107, online
 * =============================================
 *
 * Targets for key 10 (pool dev_id/global dev_id):
 * =============================================
 * 1: dev_id: 9/109
 * =============================================
 */
	iter_ut_init(&rebalance_svc, &dix_rebalance_cmt.ct_stype);
	iter = iter_ut_iter(rebalance_svc);
	cctg = iter_ut_insert_lookup_cctg(1, 0, 107);
	iter_ut_insert(cctg, 10, 20);
	device_rebalancing_set(9);
	spare_slot_unused_set();
	device_rebalancing_set(1);
	M0_SET0(iter);
	rc = m0_dix_cm_iter_start(iter, &dix_rebalance_dcmt, &reqh, RPC_CUTOFF);
	M0_ASSERT(rc == 0);
	m0_fi_enable_once("dix_cm_iter_next_key", "print_parity_group");
	m0_fi_enable_once("dix_cm_iter_next_key", "print_spare_usage");
	m0_fi_enable_once("dix_cm_iter_next_key", "print_targets");
	check_dix_iter_key_val(iter, 0, 10, 20, 109);
	check_dix_iter_key_val(iter, -ENODATA, 0, 0, 0);
	m0_dix_cm_iter_stop(iter);
	iter_ut_fini(rebalance_svc);

/*
 * Test case: firstly the data from the device 4/104 was repaired, then the data
 * from the device 9/109 (for example) was repaired on the device 0/100, which
 * in turn failed and its data was repaired on the device 7/107 (local device).
 * After that the device 9/109 was turned on and its data was rebalanced (unused
 * slot appeared in spare usage array), and then the device 0/100 has been
 * turned on and now rebalance is in progress. No targets should be determined
 * as original device that serves the data was turned on.
 *
 * Parity group info for key 10 (pool dev_id/global dev_id):
 * =============================================
 * 0: dev_id: 4/104,   data, repaired
 * 1: dev_id: 9/109, parity, online
 * 2: dev_id: 6/106, parity, online
 * 3: dev_id: 5/105, parity, online
 * 4: dev_id: 1/101,  spare, online
 * 5: dev_id: 0/100,  spare, rebalancing
 * 6: dev_id: 7/107,  spare, online, (local)
 * =============================================
 *
 * Spare usage array info (pool dev_id/global dev_id):
 * =============================================
 * 0: dev_id: 4/104, repaired, served by:
 *    dev_id: 1/101, online
 * 1: slot unused, served by:
 *    dev_id: 0/100, rebalancing
 * 2: dev_id: 0/100, rebalancing, served by:
 *    dev_id: 7/107, online
 * =============================================
 */
	iter_ut_init(&rebalance_svc, &dix_rebalance_cmt.ct_stype);
	iter = iter_ut_iter(rebalance_svc);
	cctg = iter_ut_insert_lookup_cctg(1, 0, 107);
	iter_ut_insert(cctg, 10, 20);
	device_repaired_set(4);
	spare_slot_unused_set();
	device_rebalancing_set(0);
	M0_SET0(iter);
	rc = m0_dix_cm_iter_start(iter, &dix_rebalance_dcmt, &reqh, RPC_CUTOFF);
	M0_ASSERT(rc == 0);
	m0_fi_enable_once("dix_cm_iter_next_key", "print_parity_group");
	m0_fi_enable_once("dix_cm_iter_next_key", "print_spare_usage");
	m0_fi_enable_once("dix_cm_iter_next_key", "print_targets");
	check_dix_iter_key_val(iter, -ENODATA, 0, 0, 0);
	m0_dix_cm_iter_stop(iter);
	iter_ut_fini(rebalance_svc);
	m0_fi_disable("dix_cm_iter_next_key", "print_parity_group");
	m0_fi_disable("dix_cm_iter_next_key", "print_spare_usage");
	m0_fi_disable("dix_cm_iter_next_key", "print_targets");
}

static void many_keys_reb(void)
{
	struct m0_dix_cm_iter *iter;
	struct m0_cas_ctg     *cctg;
	int                    rc;

/*
 * Parity group info for key 10/11/12 (pool dev_id/global dev_id):
 * =============================================
 * 0: dev_id: 4/104,   data, online
 * 1: dev_id: 9/109, parity, rebalancing
 * 2: dev_id: 6/106, parity, online
 * 3: dev_id: 5/105, parity, online
 * 4: dev_id: 1/101,  spare, online, (local)
 * 5: dev_id: 0/100,  spare, online
 * 6: dev_id: 7/107,  spare, online
 * =============================================
 *
 * Spare usage array info (pool dev_id/global dev_id):
 * =============================================
 * 0: dev_id: 9/109, rebalancing, served by:
 *    dev_id: 1/101, online
 * 1: slot unused, served by:
 *    dev_id: 0/100, online
 * 2: slot unused, served by:
 *    dev_id: 7/107, online
 * =============================================
 *
 * Targets for key 10/11/12 (pool dev_id/global dev_id):
 * =============================================
 * 1: dev_id: 9/109
 * =============================================
 */
	iter_ut_init(&rebalance_svc, &dix_rebalance_cmt.ct_stype);
	iter = iter_ut_iter(rebalance_svc);
	cctg = iter_ut_insert_lookup_cctg(1, 0, 101);
	iter_ut_insert(cctg, 10, 20);
	iter_ut_insert(cctg, 11, 30);
	iter_ut_insert(cctg, 12, 40);
	device_rebalancing_set(9);
	M0_SET0(iter);
	rc = m0_dix_cm_iter_start(iter, &dix_rebalance_dcmt, &reqh, RPC_CUTOFF);
	M0_ASSERT(rc == 0);
	m0_fi_enable_once("dix_cm_iter_next_key", "print_parity_group");
	m0_fi_enable_once("dix_cm_iter_next_key", "print_spare_usage");
	m0_fi_enable_once("dix_cm_iter_next_key", "print_targets");
	check_dix_iter_key_val(iter, 0, 10, 20, 109);
	m0_fi_enable_once("dix_cm_iter_next_key", "print_parity_group");
	m0_fi_enable_once("dix_cm_iter_next_key", "print_spare_usage");
	m0_fi_enable_once("dix_cm_iter_next_key", "print_targets");
	check_dix_iter_key_val(iter, 0, 11, 30, 109);
	m0_fi_enable_once("dix_cm_iter_next_key", "print_parity_group");
	m0_fi_enable_once("dix_cm_iter_next_key", "print_spare_usage");
	m0_fi_enable_once("dix_cm_iter_next_key", "print_targets");
	check_dix_iter_key_val(iter, 0, 12, 40, 109);
	check_dix_iter_key_val(iter, -ENODATA, 0, 0, 0);
	m0_dix_cm_iter_stop(iter);
	iter_ut_fini(rebalance_svc);
	m0_fi_disable("dix_cm_iter_next_key", "print_parity_group");
	m0_fi_disable("dix_cm_iter_next_key", "print_spare_usage");
	m0_fi_disable("dix_cm_iter_next_key", "print_targets");
}

static void user_concur_reb(void)
{
	struct m0_dix_cm_iter *iter;
	struct m0_cas_ctg     *cctg;
	int                    rc;

/*
 * Parity group info for key 10/11/12 (pool dev_id/global dev_id):
 * =============================================
 * 0: dev_id: 4/104,   data, online
 * 1: dev_id: 9/109, parity, rebalancing
 * 2: dev_id: 6/106, parity, online
 * 3: dev_id: 5/105, parity, online
 * 4: dev_id: 1/101,  spare, online, (local)
 * 5: dev_id: 0/100,  spare, online
 * 6: dev_id: 7/107,  spare, online
 * =============================================
 *
 * Spare usage array info (pool dev_id/global dev_id):
 * =============================================
 * 0: dev_id: 9/109, rebalancing, served by:
 *    dev_id: 1/101, online
 * 1: slot unused, served by:
 *    dev_id: 0/100, online
 * 2: slot unused, served by:
 *    dev_id: 7/107, online
 * =============================================
 *
 * Targets for key 10/11/12 (pool dev_id/global dev_id):
 * =============================================
 * 1: dev_id: 9/109
 * =============================================
 */
	iter_ut_init(&rebalance_svc, &dix_rebalance_cmt.ct_stype);
	iter = iter_ut_iter(rebalance_svc);
	cctg = iter_ut_insert_lookup_cctg(1, 0, 101);
	iter_ut_insert(cctg, 10, 20);
	iter_ut_insert(cctg, 11, 30);
	iter_ut_insert(cctg, 12, 40);
	device_rebalancing_set(9);
	M0_SET0(iter);
	rc = m0_dix_cm_iter_start(iter, &dix_rebalance_dcmt, &reqh, RPC_CUTOFF);
	M0_ASSERT(rc == 0);
	/* Delete the first record. */
	iter_ut_delete(cctg, 10, 20);

	m0_fi_enable_once("dix_cm_iter_next_key", "print_parity_group");
	m0_fi_enable_once("dix_cm_iter_next_key", "print_spare_usage");
	m0_fi_enable_once("dix_cm_iter_next_key", "print_targets");
	check_dix_iter_key_val(iter, 0, 11, 30, 109);

	check_dix_iter_key_val(iter, 0, 12, 40, 109);

	check_dix_iter_key_val(iter, -ENODATA, 0, 0, 0);
	m0_dix_cm_iter_stop(iter);
	iter_ut_fini(rebalance_svc);

	/* NEXT TEST */
	iter_ut_init(&rebalance_svc, &dix_rebalance_cmt.ct_stype);
	iter = iter_ut_iter(rebalance_svc);
	cctg = iter_ut_insert_lookup_cctg(1, 0, 101);
	iter_ut_insert(cctg, 10, 20);
	iter_ut_insert(cctg, 11, 30);
	iter_ut_insert(cctg, 12, 40);
	device_rebalancing_set(9);
	M0_SET0(iter);
	rc = m0_dix_cm_iter_start(iter, &dix_rebalance_dcmt, &reqh, RPC_CUTOFF);
	M0_ASSERT(rc == 0);
	m0_fi_enable_once("dix_cm_iter_next_key", "print_parity_group");
	m0_fi_enable_once("dix_cm_iter_next_key", "print_spare_usage");
	m0_fi_enable_once("dix_cm_iter_next_key", "print_targets");
	check_dix_iter_key_val(iter, 0, 10, 20, 109);

	/* Delete next record. */
	iter_ut_delete(cctg, 11, 30);

	check_dix_iter_key_val(iter, 0, 12, 40, 109);

	check_dix_iter_key_val(iter, -ENODATA, 0, 0, 0);
	m0_dix_cm_iter_stop(iter);
	iter_ut_fini(rebalance_svc);


	/* NEXT TEST */
	iter_ut_init(&rebalance_svc, &dix_rebalance_cmt.ct_stype);
	iter = iter_ut_iter(rebalance_svc);
	cctg = iter_ut_insert_lookup_cctg(1, 0, 101);
	iter_ut_insert(cctg, 10, 20);
	iter_ut_insert(cctg, 11, 30);
	iter_ut_insert(cctg, 12, 40);
	device_rebalancing_set(9);
	M0_SET0(iter);
	rc = m0_dix_cm_iter_start(iter, &dix_rebalance_dcmt, &reqh, RPC_CUTOFF);
	M0_ASSERT(rc == 0);
	m0_fi_enable_once("dix_cm_iter_next_key", "print_parity_group");
	m0_fi_enable_once("dix_cm_iter_next_key", "print_spare_usage");
	m0_fi_enable_once("dix_cm_iter_next_key", "print_targets");
	check_dix_iter_key_val(iter, 0, 10, 20, 109);
	check_dix_iter_key_val(iter, 0, 11, 30, 109);

	/* Delete the last record. */
	iter_ut_delete(cctg, 12, 40);

	check_dix_iter_key_val(iter, -ENODATA, 0, 0, 0);
	m0_dix_cm_iter_stop(iter);
	iter_ut_fini(rebalance_svc);
	m0_fi_disable("dix_cm_iter_next_key", "print_parity_group");
	m0_fi_disable("dix_cm_iter_next_key", "print_spare_usage");
	m0_fi_disable("dix_cm_iter_next_key", "print_targets");
	m0_fi_disable("m0_dix_target", "pdcluster-map");
}

struct m0_ut_suite dix_cm_iter_ut = {
	.ts_name   = "dix-cm-iter",
	.ts_owners = "Egor",
	.ts_init   = NULL,
	.ts_fini   = NULL,
	.ts_tests  = {
		{ "start-stop",          start_stop,          "Egor"   },
		{ "empty-store",         empty_store,         "Egor"   },
		{ "empty-cctg",          empty_cctg,          "Egor"   },
		{ "cctg-not-found",      cctg_not_found,      "Egor"   },
		{ "one-rec",             one_rec,             "Egor"   },
		{ "multi-rec",           multi_rec,           "Egor"   },
		{ "rep-coordinator",     rep_coordinator,     "Sergey" },
		{ "one-dev-fail",        one_dev_fail,        "Sergey" },
		{ "two-devs-fail",       two_devs_fail,       "Sergey" },
		{ "outside-dev-fail",    outside_dev_fail,    "Sergey" },
		{ "empty-spare-fail",    empty_spare_fail,    "Sergey" },
		{ "filled-spare-fail",   filled_spare_fail,   "Sergey" },
		{ "many-keys-rep",       many_keys_rep,       "Sergey" },
		{ "user-concur-rep",     user_concur_rep,     "Sergey" },
		{ "ctg-del-concur-rep1", ctg_del_concur_rep1, "Sergey" },
		{ "ctg-del-concur-rep2", ctg_del_concur_rep2, "Sergey" },
		{ "reb-coordinator",     reb_coordinator,     "Sergey" },
		{ "one-dev-reb",         one_dev_reb,         "Sergey" },
		{ "outside-dev-reb",     outside_dev_reb,     "Sergey" },
		{ "reb-unused",          reb_unused,          "Sergey" },
		{ "many-keys-reb",       many_keys_reb,       "Sergey" },
		{ "user-concur-reb",     user_concur_reb,     "Sergey" },
		{ NULL, NULL }
	}
};

#undef M0_TRACE_SUBSYSTEM

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
