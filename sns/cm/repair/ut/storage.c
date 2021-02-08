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


#define M0_TRACE_SUBSYSTEM M0_TRACE_SUBSYS_SNSCM
#include "lib/trace.h"
#include "lib/misc.h"
#include "lib/locality.h"
#include "lib/finject.h"
#include "lib/hash.h"
#include "reqh/reqh.h"
#include "motr/setup.h"
#include "net/net.h"
#include "sns/cm/repair/ut/cp_common.h"
#include "sns/cm/file.h"
#include "ioservice/fid_convert.h"	/* m0_fid_convert_cob2stob */
#include "sns/cm/cm.h"

M0_INTERNAL void cob_delete(struct m0_cob_domain *cdom,
			    struct m0_be_domain *bedom,
			    uint64_t cont, const struct m0_fid *gfid);

static struct m0_sns_cm         *scm;
static struct m0_reqh           *reqh;
static struct m0_semaphore       sem;
static struct m0_net_buffer_pool nbp;

/* Global structures for write copy packet. */
static struct m0_sns_cm_ag  w_sag;
static struct m0_sns_cm_cp  w_sns_cp;
static struct m0_net_buffer w_buf;

/* Global structures for read copy packet. */
static struct m0_sns_cm_ag  r_sag;
static struct m0_sns_cm_cp  r_sns_cp;
static struct m0_net_buffer r_buf;

static struct m0_fid gob_fid;
static struct m0_fid cob_fid;

static const struct m0_fid M0_SNS_CM_REPAIR_UT_PVER = M0_FID_TINIT('v', 1, 8);

/*
 * Copy packet will typically have a single segment with its size equal to
 * size of copy packet (unit).
 */
enum {
	SEG_NR = 1,
	SEG_SIZE = 4096,
};

/* Over-ridden copy packet FOM fini. */
static void dummy_fom_fini(struct m0_fom *fom)
{
	m0_cm_cp_fini(container_of(fom, struct m0_cm_cp, c_fom));
}

/* Over-ridden copy packet FOM locality (using single locality). */
static uint64_t dummy_fom_locality(const struct m0_fom *fom)
{
	return 0;
}

/*
 * Over-ridden copy packet FOM tick. This is not taken from production code
 * to keep things simple.
 */
static int dummy_fom_tick(struct m0_fom *fom)
{
	int rc;
	struct m0_cm_cp *cp = container_of(fom, struct m0_cm_cp, c_fom);

	M0_ENTRY("cp=%p phase=%d", cp, m0_fom_phase(fom));

	rc = cp->c_ops->co_action[m0_fom_phase(fom)](cp);

	return M0_RC(rc);
}

/* Over-ridden copy packet FOM ops. */
static struct m0_fom_ops dummy_cp_fom_ops = {
	.fo_fini          = dummy_fom_fini,
	.fo_tick          = dummy_fom_tick,
	.fo_home_locality = dummy_fom_locality
};

/* Over-ridden copy packet init phase. */
static int dummy_cp_init(struct m0_cm_cp *cp)
{
	/* This is used to ensure that ast has been posted. */
	m0_semaphore_up(&sem);
	return cp->c_ops->co_phase_next(cp);
}

/*
 * Over-ridden copy packet read phase. This is used when write operation of
 * copy packet has to be tested. In this case, read phase will simply be a
 * passthrough phase.
 */
static int dummy_cp_read(struct m0_cm_cp *cp)
{
	cp->c_io_op = M0_CM_CP_READ;
	cp->c_ops->co_phase_next(cp);
	return M0_FSO_AGAIN;
}

/*
 * Over-ridden copy packet write phase. This is used when read operation of
 * copy packet has to be tested. In this case, write phase will simply be a
 * passthrough phase.
 */
static int dummy_cp_write(struct m0_cm_cp *cp)
{
	cp->c_io_op = M0_CM_CP_WRITE;
	cp->c_ops->co_phase_next(cp);
	return M0_FSO_AGAIN;
}

/* Passthorugh phase for testing purpose. */
static int dummy_cp_phase(struct m0_cm_cp *cp)
{
	return cp->c_ops->co_phase_next(cp);
}

/* Passthorugh for testing purpose. */
static void dummy_cp_complete(struct m0_cm_cp *cp)
{
}

/* Passthorugh for testing purpose. */
static int dummy_cp_fini(struct m0_cm_cp *cp)
{
	return cp->c_ops->co_phase_next(cp);
}

/*
 * Over-ridden copy packet read io wait phase. This is used when write operation
 * of copy packet has to be tested. In this case, read io wait phase will
 * simply be a passthrough phase.
 */
static int dummy_cp_read_io_wait(struct m0_cm_cp *cp)
{
	return cp->c_io_op == M0_CM_CP_READ ?
	       cp->c_ops->co_phase_next(cp) :
	       m0_sns_cm_cp_io_wait(cp);
}

/*
 * Over-ridden copy packet write io wait phase. This is used when read operation
 * of copy packet has to be tested. In this case, write io wait phase will
 * simply be a passthrough phase.
 */
