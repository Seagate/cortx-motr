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


#include "lib/memory.h"
#include "lib/misc.h"
#include "ut/ut.h"

#include "fop/fop.h"
#include "fop/fom_generic.h"
#include "reqh/reqh.h"
#include "reqh/reqh_service.h"
#include "reqh/ut/service.h"
#include "reqh/ut/service_xc.h"
#include "rpc/rpc_opcodes.h"
#include "ut/ut.h"
#include "ut/cs_service.h"
#include "ut/cs_fop.h"
#include "ut/ut_rpc_machine.h"

#include "reqh/ut/reqhut_fom.c"

#define DUMMY_DBNAME      "dummy-db"
#define DUMMY_COB_ID      20
#define DUMMY_SERVER_ADDR M0_UT_DUMMY_EP_ADDR

static struct m0_ut_rpc_mach_ctx rmach_ctx;
static struct m0_fop_type m0_reqhut_dummy_fopt;

enum {
	MAX_REQH_UT_FOP = 25
};

static int m0_reqhut_fop_init(void)
{
	m0_xc_reqh_ut_service_init();
	m0_reqhut_dummy_xc->xct_flags = M0_XCODE_TYPE_FLAG_DOM_RPC;
	M0_FOP_TYPE_INIT(&m0_reqhut_dummy_fopt,
			 .name      = "Reqh unit test",
			 .opcode    = M0_REQH_UT_DUMMY_OPCODE,
			 .xt        = m0_reqhut_dummy_xc,
			 .fom_ops   = &reqhut_fom_type_ops,
			 .sm        = &m0_generic_conf,
			 .rpc_flags = M0_RPC_ITEM_TYPE_REQUEST,
			 .svc_type  = &ds1_service_type);
	return 0;
}

static void m0_reqhut_fop_fini(void)
{
	m0_fop_type_fini(&m0_reqhut_dummy_fopt);
	m0_xc_reqh_ut_service_fini();
}

static void test_service(void)
{
	int                          i;
	int                          rc;
	struct m0_reqh              *reqh;
	struct m0_reqh_service_type *svct;
	struct m0_reqh_service      *reqh_svc;
	struct m0_fop               *fop;

	m0_semaphore_init(&sem, 0);
	rc = m0_reqhut_fop_init();
	M0_UT_ASSERT(rc == 0);

	/* Workaround */
	M0_SET0(&rmach_ctx);
	rmach_ctx.rmc_cob_id.id = DUMMY_COB_ID;
	rmach_ctx.rmc_ep_addr   = DUMMY_SERVER_ADDR;
	m0_ut_rpc_mach_init_and_add(&rmach_ctx);

	reqh = &rmach_ctx.rmc_reqh;
	svct = m0_reqh_service_type_find("M0_CST_DS1");
	M0_UT_ASSERT(svct != NULL);

	rc = m0_reqh_service_allocate(&reqh_svc, svct, NULL);
	M0_UT_ASSERT(rc == 0);

	m0_reqh_service_init(reqh_svc, reqh, NULL);

	rc = m0_reqh_service_start(reqh_svc);
	M0_UT_ASSERT(rc == 0);

	fop = m0_fop_alloc(&m0_reqhut_dummy_fopt, NULL, &rmach_ctx.rmc_rpc);
	M0_UT_ASSERT(fop != NULL);

	for (i = 0; i < MAX_REQH_UT_FOP; ++i) {
		m0_reqh_fop_handle(reqh, fop);
		m0_semaphore_down(&sem);
	}
	m0_reqh_idle_wait(reqh);

	m0_fop_put_lock(fop);
	m0_ut_rpc_mach_fini(&rmach_ctx);
	m0_reqhut_fop_fini();
}

struct m0_ut_suite reqh_service_ut = {
	.ts_name = "reqh-service-ut",
	.ts_init = NULL,
	.ts_fini = NULL,
	.ts_tests = {
		{ "reqh service", test_service },
		{ NULL, NULL }
	}
};
M0_EXPORTED(reqh_service_ut);

/*
 *  Local variables:
 *  c-indentation-style: "K&R"
 *  c-basic-offset: 8
 *  tab-width: 8
 *  fill-column: 80
 *  scroll-step: 1
 *  End:
 */
