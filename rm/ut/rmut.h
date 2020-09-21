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
#ifndef __MOTR_RM_UT_RMUT_H__
#define __MOTR_RM_UT_RMUT_H__

#include "lib/types.h"            /* uint64_t */
#include "lib/chan.h"
#include "lib/misc.h"
#include "lib/memory.h"
#include "lib/time.h"
#include "ut/ut.h"
#include "fop/fom_generic.h"
#include "ut/ut_rpc_machine.h"

enum obj_type {
	OBJ_DOMAIN = 1,
	OBJ_RES_TYPE,
	OBJ_RES,
	OBJ_OWNER
};

/*
 * If you add another server, you will have to make changes in other places.
 */
enum rm_server {
	SERVER_1 = 0,
	SERVER_2,
	SERVER_3,
	SERVER_4,
	SERVER_NR,
	SERVER_INVALID,
};

enum rr_tests {
	TEST1 = 0,
	TEST2,
	TEST3,
	TEST4,
	TEST_NR,
};

enum flock_tests {
	/* Test the locking on a lock client */
	LOCK_ON_CLIENT_TEST = 0,
	/* Test the locking on a lock server */
	LOCK_ON_SERVER_TEST,
	/* Test the distributed locking */
	DISTRIBUTED_LOCK_TEST,
	LOCK_TESTS_NR,
};

enum group_tests {
	/* A group of debtors borrowing the same credit */
	GROUP_BORROW_TEST1 = 0,
	GROUP_BORROW_TEST2,
	/*
	 * A stand-alone debtor borrowing a credit other than the group credit.
	 */
	STAND_ALONE_BORROW_TEST,
	/* Revoking a credit from a group of debtors */
	GROUP_REVOKE_TEST,
	GROUP_TESTS_NR,
};

enum rm_ut_credits_list {
	RCL_BORROWED,
	RCL_SUBLET,
	RCL_HELD,
	RCL_CACHED
};

/* Forward declaration */
struct rm_ut_data;

struct rm_ut_data_ops {
	void (*rtype_set)(struct rm_ut_data *self);
	void (*rtype_unset)(struct rm_ut_data *self);
	void (*resource_set)(struct rm_ut_data *self);
	void (*resource_unset)(struct rm_ut_data *self);
	void (*owner_set)(struct rm_ut_data *self);
	void (*owner_unset)(struct rm_ut_data *self);
	void (*credit_datum_set)(struct rm_ut_data *self);
};

/*
 * Resource manager class-collection.
 */
struct rm_ut_data {
	struct m0_rm_domain          rd_dom;
	struct m0_rm_resource_type  *rd_rt;
	struct m0_rm_resource       *rd_res;
	struct m0_rm_owner          *rd_owner;
	struct m0_rm_incoming        rd_in;
	struct m0_rm_credit          rd_credit;
	const struct rm_ut_data_ops *rd_ops;
	struct m0_fid                rd_fid;
};


/*
 * RM server context. It lives inside a thread in this test.
 */
struct rm_ctx {
	enum rm_server             rc_id;
	struct m0_thread           rc_thr;
	struct m0_chan             rc_chan;
	struct m0_clink            rc_clink;
	struct m0_mutex            rc_mutex;
	struct m0_ut_rpc_mach_ctx  rc_rmach_ctx;
	struct m0_reqh_service    *rc_reqh_svc;
	struct m0_rpc_conn         rc_conn[SERVER_NR];
	struct m0_rpc_session      rc_sess[SERVER_NR];
	struct rm_ut_data          rc_test_data;
	uint32_t                   rc_debtors_nr;
	bool                       rc_is_dead;
	enum rm_server             creditor_id;
	enum rm_server             debtor_id[SERVER_NR - 1];
};

/*
 * Test variable(s)
 */
extern struct rm_ut_data     rm_test_data;
M0_EXTERN struct rm_ctx      rm_ctxs[];
M0_EXTERN const char        *serv_addr[];
M0_EXTERN const int          cob_ids[];
M0_EXTERN struct m0_chan     rm_ut_tests_chan;
M0_EXTERN struct m0_mutex    rm_ut_tests_chan_mutex;

void rm_utdata_init(struct rm_ut_data *data, enum obj_type type);
void rm_utdata_fini(struct rm_ut_data *data, enum obj_type type);
void rm_utdata_owner_windup_fini(struct rm_ut_data *data);
void rm_test_owner_capital_raise(struct m0_rm_owner *owner,
				 struct m0_rm_credit *credit);

/* Test server functions */
void rm_ctx_init(struct rm_ctx *rmctx, enum rm_server id);
void rm_ctx_fini(struct rm_ctx *rmctx);
void rm_ctx_connect(struct rm_ctx *src, const struct rm_ctx *dest);
void rm_ctx_disconnect(struct rm_ctx *src, const struct rm_ctx *dest);
void rm_ctx_server_start(enum rm_server srv_id);
void rm_ctx_server_windup(enum rm_server srv_id);
void rm_ctx_server_owner_windup(enum rm_server srv_id);
void rm_ctx_server_stop(enum rm_server srv_id);
void creditor_cookie_setup(enum rm_server dsrv_id, enum rm_server csrv_id);
void loan_session_set(enum rm_server csrv_id, enum rm_server dsrv_id);
void credits_are_equal(enum rm_server          srv_id,
		       enum rm_ut_credits_list list_id,
		       uint64_t                value);
void rm_ctxs_conf_init(struct rm_ctx *rm_ctxs, int ctxs_nr);
void rm_ctxs_conf_fini(struct rm_ctx *rm_ctxs, int ctxs_nr);


/* __MOTR_RM_UT_RMUT_H__ */
#endif

/*
 *  Local variables:
 *  c-indentation-style: "K&R"
 *  c-basic-offset: 8
 *  tab-width: 8
 *  fill-column: 80
 *  scroll-step: 1
 *  End:
 */
