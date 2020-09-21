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

#ifndef __MOTR_LIB_LINUX_KERNEL_TYPES_H__
#define __MOTR_LIB_LINUX_KERNEL_TYPES_H__

#include <linux/types.h>
#include <linux/kernel.h>  /* INT_MAX */

#include "lib/assert.h"

M0_BASSERT(((uint32_t)0) - 1 == ~(uint32_t)0);

#define UINT8_MAX  ((uint8_t)0xff)
#define INT8_MIN   ((int8_t)0x80)
#define INT8_MAX   ((int8_t)0x7f)
#define UINT16_MAX ((uint16_t)0xffff)
#define INT16_MIN  ((int16_t)0x8000)
#define INT16_MAX  ((int16_t)0x7fff)
#define UINT32_MAX ((uint32_t)0xffffffff)
#define INT32_MIN  ((int32_t)0x80000000)
#define INT32_MAX  ((int32_t)0x7fffffff)
#define UINT64_MAX ((uint64_t)0xffffffffffffffff)
#define INT64_MIN  ((int64_t)0x8000000000000000)
#define INT64_MAX  ((int64_t)0x7fffffffffffffff)

M0_BASSERT(INT8_MIN < 0);
M0_BASSERT(INT8_MAX > 0);
M0_BASSERT(INT16_MIN < 0);
M0_BASSERT(INT16_MAX > 0);
M0_BASSERT(INT32_MIN < 0);
M0_BASSERT(INT32_MAX > 0);
M0_BASSERT(INT64_MIN < 0);
M0_BASSERT(INT64_MAX > 0);

#define PRId64 "lld"
#define PRIu64 "llu"
#define PRIi64 "lli"
#define PRIo64 "llo"
#define PRIx64 "llx"
#define SCNx64 "llx"
#define SCNi64 "lli"

#define PRId32 "d"
#define PRIu32 "u"
#define PRIi32 "i"
#define PRIo32 "o"
#define PRIx32 "x"
#define SCNx32 "x"
#define SCNi32 "i"

/* __MOTR_LIB_LINUX_KERNEL_TYPES_H__ */
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
