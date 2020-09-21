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


#include "net/test/str.h"
#include "lib/vec.h"       /* m0_bufvec */
#include "lib/memory.h"    /* m0_free0 */
#include "ut/ut.h"

enum {
	STR_BUF_LEN    = 0x100,
	STR_BUF_OFFSET = 42,
};

static void try_serialize(char *str)
{
	char		 buf[STR_BUF_LEN];
	void		*addr = buf;
	m0_bcount_t	 buf_len = STR_BUF_LEN;
	struct m0_bufvec bv = M0_BUFVEC_INIT_BUF(&addr, &buf_len);
	m0_bcount_t	 serialized_len;
	m0_bcount_t	 len;
	char		*str2;
	int		 str_len;
	int		 rc;

	serialized_len = m0_net_test_str_serialize(M0_NET_TEST_SERIALIZE,
						   &str, &bv, STR_BUF_OFFSET);
	M0_UT_ASSERT(serialized_len > 0);

	str2 = NULL;
	len = m0_net_test_str_serialize(M0_NET_TEST_DESERIALIZE,
					&str2, &bv, STR_BUF_OFFSET);
	M0_UT_ASSERT(len == serialized_len);

	str_len = strlen(str);
	rc = strncmp(str, str2, str_len + 1);
	M0_UT_ASSERT(rc == 0);
	m0_free0(&str2);
}

void m0_net_test_str_ut(void)
{
	try_serialize("");
	try_serialize("asdf");
	try_serialize("SGVsbG8sIHdvcmxkIQo=");
	try_serialize("0123456789!@#$%^&*()qwertyuiopasdfghjklzxcvbnm"
		      "ABCDEFGHIJKLMNOPQRSTUVWXYZ");
	try_serialize(__FILE__);
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
