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
 */

#define M0_TRACE_SUBSYSTEM M0_TRACE_SUBSYS_DTM0
#include "lib/trace.h"
#include "dtm0/service.h"
#include "be/dtm0_log.h"             /* DTM0 log API */
#include "dtm0/addb2.h"              /* M0_AVI_DTX0_SM_STATE */
#include "dtm0/drlink.h"             /* m0_dtm0_rpc_link_mod_{init,fini} */
#include "dtm0/dtx.h"                /* m0_dtm0_dtx_domain_init */
#include "dtm0/fop.h"                /* m0_dtm0_fop_{init,fini} */
#include "dtm0/svc_internal.h"       /* dtm0_process */
#include "lib/errno.h"               /* ENOMEM and so on */
#include "lib/memory.h"              /* M0_ALLOC_PTR */
#include "lib/string.h"              /* streq */
#include "lib/tlist.h"               /* tlist API */
#include "module/instance.h"         /* m0_get */
#include "reqh/reqh.h"               /* m0_reqh */
#include "reqh/reqh.h"               /* m0_reqh2confc */
#include "reqh/reqh_service.h"       /* m0_reqh_service */
#include "rpc/rpc_machine.h"         /* m0_rpc_machine */

static struct m0_dtm0_service *to_dtm(struct m0_reqh_service *service);
static int dtm0_service_start(struct m0_reqh_service *service);
static void dtm0_service_stop(struct m0_reqh_service *service);
static void dtm0_service_prepare_to_stop(struct m0_reqh_service *service);
static int dtm0_service_allocate(struct m0_reqh_service **service,
				 const struct m0_reqh_service_type *stype);
static void dtm0_service_fini(struct m0_reqh_service *service);

static const struct m0_reqh_service_type_ops dtm0_service_type_ops = {
	.rsto_service_allocate = dtm0_service_allocate
};

static const struct m0_reqh_service_ops dtm0_service_ops = {
	.rso_start           = dtm0_service_start,
	.rso_stop            = dtm0_service_stop,
	.rso_fini            = dtm0_service_fini,
	.rso_prepare_to_stop = dtm0_service_prepare_to_stop,
};

struct m0_reqh_service_type dtm0_service_type = {
	.rst_name  = "M0_CST_DTM0",
	.rst_ops   = &dtm0_service_type_ops,
	.rst_level = M0_RS_LEVEL_LATE,
};


M0_TL_DESCR_DEFINE(dopr, "dtm0_process", static, struct dtm0_process, dop_link,
		   dop_magic, M0_DTM0_PROC_MAGIC, M0_DTM0_PROC_HEAD_MAGIC);
M0_TL_DEFINE(dopr, static, struct dtm0_process);

/**
 * typed container_of
 */
static const struct m0_bob_type dtm0_service_bob = {
	.bt_name = "dtm0 service",
	.bt_magix_offset = M0_MAGIX_OFFSET(struct m0_dtm0_service, dos_magix),
	.bt_magix = M0_DTM0_SVC_MAGIC,
	.bt_check = NULL
};
M0_BOB_DEFINE(static, &dtm0_service_bob, m0_dtm0_service);

static struct m0_dtm0_service *to_dtm(struct m0_reqh_service *service)
{
	return bob_of(service, struct m0_dtm0_service, dos_generic,
		      &dtm0_service_bob);
}

M0_INTERNAL struct m0_dtm0_service *m0_dtm0_fom2service(struct m0_fom *fom)
{
	return to_dtm(fom->fo_service);
}

/**
 * Service part
 */
static void dtm0_service__init(struct m0_dtm0_service *s)
{
	m0_dtm0_service_bob_init(s);
	dopr_tlist_init(&s->dos_processes);
	m0_dtm0_dtx_domain_init();
	m0_dtm0_clk_src_init(&s->dos_clk_src, M0_DTM0_CS_PHYS);
}

static void dtm0_service__fini(struct m0_dtm0_service *s)
{
	m0_dtm0_clk_src_fini(&s->dos_clk_src);
	m0_dtm0_dtx_domain_fini();
	dopr_tlist_fini(&s->dos_processes);
	m0_dtm0_service_bob_fini(s);
}

M0_INTERNAL int
m0_dtm_client_service_start(struct m0_reqh *reqh, struct m0_fid *cli_srv_fid,
			    struct m0_reqh_service **out)
{
	struct m0_reqh_service_type *svct;
	struct m0_reqh_service      *reqh_svc;
	int                          rc;

	svct = m0_reqh_service_type_find("M0_CST_DTM0");
	if (svct == NULL)
		return M0_ERR(-ENOENT);

	rc = m0_reqh_service_allocate(&reqh_svc, svct, NULL);
	if (rc != 0)
		return M0_ERR(rc);

	m0_reqh_service_init(reqh_svc, reqh, cli_srv_fid);

