/* -*- C -*- */
/*
 * Copyright (c) 2017-2020 Seagate Technology LLC and/or its Affiliates
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


#define M0_TRACE_SUBSYSTEM M0_TRACE_SUBSYS_UT
#include "lib/trace.h"
#include "lib/coroutine.h"
#include "lib/string.h"
#include "ut/ut.h"

#define F M0_CO_FRAME_DATA

static void foo0(struct m0_co_context *context, int nr, int *ret);
static void foo1(struct m0_co_context *context, int *ret);
static void foo2(struct m0_co_context *context, int *ret);


static void foo0(struct m0_co_context *context, int nr, int *ret)
{
	static const char *foo0_str = "FooBarBuzz";
	M0_CO_REENTER(context,
		      int   nr;
		      int   a;
		      char  b[100];
		      int   rc;
		      int   rc1;
		      int   i;
		      int   sum;
		);
	F(sum) = 0;
	F(rc) = 101;
	F(a) = 0x200;
	F(nr) = nr;
	strncpy(F(b), foo0_str, ARRAY_SIZE(F(b)));

	M0_CO_FUN(context, foo1(context, &F(rc1)));

	M0_UT_ASSERT(F(rc) == 101);
	M0_UT_ASSERT(F(rc1) == 302);
	M0_UT_ASSERT(F(a) == 0x200);
	M0_UT_ASSERT(m0_streq(F(b), foo0_str));

	M0_CO_YIELD(context);

	M0_UT_ASSERT(F(rc) == 101);
	M0_UT_ASSERT(F(rc1) == 302);
	M0_UT_ASSERT(F(a) == 0x200);
	M0_UT_ASSERT(m0_streq(F(b), foo0_str));

	for (F(i) = 0; F(i) < F(nr); F(i)++) {
		M0_CO_FUN(context, foo1(context, &F(rc1)));
		F(sum) += F(rc1);
	}

	M0_UT_ASSERT(F(sum) == 302*F(nr));

	*ret = 301;
}

static void foo1(struct m0_co_context *context, int *ret)
{
	M0_CO_REENTER(context,
		      int d;
		      int rc;
		      int rc1;
		);

	F(rc) = 102;
	F(d) = 0x100;

	M0_CO_FUN(context, foo2(context, &F(rc1)));

	M0_UT_ASSERT(F(rc) == 102);
	M0_UT_ASSERT(F(d) == 0x100);
	M0_UT_ASSERT(F(rc1) == 303);

	M0_CO_FUN(context, foo2(context, &F(rc1)));

	M0_UT_ASSERT(F(rc) == 102);
	M0_UT_ASSERT(F(rc1) == 303);
	M0_UT_ASSERT(F(d) == 0x100);

	*ret = 302;
}

static void foo2(struct m0_co_context *context, int *ret)
{
	M0_CO_REENTER(context,
		      char *c;
		      int   rc;
		);

	F(rc) = 103;

	M0_CO_YIELD(context);

	M0_UT_ASSERT(F(rc) == 103);
	F(rc) = 303;

	*ret = F(rc);
}

int m0_test_coroutine(void)
{
	struct m0_co_context context[10] = {};
	int i;
	int rc;
	int ret;
	int rcx[10];

	for (i = 0; i < ARRAY_SIZE(context); ++i) {
		rc = m0_co_context_init(&context[i]);
		M0_UT_ASSERT(rc == 0);
		rcx[i] = -EAGAIN;
	}

	rc = -EAGAIN;
	while (rc == -EAGAIN) {
		rc = 0;
		for (i = 0; i < ARRAY_SIZE(context); ++i) {
			if (rcx[i] == -EAGAIN) {
				M0_CO_START(&context[i]);
				foo0(&context[i], i+10, &ret);
				rcx[i] = M0_CO_END(&context[i]);
			}

			if (rcx[i] == -EAGAIN)
				rc = -EAGAIN;
		}
	}

	for (i = 0; i < ARRAY_SIZE(context); ++i)
		m0_co_context_fini(&context[i]);

	return 0;
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
/*
 * vim: tabstop=8 shiftwidth=8 noexpandtab textwidth=80 nowrap
 */
