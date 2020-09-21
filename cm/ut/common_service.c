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


#include "lib/arith.h"
#include "lib/errno.h"
#include "cm/ut/common_service.h"
#include "rpc/rpc_opcodes.h"             /* M0_CM_UT_OPCODE */
#include "motr/setup.h"
#include "ha/msg.h"

struct m0_cm_cp            cm_ut_cp;
struct m0_ut_cm            cm_ut[MAX_CM_NR];
struct m0_reqh_service    *cm_ut_service;
struct m0_ut_rpc_mach_ctx  cmut_rmach_ctx;

const char  lfname[] = "cm_ut.errlog";
FILE       *lf;

/* Copy machine replica identifier for multiple copy machine replicas. */
uint64_t ut_cm_id;

struct m0_ut_cm *cm2utcm(struct m0_cm *cm)
{
	return container_of(cm, struct m0_ut_cm, ut_cm);
}

static int cm_ut_service_start(struct m0_reqh_service *service)
{
	struct m0_cm *cm;

	cm = container_of(service, struct m0_cm, cm_service);
	return m0_cm_setup(cm);
}

static void cm_ut_service_stop(struct m0_reqh_service *service)
{
	struct m0_cm *cm = container_of(service, struct m0_cm, cm_service);

	m0_cm_fini(cm);
}

static void cm_ut_service_fini(struct m0_reqh_service *service)
{
	struct m0_cm    *cm = m0_cmsvc2cm(service);
	struct m0_ut_cm *ut_cm;

	cm_ut_service = NULL;
	ut_cm = cm2utcm(cm);

	m0_chan_fini_lock(&ut_cm->ut_cm_wait);
	m0_mutex_fini(&ut_cm->ut_cm_wait_mutex);
	M0_SET0(&ut_cm->ut_cm);
	M0_CNT_DEC(ut_cm_id);
}

static const struct m0_reqh_service_ops cm_ut_service_ops = {
	.rso_start = cm_ut_service_start,
	.rso_stop  = cm_ut_service_stop,
	.rso_fini  = cm_ut_service_fini
};

static void cm_cp_ut_free(struct m0_cm_cp *cp)
{
}

static bool cm_cp_ut_invariant(const struct m0_cm_cp *cp)
{
	return true;
}

static const struct m0_cm_cp_ops cm_cp_ut_ops = {
	.co_invariant = cm_cp_ut_invariant,
	.co_free = cm_cp_ut_free
};

static struct m0_cm_cp* cm_ut_cp_alloc(struct m0_cm *cm)
{
	cm_ut_cp.c_ops = &cm_cp_ut_ops;
	return &cm_ut_cp;
}

static int cm_ut_setup(struct m0_cm *cm)
{
	return 0;
}

static int cm_ut_prepare(struct m0_cm *cm)
{
	return 0;
}

static int cm_ut_start(struct m0_cm *cm)
{
	return 0;
}

static void cm_ut_stop(struct m0_cm *cm)
{
}

static int cm_ut_data_next(struct m0_cm *cm, struct m0_cm_cp *cp)
{
	return -ENODATA;
}

static void cm_ag_ut_fini(struct m0_cm_aggr_group *ag)
{
}

static uint64_t cm_ag_ut_local_cp_nr(const struct m0_cm_aggr_group *ag)
{
	return CM_UT_LOCAL_CP_NR;
}

const struct m0_cm_aggr_group_ops cm_ag_ut_ops = {
	.cago_fini = cm_ag_ut_fini,
	.cago_local_cp_nr = cm_ag_ut_local_cp_nr,
};

/* Set of aggregation groups per copy machine replica. */
struct m0_cm_aggr_group aggr_grps[MAX_CM_NR][MAX_CM_NR];
struct m0_cm_ag_id ag_ids[MAX_CM_NR][MAX_CM_NR];
uint64_t ag_id_cnt[MAX_CM_NR];
bool test_ready_fop;

static int cm_ut_ag_alloc(struct m0_cm *cm, const struct m0_cm_ag_id *id,
			  bool has_incoming, struct m0_cm_aggr_group **out)
{
	struct m0_ut_cm    *ut_cm = cm2utcm(cm);

	M0_SET0(&aggr_grps[ut_cm->ut_cm_id][id->ai_lo.u_hi]);
	m0_cm_aggr_group_init(&aggr_grps[ut_cm->ut_cm_id][id->ai_lo.u_hi],
			      cm, id, true, &cm_ag_ut_ops);
	*out = &aggr_grps[ut_cm->ut_cm_id][id->ai_lo.u_hi];

	return 0;
}