	rc = m0_reqh_service_start(reqh_svc);
	if (rc != 0)
		m0_reqh_service_fini(reqh_svc);
	else
		*out = reqh_svc;

	return M0_RC(rc);
}

M0_INTERNAL void m0_dtm_client_service_stop(struct m0_reqh_service *svc)
{
       m0_reqh_service_prepare_to_stop(svc);
       m0_reqh_idle_wait_for(svc->rs_reqh, svc);
       m0_reqh_service_stop(svc);
       m0_reqh_service_fini(svc);
}


M0_INTERNAL struct dtm0_process *
dtm0_service_process__lookup(struct m0_reqh_service *reqh_dtm0_svc,
			     const struct m0_fid    *remote_dtm0)
{
	return m0_tl_find(dopr, proc, &to_dtm(reqh_dtm0_svc)->dos_processes,
			  m0_fid_eq(&proc->dop_rserv_fid, remote_dtm0));
}


M0_INTERNAL int
m0_dtm0_service_process_connect(struct m0_reqh_service *s,
				struct m0_fid          *remote_srv,
				const char             *remote_ep,
				bool                    async)
{
	/*
	 * TODO: This function will be eliminated once drlink gets
	 * "synchronous" mode.
	 */
	M0_ASSERT_INFO(0, "Direct connect() call is not supported.");
}

static int dtm0_process_disconnect(struct dtm0_process *process)
{
	int                  rc;
	const m0_time_t      timeout =
		m0_time_from_now(DTM0_DISCONNECT_TIMEOUT_SECS, 0);

	M0_ENTRY("process=%p, rfid=" FID_F, process,
		 FID_P(&process->dop_rserv_fid));

	if (M0_IS0(&process->dop_rlink))
		return M0_RC(0);

	if (m0_rpc_link_is_connected(&process->dop_rlink)) {
		m0_rpc_conn_sessions_cancel(&process->dop_rlink.rlk_conn);
		rc = m0_rpc_link_disconnect_sync(&process->dop_rlink, timeout);
	} else
		rc = 0;


	if (M0_IN(rc, (0, -ETIMEDOUT, -ECANCELED))) {
		/*
		 * XXX: At this moment there is no special call to
		 * unconditionally terminate an RPC connection without
		 * communication with the other side. Because of that,
		 * we are reusing the existing ::m0_rpc_link_disconnect_sync,
		 * but ignoring the cases where the counterpart is
		 * dead (timed out) or has already disconnected
		 * from us (canceled).
		 */
		if (rc == -ETIMEDOUT || rc == -ECANCELED) {
			M0_LOG(M0_WARN, "Disconnect %s (suppressed)",
			       rc == -ETIMEDOUT ? "timed out" : "cancelled");
			rc = 0;
		}

		m0_rpc_link_fini(&process->dop_rlink);
		M0_SET0(&process->dop_rlink);
	}

	return M0_RC(rc);
}

M0_INTERNAL int
m0_dtm0_service_process_disconnect(struct m0_reqh_service *s,
				   struct m0_fid          *remote_srv)
{
	struct dtm0_process *process =
		dtm0_service_process__lookup(s, remote_srv);

	M0_ENTRY("rs=%p, remote="FID_F, s, FID_P(remote_srv));

	return process == NULL ? M0_ERR(-ENOENT) :
		M0_RC(dtm0_process_disconnect(process));
}

M0_INTERNAL struct m0_rpc_session *
m0_dtm0_service_process_session_get(struct m0_reqh_service *s,
				    const struct m0_fid    *remote_srv)
{
	struct dtm0_process *process =
		dtm0_service_process__lookup(s, remote_srv);

	return process == NULL ? NULL : &process->dop_rlink.rlk_sess;
}

static int dtm0_service__alloc(struct m0_reqh_service           **service,
			       const struct m0_reqh_service_type *stype,
			       const struct m0_reqh_service_ops  *ops)
{
	struct m0_dtm0_service *s;

	M0_PRE(stype != NULL && service != NULL && ops != NULL);

	M0_ALLOC_PTR(s);
	if (s == NULL)
		return M0_ERR(-ENOMEM);

	s->dos_generic.rs_type = stype;
	s->dos_generic.rs_ops  = ops;
	dtm0_service__init(s);
	*service = &s->dos_generic;

	return M0_RC(0);
}

static int dtm0_service_allocate(struct m0_reqh_service           **service,
				 const struct m0_reqh_service_type *stype)
{
	return dtm0_service__alloc(service, stype, &dtm0_service_ops);
}

static int volatile_log_init(struct m0_dtm0_service *dtm0)
{
	int rc;

	rc = m0_be_dtm0_log_alloc(&dtm0->dos_log);
	if (rc == 0) {
		rc = m0_be_dtm0_log_init(dtm0->dos_log, NULL,
					 &dtm0->dos_clk_src, false);
		if (rc != 0)
			m0_be_dtm0_log_free0(&dtm0->dos_log);
	}
	return rc;
}