static int dummy_cp_write_io_wait(struct m0_cm_cp *cp)
{
	return cp->c_io_op == M0_CM_CP_WRITE ?
	       cp->c_ops->co_phase_next(cp) :
	       m0_sns_cm_cp_io_wait(cp);
}

const struct m0_cm_cp_ops write_cp_dummy_ops = {
	.co_action = {
		[M0_CCP_INIT]        = &dummy_cp_init,
		[M0_CCP_READ]        = &dummy_cp_read,
		[M0_CCP_IO_WAIT]     = &dummy_cp_read_io_wait,
		[M0_CCP_WRITE_PRE]   = &m0_sns_cm_cp_write_pre,
		[M0_CCP_WRITE]       = &m0_sns_cm_cp_write,
		[M0_CCP_XFORM]       = &dummy_cp_phase,
                [M0_CCP_SW_CHECK]    = &dummy_cp_phase,
                [M0_CCP_SEND]        = &dummy_cp_phase,
		[M0_CCP_SEND_WAIT]   = &dummy_cp_phase,
		[M0_CCP_RECV_INIT]   = &dummy_cp_phase,
		[M0_CCP_RECV_WAIT]   = &dummy_cp_phase,
		[M0_CCP_FAIL]        = &m0_sns_cm_cp_fail,
		[M0_CCP_FINI]        = &dummy_cp_fini,
	},
	.co_action_nr               = M0_CCP_NR,
	.co_phase_next              = &m0_sns_cm_cp_phase_next,
	.co_invariant               = &m0_sns_cm_cp_invariant,
	.co_complete                = &dummy_cp_complete,
};

void write_post(struct m0_pdclust_layout *pdlay)
{
	struct m0_sns_cm_file_ctx  fctx;
	struct m0_motr            *motr;
	struct m0_pool_version    *pv;

	m0_semaphore_init(&sem, 0);
	w_buf.nb_pool = &nbp;
	w_sag.sag_fctx = &fctx;
	cp_prepare(&w_sns_cp.sc_base, &w_buf, SEG_NR, SEG_SIZE,
		   &w_sag, 'e', &dummy_cp_fom_ops, reqh, 0, false, &scm->sc_base);
	scm = cm2sns(w_sag.sag_base.cag_cm);
	motr = m0_cs_ctx_get(scm->sc_base.cm_service.rs_reqh);
	pv = m0_pool_version_find(&motr->cc_pools_common, &M0_SNS_CM_REPAIR_UT_PVER);
	w_sag.sag_fctx->sf_attr.ca_lid = M0_DEFAULT_LAYOUT_ID;
	w_sag.sag_fctx->sf_pm = &pv->pv_mach;
	w_sag.sag_fctx->sf_layout = m0_pdl_to_layout(pdlay);
	w_sns_cp.sc_base.c_ops = &write_cp_dummy_ops;
	m0_fid_convert_cob2stob(&cob_fid, &w_sns_cp.sc_stob_id);
	w_sns_cp.sc_cobfid = M0_MDSERVICE_START_FID;
	w_sag.sag_base.cag_cp_local_nr = 1;
	w_sag.sag_fnr = 1;

	m0_fom_queue(&w_sns_cp.sc_base.c_fom);

	/* Wait till ast gets posted. */
	m0_semaphore_down(&sem);
	m0_reqh_idle_wait(reqh);
}

const struct m0_cm_cp_ops read_cp_dummy_ops = {
	.co_action = {
		[M0_CCP_INIT]        = &dummy_cp_init,
		[M0_CCP_READ]        = &m0_sns_cm_cp_read,
		[M0_CCP_WRITE_PRE]   = &m0_sns_cm_cp_write_pre,
		[M0_CCP_WRITE]       = &dummy_cp_write,
		[M0_CCP_IO_WAIT]     = &dummy_cp_write_io_wait,
		[M0_CCP_XFORM]       = &dummy_cp_phase,
                [M0_CCP_SW_CHECK]    = &dummy_cp_phase,
                [M0_CCP_SEND]        = &dummy_cp_phase,
		[M0_CCP_SEND_WAIT]   = &dummy_cp_phase,
		[M0_CCP_RECV_INIT]   = &dummy_cp_phase,
		[M0_CCP_RECV_WAIT]   = &dummy_cp_phase,
		[M0_CCP_FAIL]        = &m0_sns_cm_cp_fail,
		[M0_CCP_FINI]        = &dummy_cp_fini,
	},
	.co_action_nr               = M0_CCP_NR,
	.co_phase_next              = &m0_sns_cm_cp_phase_next,
	.co_invariant               = &m0_sns_cm_cp_invariant,
	.co_complete                = &dummy_cp_complete,
};

