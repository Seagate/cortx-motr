/* -*- C -*- */
/*
 * Copyright (c) 2016-2020 Seagate Technology LLC and/or its Affiliates
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


#include "lib/vec.h"       /* m0_bufvec */
#include "ut/ut.h"         /* M0_UT_ASSERT() */
#include "lib/hash_fnc.h"

enum {
	KEY_COUNT = 100,
	KEY_SIZE  = 100
};

void hash_city(void)
{
	struct m0_bufvec vals;
	struct m0_bufvec hash1;
	struct m0_bufvec hash2;
	int              i;
	int              j;
	int              rc;

	rc = m0_bufvec_alloc(&vals, KEY_COUNT, KEY_SIZE);
	M0_UT_ASSERT(rc == 0);
	rc = m0_bufvec_alloc(&hash1, KEY_COUNT, sizeof(uint64_t));
	M0_UT_ASSERT(rc == 0);
	rc = m0_bufvec_alloc(&hash2, KEY_COUNT, sizeof(uint64_t));
	M0_UT_ASSERT(rc == 0);

	for (i = 0; i < vals.ov_vec.v_nr; i++)
		for (j = 0; j < KEY_SIZE; j++)
			*(char*)((char*)(vals.ov_buf[i]) + j) = (j + i) & 0xff;
	for (i = 0; i < vals.ov_vec.v_nr; i++)
		*(uint64_t*)hash1.ov_buf[i] =
			m0_hash_fnc_city(vals.ov_buf[i], vals.ov_vec.v_count[i]);
	for (i = 0; i < vals.ov_vec.v_nr; i++)
		*(uint64_t*)hash2.ov_buf[i] =
			m0_hash_fnc_city(vals.ov_buf[i], vals.ov_vec.v_count[i]);
	M0_UT_ASSERT(m0_forall(i, hash1.ov_vec.v_nr,
			memcmp(hash1.ov_buf[i], hash2.ov_buf[i],
			       hash1.ov_vec.v_count[i]) == 0));
	m0_bufvec_free(&vals);
	m0_bufvec_free(&hash1);
	m0_bufvec_free(&hash2);
}

void hash_fnv1(void)
{
	struct m0_bufvec vals;
	struct m0_bufvec hash1;
	struct m0_bufvec hash2;
	int              i;
	int              j;
	int              rc;

	rc = m0_bufvec_alloc(&vals, KEY_COUNT, KEY_SIZE);
	M0_UT_ASSERT(rc == 0);
	rc = m0_bufvec_alloc(&hash1, KEY_COUNT, sizeof(uint64_t));
	M0_UT_ASSERT(rc == 0);
	rc = m0_bufvec_alloc(&hash2, KEY_COUNT, sizeof(uint64_t));
	M0_UT_ASSERT(rc == 0);

	for (i = 0; i < vals.ov_vec.v_nr; i++)
		for (j = 0; j < KEY_SIZE; j++)
			*(char*)((char*)(vals.ov_buf[i]) + j) = (j + i) & 0xff;
	for (i = 0; i < vals.ov_vec.v_nr; i++)
		*(uint64_t*)hash1.ov_buf[i] =
			m0_hash_fnc_fnv1(vals.ov_buf[i], vals.ov_vec.v_count[i]);
	for (i = 0; i < vals.ov_vec.v_nr; i++)
		*(uint64_t*)hash2.ov_buf[i] =
			m0_hash_fnc_fnv1(vals.ov_buf[i], vals.ov_vec.v_count[i]);
	M0_UT_ASSERT(m0_forall(i, hash1.ov_vec.v_nr,
			memcmp(hash1.ov_buf[i], hash2.ov_buf[i],
			       hash1.ov_vec.v_count[i]) == 0));
	m0_bufvec_free(&vals);
	m0_bufvec_free(&hash1);
	m0_bufvec_free(&hash2);
}

void test_hash_fnc(void)
{
	hash_city();
	hash_fnv1();
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
