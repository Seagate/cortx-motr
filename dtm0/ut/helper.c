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

#include "dtm0/ut/helper.h"

#include "ut/ut.h"              /* M0_UT_ASSERT */

#include "lib/misc.h"           /* M0_IS0 */
#include "net/net.h"            /* m0_net_all_xprt_get */
#include "dtm0/service.h"       /* m0_dtm0_service */
#include "dtm0/cfg_default.h"   /* m0_dtm0_domain_cfg_default_dup */
#include "dtm0/dtm0.h"          /* m0_dtm0_redo */
#include "conf/objs/common.h"   /* M0_CONF__SDEV_FT_ID */
#include "be/tx_bulk.h"         /* m0_be_tx_bulk */
struct m0_reqh_service;

enum {
	M0_DTM0_UT_LOG_SIMPLE_SEG_SIZE  = 0x2000000,
	MAX_RPCS_IN_FLIGHT = 10,
};

#define SERVER_ENDPOINT_ADDR   "0@lo:12345:34:1"
#define SERVER_ENDPOINT        M0_NET_XPRT_PREFIX_DEFAULT":"SERVER_ENDPOINT_ADDR
#define DTM0_UT_CONF_PROCESS   "<0x7200000000000001:5>"

char *ut_dtm0_helper_argv[] = {
	"m0d", "-T", "linux",
	"-D", "dtm0_sdb", "-S", "dtm0_stob",
	"-A", "linuxstob:dtm0_addb_stob",
	"-e", SERVER_ENDPOINT,
	"-H", SERVER_ENDPOINT_ADDR,
	"-w", "10",
	"-f", DTM0_UT_CONF_PROCESS,
	"-c", M0_SRC_PATH("dtm0/conf.xc")
};
static const char *ut_dtm0_client_endpoint = "0@lo:12345:34:2";
const char        *ut_dtm0_helper_log      = "dtm0_ut_server.log";


M0_INTERNAL void m0_ut_dtm0_helper_init(struct m0_ut_dtm0_helper *udh)
{
	struct m0_reqh_service *svc;
	int                     rc;

	M0_PRE(M0_IS0(udh));

	*udh = (struct m0_ut_dtm0_helper){
		.udh_sctx = {
			.rsx_xprts         = m0_net_all_xprt_get(),
			.rsx_xprts_nr      = m0_net_xprt_nr(),
			.rsx_argv          = ut_dtm0_helper_argv,
			.rsx_argc          = ARRAY_SIZE(ut_dtm0_helper_argv),
			.rsx_log_file_name = ut_dtm0_helper_log,
		},
		.udh_cctx = {
			.rcx_net_dom            = &udh->udh_client_net_domain,
			.rcx_local_addr         = ut_dtm0_client_endpoint,
			.rcx_remote_addr        = SERVER_ENDPOINT_ADDR,
			.rcx_max_rpcs_in_flight = MAX_RPCS_IN_FLIGHT,
			.rcx_fid                = &g_process_fid,
		},
		.udh_server_reqh =
			&udh->udh_sctx.rsx_motr_ctx.cc_reqh_ctx.rc_reqh,
		.udh_client_reqh = &udh->udh_cctx.rcx_reqh,
		.udh_server_dtm0_fid = M0_FID_INIT(0x7300000000000001, 0x1c),
		.udh_client_dtm0_fid = M0_FID_INIT(0x7300000000000001, 0x1a),
	};
	rc = m0_rpc_server_start(&udh->udh_sctx);
	M0_UT_ASSERT(rc == 0);
	rc = m0_net_domain_init(&udh->udh_client_net_domain,
				m0_net_xprt_default_get());
	M0_UT_ASSERT(rc == 0);
	rc = m0_rpc_client_start(&udh->udh_cctx);
	M0_UT_ASSERT(rc == 0);
	rc = m0_dtm_client_service_start(udh->udh_client_reqh,
					 &udh->udh_client_dtm0_fid,
					 &svc);
	M0_UT_ASSERT(rc == 0);

	udh->udh_client_dtm0_service = container_of(svc, struct m0_dtm0_service,
						    dos_generic);

	svc = m0_reqh_service_lookup(udh->udh_server_reqh,
	                             &udh->udh_server_dtm0_fid);
	/* TODO export the function which does bob_of() */
	udh->udh_server_dtm0_service = container_of(svc, struct m0_dtm0_service,
	                                            dos_generic);
}

