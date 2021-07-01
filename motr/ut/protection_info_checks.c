/* -*- C -*- */
/*
 * Copyright (c) 2021 Seagate Technology LLC and/or its Affiliates
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

#ifndef __KERNEL__
#include <openssl/md5.h>
#endif /* __KERNEL__ */

#include "ut/ut.h"
#include "motr/client.h"
#include "motr/client_internal.h"
#include "motr/ut/client.h"

#define M0_TRACE_SUBSYSTEM M0_TRACE_SUBSYS_CLIENT
#include "lib/trace.h"          /* M0_LOG */

struct m0_ut_suite ut_suite_pi;

enum {
	BUFFER_SIZE = 4096,
	SEGS_NR     = 16,

	BIG_BUFFER_SIZE = 4096*16,
	BIG_SEGS_NR = 10
};

#define DATA_UNIT_COUNT 10
#define OBJ_CONTAINER 0x123
#define OBJ_KEY 0x456

struct m0_bufvec *user_data;
unsigned char *curr_context[DATA_UNIT_COUNT];
unsigned char *seeded_sum[DATA_UNIT_COUNT];
unsigned char *final_sum;

struct m0_bufvec *big_user_data;
unsigned char *big_curr_context;
unsigned char *big_final_sum;

M0_INTERNAL int pi_init(void)
{
#ifndef __KERNEL__
	ut_shuffle_test_order(&ut_suite_pi);
#endif

	int i, j, rc;
	// allocate and populate buffers

	M0_ALLOC_ARR(user_data, DATA_UNIT_COUNT);
	M0_UT_ASSERT(user_data != NULL);

	for (j = 0; j < DATA_UNIT_COUNT; j++) {
		rc = m0_bufvec_alloc(&user_data[j], SEGS_NR, BUFFER_SIZE);
		M0_UT_ASSERT(rc == 0);
		for (i = 0; i < user_data[j].ov_vec.v_nr; ++i) {
			memset(user_data[j].ov_buf[i], 'a' + j, BUFFER_SIZE);
		}

		M0_ALLOC_ARR(curr_context[j], sizeof(MD5_CTX));
		M0_UT_ASSERT(curr_context[j] != NULL);
		M0_ALLOC_ARR(seeded_sum[j], MD5_DIGEST_LENGTH);
		M0_UT_ASSERT(seeded_sum[j] != NULL);
	}

	M0_ALLOC_ARR(final_sum, MD5_DIGEST_LENGTH);
	M0_UT_ASSERT(final_sum != NULL);

	M0_ALLOC_ARR(big_user_data, 1);
	M0_UT_ASSERT(big_user_data != NULL);
	rc = m0_bufvec_alloc(big_user_data, BIG_SEGS_NR, BIG_BUFFER_SIZE);
	M0_UT_ASSERT(rc == 0);
	for (i = 0; i < big_user_data->ov_vec.v_nr; ++i) {
		memset(big_user_data->ov_buf[i], 'a' + i, BIG_BUFFER_SIZE);
	}

	M0_ALLOC_ARR(big_curr_context, sizeof(MD5_CTX));
	M0_UT_ASSERT(big_curr_context != NULL);

	M0_ALLOC_ARR(big_final_sum, MD5_DIGEST_LENGTH);
	M0_UT_ASSERT(big_final_sum != NULL);

	return 0;
}

M0_INTERNAL int pi_fini(void)
{
	int j;

	for (j = 0; j < DATA_UNIT_COUNT; j++) {
		m0_bufvec_free(&user_data[j]);
		m0_free(seeded_sum[j]);
		m0_free(curr_context[j]);
	}

	m0_free(user_data);
	m0_free(final_sum);
	m0_free(big_curr_context);
	m0_free(big_final_sum);
	m0_bufvec_free(big_user_data);
	m0_free(big_user_data);
	return 0;
}

