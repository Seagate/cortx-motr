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


/**
 * Helper functions for Client ST in both user space and kernel
 */
#ifdef __KERNEL__

#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/time.h>
#include <linux/slab.h>
#include <linux/sched.h>
#include <linux/random.h>

#else
#include <stdlib.h>
#include <stdint.h>
#include <stdarg.h>
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>

#endif

#include <stdbool.h>

#include "motr/st/st_assert.h"
#include "motr/st/st_misc.h"
#include "lib/memory.h"

/*******************************************************************
 *                              Time                               *
 *******************************************************************/

#ifdef __KERNEL__

uint64_t time_now(void)
{
	struct timespec ts;

	getnstimeofday(&ts);
	return ts.tv_sec * TIME_ONE_SECOND + ts.tv_nsec;
}

#else

uint64_t time_now(void)
{
	struct timeval tv;

	gettimeofday(&tv, NULL);
	return tv.tv_sec * TIME_ONE_SECOND + tv.tv_usec * 1000;
}

#endif /* end of time_now*/

uint64_t time_seconds(const uint64_t time)
{
	return time / TIME_ONE_SECOND;
}

uint64_t time_nanoseconds(const uint64_t time)
{
        return time % TIME_ONE_SECOND;
}

uint64_t time_from_now(uint64_t secs, uint64_t ns)
{
	return time_now() + secs * TIME_ONE_SECOND + ns;
}


/*******************************************************************
 *                             Memory                              *
 *******************************************************************/

void *mem_alloc(size_t size)
{
	void *p;

	p = m0_alloc(size);
	if (p != NULL)
		memset(p, 0, size);

	if (st_is_cleaner_up() == true)
		st_mark_ptr(p);

	return p;
}

void mem_free(void *p)
{
	if (st_is_cleaner_up() == true)
		st_unmark_ptr(p);

	m0_free(p);
}

/*******************************************************************
 *                             Misc                                *
 *******************************************************************/

#ifdef __KERNEL__
pid_t get_tid(void)
{
	return current->pid;
}

void console_printf(const char *fmt, ...)
{
	va_list args;

	va_start(args, fmt);
	vprintk(fmt, args);
	va_end(args);
}

uint32_t generate_random(uint32_t max)
{
	uint32_t randv;

	get_random_bytes(&randv, sizeof(randv));
	return randv % max;
}

#else

void console_printf(const char *fmt, ...)
{
	va_list args;

	va_start(args, fmt);
	vprintf(fmt, args);
	va_end(args);
}

uint32_t generate_random(uint32_t max)
{
	return (uint32_t)random()%max;
}

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
