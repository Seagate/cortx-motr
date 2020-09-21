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


#include "lib/types.h"
#include "ut/ut.h"
#include "lib/misc.h" /* M0_SET0() */

#include "capa/capa.h"

/* stub on stub :-) */
struct capa_object_guard {
};

static int cog_init(struct capa_object_guard *cog)
{
	return 0;
}

static void cog_fini(struct capa_object_guard *cog)
{
}

/**
   @todo struct m0_capa_issuer is empty, put proper values.
 */

static void capa_test(void)
{
	int                      ret;
	struct m0_capa_ctxt      ctx;
	struct m0_object_capa    read_capa;
	struct m0_object_capa    write_capa;
	struct capa_object_guard guard;
	struct m0_capa_issuer    issuer;

	ret = cog_init(&guard);
	M0_UT_ASSERT(ret == 0);

	ret = m0_capa_ctxt_init(&ctx);
	M0_UT_ASSERT(ret == 0);

	ret = m0_capa_new(&read_capa,
			  M0_CAPA_ENTITY_OBJECT,
			  M0_CAPA_OP_DATA_READ,
			  &guard);
	M0_UT_ASSERT(ret == 0);

	ret = m0_capa_new(&write_capa,
			  M0_CAPA_ENTITY_OBJECT,
			  M0_CAPA_OP_DATA_WRITE,
			  &guard);
	M0_UT_ASSERT(ret == 0);

	ret = m0_capa_get(&ctx, &issuer, &read_capa);
	M0_UT_ASSERT(ret == 0);

	ret = m0_capa_get(&ctx, &issuer, &write_capa);
	M0_UT_ASSERT(ret == 0);

	/* have capability so auth should succeed */
	ret = m0_capa_auth(&ctx, &write_capa, M0_CAPA_OP_DATA_WRITE);
	M0_UT_ASSERT(ret == 0);

	ret = m0_capa_auth(&ctx, &write_capa, M0_CAPA_OP_DATA_READ);
	M0_UT_ASSERT(ret == 0);

	m0_capa_put(&ctx, &read_capa);
	m0_capa_put(&ctx, &write_capa);

/* uncomment when realization ready */
#if 0
	/* have NO capability so auth should fail */
	ret = m0_capa_auth(&ctx, &write_capa, M0_CAPA_OP_DATA_WRITE);
	M0_UT_ASSERT(ret != 0);

	ret = m0_capa_auth(&ctx, &write_capa, M0_CAPA_OP_DATA_READ);
	M0_UT_ASSERT(ret != 0);
#endif

	cog_fini(&guard);
	m0_capa_ctxt_fini(&ctx);
}

struct m0_ut_suite capa_ut = {
        .ts_name = "capa-ut",
        .ts_init = NULL,
        .ts_fini = NULL,
        .ts_tests = {
                { "capa", capa_test },
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