M0_INTERNAL void m0_ut_dtm0_helper_fini(struct m0_ut_dtm0_helper *udh)
{
	int rc;

	m0_dtm_client_service_stop(&udh->udh_client_dtm0_service->dos_generic);
	rc = m0_rpc_client_stop(&udh->udh_cctx);
	M0_UT_ASSERT(rc == 0);
	m0_net_domain_fini(&udh->udh_client_net_domain);
	m0_rpc_server_stop(&udh->udh_sctx);
}

M0_INTERNAL struct dtm0_ut_log_ctx *dtm0_ut_log_init(void)
{
	struct dtm0_ut_log_ctx *lctx;
	int                     rc;

	M0_ALLOC_PTR(lctx);
	M0_UT_ASSERT(lctx != NULL);

	m0_be_ut_backend_init(&lctx->ut_be);
	m0_be_ut_seg_init(&lctx->ut_seg, &lctx->ut_be,
			  M0_DTM0_UT_LOG_SIMPLE_SEG_SIZE);
	rc = m0_dtm0_domain_cfg_default_dup(&lctx->dod_cfg, true);
	M0_UT_ASSERT(rc == 0);
	lctx->dod_cfg.dodc_log.dlc_be_domain = &lctx->ut_be.but_dom;
	lctx->dod_cfg.dodc_log.dlc_seg =
		m0_be_domain_seg_first(lctx->dod_cfg.dodc_log.dlc_be_domain);

	rc = m0_dtm0_log_create(&lctx->dol, &lctx->dod_cfg.dodc_log);
	M0_UT_ASSERT(rc == 0);

	return lctx;
}

M0_INTERNAL void dtm0_ut_log_fini(struct dtm0_ut_log_ctx *lctx)
{
	m0_dtm0_log_destroy(&lctx->dol);
	m0_dtm0_domain_cfg_free(&lctx->dod_cfg);
	m0_be_ut_seg_fini(&lctx->ut_seg);
	m0_be_ut_backend_fini(&lctx->ut_be);
	m0_free(lctx);

}

enum {
	M0_DTM0_UT_LOG_SIMPLE_REDO_SIZE = 0x100,
};

M0_INTERNAL struct m0_dtm0_redo *dtm0_ut_redo_get(int timestamp)
{
	int                  rc;
	struct m0_buf       *redo_buf;
	struct m0_dtm0_redo *redo;
	static struct m0_fid p_sdev_fid;
	uint64_t             seed = 42;
	int                  i;

	M0_ALLOC_PTR(redo);
	M0_UT_ASSERT(redo != NULL);

	M0_ALLOC_PTR(redo_buf);
	M0_UT_ASSERT(redo_buf != NULL);

	rc = m0_buf_alloc(redo_buf, M0_DTM0_UT_LOG_SIMPLE_REDO_SIZE);
	M0_UT_ASSERT(rc == 0);
	for (i = 0; i < redo_buf->b_nob; ++i)
		((char *)redo_buf->b_addr)[i] = m0_rnd64(&seed) & 0xff;
	p_sdev_fid = M0_FID_TINIT(M0_CONF__SDEV_FT_ID, 1, 2);

	*redo = (struct m0_dtm0_redo){
		.dtr_descriptor = {
			.dtd_id = {
				.dti_timestamp           = timestamp,
				.dti_originator_sdev_fid = p_sdev_fid,
			},
			.dtd_participants = {
				.dtpa_participants_nr = 1,
				.dtpa_participants    = &p_sdev_fid,
			},
		},
		.dtr_payload = {
			.dtp_type = M0_DTX0_PAYLOAD_BLOB,
			.dtp_data = {
				.ab_count = 1,
				.ab_elems = redo_buf,
			},
		},
	};

	return redo;
}

M0_INTERNAL void dtm0_ut_redo_put(struct m0_dtm0_redo *redo)
{
	/* TODO */
}


static void dtm0_ut_log_produce_redo_add(struct m0_be_tx_bulk   *tb,
					 struct dtm0_ut_log_ctx *lctx)
{
	int i;
	struct m0_dtm0_redo   *redo;
	struct m0_be_tx_credit credit;