void verify_case_one_two(void)
{
	int i, j, rc;
	MD5_CTX curr_ctx;
	MD5_CTX tmp_ctx;
	char seed_str[64] = {'\0'};
	struct m0_pi_seed seed;
	unsigned char *seed_sum;
	unsigned char *unseed_final_sum;

	M0_ALLOC_ARR(seed_sum, MD5_DIGEST_LENGTH);
	M0_UT_ASSERT(seed_sum != NULL);
	M0_ALLOC_ARR(unseed_final_sum, MD5_DIGEST_LENGTH);
	M0_UT_ASSERT(unseed_final_sum != NULL);

	rc = MD5_Init(&curr_ctx);
	M0_UT_ASSERT(rc == 1);

	for (j = 0; j < DATA_UNIT_COUNT; j++)
	{
		for (i = 0; i < user_data[j].ov_vec.v_nr; i++) {
			rc = MD5_Update(&curr_ctx, user_data[j].ov_buf[i],
					user_data[j].ov_vec.v_count[i]);
			M0_UT_ASSERT(rc == 1);
		}

		M0_UT_ASSERT(memcmp((MD5_CTX *)curr_context[j], &curr_ctx,
					sizeof(MD5_CTX)) == 0);

		if (j == DATA_UNIT_COUNT - 1 ) {
			rc = MD5_Final(unseed_final_sum, &curr_ctx);
			M0_UT_ASSERT(rc == 1);
			M0_UT_ASSERT(memcmp(final_sum, unseed_final_sum,
						MD5_DIGEST_LENGTH) == 0);
		}
	}

	m0_fid_set(&seed.obj_id, OBJ_CONTAINER, OBJ_KEY);
	for (j = 0; j < DATA_UNIT_COUNT; j++)
	{
		memcpy((void *)&tmp_ctx,(void *)curr_context[j],sizeof(MD5_CTX));
		seed.data_unit_offset = j*SEGS_NR*BUFFER_SIZE;
		snprintf(seed_str,sizeof(seed_str),"%"PRIx64"%"PRIx64"%"PRIx64,
				seed.obj_id.f_container, seed.obj_id.f_key,
				seed.data_unit_offset);

		rc = MD5_Update(&tmp_ctx, (unsigned char *)seed_str,
				sizeof(seed_str));
		M0_UT_ASSERT(rc == 1);
		rc = MD5_Final(seed_sum, &tmp_ctx);
		M0_UT_ASSERT(rc == 1);
		M0_UT_ASSERT(memcmp(seeded_sum[j], seed_sum,
					MD5_DIGEST_LENGTH) == 0);
	}

	M0_UT_ASSERT(memcmp(final_sum, big_final_sum,
				MD5_DIGEST_LENGTH) == 0);

	m0_free(seed_sum);
	m0_free(unseed_final_sum);
}

/*
 * CASE1:
 * Calculate seeded checksums and non seeded contexts via API
 * for multilple data units one by one. Calculate unseeded checksum
 * for last data unit.
 * Verification :  Calculate seeded checksums and non seeded contexts
 * via UT code verify_case_one_two() for multilple data units one by one.
 * Calculate unseeded checksum for last data unit.
 * Compare seeded checksums and non seeded contexts and final unseeded
 * checksum obtained via API and UT. API and UT calculations should match.
 *
 * CASE2:
 * step 1: Pass all the data units as one chunk to API with no seed and compute
 * unseeded final checksum.
 * step 2: Calculate seeded checksums and non seeded contexts via API
 * for multilple data units one by one. Calculate unseeded checksum
 * for last data unit. (Already done by case 1)
 * Verification : Unseeded checksum from step1 and step2 should match.
 */
static void ut_test_pi_api_case_one_two(void)
{

	int j, rc;
	struct m0_md5_inc_context_pi pi;
	struct m0_pi_seed seed;

	m0_fid_set(&seed.obj_id, OBJ_CONTAINER, OBJ_KEY);

	memset(&pi, 0, sizeof(struct m0_md5_inc_context_pi));
	pi.hdr.pi_type = M0_PI_TYPE_MD5_INC_CONTEXT;

	for (j = 0; j < DATA_UNIT_COUNT; j++) {

		seed.data_unit_offset = j*SEGS_NR*BUFFER_SIZE;
		if (j == 0) {
			rc = m0_client_calculate_pi((struct m0_generic_pi *)&pi,
					&seed, &user_data[j], M0_PI_CALC_UNIT_ZERO,
					curr_context[j], NULL);
			M0_UT_ASSERT(rc == 0);
		}
		else if (j == DATA_UNIT_COUNT - 1) {
			rc = m0_client_calculate_pi((struct m0_generic_pi *)&pi,
					&seed, &user_data[j], M0_PI_NO_FLAG,
					curr_context[j], final_sum);
			M0_UT_ASSERT(rc == 0);
		}
		else {
			rc = m0_client_calculate_pi((struct m0_generic_pi *)&pi,
					&seed, &user_data[j], M0_PI_NO_FLAG,
					curr_context[j], NULL);
			M0_UT_ASSERT(rc == 0);
		}

		memcpy(pi.prev_context, curr_context[j], sizeof(MD5_CTX));
		memcpy(seeded_sum[j], pi.pi_value, MD5_DIGEST_LENGTH);
	}

	memset(&pi, 0, sizeof(struct m0_md5_inc_context_pi));
	pi.hdr.pi_type = M0_PI_TYPE_MD5_INC_CONTEXT;
	rc = m0_client_calculate_pi((struct m0_generic_pi *)&pi, NULL,
			big_user_data, M0_PI_CALC_UNIT_ZERO,
			big_curr_context, big_final_sum);
	M0_UT_ASSERT(rc == 0);

	verify_case_one_two();
}


