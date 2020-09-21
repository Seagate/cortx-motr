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


#include <unistd.h>    /* daemon */

#define M0_TRACE_SUBSYSTEM M0_TRACE_SUBSYS_M0D
#include "lib/trace.h"

#include <sys/resource.h>
#include "lib/bitmap.h"
#include "motr/process_attr.h"
#include "module/instance.h"  /* m0 */

/**
   @addtogroup m0d
   @{
 */

static int reqh_memlimit_set(uint resource, uint64_t limit)
{
	int           rc;
	struct rlimit rl;

	M0_ENTRY();

	rc = -getrlimit(resource, &rl);
	if (rc !=0)
		return M0_ERR(rc);
	rl.rlim_cur = limit;
	rc = -setrlimit(resource, &rl);

	return M0_RC(rc);
}

int m0_cs_memory_limits_setup(struct m0 *instance)
{
	int                  rc;
	struct m0_proc_attr *proc_attr = &instance->i_proc_attr;

	M0_ENTRY();

	rc = proc_attr->pca_memlimit_as != M0_PROCESS_ATTRIBUTE_NO_MEMLIMIT ?
		reqh_memlimit_set(RLIMIT_AS, proc_attr->pca_memlimit_as) : 0;
	rc = rc == 0 &&
	     proc_attr->pca_memlimit_rss != M0_PROCESS_ATTRIBUTE_NO_MEMLIMIT ?
	     reqh_memlimit_set(RLIMIT_RSS, proc_attr->pca_memlimit_rss) : rc;
	rc = rc == 0 &&
	    proc_attr->pca_memlimit_stack != M0_PROCESS_ATTRIBUTE_NO_MEMLIMIT ?
	    reqh_memlimit_set(RLIMIT_STACK, proc_attr->pca_memlimit_stack) : rc;
	rc = rc == 0 &&
	   proc_attr->pca_memlimit_memlock != M0_PROCESS_ATTRIBUTE_NO_MEMLIMIT ?
	   reqh_memlimit_set(RLIMIT_MEMLOCK, proc_attr->pca_memlimit_memlock) :
	   rc;
	return M0_RC(rc);
}

/** @} endgroup m0d */
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