	for (i = 0; i < MPSC_NR_REC_TOTAL; ++i) {
		redo = dtm0_ut_redo_get(i);
		credit = M0_BE_TX_CREDIT(0, 0);
		m0_dtm0_log_redo_add_credit(&lctx->dol, redo, &credit);
		M0_BE_OP_SYNC(op,
			      m0_be_tx_bulk_put(tb, &op, &credit, 0, 0, redo));
	}

	m0_be_tx_bulk_end(tb);
}


static void dtm0_ut_log_tx_bulk_parallel_do(struct m0_be_tx_bulk *tb,
					    struct m0_be_tx      *tx,
					    struct m0_be_op      *op,
					    void                 *datum,
					    void                 *user,
					    uint64_t              worker_index,
					    uint64_t              partition)
{
	struct dtm0_ut_log_ctx *lctx = datum;
	struct m0_dtm0_redo    *redo = user;
	struct m0_fid p_sdev_fid;

	(void) worker_index;
	(void) partition;

	p_sdev_fid = M0_FID_TINIT(M0_CONF__SDEV_FT_ID, 1, 2);

	m0_be_op_active(op);
	m0_dtm0_log_redo_add(&lctx->dol, tx, redo, &p_sdev_fid);
	m0_be_op_done(op);

	dtm0_ut_redo_put(redo);
}

static void dtm0_ut_log_tx_bulk_parallel_done(struct m0_be_tx_bulk *tb,
					      void                 *datum,
					      void                 *user,
					      uint64_t              worker_index,
					      uint64_t              partition)
{
}

M0_INTERNAL void dtm0_ut_log_mp_init(struct dtm0_ut_log_mp_ctx *lmp_ctx,
				     struct dtm0_ut_log_ctx    *lctx)
{
	struct m0_be_tx_bulk     *tb;
	struct m0_be_tx_bulk_cfg *tb_cfg;
	struct m0_be_op          *op;
	int                       rc;

	M0_ALLOC_PTR(tb);
	M0_UT_ASSERT(tb != NULL);

	M0_ALLOC_PTR(tb_cfg);
	M0_UT_ASSERT(tb_cfg != NULL);

	M0_ALLOC_PTR(op);
	M0_UT_ASSERT(op != NULL);

	m0_be_op_init(op);

	*tb_cfg = (struct m0_be_tx_bulk_cfg) {
		.tbc_q_cfg                 = {
			.bqc_q_size_max       = MPSC_NR_REC_TOTAL / 2,
			.bqc_producers_nr_max = 1,
		},
		.tbc_workers_nr            = 100,
		.tbc_partitions_nr         = 1,
		.tbc_work_items_per_tx_max = 1,
		.tbc_datum                 = lctx,
		.tbc_do                    =
			&dtm0_ut_log_tx_bulk_parallel_do,
		.tbc_done                  =
			&dtm0_ut_log_tx_bulk_parallel_done,
		.tbc_dom                   = &lctx->ut_be.but_dom,
	};

	rc = m0_be_tx_bulk_init(tb, tb_cfg);
	M0_UT_ASSERT(rc == 0);

	lmp_ctx->op = op;
	lmp_ctx->lctx = lctx;
	lmp_ctx->tb_cfg = tb_cfg;
	lmp_ctx->tb = tb;
}

M0_INTERNAL void dtm0_ut_log_mp_run(struct dtm0_ut_log_mp_ctx *lmp_ctx)
{
	m0_be_tx_bulk_run(lmp_ctx->tb, lmp_ctx->op);
	dtm0_ut_log_produce_redo_add(lmp_ctx->tb, lmp_ctx->lctx);
}

M0_INTERNAL void dtm0_ut_log_mp_fini(struct dtm0_ut_log_mp_ctx *lmp_ctx)
{
	int rc;
	rc = m0_be_tx_bulk_status(lmp_ctx->tb);
	M0_UT_ASSERT(rc == 0);

	m0_be_op_fini(lmp_ctx->op);
	m0_free(lmp_ctx->op);

	m0_be_tx_bulk_fini(lmp_ctx->tb);
	m0_free(lmp_ctx->tb);
	m0_free(lmp_ctx->tb_cfg);
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
