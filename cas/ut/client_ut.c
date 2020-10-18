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


/**
 * @addtogroup cas
 *
 * @{
 */

#define M0_TRACE_SUBSYSTEM M0_TRACE_SUBSYS_CAS
#include "lib/trace.h"

#include "rpc/rpclib.h"                /* m0_rpc_server_ctx */
#include "lib/finject.h"
#include "lib/memory.h"
#include "ut/misc.h"                   /* M0_UT_PATH */
#include "ut/ut.h"
#include "cas/client.h"
#include "cas/ctg_store.h"             /* m0_ctg_recs_nr */
#include "lib/finject.h"
#include "dtm0/dtx.h"                  /* m0_dtm0_dtx */
#include "cas/cas.h"                   /* m0_crv      */

#define SERVER_LOG_FILE_NAME       "cas_server.log"
#define IFID(x, y) M0_FID_TINIT('i', (x), (y))

extern const struct m0_tl_descr ndoms_descr;

enum {
	/**
	 * @todo Greater number of indices produces -E2BIG error in idx-deleteN
	 * test case.
	 */
	COUNT = 24,
	COUNT_TREE = 10,
	COUNT_VAL_BYTES = 4096,
	COUNT_META_ENTRIES = 3
};

enum idx_operation {
	IDX_CREATE,
	IDX_DELETE
};

M0_BASSERT(COUNT % 2 == 0);

struct async_wait {
	struct m0_clink     aw_clink;
	struct m0_semaphore aw_cb_wait;
	bool                aw_done;
};

/* Client context */
struct cl_ctx {
	/* Client network domain.*/
	struct m0_net_domain     cl_ndom;
	/* Client rpc context.*/
	struct m0_rpc_client_ctx cl_rpc_ctx;
	struct async_wait        cl_wait;
};

enum { MAX_RPCS_IN_FLIGHT = 10 };
/* Configures motr environment with given parameters. */
static char *cas_startup_cmd[] = {
	"m0d", "-T", "linux",
	"-D", "cs_sdb", "-S", "cs_stob",
	"-A", "linuxstob:cs_addb_stob",
	"-e", M0_NET_XPRT_PREFIX_DEFAULT":0@lo:12345:34:1",
	"-H", "0@lo:12345:34:1",
	"-w", "10", "-F",
	"-f", M0_UT_CONF_PROCESS,
	"-c", M0_SRC_PATH("cas/ut/conf.xc")
	/* FIXME If DTM is enabled, the above conf.xc must be updated to include
	 * DTM0 services. */
};

static const char         *cdbnames[] = { "cas1" };
static const char      *cl_ep_addrs[] = { "0@lo:12345:34:2" };
static const char     *srv_ep_addrs[] = { "0@lo:12345:34:1" };

static struct cl_ctx            casc_ut_cctx;
static struct m0_rpc_server_ctx casc_ut_sctx = {
		.rsx_argv             = cas_startup_cmd,
		.rsx_argc             = ARRAY_SIZE(cas_startup_cmd),
		.rsx_log_file_name    = SERVER_LOG_FILE_NAME
};

static int bufvec_empty_alloc(struct m0_bufvec *bufvec,
			      uint32_t          num_segs)
{
	M0_UT_ASSERT(num_segs > 0);
	bufvec->ov_buf = NULL;
	bufvec->ov_vec.v_nr = num_segs;
	M0_ALLOC_ARR(bufvec->ov_vec.v_count, num_segs);
	M0_UT_ASSERT(bufvec->ov_vec.v_count != NULL);
	M0_ALLOC_ARR(bufvec->ov_buf, num_segs);
	M0_UT_ASSERT(bufvec->ov_buf != NULL);
	return 0;
}

static int bufvec_cmp(const struct m0_bufvec *left,
		      const struct m0_bufvec *right)
{
	struct m0_bufvec_cursor rcur;
	struct m0_bufvec_cursor lcur;
	m0_bufvec_cursor_init(&lcur, left);
	m0_bufvec_cursor_init(&rcur, right);
	return m0_bufvec_cursor_cmp(&lcur, &rcur);
}


static void value_create(int size, int num, char *buf)
{
	int j;

	if (size == sizeof(uint64_t))
		*(uint64_t *)buf = num;
	else {
		M0_UT_ASSERT(size > num);
		for (j = 1; j <= num + 1; j++)
			*(char *)(buf + size - j) = 0xff & j;
		memset(buf, 0, size - 1 - num);
	}
}

static void vals_create(int count, int size, struct m0_bufvec *vals)
{
	int i;
	int rc;

	M0_PRE(vals != NULL);
	rc = m0_bufvec_alloc_aligned(vals, count, size, m0_pageshift_get());
	M0_UT_ASSERT(rc == 0);
	for (i = 0; i < count; i++)
		value_create(size, i, vals->ov_buf[i]);
}

static void vals_mix_create(int count, int large_size,
			   struct m0_bufvec *vals)
{
	int rc;
	int i;

	rc = bufvec_empty_alloc(vals, count);
	M0_UT_ASSERT(rc == 0);
	M0_UT_ASSERT(vals->ov_vec.v_nr == count);
	for (i = 0; i < count; i++) {
		vals->ov_vec.v_count[i] = i % 2 ? large_size :
						  sizeof(uint64_t);
		vals->ov_buf[i] = m0_alloc(vals->ov_vec.v_count[i]);
		M0_UT_ASSERT(vals->ov_buf[i] != NULL);
		value_create(vals->ov_vec.v_count[i], i, vals->ov_buf[i]);
	}
}

static int cas_client_init(struct cl_ctx *cctx, const char *cl_ep_addr,
			   const char *srv_ep_addr, const char* dbname,
			   struct m0_net_xprt *xprt)
{
	int                       rc;
	struct m0_rpc_client_ctx *cl_rpc_ctx;

	M0_PRE(cctx != NULL && cl_ep_addr != NULL && srv_ep_addr != NULL &&
	       dbname != NULL && xprt != NULL);

	rc = m0_net_domain_init(&cctx->cl_ndom, xprt);
	M0_UT_ASSERT(rc == 0);

	m0_semaphore_init(&cctx->cl_wait.aw_cb_wait, 0);
	cctx->cl_wait.aw_done          = false;
	cl_rpc_ctx = &cctx->cl_rpc_ctx;

	cl_rpc_ctx->rcx_net_dom            = &cctx->cl_ndom;
	cl_rpc_ctx->rcx_local_addr         = cl_ep_addr;
	cl_rpc_ctx->rcx_remote_addr        = srv_ep_addr;
	cl_rpc_ctx->rcx_max_rpcs_in_flight = MAX_RPCS_IN_FLIGHT;
	cl_rpc_ctx->rcx_fid                = &g_process_fid;

	m0_fi_enable_once("m0_rpc_machine_init", "bulk_cutoff_4K");
	rc = m0_rpc_client_start(cl_rpc_ctx);
	M0_UT_ASSERT(rc == 0);

	return rc;
}

static void cas_client_fini(struct cl_ctx *cctx)
{
	int rc;

	rc = m0_rpc_client_stop(&cctx->cl_rpc_ctx);
	M0_UT_ASSERT(rc == 0);
	m0_net_domain_fini(&cctx->cl_ndom);
	m0_semaphore_fini(&cctx->cl_wait.aw_cb_wait);
}

static void casc_ut_init(struct m0_rpc_server_ctx *sctx,
			 struct cl_ctx            *cctx)
{
	int rc;
	sctx->rsx_xprts = m0_net_all_xprt_get();
	sctx->rsx_xprts_nr = m0_net_xprt_nr();
	M0_SET0(&sctx->rsx_motr_ctx);
	rc = m0_rpc_server_start(sctx);
	M0_UT_ASSERT(rc == 0);
	rc = cas_client_init(cctx, cl_ep_addrs[0],
			     srv_ep_addrs[0], cdbnames[0],
			     m0_net_xprt_default_get());
	M0_UT_ASSERT(rc == 0);
}

static void casc_ut_fini(struct m0_rpc_server_ctx *sctx,
			 struct cl_ctx            *cctx)
{
	cas_client_fini(cctx);
	m0_reqh_idle_wait(&sctx->rsx_motr_ctx.cc_reqh_ctx.rc_reqh);
	m0_rpc_server_stop(sctx);
}

static bool casc_chan_cb(struct m0_clink *clink)
{
	struct async_wait *aw = container_of(clink, struct async_wait,
					     aw_clink);
	struct m0_sm      *sm = container_of(clink->cl_chan, struct m0_sm,
					     sm_chan);

	if (sm->sm_state == CASREQ_FINAL) {
		aw->aw_done = true;
		m0_semaphore_up(&aw->aw_cb_wait);
	}
	return true;
}

static int ut_idx_crdel_wrp(enum idx_operation       op,
			    struct cl_ctx           *cctx,
			    const struct m0_fid     *ids,
			    uint64_t                 ids_nr,
			    m0_chan_cb_t             cb,
			    struct m0_cas_rec_reply *rep,
			    uint32_t                 flags)
{
	struct m0_cas_req       req;
	struct m0_cas_id       *cids;
	struct m0_chan         *chan;
	int                     rc;
	uint64_t                i;

	/* create cas ids by passed fids */
	M0_ALLOC_ARR(cids, ids_nr);
	if (cids == NULL)
		return M0_ERR(-ENOMEM);
	m0_forall(i, ids_nr, cids[i].ci_fid = ids[i], true);

	/* start operation */
	M0_SET0(&req);
	m0_cas_req_init(&req, &cctx->cl_rpc_ctx.rcx_session,
			m0_locality0_get()->lo_grp);
	chan = &req.ccr_sm.sm_chan;
	M0_UT_ASSERT(chan != NULL);
	m0_clink_init(&cctx->cl_wait.aw_clink, cb);
	m0_clink_add_lock(chan, &cctx->cl_wait.aw_clink);

	m0_cas_req_lock(&req);
	if (op == IDX_CREATE)
		rc = m0_cas_index_create(&req, cids, ids_nr, NULL);
	else
		rc = m0_cas_index_delete(&req, cids, ids_nr, NULL, flags);
	/* wait results */
	if (rc == 0) {
		if (cb != NULL) {
			m0_cas_req_unlock(&req);
			m0_semaphore_timeddown(&cctx->cl_wait.aw_cb_wait,
					       m0_time_from_now(5, 0));
			M0_UT_ASSERT(cctx->cl_wait.aw_done);
			cctx->cl_wait.aw_done = false;
			m0_cas_req_lock(&req);
		}
		else
			m0_cas_req_wait(&req, M0_BITS(CASREQ_FINAL),
					M0_TIME_NEVER);
		rc = m0_cas_req_generic_rc(&req);
		if (rc == 0) {
			M0_UT_ASSERT(m0_cas_req_nr(&req) == ids_nr);
			for (i = 0; i < ids_nr; i++)
				if (op == IDX_CREATE)
					m0_cas_index_create_rep(&req, i,
								&rep[i]);
				else
					m0_cas_index_delete_rep(&req, i,
								&rep[i]);
		}
	}
	m0_cas_req_unlock(&req);
	m0_clink_del_lock(&cctx->cl_wait.aw_clink);
	m0_cas_req_fini_lock(&req);
	m0_clink_fini(&cctx->cl_wait.aw_clink);
	m0_free(cids);
	return rc;
}

static int ut_idx_create_async(struct cl_ctx            *cctx,
			       const struct m0_fid      *ids,
			       uint64_t                  ids_nr,
			       m0_chan_cb_t              cb,
			       struct m0_cas_rec_reply *rep)
{
	M0_UT_ASSERT(cb != NULL);
	return ut_idx_crdel_wrp(IDX_CREATE, cctx, ids, ids_nr, cb, rep, 0);
}

static int ut_idx_create(struct cl_ctx            *cctx,
			 const struct m0_fid      *ids,
			 uint64_t                  ids_nr,
			 struct m0_cas_rec_reply *rep)
{
	return ut_idx_crdel_wrp(IDX_CREATE, cctx, ids, ids_nr, NULL, rep, 0);
}

static int ut_lookup_idx(struct cl_ctx           *cctx,
			 const struct m0_fid     *ids,
			 uint64_t                 ids_nr,
			 struct m0_cas_rec_reply *rep)
{
	struct m0_cas_req  req;
	struct m0_cas_id  *cids;
	struct m0_chan    *chan;
	int                rc;
	uint64_t           i;

	/* create cas ids by passed fids */
	M0_ALLOC_ARR(cids, ids_nr);
	if (cids == NULL)
		return M0_ERR(-ENOMEM);
	m0_forall(i, ids_nr, cids[i].ci_fid = ids[i], true);

	/* start operation */
	M0_SET0(&req);
	m0_cas_req_init(&req, &cctx->cl_rpc_ctx.rcx_session,
			m0_locality0_get()->lo_grp);
	chan = &req.ccr_sm.sm_chan;
	M0_UT_ASSERT(chan != NULL);
	m0_clink_init(&cctx->cl_wait.aw_clink, NULL);
	m0_clink_add_lock(chan, &cctx->cl_wait.aw_clink);

	m0_cas_req_lock(&req);
	rc = m0_cas_index_lookup(&req, cids, ids_nr);
	if (rc == 0) {
		/* wait results */
		m0_cas_req_wait(&req, M0_BITS(CASREQ_FINAL), M0_TIME_NEVER);
		rc = m0_cas_req_generic_rc(&req);
		if (rc == 0)
			for (i = 0; i < ids_nr; i++)
				m0_cas_index_lookup_rep(&req, i, &rep[i]);
	}
	m0_cas_req_unlock(&req);
	m0_clink_del_lock(&cctx->cl_wait.aw_clink);
	m0_cas_req_fini_lock(&req);
	m0_clink_fini(&cctx->cl_wait.aw_clink);
	m0_free(cids);
	return rc;
}

static int ut_idx_flagged_delete(struct cl_ctx           *cctx,
				 const struct m0_fid     *ids,
				 uint64_t                 ids_nr,
				 struct m0_cas_rec_reply *rep,
				 uint32_t                 flags)
{
	return ut_idx_crdel_wrp(IDX_DELETE, cctx, ids, ids_nr, NULL,
				rep, flags);
}

static int ut_idx_delete(struct cl_ctx           *cctx,
			 const struct m0_fid     *ids,
			 uint64_t                 ids_nr,
			 struct m0_cas_rec_reply *rep)
{
	return ut_idx_flagged_delete(cctx, ids, ids_nr, rep, 0);
}

static int ut_idx_list(struct cl_ctx             *cctx,
		       const struct m0_fid       *start_fid,
		       uint64_t                   ids_nr,
		       uint64_t                  *rep_count,
		       struct m0_cas_ilist_reply *rep)
{
	struct m0_cas_req  req;
	struct m0_chan    *chan;
	int                rc;
	uint64_t           i;

	/* start operation */
	M0_SET0(&req);
	m0_cas_req_init(&req, &cctx->cl_rpc_ctx.rcx_session,
			m0_locality0_get()->lo_grp);
	chan = &req.ccr_sm.sm_chan;
	M0_UT_ASSERT(chan != NULL);
	m0_clink_init(&cctx->cl_wait.aw_clink, NULL);
	m0_clink_add_lock(chan, &cctx->cl_wait.aw_clink);

	m0_cas_req_lock(&req);
	rc = m0_cas_index_list(&req, start_fid, ids_nr, 0);
	if (rc == 0) {
		/* wait results */
		m0_cas_req_wait(&req, M0_BITS(CASREQ_FINAL), M0_TIME_NEVER);
		rc = m0_cas_req_generic_rc(&req);
		if (rc == 0) {
			*rep_count = m0_cas_req_nr(&req);
			for (i = 0; i < *rep_count; i++)
				m0_cas_index_list_rep(&req, i, &rep[i]);
		}
	}
	m0_cas_req_unlock(&req);
	m0_clink_del_lock(&cctx->cl_wait.aw_clink);
	m0_cas_req_fini_lock(&req);
	m0_clink_fini(&cctx->cl_wait.aw_clink);
	return rc;
}

static void ut_dtx_init(struct m0_dtx **out, uint64_t version)
{
	struct m0_dtm0_dtx *dtx0;
	int                 rc;

	/* No version => no DTX */
	if (version == 0) {
		*out = NULL;
		return;
	}

	M0_ALLOC_PTR(dtx0);
	M0_UT_ASSERT(dtx0 != NULL);

	rc = m0_dtm0_tx_desc_init(&dtx0->dd_txd, 1);
	M0_UT_ASSERT(rc == 0);
	dtx0->dd_txd.dtd_id = (struct m0_dtm0_tid) {
		.dti_ts.dts_phys = version,
		.dti_fid = g_process_fid,
	};

	dtx0->dd_ancient_dtx.tx_dtx = dtx0;

	*out = &dtx0->dd_ancient_dtx;
}

static void ut_dtx_fini(struct m0_dtx *dtx)
{
	if (dtx != NULL)
		m0_free(dtx->tx_dtx);
}

static int ut_rec_common_put(struct cl_ctx           *cctx,
			     struct m0_cas_id        *index,
			     const struct m0_bufvec  *keys,
			     const struct m0_bufvec  *values,
			     struct m0_dtx           *dtx,
			     struct m0_cas_rec_reply *rep,
			     uint32_t                 flags)
{
	struct m0_cas_req  req;
	struct m0_chan    *chan;
	int                rc;
	uint64_t           i;

	M0_PRE(ergo(dtx != NULL,
		    keys->ov_vec.v_nr == 1));

	/* start operation */
	M0_SET0(&req);
	m0_cas_req_init(&req, &cctx->cl_rpc_ctx.rcx_session,
			m0_locality0_get()->lo_grp);
	chan = &req.ccr_sm.sm_chan;
	M0_UT_ASSERT(chan != NULL);
	m0_clink_init(&cctx->cl_wait.aw_clink, NULL);
	m0_clink_add_lock(chan, &cctx->cl_wait.aw_clink);

	m0_cas_req_lock(&req);
	rc = m0_cas_put(&req, index, keys, values, dtx, flags);
	if (rc == 0) {
		/* wait results */
		m0_cas_req_wait(&req, M0_BITS(CASREQ_FINAL), M0_TIME_NEVER);
		rc = m0_cas_req_generic_rc(&req);
		if (rc == 0)
			for (i = 0; i < keys->ov_vec.v_nr; i++)
				m0_cas_put_rep(&req, i, &rep[i]);
	}
	m0_cas_req_unlock(&req);
	m0_clink_del_lock(&cctx->cl_wait.aw_clink);
	m0_cas_req_fini_lock(&req);
	m0_clink_fini(&cctx->cl_wait.aw_clink);
	return rc;
}

/* Submits CAS requests separately one-by-one for each kv pair */
int ut_rec_common_put_seq(struct cl_ctx                 *cctx,
			  struct m0_cas_id              *index,
			  const struct m0_bufvec        *keys,
			  const struct m0_bufvec        *values,
			  struct m0_dtx                 *dtx,
			  struct m0_cas_rec_reply       *rep,
			  uint32_t                       flags)
{
	struct m0_bufvec k;
	struct m0_bufvec v;
	m0_bcount_t      i;
	int              rc = 0;

	for (i = 0; i < keys->ov_vec.v_nr; i++) {
		k = M0_BUFVEC_INIT_BUF(&keys->ov_buf[i],
				       &keys->ov_vec.v_count[i]);
		v = M0_BUFVEC_INIT_BUF(&values->ov_buf[i],
				       &values->ov_vec.v_count[i]);
		rc |= ut_rec_common_put(cctx, index, &k, &v, dtx, rep + i,
					flags);
		if (rc != 0)
			break;
	}

	return rc;
}

static int ut_rec_put(struct cl_ctx           *cctx,
		      struct m0_cas_id        *index,
		      const struct m0_bufvec  *keys,
		      const struct m0_bufvec  *values,
		      struct m0_cas_rec_reply *rep,
		      uint32_t                 flags)
{
	return ((flags & COF_VERSIONED) != 0 ?
		ut_rec_common_put_seq : ut_rec_common_put)
		(cctx, index, keys, values, NULL, rep, flags);
}

static void ut_get_rep_clear(struct m0_cas_get_reply *rep, uint32_t nr)
{
	uint32_t i;

	for (i = 0; i < nr; i++)
		m0_free(rep[i].cge_val.b_addr);
}

static int ut_rec__get(struct cl_ctx           *cctx,
		       struct m0_cas_id        *index,
		       const struct m0_bufvec  *keys,
		       struct m0_cas_get_reply *rep,
		       uint64_t                 flags)
{
	struct m0_cas_req  req;
	struct m0_chan    *chan;
	int                rc;
	uint64_t           i;

	/* start operation */
	M0_SET0(&req);
	m0_cas_req_init(&req, &cctx->cl_rpc_ctx.rcx_session,
			m0_locality0_get()->lo_grp);
	chan = &req.ccr_sm.sm_chan;
	M0_UT_ASSERT(chan != NULL);
	m0_clink_init(&cctx->cl_wait.aw_clink, NULL);
	m0_clink_add_lock(chan, &cctx->cl_wait.aw_clink);

	m0_cas_req_lock(&req);
	rc = ((flags & COF_VERSIONED) != 0 ?
	      m0_cas_versioned_get : m0_cas_get)(&req, index, keys);
	if (rc == 0) {
		/* wait results */
		m0_cas_req_wait(&req, M0_BITS(CASREQ_FINAL), M0_TIME_NEVER);
		rc = m0_cas_req_generic_rc(&req);
		if (rc == 0) {
			M0_UT_ASSERT(m0_cas_req_nr(&req) == keys->ov_vec.v_nr);
			for (i = 0; i < keys->ov_vec.v_nr; i++) {
				m0_cas_get_rep(&req, i, &rep[i]);
				/*
				 * Lock value in memory, because it will be
				 * deallocated after m0_cas_req_fini().
				 */
				if (rep[i].cge_rc == 0)
					m0_cas_rep_mlock(&req, i);
			}
		}
	}
	m0_cas_req_unlock(&req);
	m0_clink_del_lock(&cctx->cl_wait.aw_clink);
	m0_cas_req_fini_lock(&req);
	m0_clink_fini(&cctx->cl_wait.aw_clink);
	return rc;
}

