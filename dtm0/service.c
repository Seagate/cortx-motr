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

#define M0_TRACE_SUBSYSTEM M0_TRACE_SUBSYS_DTM0
#include "lib/trace.h"
#include "lib/string.h"
#include "lib/errno.h"
#include "lib/memory.h"
#include "reqh/reqh_service.h"
#include "reqh/reqh.h"               /* m0_reqh */
#include "rpc/rpc_machine.h"         /* m0_rpc_machine */
#include "dtm0/fop.h"
#include "lib/tlist.h"


static int dtm0_service_start(struct m0_reqh_service *service);
static void dtm0_service_stop(struct m0_reqh_service *service);
static int dtm0_service_allocate(struct m0_reqh_service **service,
				 const struct m0_reqh_service_type *stype);
static void dtm0_service_fini(struct m0_reqh_service *service);


static const struct m0_reqh_service_type_ops dtm0_service_type_ops = {
	.rsto_service_allocate = dtm0_service_allocate
};

static const struct m0_reqh_service_ops dtm0_service_ops = {
	.rso_start = dtm0_service_start,
	.rso_stop  = dtm0_service_stop,
	.rso_fini  = dtm0_service_fini
};

struct m0_reqh_service_type dtm0_service_type = {
	.rst_name  = "M0_CST_DTM0",
	.rst_ops   = &dtm0_service_type_ops,
	.rst_level = M0_RS_LEVEL_LATE,
};


/**
 * System process which dtm0 subscribes for state updates
 */
struct dtm0_process {
	struct m0_tlink     dop_link;
	uint64_t            dop_magic;

	/**
	 * Listens for an event on process conf object's HA channel.
	 * Updates dtm0_process status in the clink callback on HA notification.
	 */
	struct m0_clink     dop_ha_link;
	/**
	 * Link connected to remote process
	 */
	struct m0_rpc_link  dop_rlink;
	/**
	 * Remote process fid
	 */
	struct m0_fid       dop_rfid;
	/**
	 * Remote process endpoint
	 */
	const char         *dop_rep;
};

M0_TL_DESCR_DEFINE(dopr, "dtm0_process", static, struct dtm0_process, dop_link,
		   dop_magic, 0x8888888888888888, 0x7777777777777777);
M0_TL_DEFINE(dopr, static, struct dtm0_process);

/**
 * DTM0 service structure
 */
struct m0_dtm0_service {
       struct m0_reqh_service dos_generic;
       struct m0_tl           dos_processes;
};

static void dtm0_service__init(struct m0_dtm0_service *s)
{
       dopr_tlist_init(&s->dos_processes);
}

static void dtm0_service__fini(struct m0_dtm0_service *s)
{
       dopr_tlist_fini(&s->dos_processes);
}

M0_INTERNAL int m0_dtm0_service_process_connect(struct m0_reqh_service *s,
						struct m0_fid *remote_srv,
						const char    *remote_ep)
{
	struct m0_dtm0_service *service =
		container_of(s, struct m0_dtm0_service, dos_generic);
	struct dtm0_process *process;
	struct m0_rpc_machine *mach =
		m0_reqh_rpc_mach_tlist_head(&s->rs_reqh->rh_rpc_machines);
	int rc;


	M0_ALLOC_PTR(process);
	M0_ASSERT(process != NULL);

	rc = m0_rpc_link_init(&process->dop_rlink, mach, remote_srv,
			      remote_ep, 10);
	M0_ASSERT(rc == 0);
	rc = m0_rpc_link_connect_sync(&process->dop_rlink, M0_TIME_NEVER);
	M0_ASSERT(rc == 0);

	process->dop_rfid = *remote_srv;
	process->dop_rep  = m0_strdup(remote_ep);

	dopr_tlink_init(process);
	dopr_tlist_add(&service->dos_processes, process);

	return M0_RC(0);
}

M0_INTERNAL int m0_dtm0_service_process_disconnect(struct m0_reqh_service *s,
						   struct m0_fid *remote_srv)
{
	int rc;
	struct m0_dtm0_service *service =
		container_of(s, struct m0_dtm0_service, dos_generic);
	struct dtm0_process *process = NULL;

	m0_tl_for(dopr, &service->dos_processes, process) {
		if (m0_fid_eq(&process->dop_rfid, remote_srv))
			break;
	} m0_tl_endfor;

	if (process == NULL)
		return -ENOENT;

	rc = m0_rpc_link_disconnect_sync(&process->dop_rlink, M0_TIME_NEVER);
	M0_ASSERT(rc == 0);
	m0_rpc_link_fini(&process->dop_rlink);

	dopr_tlist_remove(process);
	dopr_tlink_fini(process);

	return M0_RC(0);
}

M0_INTERNAL struct m0_rpc_session *
m0_dtm0_service_process_session_get(struct m0_reqh_service *s,
				    struct m0_fid *remote_srv)
{
	struct m0_dtm0_service *service =
		container_of(s, struct m0_dtm0_service, dos_generic);
	struct dtm0_process *process = NULL;

	m0_tl_for(dopr, &service->dos_processes, process) {
		if (m0_fid_eq(&process->dop_rfid, remote_srv))
			break;
	} m0_tl_endfor;

	return process == NULL ? NULL : &process->dop_rlink.rlk_sess;
}

static int dtm0_service__alloc(struct m0_reqh_service **service,
			       const struct m0_reqh_service_type *stype,
			       const struct m0_reqh_service_ops *ops)
{
	struct m0_dtm0_service *s;

	M0_PRE(stype != NULL && service != NULL && ops != NULL);

	M0_ALLOC_PTR(s);
	if (s == NULL)
		return -ENOMEM;

	s->dos_generic.rs_type = stype;
	s->dos_generic.rs_ops  = ops;
	*service = &s->dos_generic;
	dtm0_service__init(s);

	return 0;
}

static int dtm0_service_allocate(struct m0_reqh_service **service,
				 const struct m0_reqh_service_type *stype)
{
	return dtm0_service__alloc(service, stype, &dtm0_service_ops);
}

static int dtm0_service_start(struct m0_reqh_service *service)
{
        M0_PRE(service != NULL);
        return m0_dtm0_fop_init();
}

static void dtm0_service_stop(struct m0_reqh_service *service)
{

        M0_PRE(service != NULL);
	m0_dtm0_fop_fini();
}

static void dtm0_service_fini(struct m0_reqh_service *service)
{
	M0_PRE(service != NULL);
	dtm0_service__fini(container_of(service,
					struct m0_dtm0_service,
					dos_generic));
        m0_free(service);
}

int m0_dtm0_stype_init(void)
{
	return m0_reqh_service_type_register(&dtm0_service_type);
}

void m0_dtm0_stype_fini(void)
{
	m0_reqh_service_type_unregister(&dtm0_service_type);
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
