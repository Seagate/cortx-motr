/* -*- C -*- */
/*
 * Copyright (c) 2020-2021 Seagate Technology LLC and/or its Affiliates
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


#include "lib/memory.h"               /* m0_alloc, m0_free */
#include "lib/cksum.h"

#define M0_TRACE_SUBSYSTEM M0_TRACE_SUBSYS_LIB
#include "lib/trace.h"
//#include "lib/finject.h"


M0_INTERNAL int m0_calculate_md5_inc_context(
		struct m0_md5_inc_context_pi *pi,
		struct m0_pi_seed *seed,
		struct m0_bufvec *bvec,
		enum m0_pi_calc_flag flag,
		unsigned char *curr_context,
		unsigned char *pi_value_without_seed)
{
#ifndef __KERNEL__
	MD5_CTX context;
	int i, rc;

	M0_ENTRY();

	M0_PRE(pi != NULL);
	M0_PRE(curr_context != NULL);
	M0_PRE(ergo(bvec != NULL && bvec->ov_vec.v_nr != 0,
				bvec->ov_vec.v_count != NULL &&
				bvec->ov_buf != NULL));

	/* This call is for first data unit, need to initialize prev_context */
	if (flag & M0_PI_CALC_UNIT_ZERO) {
		pi->hdr.pi_size = sizeof(struct m0_md5_inc_context_pi);
		rc = MD5_Init((MD5_CTX *)&pi->prev_context);
		if (rc != 1) {
			return M0_ERR_INFO(rc, "MD5_Init failed.");
		}
	}

	/* memcpy, so that we do not change the prev_context */
	memcpy(curr_context, &pi->prev_context, sizeof(MD5_CTX));


	/* get the curr context by updating it*/
	if (bvec != NULL) {
		for (i = 0; i < bvec->ov_vec.v_nr; i++) {
			rc = MD5_Update((MD5_CTX *)curr_context, bvec->ov_buf[i],
					bvec->ov_vec.v_count[i]);
			if (rc != 1) {
				return M0_ERR_INFO(rc, "MD5_Update failed."
						"curr_context=%p, "
						"bvec->ov_buf[%d]=%p, "
						"bvec->ov_vec.v_count[%d]=%lu",
						curr_context, i,
						bvec->ov_buf[i], i,
						bvec->ov_vec.v_count[i]);
			}
		}
	}

	/* If caller wants checksum without seed and with seed, caller needs to
	 * pass 'pi_value_without_seed'. pi_value will be used to return with
	 * seed checksum. 'pi_value_without_seed' will be used to return non
	 * seeded checksum.
	 */
	if (pi_value_without_seed != NULL) {
		/* 
		 * NOTE: MD5_final() changes the context itself and curr_context
		 * should not be finalised, thus copy it and use it for MD5_final
		 */
		memcpy((void *)&context, (void *)curr_context, sizeof(MD5_CTX));

		rc = MD5_Final(pi_value_without_seed, &context);
		if (rc != 1) {
			return M0_ERR_INFO(rc, "MD5_Final failed"
					"pi_value_without_seed=%p"
					"curr_context=%p",
					pi_value_without_seed, curr_context);
		}
	}

	/* if seed is passed, memcpy and update the context calculated so far.
	 * calculate checksum with seed, set the pi_value with seeded checksum.
	 * If seed is not passed than memcpy context and calculate checksum
	 * without seed, set the pi_value with unseeded checksum.
	 * NOTE: curr_context will always have context without seed.
	 */
	memcpy((void *)&context, (void *)curr_context, sizeof(MD5_CTX));

	if (seed != NULL) {

		/*
		 * seed_str have string represention for 3 uint64_t(8 bytes)
		 * range for uint64_t is 0 to 18,446,744,073,709,551,615 at
		 * max 20 chars per var, for three var it will be 3*20, +1 '\0'.
		 * seed_str needs to be 61 bytes, round off and taking 64 bytes.
		 */
		char seed_str[64] = {'\0'};
		snprintf(seed_str, sizeof(seed_str), "%"PRIx64"%"PRIx64"%"PRIx64,
				seed->obj_id.f_container, seed->obj_id.f_key,
				seed->data_unit_offset);
		rc = MD5_Update(&context, (unsigned char *)seed_str,
				sizeof(seed_str));
		if (rc != 1) {

			return M0_ERR_INFO(rc, "MD5_Update fail curr_context=%p"
					"f_container %"PRIx64"f_key %"PRIx64
					"data_unit_offset %"PRIx64"seed_str %s",
					curr_context, seed->obj_id.f_container,
					seed->obj_id.f_key,
					seed->data_unit_offset,
					(char *)seed_str);
		}
	}

	if (!(flag & M0_PI_SKIP_CALC_FINAL)) {
		rc = MD5_Final(pi->pi_value, &context);
		if (rc != 1) {
			return M0_ERR_INFO(rc, "MD5_Final fail curr_context=%p",
					curr_context);
		}
	}
#endif
	return  M0_RC(0);
}

