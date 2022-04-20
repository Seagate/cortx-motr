/* -*- C -*- */
/*
 * Copyright (c) 2022 Seagate Technology LLC and/or its Affiliates
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


#include <stdio.h>            /* fprintf */
#include <unistd.h>           /* pause */
#include <signal.h>           /* sigaction */
#include <sys/time.h>
#include <sys/resource.h>

#define M0_TRACE_SUBSYSTEM M0_TRACE_SUBSYS_CAS
#include "lib/trace.h"
#include "lib/errno.h"
#include "lib/memory.h"
#include "lib/misc.h"         /* M0_SET0 */
#include "lib/uuid.h"         /* m0_node_uuid_string_set */

#include "motr/setup.h"
#include "motr/init.h"
#include "motr/version.h"
#include "module/instance.h"  /* m0 */
#include "net/lnet/lnet.h"
#include "reqh/reqh_service.h"
#include "motr/setup_dix.h"
#include "cas/ctg_store.h"

M0_INTERNAL int main(int argc, char **argv)
{
	struct m0_motr   motr_ctx;
	static struct m0 instance;
	struct rlimit    rlim = {10240, 10240};
	char            *uuid = NULL;
	int              rc;
	int              i;

	if (argc > 1 &&
	    (strcmp(argv[1], "-v") == 0 || strcmp(argv[1], "--version") == 0)) {
		m0_build_info_print();
		exit(EXIT_SUCCESS);
	}

	rc = setrlimit(RLIMIT_NOFILE, &rlim);
	if (rc != 0) {
		m0_console_printf("\n Failed to setrlimit\n");
		goto out;
	}

	for (i = 0; i < argc; ++i)
		if (m0_streq("-u", argv[i])) {
			uuid = argv[i + 1];
			break;
		}
	m0_node_uuid_string_set(uuid);

	errno = 0;
	M0_SET0(&motr_ctx);
	rc = m0_init(&instance);
	if (rc != 0) {
		m0_console_printf("\n Failed to initialise Motr \n");
		goto out;
	}

	rc = m0_cs_init(&motr_ctx, m0_net_all_xprt_get(), m0_net_xprt_nr(),
			stderr, false /*not mkfs */);
	if (rc != 0) {
		m0_console_printf("\n Failed to initialise Motr \n");
		goto cleanup;
	}

        rc = m0_cs_setup_env(&motr_ctx, argc, argv);
        if (rc != 0) {
		m0_console_printf("\n Failed to setup cs env \n");
		goto cs_fini;
	}

	/*
	 * Usage of m0ctgdump:
	 * m0ctgdump "cs setup options" + hex (if printing kv pairs in hex)
	 *                              + index's global fid
	 */
	M0_ASSERT(argc >= 2);
	rc = ctgdump(&motr_ctx, argv[argc - 1], argv[argc - 2]);
	fflush(stdout);

cs_fini:
	m0_cs_fini(&motr_ctx);
cleanup:
	m0_fini();
out:
	errno = rc < 0 ? -rc : rc;
	return errno;
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