static int ut_rec_get(struct cl_ctx           *cctx,
		      struct m0_cas_id        *index,
		      const struct m0_bufvec  *keys,
		      struct m0_cas_get_reply *rep)
{
	return ut_rec__get(cctx, index, keys, rep, 0);
}

static void ut_next_rep_clear(struct m0_cas_next_reply *rep, uint64_t nr)
{
	uint64_t i;

	for (i = 0; i < nr; i++) {
		m0_free(rep[i].cnp_key.b_addr);
		m0_free(rep[i].cnp_val.b_addr);
		M0_SET0(&rep[i]);
	}
}

static int ut_next_rec(struct cl_ctx            *cctx,
		       struct m0_cas_id         *index,
		       struct m0_bufvec         *start_keys,
		       uint32_t                 *recs_nr,
		       struct m0_cas_next_reply *rep,
		       uint64_t                 *count,
		       uint32_t                  flags)
{
	struct m0_cas_req  req;
	struct m0_chan    *chan;
	int                rc;
	uint64_t           i;

	/* start operation */
	M0_SET0(&req);
	m0_cas_req_init(&req, &cctx->cl_rpc_ctx.rcx_session,
			m0_locality0_get()->lo_grp);
	chan = &req.ccr_sm.sm_chan;
	M0_UT_ASSERT(chan != NULL);
	m0_clink_init(&cctx->cl_wait.aw_clink, NULL);
	m0_clink_add_lock(chan, &cctx->cl_wait.aw_clink);

	m0_cas_req_lock(&req);
	rc = m0_cas_next(&req, index, start_keys, recs_nr, flags);
	if (rc == 0) {
		/* wait results */
		m0_cas_req_wait(&req, M0_BITS(CASREQ_FINAL), M0_TIME_NEVER);
		rc = m0_cas_req_generic_rc(&req);
		if (rc == 0) {
			*count = m0_cas_req_nr(&req);
			for (i = 0; i < *count; i++) {
				m0_cas_next_rep(&req, i, &rep[i]);
				/*
				 * Lock key/value in memory, because they will
				 * be deallocated after m0_cas_req_fini().
				 */
				if (rep[i].cnp_rc == 0)
					m0_cas_rep_mlock(&req, i);
			}
		}
	}
	m0_cas_req_unlock(&req);
	m0_clink_del_lock(&cctx->cl_wait.aw_clink);
	m0_cas_req_fini_lock(&req);
	m0_clink_fini(&cctx->cl_wait.aw_clink);
	return rc;
}

static int ut_rec_common_del(struct cl_ctx           *cctx,
			     struct m0_cas_id        *index,
			     const struct m0_bufvec  *keys,
			     struct m0_dtx           *dtx,
			     struct m0_cas_rec_reply *rep,
			     uint64_t                 flags)
{
	struct m0_cas_req  req;
	struct m0_chan    *chan;
	int                rc;
	uint64_t           i;

	M0_PRE(ergo(dtx != NULL,
		    (((flags & COF_VERSIONED) != 0) &&
		     keys->ov_vec.v_nr == 1)));

	/* start operation */
	M0_SET0(&req);
	m0_cas_req_init(&req, &cctx->cl_rpc_ctx.rcx_session,
			m0_locality0_get()->lo_grp);
	chan = &req.ccr_sm.sm_chan;
	M0_UT_ASSERT(chan != NULL);
	m0_clink_init(&cctx->cl_wait.aw_clink, NULL);
	m0_clink_add_lock(chan, &cctx->cl_wait.aw_clink);

	m0_cas_req_lock(&req);
	rc = m0_cas_del(&req, index, (struct m0_bufvec *) keys, dtx, flags);
	if (rc == 0) {
		/* wait results */
		m0_cas_req_wait(&req, M0_BITS(CASREQ_FINAL), M0_TIME_NEVER);
		rc = m0_cas_req_generic_rc(&req);
		if (rc == 0) {
			M0_UT_ASSERT(m0_cas_req_nr(&req) == keys->ov_vec.v_nr);
			for (i = 0; i < keys->ov_vec.v_nr; i++)
				m0_cas_del_rep(&req, i, rep);
		}
	}
	m0_cas_req_unlock(&req);
	m0_clink_del_lock(&cctx->cl_wait.aw_clink);
	m0_cas_req_fini_lock(&req);
	m0_clink_fini(&cctx->cl_wait.aw_clink);
	return rc;
}

/* Submits CAS requests separately one-by-one for each key. */
static int ut_rec_common_del_seq(struct cl_ctx           *cctx,
				 struct m0_cas_id        *index,
				 const struct m0_bufvec  *keys,
				 struct m0_dtx           *dtx,
				 struct m0_cas_rec_reply *rep,
				 uint64_t                 flags)
{
	struct m0_bufvec k;
	m0_bcount_t      i;
	int              rc = 0;

	for (i = 0; i < keys->ov_vec.v_nr; i++) {
		k = M0_BUFVEC_INIT_BUF(&keys->ov_buf[i],
				       &keys->ov_vec.v_count[i]);
		rc |= ut_rec_common_del(cctx, index, &k, dtx, rep + i, flags);
		if (rc != 0)
			break;
	}

	return rc;
}

static int ut_rec_del(struct cl_ctx           *cctx,
		      struct m0_cas_id        *index,
		      const struct m0_bufvec  *keys,
		      struct m0_cas_rec_reply *rep,
		      uint64_t                 flags)
{

	return ((flags & COF_VERSIONED) != 0 ?
		ut_rec_common_del_seq : ut_rec_common_del)
		(cctx, index, keys, NULL, rep, flags);
}

static void idx_create(void)
{
	struct m0_cas_rec_reply rep       = { 0 };
	const struct m0_fid     ifid      = IFID(2, 3);
	const struct m0_fid     ifid_fake = IFID(2, 4);
	int                     rc;

	casc_ut_init(&casc_ut_sctx, &casc_ut_cctx);

	rc = ut_idx_create(&casc_ut_cctx, &ifid, 1, &rep);
	M0_UT_ASSERT(rc == 0);
	M0_UT_ASSERT(rep.crr_rc == 0);
	rc = ut_lookup_idx(&casc_ut_cctx, &ifid, 1, &rep);
	M0_UT_ASSERT(rc == 0);
	M0_UT_ASSERT(rep.crr_rc == 0);
	rc = ut_lookup_idx(&casc_ut_cctx, &ifid_fake, 1, &rep);
	M0_UT_ASSERT(rc == 0);
	M0_UT_ASSERT(rep.crr_rc == -ENOENT);

	casc_ut_fini(&casc_ut_sctx, &casc_ut_cctx);
}

static void idx_create_fail(void)
{
	struct m0_cas_rec_reply rep  = { 0 };
	const struct m0_fid     ifid = IFID(2, 3);
	int                     rc;

	casc_ut_init(&casc_ut_sctx, &casc_ut_cctx);
	m0_fi_enable_once("creq_op_alloc", "cas_alloc_fail");
	rc = ut_idx_create(&casc_ut_cctx, &ifid, 1, &rep);
	M0_UT_ASSERT(rc == -ENOMEM);
	rc = ut_lookup_idx(&casc_ut_cctx, &ifid, 1, &rep);
	M0_UT_ASSERT(rc == 0);
	M0_UT_ASSERT(rep.crr_rc == -ENOENT);
	m0_fi_enable_once("ctg_kbuf_get", "cas_alloc_fail");
	rc = ut_idx_create(&casc_ut_cctx, &ifid, 1, &rep);
	M0_UT_ASSERT(rc == 0);
	M0_UT_ASSERT(rep.crr_rc == -ENOMEM);
	rc = ut_lookup_idx(&casc_ut_cctx, &ifid, 1, &rep);
	M0_UT_ASSERT(rc == 0);
	M0_UT_ASSERT(rep.crr_rc == -ENOENT);
	m0_fi_enable_once("creq_op_alloc", "cas_alloc_fail");
	rc = ut_lookup_idx(&casc_ut_cctx, &ifid, 1, &rep);
	M0_UT_ASSERT(rc == -ENOMEM);
	m0_fi_enable_once("ctg_kbuf_get", "cas_alloc_fail");
	rc = ut_lookup_idx(&casc_ut_cctx, &ifid, 1, &rep);
	M0_UT_ASSERT(rc == 0);
	M0_UT_ASSERT(rep.crr_rc == -ENOMEM);
	casc_ut_fini(&casc_ut_sctx, &casc_ut_cctx);
}

static void idx_create_a(void)
{
	struct m0_cas_rec_reply rep  = { 0 };
	const struct m0_fid     ifid = IFID(2, 3);
	int                     rc;

	casc_ut_init(&casc_ut_sctx, &casc_ut_cctx);

	rc = ut_idx_create_async(&casc_ut_cctx, &ifid, 1, casc_chan_cb, &rep);
	M0_UT_ASSERT(rc == 0);
	M0_UT_ASSERT(rep.crr_rc == 0);

	casc_ut_fini(&casc_ut_sctx, &casc_ut_cctx);
}

static void idx_create_n(void)
{
	struct m0_cas_rec_reply rep[COUNT];
	struct m0_fid           ifid[COUNT];
	int                     rc;

	casc_ut_init(&casc_ut_sctx, &casc_ut_cctx);

	M0_SET_ARR0(rep);
	m0_forall(i, COUNT, ifid[i] = IFID(2, 3 + i), true);
	/* create several indices */
	rc = ut_idx_create(&casc_ut_cctx, ifid, COUNT, rep);
	M0_UT_ASSERT(rc == 0);
	M0_UT_ASSERT(m0_forall(i, COUNT, rep[i].crr_rc == 0));
	rc = ut_lookup_idx(&casc_ut_cctx, ifid, COUNT, rep);
	M0_UT_ASSERT(rc == 0);
	M0_UT_ASSERT(m0_forall(i, COUNT, rep[i].crr_rc == 0));

	casc_ut_fini(&casc_ut_sctx, &casc_ut_cctx);
}

static void idx_delete(void)
{
	struct m0_cas_rec_reply rep  = { 0 };
	const struct m0_fid     ifid = IFID(2, 3);
	int                     rc;

	casc_ut_init(&casc_ut_sctx, &casc_ut_cctx);

	rc = ut_idx_create(&casc_ut_cctx, &ifid, 1, &rep);
	M0_UT_ASSERT(rc == 0);
	rc = ut_idx_delete(&casc_ut_cctx, &ifid, 1, &rep);
	M0_UT_ASSERT(rc == 0);
	rc = ut_lookup_idx(&casc_ut_cctx, &ifid, 1, &rep);
	M0_UT_ASSERT(rc == 0);
	M0_UT_ASSERT(rep.crr_rc == -ENOENT);

	casc_ut_fini(&casc_ut_sctx, &casc_ut_cctx);
}

static void idx_delete_fail(void)
{
	struct m0_cas_rec_reply rep  = { 0 };
	const struct m0_fid     ifid = IFID(2, 3);
	int                     rc;

	casc_ut_init(&casc_ut_sctx, &casc_ut_cctx);

	rc = ut_idx_create(&casc_ut_cctx, &ifid, 1, &rep);
	M0_UT_ASSERT(rc == 0);
	m0_fi_enable_once("creq_op_alloc", "cas_alloc_fail");
	rc = ut_idx_delete(&casc_ut_cctx, &ifid, 1, &rep);
	M0_UT_ASSERT(rc == -ENOMEM);
	rc = ut_lookup_idx(&casc_ut_cctx, &ifid, 1, &rep);
	M0_UT_ASSERT(rc == 0);
	M0_UT_ASSERT(rep.crr_rc == 0);
	m0_fi_enable_once("ctg_kbuf_get", "cas_alloc_fail");
	rc = ut_idx_delete(&casc_ut_cctx, &ifid, 1, &rep);
	M0_UT_ASSERT(rc == 0);
	M0_UT_ASSERT(rep.crr_rc == -ENOMEM);
	rc = ut_lookup_idx(&casc_ut_cctx, &ifid, 1, &rep);
	M0_UT_ASSERT(rc == 0);
	M0_UT_ASSERT(rep.crr_rc == 0);

	casc_ut_fini(&casc_ut_sctx, &casc_ut_cctx);
}

static void idx_delete_non_exist(void)
{
	struct m0_cas_rec_reply rep  = {};
	const struct m0_fid     ifid = IFID(2, 3);
	int                     rc;

	casc_ut_init(&casc_ut_sctx, &casc_ut_cctx);

	/* Try to remove non-existent index. */
	rc = ut_idx_delete(&casc_ut_cctx, &ifid, 1, &rep);
	M0_UT_ASSERT(rc == 0);
	M0_UT_ASSERT(rep.crr_rc == -ENOENT);

	/* Try to remove non-existent index with CROW flag. */
	rc = ut_idx_flagged_delete(&casc_ut_cctx, &ifid, 1, &rep, COF_CROW);
	M0_UT_ASSERT(rc == 0);
	M0_UT_ASSERT(rep.crr_rc == 0);

	casc_ut_fini(&casc_ut_sctx, &casc_ut_cctx);
}

static void idx_delete_n(void)
{
	struct m0_cas_rec_reply rep[COUNT];
	struct m0_fid           ifid[COUNT];
	int                     rc;

	casc_ut_init(&casc_ut_sctx, &casc_ut_cctx);

	M0_SET_ARR0(rep);
	m0_forall(i, COUNT, ifid[i] = IFID(2, 3 + i), true);
	/* create several indices*/
	rc = ut_idx_create(&casc_ut_cctx, ifid, COUNT, rep);
	M0_UT_ASSERT(rc == 0);
	M0_UT_ASSERT(m0_forall(i, COUNT, rep[i].crr_rc == 0));
	rc = ut_lookup_idx(&casc_ut_cctx, ifid, COUNT, rep);
	M0_UT_ASSERT(rc == 0);
	M0_UT_ASSERT(m0_forall(i, COUNT, rep[i].crr_rc == 0));

	rc = ut_idx_delete(&casc_ut_cctx, ifid, COUNT, rep);
	M0_UT_ASSERT(rc == 0);
	M0_UT_ASSERT(m0_forall(i, COUNT, rep[i].crr_rc == 0));
	rc = ut_lookup_idx(&casc_ut_cctx, ifid, COUNT, rep);
	M0_UT_ASSERT(rc == 0);
	M0_UT_ASSERT(m0_forall(i, COUNT, rep[i].crr_rc == -ENOENT));

	casc_ut_fini(&casc_ut_sctx, &casc_ut_cctx);
}

static void idx_tree_insert(void)
{
	struct m0_cas_get_reply rep[COUNT_TREE];
	struct m0_cas_rec_reply rec_rep[COUNT_TREE];
	struct m0_fid           ifid[COUNT_TREE];
	struct m0_cas_id        index = {};
	struct m0_bufvec        keys;
	struct m0_bufvec        values;
	int                     rc;
	int                     i;

	casc_ut_init(&casc_ut_sctx, &casc_ut_cctx);

	/* initialize data */
	M0_SET_ARR0(rep);
	rc = m0_bufvec_alloc(&keys, COUNT_TREE, sizeof(uint64_t));
	M0_UT_ASSERT(rc == 0);
	rc = m0_bufvec_alloc(&values, COUNT_TREE, sizeof(uint64_t));
	M0_UT_ASSERT(rc == 0);
	M0_UT_ASSERT(keys.ov_vec.v_nr == COUNT_TREE);
	M0_UT_ASSERT(keys.ov_vec.v_nr == values.ov_vec.v_nr);
	m0_forall(i, COUNT_TREE, ifid[i] = IFID(2, 3 + i), true);
	/* create several indices */
	rc = ut_idx_create(&casc_ut_cctx, ifid, COUNT_TREE, rec_rep);
	M0_UT_ASSERT(rc == 0);
	M0_UT_ASSERT(m0_forall(i, COUNT_TREE, rec_rep[i].crr_rc == 0));
	rc = ut_lookup_idx(&casc_ut_cctx, ifid, COUNT_TREE, rec_rep);
	M0_UT_ASSERT(rc == 0);
	M0_UT_ASSERT(m0_forall(i, COUNT_TREE, rec_rep[i].crr_rc == 0));

	/* insert several records into each index */
	m0_forall(i, keys.ov_vec.v_nr, (*(uint64_t*)keys.ov_buf[i]   = i,
					*(uint64_t*)values.ov_buf[i] = i * i,
					true));
	for (i = 0; i < COUNT_TREE; i++) {
		index.ci_fid = ifid[i];
		rc = ut_rec_put(&casc_ut_cctx, &index, &keys, &values,
				rec_rep, 0);
		M0_UT_ASSERT(rc == 0);
	}
	/* get all data */
	m0_forall(i, COUNT_TREE, *(uint64_t*)values.ov_buf[i] = 0, true);
	for (i = 0; i < COUNT_TREE; i++) {
		index.ci_fid = ifid[i];
		rc = ut_rec_get(&casc_ut_cctx, &index, &keys, rep);
		M0_UT_ASSERT(rc == 0);
		M0_UT_ASSERT(m0_forall(j, COUNT_TREE,
			       *(uint64_t*)rep[j].cge_val.b_addr == j * j));
		ut_get_rep_clear(rep, COUNT_TREE);
	}
	m0_bufvec_free(&keys);
	m0_bufvec_free(&values);
	casc_ut_fini(&casc_ut_sctx, &casc_ut_cctx);
}

static void idx_tree_delete(void)
{
	struct m0_cas_rec_reply rep[COUNT_TREE];
	struct m0_fid           ifid[COUNT_TREE];
	struct m0_cas_id        index = {};
	struct m0_bufvec        keys;
	struct m0_bufvec        values;
	int                     rc;
	int                     i;

	casc_ut_init(&casc_ut_sctx, &casc_ut_cctx);

	/* initialize data */
	M0_SET_ARR0(rep);
	rc = m0_bufvec_alloc(&keys, COUNT_TREE, sizeof(uint64_t));
	M0_UT_ASSERT(rc == 0);
	rc = m0_bufvec_alloc(&values, COUNT_TREE, sizeof(uint64_t));
	M0_UT_ASSERT(rc == 0);
	M0_UT_ASSERT(keys.ov_vec.v_nr == COUNT_TREE);
	M0_UT_ASSERT(keys.ov_vec.v_nr == values.ov_vec.v_nr);
	m0_forall(i, COUNT_TREE, ifid[i] = IFID(2, 3 + i), true);
	/* create several indices */
	rc = ut_idx_create(&casc_ut_cctx, ifid, COUNT_TREE, rep);
	M0_UT_ASSERT(rc == 0);
	M0_UT_ASSERT(m0_forall(i, COUNT_TREE, rep[i].crr_rc == 0));
	rc = ut_lookup_idx(&casc_ut_cctx, ifid, COUNT_TREE, rep);
	M0_UT_ASSERT(rc == 0);
	M0_UT_ASSERT(m0_forall(i, COUNT_TREE, rep[i].crr_rc == 0));

	/* insert several records into each index */
	m0_forall(i, keys.ov_vec.v_nr, (*(uint64_t*)keys.ov_buf[i]   = i,
					*(uint64_t*)values.ov_buf[i] = i * i,
					true));
	for (i = 0; i < COUNT_TREE; i++) {
		index.ci_fid = ifid[i];
		rc = ut_rec_put(&casc_ut_cctx, &index, &keys, &values, rep, 0);
		M0_UT_ASSERT(rc == 0);
	}

	/* delete all trees */
	rc = ut_idx_delete(&casc_ut_cctx, ifid, COUNT_TREE, rep);
	M0_UT_ASSERT(rc == 0);
	M0_UT_ASSERT(m0_forall(i, COUNT_TREE, rep[i].crr_rc == 0));

	rc = ut_lookup_idx(&casc_ut_cctx, ifid, COUNT_TREE, rep);
	M0_UT_ASSERT(rc == 0);
	M0_UT_ASSERT(m0_forall(i, COUNT_TREE, rep[i].crr_rc == -ENOENT));

	m0_bufvec_free(&keys);
	m0_bufvec_free(&values);
	casc_ut_fini(&casc_ut_sctx, &casc_ut_cctx);
}

