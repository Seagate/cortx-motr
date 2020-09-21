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


#include "lib/getopts.h"

#include "lib/misc.h"           /* strchr, m0_strtou64 */
#include "lib/errno.h"          /* EINVAL */
#include "lib/assert.h"         /* M0_CASSSERT */
#include "lib/types.h"          /* UINT64_MAX */

#define M0_TRACE_SUBSYSTEM M0_TRACE_SUBSYS_LIB
#include "lib/trace.h"

const char M0_GETOPTS_DECIMAL_POINT = '.';

M0_INTERNAL int m0_bcount_get(const char *arg, m0_bcount_t *out)
{
	char		 *end = NULL;
	char		 *pos;
	static const char suffix[] = "bkmgKMG";
	int		  rc = 0;

	static const uint64_t multiplier[] = {
		1 << 9,
		1 << 10,
		1 << 20,
		1 << 30,
		1000,
		1000 * 1000,
		1000 * 1000 * 1000
	};

	M0_CASSERT(ARRAY_SIZE(suffix) - 1 == ARRAY_SIZE(multiplier));

	*out = m0_strtou64(arg, &end, 0);

	if (*end != 0) {
		pos = strchr(suffix, *end);
		if (pos != NULL) {
			if (*out <= M0_BCOUNT_MAX / multiplier[pos - suffix])
				*out *= multiplier[pos - suffix];
			else
				rc = -EOVERFLOW;
		} else
			rc = -EINVAL;
	}
	return rc;
}

M0_INTERNAL int m0_time_get(const char *arg, m0_time_t *out)
{
	char	*end = NULL;
	uint64_t before;	/* before decimal point */
	uint64_t after = 0;	/* after decimal point */
	int	 rc = 0;
	uint64_t unit_mul = 1000000000;
	int	 i;
	uint64_t pow_of_10 = 1;

	static const char *unit[] = {
		"s",
		"ms",
		"us",
		"ns",
	};
	static const uint64_t multiplier[] = {
		1000000000,
		1000000,
		1000,
		1,
	};

	M0_CASSERT(ARRAY_SIZE(unit) == ARRAY_SIZE(multiplier));

	before = m0_strtou64(arg, &end, 10);
	if (*end == M0_GETOPTS_DECIMAL_POINT) {
		arg = ++end;
		after = m0_strtou64(arg, &end, 10);
		for (i = 0; i < end - arg; ++i) {
			pow_of_10 = pow_of_10 >= UINT64_MAX / 10 ? UINT64_MAX :
				    pow_of_10 * 10;
		}
	}
	if (before == UINT64_MAX || after == UINT64_MAX)
		rc = -E2BIG;

	if (rc == 0 && *end != '\0') {
		for (i = 0; i < ARRAY_SIZE(unit); ++i) {
			if (strncmp(end, unit[i], strlen(unit[i]) + 1) == 0) {
				unit_mul = multiplier[i];
				break;
			}
		}
		if (i == ARRAY_SIZE(unit))
			rc = -EINVAL;
	}
	if (rc == 0)
		*out = before * unit_mul + after * unit_mul / pow_of_10;
	return rc;
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