M0_INTERNAL uint64_t m0_calculate_cksum_size(struct m0_generic_pi *pi)
{
	M0_ENTRY();
#ifndef __KERNEL__
	switch(pi->hdr.pi_type) {

		case M0_PI_TYPE_MD5_INC_CONTEXT:
		{
			return sizeof(struct m0_md5_inc_context_pi);
		}
		case M0_PI_TYPE_MD5:
		{
			return sizeof(struct m0_md5_pi);
		}

	}
#endif
	return 0;
}

int m0_client_calculate_pi(struct m0_generic_pi *pi,
		struct m0_pi_seed *seed,
		struct m0_bufvec *bvec,
		enum m0_pi_calc_flag flag,
		unsigned char *curr_context,
		unsigned char *pi_value_without_seed)
{
	M0_ENTRY();
	int rc = 0;
#ifndef __KERNEL__
	switch(pi->hdr.pi_type) {

		case M0_PI_TYPE_MD5_INC_CONTEXT:
		{
			struct m0_md5_inc_context_pi *md5_context_pi = 
				(struct m0_md5_inc_context_pi *) pi;
			rc = m0_calculate_md5_inc_context(md5_context_pi, seed, bvec,
					flag, curr_context,
					pi_value_without_seed);
		}
	}
#endif
	return M0_RC(rc);
}

M0_EXPORTED(m0_client_calculate_pi);

bool m0_calc_verify_cksum_one_unit(struct m0_generic_pi *pi,
                                   struct m0_pi_seed *seed,
                                   struct m0_bufvec *bvec)
{
#ifndef __KERNEL__
	switch(pi->hdr.pi_type) {
		case M0_PI_TYPE_MD5_INC_CONTEXT:
			{
				struct m0_md5_inc_context_pi md5_ctx_pi;
				unsigned char *curr_context = m0_alloc(sizeof(MD5_CTX));
				memset(&md5_ctx_pi, 0, sizeof(struct m0_md5_inc_context_pi));
				if (curr_context == NULL) {
					return false;
				}
				memcpy(md5_ctx_pi.prev_context,
						((struct m0_md5_inc_context_pi *)pi)->prev_context,
						sizeof(MD5_CTX));


				md5_ctx_pi.hdr.pi_type =
					M0_PI_TYPE_MD5_INC_CONTEXT;
				m0_client_calculate_pi((struct m0_generic_pi *)&md5_ctx_pi,
						seed, bvec, M0_PI_NO_FLAG,
						curr_context, NULL);
				m0_free(curr_context);
				if (memcmp(((struct m0_md5_inc_context_pi *)pi)->pi_value,
							md5_ctx_pi.pi_value,
							MD5_DIGEST_LENGTH) == 0) {
					return true;
				}
				else {
					M0_LOG(M0_DEBUG, "checksum fail "
							"f_container %"PRIx64"f_key %"PRIx64
							"data_unit_offset %"PRIx64,
							seed->obj_id.f_container,
							seed->obj_id.f_key,
							seed->data_unit_offset);
				}
			}
	}
#endif
	return false;
}

M0_EXPORTED(m0_calc_verify_cksum_one_unit);

#undef M0_TRACE_SUBSYSTEM

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