static void idx_tree_delete_fail(void)
{
	struct m0_cas_rec_reply rep[COUNT_TREE];
	struct m0_cas_get_reply get_rep[COUNT_TREE];
	struct m0_fid           ifid[COUNT_TREE];
	struct m0_cas_id        index = {};
	struct m0_bufvec        keys;
	struct m0_bufvec        values;
	int                     rc;
	int                     i;

	casc_ut_init(&casc_ut_sctx, &casc_ut_cctx);

	/* initialize data */
	M0_SET_ARR0(rep);
	rc = m0_bufvec_alloc(&keys, COUNT_TREE, sizeof(uint64_t));
	M0_UT_ASSERT(rc == 0);
	rc = m0_bufvec_alloc(&values, COUNT_TREE, sizeof(uint64_t));
	M0_UT_ASSERT(rc == 0);
	M0_UT_ASSERT(keys.ov_vec.v_nr == COUNT_TREE);
	M0_UT_ASSERT(keys.ov_vec.v_nr == values.ov_vec.v_nr);
	m0_forall(i, COUNT_TREE, ifid[i] = IFID(2, 3 + i), true);
	/* create several indices */
	rc = ut_idx_create(&casc_ut_cctx, ifid, COUNT_TREE, rep);
	M0_UT_ASSERT(rc == 0);
	M0_UT_ASSERT(m0_forall(i, COUNT_TREE, rep[i].crr_rc == 0));
	rc = ut_lookup_idx(&casc_ut_cctx, ifid, COUNT_TREE, rep);
	M0_UT_ASSERT(rc == 0);
	M0_UT_ASSERT(m0_forall(i, COUNT_TREE, rep[i].crr_rc == 0));

	/* insert several records into each index */
	m0_forall(i, keys.ov_vec.v_nr, (*(uint64_t*)keys.ov_buf[i]   = i,
					*(uint64_t*)values.ov_buf[i] = i * i,
					true));
	for (i = 0; i < COUNT_TREE; i++) {
		index.ci_fid = ifid[i];
		rc = ut_rec_put(&casc_ut_cctx, &index, &keys, &values, rep, 0);
		M0_UT_ASSERT(rc == 0);
	}

	/* delete all trees */
	m0_fi_enable_once("creq_op_alloc", "cas_alloc_fail");
	rc = ut_idx_delete(&casc_ut_cctx, ifid, COUNT_TREE, rep);
	M0_UT_ASSERT(rc == -ENOMEM);

	rc = ut_lookup_idx(&casc_ut_cctx, ifid, COUNT_TREE, rep);
	M0_UT_ASSERT(rc == 0);
	M0_UT_ASSERT(m0_forall(i, COUNT_TREE, rep[i].crr_rc == 0));

	/* get all data */
	m0_forall(i, COUNT_TREE, *(uint64_t*)values.ov_buf[i] = 0, true);
	for (i = 0; i < COUNT_TREE; i++) {
		index.ci_fid = ifid[i];
		rc = ut_rec_get(&casc_ut_cctx, &index, &keys, get_rep);
		M0_UT_ASSERT(rc == 0);
		M0_UT_ASSERT(m0_forall(j, COUNT_TREE,
			       *(uint64_t*)get_rep[j].cge_val.b_addr == j * j));
		ut_get_rep_clear(get_rep, COUNT_TREE);
	}

	m0_bufvec_free(&keys);
	m0_bufvec_free(&values);
	casc_ut_fini(&casc_ut_sctx, &casc_ut_cctx);
}

static void idx_list(void)
{
	struct m0_cas_rec_reply   rep[COUNT];
	struct m0_cas_ilist_reply rep_list[COUNT + COUNT_META_ENTRIES + 1];
	struct m0_fid             ifid[COUNT];
	uint64_t                  rep_count;
	int                       rc;
	int                       i;

	casc_ut_init(&casc_ut_sctx, &casc_ut_cctx);

	M0_SET_ARR0(rep);
	m0_forall(i, COUNT, ifid[i] = IFID(2, 3 + i), true);
	/* Create several indices. */
	rc = ut_idx_create(&casc_ut_cctx, ifid, COUNT, rep);
	M0_UT_ASSERT(rc == 0);
	M0_UT_ASSERT(m0_forall(i, COUNT, rep[i].crr_rc == 0));
	/* Get list of indices from start. */
	rc = ut_idx_list(&casc_ut_cctx, &ifid[0], COUNT, &rep_count, rep_list);
	M0_UT_ASSERT(rc == 0);
	M0_UT_ASSERT(rep_count == COUNT);
	M0_UT_ASSERT(m0_forall(i, COUNT, m0_fid_eq(&rep_list[i].clr_fid,
						   &ifid[i])));
	/* Get list of indices from another position. */
	rc = ut_idx_list(&casc_ut_cctx, &ifid[COUNT / 2], COUNT,
			 &rep_count, rep_list);
	M0_UT_ASSERT(rc == 0);
	M0_UT_ASSERT(rep_count >= COUNT / 2 + 1); /* 1 for -ENOENT record */
	M0_UT_ASSERT(m0_forall(i, COUNT / 2,
				rep_list[i].clr_rc == 0 &&
				m0_fid_eq(&rep_list[i].clr_fid,
					  &ifid[i + COUNT / 2])));
	M0_UT_ASSERT(rep_list[COUNT / 2].clr_rc == -ENOENT);
	/**
	 * Get list of indices from the end. Should contain two records:
	 * the last index and -ENOENT record.
	 */
	rc = ut_idx_list(&casc_ut_cctx, &ifid[COUNT - 1], COUNT,
			 &rep_count, rep_list);
	M0_UT_ASSERT(rc == 0);
	M0_UT_ASSERT(rep_count >= 2);
	M0_UT_ASSERT(m0_fid_eq(&rep_list[0].clr_fid, &ifid[COUNT - 1]));
	M0_UT_ASSERT(rep_list[1].clr_rc == -ENOENT);

	/* Get list of indices from start (provide m0_cas_meta_fid). */
	rc = ut_idx_list(&casc_ut_cctx, &m0_cas_meta_fid,
			 /* meta, catalogue-index, dead-index and -ENOENT */
			 COUNT + COUNT_META_ENTRIES + 1,
			 &rep_count, rep_list);
	M0_UT_ASSERT(rc == 0);
	M0_UT_ASSERT(rep_count == COUNT + COUNT_META_ENTRIES + 1);
	M0_UT_ASSERT(m0_fid_eq(&rep_list[0].clr_fid, &m0_cas_meta_fid));
	M0_UT_ASSERT(m0_fid_eq(&rep_list[1].clr_fid, &m0_cas_ctidx_fid));
	M0_UT_ASSERT(m0_fid_eq(&rep_list[2].clr_fid, &m0_cas_dead_index_fid));
	for (i = COUNT_META_ENTRIES; i < COUNT + COUNT_META_ENTRIES; i++)
		M0_UT_ASSERT(m0_fid_eq(&rep_list[i].clr_fid,
				       &ifid[i-COUNT_META_ENTRIES]));
	M0_UT_ASSERT(rep_list[COUNT + COUNT_META_ENTRIES].clr_rc == -ENOENT);

	/* Delete all indices. */
	rc = ut_idx_delete(&casc_ut_cctx, ifid, COUNT, rep);
	M0_UT_ASSERT(rc == 0);
	M0_UT_ASSERT(m0_forall(j, COUNT, rep[j].crr_rc == 0));
	/* Get list - should be empty. */
	rc = ut_idx_list(&casc_ut_cctx, &ifid[0], COUNT, &rep_count, rep_list);
	M0_UT_ASSERT(rc == 0);
	M0_UT_ASSERT(rep_count >= 1);
	M0_UT_ASSERT(rep_list[0].clr_rc == -ENOENT);

	casc_ut_fini(&casc_ut_sctx, &casc_ut_cctx);
}

static void idx_list_fail(void)
{
	struct m0_cas_rec_reply   rep[COUNT];
	struct m0_cas_ilist_reply rep_list[COUNT];
	struct m0_fid             ifid[COUNT];
	uint64_t                  rep_count;
	int                       rc;

	casc_ut_init(&casc_ut_sctx, &casc_ut_cctx);

	M0_SET_ARR0(rep);
	m0_forall(i, COUNT, ifid[i] = IFID(2, 3 + i), true);
	/* create several indices */
	rc = ut_idx_create(&casc_ut_cctx, ifid, COUNT, rep);
	M0_UT_ASSERT(rc == 0);
	M0_UT_ASSERT(m0_forall(i, COUNT, rep[i].crr_rc == 0));
	/* get list of indices from start */
	rc = ut_idx_list(&casc_ut_cctx, &ifid[0], COUNT, &rep_count, rep_list);
	M0_UT_ASSERT(rc == 0);
	M0_UT_ASSERT(rep_count == COUNT);
	M0_UT_ASSERT(m0_forall(i, COUNT, m0_fid_eq(&rep_list[i].clr_fid,
						   &ifid[i])));
	/* get failed cases for list */
	m0_fi_enable_once("creq_op_alloc", "cas_alloc_fail");
	rc = ut_idx_list(&casc_ut_cctx, &ifid[0], COUNT, &rep_count, rep_list);
	M0_UT_ASSERT(rc == -ENOMEM);
	m0_fi_enable_once("ctg_kbuf_get", "cas_alloc_fail");
	rc = ut_idx_list(&casc_ut_cctx, &ifid[0], COUNT, &rep_count, rep_list);
	M0_UT_ASSERT(rc == 0);
	M0_UT_ASSERT(rep_count == COUNT);
	M0_UT_ASSERT(m0_forall(i, COUNT, i == 0 ?
					 rep_list[i].clr_rc == -ENOMEM :
					 rep_list[i].clr_rc == -EPROTO));

	casc_ut_fini(&casc_ut_sctx, &casc_ut_cctx);
}

static bool next_rep_equals(const struct m0_cas_next_reply *rep,
			    void                           *key,
			    void                           *val)
{
	return memcmp(rep->cnp_key.b_addr, key, rep->cnp_key.b_nob) == 0 &&
	       memcmp(rep->cnp_val.b_addr, val, rep->cnp_val.b_nob) == 0;
}

static void next_common(struct m0_bufvec *keys,
			struct m0_bufvec *values,
			uint32_t          flags)
{
	struct m0_cas_rec_reply   rep[COUNT + 1];
	struct m0_cas_next_reply  next_rep[COUNT + 1];
	const struct m0_fid       ifid = IFID(2, 3);
	struct m0_cas_id          index = {};
	struct m0_bufvec          start_key;
	bool                      slant;
	bool                      exclude_start_key;
	uint32_t                  recs_nr;
	uint64_t                  rep_count;
	int                       rc;

	slant = flags & COF_SLANT;
	exclude_start_key = flags & COF_EXCLUDE_START_KEY;

	M0_SET_ARR0(rep);
	M0_SET_ARR0(next_rep);
	rc = ut_idx_create(&casc_ut_cctx, &ifid, 1, rep);
	M0_UT_ASSERT(rc == 0);
	index.ci_fid = ifid;
	rc = ut_rec_put(&casc_ut_cctx, &index, keys, values, rep, 0);
	M0_UT_ASSERT(rc == 0);
	M0_UT_ASSERT(m0_forall(i, COUNT, rep[i].crr_rc == 0));
	rc = m0_bufvec_alloc(&start_key, 1, keys->ov_vec.v_count[0]);
	M0_UT_ASSERT(rc == 0);

	/* perform next for all records */
	recs_nr = COUNT;
	value_create(start_key.ov_vec.v_count[0], 0, start_key.ov_buf[0]);
	rc = ut_next_rec(&casc_ut_cctx, &index, &start_key, &recs_nr, next_rep,
			 &rep_count, flags);
	M0_UT_ASSERT(rc == 0);
	M0_UT_ASSERT(rep_count == recs_nr);
	M0_UT_ASSERT(m0_forall(i, rep_count, next_rep[i].cnp_rc == 0));
	if (!exclude_start_key || slant)
		M0_UT_ASSERT(m0_forall(i, rep_count,
				       next_rep_equals(&next_rep[i],
						       keys->ov_buf[i],
						       values->ov_buf[i])));
	else
		M0_UT_ASSERT(m0_forall(i, rep_count,
				       next_rep_equals(&next_rep[i],
						       keys->ov_buf[i + 1],
						       values->ov_buf[i + 1])));
	ut_next_rep_clear(next_rep, rep_count);

	/* perform next for small rep */
	recs_nr = COUNT / 2;
	rc = ut_next_rec(&casc_ut_cctx, &index, &start_key, &recs_nr, next_rep,
			 &rep_count, flags);
	M0_UT_ASSERT(rc == 0);
	M0_UT_ASSERT(rep_count == COUNT / 2);
	M0_UT_ASSERT(m0_forall(i, rep_count, next_rep[i].cnp_rc == 0));
	if (!exclude_start_key || slant)
		M0_UT_ASSERT(m0_forall(i, rep_count,
				       next_rep_equals(&next_rep[i],
						       keys->ov_buf[i],
						       values->ov_buf[i])));
	else
		M0_UT_ASSERT(m0_forall(i, rep_count,
				       next_rep_equals(&next_rep[i],
						       keys->ov_buf[i + 1],
						       values->ov_buf[i + 1])));
	ut_next_rep_clear(next_rep, rep_count);

	/* perform next for half records */
	value_create(start_key.ov_vec.v_count[0],
		     !slant ? COUNT / 2 : COUNT / 2 + 1,
		     start_key.ov_buf[0]);
	recs_nr = COUNT;
	rc = ut_next_rec(&casc_ut_cctx, &index, &start_key, &recs_nr, next_rep,
			 &rep_count, flags);
	M0_UT_ASSERT(rc == 0);
	M0_UT_ASSERT(rep_count <= recs_nr);
	M0_UT_ASSERT(m0_forall(i, COUNT / 2, next_rep[i].cnp_rc == 0));
	if (!exclude_start_key)
		M0_UT_ASSERT(
			m0_forall(i, COUNT / 2,
				  next_rep_equals(
					  &next_rep[i],
					  keys->ov_buf[COUNT / 2 + i],
					  values->ov_buf[COUNT / 2 + i])));
	else
		M0_UT_ASSERT(
			m0_forall(i, COUNT / 2,
				  next_rep_equals(
					  &next_rep[i],
					  keys->ov_buf[COUNT / 2 + i + 1],
					  values->ov_buf[COUNT / 2 + i + 1])));
	M0_UT_ASSERT(next_rep[COUNT / 2].cnp_rc == -ENOENT);
	ut_next_rep_clear(next_rep, rep_count);

	/* perform next for empty result set */
	value_create(start_key.ov_vec.v_count[0],
		     !slant ? COUNT : COUNT + 1,
		     start_key.ov_buf[0]);
	recs_nr = COUNT;
	rc = ut_next_rec(&casc_ut_cctx, &index, &start_key, &recs_nr,
			 next_rep, &rep_count, flags);
	M0_UT_ASSERT(rc == 0);
	M0_UT_ASSERT(rep_count >= 1);
	M0_UT_ASSERT(next_rep[0].cnp_rc == -ENOENT);
	ut_next_rep_clear(next_rep, rep_count);

	/* Remove index. */
	rc = ut_idx_delete(&casc_ut_cctx, &ifid, 1, rep);
	M0_UT_ASSERT(rc == 0);

	m0_bufvec_free(&start_key);
}

static int get_reply2bufvec(struct m0_cas_get_reply *get_rep, m0_bcount_t nr,
			    struct m0_bufvec        *out)
{
	int         rc;
	m0_bcount_t i;

	rc = m0_bufvec_empty_alloc(out, nr);
	if (rc != 0)
		return rc;
	for (i = 0; i < nr; ++i) {
		out->ov_buf[i] = get_rep[i].cge_val.b_addr;
		out->ov_vec.v_count[i] = get_rep[i].cge_val.b_nob;
	}

	return rc;
}

/*
 * Returns "true" if GET operation successfully returns all expected_values
 * (if they have been specified) or all the records exist (if expected_values
 * is NULL).
 */
static bool has_values(struct m0_cas_id       *index,
		       const struct m0_bufvec *keys,
		       const struct m0_bufvec *expected_values,
		       uint64_t                flags)
{
	int                      rc;
	struct m0_cas_get_reply *get_rep;
	bool                     result;
	struct m0_bufvec         actual_values;
	struct m0_bufvec         empty_values;

	M0_ALLOC_ARR(get_rep, keys->ov_vec.v_nr);
	M0_UT_ASSERT(get_rep != NULL);

	rc = ut_rec__get(&casc_ut_cctx, index, keys, get_rep, flags);
	M0_UT_ASSERT(rc == 0);

	rc = m0_bufvec_empty_alloc(&empty_values, keys->ov_vec.v_nr);
	M0_UT_ASSERT(rc == 0);

	if (expected_values == NULL)
		expected_values = &empty_values;

	M0_UT_ASSERT(m0_forall(i, keys->ov_vec.v_nr,
			       M0_IN(get_rep[i].cge_rc, (0, -ENOENT))));

	if (m0_exists(i, keys->ov_vec.v_nr, get_rep[i].cge_rc == -ENOENT)) {
		M0_UT_ASSERT(m0_forall(i, keys->ov_vec.v_nr,
				       get_rep[i].cge_rc == -ENOENT));

		rc = get_reply2bufvec(get_rep, keys->ov_vec.v_nr,
				      &actual_values);
		M0_UT_ASSERT(rc == 0);
		M0_UT_ASSERT(bufvec_cmp(&actual_values, expected_values) == 0);
		m0_bufvec_free2(&actual_values);
		result = false;
		goto out;
	} else {
		rc = get_reply2bufvec(get_rep, keys->ov_vec.v_nr,
				      &actual_values);
		M0_UT_ASSERT(rc == 0);
		result = bufvec_cmp(&actual_values, expected_values) == 0;
		m0_bufvec_free2(&actual_values);
	}

out:
	ut_get_rep_clear(get_rep, keys->ov_vec.v_nr);
	m0_free(get_rep);
	m0_bufvec_free(&empty_values);
	return result;
}

/*
 * Returns "true" all values returned by GET operation are the same as
 * the specified version.
 */
static bool has_versions(struct m0_cas_id *index,
			 const struct m0_bufvec *keys,
			 uint64_t version,
			 uint64_t flags)
{
	int                      rc;
	struct m0_cas_get_reply *get_rep;
	bool                     result;

	M0_ALLOC_ARR(get_rep, keys->ov_vec.v_nr);
	M0_UT_ASSERT(get_rep != NULL);

	rc = ut_rec__get(&casc_ut_cctx, index, keys, get_rep, flags);
	M0_UT_ASSERT(rc == 0);

	M0_UT_ASSERT(m0_forall(i, keys->ov_vec.v_nr,
			       M0_IN(get_rep[i].cge_rc, (0, -ENOENT))));

	result = m0_forall(i, keys->ov_vec.v_nr,
			   m0_crv_ts(&get_rep[i].cge_ver).dts_phys == version);

	ut_get_rep_clear(get_rep, keys->ov_vec.v_nr);
	m0_free(get_rep);
	return result;
}

static bool has_tombstones(struct m0_cas_id       *index,
			   const struct m0_bufvec *keys)
{
	int                      rc;
	struct m0_cas_get_reply *get_rep;
	bool                     result;

	M0_ALLOC_ARR(get_rep, keys->ov_vec.v_nr);
	M0_UT_ASSERT(get_rep != NULL);

	rc = ut_rec__get(&casc_ut_cctx, index, keys, get_rep, COF_VERSIONED);
	M0_UT_ASSERT(rc == 0);

	/* Ensure the rcs are within the range of allowed rcs. */
	M0_UT_ASSERT(m0_forall(i, keys->ov_vec.v_nr,
			       M0_IN(get_rep[i].cge_rc, (0, -ENOENT))));

	/* Ensure all-or-nothing (either all have tbs or there are no tbs). */
	M0_UT_ASSERT(m0_forall(i, keys->ov_vec.v_nr,
			     m0_crv_tbs(&get_rep[0].cge_ver)==
			     m0_crv_tbs(&get_rep[i].cge_ver)));

	/* Ensure -ENOENT matches with tombstone flag. */
	M0_UT_ASSERT(m0_forall(i, keys->ov_vec.v_nr,
		       ergo(!m0_crv_is_none(&get_rep[i].cge_ver),
				    m0_crv_tbs(&get_rep[i].cge_ver) ==
				     (get_rep[i].cge_rc == -ENOENT))));

	/* Ensure versions are always present on dead records. */
	M0_UT_ASSERT(m0_forall(i, keys->ov_vec.v_nr,
			       ergo(m0_crv_tbs(&get_rep[i].cge_ver),
				    !m0_crv_is_none(&get_rep[i].cge_ver))));

	result = m0_crv_tbs(&get_rep[0].cge_ver);

	ut_get_rep_clear(get_rep, keys->ov_vec.v_nr);
	memset(get_rep, 0, sizeof(*get_rep) * keys->ov_vec.v_nr);

	/*
	 * Additionally, ensure that all the records are visible
	 * when versioned behavior is disabled.
	 */
	rc = ut_rec__get(&casc_ut_cctx, index, keys, get_rep, 0);
	M0_UT_ASSERT(rc == 0);
	M0_UT_ASSERT(m0_forall(i, keys->ov_vec.v_nr, get_rep[i].cge_rc == 0));
	ut_get_rep_clear(get_rep, keys->ov_vec.v_nr);


	m0_free(get_rep);
	return result;
}

/*
 * Breaks an array of GET replies down into pieces (keys, values, versions).
 * In other words, it transmutes a vector of tuples into a tuple of vectors.
 */
static void next_reply_breakdown(struct m0_cas_next_reply  *next_rep,
				 m0_bcount_t                nr,
				 struct m0_bufvec          *out_key,
				 struct m0_bufvec          *out_val,
				 struct m0_crv            **out_ver)
{
	int            rc;
	m0_bcount_t    i;
	struct m0_crv *ver;

	rc = m0_bufvec_empty_alloc(out_key, nr);
	M0_UT_ASSERT(rc == 0);
	rc = m0_bufvec_empty_alloc(out_val, nr);
	M0_UT_ASSERT(rc == 0);
	M0_ALLOC_ARR(ver, nr);
	M0_UT_ASSERT(ver != NULL);

	for (i = 0; i < nr; ++i) {
		out_key->ov_buf[i] = next_rep[i].cnp_key.b_addr;
		out_key->ov_vec.v_count[i] = next_rep[i].cnp_key.b_nob;

		out_val->ov_buf[i] = next_rep[i].cnp_val.b_addr;
		out_val->ov_vec.v_count[i] = next_rep[i].cnp_val.b_nob;

		ver[i] = next_rep[i].cnp_ver;
	}

