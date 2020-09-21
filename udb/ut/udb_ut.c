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


#include "lib/types.h"
#include "ut/ut.h"
#include "lib/misc.h" /* M0_SET0() */

#include "udb/udb.h"

static void cred_init(struct m0_udb_cred *cred,
		      enum m0_udb_cred_type type,
		      struct m0_udb_domain *dom)
{
	cred->uc_type = type;
	cred->uc_domain = dom;
}

static void cred_fini(struct m0_udb_cred *cred)
{
}

/* uncomment when realization ready */
#if 0
static bool cred_cmp(struct m0_udb_cred *left,
		     struct m0_udb_cred *right)
{
	return
		left->uc_type == right->uc_type &&
		left->uc_domain == right->uc_domain;
}
#endif

static void udb_test(void)
{
	int ret;
	struct m0_udb_domain dom;
	struct m0_udb_ctxt   ctx;
	struct m0_udb_cred   external;
	struct m0_udb_cred   internal;
	struct m0_udb_cred   testcred;

	cred_init(&external, M0_UDB_CRED_EXTERNAL, &dom);
	cred_init(&internal, M0_UDB_CRED_INTERNAL, &dom);

	ret = m0_udb_ctxt_init(&ctx);
	M0_UT_ASSERT(ret == 0);

	/* add mapping */
	ret = m0_udb_add(&ctx, &dom, &external, &internal);
	M0_UT_ASSERT(ret == 0);

	M0_SET0(&testcred);
	ret = m0_udb_e2i(&ctx, &external, &testcred);
	/* means that mapping exists */
	M0_UT_ASSERT(ret == 0);
/* uncomment when realization ready */
#if 0
	/* successfully mapped */
	M0_UT_ASSERT(cred_cmp(&internal, &testcred));
#endif
	M0_SET0(&testcred);
	ret = m0_udb_i2e(&ctx, &internal, &testcred);
	/* means that mapping exists */
	M0_UT_ASSERT(ret == 0);
/* uncomment when realization ready */
#if 0
	/* successfully mapped */
	M0_UT_ASSERT(cred_cmp(&external, &testcred));
#endif
	/* delete mapping */
	ret = m0_udb_del(&ctx, &dom, &external, &internal);
	M0_UT_ASSERT(ret == 0);

/* uncomment when realization ready */
#if 0
	/* check that mapping does not exist */
	M0_SET0(&testcred);
	ret = m0_udb_e2i(&ctx, &external, &testcred);
	M0_UT_ASSERT(ret != 0);

	/* check that mapping does not exist */
	M0_SET0(&testcred);
	ret = m0_udb_i2e(&ctx, &internal, &testcred);
	M0_UT_ASSERT(ret != 0);
#endif
	cred_fini(&internal);
	cred_fini(&external);
	m0_udb_ctxt_fini(&ctx);
}

struct m0_ut_suite udb_ut = {
        .ts_name = "udb-ut",
        .ts_init = NULL,
        .ts_fini = NULL,
        .ts_tests = {
                { "udb", udb_test },
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