static int persistent_log_init(struct m0_dtm0_service *dtm0)
{
	struct m0_reqh *reqh = dtm0->dos_generic.rs_reqh;

	M0_PRE(reqh != NULL);

	dtm0->dos_log = m0_reqh_lockers_get(reqh, m0_get()->i_dtm0_log_key);
	/* 0type should create it during mkfs */
	M0_ASSERT_INFO(dtm0->dos_log != NULL, "Forgot to do mkfs?");

	return m0_be_dtm0_log_init(dtm0->dos_log, reqh->rh_beseg,
			&dtm0->dos_clk_src, true);
}

static int dtm_service__origin_fill(struct m0_reqh_service *service)
{
	struct m0_conf_service *service_obj;
	struct m0_conf_obj     *obj;
	struct m0_confc        *confc = m0_reqh2confc(service->rs_reqh);
	const char            **param;
	struct m0_dtm0_service *dtm0 = to_dtm(service);
	int                     rc;

	M0_ENTRY("rs_svc=%p", service);

	/* W/A for UTs */
	if (!m0_confc_is_inited(confc)) {
		dtm0->dos_origin = DTM0_ON_VOLATILE;
		goto out;
	}

	obj = m0_conf_cache_lookup(&confc->cc_cache, &service->rs_service_fid);
	if (obj == NULL)
		return M0_ERR(-ENOENT);

	service_obj = M0_CONF_CAST(obj, m0_conf_service);

	if (service_obj->cs_params == NULL) {
		dtm0->dos_origin = DTM0_ON_VOLATILE;
		M0_LOG(M0_WARN, "dtm0 is treated as volatile,"
		       " no parameters given");
		goto out;
	}

	for (param = service_obj->cs_params; *param != NULL; ++param) {
		if (m0_streq(*param, "origin:in-volatile"))
			dtm0->dos_origin = DTM0_ON_VOLATILE;
		else if (m0_streq(*param, "origin:in-persistent"))
			dtm0->dos_origin = DTM0_ON_PERSISTENT;
	}

out:
	if (dtm0->dos_origin == DTM0_ON_VOLATILE)
		rc = volatile_log_init(dtm0);
	else if (dtm0->dos_origin == DTM0_ON_PERSISTENT)
		rc = persistent_log_init(dtm0);
	else
		rc = M0_ERR(-EINVAL);

	return M0_RC_INFO(rc, "origin=%d", dtm0->dos_origin);
}

/*
 * Certain UTs manually control the lifetime of recovery machine.
 * When manual start-stop is disabled, DTM0 service automatically
 * starts-stops the machine.
 */
static bool is_manual_ss_enabled(void)
{
	return M0_FI_ENABLED("ut");
}

static int dtm0_service_start(struct m0_reqh_service *service)
{
	struct m0_dtm0_service *dtms = to_dtm(service);
	int                     rc;

        M0_PRE(service != NULL);
        rc = dtm_service__origin_fill(service);
	if (rc != 0)
		return M0_ERR(rc);

	if (!is_manual_ss_enabled()) {
		rc = m0_dtm0_recovery_machine_init(&dtms->dos_remach,
						   NULL, dtms);
		if (rc == 0)
			m0_dtm0_recovery_machine_start(&dtms->dos_remach);
	}

	return M0_RC(rc);
}

static void dtm0_service_prepare_to_stop(struct m0_reqh_service *reqh_rs)
{
	struct m0_dtm0_service *dtms;

	M0_PRE(reqh_rs != NULL);
	dtms = M0_AMB(dtms, reqh_rs, dos_generic);
	if (!is_manual_ss_enabled())
		m0_dtm0_recovery_machine_stop(&dtms->dos_remach);
	dtm0_service_conns_term(dtms);
}

static void dtm0_service_stop(struct m0_reqh_service *service)
{
	struct m0_dtm0_service *dtm0;

	M0_PRE(service != NULL);
	dtm0 = to_dtm(service);
	/*
	 * It is safe to remove any remaining entries from the log
	 * when a process with volatile log is going to die.
	 */
	if (dtm0->dos_origin == DTM0_ON_VOLATILE && dtm0->dos_log != NULL) {
		m0_be_dtm0_log_clear(dtm0->dos_log);
		m0_be_dtm0_log_fini(dtm0->dos_log);
		m0_be_dtm0_log_free0(&dtm0->dos_log);
	}

	if (!is_manual_ss_enabled())
		m0_dtm0_recovery_machine_fini(&dtm0->dos_remach);
}

