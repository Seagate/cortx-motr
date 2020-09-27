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


#pragma once

#ifndef __MOTR_LIB_BYTEORDER_H__
#define __MOTR_LIB_BYTEORDER_H__

#include "lib/types.h"		/* uint16_t */

#if defined(M0_LINUX)
#include <asm/byteorder.h> /* __cpu_to_be16 */
#elif defined(M0_DARWIN)
#include <libkern/OSByteOrder.h>
#define __cpu_to_be16 OSSwapHostToBigInt16
#define __cpu_to_be32 OSSwapHostToBigInt32
#define __cpu_to_be64 OSSwapHostToBigInt64
#define __cpu_to_le16 OSSwapHostToLittleInt16
#define __cpu_to_le32 OSSwapHostToLittleInt32
#define __cpu_to_le64 OSSwapHostToLittleInt64

#define __be16_to_cpu OSSwapBigToHostInt16
#define __be32_to_cpu OSSwapBigToHostInt32
#define __be64_to_cpu OSSwapBigToHostInt64
#define __le16_to_cpu OSSwapLittleToHostInt16
#define __le32_to_cpu OSSwapLittleToHostInt32
#define __le64_to_cpu OSSwapLittleToHostInt64
#endif


static uint16_t m0_byteorder_cpu_to_be16(uint16_t cpu_16bits);
static uint16_t m0_byteorder_cpu_to_le16(uint16_t cpu_16bits);
static uint16_t m0_byteorder_be16_to_cpu(uint16_t big_endian_16bits);
static uint16_t m0_byteorder_le16_to_cpu(uint16_t little_endian_16bits);

static uint32_t m0_byteorder_cpu_to_be32(uint32_t cpu_32bits);
static uint32_t m0_byteorder_cpu_to_le32(uint32_t cpu_32bits);
static uint32_t m0_byteorder_be32_to_cpu(uint32_t big_endian_32bits);
static uint32_t m0_byteorder_le32_to_cpu(uint32_t little_endian_32bits);

static uint64_t m0_byteorder_cpu_to_be64(uint64_t cpu_64bits);
static uint64_t m0_byteorder_cpu_to_le64(uint64_t cpu_64bits);
static uint64_t m0_byteorder_be64_to_cpu(uint64_t big_endian_64bits);
static uint64_t m0_byteorder_le64_to_cpu(uint64_t little_endian_64bits);


static inline uint16_t m0_byteorder_cpu_to_be16(uint16_t cpu_16bits)
{
	return __cpu_to_be16(cpu_16bits);
}

static inline uint16_t m0_byteorder_cpu_to_le16(uint16_t cpu_16bits)
{
	return __cpu_to_le16(cpu_16bits);
}

static inline uint16_t m0_byteorder_be16_to_cpu(uint16_t big_endian_16bits)
{
	return __be16_to_cpu(big_endian_16bits);
}

static inline uint16_t m0_byteorder_le16_to_cpu(uint16_t little_endian_16bits)
{
	return __le16_to_cpu(little_endian_16bits);
}

static inline uint32_t m0_byteorder_cpu_to_be32(uint32_t cpu_32bits)
{
	return __cpu_to_be32(cpu_32bits);
}

static inline uint32_t m0_byteorder_cpu_to_le32(uint32_t cpu_32bits)
{
	return __cpu_to_le32(cpu_32bits);
}

static inline uint32_t m0_byteorder_be32_to_cpu(uint32_t big_endian_32bits)
{
	return __be32_to_cpu(big_endian_32bits);
}

static inline uint32_t m0_byteorder_le32_to_cpu(uint32_t little_endian_32bits)
{
	return __le32_to_cpu(little_endian_32bits);
}

static inline uint64_t m0_byteorder_cpu_to_be64(uint64_t cpu_64bits)
{
	return __cpu_to_be64(cpu_64bits);
}

static inline uint64_t m0_byteorder_cpu_to_le64(uint64_t cpu_64bits)
{
	return __cpu_to_le64(cpu_64bits);
}

static inline uint64_t m0_byteorder_be64_to_cpu(uint64_t big_endian_64bits)
{
	return __be64_to_cpu(big_endian_64bits);
}

static inline uint64_t m0_byteorder_le64_to_cpu(uint64_t little_endian_64bits)
{
	return __le64_to_cpu(little_endian_64bits);
}

#endif /* __MOTR_LIB_BYTEORDER_H__ */

/*
 *  Local variables:
 *  c-indentation-style: "K&R"
 *  c-basic-offset: 8
 *  tab-width: 8
 *  fill-column: 79
 *  scroll-step: 1
 *  End:
 */
