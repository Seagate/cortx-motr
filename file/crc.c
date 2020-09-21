/* -*- C -*- */
/*
 * Copyright (c) 2013-2020 Seagate Technology LLC and/or its Affiliates
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


#include "lib/misc.h"

/**
   @addtogroup data_integrity

   @{
 */

static uint64_t md_crc32_cksum(void *data, uint64_t len, uint64_t *cksum);

/**
 * Table-driven implementaion of crc32.
 * CRC table is generated during first crc operation which contains the all
 * possible crc values for the byte of data. These values are used to
 * compute CRC for the all the bytes of data in the block of given length.
 */

#define CRC_POLY	0x04C11DB7
#define CRC_WIDTH	32
#define CRC_SLICE_SIZE	8
#define CRC_TABLE_SIZE	256

uint32_t crc_table[CRC_TABLE_SIZE];
static bool is_table = false;

static void crc_mktable(void)
{
	int	 i;
	int	 j;
	uint32_t hibit = M0_BITS(CRC_WIDTH - 1);
	uint32_t crc;

	for (i = 0; i < CRC_TABLE_SIZE; i++) {
		crc = (uint32_t)i <<  (CRC_WIDTH - CRC_SLICE_SIZE);
		for(j = 0; j < CRC_SLICE_SIZE; j++) {
			crc <<= 1;
			if (crc &  hibit)
				crc ^= CRC_POLY;
		}
		crc_table[i] = crc;
	}
}

static uint32_t crc32(uint32_t crc, unsigned char const *data, m0_bcount_t len)
{
	M0_PRE(data != NULL);
	M0_PRE(len > 0);

	if (!is_table) {
		crc_mktable();
		is_table = true;
	}

	while (len--)
		crc = ((crc << CRC_SLICE_SIZE) | *data++) ^
			crc_table[crc >> (CRC_WIDTH - CRC_SLICE_SIZE) & 0xFF];

	return crc;
}

M0_INTERNAL void m0_crc32(const void *data, uint64_t len,
			  uint64_t *cksum)
{
	M0_PRE(data != NULL);
	M0_PRE(len > 0);
	M0_PRE(cksum != NULL);

	cksum[0] = crc32(~0, data, len);
}

M0_INTERNAL bool m0_crc32_chk(const void *data, uint64_t len,
			      const uint64_t *cksum)
{
	M0_PRE(data != NULL);
	M0_PRE(len > 0);
	M0_PRE(cksum != NULL);

	return cksum[0] == (uint64_t) crc32(~0, data, len);
}

static void md_crc32_cksum_set(void *data, uint64_t len, uint64_t *cksum)
{
	*cksum = md_crc32_cksum(data, len, cksum);
}

static uint64_t md_crc32_cksum(void *data, uint64_t len, uint64_t *cksum)
{
	uint64_t crc = ~0;
	uint64_t old_cksum = *cksum;

	*cksum = 0;
	crc = crc32(crc, data, len);
	*cksum = old_cksum;

	return crc;
}

static bool md_crc32_cksum_check(void *data, uint64_t len, uint64_t *cksum)
{
	return *cksum == md_crc32_cksum(data, len, cksum);
}

/** @} end of data_integrity */

/*
 *  Local variables:
 *  c-indentation-style: "K&R"
 *  c-basic-offset: 8
 *  tab-width: 8
 *  fill-column: 79
 *  scroll-step: 1
 *  End:
 */
