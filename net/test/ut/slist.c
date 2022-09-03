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


#include "ut/ut.h"		/* M0_UT_ASSERT */
#include "lib/types.h"		/* m0_bcount_t */
#include "lib/vec.h"		/* m0_bufvec */

#include "net/test/slist.h"

enum {
	NET_TEST_SLIST_NR     = 64,
	NET_TEST_SLIST_BV_LEN = NET_TEST_SLIST_NR * 3 + 0x100,
};

static void slist_check(struct m0_net_test_slist *slist, int nr, char *buf)
{
	int i;
	int rc;

	/* check number of string in string list */
	M0_UT_ASSERT(slist->ntsl_nr == nr + 1);
	M0_UT_ASSERT(slist->ntsl_list != NULL);
	/* for every string in the list */
	for (i = 0; i < nr; ++i) {
		/* check the string content */
		rc = memcmp(slist->ntsl_list[i], &buf[i * 3], 2);
		M0_UT_ASSERT(rc == 0);
		/* check the string size */
		rc = strlen(slist->ntsl_list[i]);
		M0_UT_ASSERT(rc == 2);
	}
}

void m0_net_test_slist_ut(void)
{
	struct m0_net_test_slist slist;
	static char		 buf[NET_TEST_SLIST_NR * 3];
	int			 i;
	int			 rc;
	bool			 rc_bool;
	static char		 bv_buf[NET_TEST_SLIST_BV_LEN];
	void			*bv_addr = bv_buf;
	m0_bcount_t		 bv_len = NET_TEST_SLIST_BV_LEN;
	struct m0_bufvec	 bv = M0_BUFVEC_INIT_BUF(&bv_addr, &bv_len);
	m0_bcount_t		 len;
	m0_bcount_t		 len2;


	/* empty string test */
	rc = m0_net_test_slist_init(&slist, "", ':');
	M0_UT_ASSERT(rc == 0);
	M0_UT_ASSERT(slist.ntsl_nr == 0);
	rc_bool = m0_net_test_slist_unique(&slist);
	M0_UT_ASSERT(rc_bool);
	len = m0_net_test_slist_serialize(M0_NET_TEST_SERIALIZE, &slist,
					  &bv, 0);
	M0_UT_ASSERT(len > 0);
	m0_net_test_slist_fini(&slist);
	len2 = m0_net_test_slist_serialize(M0_NET_TEST_DESERIALIZE, &slist,
					   &bv, 0);
	M0_UT_ASSERT(len2 == len);
	M0_UT_ASSERT(slist.ntsl_nr == 0);
	m0_net_test_slist_fini(&slist);
	/* one string test */
	rc = m0_net_test_slist_init(&slist, "asdf", ',');
	M0_UT_ASSERT(rc == 0);
	M0_UT_ASSERT(slist.ntsl_nr == 1);
	M0_UT_ASSERT(slist.ntsl_list != NULL);
	M0_UT_ASSERT(slist.ntsl_list[0] != NULL);
	M0_UT_ASSERT(strncmp(slist.ntsl_list[0], "asdf", 5) == 0);
	rc_bool = m0_net_test_slist_unique(&slist);
	M0_UT_ASSERT(rc_bool);
	m0_net_test_slist_fini(&slist);
	/* one-of-strings-is-empty test */
	rc = m0_net_test_slist_init(&slist, "asdf,", ',');
	M0_UT_ASSERT(rc == 0);
	M0_UT_ASSERT(slist.ntsl_nr == 2);
	M0_UT_ASSERT(slist.ntsl_list != NULL);
	for (i = 0; i < 2; ++i)
		M0_UT_ASSERT(slist.ntsl_list[i] != NULL);
	M0_UT_ASSERT(strncmp(slist.ntsl_list[0], "asdf", 5) == 0);
	M0_UT_ASSERT(strncmp(slist.ntsl_list[1], "",     1) == 0);
	rc_bool = m0_net_test_slist_unique(&slist);
	M0_UT_ASSERT(rc_bool);
	m0_net_test_slist_fini(&slist);
	/* many strings test */
	/* fill string with some pattern (for example, "01,12,23,34\0") */
	for (i = 0; i < NET_TEST_SLIST_NR; ++i) {
		buf[i * 3]     = '0' + i % 10;
		buf[i * 3 + 1] = '0' + (i + 1) % 10;
		buf[i * 3 + 2] = ',';
	}
	/* run test for every string number in [0, NET_TEST_SLIST_NR) */
	for (i = 0; i < NET_TEST_SLIST_NR; ++i) {
		/* cut the line */
		buf[i * 3 + 2] = '\0';
		/* alloc slist */
		rc = m0_net_test_slist_init(&slist, buf, ',');
		M0_UT_ASSERT(rc == 0);
		rc_bool = m0_net_test_slist_unique(&slist);
		M0_UT_ASSERT((!rc_bool) ^ (i < 10));
		slist_check(&slist, i, buf);
		/* serialize string list to buffer */
		len = m0_net_test_slist_serialize(M0_NET_TEST_SERIALIZE, &slist,
						  &bv, 0);
		M0_UT_ASSERT(len > 0);
		/* free slist */
		m0_net_test_slist_fini(&slist);
		/* deserialize string slist from buffer */
		/* alloc slist */
		len2 = m0_net_test_slist_serialize(M0_NET_TEST_DESERIALIZE,
						   &slist, &bv, 0);
		M0_UT_ASSERT(len2 == len);
		slist_check(&slist, i, buf);
		/* free slist */
		m0_net_test_slist_fini(&slist);
		/* restore line delimiter */
		buf[i * 3 + 2] = ',';
	}
}

/*
 *  Local variables:
 *  c-indentation-style: "K&R"
 *  c-basic-offset: 8
 *  tab-width: 8
 *  fill-column: 79
 *  scroll-step: 1
 *  End:
 */
