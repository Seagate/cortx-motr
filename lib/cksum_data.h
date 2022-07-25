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

#pragma once

#ifndef __MOTR_CKSUM_DATA_H__
#define __MOTR_CKSUM_DATA_H__

#include "lib/cksum.h"

/*
 * This file will content the specific data structure for implementation
 * e.g. MD5, CRC, etc
 */

#ifndef __KERNEL__
#include <openssl/md5.h>
#endif

/*********************** MD5 Cksum Structure ***********************/
/** Padding size for MD5 structure */
#define M0_CKSUM_PAD_MD5 (M0_CALC_PAD((sizeof(struct m0_pi_hdr) + \
				      MD5_DIGEST_LENGTH), \
				      M0_CKSUM_DATA_ROUNDOFF_BYTE))

/** MD5 checksum structure, the checksum value is in pimd5_value */
struct m0_md5_pi {
	/* header for protection info */
	struct m0_pi_hdr pimd5_hdr;
#ifndef __KERNEL__
	/* protection value computed for the current data*/
	unsigned char    pimd5_value[MD5_DIGEST_LENGTH];
	/* structure should be 32 byte aligned */
	char             pimd5_pad[M0_CKSUM_PAD_MD5];
#endif
};

/****************** MD5 Including Context Checksum Structure ******************/
/** Padding size for MD5 Including Context structure */
#define M0_CKSUM_PAD_MD5_INC_CXT (M0_CALC_PAD((sizeof(struct m0_pi_hdr) + \
				  sizeof(MD5_CTX)+MD5_DIGEST_LENGTH), \
				  M0_CKSUM_DATA_ROUNDOFF_BYTE))

/** MD5 checksum structure:
 *  - The computed checksum value will be in pimd5c_value.
 *  - Input context from previous MD5 computation in pimd5c_prev_context
 */
struct m0_md5_inc_context_pi {
	/* header for protection info */
	struct m0_pi_hdr pimd5c_hdr;
#ifndef __KERNEL__
	/*context of previous data unit, required for checksum computation */
	unsigned char    pimd5c_prev_context[sizeof(MD5_CTX)];
	/* protection value computed for the current data unit.
	 * If seed is not provided then this checksum is
	 * calculated without seed.
	 */
	unsigned char    pimd5c_value[MD5_DIGEST_LENGTH];
	/* structure should be 32 byte aligned */
	char             pi_md5c_pad[M0_CKSUM_PAD_MD5_INC_CXT];
#endif
};

union m0_md5_union_pi
{
	struct m0_md5_pi p;
	struct m0_md5_inc_context_pi c;
};

/* Max checksum size for all supported PIs */
enum {
	M0_CKSUM_MAX_SIZE = sizeof(union m0_md5_union_pi)
};

#endif /* __MOTR_CKSUM_DATA_H__ */