	*out_ver = ver;
}


/*
 * Ensures that NEXT yields the expected keys, values and versions.
 * Values and version are optional (ignored when set to NULL).
 */
static void next_records_verified(struct m0_cas_id     *index,
				  struct m0_bufvec     *start_key,
				  uint32_t              requested_keys_nr,
				  struct m0_bufvec     *expected_keys,
				  struct m0_bufvec     *expected_values,
				  struct m0_crv        *expected_versions,
				  int                   flags)
{
	struct m0_cas_next_reply *next_rep;
	uint64_t                  rep_count;
	int                       rc;
	struct m0_bufvec          actual_keys;
	struct m0_bufvec          actual_values;
	struct m0_crv            *actual_versions;

	M0_ALLOC_ARR(next_rep, requested_keys_nr);
	M0_UT_ASSERT(next_rep != NULL);

	rc = ut_next_rec(&casc_ut_cctx, index, start_key, &requested_keys_nr,
			 next_rep, &rep_count, flags);
	M0_UT_ASSERT(rc == 0);
	M0_UT_ASSERT(rep_count == requested_keys_nr);

	if (expected_keys == NULL) {
		M0_UT_ASSERT(expected_values == NULL);
		M0_UT_ASSERT(expected_versions == NULL);
		M0_UT_ASSERT(m0_forall(i, rep_count,
				       next_rep[i].cnp_rc == -ENOENT));
	} else {
		M0_UT_ASSERT(m0_forall(i, rep_count, next_rep[i].cnp_rc == 0));
		M0_UT_ASSERT(rep_count == expected_keys->ov_vec.v_nr);
		next_reply_breakdown(next_rep, rep_count,
				     &actual_keys,
				     &actual_values,
				     &actual_versions);
		M0_UT_ASSERT(bufvec_cmp(expected_keys,
					&actual_keys) == 0);

		if (expected_values != NULL)
			M0_UT_ASSERT(bufvec_cmp(expected_values,
						&actual_values) == 0);
		if (expected_versions != NULL)
			M0_UT_ASSERT(memcmp(expected_versions,
					    actual_versions,
					    rep_count *
					    sizeof(expected_versions[0])) == 0);

		m0_bufvec_free2(&actual_keys);
		m0_bufvec_free2(&actual_values);
		m0_free(actual_versions);
	}

	ut_next_rep_clear(next_rep, rep_count);
	m0_free(next_rep);
}

/*
 * Ensures that NEXT yields the expected keys.
 */
static void next_keys_verified(struct m0_cas_id *index,
			       struct m0_bufvec *start_key,
			       uint32_t          requested_keys_nr,
			       struct m0_bufvec *expected_keys,
			       int               flags)
{
	return next_records_verified(index, start_key, requested_keys_nr,
				     expected_keys, NULL, NULL, flags);
}


static void ut_rec_common_put_verified(struct m0_cas_id       *index,
				       const struct m0_bufvec *keys,
				       const struct m0_bufvec *values,
				       uint64_t                version,
				       uint64_t                flags)
{
	struct m0_cas_rec_reply *rep;
	struct m0_dtx           *dtx;
	int                      rc;

	M0_UT_ASSERT(keys != NULL && values != NULL);
	M0_ALLOC_ARR(rep, keys->ov_vec.v_nr);
	M0_UT_ASSERT(rep != NULL);

	ut_dtx_init(&dtx, version);
	rc = ut_rec_common_put_seq(&casc_ut_cctx, index, keys, values, dtx,
				   rep, flags);
	ut_dtx_fini(dtx);
	M0_UT_ASSERT(rc == 0);
	M0_UT_ASSERT(m0_forall(i, keys->ov_vec.v_nr, rep[i].crr_rc == 0));
	m0_free(rep);
}

static void ut_rec_common_del_verified(struct m0_cas_id       *index,
				       const struct m0_bufvec *keys,
				       uint64_t                version,
				       uint64_t                flags)
{
	struct m0_cas_rec_reply *rep;
	struct m0_dtx           *dtx;
	int                      rc;

	M0_UT_ASSERT(keys != NULL);
	M0_ALLOC_ARR(rep, keys->ov_vec.v_nr);
	M0_UT_ASSERT(rep != NULL);

	ut_dtx_init(&dtx, version);
	rc = ut_rec_common_del_seq(&casc_ut_cctx, index, keys, dtx, rep, flags);
	ut_dtx_fini(dtx);
	M0_UT_ASSERT(rc == 0);
	M0_UT_ASSERT(m0_forall(i, keys->ov_vec.v_nr, rep[i].crr_rc == 0));
	m0_free(rep);
}

/*
 * PUTs (keys, values) and then ensures that expected_values are visible
 * via GET operation.
 */
static void put_get_verified(struct m0_cas_id *index,
			     struct m0_bufvec *keys,
			     struct m0_bufvec *values,
			     struct m0_bufvec *expected_values,
			     uint64_t          version,
			     int               put_flags,
			     int               get_flags)
{
	ut_rec_common_put_verified(index, keys, values, version, put_flags);
	if (expected_values != NULL)
		M0_UT_ASSERT(has_values(index, keys, expected_values,
					get_flags));
}

/*
 * DELetes (keys, values) and then ensures that the values are not visible
 * via GET operation.
 */
static void del_get_verified(struct m0_cas_id *index,
			     struct m0_bufvec *keys,
			     uint64_t          version,
			     uint64_t          del_flags,
			     uint64_t          get_flags)
{
	ut_rec_common_del_verified(index, keys, version, del_flags);
	M0_UT_ASSERT(!has_values(index, keys, NULL, get_flags));

	/*
	 * When version is set to zero, the value actually gets removed, so
	 * that tombstones are not available. Still, we can ensure that
	 * versions were wiped out.
	 */
	if (version == 0)
		M0_UT_ASSERT(has_versions(index, keys, 0, COF_VERSIONED));
	else
		M0_UT_ASSERT(has_tombstones(index, keys));
}


/*
 * Initialise a single-element bufvec using an element taken from
 * the target bufvec at position specified by __idx:
 * @verbatim
 *   bufvec target = [buf A, buf B, buf C]
 *   bufvec slice  = target.slice(1)
 *   assert slice == [buf B]
 *          slice  = target.slice(2)
 *   assert slice == [buf C]
 * @endverbatim
 */
#define M0_BUFVEC_SLICE(__bufvec, __idx)		 \
	M0_BUFVEC_INIT_BUF((__bufvec)->ov_buf + (__idx), \
			   (__bufvec)->ov_vec.v_count + (__idx))

/*
 * A test case where we are verifying that NEXT works well with
 * combinations of PUT and DEL.
 */
static void next_ver(void)
{
	enum { V_PAST, V_FUTURE, V_NR };
	struct m0_bufvec        keys;
	struct m0_bufvec        values;
	struct m0_bufvec        kodd;
	struct m0_bufvec        keven;
	int                     rc;
	uint64_t                version[V_NR] = { 2, 3 };
	int                     i;
	struct m0_cas_id        index = {};
	struct m0_cas_rec_reply rep;
	const struct m0_fid     ifid = IFID(2, 3);

	casc_ut_init(&casc_ut_sctx, &casc_ut_cctx);
	rc = ut_idx_create(&casc_ut_cctx, &ifid, 1, &rep);
	M0_UT_ASSERT(rc == 0);
	index.ci_fid = ifid;

	rc = m0_bufvec_alloc(&keys, COUNT, sizeof(uint64_t));
	M0_UT_ASSERT(rc == 0);
	M0_UT_ASSERT(keys.ov_vec.v_nr == COUNT);
	rc = m0_bufvec_alloc(&values, keys.ov_vec.v_nr, sizeof(uint64_t));
	M0_UT_ASSERT(rc == 0);
	M0_UT_ASSERT(keys.ov_vec.v_nr == values.ov_vec.v_nr);
	rc = m0_bufvec_alloc(&kodd, keys.ov_vec.v_nr / 2, sizeof(uint64_t));
	M0_UT_ASSERT(rc == 0);
	rc = m0_bufvec_alloc(&keven, keys.ov_vec.v_nr / 2, sizeof(uint64_t));
	M0_UT_ASSERT(rc == 0);

	for (i = 0; i < keys.ov_vec.v_nr; i++) {
		*(uint64_t*)keys.ov_buf[i] = i;
		*(uint64_t*)values.ov_buf[i] = i;
		memcpy(((i & 0x01) == 0 ? &keven : &kodd)->ov_buf[i / 2],
		       keys.ov_buf[i], keys.ov_vec.v_count[i]);
	}

	/* Insert all the keys. */
	put_get_verified(&index, &keys, &values, &values, version[V_PAST],
			 COF_VERSIONED | COF_OVERWRITE,
			 COF_VERSIONED);
	M0_UT_ASSERT(has_versions(&index, &keys,
				  version[V_PAST], COF_VERSIONED));

	/* Only even records will be alive in the future. */
	del_get_verified(&index, &kodd, version[V_FUTURE],
			 COF_VERSIONED, COF_VERSIONED);
	M0_UT_ASSERT(has_versions(&index, &kodd,
				  version[V_FUTURE], COF_VERSIONED));
	M0_UT_ASSERT(has_versions(&index, &keven,
				  version[V_PAST], COF_VERSIONED));

	/*
	 * Case:
	 * NEXT with the first alive key and the number records equal to
	 * (number_of_alive_keys - 1) should return all the alive keys.
	 */
	next_keys_verified(&index, &M0_BUFVEC_SLICE(&keys, 0),
			   keys.ov_vec.v_nr / 2, &keven, COF_VERSIONED);

	/*
	 * Case:
	 * Requesting one record with NEXT with the first dead key
	 * should return nothing (ENOENT).
	 */
	next_keys_verified(&index, &M0_BUFVEC_SLICE(&kodd, 0), 1, NULL,
			   COF_VERSIONED);

	/*
	 * Case:
	 * Requesting one record with NEXT(SLANT) with the first dead key
	 * should return the second alive key.
	 */
	next_keys_verified(&index, &M0_BUFVEC_SLICE(&kodd, 0), 1,
			   &M0_BUFVEC_SLICE(&keven, 1),
			   COF_SLANT | COF_VERSIONED);

	/*
	 * Case:
	 * Requesting NEXT(SLANT) record with with last dead key should
	 * return -ENOENT.
	 */
	next_keys_verified(&index,
			   &M0_BUFVEC_SLICE(&kodd, kodd.ov_vec.v_nr - 1), 1,
			   NULL, COF_SLANT | COF_VERSIONED);

	/*
	 * Case:
	 * Requesting one record with NEXT(SLANT|EXECLUDE_START_KEY) with
	 * start key equal to the first alive record should return one single
	 * pair with the next alive record.
	 */
	next_keys_verified(&index, &M0_BUFVEC_SLICE(&keven, 0), 1,
			   &M0_BUFVEC_SLICE(&keven, 1),
			   COF_SLANT | COF_EXCLUDE_START_KEY | COF_VERSIONED);

	/* Now the index should have no keys. */
	del_get_verified(&index, &keven, version[V_FUTURE],
			 COF_VERSIONED, COF_VERSIONED);

	/*
	 * Case:
	 * Requesting one record with NEXT(SLANT) should yield no keys at all
	 * (ENOENT) on an index that does not have alive records.
	 */
	next_keys_verified(&index, &M0_BUFVEC_SLICE(&keys, 0), 1, NULL,
			   COF_VERSIONED);

	/*
	 * Case:
	 * When version-awre behavior is disabled (no COF_VERSIONED),
	 * NEXT should yield all the keys that were inserted initially even
	 * if all of them have tombstones.
	 */
	next_keys_verified(&index, &M0_BUFVEC_SLICE(&keys, 0),
			   keys.ov_vec.v_nr, &keys, 0);

	/* Now insert a non-versioned record at the "end". */
	del_get_verified(&index, &M0_BUFVEC_SLICE(&keys, keys.ov_vec.v_nr - 1),
			 0, 0, 0);
	put_get_verified(&index, &M0_BUFVEC_SLICE(&keys, keys.ov_vec.v_nr - 1),
			 &M0_BUFVEC_SLICE(&values, values.ov_vec.v_nr - 1),
			 &M0_BUFVEC_SLICE(&values, values.ov_vec.v_nr - 1),
			 0, 0, 0);
	/*
	 * Case:
	 * A record without version should be treaten as an alive record.
	 * Requesting one record with NEXT(SLANT) with start key equal to the
	 * last dead record should yield the non-versioned record.
	 * However, if SLANT was not set it should still return ENOENT.
	 */
	next_keys_verified(&index,
			   &M0_BUFVEC_SLICE(&keys, keys.ov_vec.v_nr - 2), 1,
			   &M0_BUFVEC_SLICE(&keys, keys.ov_vec.v_nr - 1),
			   COF_SLANT | COF_VERSIONED);
	next_keys_verified(&index,
			   &M0_BUFVEC_SLICE(&keys, keys.ov_vec.v_nr - 2), 1,
			   NULL , COF_VERSIONED);

	m0_bufvec_free(&keys);
	m0_bufvec_free(&values);
	m0_bufvec_free(&keven);
	m0_bufvec_free(&kodd);

	rc = ut_idx_delete(&casc_ut_cctx, &ifid, 1, &rep);
	M0_UT_ASSERT(rc == 0);
	casc_ut_fini(&casc_ut_sctx, &casc_ut_cctx);
}

/*
 * A test case where we are verifying that version and tombstones exposed
 * by CAS API (see COF_VERSIONED and COF_SHOW_DEAD) show expected
 * behavior with combinations of PUT, DEL and NEXT.
 */
static void next_ver_exposed(void)
{
	enum { V_PAST, V_FUTURE, V_NR };
	struct m0_bufvec        keys;
	struct m0_bufvec        values;
	struct m0_bufvec        expected_values;
	struct m0_bufvec        kodd;
	struct m0_bufvec        keven;
	int                     rc;
	uint64_t                version[V_NR] = { 2, 3 };
	int                     i;
	struct m0_cas_id        index = {};
	struct m0_cas_rec_reply rep;
	struct m0_crv          *versions;
	bool                    is_even;
	const struct m0_fid     ifid = IFID(2, 3);

	casc_ut_init(&casc_ut_sctx, &casc_ut_cctx);
	rc = ut_idx_create(&casc_ut_cctx, &ifid, 1, &rep);
	M0_UT_ASSERT(rc == 0);
	index.ci_fid = ifid;


	rc = m0_bufvec_alloc(&keys, COUNT, sizeof(uint64_t));
	M0_UT_ASSERT(rc == 0);
	M0_UT_ASSERT(keys.ov_vec.v_nr == COUNT);
	M0_ALLOC_ARR(versions, keys.ov_vec.v_nr);
	M0_UT_ASSERT(versions != NULL);
	rc = m0_bufvec_alloc(&values, keys.ov_vec.v_nr, sizeof(uint64_t));
	M0_UT_ASSERT(rc == 0);
	rc = m0_bufvec_alloc(&expected_values,
			     keys.ov_vec.v_nr, sizeof(uint64_t));
	M0_UT_ASSERT(rc == 0);
	M0_UT_ASSERT(keys.ov_vec.v_nr == values.ov_vec.v_nr);
	rc = m0_bufvec_alloc(&kodd, keys.ov_vec.v_nr / 2, sizeof(uint64_t));
	M0_UT_ASSERT(rc == 0);
	rc = m0_bufvec_alloc(&keven, keys.ov_vec.v_nr / 2, sizeof(uint64_t));
	M0_UT_ASSERT(rc == 0);

	for (i = 0; i < keys.ov_vec.v_nr; i++) {
		is_even = (i & 0x01) == 0 ;

		*(uint64_t*)keys.ov_buf[i] = i;
		*(uint64_t*)values.ov_buf[i] = i;
		if (is_even)
			*(uint64_t*)expected_values.ov_buf[i] = i;
		else
			expected_values.ov_vec.v_count[i] = 0;

		memcpy((is_even ? &keven : &kodd)->ov_buf[i / 2],
		       keys.ov_buf[i], keys.ov_vec.v_count[i]);

		m0_crv_init(&versions[i],
			    &(struct m0_dtm0_ts) {
				.dts_phys =
				version[!is_even ? V_FUTURE : V_PAST],
			    },
			    !is_even);
	}

	/* Insert all the keys. */
	put_get_verified(&index, &keys, &values, &values, version[V_PAST],
			 COF_VERSIONED | COF_OVERWRITE,
			 COF_VERSIONED);

	/* Only even records will be alive in the future. */
	del_get_verified(&index, &kodd, version[V_FUTURE],
			 COF_VERSIONED, COF_VERSIONED);

	/*
	 * Case:
	 * If even records are alive and odd records are dead then
	 * NEXT must return all the requested keys if COF_SHOW_DEAD is set.
	 * The dead records must me "younger" then the alive records,
	 * and the dead record must have tombstones set.
	 */
	next_records_verified(&index,
			      &M0_BUFVEC_SLICE(&keys, 0), keys.ov_vec.v_nr,
			      &keys, &expected_values, versions,
			      COF_VERSIONED | COF_SHOW_DEAD);

	/*
	 * Case:
	 * When COF_SHOW_DEAD is specified, dead record is not skipped.
	 */
	next_records_verified(&index,
			      &M0_BUFVEC_SLICE(&keys, 1), 1,
			      &M0_BUFVEC_SLICE(&keys, 1),
			      &expected_values, versions + 1,
			      COF_VERSIONED | COF_SHOW_DEAD);

	/*
	 * Case:
	 * Dead record is not skipped when (SHOW_DEAD | SLANT) is specified.
	 */
	next_records_verified(&index,
			      &M0_BUFVEC_SLICE(&keys, 1), 1,
			      &M0_BUFVEC_SLICE(&keys, 1),
			      &expected_values, versions + 1,
			      COF_VERSIONED | COF_SHOW_DEAD | COF_SLANT);

	m0_bufvec_free(&keys);
	m0_bufvec_free(&values);
	m0_bufvec_free(&keven);
	m0_bufvec_free(&kodd);
	m0_free(versions);

	rc = ut_idx_delete(&casc_ut_cctx, &ifid, 1, &rep);
	M0_UT_ASSERT(rc == 0);
	casc_ut_fini(&casc_ut_sctx, &casc_ut_cctx);
}
#undef M0_BUFVEC_SLICE

static void next(void)
{
	struct m0_bufvec keys;
	struct m0_bufvec values;
	int              rc;

	casc_ut_init(&casc_ut_sctx, &casc_ut_cctx);

	/* Usual case. */
	rc = m0_bufvec_alloc(&keys, COUNT, sizeof(uint64_t));
	M0_UT_ASSERT(rc == 0);
	rc = m0_bufvec_alloc(&values, COUNT, sizeof(uint64_t));
	M0_UT_ASSERT(rc == 0);
	M0_UT_ASSERT(keys.ov_vec.v_nr == COUNT);
	M0_UT_ASSERT(keys.ov_vec.v_nr == values.ov_vec.v_nr);
	m0_forall(i, keys.ov_vec.v_nr, (*(uint64_t*)keys.ov_buf[i]   = i,
					*(uint64_t*)values.ov_buf[i] = i * i,
					true));
	/* Call next_common() with 'slant' disabled. */
	next_common(&keys, &values, 0);
	m0_bufvec_free(&keys);
	m0_bufvec_free(&values);

	/* 'Slant' case. */
	rc = m0_bufvec_alloc(&keys, COUNT, sizeof(uint64_t));
	M0_UT_ASSERT(rc == 0);
	rc = m0_bufvec_alloc(&values, COUNT, sizeof(uint64_t));
	M0_UT_ASSERT(rc == 0);
	M0_UT_ASSERT(keys.ov_vec.v_nr == COUNT);
	M0_UT_ASSERT(keys.ov_vec.v_nr == values.ov_vec.v_nr);
	/*
	 * It is required by next_common() function to fill
	 * keys/values using shift 1.
	 */
	m0_forall(i, keys.ov_vec.v_nr,
		  (*(uint64_t*)keys.ov_buf[i]   = i + 1,
		   *(uint64_t*)values.ov_buf[i] = (i + 1) * (i + 1),
		   true));
	/* Call next_common() with 'slant' enabled. */
	next_common(&keys, &values, COF_SLANT);
	m0_bufvec_free(&keys);
	m0_bufvec_free(&values);

	/* 'First key exclude' case. */
	rc = m0_bufvec_alloc(&keys, COUNT + 1, sizeof(uint64_t));
	M0_UT_ASSERT(rc == 0);
	rc = m0_bufvec_alloc(&values, COUNT + 1, sizeof(uint64_t));
	M0_UT_ASSERT(rc == 0);
	M0_UT_ASSERT(keys.ov_vec.v_nr == COUNT + 1);
	M0_UT_ASSERT(keys.ov_vec.v_nr == values.ov_vec.v_nr);
	m0_forall(i, keys.ov_vec.v_nr, (*(uint64_t*)keys.ov_buf[i]   = i,
					*(uint64_t*)values.ov_buf[i] = i * i,
					true));
	/* Call next_common() with 'first key exclude' enabled. */
	next_common(&keys, &values, COF_EXCLUDE_START_KEY);
	m0_bufvec_free(&keys);
	m0_bufvec_free(&values);

	/* 'First key exclude' with 'slant' case. */
	rc = m0_bufvec_alloc(&keys, COUNT + 1, sizeof(uint64_t));
	M0_UT_ASSERT(rc == 0);
	rc = m0_bufvec_alloc(&values, COUNT + 1, sizeof(uint64_t));
	M0_UT_ASSERT(rc == 0);
	M0_UT_ASSERT(keys.ov_vec.v_nr == COUNT + 1);
	M0_UT_ASSERT(keys.ov_vec.v_nr == values.ov_vec.v_nr);
	m0_forall(i, keys.ov_vec.v_nr,
		  (*(uint64_t*)keys.ov_buf[i]   = i + 1,
		   *(uint64_t*)values.ov_buf[i] = (i + 1) * (i + 1),
		   true));
	/* Call next_common() with 'first key exclude' and 'slant' enabled. */
	next_common(&keys, &values, COF_SLANT | COF_EXCLUDE_START_KEY);
	m0_bufvec_free(&keys);
	m0_bufvec_free(&values);

	casc_ut_fini(&casc_ut_sctx, &casc_ut_cctx);
}

