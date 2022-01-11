/* -*- C -*- */
/*
 * Copyright (c) 2017-2020 Seagate Technology LLC and/or its Affiliates
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


#define M0_TRACE_SUBSYSTEM M0_TRACE_SUBSYS_FDMI
#include "lib/trace.h"

#include "lib/memory.h"
#include "ut/ut.h"
struct m0_rpc_conn;
#include "rpc/rpc_machine_internal.h"  /* m0_rpc_machine_lock */
#include "rpc/session_internal.h"      /* m0_rpc_session_hold_busy */
#include "rpc/item_internal.h"         /* m0_rpc_item_sm_init */
#include "fdmi/fdmi.h"
#include "fdmi/service.h"              /* m0_reqh_fdmi_service */
#include "fdmi/source_dock_internal.h"
#include "fdmi/fops.h"                 /* m0_fop_fdmi_rec_release */

#include "fdmi/ut/sd_common.h"

M0_TL_DESCR_DECLARE(fdmi_record_inflight, M0_EXTERN);
M0_TL_DECLARE(fdmi_record_inflight, M0_EXTERN, struct m0_fdmi_src_rec);

static struct test_rpc_env    g_rpc_env;
static struct m0_rpc_packet  *g_sent_rpc_packet;

static int test_packet_ready(struct m0_rpc_packet *p);

static const struct m0_rpc_frm_ops test_frm_ops = {
	.fo_packet_ready = test_packet_ready,
};


static struct m0_semaphore     g_sem;
static struct m0_semaphore     g_sem2;
static struct m0_semaphore     g_sem3;
static struct m0_uint128 rec_id_to_release = M0_UINT128(0xDEAD, 0xBEEF);
static struct m0_fdmi_src_rec  g_src_rec;
static int                     g_refcount;

static int test_fs_node_eval(
	        struct m0_fdmi_src_rec *src_rec,
		struct m0_fdmi_flt_var_node *value_desc,
		struct m0_fdmi_flt_operand *value)
{
	M0_UT_ASSERT(false);
	return 0;
}

static int test_fs_encode(struct m0_fdmi_src_rec *src_rec,
			   struct m0_buf          *buf)
{
	M0_UT_ASSERT(false);
	return 0;
}

static void test_fs_get(struct m0_fdmi_src_rec *src_rec)
{
	++g_refcount;
}

static void test_fs_put(struct m0_fdmi_src_rec *src_rec)
{
	M0_UT_ASSERT(src_rec != NULL);
	M0_UT_ASSERT(!m0_uint128_cmp(&src_rec->fsr_rec_id, &rec_id_to_release));
	/**
	 * Calling of this function is a sign that FDMI
	 * finished release FOP handling.
	 */
	m0_semaphore_up(&g_sem);
	--g_refcount;
	if (g_refcount == 0)
		m0_semaphore_up(&g_sem2);
}

static struct m0_fdmi_src *src_alloc()
{
	struct m0_fdmi_src *src;
	int                 rc;

	rc = m0_fdmi_source_alloc(M0_FDMI_REC_TYPE_TEST, &src);
	M0_UT_ASSERT(rc == 0);

	src->fs_encode     = test_fs_encode;
	src->fs_get        = test_fs_get;
	src->fs_put        = test_fs_put;
	src->fs_node_eval  = test_fs_node_eval;
	return src;
}

int test_packet_ready(struct m0_rpc_packet *p)
{
	g_sent_rpc_packet = p;

	m0_semaphore_up(&g_sem);
	return 0;
}

