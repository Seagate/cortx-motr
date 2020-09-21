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


#pragma once

#ifndef __MOTR_STOB_BATTR_H__
#define __MOTR_STOB_BATTR_H__

/**
 * Identifiers of stob block attributes
 */

enum m0_battr_id {
	/**
	 * 64-bit of T10 Data Integrity Field for sector size of 512B
	 * - Reference Tag: 4 bytes
	 * - Meta Tag: 2 bytes
	 * - Guard Tag: 2 bytes
	 */
	M0_BI_T10_DIF_512B,
	/**
	 * 128-bit of T10 Data Integrity Field for sector size of 4KiB
	 * - Reference Tag: ? bytes
	 * - Meta Tag: ? bytes
	 * - Guard Tag: ? bytes
	 */
	M0_BI_T10_DIF_4KB_0,
	M0_BI_T10_DIF_4KB_1,
	/**
	 * 64-bits of data block version, used by DTM
	 */
	M0_BI_VERNO,
	/**
	 * 32-bit CRC checksum
	 */
	M0_BI_CKSUM_CRC_32,
	/**
	 * 32-bit Fletcher-32 checksum
	 */
	M0_BI_CKSUM_FLETCHER_32,
	/**
	 * 64-bit Fletcher-64 checksum
	 */
	M0_BI_CKSUM_FLETCHER_64,
	/**
	 * 256-bit SHA-256 checksum
	 */
	M0_BI_CKSUM_SHA256_0,
	M0_BI_CKSUM_SHA256_1,
	M0_BI_CKSUM_SHA256_2,
	M0_BI_CKSUM_SHA256_3,
	/**
	 * 64-bit Reference Tag
	 */
	M0_BI_REF_TAG,
};

#endif

/*
 *  Local variables:
 *  c-indentation-style: "K&R"
 *  c-basic-offset: 8
 *  tab-width: 8
 *  fill-column: 80
 *  scroll-step: 1
 *  End:
 */