static void next_bulk(void)
{
	struct m0_bufvec keys;
	struct m0_bufvec values;
	int              rc;

	casc_ut_init(&casc_ut_sctx, &casc_ut_cctx);

	/* Bulk keys and values. */
	vals_create(COUNT, COUNT_VAL_BYTES, &keys);
	vals_create(COUNT, COUNT_VAL_BYTES, &values);
	next_common(&keys, &values, 0);
	m0_bufvec_free(&keys);
	m0_bufvec_free(&values);

	/* Bulk values. */
	rc = m0_bufvec_alloc(&keys, COUNT, sizeof(uint64_t));
	M0_UT_ASSERT(rc == 0);
	m0_forall(i, keys.ov_vec.v_nr, (*(uint64_t*)keys.ov_buf[i] = i, true));
	vals_create(COUNT, COUNT_VAL_BYTES, &values);
	next_common(&keys, &values, 0);
	m0_bufvec_free(&keys);
	m0_bufvec_free(&values);

	/* Bulk keys. */
	vals_create(COUNT, COUNT_VAL_BYTES, &keys);
	rc = m0_bufvec_alloc(&values, COUNT, sizeof(uint64_t));
	M0_UT_ASSERT(rc == 0);
	m0_forall(i, values.ov_vec.v_nr, (*(uint64_t*)values.ov_buf[i] = i,
					true));
	next_common(&keys, &values, 0);
	m0_bufvec_free(&keys);
	m0_bufvec_free(&values);

	/* Bulk mix. */
	rc = m0_bufvec_alloc(&keys, COUNT, sizeof(uint64_t));
	M0_UT_ASSERT(rc == 0);
	m0_forall(i, keys.ov_vec.v_nr, (*(uint64_t*)keys.ov_buf[i]   = i,
					true));
	vals_mix_create(COUNT, COUNT_VAL_BYTES, &values);
	next_common(&keys, &values, 0);
	m0_bufvec_free(&keys);
	m0_bufvec_free(&values);

	casc_ut_fini(&casc_ut_sctx, &casc_ut_cctx);
}

static void next_fail(void)
{
	struct m0_cas_rec_reply  rep[COUNT];
	struct m0_cas_next_reply next_rep[COUNT];
	const struct m0_fid      ifid = IFID(2, 3);
	struct m0_cas_id         index = {};
	struct m0_bufvec         keys;
	struct m0_bufvec         values;
	struct m0_bufvec         start_key;
	uint32_t                 recs_nr;
	uint64_t                 rep_count;
	int                      rc;

	casc_ut_init(&casc_ut_sctx, &casc_ut_cctx);

	M0_SET_ARR0(rep);
	M0_SET_ARR0(next_rep);
	rc = m0_bufvec_alloc(&keys, COUNT, sizeof(uint64_t));
	M0_UT_ASSERT(rc == 0);
	rc = m0_bufvec_alloc(&values, COUNT, sizeof(uint64_t));
	M0_UT_ASSERT(rc == 0);
	M0_UT_ASSERT(keys.ov_vec.v_nr == COUNT);
	M0_UT_ASSERT(keys.ov_vec.v_nr == values.ov_vec.v_nr);
	/* insert index and records */
	m0_forall(i, keys.ov_vec.v_nr, (*(uint64_t*)keys.ov_buf[i]   = i,
					*(uint64_t*)values.ov_buf[i] = i * i,
					true));
	rc = ut_idx_create(&casc_ut_cctx, &ifid, 1, rep);
	M0_UT_ASSERT(rc == 0);
	index.ci_fid = ifid;
	rc = ut_rec_put(&casc_ut_cctx, &index, &keys, &values, rep, 0);
	M0_UT_ASSERT(rc == 0);
	M0_UT_ASSERT(m0_forall(i, COUNT, rep[i].crr_rc == 0));
	/* clear result set */
	m0_forall(i, keys.ov_vec.v_nr, (*(uint64_t*)keys.ov_buf[i]   = 0,
					*(uint64_t*)values.ov_buf[i] = 0,
					true));
	/* perform next for all records */
	recs_nr = COUNT;
	rc = m0_bufvec_alloc(&start_key, 1, sizeof(uint64_t));
	M0_UT_ASSERT(rc == 0);
	*(uint64_t *)start_key.ov_buf[0] = 0;
	m0_fi_enable_once("creq_op_alloc", "cas_alloc_fail");
	rc = ut_next_rec(&casc_ut_cctx, &index, &start_key, &recs_nr, next_rep,
			 &rep_count, 0);
	M0_UT_ASSERT(rc == -ENOMEM);
	m0_fi_enable_once("ctg_kbuf_get", "cas_alloc_fail");
	rc = ut_next_rec(&casc_ut_cctx, &index, &start_key, &recs_nr,
			 next_rep, &rep_count, 0);
	M0_UT_ASSERT(rc == -ENOMEM);

	m0_bufvec_free(&start_key);
	m0_bufvec_free(&keys);
	m0_bufvec_free(&values);
	casc_ut_fini(&casc_ut_sctx, &casc_ut_cctx);
}

static void next_multi_common(struct m0_bufvec *keys, struct m0_bufvec *values)
{
	struct m0_cas_rec_reply  rep[COUNT];
	struct m0_cas_next_reply next_rep[COUNT];
	const struct m0_fid      ifid = IFID(2, 3);
	struct m0_cas_id         index = {};
	struct m0_bufvec         start_keys;
	uint32_t                 recs_nr[3];
	uint64_t                 rep_count;
	int                      rc;
	int                      i;

	M0_SET_ARR0(rep);
	M0_SET_ARR0(next_rep);
	rc = ut_idx_create(&casc_ut_cctx, &ifid, 1, rep);
	M0_UT_ASSERT(rc == 0);
	index.ci_fid = ifid;
	rc = ut_rec_put(&casc_ut_cctx, &index, keys, values, rep, 0);
	M0_UT_ASSERT(rc == 0);
	M0_UT_ASSERT(m0_forall(i, COUNT, rep[i].crr_rc == 0));
	/*
	 * Perform next for three keys: first, middle and last.
	 */
	rc = m0_bufvec_alloc(&start_keys, 3, keys->ov_vec.v_count[0]);
	M0_UT_ASSERT(rc == 0);
	value_create(start_keys.ov_vec.v_count[0], 0, start_keys.ov_buf[0]);
	value_create(start_keys.ov_vec.v_count[1], COUNT / 2,
		     start_keys.ov_buf[1]);
	value_create(start_keys.ov_vec.v_count[2], COUNT,
		     start_keys.ov_buf[2]);
	recs_nr[0] = COUNT / 2 - 1;
	recs_nr[1] = COUNT / 2 - 1;
	recs_nr[2] = 1;
	rc = ut_next_rec(&casc_ut_cctx, &index, &start_keys, recs_nr, next_rep,
			 &rep_count, 0);
	M0_UT_ASSERT(rc == 0);
	M0_UT_ASSERT(rep_count == COUNT - 1);
	M0_UT_ASSERT(m0_forall(i, COUNT - 2, next_rep[i].cnp_rc == 0));
	M0_UT_ASSERT(next_rep[COUNT - 2].cnp_rc == -ENOENT);
	M0_UT_ASSERT(m0_forall(i, COUNT / 2 - 1,
			       next_rep_equals(&next_rep[i],
					       keys->ov_buf[i],
					       values->ov_buf[i])));
	for (i = COUNT / 2 - 1; i < COUNT - 2; i++) {
		M0_UT_ASSERT(next_rep_equals(&next_rep[i],
					     keys->ov_buf[i + 1],
					     values->ov_buf[i + 1]));
	}
	ut_next_rep_clear(next_rep, rep_count);

	/* Remove index. */
	rc = ut_idx_delete(&casc_ut_cctx, &ifid, 1, rep);
	M0_UT_ASSERT(rc == 0);

	m0_bufvec_free(&start_keys);
}

static void next_multi(void)
{
	struct m0_bufvec keys;
	struct m0_bufvec values;
	int              rc;

	casc_ut_init(&casc_ut_sctx, &casc_ut_cctx);
	rc = m0_bufvec_alloc(&keys, COUNT, sizeof(uint64_t));
	M0_UT_ASSERT(rc == 0);
	rc = m0_bufvec_alloc(&values, COUNT, sizeof(uint64_t));
	M0_UT_ASSERT(rc == 0);
	M0_UT_ASSERT(keys.ov_vec.v_nr == COUNT);
	M0_UT_ASSERT(keys.ov_vec.v_nr == values.ov_vec.v_nr);
	m0_forall(i, keys.ov_vec.v_nr, (*(uint64_t*)keys.ov_buf[i]   = i,
					*(uint64_t*)values.ov_buf[i] = i * i,
					true));
	next_multi_common(&keys, &values);
	m0_bufvec_free(&keys);
	m0_bufvec_free(&values);
	casc_ut_fini(&casc_ut_sctx, &casc_ut_cctx);
}

static void next_multi_bulk(void)
{
	struct m0_bufvec keys;
	struct m0_bufvec values;
	int              rc;

	casc_ut_init(&casc_ut_sctx, &casc_ut_cctx);

	/* Bulk keys and values. */
	vals_create(COUNT, COUNT_VAL_BYTES, &keys);
	vals_create(COUNT, COUNT_VAL_BYTES, &values);
	next_multi_common(&keys, &values);
	m0_bufvec_free(&keys);
	m0_bufvec_free(&values);

	/* Bulk values. */
	rc = m0_bufvec_alloc(&keys, COUNT, sizeof(uint64_t));
	M0_UT_ASSERT(rc == 0);
	m0_forall(i, keys.ov_vec.v_nr, (*(uint64_t*)keys.ov_buf[i] = i, true));
	vals_create(COUNT, COUNT_VAL_BYTES, &values);
	next_multi_common(&keys, &values);
	m0_bufvec_free(&keys);
	m0_bufvec_free(&values);

	/* Bulk keys. */
	vals_create(COUNT, COUNT_VAL_BYTES, &keys);
	rc = m0_bufvec_alloc(&values, COUNT, sizeof(uint64_t));
	M0_UT_ASSERT(rc == 0);
	m0_forall(i, values.ov_vec.v_nr, (*(uint64_t*)values.ov_buf[i] = i,
					true));
	next_multi_common(&keys, &values);
	m0_bufvec_free(&keys);
	m0_bufvec_free(&values);

	/* Bulk mix. */
	rc = m0_bufvec_alloc(&keys, COUNT, sizeof(uint64_t));
	M0_UT_ASSERT(rc == 0);
	m0_forall(i, keys.ov_vec.v_nr, (*(uint64_t*)keys.ov_buf[i]   = i,
					true));
	vals_mix_create(COUNT, COUNT_VAL_BYTES, &values);
	next_multi_common(&keys, &values);
	m0_bufvec_free(&keys);
	m0_bufvec_free(&values);

	casc_ut_fini(&casc_ut_sctx, &casc_ut_cctx);
}

static void put_common_with_ver(struct m0_bufvec *keys,
				struct m0_bufvec *values,
				uint64_t          version)
{
	const struct m0_fid      ifid = IFID(2, 3);
	struct m0_cas_id         index = {};
	int                      rc;
	struct m0_cas_rec_reply  rep;

	M0_UT_ASSERT(keys != NULL && values != NULL);

	rc = ut_idx_create(&casc_ut_cctx, &ifid, 1, &rep);
	M0_UT_ASSERT(rc == 0);
	index.ci_fid = ifid;

	ut_rec_common_put_verified(&index, keys, values, version,
				   version == 0 ? 0 : (COF_VERSIONED |
						       COF_OVERWRITE));

	/* Remove index. */
	rc = ut_idx_delete(&casc_ut_cctx, &ifid, 1, &rep);
	M0_UT_ASSERT(rc == 0);
}

static void put_common(struct m0_bufvec *keys, struct m0_bufvec *values)
{
	put_common_with_ver(keys, values, 0);
}

/*
 * PUTs keys and ensures that "older" keys are overwritten by "newer" keys,
 * while "older" keys cannot overwrite "newer" keys.
 * The test case is supposed to work in both kinds of environment (DTM
 * and non-DTM).
 */
static void put_overwrite_ver(void)
{
	enum v { V_NONE, V_PAST, V_NOW, V_FUTURE, V_NR};
	struct m0_bufvec         keys;
	struct m0_bufvec         vals[V_NR];
	uint64_t                 version[V_NR] = {};
	int                      rc;
	int                      i;
	int                      j;
	struct m0_cas_id         index = {};
	struct m0_cas_rec_reply  rep[1];
	const struct m0_fid      ifid = IFID(2, 3);

	/*
	 * For every i-th key we have V_NR values from i to i << (V_NR - 1).
	 * Ensure it will not overflow.
	 */
	M0_CASSERT((UINT64_MAX / COUNT) > (1L << V_NR));

	casc_ut_init(&casc_ut_sctx, &casc_ut_cctx);

	rc = m0_bufvec_alloc(&keys, COUNT, sizeof(uint64_t));
	M0_UT_ASSERT(rc == 0);
	M0_UT_ASSERT(keys.ov_vec.v_nr != 0);
	for (j = 0; j < V_NR; j++) {
		rc = m0_bufvec_alloc(&vals[j], keys.ov_vec.v_nr,
				     sizeof(uint64_t));
		M0_UT_ASSERT(rc == 0);
		version[j] = j;
	}

	for (i = 0; i < keys.ov_vec.v_nr; i++) {
		*(uint64_t*)keys.ov_buf[i] = i;
		for (j = 0; j < V_NR; j++)
			*(uint64_t*)vals[j].ov_buf[i] = i << j;
	}

	rc = ut_idx_create(&casc_ut_cctx, &ifid, 1, rep);
	M0_UT_ASSERT(rc == 0);
	index.ci_fid = ifid;

	/* PUT at "now". */
	put_get_verified(&index, &keys, &vals[V_NOW], &vals[V_NOW],
			 version[V_NOW], COF_VERSIONED | COF_OVERWRITE,
			 COF_VERSIONED);
	M0_UT_ASSERT(has_versions(&index, &keys,
				  version[V_NOW], COF_VERSIONED));

	/* PUT at "future" (overwrites "now"). */
	put_get_verified(&index, &keys, &vals[V_FUTURE], &vals[V_FUTURE],
			 version[V_FUTURE], COF_VERSIONED | COF_OVERWRITE,
			 COF_VERSIONED);
	M0_UT_ASSERT(has_versions(&index, &keys,
				  version[V_FUTURE], COF_VERSIONED));

	/* PUT at "past" (values should not be changed) */
	put_get_verified(&index, &keys, &vals[V_PAST], &vals[V_FUTURE],
			 version[V_PAST], COF_VERSIONED | COF_OVERWRITE,
			 COF_VERSIONED);
	M0_UT_ASSERT(has_versions(&index, &keys,
				  version[V_FUTURE], COF_VERSIONED));

	/*
	 * However, empty version with COF_VERSIONED uses current timestamp.
	 * Check that version[V_NONE] got overwritten.
	 */
	put_get_verified(&index, &keys, &vals[V_PAST], &vals[V_PAST],
			 version[V_NONE], COF_VERSIONED | COF_OVERWRITE,
			 COF_VERSIONED);
	M0_UT_ASSERT(!has_versions(&index, &keys,
				  version[V_NONE], COF_VERSIONED));

	rc = ut_idx_delete(&casc_ut_cctx, &ifid, 1, rep);
	M0_UT_ASSERT(rc == 0);

	rc = ut_idx_create(&casc_ut_cctx, &ifid, 1, rep);
	M0_UT_ASSERT(rc == 0);
	index.ci_fid = ifid;

	/*
	 * Insertion of a pair without COF_VERSIONED flag set disables
	 * the versioned behavior even if the version was specified.
	 * Let's insert some values first.
	 */
	put_get_verified(&index, &keys, &vals[V_FUTURE], &vals[V_FUTURE],
			 version[V_FUTURE], 0, 0);
	/*
	 * And now let's check if they are overwritten. We expect that
	 * the values will be overwritten because the previous operation
	 * inserted the records but did not set their versions.
	 */
	put_get_verified(&index, &keys, &vals[V_PAST], &vals[V_PAST],
			 version[V_PAST], COF_VERSIONED | COF_OVERWRITE,
			 COF_VERSIONED);

	rc = ut_idx_delete(&casc_ut_cctx, &ifid, 1, rep);
	M0_UT_ASSERT(rc == 0);

	for (j = 0; j < V_NR; j++)
		m0_bufvec_free(&vals[j]);
	m0_bufvec_free(&keys);
	casc_ut_fini(&casc_ut_sctx, &casc_ut_cctx);
}

/*
 * Put a versioned key-value pair.
 */
static void put_ver(void)
{
	struct m0_bufvec keys;
	struct m0_bufvec values;
	int              rc;

	casc_ut_init(&casc_ut_sctx, &casc_ut_cctx);

	rc = m0_bufvec_alloc(&keys, 1, sizeof(uint64_t));
	M0_UT_ASSERT(rc == 0);
	rc = m0_bufvec_alloc(&values, 1, sizeof(uint64_t));
	M0_UT_ASSERT(rc == 0);
	M0_UT_ASSERT(keys.ov_vec.v_nr != 0);
	M0_UT_ASSERT(keys.ov_vec.v_nr == values.ov_vec.v_nr);
	m0_forall(i, keys.ov_vec.v_nr, (*(uint64_t*)keys.ov_buf[i]   = i,
					*(uint64_t*)values.ov_buf[i] = i * i,
					true));
	put_common_with_ver(&keys, &values, 1);
	m0_bufvec_free(&keys);
	m0_bufvec_free(&values);

	casc_ut_fini(&casc_ut_sctx, &casc_ut_cctx);
}

/*
 * Put small Keys and Values.
 */
static void put(void)
{
	struct m0_bufvec keys;
	struct m0_bufvec values;
	int              rc;

	casc_ut_init(&casc_ut_sctx, &casc_ut_cctx);
	rc = m0_bufvec_alloc(&keys, COUNT, sizeof(uint64_t));
	M0_UT_ASSERT(rc == 0);
	rc = m0_bufvec_alloc(&values, COUNT, sizeof(uint64_t));
	M0_UT_ASSERT(rc == 0);
	M0_UT_ASSERT(keys.ov_vec.v_nr != 0);
	M0_UT_ASSERT(keys.ov_vec.v_nr == values.ov_vec.v_nr);
	m0_forall(i, keys.ov_vec.v_nr, (*(uint64_t*)keys.ov_buf[i]   = i,
					*(uint64_t*)values.ov_buf[i] = i * i,
					true));
	put_common(&keys, &values);
	m0_bufvec_free(&keys);
	m0_bufvec_free(&values);
	casc_ut_fini(&casc_ut_sctx, &casc_ut_cctx);
}

/*
 * Test fragmented requests.
 */