static void my_fop_release(struct m0_ref *ref)
{
	/* call Motr default fop release */
	m0_fop_release(ref);
	m0_semaphore_up(&g_sem3);
}
int imitate_release_fop_recv(struct test_rpc_env *env)
{
	int                              rc;
	struct m0_fop                   *fop;
	struct m0_fop_fdmi_rec_release  *fop_data;
	struct m0_reqh                  *reqh;
	struct m0_rpc_item              *rpc_item;

	M0_ENTRY();

	reqh = &g_sd_ut.motr.cc_reqh_ctx.rc_reqh;

	fop = m0_fop_alloc(&m0_fop_fdmi_rec_release_fopt, NULL,
			   &env->tre_rpc_machine);
	M0_UT_ASSERT(fop != NULL);

	/* Overwrite to use my own fop release */
	m0_ref_init(&fop->f_ref, 1, my_fop_release);

	fop_data = m0_fop_data(fop);
	fop_data->frr_frid = rec_id_to_release;
	fop_data->frr_frt  = M0_FDMI_REC_TYPE_TEST;
	rpc_item = &fop->f_item;

	m0_fop_rpc_machine_set(fop, &env->tre_rpc_machine);
	m0_rpc_item_sm_init(rpc_item, M0_RPC_ITEM_INCOMING);

	rpc_item->ri_session = env->tre_session;

	m0_rpc_machine_lock(&env->tre_rpc_machine);
	m0_rpc_item_change_state(rpc_item, M0_RPC_ITEM_ACCEPTED);
	m0_rpc_machine_unlock(&env->tre_rpc_machine);

	m0_rpc_machine_lock(&env->tre_rpc_machine);
	m0_rpc_session_hold_busy(env->tre_session);
	m0_rpc_machine_unlock(&env->tre_rpc_machine);

	rc = m0_reqh_fop_handle(reqh, fop);
	m0_fop_put_lock(fop);

	M0_LEAVE();
	return rc;
}

static void g_src_rec_free(struct m0_ref *ref)
{
}
void fdmi_sd_release_fom(void)
{
	struct m0_fdmi_src             *src = src_alloc();
	int                             rc;
	static struct m0_rpc_conn       rpc_conn;
	static struct m0_rpc_session    rpc_session;
	bool                            ok;
	struct m0_fdmi_src_dock        *src_dock = m0_fdmi_src_dock_get();

	M0_ENTRY();

	fdmi_serv_start_ut(&filterc_stub_ops);
	prepare_rpc_env(&g_rpc_env, &g_sd_ut.motr.cc_reqh_ctx.rc_reqh,
			&test_frm_ops, false, &rpc_conn, &rpc_session);
	m0_semaphore_init(&g_sem, 0);
	m0_semaphore_init(&g_sem2, 0);
	m0_semaphore_init(&g_sem3, 0);
	rc = m0_fdmi_source_register(src);
	M0_UT_ASSERT(rc == 0);
	g_src_rec.fsr_src = src;
	m0_fdmi__record_init(&g_src_rec);
	m0_fdmi__rec_id_gen(&g_src_rec);
	m0_ref_init(&g_src_rec.fsr_ref, 1, g_src_rec_free);
	m0_fdmi__fs_get(&g_src_rec);
	fdmi_record_inflight_tlist_add_tail(
				&src_dock->fsdc_rec_inflight, &g_src_rec);
	rec_id_to_release = g_src_rec.fsr_rec_id;
	rc = imitate_release_fop_recv(&g_rpc_env);
	M0_UT_ASSERT(rc == 0);

	/**
	 * Wait until record is processed and released.  Must happen within 10
	 * sec, otherwise we consider it a failure.
	 */
	ok = m0_semaphore_timeddown(&g_sem, m0_time_from_now(10, 0));
	M0_UT_ASSERT(ok);

	/**
	 * Wait until record is sent over RPC.  Must happen within 10 sec,
	 * otherwise we consider it a failure.
	 */
	ok = m0_semaphore_timeddown(&g_sem, m0_time_from_now(10, 0));
	M0_UT_ASSERT(ok);
	fdmi_ut_packet_send_failed(&g_rpc_env.tre_rpc_machine,
				   g_sent_rpc_packet);
	ok = m0_semaphore_timeddown(&g_sem2, m0_time_from_now(10, 0));
	M0_UT_ASSERT(ok);
	m0_fdmi__record_deinit(&g_src_rec);
	m0_fdmi_source_deregister(src);
	m0_fdmi_source_free(src);
	m0_semaphore_fini(&g_sem);
	m0_semaphore_fini(&g_sem2);
	ok = m0_semaphore_timeddown(&g_sem3, m0_time_from_now(10, 0));
	M0_UT_ASSERT(ok);
	m0_semaphore_fini(&g_sem3);
	unprepare_rpc_env(&g_rpc_env);
	fdmi_serv_stop_ut();
	M0_LEAVE();
}


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