static int cm_ut_ag_next(struct m0_cm *cm, const struct m0_cm_ag_id *id_curr,
			 struct m0_cm_ag_id *id_next)
{
	struct m0_fid       gfid = {0, 4};
	struct m0_ut_cm    *ut_cm = cm2utcm(cm);
	uint64_t            cid = ut_cm->ut_cm_id;

	if (ag_id_cnt[cid] == MAX_CM_NR || !test_ready_fop) {
		ag_id_cnt[cid] = 0;
		return -ENOENT;
	}

	if (test_ready_fop) {
		M0_SET0(&ag_ids[cid][ag_id_cnt[cid]]);
		ag_ids[cid][ag_id_cnt[cid]].ai_hi.u_hi = gfid.f_container;
		ag_ids[cid][ag_id_cnt[cid]].ai_hi.u_lo = gfid.f_key;
		ag_ids[cid][ag_id_cnt[cid]].ai_lo.u_hi = 0;
		ag_ids[cid][ag_id_cnt[cid]].ai_lo.u_hi = ag_id_cnt[cid];
	}

	*id_next = ag_ids[cid][ag_id_cnt[cid]];
	++ag_id_cnt[cid];

	return 0;
}

static void cm_ut_ha_msg(struct m0_cm *cm, struct m0_ha_msg *msg, int rc)
{
}
static void cm_ut_fini(struct m0_cm *cm)
{
}

static const struct m0_cm_ops cm_ut_ops = {
	.cmo_setup     = cm_ut_setup,
	.cmo_prepare   = cm_ut_prepare,
	.cmo_start     = cm_ut_start,
	.cmo_stop      = cm_ut_stop,
	.cmo_ag_alloc  = cm_ut_ag_alloc,
	.cmo_cp_alloc  = cm_ut_cp_alloc,
	.cmo_data_next = cm_ut_data_next,
	.cmo_ag_next   = cm_ut_ag_next,
	.cmo_ha_msg    = cm_ut_ha_msg,
	.cmo_fini      = cm_ut_fini
};

static int cm_ut_service_allocate(struct m0_reqh_service **service,
				  const struct m0_reqh_service_type *stype)
{
	struct m0_cm *cm = &cm_ut[ut_cm_id].ut_cm;

	*service = &cm->cm_service;
	(*service)->rs_ops = &cm_ut_service_ops;
	(*service)->rs_sm.sm_state = M0_RST_INITIALISING;
	cm_ut[ut_cm_id].ut_cm_id = ut_cm_id;
	m0_mutex_init(&cm_ut[ut_cm_id].ut_cm_wait_mutex);
	m0_chan_init(&cm_ut[ut_cm_id].ut_cm_wait,
		     &cm_ut[ut_cm_id].ut_cm_wait_mutex);
	M0_CNT_INC(ut_cm_id);

	return m0_cm_init(cm, container_of(stype, struct m0_cm_type, ct_stype),
			  &cm_ut_ops);
}

static const struct m0_reqh_service_type_ops cm_ut_service_type_ops = {
	.rsto_service_allocate = cm_ut_service_allocate,
};

M0_CM_TYPE_DECLARE(cm_ut, M0_CM_UT_OPCODE, &cm_ut_service_type_ops, "cm_ut", 0);

struct m0_motr         motr = { .cc_pool_width = 10 };
struct m0_reqh_context rctx = { .rc_motr = &motr };

void cm_ut_service_alloc_init(struct m0_reqh *reqh)
{
	int rc;
	/* Internally calls m0_cm_init(). */
	M0_ASSERT(cm_ut_service == NULL);
	rc = m0_reqh_service_allocate(&cm_ut_service, &cm_ut_cmt.ct_stype,
				      NULL);
	M0_ASSERT(rc == 0);
	m0_reqh_service_init(cm_ut_service, reqh, NULL);
}

void cm_ut_service_cleanup()
{
	m0_reqh_service_prepare_to_stop(cm_ut_service);
	m0_reqh_service_stop(cm_ut_service);
	m0_reqh_service_fini(cm_ut_service);
}

/*
 *  Local variables:
 *  c-indentation-style: "K&R"
 *  c-basic-offset: 8
 *  tab-width: 8
 *  fill-column: 80
 *  scroll-step: 1
 *  End:
 */