static void dtm0_service_fini(struct m0_reqh_service *service)
{
	M0_PRE(service != NULL);
	dtm0_service__fini(to_dtm(service));
	m0_free(service);
}

M0_INTERNAL int m0_dtm0_stype_init(void)
{
	extern struct m0_sm_conf m0_dtx_sm_conf;

	return m0_sm_addb2_init(&m0_dtx_sm_conf,
				M0_AVI_DTX0_SM_STATE, M0_AVI_DTX0_SM_COUNTER) ?:
		m0_dtm0_fop_init() ?:
		m0_reqh_service_type_register(&dtm0_service_type) ?:
		m0_dtm0_rpc_link_mod_init() ?:
		m0_drm_domain_init();
}

M0_INTERNAL void m0_dtm0_stype_fini(void)
{
	extern struct m0_sm_conf m0_dtx_sm_conf;
	m0_drm_domain_fini();
	m0_dtm0_rpc_link_mod_fini();
	m0_reqh_service_type_unregister(&dtm0_service_type);
	m0_dtm0_fop_fini();
	m0_sm_addb2_fini(&m0_dtx_sm_conf);
}

M0_INTERNAL bool m0_dtm0_is_a_volatile_dtm(struct m0_reqh_service *service)
{
	return m0_streq(service->rs_type->rst_name, "M0_CST_DTM0") &&
		to_dtm(service)->dos_origin == DTM0_ON_VOLATILE;
}

M0_INTERNAL bool m0_dtm0_is_a_persistent_dtm(struct m0_reqh_service *service)
{
	return m0_streq(service->rs_type->rst_name, "M0_CST_DTM0") &&
		to_dtm(service)->dos_origin == DTM0_ON_PERSISTENT;
}

M0_INTERNAL struct m0_dtm0_service *
m0_dtm0_service_find(const struct m0_reqh *reqh)
{
	struct m0_reqh_service *rh_srv;

	rh_srv = m0_reqh_service_find(&dtm0_service_type, reqh);

	return rh_srv == NULL ? NULL : to_dtm(rh_srv);
}

M0_INTERNAL bool m0_dtm0_in_ut(void)
{
	return M0_FI_ENABLED("ut");
}


M0_INTERNAL int dtm0_process_init(struct dtm0_process    *proc,
				  struct m0_dtm0_service *dtms,
				  const struct m0_fid    *rem_svc_fid)
{
	struct m0_conf_process *rem_proc_conf;
	struct m0_conf_service *rem_svc_conf;
	struct m0_conf_obj     *obj;
	struct m0_reqh         *reqh;
	struct m0_conf_cache   *cache;

	M0_ENTRY("proc=%p, dtms=%p, rem=" FID_F, proc, dtms,
		 FID_P(rem_svc_fid));

	reqh = dtms->dos_generic.rs_reqh;
	cache = &m0_reqh2confc(reqh)->cc_cache;

	obj = m0_conf_cache_lookup(cache, rem_svc_fid);
	if (obj == NULL)
		return M0_ERR_INFO(-ENOENT,
				   "Cannot find svc" FID_F
				   " in the conf cache.", FID_P(rem_svc_fid));
	rem_svc_conf = M0_CONF_CAST(obj, m0_conf_service);
	obj = m0_conf_obj_grandparent(obj);
	M0_ASSERT_INFO(obj != NULL, "Service " FID_F " does not belong to "
		       "any process?", FID_P(rem_svc_fid));

	rem_proc_conf = M0_CONF_CAST(obj, m0_conf_process);
	if (rem_svc_conf->cs_type != M0_CST_DTM0)
		return M0_ERR_INFO(-ENOENT, "Not a DTM0 service.");

	dopr_tlink_init(proc);
	dopr_tlist_add(&dtms->dos_processes, proc);

	proc->dop_rproc_fid = rem_proc_conf->pc_obj.co_id;
	proc->dop_rserv_fid = rem_svc_conf->cs_obj.co_id;

	proc->dop_rep = m0_strdup(rem_proc_conf->pc_endpoint);

	m0_long_lock_init(&proc->dop_llock);

	return M0_RC(0);
}

M0_INTERNAL void dtm0_process_fini(struct dtm0_process *proc)
{
	dopr_tlink_fini(proc);
	m0_free(proc->dop_rep);
	m0_long_lock_fini(&proc->dop_llock);
}

M0_INTERNAL void dtm0_service_conns_term(struct m0_dtm0_service *service)
{
	struct dtm0_process *process;
	int                  rc;

	M0_ENTRY("dtms=%p", service);

	while ((process = dopr_tlist_pop(&service->dos_processes)) != NULL) {
		rc = dtm0_process_disconnect(process);
		M0_ASSERT_INFO(rc == 0, "Failed to disconnect from %p?",
			       process);
		dtm0_process_fini(process);
		m0_free(process);
	}

	M0_LEAVE();
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
