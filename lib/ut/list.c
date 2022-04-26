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
#include "lib/list.h"

struct test1 {
	struct m0_list_link	t_link;
	int	c;
};

void test_list(void)
{
	struct test1	t1 = {}, t2 = {}, t3 = {};
	struct m0_list_link	*pos;
	struct m0_list	test_head;
	struct test1 *p;
	int t_sum;
	int count;

	m0_list_init(&test_head);

	m0_list_link_init(&t1.t_link);
	t1.c = 5;
	m0_list_link_init(&t2.t_link);
	t2.c = 10;
	m0_list_link_init(&t3.t_link);
	t3.c = 15;

	M0_UT_ASSERT(!m0_list_contains(&test_head, &t1.t_link));
	M0_UT_ASSERT(!m0_list_contains(&test_head, &t2.t_link));
	M0_UT_ASSERT(!m0_list_contains(&test_head, &t3.t_link));

	m0_list_add(&test_head, &t1.t_link);
	m0_list_add_tail(&test_head, &t2.t_link);
	m0_list_add(&test_head, &t3.t_link);

	M0_UT_ASSERT(m0_list_contains(&test_head, &t1.t_link));
	M0_UT_ASSERT(m0_list_contains(&test_head, &t2.t_link));
	M0_UT_ASSERT(m0_list_contains(&test_head, &t3.t_link));

	count = 0;
	M0_UT_ASSERT(m0_list_forall(e, &test_head,
				    count++;
				    m0_list_entry(e, struct test1,
						  t_link)->c > 4));
	M0_UT_ASSERT(count == 3);

	count = 0;
	M0_UT_ASSERT(m0_list_entry_forall(e, &test_head, struct test1, t_link,
					  count++; e->c % 5 == 0));
	M0_UT_ASSERT(count == 3);

	t_sum = 0;
	count = 0;
	M0_UT_ASSERT(m0_list_entry_forall(e, &test_head, struct test1, t_link,
					  count++; t_sum += e->c; e->c < 16));
	M0_UT_ASSERT(t_sum == 30);
	M0_UT_ASSERT(count == 3);

	count = 0;
	m0_list_for_each(&test_head, pos) {
		p = m0_list_entry(pos,struct test1, t_link);
		M0_UT_ASSERT(p->c % 5 == 0);
		count++;
	}
	M0_UT_ASSERT(count == 3);

	m0_list_del(&t2.t_link);

	M0_UT_ASSERT( m0_list_contains(&test_head, &t1.t_link));
	M0_UT_ASSERT(!m0_list_contains(&test_head, &t2.t_link));
	M0_UT_ASSERT( m0_list_contains(&test_head, &t3.t_link));

	count = 0;
	t_sum = 0;
	m0_list_for_each(&test_head, pos) {
		p = m0_list_entry(pos,struct test1, t_link);
		t_sum += p->c;
		count++;
	}
	M0_UT_ASSERT(t_sum == 20);
	M0_UT_ASSERT(count == 2);

	m0_list_del(&t1.t_link);

	M0_UT_ASSERT(!m0_list_contains(&test_head, &t1.t_link));
	M0_UT_ASSERT(!m0_list_contains(&test_head, &t2.t_link));
	M0_UT_ASSERT( m0_list_contains(&test_head, &t3.t_link));

	t_sum = 0;
	count = 0;
	m0_list_for_each_entry(&test_head, p, struct test1, t_link) {
		t_sum += p->c;
		count++;
	}
	M0_UT_ASSERT(t_sum == 15);
	M0_UT_ASSERT(count == 1);

	m0_list_del(&t3.t_link);

	M0_UT_ASSERT(!m0_list_contains(&test_head, &t1.t_link));
	M0_UT_ASSERT(!m0_list_contains(&test_head, &t2.t_link));
	M0_UT_ASSERT(!m0_list_contains(&test_head, &t3.t_link));

	t_sum = 0;
	count = 0;
	m0_list_for_each_entry(&test_head, p, struct test1, t_link) {
		t_sum += p->c;
		count++;
	}
	M0_UT_ASSERT(t_sum == 0);
	M0_UT_ASSERT(count == 0);

	M0_UT_ASSERT(m0_list_is_empty(&test_head));
	m0_list_fini(&test_head);
}

enum {
	UB_ITER = 100000
};

static struct test1 t[UB_ITER];
static struct m0_list list;

static int ub_init(const char *opts M0_UNUSED)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(t); ++i)
		m0_list_link_init(&t[i].t_link);
	m0_list_init(&list);
	return 0;
}

static void ub_fini(void)
{
	int i;

	m0_list_fini(&list);
	for (i = 0; i < ARRAY_SIZE(t); ++i)
		m0_list_link_fini(&t[i].t_link);
}

static void ub_insert(int i)
{
	m0_list_add(&list, &t[i].t_link);
}

static void ub_delete(int i)
{
	m0_list_del(&t[i].t_link);
}

struct m0_ub_set m0_list_ub = {
	.us_name = "list-ub",
	.us_init = ub_init,
	.us_fini = ub_fini,
	.us_run  = {
		{ .ub_name = "insert",
		  .ub_iter = UB_ITER,
		  .ub_round = ub_insert },

		{ .ub_name = "delete",
		  .ub_iter = UB_ITER,
		  .ub_round = ub_delete },

		{ .ub_name = NULL }
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
