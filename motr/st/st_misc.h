/* -*- C -*- */
/*
 * Copyright (c) 2020 Seagate Technology LLC and/or its Affiliates
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

#ifndef __MOTR_ST_ST_MISC_H__
#define __MOTR_ST_ST_MISC_H__

#ifdef __KERNEL__

# include <linux/kernel.h>
# include <linux/ctype.h>
# include <linux/string.h>
# include <linux/types.h>

#define str_dup(s) kstrdup((s), GFP_KERNEL)

/*
 * syslog(8) will trim leading spaces of each kernel log line, so we need to use
 * a non-space character at the beginning of each line to preserve formatting
 */

#define LOG_PREFIX "."

#else

# include <ctype.h>
# include <stdio.h>
# include <stdint.h>
# include <stdlib.h>
# include <string.h>

#define str_dup(s) strdup((s))

#define LOG_PREFIX

#endif /* __KERNEL__ */

#define str_eq(a, b) (strcmp((a), (b)) == 0)

enum {
	TIME_ONE_SECOND = 1000000000ULL,
	TIME_ONE_MSEC   = TIME_ONE_SECOND / 1000
};

/**
 * Helper functions
 */
pid_t get_tid(void);
void console_printf(const char *fmt, ...);
uint32_t generate_random(uint32_t max);

uint64_t time_now(void);
uint64_t time_from_now(uint64_t secs, uint64_t ns);
uint64_t time_seconds(const uint64_t time);
uint64_t time_nanoseconds(const uint64_t time);

void *mem_alloc(size_t size);
void  mem_free(void *p);

#define MEM_ALLOC_ARR(arr, nr)  ((arr) = mem_alloc((nr) * sizeof ((arr)[0])))
#define MEM_ALLOC_PTR(arr) MEM_ALLOC_ARR(arr, 1)

#endif /* __MOTR_ST_ST_MISC_H__ */
