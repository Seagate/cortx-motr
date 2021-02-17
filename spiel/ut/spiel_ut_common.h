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

#pragma once

#ifndef __MOTR_SPIEL_UT_SPIEL_UT_COMMON_H__
#define __MOTR_SPIEL_UT_SPIEL_UT_COMMON_H__

#include "net/net.h"          /* m0_net_domain */
#include "net/buffer_pool.h"  /* m0_net_buffer_pool */
#include "reqh/reqh.h"        /* m0_reqh */
#include "rpc/rpc_machine.h"  /* m0_rpc_machine */
#include "rpc/rpclib.h"       /* m0_rpc_server_ctx */
#include "rm/rm_service.h"    /* m0_rms_type */

#define SERVER_ENDPOINT_ADDR "0@lo:12345:34:1"
#ifdef ENABLE_LIBFAB
#define SERVER_ENDPOINT      "libfab:" SERVER_ENDPOINT_ADDR
#else
#define SERVER_ENDPOINT      "lnet:" SERVER_ENDPOINT_ADDR
#endif
#define CLIENT_ENDPOINT_ADDR "0@lo:12345:34:*"

struct m0_spiel;

extern const char *confd_addr[];
extern const char *rm_addr;

/**
 * Request handler context with all necessary structures.
 *
 * Field sur_reqh can be passed to m0_spiel_start() function
 */
struct m0_spiel_ut_reqh {
	struct m0_net_domain      sur_net_dom;
	struct m0_net_buffer_pool sur_buf_pool;
	struct m0_reqh            sur_reqh;
	struct m0_rpc_machine     sur_rmachine;
	struct m0_rpc_server_ctx  sur_confd_srv;
};

M0_INTERNAL int m0_spiel__ut_reqh_init(struct m0_spiel_ut_reqh *spl_reqh,
				       const char              *ep_addr);

M0_INTERNAL void m0_spiel__ut_reqh_fini(struct m0_spiel_ut_reqh *spl_reqh);

M0_INTERNAL int m0_spiel__ut_rpc_server_start(struct m0_rpc_server_ctx *rpc_srv,
					const char               *confd_ep,
					const char               *confdb_path);

M0_INTERNAL void m0_spiel__ut_rpc_server_stop(
					struct m0_rpc_server_ctx *rpc_srv);

M0_INTERNAL void m0_spiel__ut_init(struct m0_spiel *spiel,
				   const char      *confd_path,
				   bool             cmd_iface);

M0_INTERNAL void m0_spiel__ut_fini(struct m0_spiel *spiel,
				   bool             cmd_iface);

#endif /* __MOTR_SPIEL_UT_SPIEL_UT_COMMON_H__ */
/*
 *  Local variables:
 *  c-indentation-style: "K&R"
 *  c-basic-offset: 8
 *  tab-width: 8
 *  fill-column: 80
 *  scroll-step: 1
 *  End:
 */
