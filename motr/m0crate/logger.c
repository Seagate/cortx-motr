/* -*- C -*- */
/*
 * Copyright (c) 2017-2020 Seagate Technology LLC and/or its Affiliates
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


#include "motr/m0crate/logger.h"
#include <string.h>

enum cr_log_level log_level  = CLL_INFO;
enum cr_log_level prev_level = CLL_DEBUG;

struct {
	const char *name;
} level_str[] = {
        [CLL_ERROR]	= {"error"},
        [CLL_WARN]	= {"warning"},
        [CLL_INFO]	= {"info"},
        [CLL_TRACE]	= {"trace"},
        [CLL_DEBUG]	= {"dbg"},
};

void cr_log(enum cr_log_level lev, const char *fmt, ...)
{
        va_list va;
	va_start(va, fmt);
	cr_vlog(lev, fmt, va);
	va_end(va);

}

void cr_vlog(enum cr_log_level lev, const char *fmt, va_list args)
{
	if (lev == CLL_SAME) {
		if (prev_level <= log_level) {
			(void) vfprintf(stderr, fmt, args);
		}
	} else {
		if (lev <= log_level) {
			(void) fprintf(stderr, "%s: ", level_str[lev].name);
			(void) vfprintf(stderr, fmt, args);
		}
		prev_level = lev;
	}
}

void cr_set_debug_level(enum cr_log_level level)
{
	log_level = level;
	prev_level = level;
}

void cr_log_ex(enum cr_log_level lev,
	       const char *pre,
	       const char *post,
	       const char *fmt, ...)
{
        va_list va;
	cr_log(lev, "%s", pre);
	va_start(va, fmt);
	cr_vlog(CLL_SAME, fmt, va);
	va_end(va);
	cr_log(CLL_SAME, "%s", post);
}

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
