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

#define M0_TRACE_SUBSYSTEM M0_TRACE_SUBSYS_LIB
#include "lib/trace.h"
#include "lib/uuid.h"
#include "lib/errno.h"               /* EINVAL */

#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

/** path to read kmod uuid parameter */
static const char *kmod_uuid_file = "/sys/module/m0tr/parameters/node_uuid";

/**
 * Default node uuid which can be used instead of a "real" one, which is
 * obtained from kernel module; this can be handy for some utility applications
 * which don't need full functionality of libmotr.so, so they can provide some
 * fake uuid.
*/
static char default_node_uuid[M0_UUID_STRLEN + 1] =
		"00000000-0000-0000-0000-000000000000"; /* nil UUID */

/** Flag, which specify whether to use a "real" node uuid or a default one. */
static bool use_default_node_uuid = false;

void m0_kmod_uuid_file_set(const char *path)
{
	kmod_uuid_file = path;
}

void m0_node_uuid_string_set(const char *uuid)
{
	use_default_node_uuid = true;
	if (uuid != NULL) {
		strncpy(default_node_uuid, uuid, M0_UUID_STRLEN);
		default_node_uuid[M0_UUID_STRLEN] = '\0';
	}
}

/**
 * Constructs the node UUID in user space by reading our kernel module's
 * node_uuid parameter.
 */
int m0_node_uuid_string_get(char buf[M0_UUID_STRLEN + 1])
{
	int fd;
	int rc = 0;

	if (use_default_node_uuid) {
		strncpy(buf, default_node_uuid, M0_UUID_STRLEN + 1);
		buf[M0_UUID_STRLEN] = '\0';
	} else {
		fd = open(kmod_uuid_file, O_RDONLY);
		if (fd < 0) {
			return M0_ERR_INFO(-EINVAL,
			                   "open(\"%s\", O_RDONLY)=%d "
			                   "errno=%d: is m0tr.ko loaded?",
					   kmod_uuid_file, fd, errno);
		}
		if (read(fd, buf, M0_UUID_STRLEN) == M0_UUID_STRLEN) {
			rc = 0;
			buf[M0_UUID_STRLEN] = '\0';
		} else {
			rc = M0_ERR_INFO(-EINVAL, "Incorrect data in %s",
			                 kmod_uuid_file);
		}
		close(fd);
	}

	return M0_RC(rc);
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
