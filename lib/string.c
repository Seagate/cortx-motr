/* -*- C -*- */
/*
 * Copyright (c) 2014-2020 Seagate Technology LLC and/or its Affiliates
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

#include <stdarg.h>

#define M0_TRACE_SUBSYSTEM M0_TRACE_SUBSYS_LIB
#include "lib/trace.h"

#include "lib/types.h"        /* m0_bcount_t */
#include "lib/memory.h"       /* M0_ALLOC_ARR */
#include "lib/misc.h"         /* ARRAY_SIZE */
#include "lib/finject.h"      /* M0_FI_ENABLED */
#include "lib/string.h"

const char *m0_bcount_with_suffix(char *buf, size_t size, m0_bcount_t c)
{
	static const char *suffix[] = {
		"", "Ki", "Mi", "Gi", "Ti", "Pi", "Ei", "Zi", "Yi"
	};
	int i;

	for (i = 0; i < ARRAY_SIZE(suffix) - 1 && c >= 1024; ++i, c /= 1024)
		;
	snprintf(buf, size, "%3" PRId64 " %s", c, suffix[i]);
	return buf;
}

#ifndef __KERNEL__

M0_INTERNAL char *m0_strdup(const char *s)
{
	char *copy = m0_alloc(strlen(s) + 1);
	if (copy != NULL)
		strcpy(copy, s);
	return copy;
}

M0_INTERNAL void m0_asprintf(char **ret, const char *format, ...)
{
	va_list args;
	char   *s;

	va_start(args, format);
	if (vasprintf(&s, format, args) > 0) {
		*ret = m0_strdup(s);
		free(s);
	} else
		*ret = NULL;
	va_end(args);
}

#endif /* __KERNEL__ */

M0_INTERNAL void m0_strings_free(const char **arr)
{
	if (arr != NULL) {
		const char **p;
		for (p = arr; *p != NULL; ++p)
			m0_free((void *)*p);
		m0_free0(&arr);
	}
}

M0_INTERNAL const char **m0_strings_dup(const char **src)
{
	int     i;
	int     n;
	char  **dest;

	for (n = 0; src[n] != NULL; ++n)
		; /* counting */

	M0_ALLOC_ARR(dest, n + 1);
	if (dest == NULL)
		return NULL;

	for (i = 0; i < n; ++i) {
		if (!M0_FI_ENABLED("strdup_failed")) {
			dest[i] = m0_strdup(src[i]);
		}

		if (dest[i] == NULL) {
			m0_strings_free((const char **)dest);
			return NULL;
		}
	}
	dest[n] = NULL; /* end of list */

	return (const char **)dest;
}

M0_INTERNAL char *
m0_vsnprintf(char *buf, size_t buflen, const char *format, ...)
{
	va_list ap;
	int     n;

	va_start(ap, format);
	n = vsnprintf(buf, buflen, format, ap);
	va_end(ap);
	M0_ASSERT(n > 0);
	if ((size_t)n >= buflen)
		M0_LOG(M0_WARN, "Output was truncated");
	return buf;
}

M0_INTERNAL bool m0_startswith(const char *prefix, const char *str)
{
	size_t len_pre = strlen(prefix);

	return len_pre <= strlen(str) && strncmp(prefix, str, len_pre) == 0;
}

#undef M0_TRACE_SUBSYSTEM

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