static void recs_fragm(void)
{
	struct m0_cas_rec_reply  rep[COUNT];
	struct m0_cas_get_reply  get_rep[COUNT];
	struct m0_cas_next_reply next_rep[60];
	const struct m0_fid      ifid = IFID(2, 3);
	struct m0_cas_id         index = {};
	struct m0_bufvec         keys;
	struct m0_bufvec         values;
	struct m0_bufvec         start_keys;
	uint64_t                 rep_count;
	uint32_t                 recs_nr[20];
	int                      i;
	int                      j;
	int                      k;
	int                      rc;

	casc_ut_init(&casc_ut_sctx, &casc_ut_cctx);
	rc = m0_bufvec_alloc(&keys, COUNT, sizeof(uint64_t));
	M0_UT_ASSERT(rc == 0);
	rc = m0_bufvec_alloc(&values, COUNT, sizeof(uint64_t));
	M0_UT_ASSERT(rc == 0);
	M0_UT_ASSERT(keys.ov_vec.v_nr != 0);
	M0_UT_ASSERT(keys.ov_vec.v_nr == values.ov_vec.v_nr);
	m0_forall(i, keys.ov_vec.v_nr, (*(uint64_t*)keys.ov_buf[i]   = i,
					*(uint64_t*)values.ov_buf[i] = i * i,
					true));

	M0_SET_ARR0(rep);

	rc = ut_idx_create(&casc_ut_cctx, &ifid, 1, rep);
	M0_UT_ASSERT(rc == 0);
	index.ci_fid = ifid;


	/* Test fragmented PUT. */
	m0_fi_enable_once("m0_rpc_item_max_payload_exceeded",
			  "payload_too_large1");
	m0_fi_enable_off_n_on_m("m0_rpc_item_max_payload_exceeded",
				"payload_too_large2",
				10, 1);
	//m0_fi_enable_once("cas_req_fragmentation", "fragm_error");
	rc = ut_rec_put(&casc_ut_cctx, &index, &keys, &values, rep, 0);
	M0_UT_ASSERT(rc == 0);
	M0_UT_ASSERT(m0_forall(i, COUNT, rep[i].crr_rc == 0));
	m0_fi_disable("m0_rpc_item_max_payload_exceeded", "payload_too_large2");


	/* Test fragmented GET. */
	M0_SET_ARR0(rep);
	M0_SET_ARR0(get_rep);
	m0_fi_enable_once("m0_rpc_item_max_payload_exceeded",
			  "payload_too_large1");
	m0_fi_enable_off_n_on_m("m0_rpc_item_max_payload_exceeded",
				"payload_too_large2",
				10, 1);
	rc = ut_rec_get(&casc_ut_cctx, &index, &keys, get_rep);
	M0_UT_ASSERT(rc == 0);
	M0_UT_ASSERT(m0_forall(i, keys.ov_vec.v_nr,
		     memcmp(get_rep[i].cge_val.b_addr,
			    values.ov_buf[i],
			    values.ov_vec.v_count[i] ) == 0));
	m0_fi_disable("m0_rpc_item_max_payload_exceeded", "payload_too_large2");
	ut_get_rep_clear(get_rep, keys.ov_vec.v_nr);


	/* Test fragmented NEXT. */
	M0_SET_ARR0(rep);
	M0_SET_ARR0(next_rep);
	rc = m0_bufvec_alloc(&start_keys, 20, keys.ov_vec.v_count[0]);
	M0_UT_ASSERT(rc == 0);
	for (i = 0; i < 20; i++) {
		value_create(start_keys.ov_vec.v_count[i], i,
			     start_keys.ov_buf[i]);
		recs_nr[i] = 3;
	}
	m0_fi_enable_once("m0_rpc_item_max_payload_exceeded",
			  "payload_too_large1");
	m0_fi_enable_off_n_on_m("m0_rpc_item_max_payload_exceeded",
				"payload_too_large2",
				10, 1);
	rc = ut_next_rec(&casc_ut_cctx, &index, &start_keys, recs_nr, next_rep,
			 &rep_count, 0);
	M0_UT_ASSERT(rc == 0);
	M0_UT_ASSERT(rep_count == 60);
	M0_UT_ASSERT(m0_forall(i, rep_count, next_rep[i].cnp_rc == 0));
	m0_fi_disable("m0_rpc_item_max_payload_exceeded", "payload_too_large2");

	k = 0;
	for (i = 0; i < 20; i++) {
		for (j = 0; j < 3; j++) {
			M0_UT_ASSERT(next_rep_equals(&next_rep[k++],
						     keys.ov_buf[i + j],
						     values.ov_buf[i + j]));
		}
	}
	ut_next_rep_clear(next_rep, rep_count);


	/* Test fragmented DEL. */
	M0_SET_ARR0(rep);
	M0_SET_ARR0(get_rep);
	m0_fi_enable_once("m0_rpc_item_max_payload_exceeded",
			  "payload_too_large1");
	m0_fi_enable_off_n_on_m("m0_rpc_item_max_payload_exceeded",
				"payload_too_large2",
				10, 1);
	rc = ut_rec_del(&casc_ut_cctx, &index, &keys, rep, 0);
	M0_UT_ASSERT(rc == 0);
	M0_UT_ASSERT(m0_forall(i, COUNT, rep[i].crr_rc == 0));
	m0_fi_disable("m0_rpc_item_max_payload_exceeded", "payload_too_large2");


	/* Check selected values - must be empty. */
	m0_fi_enable_once("m0_rpc_item_max_payload_exceeded",
			  "payload_too_large1");
	m0_fi_enable_off_n_on_m("m0_rpc_item_max_payload_exceeded",
				"payload_too_large2",
				10, 1);
	rc = ut_rec_get(&casc_ut_cctx, &index, &keys, get_rep);
	M0_UT_ASSERT(rc == 0);
	M0_UT_ASSERT(m0_forall(i, COUNT, get_rep[i].cge_rc == -ENOENT));
	m0_fi_disable("m0_rpc_item_max_payload_exceeded", "payload_too_large2");
	ut_get_rep_clear(get_rep, COUNT);


	/* Remove index. */
	rc = ut_idx_delete(&casc_ut_cctx, &ifid, 1, rep);
	M0_UT_ASSERT(rc == 0);

	m0_bufvec_free(&start_keys);
	m0_bufvec_free(&keys);
	m0_bufvec_free(&values);
	casc_ut_fini(&casc_ut_sctx, &casc_ut_cctx);
}

/*
 * Test errors during fragmented requests.
 */
static void recs_fragm_fail(void)
{
	struct m0_cas_rec_reply rep[COUNT];
	const struct m0_fid     ifids[2] = {IFID(2, 3), IFID(2, 4)};
	struct m0_cas_id        index = {};
	struct m0_bufvec        keys;
	struct m0_bufvec        values;
	int                     rc;

	casc_ut_init(&casc_ut_sctx, &casc_ut_cctx);

	rc = m0_bufvec_alloc(&keys, COUNT, sizeof(uint64_t));
	M0_UT_ASSERT(rc == 0);
	rc = m0_bufvec_alloc(&values, COUNT, sizeof(uint64_t));
	M0_UT_ASSERT(rc == 0);
	M0_UT_ASSERT(keys.ov_vec.v_nr != 0);
	M0_UT_ASSERT(keys.ov_vec.v_nr == values.ov_vec.v_nr);
	m0_forall(i, keys.ov_vec.v_nr, (*(uint64_t*)keys.ov_buf[i]   = i,
					*(uint64_t*)values.ov_buf[i] = i * i,
					true));
	M0_SET_ARR0(rep);

	/* Create indices. */
	rc = ut_idx_create(&casc_ut_cctx, ifids, 2, rep);
	M0_UT_ASSERT(rc == 0);

	index.ci_fid = ifids[0];

	m0_fi_enable_once("m0_rpc_item_max_payload_exceeded",
			  "payload_too_large1");
	m0_fi_enable_off_n_on_m("m0_rpc_item_max_payload_exceeded",
				"payload_too_large2",
				10, 1);
	m0_fi_enable_off_n_on_m("cas_req_fragmentation", "fragm_error", 1, 1);
	rc = ut_rec_put(&casc_ut_cctx, &index, &keys, &values, rep, 0);
	m0_fi_disable("m0_rpc_item_max_payload_exceeded", "payload_too_large2");
	m0_fi_disable("cas_req_fragmentation", "fragm_error");
	M0_UT_ASSERT(rc == -E2BIG);

	M0_SET_ARR0(rep);

	index.ci_fid = ifids[1];

	m0_fi_enable_once("m0_rpc_item_max_payload_exceeded",
			  "payload_too_large1");
	m0_fi_enable_off_n_on_m("m0_rpc_item_max_payload_exceeded",
				"payload_too_large2",
				10, 1);
	m0_fi_enable_off_n_on_m("cas_req_replied_cb", "send-failure", 1, 1);
	rc = ut_rec_put(&casc_ut_cctx, &index, &keys, &values, rep, 0);
	m0_fi_disable("m0_rpc_item_max_payload_exceeded", "payload_too_large2");
	m0_fi_disable("cas_req_replied_cb", "send-failure");
	M0_UT_ASSERT(rc == -ENOTCONN);

	/* Remove indices. */
	rc = ut_idx_delete(&casc_ut_cctx, ifids, 2, rep);
	M0_UT_ASSERT(rc == 0);

	m0_bufvec_free(&keys);
	m0_bufvec_free(&values);

	casc_ut_fini(&casc_ut_sctx, &casc_ut_cctx);
}

/*
 * Put Large Keys and Values.
 */
static void put_bulk(void)
{
	struct m0_bufvec keys;
	struct m0_bufvec values;
	int              rc;

	casc_ut_init(&casc_ut_sctx, &casc_ut_cctx);
	/* Bulk keys and values. */
	vals_create(COUNT, COUNT_VAL_BYTES, &keys);
	vals_create(COUNT, COUNT_VAL_BYTES, &values);
	put_common(&keys, &values);
	m0_bufvec_free(&keys);
	m0_bufvec_free(&values);

	/* Bulk keys. */
	rc = m0_bufvec_alloc(&keys, COUNT, sizeof(uint64_t));
	M0_UT_ASSERT(rc == 0);
	m0_forall(i, keys.ov_vec.v_nr, (*(uint64_t*)keys.ov_buf[i] = i, true));
	vals_create(COUNT, COUNT_VAL_BYTES, &values);
	put_common(&keys, &values);
	m0_bufvec_free(&keys);
	m0_bufvec_free(&values);

	/* Bulk values. */
	vals_create(COUNT, COUNT_VAL_BYTES, &keys);
	rc = m0_bufvec_alloc(&values, COUNT, sizeof(uint64_t));
	M0_UT_ASSERT(rc == 0);
	m0_forall(i, values.ov_vec.v_nr, (*(uint64_t*)values.ov_buf[i] = i,
					  true));

	put_common(&keys, &values);
	m0_bufvec_free(&keys);
	m0_bufvec_free(&values);

	/* Bulk mix. */
	vals_mix_create(COUNT, COUNT_VAL_BYTES, &keys);
	rc = m0_bufvec_alloc(&values, COUNT, sizeof(uint64_t));
	M0_UT_ASSERT(rc == 0);
	m0_forall(i, values.ov_vec.v_nr, (*(uint64_t*)values.ov_buf[i] = i,
					  true));
	put_common(&keys, &values);
	m0_bufvec_free(&keys);
	m0_bufvec_free(&values);

	rc = m0_bufvec_alloc(&keys, COUNT, sizeof(uint64_t));
	M0_UT_ASSERT(rc == 0);
	m0_forall(i, keys.ov_vec.v_nr, (*(uint64_t*)keys.ov_buf[i] = i,
					  true));
	vals_mix_create(COUNT, COUNT_VAL_BYTES, &values);
	put_common(&keys, &values);
	m0_bufvec_free(&keys);
	m0_bufvec_free(&values);
	casc_ut_fini(&casc_ut_sctx, &casc_ut_cctx);
}

/*
 * Put small Keys and Values with 'create' or 'overwrite' flag.
 */
static void put_save_common(uint32_t flags)
{
	struct m0_cas_rec_reply rep[1];
	struct m0_cas_get_reply grep[1];
	const struct m0_fid     ifid = IFID(2, 3);
	struct m0_cas_id        index = {};
	struct m0_bufvec        keys;
	struct m0_bufvec        values;
	int                     rc;

	casc_ut_init(&casc_ut_sctx, &casc_ut_cctx);
	rc = m0_bufvec_alloc(&keys, 1, sizeof(uint64_t));
	M0_UT_ASSERT(rc == 0);
	rc = m0_bufvec_alloc(&values, 1, sizeof(uint32_t));
	M0_UT_ASSERT(rc == 0);
	M0_UT_ASSERT(keys.ov_vec.v_nr != 0);
	M0_UT_ASSERT(keys.ov_vec.v_nr == values.ov_vec.v_nr);
	*(uint64_t*)keys.ov_buf[0] = 1;
	*(uint32_t*)values.ov_buf[0] = 1;

	M0_SET_ARR0(rep);

	/* create index */
	rc = ut_idx_create(&casc_ut_cctx, &ifid, 1, rep);
	M0_UT_ASSERT(rc == 0);

	index.ci_fid = ifid;
	rc = ut_rec_put(&casc_ut_cctx, &index, &keys, &values, rep, 0);

	M0_UT_ASSERT(rc == 0);
	M0_UT_ASSERT(rep[0].crr_rc == 0);

	m0_bufvec_free(&values);
	/* Allocate value of size greater than size of previous value. */
	rc = m0_bufvec_alloc(&values, 1, sizeof(uint64_t));
	M0_UT_ASSERT(rc == 0);
	M0_UT_ASSERT(keys.ov_vec.v_nr == values.ov_vec.v_nr);

	*(uint64_t*)values.ov_buf[0] = 2;
	rc = ut_rec_put(&casc_ut_cctx, &index, &keys, &values, rep, flags);
	M0_UT_ASSERT(rc == 0);
	M0_UT_ASSERT(rep[0].crr_rc == 0);

	rc = ut_rec_get(&casc_ut_cctx, &index, &keys, grep);
	M0_UT_ASSERT(rc == 0);
	if (flags & COF_CREATE)
		M0_UT_ASSERT(*(uint32_t*)grep[0].cge_val.b_addr == 1);
	if (flags & COF_OVERWRITE)
		M0_UT_ASSERT(*(uint64_t*)grep[0].cge_val.b_addr == 2);
	ut_get_rep_clear(grep, 1);

	*(uint64_t*)values.ov_buf[0] = 3;
	rc = ut_rec_put(&casc_ut_cctx, &index, &keys, &values, rep, flags);
	M0_UT_ASSERT(rc == 0);
	M0_UT_ASSERT(rep[0].crr_rc == 0);

	rc = ut_rec_get(&casc_ut_cctx, &index, &keys, grep);
	M0_UT_ASSERT(rc == 0);
	if (flags & COF_CREATE)
		M0_UT_ASSERT(*(uint32_t*)grep[0].cge_val.b_addr == 1);
	if (flags & COF_OVERWRITE)
		M0_UT_ASSERT(*(uint64_t*)grep[0].cge_val.b_addr == 3);
	ut_get_rep_clear(grep, 1);

	/* Remove index. */
	rc = ut_idx_delete(&casc_ut_cctx, &ifid, 1, rep);
	M0_UT_ASSERT(rc == 0);

	m0_bufvec_free(&keys);
	m0_bufvec_free(&values);
	casc_ut_fini(&casc_ut_sctx, &casc_ut_cctx);
}

/*
 * Put small Keys and Values with 'create' flag.
 */
static void put_create(void)
{
	put_save_common(COF_CREATE);
}

/*
 * Put small Keys and Values with 'overwrite' flag.
 */
static void put_overwrite(void)
{
	put_save_common(COF_OVERWRITE);
}

/*
 * Put small Keys and Values with 'create-on-write' flag.
 */
static void put_crow(void)
{
	struct m0_cas_rec_reply rep[1];
	struct m0_cas_get_reply grep[1];
	const struct m0_fid     ifid = IFID(2, 3);
	struct m0_cas_id        index = {};
	struct m0_bufvec        keys;
	struct m0_bufvec        values;
	int                     rc;

	casc_ut_init(&casc_ut_sctx, &casc_ut_cctx);
	rc = m0_bufvec_alloc(&keys, 1, sizeof(uint64_t));
	M0_UT_ASSERT(rc == 0);
	rc = m0_bufvec_alloc(&values, 1, sizeof(uint64_t));
	M0_UT_ASSERT(rc == 0);
	M0_UT_ASSERT(keys.ov_vec.v_nr != 0);
	M0_UT_ASSERT(keys.ov_vec.v_nr == values.ov_vec.v_nr);
	*(uint64_t*)keys.ov_buf[0] = 1;
	*(uint64_t*)values.ov_buf[0] = 1;

	M0_SET_ARR0(rep);

	index.ci_fid = ifid;
	rc = ut_rec_put(&casc_ut_cctx, &index, &keys, &values, rep, COF_CROW);

	M0_UT_ASSERT(rc == 0);
	M0_UT_ASSERT(rep[0].crr_rc == 0);

	rc = ut_rec_get(&casc_ut_cctx, &index, &keys, grep);
	M0_UT_ASSERT(rc == 0);
	M0_UT_ASSERT(*(uint64_t*)grep[0].cge_val.b_addr == 1);
	ut_get_rep_clear(grep, 1);

	/* Remove index. */
	rc = ut_idx_delete(&casc_ut_cctx, &ifid, 1, rep);
	M0_UT_ASSERT(rc == 0);

	m0_bufvec_free(&keys);
	m0_bufvec_free(&values);
	casc_ut_fini(&casc_ut_sctx, &casc_ut_cctx);
}

/*
 * Fail to create catalogue during putting of small Keys and Values with
 * 'create-on-write' flag.
 */
static void put_crow_fail(void)
{
	struct m0_cas_rec_reply rep[1];
	struct m0_cas_get_reply grep[1];
	const struct m0_fid     ifid = IFID(2, 3);
	struct m0_cas_id        index = {};
	struct m0_bufvec        keys;
	struct m0_bufvec        values;
	int                     rc;

	casc_ut_init(&casc_ut_sctx, &casc_ut_cctx);
	rc = m0_bufvec_alloc(&keys, 1, sizeof(uint64_t));
	M0_UT_ASSERT(rc == 0);
	rc = m0_bufvec_alloc(&values, 1, sizeof(uint64_t));
	M0_UT_ASSERT(rc == 0);
	M0_UT_ASSERT(keys.ov_vec.v_nr != 0);
	M0_UT_ASSERT(keys.ov_vec.v_nr == values.ov_vec.v_nr);
	*(uint64_t*)keys.ov_buf[0] = 1;
	*(uint64_t*)values.ov_buf[0] = 1;

	M0_SET_ARR0(rep);

	m0_fi_enable_off_n_on_m("ctg_kbuf_get", "cas_alloc_fail", 1, 1);
	index.ci_fid = ifid;
	rc = ut_rec_put(&casc_ut_cctx, &index, &keys, &values, rep, COF_CROW);
	M0_UT_ASSERT(rc == -ENOMEM);
	m0_fi_disable("ctg_kbuf_get", "cas_alloc_fail");

	rc = ut_rec_get(&casc_ut_cctx, &index, &keys, grep);
	M0_UT_ASSERT(rc == -ENOENT);

	/* Try to lookup non-existent index. */
	rc = ut_lookup_idx(&casc_ut_cctx, &ifid, 1, rep);
	M0_UT_ASSERT(rc == 0);
	M0_UT_ASSERT(rep[0].crr_rc == -ENOENT);

	m0_bufvec_free(&keys);
	m0_bufvec_free(&values);
	casc_ut_fini(&casc_ut_sctx, &casc_ut_cctx);
}

static void put_fail_common(struct m0_bufvec *keys, struct m0_bufvec *values)
{
	struct m0_cas_rec_reply rep[COUNT];
	const struct m0_fid     ifid = IFID(2, 3);
	struct m0_cas_id        index = {};
	int                     rc;

	casc_ut_init(&casc_ut_sctx, &casc_ut_cctx);

	M0_SET_ARR0(rep);
	rc = ut_idx_create(&casc_ut_cctx, &ifid, 1, rep);
	M0_UT_ASSERT(rc == 0);
	index.ci_fid = ifid;

	m0_fi_enable_once("creq_op_alloc", "cas_alloc_fail");
	rc = ut_rec_put(&casc_ut_cctx, &index, keys, values, rep, 0);
	M0_UT_ASSERT(rc == -ENOMEM);
	m0_fi_enable_once("ctg_kbuf_get", "cas_alloc_fail");
	rc = ut_rec_put(&casc_ut_cctx, &index, keys, values, rep, 0);
	M0_UT_ASSERT(rc == -ENOMEM);

	casc_ut_fini(&casc_ut_sctx, &casc_ut_cctx);
}

static void put_fail(void)
{
	struct m0_bufvec keys;
	struct m0_bufvec values;
	int              rc;

	rc = m0_bufvec_alloc(&keys, COUNT, sizeof(uint64_t));
	M0_UT_ASSERT(rc == 0);
	rc = m0_bufvec_alloc(&values, COUNT, sizeof(uint64_t));
	M0_UT_ASSERT(rc == 0);
	M0_UT_ASSERT(keys.ov_vec.v_nr != 0);
	M0_UT_ASSERT(keys.ov_vec.v_nr == values.ov_vec.v_nr);
	m0_forall(i, keys.ov_vec.v_nr, (*(uint64_t*)keys.ov_buf[i]   = i,
					*(uint64_t*)values.ov_buf[i] = i * i,
					true));
	put_fail_common(&keys, &values);
	m0_bufvec_free(&keys);
	m0_bufvec_free(&values);
}

static void put_bulk_fail(void)
{
	struct m0_bufvec keys;
	struct m0_bufvec values;

	vals_create(COUNT, COUNT_VAL_BYTES, &keys);
	vals_create(COUNT, COUNT_VAL_BYTES, &values);
	put_fail_common(&keys, &values);
	m0_bufvec_free(&keys);
	m0_bufvec_free(&values);
}

static void upd(void)
{
	struct m0_cas_rec_reply  rep[COUNT];
	struct m0_cas_get_reply  get_rep[COUNT];
	const struct m0_fid      ifid = IFID(2, 3);
	struct m0_cas_id         index = {};
	struct m0_bufvec         keys;
	struct m0_bufvec         values;
	int                      rc;

	casc_ut_init(&casc_ut_sctx, &casc_ut_cctx);

	M0_SET_ARR0(rep);
	M0_SET_ARR0(get_rep);
	rc = m0_bufvec_alloc(&keys, COUNT, sizeof(uint64_t));
	M0_UT_ASSERT(rc == 0);
	rc = m0_bufvec_alloc(&values, COUNT, sizeof(uint64_t));
	M0_UT_ASSERT(rc == 0);
	M0_UT_ASSERT(keys.ov_vec.v_nr != 0);
	M0_UT_ASSERT(keys.ov_vec.v_nr == values.ov_vec.v_nr);
	m0_forall(i, keys.ov_vec.v_nr, (*(uint64_t*)keys.ov_buf[i]   = i,
					*(uint64_t*)values.ov_buf[i] = i * i,
					true));
	rc = ut_idx_create(&casc_ut_cctx, &ifid, 1, rep);
	M0_UT_ASSERT(rc == 0);
	index.ci_fid = ifid;
	/* Insert new records */
	rc = ut_rec_put(&casc_ut_cctx, &index, &keys, &values, rep, 0);
	M0_UT_ASSERT(rc == 0);
	/* update several records */
	m0_forall(i, values.ov_vec.v_nr / 3,
		  *(uint64_t*)values.ov_buf[i] = COUNT * COUNT, true);
	keys.ov_vec.v_nr /= 3;
	values.ov_vec.v_nr = keys.ov_vec.v_nr;
	rc = ut_rec_put(&casc_ut_cctx, &index, &keys, &values, rep, 0);
	values.ov_vec.v_nr = keys.ov_vec.v_nr = COUNT;
	M0_UT_ASSERT(rc == 0);
	M0_UT_ASSERT(m0_forall(i, COUNT, i < values.ov_vec.v_nr / 3 ?
				rep[i].crr_rc == -EEXIST : rep[i].crr_rc == 0));

	m0_forall(i, values.ov_vec.v_nr,
		 (*(uint64_t*)values.ov_buf[i] = 0, true));
	/* check selected values*/
	rc = ut_rec_get(&casc_ut_cctx, &index, &keys, get_rep);
	M0_UT_ASSERT(rc == 0);
	M0_UT_ASSERT(m0_forall(i, keys.ov_vec.v_nr,
			       *(uint64_t*)get_rep[i].cge_val.b_addr == i * i));
	ut_get_rep_clear(get_rep, keys.ov_vec.v_nr);
	m0_bufvec_free(&keys);
	m0_bufvec_free(&values);
	casc_ut_fini(&casc_ut_sctx, &casc_ut_cctx);
}

