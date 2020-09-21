/* -*- C -*- */
/*
 * Copyright (c) 2015-2020 Seagate Technology LLC and/or its Affiliates
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
 * @addtogroup uuid
 *
 * @{
 */

#include <linux/moduleparam.h> /* module_param */

#define M0_TRACE_SUBSYSTEM M0_TRACE_SUBSYS_LIB
#include "lib/trace.h"
#include "lib/uuid.h"

static char *node_uuid = "00000000-0000-0000-0000-000000000000"; /* nil UUID */
module_param(node_uuid, charp, S_IRUGO);
MODULE_PARM_DESC(node_uuid, "UUID of Motr node");

/**
 * Return the value of the kernel node_uuid parameter.
 */
static const char *m0_param_node_uuid_get(void)
{
	return node_uuid;
}

/**
 * Construct the node uuid from the kernel's node_uuid parameter.
 */
int m0_node_uuid_string_get(char buf[M0_UUID_STRLEN + 1])
{
	const char *s;

	s = m0_param_node_uuid_get();
	if (s == NULL)
		return M0_ERR(-EINVAL);
	strncpy(buf, s, M0_UUID_STRLEN);
	buf[M0_UUID_STRLEN] = '\0';
	return 0;
}

#undef M0_TRACE_SUBSYSTEM

/** @} end of uuid group */

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