/* CASE 3:
 * STEP1: Call API for different data units one by one. Seed should be passed
 * for only last data unit which is some non-zero offset.
 * STEP2 : call API one time with all data units as one chunk
 * with the same seed as used in step1.
 * Verification: seeded checksum (pi_value) from both the steps should match.
 */
static void ut_test_pi_api_case_third(void)
{

	int j, rc;
	struct m0_md5_inc_context_pi pi;
	struct m0_pi_seed seed;
	unsigned char seeded_final_chunks_value[MD5_DIGEST_LENGTH];

	m0_fid_set(&seed.obj_id, OBJ_CONTAINER, OBJ_KEY);
	seed.data_unit_offset = (DATA_UNIT_COUNT-1)*SEGS_NR*BUFFER_SIZE;

	memset(&pi, 0, sizeof(struct m0_md5_inc_context_pi));
	pi.hdr.pi_type = M0_PI_TYPE_MD5_INC_CONTEXT;

	/* STEP 1 */
	for (j = 0; j < DATA_UNIT_COUNT; j++) {

		if (j == 0) {
			rc = m0_client_calculate_pi((struct m0_generic_pi *)&pi,
					NULL, &user_data[j], M0_PI_CALC_UNIT_ZERO,
					curr_context[j], NULL);
			M0_UT_ASSERT(rc == 0);
		}
		else if (j == DATA_UNIT_COUNT - 1) {
			rc = m0_client_calculate_pi((struct m0_generic_pi *)&pi,
					&seed, &user_data[j], M0_PI_NO_FLAG,
					curr_context[j], final_sum);
			M0_UT_ASSERT(rc == 0);
		}
		else {
			rc = m0_client_calculate_pi((struct m0_generic_pi *)&pi,
					NULL, &user_data[j], M0_PI_NO_FLAG,
					curr_context[j], NULL);
			M0_UT_ASSERT(rc == 0);
		}

		memcpy(pi.prev_context, curr_context[j], sizeof(MD5_CTX));
		memcpy(seeded_sum[j], pi.pi_value, MD5_DIGEST_LENGTH);
	}

	memcpy(&seeded_final_chunks_value, pi.pi_value, MD5_DIGEST_LENGTH);

	/* STEP 2 */
	memset(&pi, 0, sizeof(struct m0_md5_inc_context_pi));
	pi.hdr.pi_type = M0_PI_TYPE_MD5_INC_CONTEXT;
	rc = m0_client_calculate_pi((struct m0_generic_pi *)&pi,
			&seed, big_user_data, M0_PI_CALC_UNIT_ZERO,
			big_curr_context, big_final_sum);
	M0_UT_ASSERT(rc == 0);

	/* VERIFICATION */
	M0_UT_ASSERT(memcmp(&seeded_final_chunks_value, pi.pi_value,
				MD5_DIGEST_LENGTH) == 0);
	M0_UT_ASSERT(memcmp(final_sum, big_final_sum, MD5_DIGEST_LENGTH) == 0);

}


struct m0_ut_suite ut_suite_pi = {
	.ts_name = "pi_ut",
	.ts_init = pi_init,
	.ts_fini = pi_fini,
	.ts_tests = {

		/* Initialising client. */
		{ "m0_pi_checks_case_one_two", &ut_test_pi_api_case_one_two},
		{ "m0_pi_checks_case_third", &ut_test_pi_api_case_third},
		{ NULL, NULL },
	}
};

#undef M0_TRACE_SUBSYSTEM