static void del_common(struct m0_bufvec *keys,
		       struct m0_bufvec *values,
		       uint64_t          version,
		       uint64_t          put_flags,
		       uint64_t          del_flags,
		       uint64_t          get_flags)
{
	struct m0_cas_rec_reply  rep;
	const struct m0_fid      ifid = IFID(2, 3);
	struct m0_cas_id         index = {};
	int                      rc;
	uint64_t                 del_version = version == 0 ? 0 : version + 1;

	rc = ut_idx_create(&casc_ut_cctx, &ifid, 1, &rep);
	M0_UT_ASSERT(rc == 0);
	index.ci_fid = ifid;

	ut_rec_common_put_verified(&index, keys, values, version, put_flags);
	ut_rec_common_del_verified(&index, keys, del_version, del_flags);
	M0_UT_ASSERT(!has_values(&index, keys, NULL, get_flags));

	rc = ut_idx_delete(&casc_ut_cctx, &ifid, 1, &rep);
	M0_UT_ASSERT(rc == 0);
}

static void del_ver(void)
{
	struct m0_bufvec keys;
	struct m0_bufvec values;
	int              rc;

	casc_ut_init(&casc_ut_sctx, &casc_ut_cctx);

	rc = m0_bufvec_alloc(&keys, 1, sizeof(uint64_t));
	M0_UT_ASSERT(rc == 0);
	rc = m0_bufvec_alloc(&values, 1, sizeof(uint64_t));
	M0_UT_ASSERT(rc == 0);
	M0_UT_ASSERT(keys.ov_vec.v_nr != 0);
	M0_UT_ASSERT(keys.ov_vec.v_nr == values.ov_vec.v_nr);
	m0_forall(i, keys.ov_vec.v_nr, (*(uint64_t*)keys.ov_buf[i]   = i,
					*(uint64_t*)values.ov_buf[i] = i * i,
					true));
	del_common(&keys, &values, 1,
		   COF_OVERWRITE | COF_VERSIONED,
		   COF_VERSIONED,
		   COF_VERSIONED);
	m0_bufvec_free(&keys);
	m0_bufvec_free(&values);

	casc_ut_fini(&casc_ut_sctx, &casc_ut_cctx);
}

/*
 * Ensures that versions exposed by GET are visible.
 */
static void get_ver_exposed(void)
{
	struct m0_bufvec        keys;
	struct m0_bufvec        values;
	struct m0_cas_rec_reply rep;
	int                     rc;
	const struct m0_fid     ifid = IFID(2, 3);
	struct m0_cas_id        index = {};
	enum v { V_NONE, V_PAST, V_NOW, V_FUTURE, V_NR};
	uint64_t                version[V_NR] = { 0, 1, 2, 3,};

	casc_ut_init(&casc_ut_sctx, &casc_ut_cctx);

	rc = ut_idx_create(&casc_ut_cctx, &ifid, 1, &rep);
	M0_UT_ASSERT(rc == 0);
	index.ci_fid = ifid;

	rc = m0_bufvec_alloc(&keys, 1, sizeof(uint64_t));
	M0_UT_ASSERT(rc == 0);
	rc = m0_bufvec_alloc(&values, 1, sizeof(uint64_t));
	M0_UT_ASSERT(rc == 0);
	M0_UT_ASSERT(keys.ov_vec.v_nr != 0);
	M0_UT_ASSERT(keys.ov_vec.v_nr == values.ov_vec.v_nr);
	m0_forall(i, keys.ov_vec.v_nr, (*(uint64_t*)keys.ov_buf[i]   = i,
					*(uint64_t*)values.ov_buf[i] = i * i,
					true));

	/* Insert @past */
	put_get_verified(&index, &keys, &values, &values, version[V_PAST],
			 COF_VERSIONED | COF_OVERWRITE,
			 COF_VERSIONED);
	M0_UT_ASSERT(has_versions(&index, &keys,
				  version[V_PAST], COF_VERSIONED));

	/* Delete @future */
	del_get_verified(&index, &keys, version[V_FUTURE],
			 COF_VERSIONED, COF_VERSIONED);
	M0_UT_ASSERT(has_versions(&index, &keys,
				  version[V_FUTURE], COF_VERSIONED));

	/* Cleanup */
	del_get_verified(&index, &keys, version[V_NONE], 0, 0);

	/* Insert @now */
	put_get_verified(&index, &keys, &values, &values, version[V_NOW],
			 COF_VERSIONED | COF_OVERWRITE,
			 COF_VERSIONED);
	M0_UT_ASSERT(has_versions(&index, &keys,
				  version[V_NOW], COF_VERSIONED));

	/* Delete @now */
	del_get_verified(&index, &keys, version[V_NOW],
			 COF_VERSIONED, COF_VERSIONED);
	M0_UT_ASSERT(has_versions(&index, &keys,
				  version[V_NOW], COF_VERSIONED));
	M0_UT_ASSERT(has_tombstones(&index, &keys));

	/* Insert @past */
	put_get_verified(&index, &keys, &values, NULL, version[V_PAST],
			 COF_VERSIONED | COF_OVERWRITE,
			 COF_VERSIONED);
	M0_UT_ASSERT(has_versions(&index, &keys,
				  version[V_NOW], COF_VERSIONED));
	M0_UT_ASSERT(has_tombstones(&index, &keys));

	/* Insert @future */
	put_get_verified(&index, &keys, &values, NULL, version[V_FUTURE],
			 COF_VERSIONED | COF_OVERWRITE,
			 COF_VERSIONED);
	M0_UT_ASSERT(has_versions(&index, &keys,
				  version[V_FUTURE], COF_VERSIONED));
	M0_UT_ASSERT(!has_tombstones(&index, &keys));


	m0_bufvec_free(&keys);
	m0_bufvec_free(&values);

	rc = ut_idx_delete(&casc_ut_cctx, &ifid, 1, &rep);
	M0_UT_ASSERT(rc == 0);
	casc_ut_fini(&casc_ut_sctx, &casc_ut_cctx);
}

static void del(void)
{
	struct m0_bufvec keys;
	struct m0_bufvec values;
	int              rc;

	casc_ut_init(&casc_ut_sctx, &casc_ut_cctx);
	rc = m0_bufvec_alloc(&keys, COUNT, sizeof(uint64_t));
	M0_UT_ASSERT(rc == 0);
	rc = m0_bufvec_alloc(&values, COUNT, sizeof(uint64_t));
	M0_UT_ASSERT(rc == 0);
	M0_UT_ASSERT(keys.ov_vec.v_nr != 0);
	M0_UT_ASSERT(keys.ov_vec.v_nr == values.ov_vec.v_nr);
	m0_forall(i, keys.ov_vec.v_nr, (*(uint64_t*)keys.ov_buf[i]   = i,
					*(uint64_t*)values.ov_buf[i] = i * i,
					true));
	del_common(&keys, &values, 0, 0, 0, 0);
	m0_bufvec_free(&keys);
	m0_bufvec_free(&values);
	casc_ut_fini(&casc_ut_sctx, &casc_ut_cctx);
}

static void del_bulk(void)
{
	struct m0_bufvec keys;
	struct m0_bufvec values;

	casc_ut_init(&casc_ut_sctx, &casc_ut_cctx);
	/* Bulk keys and  values. */
	vals_create(COUNT, COUNT_VAL_BYTES, &keys);
	vals_create(COUNT, COUNT_VAL_BYTES, &values);
	del_common(&keys, &values, 0, 0, 0, 0);
	m0_bufvec_free(&keys);
	m0_bufvec_free(&values);
	casc_ut_fini(&casc_ut_sctx, &casc_ut_cctx);
}

static void del_fail(void)
{
	struct m0_cas_rec_reply rep[COUNT];
	struct m0_cas_get_reply get_rep[COUNT];
	const struct m0_fid     ifid = IFID(2, 3);
	struct m0_cas_id        index = {};
	struct m0_bufvec        keys;
	struct m0_bufvec        values;
	int                     rc;

	casc_ut_init(&casc_ut_sctx, &casc_ut_cctx);

	M0_SET_ARR0(rep);
	M0_SET_ARR0(get_rep);
	rc = m0_bufvec_alloc(&keys, COUNT, sizeof(uint64_t));
	M0_UT_ASSERT(rc == 0);
	rc = m0_bufvec_alloc(&values, COUNT, sizeof(uint64_t));
	M0_UT_ASSERT(rc == 0);
	M0_UT_ASSERT(keys.ov_vec.v_nr != 0);
	M0_UT_ASSERT(keys.ov_vec.v_nr == values.ov_vec.v_nr);
	m0_forall(i, keys.ov_vec.v_nr, (*(uint64_t*)keys.ov_buf[i]   = i,
					*(uint64_t*)values.ov_buf[i] = i * i,
					true));
	rc = ut_idx_create(&casc_ut_cctx, &ifid, 1, rep);
	M0_UT_ASSERT(rc == 0);
	index.ci_fid = ifid;
	/* Insert new records */
	rc = ut_rec_put(&casc_ut_cctx, &index, &keys, &values, rep, 0);
	M0_UT_ASSERT(rc == 0);
	/* Delete all records */
	m0_fi_enable_once("creq_op_alloc", "cas_alloc_fail");
	rc = ut_rec_del(&casc_ut_cctx, &index, &keys, rep, 0);
	M0_UT_ASSERT(rc == -ENOMEM);
	m0_fi_enable_once("ctg_kbuf_get", "cas_alloc_fail");
	rc = ut_rec_del(&casc_ut_cctx, &index, &keys, rep, 0);
	M0_UT_ASSERT(rc == -ENOMEM);
	/* check selected values - must be empty*/
	m0_forall(i, values.ov_vec.v_nr,
		 (*(uint64_t*)values.ov_buf[i] = 0, true));
	rc = ut_rec_get(&casc_ut_cctx, &index, &keys, get_rep);
	M0_UT_ASSERT(rc == 0);
	M0_UT_ASSERT(m0_forall(i, COUNT, get_rep[i].cge_rc == 0));
	ut_get_rep_clear(get_rep, COUNT);
	m0_bufvec_free(&keys);
	m0_bufvec_free(&values);
	casc_ut_fini(&casc_ut_sctx, &casc_ut_cctx);
}

static void del_n(void)
{
	struct m0_cas_rec_reply rep[COUNT];
	struct m0_cas_get_reply get_rep[COUNT];
	const struct m0_fid     ifid = IFID(2, 3);
	struct m0_cas_id        index = {};
	struct m0_bufvec        keys;
	struct m0_bufvec        values;
	int                     rc;

	casc_ut_init(&casc_ut_sctx, &casc_ut_cctx);

	M0_SET_ARR0(rep);
	M0_SET_ARR0(get_rep);
	rc = m0_bufvec_alloc(&keys, COUNT, sizeof(uint64_t));
	M0_UT_ASSERT(rc == 0);
	rc = m0_bufvec_alloc(&values, COUNT, sizeof(uint64_t));
	M0_UT_ASSERT(rc == 0);
	M0_UT_ASSERT(keys.ov_vec.v_nr != 0);
	M0_UT_ASSERT(keys.ov_vec.v_nr == COUNT);
	M0_UT_ASSERT(keys.ov_vec.v_nr == values.ov_vec.v_nr);
	m0_forall(i, keys.ov_vec.v_nr, (*(uint64_t*)keys.ov_buf[i]   = i,
					*(uint64_t*)values.ov_buf[i] = i * i,
					true));
	rc = ut_idx_create(&casc_ut_cctx, &ifid, 1, rep);
	M0_UT_ASSERT(rc == 0);
	index.ci_fid = ifid;
	/* Insert new records */
	rc = ut_rec_put(&casc_ut_cctx, &index, &keys, &values, rep, 0);
	M0_UT_ASSERT(rc == 0);
	/* Delete several records */
	keys.ov_vec.v_nr /= 3;
	rc = ut_rec_del(&casc_ut_cctx, &index, &keys, rep, 0);
	M0_UT_ASSERT(rc == 0);
	/* restore old count value */
	keys.ov_vec.v_nr = COUNT;
	/* check selected values - some records not found*/
	m0_forall(i, values.ov_vec.v_nr,
		 (*(uint64_t*)values.ov_buf[i] = 0, true));
	rc = ut_rec_get(&casc_ut_cctx, &index, &keys, get_rep);
	M0_UT_ASSERT(rc == 0);
	M0_UT_ASSERT(m0_forall(i, COUNT,
			       (i < COUNT / 3) ? get_rep[i].cge_rc == -ENOENT :
			       rep[i].crr_rc == 0 &&
			       *(uint64_t*)get_rep[i].cge_val.b_addr == i * i));
	ut_get_rep_clear(get_rep, COUNT);
	m0_bufvec_free(&keys);
	m0_bufvec_free(&values);
	casc_ut_fini(&casc_ut_sctx, &casc_ut_cctx);
}

static void null_value(void)
{
	struct m0_cas_rec_reply  rep[COUNT];
	struct m0_cas_get_reply  get_rep[COUNT];
	struct m0_cas_next_reply next_rep[COUNT];
	const struct m0_fid      ifid = IFID(2, 3);
	struct m0_cas_id         index = {};
	struct m0_bufvec         keys;
	struct m0_bufvec         values;
	struct m0_bufvec         start_key;
	uint32_t                 recs_nr;
	uint64_t                 rep_count;
	int                      rc;

	casc_ut_init(&casc_ut_sctx, &casc_ut_cctx);

	M0_SET_ARR0(rep);
	rc = m0_bufvec_alloc(&keys, COUNT, sizeof(uint64_t));
	M0_UT_ASSERT(rc == 0);
	rc = m0_bufvec_empty_alloc(&values, COUNT);
	M0_UT_ASSERT(rc == 0);
	m0_forall(i, keys.ov_vec.v_nr, *(uint64_t*)keys.ov_buf[i] = i, true);
	rc = ut_idx_create(&casc_ut_cctx, &ifid, 1, rep);
	M0_UT_ASSERT(rc == 0);
	index.ci_fid = ifid;

	/* Insert new records with empty (NULL) values. */
	rc = ut_rec_put(&casc_ut_cctx, &index, &keys, &values, rep, 0);
	M0_UT_ASSERT(rc == 0);

	/* Get inserted records through 'GET' request. */
	rc = ut_rec_get(&casc_ut_cctx, &index, &keys, get_rep);
	M0_UT_ASSERT(rc == 0);
	M0_UT_ASSERT(m0_forall(i, keys.ov_vec.v_nr,
			       (get_rep[i].cge_rc == 0 &&
				get_rep[i].cge_val.b_addr == NULL &&
				get_rep[i].cge_val.b_nob == 0)));

	/* Get inserted records through 'NEXT' request. */
	rc = m0_bufvec_alloc(&start_key, 1, keys.ov_vec.v_count[0]);
	M0_UT_ASSERT(rc == 0);
	recs_nr = COUNT;
	value_create(start_key.ov_vec.v_count[0], 0, start_key.ov_buf[0]);
	rc = ut_next_rec(&casc_ut_cctx, &index, &start_key, &recs_nr, next_rep,
			 &rep_count, 0);
	M0_UT_ASSERT(rc == 0);
	M0_UT_ASSERT(rc == 0);
	M0_UT_ASSERT(rep_count == recs_nr);
	M0_UT_ASSERT(m0_forall(i, rep_count, next_rep[i].cnp_rc == 0));
	M0_UT_ASSERT(m0_forall(i, rep_count,
			       next_rep_equals(&next_rep[i],
					       keys.ov_buf[i],
					       values.ov_buf[i])));
	m0_bufvec_free(&start_key);

	/* Delete records. */
	rc = ut_rec_del(&casc_ut_cctx, &index, &keys, rep, 0);
	M0_UT_ASSERT(rc == 0);
	M0_UT_ASSERT(m0_forall(i, COUNT, rep[i].crr_rc == 0));

	m0_bufvec_free(&keys);
	m0_bufvec_free(&values);
	casc_ut_fini(&casc_ut_sctx, &casc_ut_cctx);
}

static void get_common(struct m0_bufvec *keys, struct m0_bufvec *values)
{
	struct m0_cas_rec_reply rep[COUNT];
	struct m0_cas_get_reply get_rep[COUNT];
	const struct m0_fid     ifid = IFID(2, 3);
	struct m0_cas_id        index = {};
	int                     rc;

	M0_SET_ARR0(rep);
	M0_SET_ARR0(get_rep);

	rc = ut_idx_create(&casc_ut_cctx, &ifid, 1, rep);
	M0_UT_ASSERT(rc == 0);
	M0_UT_ASSERT(rep[0].crr_rc == 0);
	index.ci_fid = ifid;
	/* Insert new records */
	rc = ut_rec_put(&casc_ut_cctx, &index, keys, values, rep, 0);
	M0_UT_ASSERT(rc == 0);
	M0_UT_ASSERT(m0_forall(i, COUNT, rep[i].crr_rc == 0));

	/* check selected values */
	rc = ut_rec_get(&casc_ut_cctx, &index, keys, get_rep);
	M0_UT_ASSERT(rc == 0);
	M0_UT_ASSERT(m0_forall(i, keys->ov_vec.v_nr,
		     memcmp(get_rep[i].cge_val.b_addr,
			    values->ov_buf[i],
			    values->ov_vec.v_count[i] ) == 0));
	ut_get_rep_clear(get_rep, keys->ov_vec.v_nr);

	/* Remove index. */
	rc = ut_idx_delete(&casc_ut_cctx, &ifid, 1, rep);
	M0_UT_ASSERT(rc == 0);
}

static void get(void)
{
	struct m0_bufvec keys;
	struct m0_bufvec values;
	int              rc;

	casc_ut_init(&casc_ut_sctx, &casc_ut_cctx);
	rc = m0_bufvec_alloc(&keys, COUNT, sizeof(uint64_t));
	M0_UT_ASSERT(rc == 0);
	rc = m0_bufvec_alloc(&values, COUNT, sizeof(uint64_t));
	M0_UT_ASSERT(rc == 0);
	M0_UT_ASSERT(keys.ov_vec.v_nr == COUNT);
	M0_UT_ASSERT(keys.ov_vec.v_nr == values.ov_vec.v_nr);
	m0_forall(i, keys.ov_vec.v_nr, (*(uint64_t*)keys.ov_buf[i]   = i,
					*(uint64_t*)values.ov_buf[i] = i * i,
					true));
	get_common(&keys, &values);
	m0_bufvec_free(&keys);
	m0_bufvec_free(&values);
	casc_ut_fini(&casc_ut_sctx, &casc_ut_cctx);
}

static void get_bulk(void)
{
	struct m0_bufvec keys;
	struct m0_bufvec values;
	int              rc;

	casc_ut_init(&casc_ut_sctx, &casc_ut_cctx);
	/* Bulk keys and values. */
	vals_create(COUNT, COUNT_VAL_BYTES, &keys);
	vals_create(COUNT, COUNT_VAL_BYTES, &values);
	get_common(&keys, &values);
	m0_bufvec_free(&keys);
	m0_bufvec_free(&values);

	/* Bulk values. */
	rc = m0_bufvec_alloc(&keys, COUNT, sizeof(uint64_t));
	M0_UT_ASSERT(rc == 0);
	m0_forall(i, keys.ov_vec.v_nr, (*(uint64_t*)keys.ov_buf[i] = i, true));
	vals_create(COUNT, COUNT_VAL_BYTES, &values);
	get_common(&keys, &values);
	m0_bufvec_free(&keys);
	m0_bufvec_free(&values);

	/* Bulk keys. */
	vals_create(COUNT, COUNT_VAL_BYTES, &keys);
	rc = m0_bufvec_alloc(&values, COUNT, sizeof(uint64_t));
	M0_UT_ASSERT(rc == 0);
	m0_forall(i, values.ov_vec.v_nr, (*(uint64_t*)values.ov_buf[i] = i,
					true));
	get_common(&keys, &values);
	m0_bufvec_free(&keys);
	m0_bufvec_free(&values);

	/* Bulk mix. */
	vals_mix_create(COUNT, COUNT_VAL_BYTES, &keys);
	rc = m0_bufvec_alloc(&values, COUNT, sizeof(uint64_t));
	M0_UT_ASSERT(rc == 0);
	m0_forall(i, values.ov_vec.v_nr, (*(uint64_t*)values.ov_buf[i]   = i,
					true));
	get_common(&keys, &values);
	m0_bufvec_free(&keys);
	m0_bufvec_free(&values);

	rc = m0_bufvec_alloc(&keys, COUNT, sizeof(uint64_t));
	M0_UT_ASSERT(rc == 0);
	m0_forall(i, keys.ov_vec.v_nr, (*(uint64_t*)keys.ov_buf[i]   = i,
					true));
	vals_mix_create(COUNT, COUNT_VAL_BYTES, &values);
	get_common(&keys, &values);
	m0_bufvec_free(&keys);
	m0_bufvec_free(&values);
	casc_ut_fini(&casc_ut_sctx, &casc_ut_cctx);
}

