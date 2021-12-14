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


#define M0_TRACE_SUBSYSTEM M0_TRACE_SUBSYS_CONF
#include "lib/trace.h"

#include "conf/preload.h"
#include "conf/onwire.h"     /* m0_confx */
#include "conf/onwire_xc.h"  /* m0_confx_xc */
#include "xcode/xcode.h"
#include "lib/memory.h"      /* M0_ALLOC_PTR */
#include "lib/errno.h"       /* ENOMEM */

M0_INTERNAL void m0_confx_free(struct m0_confx *enc)
{
	M0_ENTRY();
	if (enc != NULL)
		m0_xcode_free_obj(&M0_XCODE_OBJ(m0_confx_xc, enc));
	M0_LEAVE();
}

M0_INTERNAL int m0_confstr_parse(const char *str, struct m0_confx **out)
{
	int rc;

	M0_ENTRY();

	M0_ALLOC_PTR(*out);
	if (*out == NULL)
		return M0_ERR(-ENOMEM);

	rc = m0_xcode_read(&M0_XCODE_OBJ(m0_confx_xc, *out), str);
	if (rc != 0) {
		M0_LOG(M0_WARN, "Cannot parse configuration string:\n%s", str);
		m0_confx_free(*out);
		*out = NULL;
	}
	return M0_RC(rc);
}

M0_INTERNAL int m0_confx_to_string(struct m0_confx *confx, char **out)
{
	m0_bcount_t size;
	int         rc;

	M0_ENTRY();

	size = m0_xcode_print(&M0_XCODE_OBJ(m0_confx_xc, confx), NULL, 0) + 1;
	/*
	 * Spiel sends conf string over bulk transport. Bulk buffers
	 * have to be aligned.
	 */
	*out = m0_alloc_aligned(size, m0_pageshift_get());
	if (*out == NULL)
		return M0_ERR_INFO(-ENOMEM, "failed to allocate internal buffer"
				   " for encoded Spiel conf data");
	/* Convert */
	rc = m0_xcode_print(&M0_XCODE_OBJ(m0_confx_xc, confx),
			    *out, size) <= size ? 0 : M0_ERR(-ENOMEM);
	if (rc != 0) {
		m0_free_aligned(*out, size, m0_pageshift_get());
		*out = NULL;
	}
	return M0_RC(rc);
}

M0_INTERNAL void m0_confx_string_free(char *str)
{
	M0_PRE(m0_addr_is_aligned(str, m0_pageshift_get()));
	m0_free_aligned(str, strlen(str)+1, m0_pageshift_get());
}

#undef M0_TRACE_SUBSYSTEM
