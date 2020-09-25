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

#ifndef __MOTR_LIB_USER_SPACE_MISC_H__
#define __MOTR_LIB_USER_SPACE_MISC_H__

#ifndef offsetof
#define offsetof(typ,memb) __builtin_offsetof(typ, memb)
#endif

#ifndef container_of
#define container_of(ptr, type, member) \
	((type *)((char *)(ptr)-(char *)(&((type *)0)->member)))
#endif

#ifndef NULL
#define NULL ((void *)0)
#endif

/**
 * size of array
 */
#ifndef ARRAY_SIZE
#define ARRAY_SIZE(a) ((sizeof (a)) / (sizeof (a)[0]))
#endif

#define M0_EXPORTED(s)

/**
 * Print performance counters: getrusage(), /proc/self/io.
 *
 * All errors (can't open file etc.) are silently ignored.
 */
M0_INTERNAL void m0_performance_counters(char *buf, size_t buf_len);

#endif /* __MOTR_LIB_USER_SPACE_MISC_H__ */

/*
 *  Local variables:
 *  c-indentation-style: "K&R"
 *  c-basic-offset: 8
 *  tab-width: 8
 *  fill-column: 80
 *  scroll-step: 1
 *  End:
 */