static void get_fail(void)
{
	struct m0_cas_rec_reply rep[COUNT];
	struct m0_cas_get_reply get_rep[COUNT];
	const struct m0_fid     ifid = IFID(2, 3);
	struct m0_cas_id        index = {};
	struct m0_bufvec        keys;
	struct m0_bufvec        values;
	int                     rc;

	casc_ut_init(&casc_ut_sctx, &casc_ut_cctx);

	M0_SET_ARR0(rep);
	M0_SET_ARR0(get_rep);
	rc = m0_bufvec_alloc(&keys, COUNT, sizeof(uint64_t));
	M0_UT_ASSERT(rc == 0);
	rc = m0_bufvec_alloc(&values, COUNT, sizeof(uint64_t));
	M0_UT_ASSERT(rc == 0);
	M0_UT_ASSERT(keys.ov_vec.v_nr == COUNT);
	M0_UT_ASSERT(keys.ov_vec.v_nr == values.ov_vec.v_nr);
	m0_forall(i, keys.ov_vec.v_nr, (*(uint64_t*)keys.ov_buf[i]   = i,
					*(uint64_t*)values.ov_buf[i] = i * i,
					true));
	rc = ut_idx_create(&casc_ut_cctx, &ifid, 1, rep);
	M0_UT_ASSERT(rc == 0);
	index.ci_fid = ifid;
	/* Insert new records */
	rc = ut_rec_put(&casc_ut_cctx, &index, &keys, &values, rep, 0);
	M0_UT_ASSERT(rc == 0);

	m0_forall(i, values.ov_vec.v_nr / 3,
		 (*(uint64_t*)values.ov_buf[i] = 0, true));
	/* check selected values */
	m0_fi_enable_once("creq_op_alloc", "cas_alloc_fail");
	rc = ut_rec_get(&casc_ut_cctx, &index, &keys, get_rep);
	M0_UT_ASSERT(rc == -ENOMEM);
	m0_fi_enable_once("ctg_kbuf_get", "cas_alloc_fail");
	rc = ut_rec_get(&casc_ut_cctx, &index, &keys, get_rep);
	M0_UT_ASSERT(rc == -ENOMEM);
	m0_bufvec_free(&keys);
	m0_bufvec_free(&values);
	casc_ut_fini(&casc_ut_sctx, &casc_ut_cctx);
}

static void recs_count(void)
{
	struct m0_cas_rec_reply rep[COUNT];
	const struct m0_fid     ifid = IFID(2, 3);
	struct m0_cas_id        index = {};
	struct m0_bufvec        keys;
	struct m0_bufvec        values;
	int                     rc;

	casc_ut_init(&casc_ut_sctx, &casc_ut_cctx);
	rc = m0_bufvec_alloc(&keys, COUNT, sizeof(uint64_t));
	M0_UT_ASSERT(rc == 0);
	rc = m0_bufvec_alloc(&values, COUNT, sizeof(uint64_t));
	M0_UT_ASSERT(rc == 0);
	M0_UT_ASSERT(keys.ov_vec.v_nr == COUNT);
	M0_UT_ASSERT(keys.ov_vec.v_nr == values.ov_vec.v_nr);
	m0_forall(i, keys.ov_vec.v_nr, (*(uint64_t*)keys.ov_buf[i]   = i,
					*(uint64_t*)values.ov_buf[i] = i * i,
					true));
	M0_SET_ARR0(rep);
	M0_UT_ASSERT(m0_ctg_rec_nr() == 0);
	M0_UT_ASSERT(m0_ctg_rec_size() == 0);
	rc = ut_idx_create(&casc_ut_cctx, &ifid, 1, rep);
	M0_UT_ASSERT(rc == 0);
	index.ci_fid = ifid;
	rc = ut_rec_put(&casc_ut_cctx, &index, &keys, &values, rep, 0);
	M0_UT_ASSERT(rc == 0);
	M0_UT_ASSERT(m0_ctg_rec_nr() == COUNT);
	M0_UT_ASSERT(m0_ctg_rec_size() == COUNT * 2 * sizeof(uint64_t));
	rc = ut_rec_del(&casc_ut_cctx, &index, &keys, rep, 0);
	M0_UT_ASSERT(rc == 0);
	M0_UT_ASSERT(m0_ctg_rec_nr() == 0);
	/* Currently total size of records is not decremented on deletion. */
	M0_UT_ASSERT(m0_ctg_rec_size() == COUNT * 2 * sizeof(uint64_t));

	/*
	 * Check total records size overflow.
	 * The total records size in this case should stick to ~0ULL.
	 */
	m0_fi_enable_off_n_on_m("ctg_state_update", "test_overflow", 1, 1);
	rc = ut_rec_put(&casc_ut_cctx, &index, &keys, &values, rep, 0);
	m0_fi_disable("ctg_state_update", "test_overflow");
	M0_UT_ASSERT(rc == 0);
	M0_UT_ASSERT(m0_ctg_rec_nr() == COUNT);
	M0_UT_ASSERT(m0_ctg_rec_size() == ~0ULL);
	/* Remove index. */
	rc = ut_idx_delete(&casc_ut_cctx, &ifid, 1, rep);
	M0_UT_ASSERT(rc == 0);
	m0_bufvec_free(&keys);
	m0_bufvec_free(&values);
	casc_ut_fini(&casc_ut_sctx, &casc_ut_cctx);
}

static void reply_too_large(void)
{
	struct m0_bufvec         keys;
	struct m0_bufvec         values;
	struct m0_cas_rec_reply  rep[COUNT];
	struct m0_cas_get_reply  get_rep[COUNT];
	struct m0_cas_next_reply next_rep[COUNT];
	const struct m0_fid      ifid = IFID(2, 3);
	struct m0_bufvec         start_key;
	uint32_t                 recs_nr;
	uint64_t                 rep_count;
	struct m0_cas_id         index = {};
	int                      rc;

	casc_ut_init(&casc_ut_sctx, &casc_ut_cctx);
	rc = m0_bufvec_alloc(&keys, COUNT, sizeof(uint64_t));
	M0_UT_ASSERT(rc == 0);
	rc = m0_bufvec_alloc(&values, COUNT, sizeof(uint64_t));
	M0_UT_ASSERT(rc == 0);
	M0_UT_ASSERT(keys.ov_vec.v_nr == COUNT);
	M0_UT_ASSERT(keys.ov_vec.v_nr == values.ov_vec.v_nr);
	m0_forall(i, keys.ov_vec.v_nr, (*(uint64_t*)keys.ov_buf[i]   = i,
					*(uint64_t*)values.ov_buf[i] = i * i,
					true));
	M0_SET_ARR0(rep);
	M0_SET_ARR0(get_rep);

	rc = ut_idx_create(&casc_ut_cctx, &ifid, 1, rep);
	M0_UT_ASSERT(rc == 0);
	M0_UT_ASSERT(rep[0].crr_rc == 0);
	index.ci_fid = ifid;
	/* Insert new records */
	rc = ut_rec_put(&casc_ut_cctx, &index, &keys, &values, rep, 0);
	M0_UT_ASSERT(rc == 0);
	M0_UT_ASSERT(m0_forall(i, COUNT, rep[i].crr_rc == 0));

	m0_fi_enable_off_n_on_m("m0_rpc_item_max_payload_exceeded",
				"payload_too_large1", 1, 1);
	rc = ut_rec_get(&casc_ut_cctx, &index, &keys, get_rep);
	M0_UT_ASSERT(rc == -E2BIG);

	M0_SET_ARR0(next_rep);
	rc = m0_bufvec_alloc(&start_key, 1, keys.ov_vec.v_count[0]);
	M0_UT_ASSERT(rc == 0);
	/* perform next for all records */
	recs_nr = COUNT;
	value_create(start_key.ov_vec.v_count[0], 0, start_key.ov_buf[0]);
	rc = ut_next_rec(&casc_ut_cctx, &index, &start_key, &recs_nr, next_rep,
			 &rep_count, 0);
	M0_UT_ASSERT(rc == -E2BIG);
	m0_fi_disable("m0_rpc_item_max_payload_exceeded", "payload_too_large1");

	/* Remove index. */
	rc = ut_idx_delete(&casc_ut_cctx, &ifid, 1, rep);
	M0_UT_ASSERT(rc == 0);

	m0_bufvec_free(&keys);
	m0_bufvec_free(&values);
	casc_ut_fini(&casc_ut_sctx, &casc_ut_cctx);
}

enum named_version {
	PAST   = 2,
	FUTURE = 3,
};

enum named_outcome {
	/* No value at all. */
	TOMBSTONE,
	/* New value was not inserted. */
	PRESERVED,
	/* Old value was replaced by the new value. */
	OVERWRITTEN,
};

enum named_op {
	NOP,
	PUT,
	DEL,
};

/* PUT or DEL operation with a human-readable version description. */
struct cas_ver_op {
	enum named_version       ver;
	enum named_op            op;
	const struct m0_bufvec  *keys;
	const struct m0_bufvec  *values;
};

struct put_del_ver_case {
	/* An operation to be executed first. */
	struct cas_ver_op  before;
	/* Second operation that happen right after the first one. */
	struct cas_ver_op  after;
};

static void cas_ver_op_execute(const struct cas_ver_op *cvop,
			       struct m0_cas_id        *index)
{
	switch (cvop->op) {
	case PUT:
		ut_rec_common_put_verified(index, cvop->keys, cvop->values,
					   cvop->ver,
					   COF_VERSIONED | COF_OVERWRITE);
		break;
	case DEL:
		ut_rec_common_del_verified(index, cvop->keys, cvop->ver,
					   COF_VERSIONED);
		break;
	case NOP:
		/* Nothing to do. */
		break;
	}
}

static void put_del_ver_case_execute(const struct put_del_ver_case *c,
				     struct m0_cas_id              *index)
{
	cas_ver_op_execute(&c->before, index);
	cas_ver_op_execute(&c->after, index);
}

/*
 * The function verifies the following properties:
 *  Put a tombstone:
 *   DEL@version
 *   has_tombstone => true
 *  Put some old version:
 *   PUT@(version - 1)
 *   has_tombstone => true
 *  Put some new version:
 *   PUT@(version + 1)
 *   has_tombstone => false.
 */
static void verify_version_properties(struct m0_cas_id       *index,
				      const struct m0_bufvec *keys,
				      uint64_t                version)
{
	int                      rc;
	struct m0_cas_get_reply *get_rep;
	struct m0_bufvec         values;
	bool                     tombstones;

	M0_ALLOC_ARR(get_rep, keys->ov_vec.v_nr);
	M0_UT_ASSERT(get_rep != NULL);

	/* Save the existing values. */
	rc = ut_rec__get(&casc_ut_cctx, index, keys, get_rep, 0);
	M0_UT_ASSERT(rc == 0);
	M0_UT_ASSERT(m0_forall(i, keys->ov_vec.v_nr,
			       get_rep[i].cge_rc == 0));
	rc = get_reply2bufvec(get_rep, keys->ov_vec.v_nr, &values);
	M0_UT_ASSERT(rc == 0);
	m0_free(get_rep);
	/* Save the existing tombstones (assuming all-or-nothing). */
	tombstones = has_tombstones(index, keys);

	if (!tombstones) {
		ut_rec_common_del_verified(index, keys, version, COF_VERSIONED);
	}
	M0_UT_ASSERT(has_tombstones(index, keys));

	ut_rec_common_put_verified(index, keys, &values, version - 1,
				   COF_VERSIONED | COF_OVERWRITE);
	M0_UT_ASSERT(has_tombstones(index, keys));

	ut_rec_common_put_verified(index, keys, &values, version + 1,
				   COF_VERSIONED | COF_OVERWRITE);
	M0_UT_ASSERT(!has_tombstones(index, keys));

	/* Restore the existing values. */
	ut_rec_common_del_verified(index, keys, 0, 0);
	ut_rec_common_put_verified(index, keys, &values, version,
				   COF_VERSIONED | COF_OVERWRITE);
	M0_UT_ASSERT(!has_tombstones(index, keys));
	/* Restore the existing tombstones. */
	if (tombstones) {
		ut_rec_common_del_verified(index, keys, version,
					   COF_VERSIONED);
		M0_UT_ASSERT(has_tombstones(index, keys));
	}
}

static enum named_outcome outcome(const struct put_del_ver_case *c)
{
	const struct cas_ver_op *before = &c->before;
	const struct cas_ver_op *after  = &c->after;
	const struct cas_ver_op *winner;

	/* Get the operation with the "latest" version. */
	if (before->ver == FUTURE)
		winner = before;
	else if (after->ver == FUTURE)
		winner = after;
	else
		M0_IMPOSSIBLE("Did you forget to add an op with FUTURE?");

	if (winner->op == DEL)
		return TOMBSTONE;
	else if (winner == before)
		return PRESERVED;
	else
		return OVERWRITTEN;
}

static void put_del_ver_case_verify(const struct put_del_ver_case *c,
				    struct m0_cas_id              *index)
{
	const struct m0_bufvec *keys = c->before.keys;
	const struct m0_bufvec *before_values = c->before.values;
	const struct m0_bufvec *after_values = c->after.values;

	switch (outcome(c)) {
	case TOMBSTONE:
		M0_UT_ASSERT(has_tombstones(index, keys));
		M0_UT_ASSERT(!has_values(index, keys, before_values,
				       COF_VERSIONED));
		M0_UT_ASSERT(!has_values(index, keys, after_values,
				       COF_VERSIONED));
		break;
	case OVERWRITTEN:
		M0_UT_ASSERT(!has_tombstones(index, keys));
		/*
		 * The values should be overwritten no matter what behavior we
		 * have specified.
		 */
		M0_UT_ASSERT(has_values(index, keys, after_values, 0));
		M0_UT_ASSERT(has_values(index, keys, after_values,
				       COF_VERSIONED));
		M0_UT_ASSERT(!has_values(index, keys, before_values, 0));
		M0_UT_ASSERT(!has_values(index, keys, before_values,
				       COF_VERSIONED));
		break;
	case PRESERVED:
		M0_UT_ASSERT(!has_tombstones(index, keys));
		/*
		 * The values should be preserved, and it should be visible
		 * even if the versioned behavior was not requested by GET
		 * operation.
		 */
		M0_UT_ASSERT(has_values(index, keys, before_values, 0));
		M0_UT_ASSERT(has_values(index, keys, before_values,
				       COF_VERSIONED));
		M0_UT_ASSERT(!has_values(index, keys, after_values, 0));
		M0_UT_ASSERT(!has_values(index, keys, after_values,
				       COF_VERSIONED));
		break;
	}

	M0_UT_ASSERT(has_versions(index, keys, FUTURE, COF_VERSIONED));
	verify_version_properties(index, keys, FUTURE);
}

static void put_del_ver(void)
{
	const uint64_t           key = 1;
	const uint64_t           before_value = 1;
	const uint64_t           after_value  = 2;
	const m0_bcount_t        nr_bytes = sizeof(key);
	const void              *key_data = &key;
	const void              *before_val_data = &before_value;
	const void              *after_val_data = &after_value;
	const struct m0_bufvec   keys =
		M0_BUFVEC_INIT_BUF((void **) &key_data,
				   (m0_bcount_t *) &nr_bytes);
	const struct m0_bufvec   before_values =
		M0_BUFVEC_INIT_BUF((void **) &before_val_data,
				   (m0_bcount_t *) &nr_bytes);
	const struct m0_bufvec   after_values =
		M0_BUFVEC_INIT_BUF((void **) &after_val_data,
				   (m0_bcount_t *) &nr_bytes);

#define BEFORE(_what, _when)              \
	.before = {                       \
		.ver   = _when,           \
		.op    = _what,           \
		.keys   = &keys,          \
		.values = &before_values, \
	},

#define AFTER(_what, _when)              \
	.after = {                       \
		.ver    = _when,         \
		.op     = _what,         \
		.keys   = &keys,         \
		.values = &after_values, \
	},

	const struct put_del_ver_case cases[] = {
		{ BEFORE(PUT, PAST)   AFTER(DEL, FUTURE) },
		{ BEFORE(PUT, PAST)   AFTER(PUT, FUTURE) },

		{ BEFORE(PUT, FUTURE) AFTER(PUT, PAST)   },
		{ BEFORE(PUT, FUTURE) AFTER(DEL, PAST)   },

		/* May want to re-enable these with tombstone cleanup

		{ BEFORE(DEL, FUTURE) AFTER(DEL, PAST)   },
		{ BEFORE(DEL, FUTURE) AFTER(PUT, PAST)   },

		{ BEFORE(DEL, PAST)   AFTER(DEL, FUTURE) },
		{ BEFORE(DEL, PAST)   AFTER(PUT, FUTURE) },

		{ BEFORE(NOP, PAST)   AFTER(DEL, FUTURE) },
		*/
		{ BEFORE(NOP, PAST)   AFTER(PUT, FUTURE) },
	};
#undef BEFORE
#undef AFTER
#undef OUTCOME

	int                     rc;
	int                     i;
	struct m0_cas_id        index = {};
	struct m0_cas_rec_reply rep;
	const struct m0_fid     ifid = IFID(2, 3);

	casc_ut_init(&casc_ut_sctx, &casc_ut_cctx);
	rc = ut_idx_create(&casc_ut_cctx, &ifid, 1, &rep);
	M0_UT_ASSERT(rc == 0);
	index.ci_fid = ifid;

	for (i = 0; i < ARRAY_SIZE(cases); ++i) {
		put_del_ver_case_execute(&cases[i], &index);
		put_del_ver_case_verify(&cases[i],  &index);
		ut_rec_common_del_verified(&index, cases[i].before.keys, 0, 0);
	}

	rc = ut_idx_delete(&casc_ut_cctx, &ifid, 1, &rep);
	M0_UT_ASSERT(rc == 0);
	casc_ut_fini(&casc_ut_sctx, &casc_ut_cctx);
}

struct m0_ut_suite cas_client_ut = {
	.ts_name   = "cas-client",
	.ts_owners = "Leonid",
	.ts_init   = NULL,
	.ts_fini   = NULL,
	.ts_tests  = {
		{ "idx-create",             idx_create,             "Leonid" },
		{ "idx-create-fail",        idx_create_fail,        "Leonid" },
		{ "idx-create-async",       idx_create_a,           "Leonid" },
		{ "idx-delete",             idx_delete,             "Leonid" },
		{ "idx-delete-fail",        idx_delete_fail,        "Leonid" },
		{ "idx-delete-non-exist",   idx_delete_non_exist,   "Sergey" },
		{ "idx-createN",            idx_create_n,           "Leonid" },
		{ "idx-deleteN",            idx_delete_n,           "Leonid" },
		{ "idx-list",               idx_list,               "Leonid" },
		{ "idx-list-fail",          idx_list_fail,          "Leonid" },
		{ "next",                   next,                   "Leonid" },
		{ "next-fail",              next_fail,              "Leonid" },
		{ "next-multi",             next_multi,             "Egor"   },
		{ "next-bulk",              next_bulk,              "Leonid" },
		{ "next-multi-bulk",        next_multi_bulk,        "Leonid" },
		{ "put",                    put,                    "Leonid" },
		{ "put-bulk",               put_bulk,               "Leonid" },
		{ "put-create",             put_create,             "Sergey" },
		{ "put-overwrite",          put_overwrite,          "Sergey" },
		{ "put-crow",               put_crow,               "Sergey" },
		{ "put-fail",               put_fail,               "Leonid" },
		{ "put-bulk-fail",          put_bulk_fail,          "Leonid" },
		{ "put-crow-fail",          put_crow_fail,          "Sergey" },
		{ "get",                    get,                    "Leonid" },
		{ "get-bulk",               get_bulk,               "Leonid" },
		{ "get-fail",               get_fail,               "Leonid" },
		{ "upd",                    upd,                    "Leonid" },
		{ "del",                    del,                    "Leonid" },
		{ "del-bulk",               del_bulk,               "Leonid" },
		{ "del-fail",               del_fail,               "Leonid" },
		{ "delN",                   del_n,                  "Leonid" },
		{ "null-value",             null_value,             "Egor"   },
		{ "idx-tree-insert",        idx_tree_insert,        "Leonid" },
		{ "idx-tree-delete",        idx_tree_delete,        "Leonid" },
		{ "idx-tree-delete-fail",   idx_tree_delete_fail,   "Leonid" },
		{ "recs-count",             recs_count,             "Leonid" },
		{ "reply-too-large",        reply_too_large,        "Sergey" },
		{ "recs-fragm",             recs_fragm,             "Sergey" },
		{ "recs_fragm_fail",        recs_fragm_fail,        "Sergey" },
		{ "put-ver",                put_ver,                "Ivan"   },
		{ "put-overwrite-ver",      put_overwrite_ver,      "Ivan"   },
		{ "del-ver",                del_ver,                "Ivan"   },
		{ "next-ver",               next_ver,               "Ivan"   },
		{ "put-del-ver",            put_del_ver,            "Ivan"   },
		{ "next-ver-exposed",       next_ver_exposed,       "Ivan"   },
		{ "get-ver-exposed",        get_ver_exposed,        "Ivan"   },
		{ NULL, NULL }
	}
};

#undef M0_TRACE_SUBSYSTEM

/** @} end of cas group */

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
