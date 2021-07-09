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
 * Original author: Andriy Tkachuk <andriy.tkachuk@seagate.com>
 * Original creation date: 27-Apr-2021
 */


#define M0_TRACE_SUBSYSTEM M0_TRACE_SUBSYS_LAYOUT
#include "lib/trace.h"     /* M0_LOG */

#include "lib/errno.h"     /* ENOENT */
#include "lib/vec.h"       /* m0_indexvec_alloc */
#include "rpc/rpclib.h"    /* m0_rpc_server_start */
#include "ut/ut.h"
#include "ut/misc.h"
#include "motr/client_internal.h" /* m0_op_obj */
#include "motr/io.h"              /* m0_op_io */
#include "motr/ut/client.h"       /* ut_realm_entity_setup */
#include "layout/plan.h"

static struct m0_client *client_inst = NULL;

#define SERVER_ENDPOINT_ADDR "0@lo:12345:34:1"
#define SERVER_ENDPOINT      "lnet:" SERVER_ENDPOINT_ADDR
#define CLIENT_ENDPOINT_ADDR "0@lo:12345:34:2"

static const char SERVER_LOGFILE[] = "lap_ut.log";

static char *lap_ut_server_args[] = { "m0d", "-T", "AD",
				"-D", "db", "-S", "stob",
				"-A", "linuxstob:addb-stob",
				"-f", M0_UT_CONF_PROCESS,
				"-w", "10",
				"-G", SERVER_ENDPOINT,
				"-e", SERVER_ENDPOINT,
				"-H", SERVER_ENDPOINT_ADDR,
				"-c", M0_SRC_PATH("motr/ut/dix_conf.xc")};

static struct m0_rpc_server_ctx lap_ut_sctx = {
	.rsx_argv = lap_ut_server_args,
	.rsx_argc = ARRAY_SIZE(lap_ut_server_args),
	.rsx_log_file_name = SERVER_LOGFILE,
};

static int lap_ut_server_start(void)
{
	lap_ut_sctx.rsx_xprts    = m0_net_all_xprt_get();
	lap_ut_sctx.rsx_xprts_nr = m0_net_xprt_nr();

	return m0_rpc_server_start(&lap_ut_sctx);
}

static void lap_ut_server_stop(void)
{
	m0_rpc_server_stop(&lap_ut_sctx);
}

static struct m0_idx_dix_config dix_conf = {
	.kc_create_meta = false,
};

static struct m0_config client_conf = {
	.mc_is_oostore     = true,
	.mc_is_read_verify = false,
	.mc_layout_id      = 1,
	.mc_local_addr     = CLIENT_ENDPOINT_ADDR,
	.mc_ha_addr        = SERVER_ENDPOINT_ADDR,
	.mc_profile        = M0_UT_CONF_PROFILE,
	.mc_process_fid    = M0_UT_CONF_PROCESS,
	.mc_tm_recv_queue_min_len = M0_NET_TM_RECV_QUEUE_DEF_LEN,
	.mc_max_rpc_msg_size      = M0_RPC_DEF_MAX_RPC_MSG_SIZE,
	.mc_idx_service_id        = M0_IDX_DIX,
	.mc_idx_service_conf      = &dix_conf,
};

static int lap_ut_client_start(void)
{
	int rc;

	m0_fi_enable_once("ha_init", "skip-ha-init");
	/* Skip HA finalisation in case of failure path. */
	m0_fi_enable("ha_fini", "skip-ha-fini");
	/*
	 * We can't use m0_fi_enable_once() here, because
	 * initlift_addb2() may be called twice in case of failure path.
	 */
	m0_fi_enable("initlift_addb2", "no-addb2");
	m0_fi_enable("ha_process_event", "no-link");
	rc = m0_client_init(&client_inst, &client_conf, false);
	M0_ASSERT_INFO(rc == 0, "rc=%d", rc);
	m0_fi_disable("ha_process_event", "no-link");
	m0_fi_disable("initlift_addb2", "no-addb2");
	m0_fi_disable("ha_fini", "skip-ha-fini");

	return 0;
}

static void lap_ut_client_stop(void)
{
	m0_fi_enable_once("ha_fini", "skip-ha-fini");
	m0_fi_enable_once("initlift_addb2", "no-addb2");
	m0_fi_enable("ha_process_event", "no-link");
	m0_client_fini(client_inst, false);
	m0_fi_disable("ha_process_event", "no-link");
}

