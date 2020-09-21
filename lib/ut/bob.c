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


#include "lib/ub.h"
#include "ut/ut.h"
#include "lib/tlist.h"
#include "lib/bob.h"

enum {
	N = 256
};

struct foo {
	void           *f_payload;
	struct m0_tlink f_linkage;
	char            f_x[7];
	uint64_t        f_magix;
};

enum {
	magix = 0xbeda551edcaca0ffULL
};

M0_TL_DESCR_DEFINE(foo, "foo-s", static, struct foo, f_linkage,
		   f_magix, magix, 0);
M0_TL_DEFINE(foo, static, struct foo);

static struct m0_bob_type foo_bob;
static struct foo F;
static struct foo rank[N];

M0_BOB_DEFINE(static, &foo_bob, foo);

static void test_tlist_init(void)
{
	M0_SET0(&foo_bob);
	m0_bob_type_tlist_init(&foo_bob, &foo_tl);
	M0_UT_ASSERT(!strcmp(foo_bob.bt_name, foo_tl.td_name));
	M0_UT_ASSERT(foo_bob.bt_magix == magix);
	M0_UT_ASSERT(foo_bob.bt_magix_offset == foo_tl.td_link_magic_offset);
}

static void test_bob_init(void)
{
	foo_bob_init(&F);
	M0_UT_ASSERT(F.f_magix == magix);
	M0_UT_ASSERT(foo_bob_check(&F));
}

static void test_bob_fini(void)
{
	foo_bob_fini(&F);
	M0_UT_ASSERT(F.f_magix == 0);
	M0_UT_ASSERT(!foo_bob_check(&F));
}

static void test_tlink_init(void)
{
	foo_tlink_init(&F);
	M0_UT_ASSERT(foo_bob_check(&F));
}

static void test_tlink_fini(void)
{
	foo_tlink_fini(&F);
	M0_UT_ASSERT(foo_bob_check(&F));
	F.f_magix = 0;
	M0_UT_ASSERT(!foo_bob_check(&F));
}

static bool foo_check(const void *bob)
{
	const struct foo *f = bob;

	return f->f_payload == f + 1;
}

static void test_check(void)
{
	int i;

	foo_bob.bt_check = &foo_check;

	for (i = 0; i < N; ++i) {
		foo_bob_init(&rank[i]);
		rank[i].f_payload = rank + i + 1;
	}

	for (i = 0; i < N; ++i)
		M0_UT_ASSERT(foo_bob_check(&rank[i]));

	for (i = 0; i < N; ++i)
		foo_bob_fini(&rank[i]);

	for (i = 0; i < N; ++i)
		M0_UT_ASSERT(!foo_bob_check(&rank[i]));

}

static void test_bob_of(void)
{
	void *p;
	int   i;

	foo_bob_init(&F);
	for (i = -1; i < ARRAY_SIZE(F.f_x) + 3; ++i) {
		p = &F.f_x[i];
		M0_UT_ASSERT(bob_of(p, struct foo, f_x[i], &foo_bob) == &F);
	}
}

void test_bob(void)
{
	test_tlist_init();
	test_bob_init();
	test_bob_fini();
	test_tlink_init();
	test_tlink_fini();
	test_check();
	test_bob_of();
	/*
	 * Some of the above tests make an unsuccessful m0_bob_check(),
	 * setting m0_failed_condition. Don't let m0_panic() use it.
	 */
	m0_failed_condition = NULL;
}
M0_EXPORTED(test_bob);

/*
 *  Local variables:
 *  c-indentation-style: "K&R"
 *  c-basic-offset: 8
 *  tab-width: 8
 *  fill-column: 80
 *  scroll-step: 1
 *  End:
 */
