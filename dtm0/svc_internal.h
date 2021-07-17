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


#pragma once

#ifndef __MOTR_DTM0_SVC_INTERNAL_H__
#define __MOTR_DTM0_SVC_INTERNAL_H__

/* import */
#include "fop/fom_long_lock.h"       /* m0_long_lock */
#include "rpc/link.h"                /* m0_rpc_link */
struct m0_dtm0_service;

/* export */
struct dtm0_process;


/* Settings for RPC connections with DTM0 services. */
enum {
	DTM0_MAX_RPCS_IN_FLIGHT = 10,
	DTM0_DISCONNECT_TIMEOUT_SECS = 1,
};


/**
 * A sturcture to keep information about a remote DTM0 service.
 */
struct dtm0_process {
	/** Linkage to m0_dtm0_service::dos_processes. */
	struct m0_tlink         dop_link;
	uint64_t                dop_magic;

	/**
	 * Listens for an event on process conf object's HA channel.
	 * Updates dtm0_process status in the clink callback on HA notification.
	 */
	struct m0_clink         dop_ha_link;
	/**
	 * RPC link to be used to send messages to this process.
	 */
	struct m0_rpc_link      dop_rlink;
	/** Remote process fid. */
	struct m0_fid           dop_rproc_fid;
	/** Remote DTM0 service fid. */
	struct m0_fid           dop_rserv_fid;
	/** Remote process endpoint. */
	char                   *dop_rep;
	/**
	 * Protects dop_rlink from concurrent access from drlink FOMs.
	 * See ::m0_dtm0_req_post.
	 */
	struct m0_long_lock     dop_llock;
};

M0_INTERNAL int dtm0_process_init(struct dtm0_process    *proc,
				  struct m0_dtm0_service *dtms,
				  const struct m0_fid    *rem_svc_fid);

M0_INTERNAL void dtm0_process_fini(struct dtm0_process *proc);

/** Terminates all the connections established from a local service. */
M0_INTERNAL void dtm0_service_conns_term(struct m0_dtm0_service *service);
M0_INTERNAL struct dtm0_process *
dtm0_service_process__lookup(struct m0_reqh_service *reqh_dtm0_svc,
			     const struct m0_fid    *remote_dtm0);

#endif /* __MOTR_DTM0_SVC_INTERNAL_H__ */

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
