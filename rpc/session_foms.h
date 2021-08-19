/* -*- C -*- */
/*
 * Copyright (c) 2012-2020 Seagate Technology LLC and/or its Affiliates
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

#ifndef __MOTR_RPC_SESSION_FOM_H__
#define __MOTR_RPC_SESSION_FOM_H__

#include "fop/fop.h"
#include "rpc/session_fops.h"
#include "rpc/session_fops_xc.h"
#include "fop/fom.h"
#include "fop/fom_generic.h"     /* M0_FOPH_NR */

/**
   @addtogroup rpc_session

   @{

   This file contains, fom declarations for
   [conn|session]_[establish|terminate].
 */

/*
 * FOM to execute "RPC connection create" request
 */

enum m0_rpc_fom_conn_establish_phase {
	M0_FOPH_CONN_ESTABLISHING = M0_FOPH_NR + 1
};

extern struct m0_fom_type m0_rpc_fom_conn_establish_type;
extern const struct m0_fom_ops m0_rpc_fom_conn_establish_ops;

M0_INTERNAL size_t m0_rpc_session_default_home_locality(const struct m0_fom
							*fom);
M0_INTERNAL int m0_rpc_fom_conn_establish_tick(struct m0_fom *fom);
M0_INTERNAL void m0_rpc_fom_conn_establish_fini(struct m0_fom *fom);

/*
 * FOM to execute "Session Create" request
 */

enum m0_rpc_fom_session_establish_phase {
	M0_FOPH_SESSION_ESTABLISHING = M0_FOPH_NR + 1
};

extern struct m0_fom_type m0_rpc_fom_session_establish_type;
extern const struct m0_fom_ops m0_rpc_fom_session_establish_ops;

M0_INTERNAL int m0_rpc_fom_session_establish_tick(struct m0_fom *fom);
M0_INTERNAL void m0_rpc_fom_session_establish_fini(struct m0_fom *fom);

/*
 * FOM to execute session terminate request
 */

enum m0_rpc_fom_session_terminate_phase {
	M0_FOPH_SESSION_TERMINATING = M0_FOPH_NR + 1
};

extern struct m0_fom_type m0_rpc_fom_session_terminate_type;
extern const struct m0_fom_ops m0_rpc_fom_session_terminate_ops;

M0_INTERNAL int m0_rpc_fom_session_terminate_tick(struct m0_fom *fom);
M0_INTERNAL void m0_rpc_fom_session_terminate_fini(struct m0_fom *fom);

/*
 * FOM to execute RPC connection terminate request
 */

enum m0_rpc_fom_conn_terminate_phase {
	M0_FOPH_CONN_TERMINATING = M0_FOPH_NR + 1
};

extern struct m0_fom_type m0_rpc_fom_conn_terminate_type;
extern const struct m0_fom_ops m0_rpc_fom_conn_terminate_ops;

M0_INTERNAL int m0_rpc_fom_conn_terminate_tick(struct m0_fom *fom);
M0_INTERNAL void m0_rpc_fom_conn_terminate_fini(struct m0_fom *fom);


/*
 * Context fom used store fom data between its processing phases.
 */
struct m0_rpc_connection_session_specific_fom {
	/**
	   Genreric fom
	 */
	struct m0_fom ssf_fom_generic;

	/**
	   session pointer, during termination phase it has to be stored
	   between FOM phases
	 */
	struct m0_rpc_session *ssf_term_session;
};

#endif

/** @} end of rpc_session group */
/*
 *  Local variables:
 *  c-indentation-style: "K&R"
 *  c-basic-offset: 8
 *  tab-width: 8
 *  fill-column: 80
 *  scroll-step: 1
 *  End:
 */
