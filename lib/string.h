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

#ifndef __MOTR_LIB_STRING_H__
#define __MOTR_LIB_STRING_H__

/*
 * Define standard string manipulation functions (strcat, strlen, strcmp, &c.)
 * together with sprintf(3) and snprintf(3).
 * Also pick up support for strtoul(3) and variants, and ctype macros.
 */

#define m0_streq(a, b) (strcmp((a), (b)) == 0)
#define m0_strcaseeq(a, b) (strcasecmp((a), (b)) == 0)

#ifndef __KERNEL__
# include <ctype.h>
# include <stdio.h>
# include <stdlib.h>
# include <string.h>

#define m0_strdup(s) strdup((s))
#define m0_asprintf(s, fmt, ...)                          \
	({                                                \
		int __nr;                                 \
		char **__s = (s);                         \
		__nr = asprintf(__s, (fmt), __VA_ARGS__); \
		if (__nr <= 0)                            \
			*__s = NULL;                      \
	})

#else
# include <linux/ctype.h>
# include <linux/kernel.h>
# include <linux/string.h>

#define m0_strdup(s) kstrdup((s), GFP_KERNEL)
#define m0_asprintf(s, fmt, ...) \
	({ *(s) = kasprintf(GFP_ATOMIC, (fmt), __VA_ARGS__); })

static inline char *strerror(int errnum)
{
	return "strerror() is not supported in kernel";
}
#endif /* __KERNEL__ */

#include "lib/types.h"

/**
 * Converts m0_bcount_t number into a reduced string representation, calculating
 * a magnitude and representing it as standard suffix like "Ki", "Mi", "Gi" etc.
 * So, for example, 87654321 becomes "83 Mi".
 */
const char *m0_bcount_with_suffix(char *buf, size_t size, m0_bcount_t c);

M0_INTERNAL void m0_strings_free(const char **arr);

M0_INTERNAL const char **m0_strings_dup(const char **src);

M0_INTERNAL char *
m0_vsnprintf(char *buf, size_t buflen, const char *format, ...)
	__attribute__((format (printf, 3, 4)));

/** Returns true iff `str' starts with the specified `prefix'. */
M0_INTERNAL bool m0_startswith(const char *prefix, const char *str);

#endif /* __MOTR_LIB_STRING_H__ */

/*
 *  Local variables:
 *  c-indentation-style: "K&R"
 *  c-basic-offset: 8
 *  tab-width: 8
 *  fill-column: 80
 *  scroll-step: 1
 *  End:
 */