static void test_plan_build_fini(void)
{
	int                         rc;
	struct m0_client           *cinst = client_inst;
	struct m0_pool_version     *pv;
	struct m0_layout_plan      *plan;
	struct m0_op               *op = NULL;
	struct m0_indexvec          ext;
	struct m0_bufvec            data;
	struct m0_bufvec            attr;
	struct m0_realm             realm;
	struct m0_obj               obj = {};

	M0_ENTRY();

	M0_UT_ASSERT(m0_indexvec_alloc(&ext, 1) == 0);
	ext.iv_vec.v_count[0] = UT_DEFAULT_BLOCK_SIZE;
	M0_UT_ASSERT(m0_bufvec_alloc(&data, 1, UT_DEFAULT_BLOCK_SIZE) == 0);
	M0_UT_ASSERT(m0_bufvec_alloc(&attr, 1, 1) == 0);

	rc = m0_pool_version_get(&cinst->m0c_pools_common, NULL, &pv);
	M0_UT_ASSERT(rc == 0);
	ut_realm_entity_setup(&realm, &obj.ob_entity, cinst);
	obj.ob_attr.oa_bshift = M0_MIN_BUF_SHIFT;
	obj.ob_attr.oa_pver   = pv->pv_id;
	obj.ob_attr.oa_layout_id = M0_DEFAULT_LAYOUT_ID;

	rc = m0_obj_op(&obj, M0_OC_READ, &ext, &data, &attr, 0, 0, &op);
	M0_UT_ASSERT(rc == 0);

	/* check happy path */
	plan = m0_layout_plan_build(op);
	M0_UT_ASSERT(plan != NULL);

	m0_layout_plan_fini(plan);

	/* check error paths */
	m0_fi_enable_once("pargrp_iomap_init", "no-mem-err");
	plan = m0_layout_plan_build(op);
	M0_UT_ASSERT(plan == NULL);

	m0_fi_enable_once("target_ioreq_init", "no-mem-err");
	plan = m0_layout_plan_build(op);
	M0_UT_ASSERT(plan == NULL);

	m0_op_fini(op);
	m0_op_free(op);

	m0_entity_fini(&obj.ob_entity);

	m0_bufvec_free(&attr);
	m0_bufvec_free(&data);
	m0_indexvec_free(&ext);

	M0_LEAVE();
}