static void read_post(struct m0_pdclust_layout *pdlay)
{
	struct m0_sns_cm_file_ctx fctx;
	struct m0_pool_version    pv;
	struct m0_poolmach        pm;
	m0_semaphore_init(&sem, 0);

	pm.pm_pver = &pv;
	fctx.sf_pm = &pm;
	fctx.sf_layout = m0_pdl_to_layout(pdlay);
	r_buf.nb_pool = &nbp;
	/*
	 * Purposefully fill the read bv with spaces i.e. ' '. This should get
	 * replaced by 'e', when the data is read. This is due to the fact
	 * that write operation is writing 'e' to the bv.
	 */
	cp_prepare(&r_sns_cp.sc_base, &r_buf, SEG_NR, SEG_SIZE,
		   &r_sag, ' ', &dummy_cp_fom_ops, reqh, 0, false, &scm->sc_base);
	r_sns_cp.sc_base.c_ops = &read_cp_dummy_ops;
	r_sag.sag_base.cag_cp_local_nr = 1;
	r_sag.sag_fnr = 1;
	r_sag.sag_fctx = &fctx;
	m0_fid_convert_cob2stob(&cob_fid, &r_sns_cp.sc_stob_id);
	r_sns_cp.sc_cobfid = M0_MDSERVICE_START_FID;
	m0_fom_queue(&r_sns_cp.sc_base.c_fom);

        /* Wait till ast gets posted. */
	m0_semaphore_down(&sem);
	m0_reqh_idle_wait(reqh);
}

static void test_cp_write_read(void)
{
	struct m0_pdclust_layout *pdlay;
	int                       rc;

	scm = NULL;
	reqh = NULL;
	M0_SET0(&sem);
	M0_SET0(&nbp);

	/* Global structures for write copy packet. */
	M0_SET0(&w_sag);
	M0_SET0(&w_sns_cp);
	M0_SET0(&w_buf);

	/* Global structures for read copy packet. */
	M0_SET0(&r_sag);
	M0_SET0(&r_sns_cp);
	M0_SET0(&r_buf);

	M0_SET0(&gob_fid);
	M0_SET0(&cob_fid);


	rc = cs_init_with_ad_stob(&sctx);
	M0_ASSERT(rc == 0);

	m0_fi_enable("m0_sns_cm_tgt_ep", "local-ep");
	m0_fid_gob_make(&gob_fid, 1, M0_MDSERVICE_START_FID.f_key);
	m0_fid_convert_gob2cob(&gob_fid, &cob_fid, 1);
	reqh = m0_cs_reqh_get(&sctx);
	layout_gen(&pdlay, reqh);
	/*
	 * Write using a dummy copy packet. This data which is written, will
	 * be used by the next copy packet to read.
	 */
	write_post(pdlay);

	/* Read the previously written bv. */
	read_post(pdlay);
	layout_destroy(pdlay);

	/*
	 * Compare the bv that is read with the previously written bv.
	 * This verifies the correctness of both write and read operation.
	 */
	bv_compare(&r_buf.nb_buffer, &w_buf.nb_buffer, SEG_NR, SEG_SIZE);

	/*
	 * Ensure the subsequent write on the same offsets frees the previously
	 * used extent before allocating the new extent.
	 */
	m0_fi_enable("ext_punch", "test-ext-release");
	M0_SET0(&w_sns_cp);
	M0_SET0(&r_sns_cp);
	layout_gen(&pdlay, reqh);
	write_post(pdlay);

	read_post(pdlay);
	layout_destroy(pdlay);
	m0_fi_disable("ext_punch", "test-ext-release");

	/* IO failure due to cp_stob_io_init() failure in sns/cm/storage.c */
	m0_fi_enable("cp_stob_io_init", "no-stob");

	M0_SET0(&w_sns_cp);
	M0_SET0(&r_sns_cp);
	layout_gen(&pdlay, reqh);
	write_post(pdlay);

	read_post(pdlay);
	layout_destroy(pdlay);

	m0_fi_disable("cp_stob_io_init", "no-stob");

	/* IO failure test case. */
	m0_fi_enable("cp_io", "io-fail");

	M0_SET0(&w_sns_cp);
	M0_SET0(&r_sns_cp);

	layout_gen(&pdlay, reqh);
	write_post(pdlay);

	read_post(pdlay);
	layout_destroy(pdlay);
	m0_fi_disable("cp_io", "io-fail");

	bv_free(&r_buf.nb_buffer);
	bv_free(&w_buf.nb_buffer);

	cob_delete(scm->sc_cob_dom, reqh->rh_beseg->bs_domain, 1, &gob_fid);
	m0_fi_disable("m0_sns_cm_tgt_ep", "local-ep");

	cs_fini(&sctx);
}

struct m0_ut_suite snscm_storage_ut = {
	.ts_name = "snscm_storage-ut",
	.ts_init = NULL,
	.ts_fini = NULL,
	.ts_tests = {
		{ "cp_write_read", test_cp_write_read },
		{ NULL, NULL }
	}
};

/*
 *  Local variables:
 *  c-indentation-style: "K&R"
 *  c-basic-offset: 8
 *  tab-width: 8
 *  fill-column: 80
 *  scroll-step: 1
 *  End:
 */