static void test_plan_get_done(void)
{
	int                         rc;
	struct m0_client           *cinst = client_inst;
	struct m0_pool_version     *pv;
	struct m0_layout_plan      *plan;
	struct m0_op               *op = NULL;
	struct m0_layout_plop      *plop;
	struct m0_layout_io_plop   *iopl;
	struct m0_layout_plop_rel  *plrel;
	struct m0_indexvec          ext;
	struct m0_bufvec            data;
	struct m0_bufvec            attr;
	struct m0_realm             realm;
	struct m0_obj               obj = {};

	M0_ENTRY();

	M0_UT_ASSERT(m0_indexvec_alloc(&ext, 2) == 0);
	ext.iv_vec.v_count[0] = UT_DEFAULT_BLOCK_SIZE;
	ext.iv_vec.v_count[1] = UT_DEFAULT_BLOCK_SIZE;
	ext.iv_index[0] = 0;
	ext.iv_index[1] = UT_DEFAULT_BLOCK_SIZE;
	M0_UT_ASSERT(m0_bufvec_alloc(&data, 2, UT_DEFAULT_BLOCK_SIZE) == 0);
	M0_UT_ASSERT(m0_bufvec_alloc(&attr, 2, 1) == 0);

	rc = m0_pool_version_get(&cinst->m0c_pools_common, NULL, &pv);
	M0_UT_ASSERT(rc == 0);
	ut_realm_entity_setup(&realm, &obj.ob_entity, cinst);
	obj.ob_attr.oa_bshift = M0_MIN_BUF_SHIFT;
	obj.ob_attr.oa_pver   = pv->pv_id;
	obj.ob_attr.oa_layout_id = M0_DEFAULT_LAYOUT_ID;

	rc = m0_obj_op(&obj, M0_OC_READ, &ext, &data, &attr, 0, 0, &op);
	M0_UT_ASSERT(rc == 0);

	plan = m0_layout_plan_build(op);
	M0_UT_ASSERT(plan != NULL);

	/* 1st unit */
	rc = m0_layout_plan_get(plan, 0, &plop);
	M0_UT_ASSERT(rc == 0);
	M0_UT_ASSERT(plop != NULL);
	M0_UT_ASSERT(plop->pl_type == M0_LAT_READ);
	M0_UT_ASSERT(plop->pl_ent.f_container != 0 ||
		     plop->pl_ent.f_key != 0);
	iopl = container_of(plop, struct m0_layout_io_plop, iop_base);
	M0_UT_ASSERT(iopl->iop_session != NULL);
	M0_UT_ASSERT(iopl->iop_ext.iv_index != NULL);
	M0_UT_ASSERT(iopl->iop_goff == 0);
	M0_UT_ASSERT(m0_vec_count(&iopl->iop_ext.iv_vec) ==
		                                UT_DEFAULT_BLOCK_SIZE);
	M0_UT_ASSERT(iopl->iop_data.ov_buf != NULL);

	m0_layout_plop_start(plop);
	plop->pl_rc = 0;
	m0_layout_plop_done(plop);

	rc = m0_layout_plan_get(plan, 0, &plop);
	M0_UT_ASSERT(rc == 0);
	M0_UT_ASSERT(plop != NULL);
	M0_UT_ASSERT(plop->pl_type == M0_LAT_OUT_READ);
	plrel = pldeps_tlist_head(&plop->pl_deps);
	M0_UT_ASSERT(plrel != NULL);
	M0_UT_ASSERT(plrel->plr_dep == &iopl->iop_base);
	M0_UT_ASSERT(plrel->plr_rdep == plop);
	m0_layout_plop_start(plop);
	m0_layout_plop_done(plop);

	/* 2nd unit */
	rc = m0_layout_plan_get(plan, 0, &plop);
	M0_UT_ASSERT(rc == 0);
	M0_UT_ASSERT(plop != NULL);
	M0_UT_ASSERT(plop->pl_type == M0_LAT_READ);
	M0_UT_ASSERT(plop->pl_ent.f_container != 0 ||
		     plop->pl_ent.f_key != 0);
	iopl = container_of(plop, struct m0_layout_io_plop, iop_base);
	M0_UT_ASSERT(iopl->iop_session != NULL);
	M0_UT_ASSERT(iopl->iop_ext.iv_index != NULL);
	M0_UT_ASSERT(iopl->iop_goff == UT_DEFAULT_BLOCK_SIZE);
	M0_UT_ASSERT(m0_vec_count(&iopl->iop_ext.iv_vec) ==
		                                UT_DEFAULT_BLOCK_SIZE);
	M0_UT_ASSERT(iopl->iop_data.ov_buf != NULL);

	m0_layout_plop_start(plop);
	plop->pl_rc = 0;
	m0_layout_plop_done(plop);

	rc = m0_layout_plan_get(plan, 0, &plop);
	M0_UT_ASSERT(rc == 0);
	M0_UT_ASSERT(plop != NULL);
	M0_UT_ASSERT(plop->pl_type == M0_LAT_OUT_READ);
	plrel = pldeps_tlist_head(&plop->pl_deps);
	M0_UT_ASSERT(plrel != NULL);
	M0_UT_ASSERT(plrel->plr_dep == &iopl->iop_base);
	M0_UT_ASSERT(plrel->plr_rdep == plop);
	m0_layout_plop_start(plop);
	m0_layout_plop_done(plop);

	rc = m0_layout_plan_get(plan, 0, &plop);
	M0_UT_ASSERT(rc == 0);
	M0_UT_ASSERT(plop != NULL);
	M0_UT_ASSERT(plop->pl_type == M0_LAT_DONE);
	m0_layout_plop_start(plop);
	m0_layout_plop_done(plop);

	m0_layout_plan_fini(plan);

	m0_op_fini(op);
	m0_op_free(op);

	m0_entity_fini(&obj.ob_entity);

	m0_bufvec_free(&attr);
	m0_bufvec_free(&data);
	m0_indexvec_free(&ext);

	M0_LEAVE();
}

/*
 * Note: In test_init() and test_fini(), need to use M0_ASSERT()
 * instead of M0_UT_ASSERT().
 */
static int lap_ut_init(void)
{
	int rc;

	rc = lap_ut_server_start();
	M0_ASSERT(rc == 0);

	rc = lap_ut_client_start();
	M0_ASSERT(rc == 0);

	return 0;
}

static int lap_ut_fini(void)
{
	lap_ut_client_stop();
	lap_ut_server_stop();

	return 0;
}

struct m0_ut_suite layout_access_plan_ut = {
	.ts_name  = "layout-access-plan-ut",
	.ts_owners = "Andriy T.",
	.ts_init  = lap_ut_init,
	.ts_fini  = lap_ut_fini,
	.ts_tests = {
		{ "layout-access-plan-build-fini", test_plan_build_fini },
		{ "layout-access-plan-get-done", test_plan_get_done },
		{ NULL, NULL }
	}
};
M0_EXPORTED(layout_access_plan_ut);

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
